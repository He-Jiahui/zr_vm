#include "debug_internal.h"

#define ZR_DEBUG_VARIABLE_HANDLE_BASE ((TZrUInt32)1000u)
#define ZR_DEBUG_SYNTHETIC_OBJECT_FIELD_COUNT ((TZrSize)7u)
#define ZR_DEBUG_INVALID_SLOT_INDEX ((TZrUInt32)0xFFFFFFFFu)

static const SZrTypeValue *zr_debug_object_get_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrTypeValue key;
    SZrString *fieldString;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

static TZrBool zr_debug_make_string_key(SZrState *state, const TZrChar *name, SZrTypeValue *outKey) {
    SZrString *stringObject;

    if (outKey != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outKey);
    }
    if (state == ZR_NULL || name == ZR_NULL || outKey == ZR_NULL) {
        return ZR_FALSE;
    }

    stringObject = ZrCore_String_CreateFromNative(state, (TZrNativeString)name);
    if (stringObject == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, outKey, ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject));
    outKey->type = ZR_VALUE_TYPE_STRING;
    return ZR_TRUE;
}

static TZrBool zr_debug_is_receiver_name(const TZrChar *name) {
    return (TZrBool)(name != ZR_NULL && (strcmp(name, "this") == 0 || strcmp(name, "self") == 0));
}

static const TZrChar *zr_debug_protocol_name(EZrProtocolId protocolId) {
    switch (protocolId) {
        case ZR_PROTOCOL_ID_EQUATABLE:
            return "Equatable";
        case ZR_PROTOCOL_ID_HASHABLE:
            return "Hashable";
        case ZR_PROTOCOL_ID_COMPARABLE:
            return "Comparable";
        case ZR_PROTOCOL_ID_ITERABLE:
            return "Iterable";
        case ZR_PROTOCOL_ID_ITERATOR:
            return "Iterator";
        case ZR_PROTOCOL_ID_ARRAY_LIKE:
            return "ArrayLike";
        case ZR_PROTOCOL_ID_NONE:
        default:
            return "None";
    }
}

static const TZrChar *zr_debug_value_type_name_safe(SZrState *state, const SZrTypeValue *value) {
    SZrObjectPrototype *prototype;

    if (state == ZR_NULL || value == ZR_NULL) {
        return "value";
    }

    prototype = zr_debug_resolve_value_prototype(state, value);
    if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) &&
        prototype != ZR_NULL &&
        prototype->name != ZR_NULL) {
        return zr_debug_string_native(prototype->name);
    }

    return zr_debug_value_type_name(value->type);
}

static TZrBool zr_debug_value_is_expandable(SZrState *state, const SZrTypeValue *value) {
    ZR_UNUSED_PARAMETER(state);

    if (value == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) &&
                     value->value.object != ZR_NULL);
}

static SZrObject *zr_debug_exception_frames_object(ZrDebugAgent *agent) {
    const SZrTypeValue *stacksValue;
    SZrObject *exceptionObject;

    if (agent == ZR_NULL || agent->state == ZR_NULL || !agent->state->hasCurrentException ||
        agent->state->currentException.type != ZR_VALUE_TYPE_OBJECT ||
        agent->state->currentException.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    exceptionObject = ZR_CAST_OBJECT(agent->state, agent->state->currentException.value.object);
    stacksValue = zr_debug_object_get_field(agent->state, exceptionObject, "stacks");
    if (stacksValue == ZR_NULL || stacksValue->type != ZR_VALUE_TYPE_ARRAY || stacksValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(agent->state, stacksValue->value.object);
}

TZrSize zr_debug_exception_frame_count(ZrDebugAgent *agent) {
    SZrObject *frames = zr_debug_exception_frames_object(agent);
    return frames != ZR_NULL ? frames->nodeMap.elementCount : 0;
}

TZrBool zr_debug_exception_read_frame(ZrDebugAgent *agent, TZrUInt32 frameId, ZrDebugFrameSnapshot *outFrame) {
    SZrObject *framesObject;
    SZrTypeValue key;
    const SZrTypeValue *frameValue;
    const SZrTypeValue *fieldValue;
    SZrObject *frameObject;

    if (outFrame != ZR_NULL) {
        memset(outFrame, 0, sizeof(*outFrame));
    }
    if (agent == ZR_NULL || agent->state == ZR_NULL || outFrame == ZR_NULL || frameId == 0) {
        return ZR_FALSE;
    }

    framesObject = zr_debug_exception_frames_object(agent);
    if (framesObject == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(agent->state, &key, (TZrInt64)(frameId - 1u));
    frameValue = ZrCore_Object_GetValue(agent->state, framesObject, &key);
    if (frameValue == ZR_NULL || frameValue->type != ZR_VALUE_TYPE_OBJECT || frameValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    frameObject = ZR_CAST_OBJECT(agent->state, frameValue->value.object);
    outFrame->frame_id = frameId;
    outFrame->frame_depth = frameId - 1u;
    outFrame->return_slot = -1;
    outFrame->is_exception_frame = ZR_TRUE;
    zr_debug_copy_text(outFrame->module_name, sizeof(outFrame->module_name), agent->moduleName);
    zr_debug_copy_text(outFrame->call_kind, sizeof(outFrame->call_kind), "exception");

    fieldValue = zr_debug_object_get_field(agent->state, frameObject, "functionName");
    if (fieldValue != ZR_NULL && fieldValue->type == ZR_VALUE_TYPE_STRING && fieldValue->value.object != ZR_NULL) {
        zr_debug_copy_text(outFrame->function_name,
                           sizeof(outFrame->function_name),
                           ZrCore_String_GetNativeString(ZR_CAST_STRING(agent->state, fieldValue->value.object)));
    } else {
        zr_debug_copy_text(outFrame->function_name, sizeof(outFrame->function_name), "<anonymous>");
    }

    fieldValue = zr_debug_object_get_field(agent->state, frameObject, "sourceFile");
    if (fieldValue != ZR_NULL && fieldValue->type == ZR_VALUE_TYPE_STRING && fieldValue->value.object != ZR_NULL) {
        zr_debug_copy_text(outFrame->source_file,
                           sizeof(outFrame->source_file),
                           ZrCore_String_GetNativeString(ZR_CAST_STRING(agent->state, fieldValue->value.object)));
    }

    fieldValue = zr_debug_object_get_field(agent->state, frameObject, "sourceLine");
    if (fieldValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(fieldValue->type)) {
        outFrame->line = (TZrUInt32)fieldValue->value.nativeObject.nativeInt64;
    }

    fieldValue = zr_debug_object_get_field(agent->state, frameObject, "instructionOffset");
    if (fieldValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(fieldValue->type)) {
        outFrame->instruction_index = (TZrUInt32)fieldValue->value.nativeObject.nativeInt64;
    }

    return ZR_TRUE;
}

const TZrChar *zr_debug_prototype_type_name(EZrObjectPrototypeType type) {
    switch (type) {
        case ZR_OBJECT_PROTOTYPE_TYPE_CLASS:
            return "class";
        case ZR_OBJECT_PROTOTYPE_TYPE_STRUCT:
            return "struct";
        case ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE:
            return "interface";
        case ZR_OBJECT_PROTOTYPE_TYPE_ENUM:
            return "enum";
        case ZR_OBJECT_PROTOTYPE_TYPE_MODULE:
            return "module";
        case ZR_OBJECT_PROTOTYPE_TYPE_NATIVE:
            return "native";
        case ZR_OBJECT_PROTOTYPE_TYPE_INVALID:
        default:
            return "prototype";
    }
}

SZrCallInfo *zr_debug_find_call_info_by_frame_id(ZrDebugAgent *agent, TZrUInt32 frameId, SZrFunction **outFunction) {
    TZrUInt32 currentId = 1;
    SZrCallInfo *callInfo;

    if (outFunction != ZR_NULL) {
        *outFunction = ZR_NULL;
    }
    if (agent == ZR_NULL || agent->state == ZR_NULL || frameId == 0) {
        return ZR_NULL;
    }

    callInfo = agent->state->callInfoList;
    while (callInfo != ZR_NULL) {
        SZrFunction *function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(agent->state, callInfo);
        if (function != ZR_NULL) {
            if (currentId == frameId) {
                if (outFunction != ZR_NULL) {
                    *outFunction = function;
                }
                return callInfo;
            }
            currentId++;
        }
        callInfo = callInfo->previous;
    }

    return ZR_NULL;
}

SZrObjectPrototype *zr_debug_resolve_value_prototype(SZrState *state, const SZrTypeValue *value) {
    SZrObject *object;

    if (state == ZR_NULL || state->global == ZR_NULL || value == ZR_NULL) {
        return ZR_NULL;
    }

    if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) && value->value.object != ZR_NULL) {
        object = ZR_CAST_OBJECT(state, value->value.object);
        if (object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
            return (SZrObjectPrototype *)object;
        }
        if (object != ZR_NULL && object->prototype != ZR_NULL) {
            return object->prototype;
        }
        if ((TZrUInt32)value->type < ZR_VALUE_TYPE_ENUM_MAX) {
            return state->global->basicTypeObjectPrototype[value->type];
        }
        return ZR_NULL;
    }

    if ((TZrUInt32)value->type < ZR_VALUE_TYPE_ENUM_MAX) {
        return state->global->basicTypeObjectPrototype[value->type];
    }

    return ZR_NULL;
}

void zr_debug_format_value_text_safe(SZrState *state, const SZrTypeValue *value, TZrChar *buffer, TZrSize bufferSize) {
    SZrObjectPrototype *prototype;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }
    buffer[0] = '\0';
    if (state == ZR_NULL || value == ZR_NULL) {
        return;
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_NULL:
            zr_debug_copy_text(buffer, bufferSize, "null");
            return;
        case ZR_VALUE_TYPE_BOOL:
            zr_debug_copy_text(buffer, bufferSize, value->value.nativeObject.nativeBool ? "true" : "false");
            return;
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            snprintf(buffer, bufferSize, "%lld", (long long)value->value.nativeObject.nativeInt64);
            buffer[bufferSize - 1u] = '\0';
            return;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            snprintf(buffer, bufferSize, "%llu", (unsigned long long)value->value.nativeObject.nativeUInt64);
            buffer[bufferSize - 1u] = '\0';
            return;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            snprintf(buffer, bufferSize, "%.15g", value->value.nativeObject.nativeDouble);
            buffer[bufferSize - 1u] = '\0';
            return;
        case ZR_VALUE_TYPE_STRING:
            if (value->value.object != ZR_NULL) {
                zr_debug_copy_text(buffer,
                                   bufferSize,
                                   ZrCore_String_GetNativeString(ZR_CAST_STRING(state, value->value.object)));
            }
            return;
        case ZR_VALUE_TYPE_FUNCTION:
            if (value->value.object != ZR_NULL) {
                zr_debug_copy_text(buffer,
                                   bufferSize,
                                   zr_debug_function_name(ZR_CAST_FUNCTION(state, value->value.object)));
            } else {
                zr_debug_copy_text(buffer, bufferSize, "<function>");
            }
            return;
        case ZR_VALUE_TYPE_CLOSURE:
            if (value->value.object != ZR_NULL) {
                SZrFunction *metadataFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state, value);
                zr_debug_copy_text(buffer,
                                   bufferSize,
                                   metadataFunction != ZR_NULL
                                           ? zr_debug_function_name(metadataFunction)
                                           : (value->isNative ? "<native closure>" : "<closure>"));
            } else {
                zr_debug_copy_text(buffer, bufferSize, "<closure>");
            }
            return;
        case ZR_VALUE_TYPE_OBJECT:
        case ZR_VALUE_TYPE_ARRAY:
            prototype = zr_debug_resolve_value_prototype(state, value);
            if (prototype != ZR_NULL && prototype->name != ZR_NULL) {
                snprintf(buffer,
                         bufferSize,
                         "<%s %s>",
                         zr_debug_prototype_type_name(prototype->type),
                         zr_debug_string_native(prototype->name));
            } else if (value->type == ZR_VALUE_TYPE_ARRAY) {
                zr_debug_copy_text(buffer, bufferSize, "<array>");
            } else {
                zr_debug_copy_text(buffer, bufferSize, "<object>");
            }
            buffer[bufferSize - 1u] = '\0';
            return;
        default:
            zr_debug_copy_text(buffer, bufferSize, zr_debug_value_type_name(value->type));
            return;
    }
}

TZrUInt32 zr_debug_call_info_line(ZrDebugAgent *agent,
                                  SZrCallInfo *callInfo,
                                  SZrFunction *function,
                                  TZrBool isTopFrame) {
    TZrUInt32 instructionIndex;
    TZrUInt32 sourceLine;

    if (callInfo == ZR_NULL || function == ZR_NULL) {
        return 0;
    }

    instructionIndex = zr_debug_instruction_offset(callInfo, function);
    sourceLine = ZrCore_Exception_FindSourceLine(function, instructionIndex);
    if (sourceLine == 0 && isTopFrame && agent != ZR_NULL &&
        agent->lastStopEvent.reason != ZR_DEBUG_STOP_REASON_NONE) {
        sourceLine = agent->lastStopEvent.line;
    }
    return sourceLine;
}

static ZrDebugVariableHandle *zr_debug_find_variable_handle(ZrDebugAgent *agent, TZrUInt32 handleId) {
    TZrSize index;

    if (agent == ZR_NULL || handleId < ZR_DEBUG_VARIABLE_HANDLE_BASE) {
        return ZR_NULL;
    }

    for (index = 0; index < agent->variableHandleCount; index++) {
        if (agent->variableHandles[index].handle_id == handleId &&
            agent->variableHandles[index].state_id == agent->stopStateId) {
            return &agent->variableHandles[index];
        }
    }

    return ZR_NULL;
}

static ZrDebugVariableHandle *zr_debug_alloc_variable_handle(ZrDebugAgent *agent) {
    ZrDebugVariableHandle *expandedHandles;
    TZrSize newCapacity;
    ZrDebugVariableHandle *handle;

    if (agent == ZR_NULL) {
        return ZR_NULL;
    }

    if (agent->variableHandleCount >= agent->variableHandleCapacity) {
        newCapacity = agent->variableHandleCapacity > 0 ? agent->variableHandleCapacity * 2u : 16u;
        expandedHandles = (ZrDebugVariableHandle *)realloc(agent->variableHandles, sizeof(*expandedHandles) * newCapacity);
        if (expandedHandles == ZR_NULL) {
            return ZR_NULL;
        }
        agent->variableHandles = expandedHandles;
        agent->variableHandleCapacity = newCapacity;
    }

    handle = &agent->variableHandles[agent->variableHandleCount++];
    memset(handle, 0, sizeof(*handle));
    handle->state_id = agent->stopStateId;
    handle->handle_id = agent->nextVariableHandleId++;
    return handle;
}

static TZrUInt32 zr_debug_register_prototype_handle(ZrDebugAgent *agent, SZrObjectPrototype *prototype) {
    ZrDebugVariableHandle *handle;

    if (agent == ZR_NULL || prototype == ZR_NULL) {
        return 0;
    }

    handle = zr_debug_alloc_variable_handle(agent);
    if (handle == ZR_NULL) {
        return 0;
    }

    handle->kind = ZR_DEBUG_VARIABLE_HANDLE_KIND_PROTOTYPE;
    handle->prototype = prototype;
    ZrCore_Value_InitAsRawObject(agent->state, &handle->value, ZR_CAST_RAW_OBJECT_AS_SUPER(&prototype->super));
    handle->value.type = ZR_VALUE_TYPE_OBJECT;
    return handle->handle_id;
}

static TZrUInt32 zr_debug_register_prototype_view_handle(ZrDebugAgent *agent,
                                                         SZrObjectPrototype *prototype,
                                                         EZrDebugPrototypeViewKind viewKind) {
    ZrDebugVariableHandle *handle;

    if (agent == ZR_NULL || prototype == ZR_NULL || viewKind == ZR_DEBUG_PROTOTYPE_VIEW_NONE) {
        return 0;
    }

    handle = zr_debug_alloc_variable_handle(agent);
    if (handle == ZR_NULL) {
        return 0;
    }

    handle->kind = ZR_DEBUG_VARIABLE_HANDLE_KIND_PROTOTYPE_VIEW;
    handle->prototype = prototype;
    handle->prototype_view_kind = viewKind;
    ZrCore_Value_InitAsRawObject(agent->state, &handle->value, ZR_CAST_RAW_OBJECT_AS_SUPER(&prototype->super));
    handle->value.type = ZR_VALUE_TYPE_OBJECT;
    return handle->handle_id;
}

static TZrUInt32 zr_debug_register_global_state_handle(ZrDebugAgent *agent) {
    ZrDebugVariableHandle *handle;

    if (agent == ZR_NULL || agent->state == ZR_NULL) {
        return 0;
    }

    handle = zr_debug_alloc_variable_handle(agent);
    if (handle == ZR_NULL) {
        return 0;
    }

    handle->kind = ZR_DEBUG_VARIABLE_HANDLE_KIND_GLOBAL_STATE;
    return handle->handle_id;
}

static TZrUInt32 zr_debug_register_exception_handle(ZrDebugAgent *agent) {
    ZrDebugVariableHandle *handle;

    if (agent == ZR_NULL || agent->state == ZR_NULL || !agent->state->hasCurrentException) {
        return 0;
    }

    handle = zr_debug_alloc_variable_handle(agent);
    if (handle == ZR_NULL) {
        return 0;
    }

    handle->kind = ZR_DEBUG_VARIABLE_HANDLE_KIND_EXCEPTION;
    handle->value = agent->state->currentException;
    return handle->handle_id;
}

static TZrUInt32 zr_debug_register_value_handle(ZrDebugAgent *agent, const SZrTypeValue *value) {
    ZrDebugVariableHandle *handle;
    SZrObjectPrototype *prototype;
    SZrObject *object;

    if (agent == ZR_NULL || agent->state == ZR_NULL || value == ZR_NULL ||
        !(value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) ||
        value->value.object == ZR_NULL) {
        return 0;
    }

    prototype = zr_debug_resolve_value_prototype(agent->state, value);
    object = ZR_CAST_OBJECT(agent->state, value->value.object);
    if (prototype != ZR_NULL && object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        return zr_debug_register_prototype_handle(agent, prototype);
    }

    handle = zr_debug_alloc_variable_handle(agent);
    if (handle == ZR_NULL) {
        return 0;
    }

    handle->kind = ZR_DEBUG_VARIABLE_HANDLE_KIND_VALUE;
    handle->value = *value;
    handle->prototype = prototype;
    return handle->handle_id;
}

static void zr_debug_fill_text_preview(ZrDebugValuePreview *preview,
                                       EZrDebugScopeKind scopeKind,
                                       const TZrChar *name,
                                       const TZrChar *typeName,
                                       const TZrChar *valueText,
                                       TZrUInt32 variablesReference) {
    if (preview == ZR_NULL) {
        return;
    }

    memset(preview, 0, sizeof(*preview));
    preview->scope_kind = scopeKind;
    preview->variables_reference = variablesReference;
    zr_debug_copy_text(preview->name, sizeof(preview->name), name != ZR_NULL ? name : "");
    zr_debug_copy_text(preview->type_name, sizeof(preview->type_name), typeName != ZR_NULL ? typeName : "value");
    zr_debug_copy_text(preview->value_text, sizeof(preview->value_text), valueText != ZR_NULL ? valueText : "");
}

static TZrBool zr_debug_fill_value_preview(ZrDebugAgent *agent,
                                           const TZrChar *name,
                                           EZrDebugScopeKind scopeKind,
                                           const SZrTypeValue *value,
                                           ZrDebugValuePreview *preview) {
    TZrUInt32 variablesReference = 0;

    if (agent == ZR_NULL || preview == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (zr_debug_value_is_expandable(agent->state, value)) {
        variablesReference = zr_debug_register_value_handle(agent, value);
    }

    zr_debug_fill_text_preview(preview,
                               scopeKind,
                               name,
                               zr_debug_value_type_name_safe(agent->state, value),
                               "",
                               variablesReference);
    zr_debug_format_value_text_safe(agent->state, value, preview->value_text, sizeof(preview->value_text));
    return ZR_TRUE;
}

static TZrSize zr_debug_count_visible_object_entries(const SZrHashSet *set) {
    TZrSize bucketIndex;
    TZrSize count = 0;

    if (set == ZR_NULL || !set->isValid || set->buckets == ZR_NULL || set->capacity == 0) {
        return 0;
    }

    for (bucketIndex = 0; bucketIndex < set->capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = set->buckets[bucketIndex];
        while (pair != ZR_NULL) {
            const TZrChar *keyText = pair->key.type == ZR_VALUE_TYPE_STRING && pair->key.value.object != ZR_NULL
                                             ? ZrCore_String_GetNativeString(ZR_CAST_STRING(ZR_NULL, pair->key.value.object))
                                             : ZR_NULL;
            if (keyText == ZR_NULL || strncmp(keyText, "__zr_", 5) != 0) {
                count++;
            }
            pair = pair->next;
        }
    }

    return count;
}

static TZrUInt32 zr_debug_loaded_module_count(ZrDebugAgent *agent) {
    if (agent == ZR_NULL || agent->state == ZR_NULL || agent->state->global == ZR_NULL ||
        agent->state->global->loadedModulesRegistry.value.object == ZR_NULL) {
        return 0;
    }

    return (TZrUInt32)ZR_CAST_OBJECT(agent->state, agent->state->global->loadedModulesRegistry.value.object)->nodeMap.elementCount;
}

static TZrBool zr_debug_expand_global_state(ZrDebugAgent *agent,
                                            ZrDebugValuePreview **outValues,
                                            TZrSize *outCount) {
    ZrDebugValuePreview *values;
    TZrSize count = 6;

    if (outValues != ZR_NULL) {
        *outValues = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (agent == ZR_NULL || agent->state == ZR_NULL || outValues == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    values = (ZrDebugValuePreview *)calloc(count, sizeof(*values));
    if (values == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_debug_fill_text_preview(&values[0], ZR_DEBUG_SCOPE_KIND_GLOBALS, "moduleName", "string", agent->moduleName, 0);

    {
        TZrChar buffer[64];
        snprintf(buffer, sizeof(buffer), "%u", zr_debug_loaded_module_count(agent));
        zr_debug_fill_text_preview(&values[1], ZR_DEBUG_SCOPE_KIND_GLOBALS, "loadedModuleCount", "int", buffer, 0);
        snprintf(buffer, sizeof(buffer), "%u", zr_debug_frame_depth(agent->state));
        zr_debug_fill_text_preview(&values[2], ZR_DEBUG_SCOPE_KIND_GLOBALS, "frameDepth", "int", buffer, 0);
        snprintf(buffer, sizeof(buffer), "%d", (int)agent->state->threadStatus);
        zr_debug_fill_text_preview(&values[4], ZR_DEBUG_SCOPE_KIND_GLOBALS, "threadStatus", "int", buffer, 0);
        snprintf(buffer, sizeof(buffer), "%u", agent->lastStopEvent.line);
        zr_debug_fill_text_preview(&values[5], ZR_DEBUG_SCOPE_KIND_GLOBALS, "stopLine", "int", buffer, 0);
    }

    zr_debug_fill_text_preview(&values[3],
                               ZR_DEBUG_SCOPE_KIND_GLOBALS,
                               "hasCurrentException",
                               "bool",
                               agent->state->hasCurrentException ? "true" : "false",
                               0);

    *outValues = values;
    *outCount = count;
    return ZR_TRUE;
}

static TZrBool zr_debug_expand_prototype_summary(ZrDebugAgent *agent,
                                                 SZrObjectPrototype *prototype,
                                                 ZrDebugValuePreview **outValues,
                                                 TZrSize *outCount) {
    ZrDebugValuePreview *values;
    TZrSize count = 9;

    if (outValues != ZR_NULL) {
        *outValues = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (agent == ZR_NULL || prototype == ZR_NULL || outValues == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    values = (ZrDebugValuePreview *)calloc(count, sizeof(*values));
    if (values == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_debug_fill_text_preview(&values[0],
                               ZR_DEBUG_SCOPE_KIND_PROTOTYPE,
                               "name",
                               "string",
                               prototype->name != ZR_NULL ? zr_debug_string_native(prototype->name) : "",
                               0);
    zr_debug_fill_text_preview(&values[1],
                               ZR_DEBUG_SCOPE_KIND_PROTOTYPE,
                               "prototypeType",
                               "string",
                               zr_debug_prototype_type_name(prototype->type),
                               0);
    zr_debug_fill_text_preview(&values[2],
                               ZR_DEBUG_SCOPE_KIND_PROTOTYPE,
                               "superPrototype",
                               "prototype",
                               prototype->superPrototype != ZR_NULL && prototype->superPrototype->name != ZR_NULL
                                       ? zr_debug_string_native(prototype->superPrototype->name)
                                       : "null",
                               prototype->superPrototype != ZR_NULL
                                       ? zr_debug_register_prototype_handle(agent, prototype->superPrototype)
                                       : 0);

    {
        TZrChar buffer[64];
        snprintf(buffer, sizeof(buffer), "%u", prototype->memberDescriptorCount);
        zr_debug_fill_text_preview(&values[3], ZR_DEBUG_SCOPE_KIND_PROTOTYPE, "memberDescriptorCount", "int", buffer, 0);
        snprintf(buffer, sizeof(buffer), "%u", prototype->managedFieldCount);
        zr_debug_fill_text_preview(&values[4], ZR_DEBUG_SCOPE_KIND_PROTOTYPE, "managedFieldCount", "int", buffer, 0);
        snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)prototype->protocolMask);
        zr_debug_fill_text_preview(&values[5], ZR_DEBUG_SCOPE_KIND_PROTOTYPE, "protocolMask", "uint", buffer, 0);
    }

    zr_debug_fill_text_preview(&values[6],
                               ZR_DEBUG_SCOPE_KIND_PROTOTYPE,
                               "dynamicMemberCapable",
                               "bool",
                               prototype->dynamicMemberCapable ? "true" : "false",
                               0);
    zr_debug_fill_text_preview(&values[7],
                               ZR_DEBUG_SCOPE_KIND_PROTOTYPE,
                               "indexContract",
                               "string",
                               (prototype->indexContract.getByIndexFunction != ZR_NULL ||
                                prototype->indexContract.setByIndexFunction != ZR_NULL ||
                                prototype->indexContract.containsKeyFunction != ZR_NULL ||
                                prototype->indexContract.getLengthFunction != ZR_NULL)
                                       ? "enabled"
                                       : "none",
                               0);
    zr_debug_fill_text_preview(&values[8],
                               ZR_DEBUG_SCOPE_KIND_PROTOTYPE,
                               "managedFields",
                               "descriptorList",
                               prototype->managedFieldCount > 0 ? "available" : "none",
                               prototype->managedFieldCount > 0
                                       ? zr_debug_register_prototype_view_handle(agent,
                                                                                 prototype,
                                                                                 ZR_DEBUG_PROTOTYPE_VIEW_MANAGED_FIELDS)
                                       : 0);

    *outValues = values;
    *outCount = count;
    return ZR_TRUE;
}

static TZrBool zr_debug_expand_prototype_view(ZrDebugAgent *agent,
                                              SZrObjectPrototype *prototype,
                                              EZrDebugPrototypeViewKind viewKind,
                                              EZrDebugScopeKind scopeKind,
                                              ZrDebugValuePreview **outValues,
                                              TZrSize *outCount) {
    ZrDebugValuePreview *values = ZR_NULL;
    TZrSize count = 0;
    TZrSize index = 0;
    TZrUInt32 descriptorIndex;

    if (outValues != ZR_NULL) {
        *outValues = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (agent == ZR_NULL || prototype == ZR_NULL || outValues == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    if (viewKind == ZR_DEBUG_PROTOTYPE_VIEW_PROTOCOLS) {
        EZrProtocolId protocolId;
        for (protocolId = (EZrProtocolId)(ZR_PROTOCOL_ID_NONE + 1);
             protocolId <= ZR_PROTOCOL_ID_ARRAY_LIKE;
             protocolId = (EZrProtocolId)(protocolId + 1)) {
            if ((prototype->protocolMask & ZR_PROTOCOL_BIT(protocolId)) != 0) {
                count++;
            }
        }
        values = count > 0 ? (ZrDebugValuePreview *)calloc(count, sizeof(*values)) : ZR_NULL;
        if (count > 0 && values == ZR_NULL) {
            return ZR_FALSE;
        }
        for (protocolId = (EZrProtocolId)(ZR_PROTOCOL_ID_NONE + 1);
             protocolId <= ZR_PROTOCOL_ID_ARRAY_LIKE;
             protocolId = (EZrProtocolId)(protocolId + 1)) {
            if ((prototype->protocolMask & ZR_PROTOCOL_BIT(protocolId)) == 0) {
                continue;
            }
            zr_debug_fill_text_preview(&values[index++],
                                       scopeKind,
                                       zr_debug_protocol_name(protocolId),
                                       "protocol",
                                       "implemented",
                                       0);
        }
        *outValues = values;
        *outCount = count;
        return ZR_TRUE;
    }

    if (viewKind == ZR_DEBUG_PROTOTYPE_VIEW_MANAGED_FIELDS) {
        count = prototype->managedFieldCount;
        values = count > 0 ? (ZrDebugValuePreview *)calloc(count, sizeof(*values)) : ZR_NULL;
        if (count > 0 && values == ZR_NULL) {
            return ZR_FALSE;
        }
        for (index = 0; index < prototype->managedFieldCount; index++) {
            TZrChar buffer[ZR_DEBUG_TEXT_CAPACITY];
            snprintf(buffer,
                     sizeof(buffer),
                     "offset=%u size=%u",
                     prototype->managedFields[index].fieldOffset,
                     prototype->managedFields[index].fieldSize);
            zr_debug_fill_text_preview(&values[index],
                                       scopeKind,
                                       prototype->managedFields[index].name != ZR_NULL
                                               ? zr_debug_string_native(prototype->managedFields[index].name)
                                               : "<field>",
                                       "managedField",
                                       buffer,
                                       0);
        }
        *outValues = values;
        *outCount = count;
        return ZR_TRUE;
    }

    for (descriptorIndex = 0; descriptorIndex < prototype->memberDescriptorCount; descriptorIndex++) {
        const SZrMemberDescriptor *descriptor = &prototype->memberDescriptors[descriptorIndex];
        TZrBool include = ZR_FALSE;

        switch (viewKind) {
            case ZR_DEBUG_PROTOTYPE_VIEW_MEMBERS:
                include = (TZrBool)(descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_FIELD && !descriptor->isStatic);
                break;
            case ZR_DEBUG_PROTOTYPE_VIEW_METHODS:
                include = (TZrBool)(descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_METHOD);
                break;
            case ZR_DEBUG_PROTOTYPE_VIEW_PROPERTIES:
                include = (TZrBool)(descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY);
                break;
            case ZR_DEBUG_PROTOTYPE_VIEW_STATICS:
                include = (TZrBool)(descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_STATIC_MEMBER || descriptor->isStatic);
                break;
            case ZR_DEBUG_PROTOTYPE_VIEW_NONE:
            case ZR_DEBUG_PROTOTYPE_VIEW_PROTOCOLS:
            case ZR_DEBUG_PROTOTYPE_VIEW_MANAGED_FIELDS:
            default:
                include = ZR_FALSE;
                break;
        }

        if (include) {
            count++;
        }
    }

    values = count > 0 ? (ZrDebugValuePreview *)calloc(count, sizeof(*values)) : ZR_NULL;
    if (count > 0 && values == ZR_NULL) {
        return ZR_FALSE;
    }

    for (descriptorIndex = 0; descriptorIndex < prototype->memberDescriptorCount; descriptorIndex++) {
        const SZrMemberDescriptor *descriptor = &prototype->memberDescriptors[descriptorIndex];
        const TZrChar *kindName = "member";
        TZrBool include = ZR_FALSE;

        switch (viewKind) {
            case ZR_DEBUG_PROTOTYPE_VIEW_MEMBERS:
                include = (TZrBool)(descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_FIELD && !descriptor->isStatic);
                kindName = "field";
                break;
            case ZR_DEBUG_PROTOTYPE_VIEW_METHODS:
                include = (TZrBool)(descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_METHOD);
                kindName = "method";
                break;
            case ZR_DEBUG_PROTOTYPE_VIEW_PROPERTIES:
                include = (TZrBool)(descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY);
                kindName = "property";
                break;
            case ZR_DEBUG_PROTOTYPE_VIEW_STATICS:
                include = (TZrBool)(descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_STATIC_MEMBER || descriptor->isStatic);
                kindName = "static";
                break;
            case ZR_DEBUG_PROTOTYPE_VIEW_NONE:
            case ZR_DEBUG_PROTOTYPE_VIEW_PROTOCOLS:
            case ZR_DEBUG_PROTOTYPE_VIEW_MANAGED_FIELDS:
            default:
                include = ZR_FALSE;
                break;
        }

        if (!include) {
            continue;
        }

        {
            const SZrTypeValue *directValue = ZR_NULL;
            SZrTypeValue key;

            ZrCore_Value_ResetAsNull(&key);
            if (descriptor->name != ZR_NULL &&
                zr_debug_make_string_key(agent->state, zr_debug_string_native(descriptor->name), &key)) {
                directValue = ZrCore_Object_GetValue(agent->state, &prototype->super, &key);
            }

            if (directValue != ZR_NULL) {
                zr_debug_fill_value_preview(agent,
                                            descriptor->name != ZR_NULL ? zr_debug_string_native(descriptor->name) : "<member>",
                                            scopeKind,
                                            directValue,
                                            &values[index]);
            } else {
                zr_debug_fill_text_preview(&values[index],
                                           scopeKind,
                                           descriptor->name != ZR_NULL ? zr_debug_string_native(descriptor->name) : "<member>",
                                           kindName,
                                           descriptor->isWritable ? "writable" : "readonly",
                                           0);
            }
        }

        index++;
    }

    *outValues = values;
    *outCount = count;
    return ZR_TRUE;
}

static TZrBool zr_debug_expand_statics_scope(ZrDebugAgent *agent,
                                             SZrObjectPrototype *prototype,
                                             ZrDebugValuePreview **outValues,
                                             TZrSize *outCount) {
    return zr_debug_expand_prototype_view(agent,
                                          prototype,
                                          ZR_DEBUG_PROTOTYPE_VIEW_STATICS,
                                          ZR_DEBUG_SCOPE_KIND_STATICS,
                                          outValues,
                                          outCount);
}

static TZrBool zr_debug_expand_exception_value(ZrDebugAgent *agent,
                                               ZrDebugValuePreview **outValues,
                                               TZrSize *outCount) {
    ZrDebugValuePreview *values;

    if (outValues != ZR_NULL) {
        *outValues = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (agent == ZR_NULL || agent->state == ZR_NULL || !agent->state->hasCurrentException ||
        outValues == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    values = (ZrDebugValuePreview *)calloc(1, sizeof(*values));
    if (values == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_debug_fill_value_preview(agent,
                                "error",
                                ZR_DEBUG_SCOPE_KIND_EXCEPTION,
                                &agent->state->currentException,
                                &values[0]);
    *outValues = values;
    *outCount = 1;
    return ZR_TRUE;
}

static TZrBool zr_debug_expand_value_handle(ZrDebugAgent *agent,
                                            const SZrTypeValue *value,
                                            EZrDebugScopeKind scopeKind,
                                            ZrDebugValuePreview **outValues,
                                            TZrSize *outCount) {
    SZrObject *object;
    SZrObjectPrototype *prototype;
    ZrDebugValuePreview *values;
    TZrSize count;
    TZrSize index = 0;
    TZrSize bucketIndex;

    if (outValues != ZR_NULL) {
        *outValues = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (agent == ZR_NULL || agent->state == ZR_NULL || value == ZR_NULL ||
        outValues == ZR_NULL || outCount == ZR_NULL ||
        !(value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) ||
        value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(agent->state, value->value.object);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    prototype = zr_debug_resolve_value_prototype(agent->state, value);
    count = zr_debug_count_visible_object_entries(&object->nodeMap);
    count += prototype != ZR_NULL ? ZR_DEBUG_SYNTHETIC_OBJECT_FIELD_COUNT : 0u;

    values = count > 0 ? (ZrDebugValuePreview *)calloc(count, sizeof(*values)) : ZR_NULL;
    if (count > 0 && values == ZR_NULL) {
        return ZR_FALSE;
    }

    for (bucketIndex = 0; object->nodeMap.isValid && object->nodeMap.buckets != ZR_NULL && bucketIndex < object->nodeMap.capacity;
         bucketIndex++) {
        SZrHashKeyValuePair *pair = object->nodeMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            TZrChar nameBuffer[ZR_DEBUG_NAME_CAPACITY];

            nameBuffer[0] = '\0';
            if (pair->key.type == ZR_VALUE_TYPE_STRING && pair->key.value.object != ZR_NULL) {
                zr_debug_copy_text(nameBuffer,
                                   sizeof(nameBuffer),
                                   ZrCore_String_GetNativeString(ZR_CAST_STRING(agent->state, pair->key.value.object)));
            } else {
                zr_debug_format_value_text_safe(agent->state, &pair->key, nameBuffer, sizeof(nameBuffer));
            }

            if (strncmp(nameBuffer, "__zr_", 5) != 0) {
                zr_debug_fill_value_preview(agent, nameBuffer, scopeKind, &pair->value, &values[index]);
                index++;
            }
            pair = pair->next;
        }
    }

    if (prototype != ZR_NULL) {
        const TZrChar *prototypeName = prototype->name != ZR_NULL ? zr_debug_string_native(prototype->name) : "prototype";
        zr_debug_fill_text_preview(&values[index++],
                                   scopeKind,
                                   "__type",
                                   "prototype",
                                   prototypeName,
                                   zr_debug_register_prototype_handle(agent, prototype));
        zr_debug_fill_text_preview(&values[index++],
                                   scopeKind,
                                   "__prototype",
                                   "prototype",
                                   prototypeName,
                                   zr_debug_register_prototype_handle(agent, prototype));
        zr_debug_fill_text_preview(&values[index++],
                                   scopeKind,
                                   "__members",
                                   "descriptorList",
                                   "members",
                                   zr_debug_register_prototype_view_handle(agent,
                                                                           prototype,
                                                                           ZR_DEBUG_PROTOTYPE_VIEW_MEMBERS));
        zr_debug_fill_text_preview(&values[index++],
                                   scopeKind,
                                   "__methods",
                                   "descriptorList",
                                   "methods",
                                   zr_debug_register_prototype_view_handle(agent,
                                                                           prototype,
                                                                           ZR_DEBUG_PROTOTYPE_VIEW_METHODS));
        zr_debug_fill_text_preview(&values[index++],
                                   scopeKind,
                                   "__properties",
                                   "descriptorList",
                                   "properties",
                                   zr_debug_register_prototype_view_handle(agent,
                                                                           prototype,
                                                                           ZR_DEBUG_PROTOTYPE_VIEW_PROPERTIES));
        zr_debug_fill_text_preview(&values[index++],
                                   scopeKind,
                                   "__staticMembers",
                                   "descriptorList",
                                   "statics",
                                   zr_debug_register_prototype_view_handle(agent,
                                                                           prototype,
                                                                           ZR_DEBUG_PROTOTYPE_VIEW_STATICS));
        zr_debug_fill_text_preview(&values[index++],
                                   scopeKind,
                                   "__protocols",
                                   "descriptorList",
                                   "protocols",
                                   zr_debug_register_prototype_view_handle(agent,
                                                                           prototype,
                                                                           ZR_DEBUG_PROTOTYPE_VIEW_PROTOCOLS));
    }

    *outValues = values;
    *outCount = index;
    return ZR_TRUE;
}

void zr_debug_variable_handles_clear(ZrDebugAgent *agent) {
    if (agent == ZR_NULL) {
        return;
    }

    if (agent->variableHandles != ZR_NULL) {
        free(agent->variableHandles);
        agent->variableHandles = ZR_NULL;
    }
    agent->variableHandleCount = 0;
    agent->variableHandleCapacity = 0;
    agent->nextVariableHandleId = ZR_DEBUG_VARIABLE_HANDLE_BASE;
}

static TZrUInt32 zr_debug_find_receiver_slot(SZrFunction *function, TZrUInt32 pc, TZrChar *outName, TZrSize outNameSize) {
    SZrString *slotName;

    if (outName != ZR_NULL && outNameSize > 0) {
        outName[0] = '\0';
    }
    if (function == ZR_NULL || function->parameterCount == 0) {
        return ZR_DEBUG_INVALID_SLOT_INDEX;
    }

    slotName = ZrCore_Function_GetLocalVariableName(function, 0, pc);
    if (slotName != ZR_NULL && zr_debug_is_receiver_name(zr_debug_string_native(slotName))) {
        if (outName != ZR_NULL && outNameSize > 0) {
            zr_debug_copy_text(outName, outNameSize, zr_debug_string_native(slotName));
        }
        return 0;
    }

    return ZR_DEBUG_INVALID_SLOT_INDEX;
}

TZrBool zr_debug_resolve_identifier_value(ZrDebugAgent *agent,
                                          TZrUInt32 frameId,
                                          const TZrChar *name,
                                          SZrTypeValue *outValue,
                                          TZrChar *errorBuffer,
                                          TZrSize errorBufferSize) {
    SZrFunction *function = ZR_NULL;
    SZrCallInfo *callInfo;
    TZrUInt32 pc;
    TZrUInt32 slotIndex;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (errorBuffer != ZR_NULL && errorBufferSize > 0) {
        errorBuffer[0] = '\0';
    }
    if (agent == ZR_NULL || agent->state == ZR_NULL || name == ZR_NULL || outValue == ZR_NULL) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "invalid evaluation request");
        return ZR_FALSE;
    }

    callInfo = zr_debug_find_call_info_by_frame_id(agent, frameId, &function);
    if (callInfo != ZR_NULL && function != ZR_NULL) {
        pc = zr_debug_instruction_offset(callInfo, function);
        for (slotIndex = 0; slotIndex < function->stackSize; slotIndex++) {
            SZrString *localName = ZrCore_Function_GetLocalVariableName(function, slotIndex, pc);
            if (localName != ZR_NULL && strcmp(zr_debug_string_native(localName), name) == 0) {
                *outValue = *ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 1 + slotIndex);
                return ZR_TRUE;
            }
        }

        {
            SZrTypeValue *closureValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
            SZrClosure *closure = closureValue != ZR_NULL && closureValue->value.object != ZR_NULL
                                          && !closureValue->isNative
                                          ? ZR_CAST_VM_CLOSURE(agent->state, closureValue->value.object)
                                          : ZR_NULL;
            if (closure != ZR_NULL) {
                for (slotIndex = 0; slotIndex < function->closureValueLength; slotIndex++) {
                    if (function->closureValueList[slotIndex].name != ZR_NULL &&
                        strcmp(zr_debug_string_native(function->closureValueList[slotIndex].name), name) == 0 &&
                        slotIndex < closure->closureValueCount) {
                        const SZrTypeValue *closureSlot =
                                ZrCore_ClosureValue_GetValue(closure->closureValuesExtend[slotIndex]);
                        if (closureSlot != ZR_NULL) {
                            *outValue = *closureSlot;
                            return ZR_TRUE;
                        }
                    }
                }
            }
        }
    }

    if (strcmp(name, "zr") == 0) {
        *outValue = agent->state->global->zrObject;
        return ZR_TRUE;
    }
    if (strcmp(name, "loadedModules") == 0) {
        *outValue = agent->state->global->loadedModulesRegistry;
        return ZR_TRUE;
    }
    if (strcmp(name, "error") == 0 && agent->state->hasCurrentException) {
        *outValue = agent->state->currentException;
        return ZR_TRUE;
    }

    snprintf(errorBuffer, errorBufferSize, "unknown identifier: %s", name);
    return ZR_FALSE;
}

TZrBool zr_debug_safe_get_member_value(ZrDebugAgent *agent,
                                       const SZrTypeValue *receiver,
                                       const TZrChar *memberName,
                                       SZrTypeValue *outValue,
                                       TZrChar *errorBuffer,
                                       TZrSize errorBufferSize) {
    SZrObject *object = ZR_NULL;
    SZrObjectPrototype *prototype;
    SZrString *memberString;
    SZrTypeValue key;
    const SZrTypeValue *resolvedValue;
    TZrBool isPrototypeReceiver = ZR_FALSE;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (agent == ZR_NULL || agent->state == ZR_NULL || receiver == ZR_NULL || memberName == ZR_NULL || outValue == ZR_NULL) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "invalid member evaluation request");
        return ZR_FALSE;
    }

    if (receiver->type != ZR_VALUE_TYPE_OBJECT &&
        receiver->type != ZR_VALUE_TYPE_ARRAY &&
        receiver->type != ZR_VALUE_TYPE_STRING) {
        snprintf(errorBuffer, errorBufferSize, "member access is not supported on %s", zr_debug_value_type_name(receiver->type));
        return ZR_FALSE;
    }

    memberString = ZrCore_String_CreateFromNative(agent->state, (TZrNativeString)memberName);
    if (memberString == ZR_NULL || !zr_debug_make_string_key(agent->state, memberName, &key)) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "failed to allocate debug member name");
        return ZR_FALSE;
    }

    if ((receiver->type == ZR_VALUE_TYPE_OBJECT || receiver->type == ZR_VALUE_TYPE_ARRAY) && receiver->value.object != ZR_NULL) {
        object = ZR_CAST_OBJECT(agent->state, receiver->value.object);
        isPrototypeReceiver = (TZrBool)(object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE);
        if (object != ZR_NULL) {
            resolvedValue = ZrCore_Object_GetValue(agent->state, object, &key);
            if (resolvedValue != ZR_NULL) {
                *outValue = *resolvedValue;
                return ZR_TRUE;
            }
        }
    }

    prototype = isPrototypeReceiver ? (SZrObjectPrototype *)object : zr_debug_resolve_value_prototype(agent->state, receiver);
    while (prototype != ZR_NULL) {
        const SZrMemberDescriptor *descriptor =
                ZrCore_ObjectPrototype_FindMemberDescriptor(prototype, memberString, ZR_FALSE);

        if (descriptor != ZR_NULL) {
            if (descriptor->isStatic && !isPrototypeReceiver) {
                snprintf(errorBuffer, errorBufferSize, "static member access requires prototype receiver: %s", memberName);
                return ZR_FALSE;
            }
            if (descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY) {
                snprintf(errorBuffer, errorBufferSize, "property access is not allowed in safe debug evaluate: %s", memberName);
                return ZR_FALSE;
            }
        }

        resolvedValue = ZrCore_Object_GetValue(agent->state, &prototype->super, &key);
        if (resolvedValue != ZR_NULL && (descriptor == ZR_NULL || !descriptor->isStatic || isPrototypeReceiver)) {
            *outValue = *resolvedValue;
            return ZR_TRUE;
        }

        prototype = prototype->superPrototype;
    }

    snprintf(errorBuffer, errorBufferSize, "failed to resolve member: %s", memberName);
    return ZR_FALSE;
}

TZrBool zr_debug_safe_get_index_value(ZrDebugAgent *agent,
                                      const SZrTypeValue *receiver,
                                      const SZrTypeValue *key,
                                      SZrTypeValue *outValue,
                                      TZrChar *errorBuffer,
                                      TZrSize errorBufferSize) {
    SZrObject *object;
    SZrObjectPrototype *prototype;
    const SZrTypeValue *resolvedValue;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (agent == ZR_NULL || agent->state == ZR_NULL || receiver == ZR_NULL || key == ZR_NULL || outValue == ZR_NULL) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "invalid index evaluation request");
        return ZR_FALSE;
    }

    if ((receiver->type != ZR_VALUE_TYPE_OBJECT && receiver->type != ZR_VALUE_TYPE_ARRAY) || receiver->value.object == ZR_NULL) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "index access is only supported for object/array values");
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(agent->state, receiver->value.object);
    if (object == ZR_NULL) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "invalid index receiver");
        return ZR_FALSE;
    }

    prototype = zr_debug_resolve_value_prototype(agent->state, receiver);
    if (prototype != ZR_NULL &&
        (prototype->indexContract.getByIndexFunction != ZR_NULL ||
         prototype->indexContract.setByIndexFunction != ZR_NULL ||
         prototype->indexContract.containsKeyFunction != ZR_NULL ||
         prototype->indexContract.getLengthFunction != ZR_NULL)) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "index contract access is not allowed in safe debug evaluate");
        return ZR_FALSE;
    }

    resolvedValue = ZrCore_Object_GetValue(agent->state, object, key);
    if (resolvedValue == ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    } else {
        *outValue = *resolvedValue;
    }

    return ZR_TRUE;
}

TZrBool ZrDebug_Evaluate(ZrDebugAgent *agent,
                         TZrUInt32 frameId,
                         const TZrChar *expression,
                         ZrDebugEvaluateResult *outResult,
                         TZrChar *errorBuffer,
                         TZrSize errorBufferSize) {
    SZrTypeValue value;

    if (outResult != ZR_NULL) {
        memset(outResult, 0, sizeof(*outResult));
    }
    if (errorBuffer != ZR_NULL && errorBufferSize > 0) {
        errorBuffer[0] = '\0';
    }
    if (agent == ZR_NULL || outResult == ZR_NULL || expression == ZR_NULL) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "invalid evaluate request");
        return ZR_FALSE;
    }
    if (agent->runMode != ZR_DEBUG_RUN_MODE_PAUSED) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "evaluate is only available while paused");
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(&value);
    if (!zr_debug_evaluate_expression(agent, frameId == 0 ? 1u : frameId, expression, &value, errorBuffer, errorBufferSize)) {
        return ZR_FALSE;
    }

    zr_debug_copy_text(outResult->type_name,
                       sizeof(outResult->type_name),
                       zr_debug_value_type_name_safe(agent->state, &value));
    zr_debug_format_value_text_safe(agent->state, &value, outResult->value_text, sizeof(outResult->value_text));
    outResult->variables_reference = zr_debug_value_is_expandable(agent->state, &value)
                                             ? zr_debug_register_value_handle(agent, &value)
                                             : 0;
    return ZR_TRUE;
}

TZrBool ZrDebug_ReadStack(ZrDebugAgent *agent, ZrDebugFrameSnapshot **outFrames, TZrSize *outCount) {
    SZrCallInfo *callInfo;
    ZrDebugFrameSnapshot *frames;
    TZrUInt32 frameCount = 0;
    TZrUInt32 frameId = 1;
    TZrSize exceptionFrameCount;
    SZrFunction *topFunction = ZR_NULL;
    TZrBool needsSyntheticEntryFrame = ZR_FALSE;

    if (outFrames != ZR_NULL) {
        *outFrames = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (agent == ZR_NULL || agent->state == ZR_NULL || outFrames == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    exceptionFrameCount = agent->lastStopEvent.reason == ZR_DEBUG_STOP_REASON_EXCEPTION
                                  ? zr_debug_exception_frame_count(agent)
                                  : 0;
    if (exceptionFrameCount > 0) {
        frames = (ZrDebugFrameSnapshot *)calloc(exceptionFrameCount, sizeof(*frames));
        if (frames == ZR_NULL) {
            return ZR_FALSE;
        }

        for (frameId = 1; frameId <= exceptionFrameCount; frameId++) {
            if (!zr_debug_exception_read_frame(agent, frameId, &frames[frameId - 1])) {
                free(frames);
                return ZR_FALSE;
            }
        }

        *outFrames = frames;
        *outCount = exceptionFrameCount;
        return ZR_TRUE;
    }

    for (callInfo = agent->state->callInfoList; callInfo != ZR_NULL; callInfo = callInfo->previous) {
        SZrFunction *function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(agent->state, callInfo);
        if (function != ZR_NULL) {
            if (topFunction == ZR_NULL) {
                topFunction = function;
            }
            frameCount++;
        }
    }

    needsSyntheticEntryFrame = (TZrBool)(frameCount == 1 &&
                                         topFunction != ZR_NULL &&
                                         agent->entryFunction != ZR_NULL &&
                                         topFunction != agent->entryFunction);
    frames = (frameCount > 0 || needsSyntheticEntryFrame)
                     ? (ZrDebugFrameSnapshot *)calloc(frameCount + (needsSyntheticEntryFrame ? 1u : 0u), sizeof(*frames))
                     : ZR_NULL;
    if ((frameCount > 0 || needsSyntheticEntryFrame) && frames == ZR_NULL) {
        return ZR_FALSE;
    }

    for (callInfo = agent->state->callInfoList; callInfo != ZR_NULL; callInfo = callInfo->previous) {
        SZrFunction *function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(agent->state, callInfo);
        TZrUInt32 instructionIndex;
        TZrUInt32 receiverSlot;
        TZrChar receiverName[ZR_DEBUG_NAME_CAPACITY];
        TZrUInt32 argumentBase = 0;
        if (function == ZR_NULL) {
            continue;
        }

        receiverName[0] = '\0';
        instructionIndex = zr_debug_instruction_offset(callInfo, function);
        receiverSlot = zr_debug_find_receiver_slot(function, instructionIndex, receiverName, sizeof(receiverName));
        if (receiverSlot != ZR_DEBUG_INVALID_SLOT_INDEX) {
            const SZrTypeValue *receiverValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 1 + receiverSlot);
            argumentBase = 1u;
            zr_debug_copy_text(frames[frameId - 1].receiver_name, sizeof(frames[frameId - 1].receiver_name), receiverName);
            zr_debug_copy_text(frames[frameId - 1].call_kind, sizeof(frames[frameId - 1].call_kind), "method");
            if (receiverValue != ZR_NULL) {
                zr_debug_copy_text(frames[frameId - 1].receiver_type_name,
                                   sizeof(frames[frameId - 1].receiver_type_name),
                                   zr_debug_value_type_name_safe(agent->state, receiverValue));
                zr_debug_format_value_text_safe(agent->state,
                                                receiverValue,
                                                frames[frameId - 1].receiver_value_text,
                                                sizeof(frames[frameId - 1].receiver_value_text));
                frames[frameId - 1].receiver_variables_reference =
                        zr_debug_value_is_expandable(agent->state, receiverValue)
                                ? zr_debug_register_value_handle(agent, receiverValue)
                                : 0;
            }
        } else {
            zr_debug_copy_text(frames[frameId - 1].call_kind, sizeof(frames[frameId - 1].call_kind), "function");
        }

        frames[frameId - 1].frame_id = frameId;
        frames[frameId - 1].frame_depth = frameId - 1u;
        frames[frameId - 1].instruction_index = instructionIndex;
        frames[frameId - 1].line = zr_debug_call_info_line(agent, callInfo, function, frameId == 1u);
        frames[frameId - 1].argument_count = function->parameterCount > argumentBase
                                                     ? (function->parameterCount - argumentBase)
                                                     : 0;
        frames[frameId - 1].return_slot =
                callInfo->hasReturnDestination && callInfo->returnDestination != ZR_NULL
                        ? (TZrInt32)ZrCore_Stack_SavePointerAsOffset(agent->state, callInfo->returnDestination)
                        : -1;
        frames[frameId - 1].is_exception_frame = ZR_FALSE;
        zr_debug_copy_text(frames[frameId - 1].module_name, sizeof(frames[frameId - 1].module_name), agent->moduleName);
        zr_debug_copy_text(frames[frameId - 1].function_name,
                           sizeof(frames[frameId - 1].function_name),
                           zr_debug_function_name(function));
        zr_debug_copy_text(frames[frameId - 1].source_file,
                           sizeof(frames[frameId - 1].source_file),
                           zr_debug_function_source(function));
        frameId++;
    }

    if (needsSyntheticEntryFrame) {
        ZrDebugFrameSnapshot *entryFrame = &frames[frameId - 1];
        entryFrame->frame_id = frameId;
        entryFrame->frame_depth = frameId - 1u;
        entryFrame->instruction_index = 0;
        entryFrame->line = agent->entryFunction->lineInSourceEnd != 0
                                   ? agent->entryFunction->lineInSourceEnd
                                   : agent->entryFunction->lineInSourceStart;
        entryFrame->argument_count = 0;
        entryFrame->return_slot = -1;
        entryFrame->is_exception_frame = ZR_FALSE;
        zr_debug_copy_text(entryFrame->module_name, sizeof(entryFrame->module_name), agent->moduleName);
        zr_debug_copy_text(entryFrame->call_kind, sizeof(entryFrame->call_kind), "function");
        zr_debug_copy_text(entryFrame->function_name,
                           sizeof(entryFrame->function_name),
                           zr_debug_function_name(agent->entryFunction));
        zr_debug_copy_text(entryFrame->source_file,
                           sizeof(entryFrame->source_file),
                           zr_debug_function_source(agent->entryFunction));
        frameId++;
    }

    *outFrames = frames;
    *outCount = frameId - 1u;
    return ZR_TRUE;
}

TZrBool ZrDebug_ReadScopes(ZrDebugAgent *agent,
                           TZrUInt32 frameId,
                           ZrDebugScopeSnapshot **outScopes,
                           TZrSize *outCount) {
    SZrFunction *function = ZR_NULL;
    SZrCallInfo *callInfo;
    ZrDebugScopeSnapshot *scopes = ZR_NULL;
    TZrSize count = 0;
    TZrSize index = 0;
    TZrUInt32 instructionIndex;
    TZrUInt32 receiverSlot;

    if (outScopes != ZR_NULL) {
        *outScopes = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (agent == ZR_NULL || outScopes == ZR_NULL || outCount == ZR_NULL || frameId == 0) {
        return ZR_FALSE;
    }

    if (agent->lastStopEvent.reason == ZR_DEBUG_STOP_REASON_EXCEPTION &&
        zr_debug_exception_frame_count(agent) > 0) {
        count = 1;
        scopes = (ZrDebugScopeSnapshot *)calloc(count, sizeof(*scopes));
        if (scopes == ZR_NULL) {
            return ZR_FALSE;
        }
        scopes[0].frame_id = frameId;
        scopes[0].kind = ZR_DEBUG_SCOPE_KIND_EXCEPTION;
        scopes[0].scope_id = zr_debug_scope_id(frameId, scopes[0].kind);
        zr_debug_copy_text(scopes[0].name, sizeof(scopes[0].name), zr_debug_scope_kind_name(scopes[0].kind));
        *outScopes = scopes;
        *outCount = count;
        return ZR_TRUE;
    }

    callInfo = zr_debug_find_call_info_by_frame_id(agent, frameId, &function);
    if (callInfo == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    count = 3; /* Arguments + Locals + Globals */
    if (function->closureValueLength > 0) {
        count++;
    }

    instructionIndex = zr_debug_instruction_offset(callInfo, function);
    receiverSlot = zr_debug_find_receiver_slot(function, instructionIndex, ZR_NULL, 0);
    if (receiverSlot != ZR_DEBUG_INVALID_SLOT_INDEX) {
        count += 2; /* Prototype + Statics */
    }
    if (agent->state->hasCurrentException) {
        count++;
    }

    scopes = (ZrDebugScopeSnapshot *)calloc(count, sizeof(*scopes));
    if (scopes == ZR_NULL) {
        return ZR_FALSE;
    }

    scopes[index].frame_id = frameId;
    scopes[index].kind = ZR_DEBUG_SCOPE_KIND_ARGUMENTS;
    scopes[index].scope_id = zr_debug_scope_id(frameId, scopes[index].kind);
    zr_debug_copy_text(scopes[index].name, sizeof(scopes[index].name), zr_debug_scope_kind_name(scopes[index].kind));
    index++;

    scopes[index].frame_id = frameId;
    scopes[index].kind = ZR_DEBUG_SCOPE_KIND_LOCALS;
    scopes[index].scope_id = zr_debug_scope_id(frameId, scopes[index].kind);
    zr_debug_copy_text(scopes[index].name, sizeof(scopes[index].name), zr_debug_scope_kind_name(scopes[index].kind));
    index++;

    if (function->closureValueLength > 0) {
        scopes[index].frame_id = frameId;
        scopes[index].kind = ZR_DEBUG_SCOPE_KIND_CLOSURES;
        scopes[index].scope_id = zr_debug_scope_id(frameId, scopes[index].kind);
        zr_debug_copy_text(scopes[index].name, sizeof(scopes[index].name), zr_debug_scope_kind_name(scopes[index].kind));
        index++;
    }

    scopes[index].frame_id = frameId;
    scopes[index].kind = ZR_DEBUG_SCOPE_KIND_GLOBALS;
    scopes[index].scope_id = zr_debug_scope_id(frameId, scopes[index].kind);
    zr_debug_copy_text(scopes[index].name, sizeof(scopes[index].name), zr_debug_scope_kind_name(scopes[index].kind));
    index++;

    if (receiverSlot != ZR_DEBUG_INVALID_SLOT_INDEX) {
        scopes[index].frame_id = frameId;
        scopes[index].kind = ZR_DEBUG_SCOPE_KIND_PROTOTYPE;
        scopes[index].scope_id = zr_debug_scope_id(frameId, scopes[index].kind);
        zr_debug_copy_text(scopes[index].name, sizeof(scopes[index].name), zr_debug_scope_kind_name(scopes[index].kind));
        index++;

        scopes[index].frame_id = frameId;
        scopes[index].kind = ZR_DEBUG_SCOPE_KIND_STATICS;
        scopes[index].scope_id = zr_debug_scope_id(frameId, scopes[index].kind);
        zr_debug_copy_text(scopes[index].name, sizeof(scopes[index].name), zr_debug_scope_kind_name(scopes[index].kind));
        index++;
    }

    if (agent->state->hasCurrentException) {
        scopes[index].frame_id = frameId;
        scopes[index].kind = ZR_DEBUG_SCOPE_KIND_EXCEPTION;
        scopes[index].scope_id = zr_debug_scope_id(frameId, scopes[index].kind);
        zr_debug_copy_text(scopes[index].name, sizeof(scopes[index].name), zr_debug_scope_kind_name(scopes[index].kind));
        index++;
    }

    *outScopes = scopes;
    *outCount = index;
    return ZR_TRUE;
}

TZrBool ZrDebug_ReadVariables(ZrDebugAgent *agent,
                              TZrUInt32 scopeId,
                              ZrDebugValuePreview **outValues,
                              TZrSize *outCount) {
    ZrDebugVariableHandle *handle;
    TZrUInt32 frameId;
    EZrDebugScopeKind scopeKind;
    SZrFunction *function = ZR_NULL;
    SZrCallInfo *callInfo;

    if (outValues != ZR_NULL) {
        *outValues = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (agent == ZR_NULL || outValues == ZR_NULL || outCount == ZR_NULL || scopeId == 0) {
        return ZR_FALSE;
    }

    handle = zr_debug_find_variable_handle(agent, scopeId);
    if (handle != ZR_NULL) {
        switch (handle->kind) {
            case ZR_DEBUG_VARIABLE_HANDLE_KIND_VALUE:
                return zr_debug_expand_value_handle(agent,
                                                    &handle->value,
                                                    ZR_DEBUG_SCOPE_KIND_LOCALS,
                                                    outValues,
                                                    outCount);
            case ZR_DEBUG_VARIABLE_HANDLE_KIND_GLOBAL_STATE:
                return zr_debug_expand_global_state(agent, outValues, outCount);
            case ZR_DEBUG_VARIABLE_HANDLE_KIND_PROTOTYPE:
                return zr_debug_expand_prototype_summary(agent, handle->prototype, outValues, outCount);
            case ZR_DEBUG_VARIABLE_HANDLE_KIND_PROTOTYPE_VIEW:
                return zr_debug_expand_prototype_view(agent,
                                                      handle->prototype,
                                                      handle->prototype_view_kind,
                                                      ZR_DEBUG_SCOPE_KIND_PROTOTYPE,
                                                      outValues,
                                                      outCount);
            case ZR_DEBUG_VARIABLE_HANDLE_KIND_EXCEPTION:
                return zr_debug_expand_exception_value(agent, outValues, outCount);
            case ZR_DEBUG_VARIABLE_HANDLE_KIND_NONE:
            default:
                return ZR_FALSE;
        }
    }

    frameId = scopeId / 10u;
    scopeKind = (EZrDebugScopeKind)(scopeId % 10u);
    if (frameId == 0) {
        return ZR_FALSE;
    }

    if (scopeKind == ZR_DEBUG_SCOPE_KIND_EXCEPTION) {
        return zr_debug_expand_exception_value(agent, outValues, outCount);
    }

    callInfo = zr_debug_find_call_info_by_frame_id(agent, frameId, &function);
    if (callInfo == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    if (scopeKind == ZR_DEBUG_SCOPE_KIND_ARGUMENTS || scopeKind == ZR_DEBUG_SCOPE_KIND_LOCALS) {
        ZrDebugValuePreview *values = ZR_NULL;
        TZrSize valueCount = 0;
        TZrSize index = 0;
        TZrUInt32 pc = zr_debug_instruction_offset(callInfo, function);
        TZrUInt32 receiverSlot = zr_debug_find_receiver_slot(function, pc, ZR_NULL, 0);
        TZrUInt32 slotIndex;
        TZrUInt32 startSlot = 0;
        TZrUInt32 endSlot = function->stackSize;

        if (scopeKind == ZR_DEBUG_SCOPE_KIND_ARGUMENTS) {
            startSlot = receiverSlot != ZR_DEBUG_INVALID_SLOT_INDEX ? 1u : 0u;
            endSlot = function->parameterCount;
        } else {
            startSlot = receiverSlot != ZR_DEBUG_INVALID_SLOT_INDEX ? 1u : 0u;
            endSlot = function->stackSize;
        }
        if (endSlot > function->stackSize) {
            endSlot = function->stackSize;
        }

        for (slotIndex = startSlot; slotIndex < endSlot; slotIndex++) {
            if (ZrCore_Function_GetLocalVariableName(function, slotIndex, pc) != ZR_NULL) {
                valueCount++;
            }
        }

        values = valueCount > 0 ? (ZrDebugValuePreview *)calloc(valueCount, sizeof(*values)) : ZR_NULL;
        if (valueCount > 0 && values == ZR_NULL) {
            return ZR_FALSE;
        }

        for (slotIndex = startSlot; slotIndex < endSlot; slotIndex++) {
            SZrString *name = ZrCore_Function_GetLocalVariableName(function, slotIndex, pc);
            if (name == ZR_NULL) {
                continue;
            }
            zr_debug_fill_value_preview(agent,
                                        zr_debug_string_native(name),
                                        scopeKind,
                                        ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 1 + slotIndex),
                                        &values[index]);
            index++;
        }

        *outValues = values;
        *outCount = valueCount;
        return ZR_TRUE;
    }

    if (scopeKind == ZR_DEBUG_SCOPE_KIND_CLOSURES) {
        ZrDebugValuePreview *values = ZR_NULL;
        SZrTypeValue *closureValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
        SZrClosure *closure = closureValue != ZR_NULL && closureValue->value.object != ZR_NULL
                                      && !closureValue->isNative
                                      ? ZR_CAST_VM_CLOSURE(agent->state, closureValue->value.object)
                                      : ZR_NULL;
        TZrUInt32 closureIndex;

        values = function->closureValueLength > 0 ? (ZrDebugValuePreview *)calloc(function->closureValueLength, sizeof(*values))
                                                  : ZR_NULL;
        if (function->closureValueLength > 0 && values == ZR_NULL) {
            return ZR_FALSE;
        }

        for (closureIndex = 0; closureIndex < function->closureValueLength; closureIndex++) {
            const SZrTypeValue *value = closure != ZR_NULL && closureIndex < closure->closureValueCount
                                                ? ZrCore_ClosureValue_GetValue(closure->closureValuesExtend[closureIndex])
                                                : &agent->state->global->nullValue;
            zr_debug_fill_value_preview(agent,
                                        function->closureValueList[closureIndex].name != ZR_NULL
                                                ? zr_debug_string_native(function->closureValueList[closureIndex].name)
                                                : "<closure>",
                                        scopeKind,
                                        value,
                                        &values[closureIndex]);
        }

        *outValues = values;
        *outCount = function->closureValueLength;
        return ZR_TRUE;
    }

    if (scopeKind == ZR_DEBUG_SCOPE_KIND_GLOBALS) {
        ZrDebugValuePreview *values = (ZrDebugValuePreview *)calloc(agent->state->hasCurrentException ? 4u : 3u, sizeof(*values));
        TZrSize index = 0;
        if (values == ZR_NULL) {
            return ZR_FALSE;
        }

        zr_debug_fill_text_preview(&values[index++],
                                   scopeKind,
                                   "zrState",
                                   "debugState",
                                   "state",
                                   zr_debug_register_global_state_handle(agent));
        zr_debug_fill_value_preview(agent,
                                    "loadedModules",
                                    scopeKind,
                                    &agent->state->global->loadedModulesRegistry,
                                    &values[index++]);
        zr_debug_fill_value_preview(agent,
                                    "zr",
                                    scopeKind,
                                    &agent->state->global->zrObject,
                                    &values[index++]);
        if (agent->state->hasCurrentException) {
            zr_debug_fill_text_preview(&values[index++],
                                       scopeKind,
                                       "error",
                                       "exception",
                                       "currentException",
                                       zr_debug_register_exception_handle(agent));
        }

        *outValues = values;
        *outCount = index;
        return ZR_TRUE;
    }

    if (scopeKind == ZR_DEBUG_SCOPE_KIND_PROTOTYPE || scopeKind == ZR_DEBUG_SCOPE_KIND_STATICS) {
        TZrUInt32 pc = zr_debug_instruction_offset(callInfo, function);
        TZrUInt32 receiverSlot = zr_debug_find_receiver_slot(function, pc, ZR_NULL, 0);
        const SZrTypeValue *receiverValue;
        SZrObjectPrototype *prototype;

        if (receiverSlot == ZR_DEBUG_INVALID_SLOT_INDEX) {
            return ZR_FALSE;
        }

        receiverValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 1 + receiverSlot);
        prototype = zr_debug_resolve_value_prototype(agent->state, receiverValue);
        if (prototype == ZR_NULL) {
            return ZR_FALSE;
        }

        if (scopeKind == ZR_DEBUG_SCOPE_KIND_PROTOTYPE) {
            return zr_debug_expand_prototype_summary(agent, prototype, outValues, outCount);
        }

        return zr_debug_expand_statics_scope(agent, prototype, outValues, outCount);
    }

    return ZR_FALSE;
}

void ZrDebug_Free(void *pointer) {
    if (pointer != ZR_NULL) {
        free(pointer);
    }
}
