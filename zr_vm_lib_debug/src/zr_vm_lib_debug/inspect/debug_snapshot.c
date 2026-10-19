#include "debug_internal.h"

#include "zr_vm_core/debug.h"

/* 原型对象固定展示六个入口；无效槽哨兵与真实栈槽索引分离。 */
#define ZR_DEBUG_TYPE_OBJECT_FIELD_COUNT ((TZrSize)6u)
#define ZR_DEBUG_INVALID_SLOT_INDEX ((TZrUInt32)0xFFFFFFFFu)
/* 长字符串的子变量以字节区间分页，区间名也使用字节偏移。 */
#define ZR_DEBUG_STRING_CHUNK_SIZE ((TZrSize)64u)

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

static void zr_debug_copy_value_snapshot(SZrTypeValue *destination, const SZrTypeValue *source) {
    if (destination == ZR_NULL) {
        return;
    }

    ZrCore_Value_ResetAsNull(destination);
    if (source == ZR_NULL) {
        return;
    }

    /* 调试快照只借用 VM 值：不能复制独占/共享所有权后再按普通值释放。 */
    *destination = *source;
    if (!destination->isGarbageCollectable ||
        ZR_VALUE_IS_TYPE_NULL(destination->type) ||
        destination->value.object == ZR_NULL) {
        destination->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
        destination->ownershipControl = ZR_NULL;
        destination->ownershipWeakRef = ZR_NULL;
    } else if (destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE) {
        destination->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_BORROWED;
        destination->ownershipWeakRef = ZR_NULL;
    }
}

static TZrBool zr_debug_is_receiver_name(const TZrChar *name) {
    return (TZrBool)(name != ZR_NULL && (strcmp(name, "this") == 0 || strcmp(name, "self") == 0));
}

static SZrClosure *zr_debug_try_get_vm_closure_from_value(const SZrTypeValue *value) {
    SZrRawObject *rawObject;

    if (value == ZR_NULL ||
        value->type != ZR_VALUE_TYPE_CLOSURE ||
        value->isNative ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    rawObject = value->value.object;
    if (rawObject->type != ZR_RAW_OBJECT_TYPE_CLOSURE || rawObject->isNative) {
        return ZR_NULL;
    }

    return ZR_CAST(SZrClosure *, rawObject);
}

const SZrTypeValue *zr_debug_frame_value_slot(SZrState *state,
                                              const SZrFunction *function,
                                              const SZrCallInfo *callInfo,
                                              TZrUInt32 stackSlot) {
    TZrStackValuePointer frameBase;
    const SZrFunctionFrameSlotLayout *slotLayout;
    SZrStackFramePlace place;

    if (callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL) {
        return ZR_NULL;
    }

    frameBase = callInfo->functionBase.valuePointer + 1;
    if (function == ZR_NULL ||
        function->frameSlotLayouts == ZR_NULL ||
        function->frameSlotLayoutLength == 0u ||
        state == ZR_NULL) {
        return ZrCore_Stack_GetValue(frameBase + stackSlot);
    }

    /* 内联值槽须按编译器帧布局取地址；其余槽仍走标准栈值接口。 */
    slotLayout = ZrCore_Function_FindFrameSlotLayout(function, stackSlot);
    if (slotLayout != ZR_NULL &&
        slotLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE &&
        slotLayout->byteSize >= (TZrUInt32)sizeof(SZrTypeValue) &&
        ZrCore_Function_MakeFrameSlotPlace(state, function, frameBase, stackSlot, &place)) {
        return (const SZrTypeValue *)place.address;
    }

    return ZrCore_Stack_GetValue(frameBase + stackSlot);
}

static const TZrChar *zr_debug_protocol_name(EZrProtocolId protocolId) {
    switch (protocolId) {
        case ZR_PROTOCOL_ID_EQUATABLE:
            return "zr.builtin.IEquatable";
        case ZR_PROTOCOL_ID_HASHABLE:
            return "zr.builtin.IHashable";
        case ZR_PROTOCOL_ID_COMPARABLE:
            return "zr.builtin.IComparable";
        case ZR_PROTOCOL_ID_ITERABLE:
            return "zr.iteration.Iterable";
        case ZR_PROTOCOL_ID_ITERATOR:
            return "zr.iteration.Enumerator";
        case ZR_PROTOCOL_ID_ARRAY_LIKE:
            return "zr.builtin.IArrayLike";
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

static SZrString *zr_debug_value_as_string(SZrState *state, const SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL ||
        value->type != ZR_VALUE_TYPE_STRING ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_STRING(state, value->value.object);
}

static TZrSize zr_debug_string_chunk_count(TZrSize byteLength) {
    return byteLength == 0u ? 0u : ((byteLength + ZR_DEBUG_STRING_CHUNK_SIZE - 1u) / ZR_DEBUG_STRING_CHUNK_SIZE);
}

static TZrBool zr_debug_string_needs_chunk_handle(SZrState *state,
                                                  const SZrTypeValue *value,
                                                  TZrSize *outChunkCount) {
    SZrString *stringValue = zr_debug_value_as_string(state, value);
    TZrSize byteLength;
    TZrSize chunkCount;

    if (outChunkCount != ZR_NULL) {
        *outChunkCount = 0;
    }
    if (stringValue == ZR_NULL) {
        return ZR_FALSE;
    }

    byteLength = ZrCore_String_GetByteLength(stringValue);
    /* 原始字节长达到固定阈值时才暴露分页句柄；短字符串仍按标量展示。 */
    if (byteLength < ZR_DEBUG_TEXT_CAPACITY) {
        return ZR_FALSE;
    }

    chunkCount = zr_debug_string_chunk_count(byteLength);
    if (outChunkCount != ZR_NULL) {
        *outChunkCount = chunkCount;
    }
    return chunkCount > 0 ? ZR_TRUE : ZR_FALSE;
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
    return ZrCore_Object_SuperArrayLength(frames);
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

    /* 异常对象中的 stacks 数组使用零基索引，协议帧 ID 使用一基索引。 */
    ZrCore_Value_InitAsInt(agent->state, &key, (TZrInt64)(frameId - 1u));
    frameValue = ZrCore_Object_GetValue(agent->state, framesObject, &key);
    if (frameValue == ZR_NULL || frameValue->type != ZR_VALUE_TYPE_OBJECT || frameValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    frameObject = ZR_CAST_OBJECT(agent->state, frameValue->value.object);
    outFrame->thread_id = zr_debug_agent_effective_thread_id(agent, 0);
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
        case ZR_OBJECT_PROTOTYPE_TYPE_UNION:
            return "union";
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
    SZrDebugActivation activation;

    if (outFunction != ZR_NULL) {
        *outFunction = ZR_NULL;
    }
    if (agent == ZR_NULL || agent->state == ZR_NULL || frameId == 0) {
        return ZR_NULL;
    }

    memset(&activation, 0, sizeof(activation));
    if (!ZrCore_Debug_GetStack(agent->state, frameId - 1u, &activation)) {
        return ZR_NULL;
    }
    if (outFunction != ZR_NULL) {
        *outFunction = activation.function;
    }
    return activation.callInfo;
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

    /* 断点日志、求值及变量预览共用此路径；对象只展示类型，不调用用户代码。 */
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
        /* 恢复运行或下一次停点后的句柄即使数字相同也不能复用。 */
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

    if (agent == ZR_NULL || agent->nextVariableHandleId < ZR_DEBUG_VARIABLE_HANDLE_BASE) {
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
    /* ID 单调递增且清表时不回退；低于保留区表示溢出后停止分配。 */
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

static TZrUInt32 zr_debug_register_index_window_handle(ZrDebugAgent *agent,
                                                       const SZrTypeValue *value,
                                                       TZrSize start,
                                                       TZrSize count) {
    ZrDebugVariableHandle *handle;

    if (agent == ZR_NULL || agent->state == ZR_NULL || value == ZR_NULL ||
        value->value.object == ZR_NULL ||
        (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY)) {
        return 0;
    }

    handle = zr_debug_alloc_variable_handle(agent);
    if (handle == ZR_NULL) {
        return 0;
    }

    handle->kind = ZR_DEBUG_VARIABLE_HANDLE_KIND_INDEX_WINDOW;
    handle->value = *value;
    handle->prototype = zr_debug_resolve_value_prototype(agent->state, value);
    handle->window_start = start;
    handle->window_count = count;
    return handle->handle_id;
}

static TZrUInt32 zr_debug_register_inline_union_handle(ZrDebugAgent *agent,
                                                       TZrUInt32 frameId,
                                                       TZrUInt32 stackSlot) {
    ZrDebugVariableHandle *handle;

    if (agent == ZR_NULL || agent->state == ZR_NULL || frameId == 0u) {
        return 0;
    }

    handle = zr_debug_alloc_variable_handle(agent);
    if (handle == ZR_NULL) {
        return 0;
    }

    handle->kind = ZR_DEBUG_VARIABLE_HANDLE_KIND_INLINE_UNION;
    handle->frame_id = frameId;
    handle->stack_slot = stackSlot;
    return handle->handle_id;
}

static TZrUInt32 zr_debug_register_string_chunks_handle(ZrDebugAgent *agent, const SZrTypeValue *value) {
    ZrDebugVariableHandle *handle;
    TZrSize chunkCount;

    if (agent == ZR_NULL || agent->state == ZR_NULL || value == ZR_NULL ||
        !zr_debug_string_needs_chunk_handle(agent->state, value, &chunkCount)) {
        return 0;
    }

    handle = zr_debug_alloc_variable_handle(agent);
    if (handle == ZR_NULL) {
        return 0;
    }

    handle->kind = ZR_DEBUG_VARIABLE_HANDLE_KIND_STRING_CHUNKS;
    zr_debug_copy_value_snapshot(&handle->value, value);
    handle->window_start = 0;
    handle->window_count = chunkCount;
    return handle->handle_id;
}

static void zr_debug_attach_string_chunk_handle(ZrDebugAgent *agent,
                                                const SZrTypeValue *value,
                                                TZrUInt32 *variablesReference,
                                                TZrSize *namedVariables,
                                                TZrSize *indexedVariables) {
    TZrSize chunkCount = 0;
    TZrUInt32 handleId;

    if (agent == ZR_NULL || agent->state == ZR_NULL || value == ZR_NULL ||
        !zr_debug_string_needs_chunk_handle(agent->state, value, &chunkCount)) {
        return;
    }

    handleId = zr_debug_register_string_chunks_handle(agent, value);
    if (handleId == 0) {
        return;
    }

    if (variablesReference != ZR_NULL) {
        *variablesReference = handleId;
    }
    if (namedVariables != ZR_NULL) {
        *namedVariables = 0;
    }
    if (indexedVariables != ZR_NULL) {
        *indexedVariables = chunkCount;
    }
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
    zr_debug_copy_text(preview->semantic_summary,
                       sizeof(preview->semantic_summary),
                       valueText != ZR_NULL && valueText[0] != '\0' ? valueText : preview->type_name);
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

    /* 子节点句柄随本次停点注册；预览文本和子节点数量从同一值生成。 */
    if (zr_debug_value_is_expandable(agent->state, value)) {
        variablesReference = zr_debug_register_value_handle(agent, value);
    }

    zr_debug_fill_text_preview(preview,
                               scopeKind,
                               name,
                               zr_debug_value_type_name_safe(agent->state, value),
                               "",
                               variablesReference);
    zr_debug_value_child_shape(agent->state,
                               value,
                               &preview->named_variables,
                               &preview->indexed_variables);
    if (!zr_debug_format_union_value_text(agent, value, preview->value_text, sizeof(preview->value_text))) {
        zr_debug_format_value_text_safe(agent->state, value, preview->value_text, sizeof(preview->value_text));
    }
    zr_debug_attach_string_chunk_handle(agent,
                                        value,
                                        &preview->variables_reference,
                                        &preview->named_variables,
                                        &preview->indexed_variables);
    zr_debug_value_semantic_summary(agent->state,
                                    value,
                                    preview->named_variables,
                                    preview->indexed_variables,
                                    preview->semantic_summary,
                                    sizeof(preview->semantic_summary));
    return ZR_TRUE;
}

static TZrBool zr_debug_key_to_index(const SZrTypeValue *key, TZrSize *outIndex) {
    TZrUInt64 indexValue;

    if (outIndex != ZR_NULL) {
        *outIndex = 0;
    }
    if (key == ZR_NULL || outIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(key->type)) {
        if (key->value.nativeObject.nativeInt64 < 0) {
            return ZR_FALSE;
        }
        indexValue = (TZrUInt64)key->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(key->type)) {
        indexValue = key->value.nativeObject.nativeUInt64;
    } else {
        return ZR_FALSE;
    }

    *outIndex = (TZrSize)indexValue;
    return ZR_TRUE;
}

static void zr_debug_read_indexed_storage_value(SZrState *state,
                                                SZrObject *storage,
                                                TZrSize index,
                                                SZrTypeValue *outValue) {
    SZrTypeValue key;
    const SZrTypeValue *resolvedValue;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (state == ZR_NULL || storage == ZR_NULL || outValue == ZR_NULL) {
        return;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    resolvedValue = ZrCore_Object_GetValue(state, storage, &key);
    if (resolvedValue != ZR_NULL) {
        zr_debug_copy_value_snapshot(outValue, resolvedValue);
    }
}

TZrBool zr_debug_try_resolve_indexed_storage(SZrState *state,
                                             const SZrTypeValue *value,
                                             SZrObject **outStorage,
                                             const TZrChar **outSyntheticName,
                                             TZrSize *outLength) {
    SZrObject *object;
    const SZrTypeValue *storageValue;

    if (outStorage != ZR_NULL) {
        *outStorage = ZR_NULL;
    }
    if (outSyntheticName != ZR_NULL) {
        *outSyntheticName = ZR_NULL;
    }
    if (outLength != ZR_NULL) {
        *outLength = 0;
    }
    if (state == ZR_NULL || value == ZR_NULL || value->value.object == ZR_NULL ||
        (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(state, value->value.object);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (object->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        if (outStorage != ZR_NULL) {
            *outStorage = object;
        }
        if (outLength != ZR_NULL) {
            *outLength = ZrCore_Object_SuperArrayLength(object);
        }
        return ZR_TRUE;
    }

    /* 库容器的内部数组只作为调试索引来源，并以 data/entries 合成节点公开。 */
    storageValue = zr_debug_object_get_field(state, object, "__zr_items");
    if (storageValue != ZR_NULL && storageValue->type == ZR_VALUE_TYPE_ARRAY && storageValue->value.object != ZR_NULL) {
        if (outStorage != ZR_NULL) {
            *outStorage = ZR_CAST_OBJECT(state, storageValue->value.object);
        }
        if (outSyntheticName != ZR_NULL) {
            *outSyntheticName = "data";
        }
        if (outLength != ZR_NULL && outStorage != ZR_NULL && *outStorage != ZR_NULL &&
            (*outStorage)->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
            *outLength = ZrCore_Object_SuperArrayLength(*outStorage);
        }
        return outStorage == ZR_NULL || *outStorage != ZR_NULL;
    }

    storageValue = zr_debug_object_get_field(state, object, "__zr_entries");
    if (storageValue != ZR_NULL && storageValue->type == ZR_VALUE_TYPE_ARRAY && storageValue->value.object != ZR_NULL) {
        if (outStorage != ZR_NULL) {
            *outStorage = ZR_CAST_OBJECT(state, storageValue->value.object);
        }
        if (outSyntheticName != ZR_NULL) {
            *outSyntheticName = "entries";
        }
        if (outLength != ZR_NULL && outStorage != ZR_NULL && *outStorage != ZR_NULL &&
            (*outStorage)->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
            *outLength = ZrCore_Object_SuperArrayLength(*outStorage);
        }
        return outStorage == ZR_NULL || *outStorage != ZR_NULL;
    }

    return ZR_FALSE;
}

static TZrBool zr_debug_expand_indexed_window(ZrDebugAgent *agent,
                                              SZrObject *storage,
                                              TZrSize baseIndex,
                                              TZrSize totalIndexed,
                                              TZrSize start,
                                              TZrSize countLimit,
                                              EZrDebugScopeKind scopeKind,
                                              ZrDebugValuePreview **outValues,
                                              TZrSize *outCount,
                                              TZrSize *outNamedVariables,
                                              TZrSize *outIndexedVariables) {
    ZrDebugValuePreview *values = ZR_NULL;
    TZrSize windowStart;
    TZrSize windowEnd;
    TZrSize windowCount;
    TZrSize index;

    if (outValues != ZR_NULL) {
        *outValues = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (outNamedVariables != ZR_NULL) {
        *outNamedVariables = 0;
    }
    if (outIndexedVariables != ZR_NULL) {
        *outIndexedVariables = totalIndexed;
    }
    if (agent == ZR_NULL || agent->state == ZR_NULL || storage == ZR_NULL ||
        outValues == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    /* start/count 是窗口内偏移，baseIndex 才是原数组中的绝对索引。 */
    windowStart = start > totalIndexed ? totalIndexed : start;
    if (countLimit == 0 || windowStart + countLimit > totalIndexed) {
        windowEnd = totalIndexed;
    } else {
        windowEnd = windowStart + countLimit;
    }
    windowCount = windowEnd > windowStart ? (windowEnd - windowStart) : 0;

    values = windowCount > 0 ? (ZrDebugValuePreview *)calloc(windowCount, sizeof(*values)) : ZR_NULL;
    if (windowCount > 0 && values == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < windowCount; index++) {
        TZrChar nameBuffer[ZR_DEBUG_NAME_CAPACITY];
        SZrTypeValue entryValue;
        TZrSize actualIndex = baseIndex + windowStart + index;

        snprintf(nameBuffer, sizeof(nameBuffer), "%llu", (unsigned long long)actualIndex);
        nameBuffer[sizeof(nameBuffer) - 1u] = '\0';
        ZrCore_Value_ResetAsNull(&entryValue);
        zr_debug_read_indexed_storage_value(agent->state, storage, actualIndex, &entryValue);
        zr_debug_fill_value_preview(agent, nameBuffer, scopeKind, &entryValue, &values[index]);
    }

    *outValues = values;
    *outCount = windowCount;
    return ZR_TRUE;
}

static TZrBool zr_debug_expand_string_chunks_handle(ZrDebugAgent *agent,
                                                    const SZrTypeValue *value,
                                                    TZrSize start,
                                                    TZrSize countLimit,
                                                    EZrDebugScopeKind scopeKind,
                                                    ZrDebugValuePreview **outValues,
                                                    TZrSize *outCount,
                                                    TZrSize *outNamedVariables,
                                                    TZrSize *outIndexedVariables) {
    SZrString *stringValue;
    const TZrChar *nativeText;
    ZrDebugValuePreview *values;
    TZrSize byteLength;
    TZrSize totalChunks;
    TZrSize windowStart;
    TZrSize windowEnd;
    TZrSize windowCount;
    TZrSize index;

    if (outValues != ZR_NULL) {
        *outValues = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (outNamedVariables != ZR_NULL) {
        *outNamedVariables = 0;
    }
    if (outIndexedVariables != ZR_NULL) {
        *outIndexedVariables = 0;
    }
    if (agent == ZR_NULL || agent->state == ZR_NULL || value == ZR_NULL ||
        outValues == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    stringValue = zr_debug_value_as_string(agent->state, value);
    if (stringValue == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeText = ZrCore_String_GetNativeString(stringValue);
    byteLength = ZrCore_String_GetByteLength(stringValue);
    totalChunks = zr_debug_string_chunk_count(byteLength);
    if (outIndexedVariables != ZR_NULL) {
        *outIndexedVariables = totalChunks;
    }

    windowStart = start > totalChunks ? totalChunks : start;
    if (countLimit == 0 || windowStart + countLimit > totalChunks) {
        windowEnd = totalChunks;
    } else {
        windowEnd = windowStart + countLimit;
    }
    windowCount = windowEnd > windowStart ? (windowEnd - windowStart) : 0;

    values = windowCount > 0 ? (ZrDebugValuePreview *)calloc(windowCount, sizeof(*values)) : ZR_NULL;
    if (windowCount > 0 && values == ZR_NULL) {
        return ZR_FALSE;
    }

    /* BUG: 长字符串按固定 64 字节切块，63 个 ASCII 后接 é 时首块仅含 C3；
     * cJSON 把该字节原样写入 JSON，zrdbgClient 以 UTF-8 解码后显示 U+FFFD，
     * 原字符丢失。补 test_debug_truncation.c 的跨边界协议用例。 */
    for (index = 0; index < windowCount; index++) {
        TZrChar nameBuffer[ZR_DEBUG_NAME_CAPACITY];
        TZrChar chunkBuffer[ZR_DEBUG_STRING_CHUNK_SIZE + 1u];
        TZrSize chunkIndex = windowStart + index;
        TZrSize chunkStart = chunkIndex * ZR_DEBUG_STRING_CHUNK_SIZE;
        TZrSize chunkLength = byteLength > chunkStart ? (byteLength - chunkStart) : 0;
        TZrSize chunkEnd;

        if (chunkLength > ZR_DEBUG_STRING_CHUNK_SIZE) {
            chunkLength = ZR_DEBUG_STRING_CHUNK_SIZE;
        }
        chunkEnd = chunkStart + chunkLength;
        if (chunkLength > 0) {
            memcpy(chunkBuffer, nativeText + chunkStart, chunkLength);
        }
        chunkBuffer[chunkLength] = '\0';
        snprintf(nameBuffer,
                 sizeof(nameBuffer),
                 "[%llu..%llu)",
                 (unsigned long long)chunkStart,
                 (unsigned long long)chunkEnd);
        nameBuffer[sizeof(nameBuffer) - 1u] = '\0';
        zr_debug_fill_text_preview(&values[index], scopeKind, nameBuffer, "string", chunkBuffer, 0);
    }

    *outValues = values;
    *outCount = windowCount;
    return ZR_TRUE;
}

static TZrUInt32 zr_debug_loaded_module_count(ZrDebugAgent *agent) {
    if (agent == ZR_NULL || agent->state == ZR_NULL || agent->state->global == ZR_NULL ||
        agent->state->global->loadedModulesRegistry.value.object == ZR_NULL) {
        return 0;
    }

    return (TZrUInt32)ZrCore_Object_SuperArrayLength(
            ZR_CAST_OBJECT(agent->state, agent->state->global->loadedModulesRegistry.value.object));
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

static TZrBool zr_debug_expand_type_object_handle(ZrDebugAgent *agent,
                                                  SZrObjectPrototype *prototype,
                                                  ZrDebugValuePreview **outValues,
                                                  TZrSize *outCount,
                                                  TZrSize *outNamedVariables,
                                                  TZrSize *outIndexedVariables) {
    ZrDebugValuePreview *values;
    TZrSize count = ZR_DEBUG_TYPE_OBJECT_FIELD_COUNT;

    if (outValues != ZR_NULL) {
        *outValues = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (outNamedVariables != ZR_NULL) {
        *outNamedVariables = count;
    }
    if (outIndexedVariables != ZR_NULL) {
        *outIndexedVariables = 0;
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
                               "$metadata",
                               "metadata",
                               "metadata",
                               zr_debug_register_prototype_view_handle(agent,
                                                                       prototype,
                                                                       ZR_DEBUG_PROTOTYPE_VIEW_METADATA));
    zr_debug_fill_text_preview(&values[1],
                               ZR_DEBUG_SCOPE_KIND_PROTOTYPE,
                               "$members",
                               "descriptorList",
                               "members",
                               zr_debug_register_prototype_view_handle(agent,
                                                                       prototype,
                                                                       ZR_DEBUG_PROTOTYPE_VIEW_MEMBERS));
    zr_debug_fill_text_preview(&values[2],
                               ZR_DEBUG_SCOPE_KIND_PROTOTYPE,
                               "$methods",
                               "descriptorList",
                               "methods",
                               zr_debug_register_prototype_view_handle(agent,
                                                                       prototype,
                                                                       ZR_DEBUG_PROTOTYPE_VIEW_METHODS));
    zr_debug_fill_text_preview(&values[3],
                               ZR_DEBUG_SCOPE_KIND_PROTOTYPE,
                               "$properties",
                               "descriptorList",
                               "properties",
                               zr_debug_register_prototype_view_handle(agent,
                                                                       prototype,
                                                                       ZR_DEBUG_PROTOTYPE_VIEW_PROPERTIES));
    zr_debug_fill_text_preview(&values[4],
                               ZR_DEBUG_SCOPE_KIND_PROTOTYPE,
                               "$statics",
                               "descriptorList",
                               "statics",
                               zr_debug_register_prototype_view_handle(agent,
                                                                       prototype,
                                                                       ZR_DEBUG_PROTOTYPE_VIEW_STATICS));
    zr_debug_fill_text_preview(&values[5],
                               ZR_DEBUG_SCOPE_KIND_PROTOTYPE,
                               "$protocols",
                               "descriptorList",
                               "protocols",
                               zr_debug_register_prototype_view_handle(agent,
                                                                       prototype,
                                                                       ZR_DEBUG_PROTOTYPE_VIEW_PROTOCOLS));

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

    if (viewKind == ZR_DEBUG_PROTOTYPE_VIEW_METADATA) {
        return zr_debug_expand_prototype_summary(agent, prototype, outValues, outCount);
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

    /* 先按同一谓词计数、再填充；缺失的直接值保留描述符的静态说明。 */
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
            case ZR_DEBUG_PROTOTYPE_VIEW_METADATA:
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
            case ZR_DEBUG_PROTOTYPE_VIEW_METADATA:
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

static TZrBool zr_debug_expand_union_value_handle(ZrDebugAgent *agent,
                                                  const SZrTypeValue *value,
                                                  EZrDebugScopeKind scopeKind,
                                                  ZrDebugValuePreview **outValues,
                                                  TZrSize *outCount,
                                                  TZrSize *outNamedVariables,
                                                  TZrSize *outIndexedVariables) {
    ZrDebugUnionView view;
    ZrDebugValuePreview *values;
    TZrSize count;
    TZrSize index = 0;

    if (outValues != ZR_NULL) {
        *outValues = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (outNamedVariables != ZR_NULL) {
        *outNamedVariables = 0;
    }
    if (outIndexedVariables != ZR_NULL) {
        *outIndexedVariables = 0;
    }
    if (agent == ZR_NULL ||
        value == ZR_NULL ||
        outValues == ZR_NULL ||
        outCount == ZR_NULL ||
        !zr_debug_union_value_view(agent, value, &view)) {
        return ZR_FALSE;
    }

    count = 1u + view.payload_count;
    values = count > 0 ? (ZrDebugValuePreview *)calloc(count, sizeof(*values)) : ZR_NULL;
    if (count > 0 && values == ZR_NULL) {
        return ZR_FALSE;
    }

    {
        SZrTypeValue variantValue;
        ZrCore_Value_ResetAsNull(&variantValue);
        if (zr_debug_union_safe_get_member_value(agent, value, "variant", &variantValue)) {
            zr_debug_fill_value_preview(agent, "variant", scopeKind, &variantValue, &values[index]);
        } else {
            zr_debug_fill_text_preview(&values[index], scopeKind, "variant", "string", view.variant_name, 0);
        }
        index++;
    }

    for (TZrUInt32 payloadIndex = 0u; payloadIndex < view.payload_count; payloadIndex++) {
        TZrChar displayName[ZR_DEBUG_NAME_CAPACITY];
        SZrTypeValue payloadValue;

        ZrCore_Value_ResetAsNull(&payloadValue);
        if (!zr_debug_union_payload_display_name(agent->state,
                                                 &view,
                                                 payloadIndex,
                                                 displayName,
                                                 sizeof(displayName))) {
            snprintf(displayName, sizeof(displayName), "payload%u", (unsigned)payloadIndex);
            displayName[sizeof(displayName) - 1u] = '\0';
        }
        if (zr_debug_union_payload_value_from_carrier(agent, value, &view, payloadIndex, &payloadValue)) {
            zr_debug_fill_value_preview(agent, displayName, scopeKind, &payloadValue, &values[index]);
        } else {
            zr_debug_fill_text_preview(&values[index], scopeKind, displayName, "value", "<unavailable>", 0);
        }
        index++;
    }

    *outValues = values;
    *outCount = index;
    if (outNamedVariables != ZR_NULL) {
        *outNamedVariables = index;
    }
    if (outIndexedVariables != ZR_NULL) {
        *outIndexedVariables = 0;
    }
    return ZR_TRUE;
}

static TZrBool zr_debug_expand_inline_union_handle(ZrDebugAgent *agent,
                                                   TZrUInt32 frameId,
                                                   TZrUInt32 stackSlot,
                                                   EZrDebugScopeKind scopeKind,
                                                   ZrDebugValuePreview **outValues,
                                                   TZrSize *outCount,
                                                   TZrSize *outNamedVariables,
                                                   TZrSize *outIndexedVariables) {
    SZrFunction *function = ZR_NULL;
    SZrCallInfo *callInfo;
    SZrStackFramePlace place;
    ZrDebugUnionView view;
    ZrDebugValuePreview *values;
    TZrSize count;
    TZrSize index = 0;

    if (outValues != ZR_NULL) {
        *outValues = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (outNamedVariables != ZR_NULL) {
        *outNamedVariables = 0;
    }
    if (outIndexedVariables != ZR_NULL) {
        *outIndexedVariables = 0;
    }
    if (agent == ZR_NULL || outValues == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    callInfo = zr_debug_find_call_info_by_frame_id(agent, frameId, &function);
    if (!zr_debug_inline_union_slot_view(agent, function, callInfo, stackSlot, &place, &view)) {
        return ZR_FALSE;
    }

    count = 1u + view.payload_count;
    values = count > 0 ? (ZrDebugValuePreview *)calloc(count, sizeof(*values)) : ZR_NULL;
    if (count > 0 && values == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_debug_fill_text_preview(&values[index++], scopeKind, "variant", "string", view.variant_name, 0);

    for (TZrUInt32 payloadIndex = 0u; payloadIndex < view.payload_count; payloadIndex++) {
        TZrChar displayName[ZR_DEBUG_NAME_CAPACITY];
        SZrTypeValue payloadValue;

        ZrCore_Value_ResetAsNull(&payloadValue);
        if (!zr_debug_union_payload_display_name(agent->state,
                                                 &view,
                                                 payloadIndex,
                                                 displayName,
                                                 sizeof(displayName))) {
            snprintf(displayName, sizeof(displayName), "payload%u", (unsigned)payloadIndex);
            displayName[sizeof(displayName) - 1u] = '\0';
        }
        if (zr_debug_union_payload_value_from_inline(agent, function, &place, &view, payloadIndex, &payloadValue)) {
            zr_debug_fill_value_preview(agent, displayName, scopeKind, &payloadValue, &values[index]);
        } else {
            zr_debug_fill_text_preview(&values[index], scopeKind, displayName, "value", "<unavailable>", 0);
        }
        index++;
    }

    *outValues = values;
    *outCount = index;
    if (outNamedVariables != ZR_NULL) {
        *outNamedVariables = index;
    }
    if (outIndexedVariables != ZR_NULL) {
        *outIndexedVariables = 0;
    }
    return ZR_TRUE;
}

static TZrBool zr_debug_expand_value_handle(ZrDebugAgent *agent,
                                            const SZrTypeValue *value,
                                            TZrSize start,
                                            TZrSize countLimit,
                                            EZrDebugScopeKind scopeKind,
                                            ZrDebugValuePreview **outValues,
                                            TZrSize *outCount,
                                            TZrSize *outNamedVariables,
                                            TZrSize *outIndexedVariables) {
    SZrObject *object;
    SZrObject *indexedStorage = ZR_NULL;
    SZrObjectPrototype *prototype;
    const TZrChar *indexedSyntheticName = ZR_NULL;
    ZrDebugValuePreview *values;
    TZrSize count;
    TZrSize indexedCount = 0;
    TZrSize index = 0;
    TZrSize bucketIndex;

    if (outValues != ZR_NULL) {
        *outValues = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (outNamedVariables != ZR_NULL) {
        *outNamedVariables = 0;
    }
    if (outIndexedVariables != ZR_NULL) {
        *outIndexedVariables = 0;
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
    /* union、原生数组与普通对象的子节点结构不同，按优先级分流。 */
    if (zr_debug_value_is_union_carrier(agent->state, value)) {
        return zr_debug_expand_union_value_handle(agent,
                                                 value,
                                                 scopeKind,
                                                 outValues,
                                                 outCount,
                                                 outNamedVariables,
                                                 outIndexedVariables);
    }

    if (zr_debug_try_resolve_indexed_storage(agent->state,
                                             value,
                                             &indexedStorage,
                                             &indexedSyntheticName,
                                             &indexedCount) &&
        object->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return zr_debug_expand_indexed_window(agent,
                                              indexedStorage,
                                              0,
                                              indexedCount,
                                              start,
                                              countLimit,
                                              scopeKind,
                                              outValues,
                                              outCount,
                                              outNamedVariables,
                                              outIndexedVariables);
    }

    count = zr_debug_count_visible_object_entries(&object->nodeMap);
    if (indexedStorage != ZR_NULL && indexedSyntheticName != ZR_NULL) {
        count++;
    }
    count += prototype != ZR_NULL ? ZR_DEBUG_INSTANCE_SYNTHETIC_FIELD_COUNT : 0u;

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

    if (indexedStorage != ZR_NULL && indexedSyntheticName != ZR_NULL) {
        SZrTypeValue indexedValue;

        ZrCore_Value_InitAsRawObject(agent->state, &indexedValue, ZR_CAST_RAW_OBJECT_AS_SUPER(indexedStorage));
        indexedValue.type = ZR_VALUE_TYPE_ARRAY;
        zr_debug_fill_value_preview(agent, indexedSyntheticName, scopeKind, &indexedValue, &values[index]);
        index++;
    }

    if (prototype != ZR_NULL) {
        const TZrChar *prototypeName = prototype->name != ZR_NULL ? zr_debug_string_native(prototype->name) : "prototype";
        zr_debug_fill_text_preview(&values[index++],
                                   scopeKind,
                                   "$prototype",
                                   "prototype",
                                   prototypeName,
                                   zr_debug_register_prototype_handle(agent, prototype));
    }

    *outValues = values;
    *outCount = index;
    if (outNamedVariables != ZR_NULL) {
        *outNamedVariables = index;
    }
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
    /* 恢复执行、换停点和停止 agent 都调用这里；保留 nextVariableHandleId 防止旧 ID 复活。 */
    agent->variableHandleCount = 0;
    agent->variableHandleCapacity = 0;
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

static TZrBool zr_debug_local_is_active_at_pc(const SZrFunctionLocalVariable *local, TZrUInt32 pc) {
    return (TZrBool)(local != ZR_NULL &&
                     local->name != ZR_NULL &&
                     local->offsetActivate <= pc &&
                     pc < local->offsetDead);
}

static TZrBool zr_debug_local_scope_accepts_slot(EZrDebugScopeKind scopeKind,
                                                 const SZrFunction *function,
                                                 TZrUInt32 stackSlot,
                                                 TZrUInt32 receiverSlot) {
    TZrUInt32 startSlot = receiverSlot != ZR_DEBUG_INVALID_SLOT_INDEX ? 1u : 0u;
    TZrUInt32 endSlot;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    endSlot = scopeKind == ZR_DEBUG_SCOPE_KIND_ARGUMENTS ? function->parameterCount : function->stackSize;
    if (endSlot > function->stackSize) {
        endSlot = function->stackSize;
    }

    return (TZrBool)(stackSlot >= startSlot && stackSlot < endSlot);
}

static TZrBool zr_debug_core_local_scope_entry(ZrDebugAgent *agent,
                                               const SZrDebugActivation *activation,
                                               const SZrFunctionLocalVariable *local,
                                               TZrInt32 localIndex,
                                               EZrDebugScopeKind scopeKind,
                                               TZrUInt32 receiverSlot,
                                               TZrNativeString *outName,
                                               SZrTypeValue *outValue) {
    TZrNativeString name;

    if (outName != ZR_NULL) {
        *outName = ZR_NULL;
    }
    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (agent == ZR_NULL || activation == ZR_NULL || local == ZR_NULL ||
        !zr_debug_local_scope_accepts_slot(scopeKind, activation->function, local->stackSlot, receiverSlot)) {
        return ZR_FALSE;
    }

    /* Core 调试 API 负责解释活动局部变量序号和内联槽布局。 */
    name = ZrCore_Debug_GetLocal(agent->state, activation, localIndex, outValue);
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }

    if (outName != ZR_NULL) {
        *outName = name;
    }
    return ZR_TRUE;
}

const SZrTypeValue *zr_debug_closure_capture_value(ZrDebugAgent *agent,
                                                   const SZrFunction *function,
                                                   const SZrCallInfo *callInfo,
                                                   const TZrChar *name) {
    SZrTypeValue *closureValue;
    SZrClosure *closure;
    TZrUInt32 slotIndex;

    if (agent == ZR_NULL ||
        agent->state == ZR_NULL ||
        function == ZR_NULL ||
        callInfo == ZR_NULL ||
        name == ZR_NULL ||
        name[0] == '\0') {
        return ZR_NULL;
    }

    closureValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    closure = zr_debug_try_get_vm_closure_from_value(closureValue);
    if (closure == ZR_NULL) {
        return ZR_NULL;
    }

    /* 返回捕获槽的借用指针；调用方必须在当前停点内复制所需值。 */
    for (slotIndex = 0; slotIndex < function->closureValueLength; slotIndex++) {
        TZrNativeString upvalueName = ZrCore_Debug_GetUpvalue(agent->state, closure, (TZrInt32)slotIndex + 1, ZR_NULL);
        SZrClosureValue *upvalueId = (SZrClosureValue *)ZrCore_Debug_GetUpvalueId(agent->state,
                                                                                  closure,
                                                                                  (TZrInt32)slotIndex + 1);
        if (upvalueName != ZR_NULL && strcmp(upvalueName, name) == 0 && upvalueId != ZR_NULL) {
            return ZrCore_ClosureValue_GetValue(upvalueId);
        }
    }

    return ZR_NULL;
}

static TZrBool zr_debug_fill_inline_union_preview(ZrDebugAgent *agent,
                                                  const TZrChar *name,
                                                  EZrDebugScopeKind scopeKind,
                                                  TZrUInt32 frameId,
                                                  const SZrFunction *function,
                                                  const SZrCallInfo *callInfo,
                                                  TZrUInt32 stackSlot,
                                                  ZrDebugValuePreview *preview) {
    SZrStackFramePlace place;
    ZrDebugUnionView view;
    TZrChar valueText[ZR_DEBUG_TEXT_CAPACITY];
    TZrUInt32 variablesReference;
    TZrSize namedVariables;

    if (agent == ZR_NULL ||
        preview == ZR_NULL ||
        !zr_debug_inline_union_slot_view(agent, function, callInfo, stackSlot, &place, &view)) {
        return ZR_FALSE;
    }

    ZR_UNUSED_PARAMETER(place);
    zr_debug_format_inline_union_text(&view, valueText, sizeof(valueText));
    variablesReference = zr_debug_register_inline_union_handle(agent, frameId, stackSlot);
    namedVariables = 1u + view.payload_count;
    zr_debug_fill_text_preview(preview,
                               scopeKind,
                               name,
                               view.type_name[0] != '\0' ? view.type_name : "union",
                               valueText,
                               variablesReference);
    preview->named_variables = namedVariables;
    preview->indexed_variables = 0;
    snprintf(preview->semantic_summary,
             sizeof(preview->semantic_summary),
             "union %s.%s, named %llu",
             view.type_name[0] != '\0' ? view.type_name : "value",
             view.variant_name[0] != '\0' ? view.variant_name : "variant",
             (unsigned long long)namedVariables);
    preview->semantic_summary[sizeof(preview->semantic_summary) - 1u] = '\0';
    return ZR_TRUE;
}

TZrBool zr_debug_resolve_identifier_value(ZrDebugAgent *agent,
                                          TZrUInt32 frameId,
                                          const TZrChar *name,
                                          SZrTypeValue *outValue,
                                          TZrChar *errorBuffer,
                                          TZrSize errorBufferSize) {
    SZrFunction *function = ZR_NULL;
    SZrCallInfo *callInfo;
    SZrDebugActivation activation;
    TZrUInt32 pc;

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

    memset(&activation, 0, sizeof(activation));
    callInfo = ZrCore_Debug_GetStack(agent->state, frameId - 1u, &activation) ? activation.callInfo : ZR_NULL;
    function = activation.function;
    /* 兼容求值器先查捕获和当前活动局部变量，再查受限的调试全局名。 */
    if (callInfo != ZR_NULL && function != ZR_NULL) {
        const SZrTypeValue *closureSlot = zr_debug_closure_capture_value(agent, function, callInfo, name);
        if (closureSlot != ZR_NULL) {
            zr_debug_copy_value_snapshot(outValue, closureSlot);
            return ZR_TRUE;
        }

        pc = zr_debug_instruction_offset(callInfo, function);
        {
            TZrUInt32 localMetadataIndex;
            TZrInt32 activeLocalIndex = 0;
            for (localMetadataIndex = 0u;
                 localMetadataIndex < function->localVariableLength &&
                 function->localVariableList[localMetadataIndex].offsetActivate <= pc;
                 localMetadataIndex++) {
                const SZrFunctionLocalVariable *local = &function->localVariableList[localMetadataIndex];
                TZrNativeString localName;
                SZrTypeValue localValue;

                if (!zr_debug_local_is_active_at_pc(local, pc)) {
                    continue;
                }
                activeLocalIndex++;
                ZrCore_Value_ResetAsNull(&localValue);
                localName = ZrCore_Debug_GetLocal(agent->state, &activation, activeLocalIndex, &localValue);
                if (localName == ZR_NULL || strcmp(localName, name) != 0) {
                    continue;
                }

                if (zr_debug_materialize_inline_union_slot(agent, function, callInfo, local->stackSlot, outValue)) {
                    return ZR_TRUE;
                }
                zr_debug_copy_value_snapshot(outValue, &localValue);
                return ZR_TRUE;
            }
        }

    }

    if (strcmp(name, "zr") == 0) {
        zr_debug_copy_value_snapshot(outValue, &agent->state->global->zrObject);
        return ZR_TRUE;
    }
    if (strcmp(name, "loadedModules") == 0) {
        zr_debug_copy_value_snapshot(outValue, &agent->state->global->loadedModulesRegistry);
        return ZR_TRUE;
    }
    if (strcmp(name, "error") == 0 && agent->state->hasCurrentException) {
        zr_debug_copy_value_snapshot(outValue, &agent->state->currentException);
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

    if (zr_debug_union_safe_get_member_value(agent, receiver, memberName, outValue)) {
        return ZR_TRUE;
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
                zr_debug_copy_value_snapshot(outValue, resolvedValue);
                return ZR_TRUE;
            }
        }
    }

    /* 只读直接存储与描述符：跳过可能执行用户代码的属性 getter；
     * 静态成员仅接受原型对象作为 receiver。 */
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
            zr_debug_copy_value_snapshot(outValue, resolvedValue);
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
    SZrObject *indexedStorage = ZR_NULL;
    SZrObjectPrototype *prototype;
    const SZrTypeValue *resolvedValue;
    TZrSize indexValue;
    SZrTypeValue normalizedKey;

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

    if (zr_debug_try_resolve_indexed_storage(agent->state, receiver, &indexedStorage, ZR_NULL, ZR_NULL) &&
        indexedStorage != ZR_NULL) {
        if (!zr_debug_key_to_index(key, &indexValue)) {
            zr_debug_copy_text(errorBuffer, errorBufferSize, "index access expects a non-negative integer");
            return ZR_FALSE;
        }

        ZrCore_Value_InitAsInt(agent->state, &normalizedKey, (TZrInt64)indexValue);
        resolvedValue = ZrCore_Object_GetValue(agent->state, indexedStorage, &normalizedKey);
        if (resolvedValue != ZR_NULL) {
            zr_debug_copy_value_snapshot(outValue, resolvedValue);
        } else {
            ZrCore_Value_ResetAsNull(outValue);
        }
        return ZR_TRUE;
    }

    /* 自定义索引协议可能执行用户代码，安全求值只允许直接对象查表。 */
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
        zr_debug_copy_value_snapshot(outValue, resolvedValue);
    }

    return ZR_TRUE;
}

static TZrBool zr_debug_is_space_char(TZrChar ch) {
    return (TZrBool)(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n');
}

static TZrBool zr_debug_copy_trimmed_text(const TZrChar *start,
                                          const TZrChar *end,
                                          TZrChar *buffer,
                                          TZrSize bufferSize) {
    TZrSize length;

    if (buffer != ZR_NULL && bufferSize > 0) {
        buffer[0] = '\0';
    }
    if (start == ZR_NULL || end == ZR_NULL || buffer == ZR_NULL || bufferSize == 0 || end < start) {
        return ZR_FALSE;
    }

    while (start < end && zr_debug_is_space_char(*start)) {
        start++;
    }
    while (end > start && zr_debug_is_space_char(*(end - 1))) {
        end--;
    }

    length = (TZrSize)(end - start);
    if (length == 0 || length + 1 > bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer, start, length);
    buffer[length] = '\0';
    return ZR_TRUE;
}

static TZrBool zr_debug_split_index_window_expression(const TZrChar *expression,
                                                      TZrChar *baseBuffer,
                                                      TZrSize baseBufferSize,
                                                      TZrChar *startBuffer,
                                                      TZrSize startBufferSize,
                                                      TZrChar *endBuffer,
                                                      TZrSize endBufferSize) {
    const TZrChar *trimmedEnd;
    const TZrChar *cursor;
    const TZrChar *openBracket = ZR_NULL;
    const TZrChar *rangeDots = ZR_NULL;
    TZrUInt32 bracketDepth = 0;
    TZrUInt32 parenDepth = 0;

    if (baseBuffer != ZR_NULL && baseBufferSize > 0) {
        baseBuffer[0] = '\0';
    }
    if (startBuffer != ZR_NULL && startBufferSize > 0) {
        startBuffer[0] = '\0';
    }
    if (endBuffer != ZR_NULL && endBufferSize > 0) {
        endBuffer[0] = '\0';
    }
    if (expression == ZR_NULL) {
        return ZR_FALSE;
    }

    trimmedEnd = expression + strlen(expression);
    while (trimmedEnd > expression && zr_debug_is_space_char(*(trimmedEnd - 1))) {
        trimmedEnd--;
    }
    if (trimmedEnd <= expression || *(trimmedEnd - 1) != ']') {
        return ZR_FALSE;
    }

    /* 仅识别末尾的顶层 [start..end]；嵌套括号内的省略号不作窗口分隔符。 */
    bracketDepth = 1;
    cursor = trimmedEnd - 1;
    while (cursor > expression) {
        cursor--;
        if (*cursor == ']') {
            bracketDepth++;
        } else if (*cursor == '[') {
            bracketDepth--;
            if (bracketDepth == 0) {
                openBracket = cursor;
                break;
            }
        }
    }
    if (openBracket == ZR_NULL) {
        return ZR_FALSE;
    }

    for (cursor = openBracket + 1; cursor + 1 < trimmedEnd - 1; cursor++) {
        if (*cursor == '[') {
            bracketDepth++;
            continue;
        }
        if (*cursor == ']') {
            if (bracketDepth > 0) {
                bracketDepth--;
            }
            continue;
        }
        if (*cursor == '(') {
            parenDepth++;
            continue;
        }
        if (*cursor == ')') {
            if (parenDepth > 0) {
                parenDepth--;
            }
            continue;
        }
        if (bracketDepth == 0 && parenDepth == 0 && cursor[0] == '.' && cursor[1] == '.') {
            rangeDots = cursor;
            break;
        }
    }

    if (rangeDots == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_debug_copy_trimmed_text(expression, openBracket, baseBuffer, baseBufferSize) &&
           zr_debug_copy_trimmed_text(openBracket + 1, rangeDots, startBuffer, startBufferSize) &&
           zr_debug_copy_trimmed_text(rangeDots + 2, trimmedEnd - 1, endBuffer, endBufferSize);
}

static TZrBool zr_debug_value_to_non_negative_size(const SZrTypeValue *value, TZrSize *outSize) {
    TZrUInt64 sizeValue;

    if (outSize != ZR_NULL) {
        *outSize = 0;
    }
    if (value == ZR_NULL || outSize == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        if (value->value.nativeObject.nativeInt64 < 0) {
            return ZR_FALSE;
        }
        sizeValue = (TZrUInt64)value->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        sizeValue = value->value.nativeObject.nativeUInt64;
    } else {
        return ZR_FALSE;
    }

    *outSize = (TZrSize)sizeValue;
    return ZR_TRUE;
}

static TZrBool zr_debug_try_evaluate_index_window(ZrDebugAgent *agent,
                                                  TZrUInt32 frameId,
                                                  const TZrChar *expression,
                                                  ZrDebugEvaluateResult *outResult,
                                                  TZrChar *errorBuffer,
                                                  TZrSize errorBufferSize) {
    TZrChar baseExpression[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar startExpression[ZR_DEBUG_NAME_CAPACITY];
    TZrChar endExpression[ZR_DEBUG_NAME_CAPACITY];
    TZrChar baseReference[ZR_DEBUG_TEXT_CAPACITY];
    SZrTypeValue baseValue;
    SZrTypeValue startValue;
    SZrTypeValue endValue;
    SZrObject *indexedStorage = ZR_NULL;
    TZrSize indexedLength = 0;
    TZrSize startIndex = 0;
    TZrSize endIndex = 0;
    TZrUInt32 handleId;

    if (agent == ZR_NULL || outResult == ZR_NULL || expression == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!zr_debug_split_index_window_expression(expression,
                                                baseExpression,
                                                sizeof(baseExpression),
                                                startExpression,
                                                sizeof(startExpression),
                                                endExpression,
                                                sizeof(endExpression))) {
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(&baseValue);
    ZrCore_Value_ResetAsNull(&startValue);
    ZrCore_Value_ResetAsNull(&endValue);
    baseReference[0] = '\0';
    if (!zr_debug_evaluate_expression(agent,
                                      frameId,
                                      baseExpression,
                                      &baseValue,
                                      errorBuffer,
                                      errorBufferSize,
                                      baseReference,
                                      sizeof(baseReference)) ||
        !zr_debug_evaluate_expression(agent, frameId, startExpression, &startValue, errorBuffer, errorBufferSize, ZR_NULL, 0) ||
        !zr_debug_evaluate_expression(agent, frameId, endExpression, &endValue, errorBuffer, errorBufferSize, ZR_NULL, 0)) {
        return ZR_TRUE;
    }

    if (!zr_debug_try_resolve_indexed_storage(agent->state, &baseValue, &indexedStorage, ZR_NULL, &indexedLength) ||
        indexedStorage == ZR_NULL) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "range access is only supported for arrays and debug-indexed containers");
        return ZR_TRUE;
    }
    if (!zr_debug_value_to_non_negative_size(&startValue, &startIndex) ||
        !zr_debug_value_to_non_negative_size(&endValue, &endIndex)) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "range bounds must be non-negative integers");
        return ZR_TRUE;
    }
    if (endIndex < startIndex) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "range end must be greater than or equal to range start");
        return ZR_TRUE;
    }
    if (startIndex > indexedLength) {
        startIndex = indexedLength;
    }
    if (endIndex > indexedLength) {
        endIndex = indexedLength;
    }

    /* 窗口只保存基值与范围，后续 variables 请求重新解析当前索引存储。 */
    handleId = zr_debug_register_index_window_handle(agent, &baseValue, startIndex, endIndex - startIndex);
    if (handleId == 0) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "failed to allocate debug range window");
        return ZR_TRUE;
    }

    zr_debug_copy_text(outResult->type_name,
                       sizeof(outResult->type_name),
                       zr_debug_value_type_name_safe(agent->state, &baseValue));
    snprintf(outResult->value_text,
             sizeof(outResult->value_text),
             "[%llu..%llu)",
             (unsigned long long)startIndex,
             (unsigned long long)endIndex);
    outResult->value_text[sizeof(outResult->value_text) - 1u] = '\0';
    outResult->variables_reference = handleId;
    outResult->named_variables = 0;
    outResult->indexed_variables = endIndex - startIndex;
    if (baseReference[0] != '\0') {
        zr_debug_copy_text(outResult->reference_summary,
                           sizeof(outResult->reference_summary),
                           baseReference);
    }
    snprintf(outResult->semantic_summary,
             sizeof(outResult->semantic_summary),
             "indexed window, start %llu, indexed %llu",
             (unsigned long long)startIndex,
             (unsigned long long)(endIndex - startIndex));
    outResult->semantic_summary[sizeof(outResult->semantic_summary) - 1u] = '\0';
    return ZR_TRUE;
}

static TZrBool zr_debug_evaluate_with_policy(ZrDebugAgent *agent,
                                             TZrUInt32 frameId,
                                             const TZrChar *expression,
                                             TZrUInt32 allowedEffectFlags,
                                             TZrBool allowLegacyCompatibility,
                                             ZrDebugEvaluateResult *outResult,
                                             ZrDebugEvaluateFailure *outFailure,
                                             TZrChar *errorBuffer,
                                             TZrSize errorBufferSize) {
    SZrTypeValue value;

    if (outResult != ZR_NULL) {
        memset(outResult, 0, sizeof(*outResult));
    }
    zr_debug_evaluate_failure_clear(outFailure);
    if (errorBuffer != ZR_NULL && errorBufferSize > 0) {
        errorBuffer[0] = '\0';
    }
    if (agent == ZR_NULL || outResult == ZR_NULL || expression == ZR_NULL) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "invalid evaluate request");
        zr_debug_evaluate_failure_set(outFailure,
                                      ZR_DEBUG_EVALUATE_FAILURE_REQUEST,
                                      agent != ZR_NULL ? agent->stopStateId : 0u,
                                      "debug_evaluation_invalid_request",
                                      "invalid evaluate request");
        return ZR_FALSE;
    }
    /* 表达式结果与诊断都绑定停点代号，运行态禁止读取 VM 帧。 */
    if (agent->runMode != ZR_DEBUG_RUN_MODE_PAUSED) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "evaluate is only available while paused");
        zr_debug_evaluate_failure_set(outFailure,
                                      ZR_DEBUG_EVALUATE_FAILURE_REQUEST,
                                      agent->stopStateId,
                                      "debug_evaluation_not_paused",
                                      "evaluate is only available while paused");
        return ZR_FALSE;
    }
    outResult->state_id = agent->stopStateId;
    if (outFailure != ZR_NULL) {
        outFailure->state_id = agent->stopStateId;
    }
    /* 旧求值入口仍支持调试窗口语法；能力化入口交由正式求值器处理。 */
    if (allowLegacyCompatibility &&
        zr_debug_try_evaluate_index_window(agent,
                                           frameId == 0 ? 1u : frameId,
                                           expression,
                                           outResult,
                                           errorBuffer,
                                           errorBufferSize)) {
        return errorBuffer == ZR_NULL || errorBuffer[0] == '\0';
    }

    ZrCore_Value_ResetAsNull(&value);
    if (!zr_debug_evaluate_expression_with_capabilities(
                agent,
                frameId == 0 ? 1u : frameId,
                expression,
                allowedEffectFlags,
                allowLegacyCompatibility,
                &value,
                &outResult->canonical_type_id,
                outFailure,
                errorBuffer,
                errorBufferSize,
                outResult->reference_summary,
                sizeof(outResult->reference_summary))) {
        return ZR_FALSE;
    }
    outResult->has_canonical_type =
            outResult->canonical_type_id != ZR_DEBUG_CANONICAL_TYPE_ID_INVALID ? ZR_TRUE : ZR_FALSE;

    if (zr_debug_value_is_union_carrier(agent->state, &value)) {
        ZrDebugUnionView view;
        if (zr_debug_union_value_view(agent, &value, &view)) {
            zr_debug_copy_text(outResult->type_name, sizeof(outResult->type_name), view.type_name);
        } else {
            zr_debug_copy_text(outResult->type_name, sizeof(outResult->type_name), "union");
        }
    } else {
        zr_debug_copy_text(outResult->type_name,
                           sizeof(outResult->type_name),
                           zr_debug_value_type_name_safe(agent->state, &value));
    }
    if (!zr_debug_format_union_value_text(agent, &value, outResult->value_text, sizeof(outResult->value_text))) {
        zr_debug_format_value_text_safe(agent->state, &value, outResult->value_text, sizeof(outResult->value_text));
    }
    outResult->variables_reference = zr_debug_value_is_expandable(agent->state, &value)
                                             ? zr_debug_register_value_handle(agent, &value)
                                             : 0;
    zr_debug_value_child_shape(agent->state,
                               &value,
                               &outResult->named_variables,
                               &outResult->indexed_variables);
    zr_debug_attach_string_chunk_handle(agent,
                                        &value,
                                        &outResult->variables_reference,
                                        &outResult->named_variables,
                                        &outResult->indexed_variables);
    zr_debug_value_semantic_summary(agent->state,
                                    &value,
                                    outResult->named_variables,
                                    outResult->indexed_variables,
                                    outResult->semantic_summary,
                                    sizeof(outResult->semantic_summary));
    zr_debug_append_expression_semantic_facts(agent,
                                              frameId == 0 ? 1u : frameId,
                                              expression,
                                              outResult->semantic_summary,
                                              sizeof(outResult->semantic_summary));
    zr_debug_append_expression_reference_summary(agent,
                                                 frameId == 0 ? 1u : frameId,
                                                 expression,
                                                 outResult->reference_summary,
                                                 sizeof(outResult->reference_summary));
    return ZR_TRUE;
}

TZrBool ZrDebug_Evaluate(ZrDebugAgent *agent,
                         TZrUInt32 frameId,
                         const TZrChar *expression,
                         ZrDebugEvaluateResult *outResult,
                         TZrChar *errorBuffer,
                         TZrSize errorBufferSize) {
    return zr_debug_evaluate_with_policy(agent,
                                         frameId,
                                         expression,
                                         ZR_DEBUG_EVALUATION_EFFECT_NONE,
                                         ZR_TRUE,
                                         outResult,
                                         ZR_NULL,
                                         errorBuffer,
                                         errorBufferSize);
}

TZrBool ZrDebug_EvaluateWithCapabilities(ZrDebugAgent *agent,
                                         TZrUInt32 frameId,
                                         const TZrChar *expression,
                                         TZrUInt32 allowedEffectFlags,
                                         ZrDebugEvaluateResult *outResult,
                                         TZrChar *errorBuffer,
                                         TZrSize errorBufferSize) {
    return ZrDebug_EvaluateWithCapabilitiesDetailed(agent,
                                                     frameId,
                                                     expression,
                                                     allowedEffectFlags,
                                                     outResult,
                                                     ZR_NULL,
                                                     errorBuffer,
                                                     errorBufferSize);
}

TZrBool ZrDebug_EvaluateWithCapabilitiesDetailed(ZrDebugAgent *agent,
                                                 TZrUInt32 frameId,
                                                 const TZrChar *expression,
                                                 TZrUInt32 allowedEffectFlags,
                                                 ZrDebugEvaluateResult *outResult,
                                                 ZrDebugEvaluateFailure *outFailure,
                                                 TZrChar *errorBuffer,
                                                 TZrSize errorBufferSize) {
    return zr_debug_evaluate_with_policy(agent,
                                         frameId,
                                         expression,
                                         allowedEffectFlags,
                                         ZR_FALSE,
                                         outResult,
                                         outFailure,
                                         errorBuffer,
                                         errorBufferSize);
}

static const SZrParserTestEntry *zr_debug_find_test_entry(
        const ZrDebugAgent *agent,
        const SZrFunction *function) {
    if (agent == ZR_NULL || function == ZR_NULL || !agent->hasTestManifest ||
        agent->entryFunction == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrUInt32 index = 0U; index < agent->testManifest.entryCount; index++) {
        const SZrParserTestEntry *entry = &agent->testManifest.entries[index];
        if (entry->callableChildIndex < agent->entryFunction->childFunctionLength &&
            &agent->entryFunction->childFunctionList[entry->callableChildIndex] == function) {
            return entry;
        }
    }
    return ZR_NULL;
}

TZrBool ZrDebug_ReadStack(ZrDebugAgent *agent, ZrDebugFrameSnapshot **outFrames, TZrSize *outCount) {
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

    /* 异常对象保存的是抛出时的栈；异常停点优先返回这份快照。 */
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

    {
        SZrDebugActivation activation;
        while (ZrCore_Debug_GetStack(agent->state, frameCount, &activation)) {
            if (topFunction == ZR_NULL) {
                topFunction = activation.function;
            }
            frameCount++;
        }
    }

    /* 只有入口帧已不在 VM 栈、且当前仅剩单帧时补一条可显示的入口记录。 */
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

    for (frameId = 1; frameId <= frameCount; frameId++) {
        SZrDebugActivation activation;
        SZrDebugInfo debugInfo;
        SZrCallInfo *callInfo;
        SZrFunction *function;
        TZrUInt32 instructionIndex;
        TZrUInt32 receiverSlot;
        TZrChar receiverName[ZR_DEBUG_NAME_CAPACITY];
        TZrUInt32 argumentBase = 0;
        if (!ZrCore_Debug_GetStack(agent->state, frameId - 1u, &activation)) {
            continue;
        }
        memset(&debugInfo, 0, sizeof(debugInfo));
        (void)ZrCore_Debug_GetInfo(agent->state,
                                   &activation,
                                   (EZrDebugInfoType)(ZR_DEBUG_INFO_FUNCTION_NAME |
                                                      ZR_DEBUG_INFO_SOURCE_FILE |
                                                      ZR_DEBUG_INFO_LINE_NUMBER |
                                                      ZR_DEBUG_INFO_CLOSURE),
                                   &debugInfo);

        callInfo = activation.callInfo;
        function = activation.function;
        if (callInfo == ZR_NULL || function == ZR_NULL) {
            continue;
        }

        receiverName[0] = '\0';
        instructionIndex = zr_debug_instruction_offset(callInfo, function);
        receiverSlot = zr_debug_find_receiver_slot(function, instructionIndex, receiverName, sizeof(receiverName));
        if (receiverSlot != ZR_DEBUG_INVALID_SLOT_INDEX) {
            const SZrTypeValue *receiverValue = zr_debug_frame_value_slot(agent->state, function, callInfo, receiverSlot);
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

        frames[frameId - 1].thread_id = zr_debug_agent_effective_thread_id(agent, 0);
        frames[frameId - 1].frame_id = frameId;
        frames[frameId - 1].frame_depth = frameId - 1u;
        frames[frameId - 1].instruction_index = instructionIndex;
        frames[frameId - 1].line = debugInfo.currentLine != 0
                                           ? (TZrUInt32)debugInfo.currentLine
                                           : zr_debug_call_info_line(agent, callInfo, function, frameId == 1u);
        frames[frameId - 1].argument_count = function->parameterCount > argumentBase
                                                     ? (function->parameterCount - argumentBase)
                                                     : 0;
        frames[frameId - 1].return_slot =
                callInfo->hasReturnDestination && callInfo->returnDestination != ZR_NULL
                        ? (TZrInt32)ZrCore_Stack_SavePointerAsOffset(agent->state, callInfo->returnDestination)
                        : -1;
        frames[frameId - 1].is_exception_frame = ZR_FALSE;
        /* 异步调度与测试元数据是帧的附加投影，缺失时保留普通帧。 */
        {
            SZrDebugAsyncSchedulerContract asyncContract;
            if (ZrCore_Debug_ProjectSchedulerSourceContract(function, 0u, &asyncContract)) {
                frames[frameId - 1].has_async_contract = ZR_TRUE;
                frames[frameId - 1].async_contract_origin = (TZrUInt32)asyncContract.origin;
                frames[frameId - 1].async_scheduler_type_token = asyncContract.schedulerTypeToken;
                frames[frameId - 1].async_task_type_token = asyncContract.taskTypeToken;
                frames[frameId - 1].async_job_type_token = asyncContract.jobTypeToken;
                frames[frameId - 1].async_schedule_member_token = asyncContract.scheduleMemberToken;
                frames[frameId - 1].async_schedule_signature_token = asyncContract.scheduleSignatureToken;
                frames[frameId - 1].async_schedule_signature_hash = asyncContract.scheduleSignatureHash;
                frames[frameId - 1].async_scheduler_abi_version = asyncContract.schedulerAbiVersion;
                frames[frameId - 1].async_scheduler_policy_mask = asyncContract.schedulerPolicyMask;
                frames[frameId - 1].async_attached_requirement_flags = asyncContract.attachedRequirementFlags;
                frames[frameId - 1].async_isolated_requirement_flags = asyncContract.isolatedRequirementFlags;
                frames[frameId - 1].async_transport_contract_hash = asyncContract.transportContractHash;
                frames[frameId - 1].async_scheduler_contract_hash = asyncContract.schedulerContractHash;
            }
        }
        {
            const SZrParserTestEntry *testEntry =
                    zr_debug_find_test_entry(agent, function);
            if (testEntry != ZR_NULL) {
                frames[frameId - 1].has_test_contract = ZR_TRUE;
                frames[frameId - 1].test_is_async = testEntry->isAsync;
                frames[frameId - 1].test_function_symbol_id = testEntry->functionSymbolId;
                frames[frameId - 1].test_function_type_id = testEntry->functionTypeId;
                frames[frameId - 1].test_case_count = testEntry->caseCount;
                zr_debug_copy_text(frames[frameId - 1].test_qualified_name,
                                   sizeof(frames[frameId - 1].test_qualified_name),
                                   testEntry->qualifiedName != ZR_NULL
                                           ? testEntry->qualifiedName
                                           : "");
            }
        }
        zr_debug_copy_text(frames[frameId - 1].module_name, sizeof(frames[frameId - 1].module_name), agent->moduleName);
        zr_debug_copy_text(frames[frameId - 1].function_name,
                           sizeof(frames[frameId - 1].function_name),
                           debugInfo.name != ZR_NULL ? debugInfo.name : zr_debug_function_name(function));
        zr_debug_copy_text(frames[frameId - 1].source_file,
                           sizeof(frames[frameId - 1].source_file),
                           debugInfo.source != ZR_NULL ? debugInfo.source : zr_debug_function_source(function));
    }

    if (needsSyntheticEntryFrame) {
        ZrDebugFrameSnapshot *entryFrame = &frames[frameId - 1];
        entryFrame->thread_id = zr_debug_agent_effective_thread_id(agent, 0);
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

    /* BUG: 异常停点有 stacks 时，此分支未核对 frameId 上界；协议 scopes 请求
     * 传入大于异常帧数的 ID 仍得到 Exception scope。证据：debug_protocol.c 的
     * zr_debug_agent_process_scopes 直传请求 ID；此分支仅判断帧数非零。 */
    if (agent->lastStopEvent.reason == ZR_DEBUG_STOP_REASON_EXCEPTION &&
        zr_debug_exception_frame_count(agent) > 0) {
        count = 1;
        scopes = (ZrDebugScopeSnapshot *)calloc(count, sizeof(*scopes));
        if (scopes == ZR_NULL) {
            return ZR_FALSE;
        }
        scopes[0].thread_id = zr_debug_agent_effective_thread_id(agent, 0);
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

    for (TZrSize scopeIndex = 0; scopeIndex < index; scopeIndex++) {
        scopes[scopeIndex].thread_id = zr_debug_agent_effective_thread_id(agent, 0);
    }

    *outScopes = scopes;
    *outCount = index;
    return ZR_TRUE;
}

TZrBool ZrDebug_ReadVariables(ZrDebugAgent *agent,
                              TZrUInt32 scopeId,
                              TZrSize start,
                              TZrSize countLimit,
                              ZrDebugValuePreview **outValues,
                              TZrSize *outCount,
                              TZrSize *outNamedVariables,
                              TZrSize *outIndexedVariables) {
    ZrDebugVariableHandle *handle;
    TZrUInt32 frameId;
    EZrDebugScopeKind scopeKind;
    SZrFunction *function = ZR_NULL;
    SZrCallInfo *callInfo;
    SZrDebugActivation activation;

    if (outValues != ZR_NULL) {
        *outValues = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (outNamedVariables != ZR_NULL) {
        *outNamedVariables = 0;
    }
    if (outIndexedVariables != ZR_NULL) {
        *outIndexedVariables = 0;
    }
    if (agent == ZR_NULL || outValues == ZR_NULL || outCount == ZR_NULL || scopeId == 0) {
        return ZR_FALSE;
    }

    /* BUG: ReadScopes 对第 100 帧及更深帧生成 frameId*10+kind >= 1000，
     * 与动态句柄区间重叠；此处无对应 handle 即拒绝，合法深帧变量无法展开。
     * 证据：debug.c:zr_debug_scope_id、core/debug.c:ZrCore_Debug_GetStack。 */
    /* 高位区间只接收本停点句柄，不能退回 frameId*10+scopeKind 的算术解码。 */
    handle = zr_debug_find_variable_handle(agent, scopeId);
    if (scopeId >= ZR_DEBUG_VARIABLE_HANDLE_BASE && handle == ZR_NULL) {
        return ZR_FALSE;
    }
    if (handle != ZR_NULL) {
        TZrBool ok = ZR_FALSE;
        switch (handle->kind) {
            case ZR_DEBUG_VARIABLE_HANDLE_KIND_VALUE:
                return zr_debug_expand_value_handle(agent,
                                                    &handle->value,
                                                    start,
                                                    countLimit,
                                                    ZR_DEBUG_SCOPE_KIND_LOCALS,
                                                    outValues,
                                                    outCount,
                                                    outNamedVariables,
                                                    outIndexedVariables);
            case ZR_DEBUG_VARIABLE_HANDLE_KIND_GLOBAL_STATE:
                ok = zr_debug_expand_global_state(agent, outValues, outCount);
                if (ok && outNamedVariables != ZR_NULL) {
                    *outNamedVariables = *outCount;
                }
                return ok;
            case ZR_DEBUG_VARIABLE_HANDLE_KIND_PROTOTYPE:
                return zr_debug_expand_type_object_handle(agent,
                                                          handle->prototype,
                                                          outValues,
                                                          outCount,
                                                          outNamedVariables,
                                                          outIndexedVariables);
            case ZR_DEBUG_VARIABLE_HANDLE_KIND_PROTOTYPE_VIEW:
                ok = zr_debug_expand_prototype_view(agent,
                                                    handle->prototype,
                                                    handle->prototype_view_kind,
                                                    ZR_DEBUG_SCOPE_KIND_PROTOTYPE,
                                                    outValues,
                                                    outCount);
                if (ok && outNamedVariables != ZR_NULL) {
                    *outNamedVariables = *outCount;
                }
                return ok;
            case ZR_DEBUG_VARIABLE_HANDLE_KIND_EXCEPTION:
                ok = zr_debug_expand_exception_value(agent, outValues, outCount);
                if (ok && outNamedVariables != ZR_NULL) {
                    *outNamedVariables = *outCount;
                }
                return ok;
            case ZR_DEBUG_VARIABLE_HANDLE_KIND_INDEX_WINDOW: {
                SZrObject *indexedStorage = ZR_NULL;
                TZrSize indexedCount = 0;
                if (!zr_debug_try_resolve_indexed_storage(agent->state,
                                                          &handle->value,
                                                          &indexedStorage,
                                                          ZR_NULL,
                                                          &indexedCount) ||
                    indexedStorage == ZR_NULL) {
                    return ZR_FALSE;
                }
                if (handle->window_start > indexedCount) {
                    indexedCount = 0;
                } else if (handle->window_start + handle->window_count < indexedCount) {
                    indexedCount = handle->window_count;
                } else {
                    indexedCount -= handle->window_start;
                }
                return zr_debug_expand_indexed_window(agent,
                                                      indexedStorage,
                                                      handle->window_start,
                                                      indexedCount,
                                                      start,
                                                      countLimit,
                                                      ZR_DEBUG_SCOPE_KIND_LOCALS,
                                                      outValues,
                                                      outCount,
                                                      outNamedVariables,
                                                      outIndexedVariables);
            }
            case ZR_DEBUG_VARIABLE_HANDLE_KIND_INLINE_UNION:
                return zr_debug_expand_inline_union_handle(agent,
                                                           handle->frame_id,
                                                           handle->stack_slot,
                                                           ZR_DEBUG_SCOPE_KIND_LOCALS,
                                                           outValues,
                                                           outCount,
                                                           outNamedVariables,
                                                           outIndexedVariables);
            case ZR_DEBUG_VARIABLE_HANDLE_KIND_STRING_CHUNKS:
                return zr_debug_expand_string_chunks_handle(agent,
                                                            &handle->value,
                                                            start,
                                                            countLimit,
                                                            ZR_DEBUG_SCOPE_KIND_LOCALS,
                                                            outValues,
                                                            outCount,
                                                            outNamedVariables,
                                                            outIndexedVariables);
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

    /* 与异常栈的 scope 投影同源；当前只按 kind 展开，没有核对 frameId 对应的异常帧。 */
    if (scopeKind == ZR_DEBUG_SCOPE_KIND_EXCEPTION) {
        TZrBool ok = zr_debug_expand_exception_value(agent, outValues, outCount);
        if (ok && outNamedVariables != ZR_NULL) {
            *outNamedVariables = *outCount;
        }
        return ok;
    }

    memset(&activation, 0, sizeof(activation));
    if (!ZrCore_Debug_GetStack(agent->state, frameId - 1u, &activation)) {
        return ZR_FALSE;
    }
    callInfo = activation.callInfo;
    function = activation.function;
    if (callInfo == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    if (scopeKind == ZR_DEBUG_SCOPE_KIND_ARGUMENTS || scopeKind == ZR_DEBUG_SCOPE_KIND_LOCALS) {
        ZrDebugValuePreview *values = ZR_NULL;
        TZrSize valueCount = 0;
        TZrSize index = 0;
        TZrUInt32 pc = zr_debug_instruction_offset(callInfo, function);
        TZrUInt32 receiverSlot = zr_debug_find_receiver_slot(function, pc, ZR_NULL, 0);
        TZrUInt32 localMetadataIndex;
        TZrInt32 activeLocalIndex = 0;

        /* 先计数后分配：两遍都用活动局部变量序号交给 Core 获取，避免直接解释帧槽。 */
        for (localMetadataIndex = 0u;
             localMetadataIndex < function->localVariableLength &&
             function->localVariableList[localMetadataIndex].offsetActivate <= pc;
             localMetadataIndex++) {
            const SZrFunctionLocalVariable *local = &function->localVariableList[localMetadataIndex];
            if (!zr_debug_local_is_active_at_pc(local, pc)) {
                continue;
            }
            activeLocalIndex++;
            if (zr_debug_local_scope_accepts_slot(scopeKind, function, local->stackSlot, receiverSlot) &&
                ZrCore_Debug_GetLocal(agent->state, &activation, activeLocalIndex, ZR_NULL) != ZR_NULL) {
                valueCount++;
            }
        }

        values = valueCount > 0 ? (ZrDebugValuePreview *)calloc(valueCount, sizeof(*values)) : ZR_NULL;
        if (valueCount > 0 && values == ZR_NULL) {
            return ZR_FALSE;
        }

        activeLocalIndex = 0;
        for (localMetadataIndex = 0u;
             localMetadataIndex < function->localVariableLength &&
             function->localVariableList[localMetadataIndex].offsetActivate <= pc;
             localMetadataIndex++) {
            const SZrFunctionLocalVariable *local = &function->localVariableList[localMetadataIndex];
            TZrNativeString name;
            SZrTypeValue localValue;

            if (!zr_debug_local_is_active_at_pc(local, pc)) {
                continue;
            }
            activeLocalIndex++;
            ZrCore_Value_ResetAsNull(&localValue);
            if (!zr_debug_core_local_scope_entry(agent,
                                                &activation,
                                                local,
                                                activeLocalIndex,
                                                scopeKind,
                                                receiverSlot,
                                                &name,
                                                &localValue)) {
                continue;
            }
            if (!zr_debug_fill_inline_union_preview(agent,
                                                    name,
                                                    scopeKind,
                                                    frameId,
                                                    function,
                                                    callInfo,
                                                    local->stackSlot,
                                                    &values[index])) {
                zr_debug_fill_value_preview(agent,
                                            name,
                                            scopeKind,
                                            &localValue,
                                            &values[index]);
            }
            zr_debug_reference_summary_from_scope(scopeKind,
                                                  name,
                                                  values[index].reference_summary,
                                                  sizeof(values[index].reference_summary));
            index++;
        }

        *outValues = values;
        *outCount = valueCount;
        if (outNamedVariables != ZR_NULL) {
            *outNamedVariables = valueCount;
        }
        return ZR_TRUE;
    }

    if (scopeKind == ZR_DEBUG_SCOPE_KIND_CLOSURES) {
        ZrDebugValuePreview *values = ZR_NULL;
        SZrTypeValue *closureValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
        SZrClosure *closure = zr_debug_try_get_vm_closure_from_value(closureValue);
        TZrUInt32 closureIndex;

        values = function->closureValueLength > 0 ? (ZrDebugValuePreview *)calloc(function->closureValueLength, sizeof(*values))
                                                  : ZR_NULL;
        if (function->closureValueLength > 0 && values == ZR_NULL) {
            return ZR_FALSE;
        }

        for (closureIndex = 0; closureIndex < function->closureValueLength; closureIndex++) {
            SZrTypeValue previewValue;
            TZrNativeString upvalueName = closure != ZR_NULL
                                                  ? ZrCore_Debug_GetUpvalue(agent->state,
                                                                            closure,
                                                                            (TZrInt32)closureIndex + 1,
                                                                            &previewValue)
                                                  : ZR_NULL;
            if (upvalueName == ZR_NULL) {
                ZrCore_Value_ResetAsNull(&previewValue);
                previewValue = agent->state->global->nullValue;
            }
            const TZrChar *previewName = upvalueName != ZR_NULL ? upvalueName : "<closure>";
            zr_debug_fill_value_preview(agent,
                                        previewName,
                                        scopeKind,
                                        &previewValue,
                                        &values[closureIndex]);
            zr_debug_reference_summary_from_scope(scopeKind,
                                                  previewName,
                                                  values[closureIndex].reference_summary,
                                                  sizeof(values[closureIndex].reference_summary));
        }

        *outValues = values;
        *outCount = function->closureValueLength;
        if (outNamedVariables != ZR_NULL) {
            *outNamedVariables = function->closureValueLength;
        }
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
        zr_debug_reference_summary_from_scope(scopeKind,
                                              values[index - 1u].name,
                                              values[index - 1u].reference_summary,
                                              sizeof(values[index - 1u].reference_summary));
        zr_debug_fill_value_preview(agent,
                                    "zr",
                                    scopeKind,
                                    &agent->state->global->zrObject,
                                    &values[index++]);
        zr_debug_reference_summary_from_scope(scopeKind,
                                              values[index - 1u].name,
                                              values[index - 1u].reference_summary,
                                              sizeof(values[index - 1u].reference_summary));
        if (agent->state->hasCurrentException) {
            zr_debug_fill_text_preview(&values[index++],
                                       scopeKind,
                                       "error",
                                       "exception",
                                       "currentException",
                                       zr_debug_register_exception_handle(agent));
            zr_debug_reference_summary_from_scope(scopeKind,
                                                  values[index - 1u].name,
                                                  values[index - 1u].reference_summary,
                                                  sizeof(values[index - 1u].reference_summary));
        }

        *outValues = values;
        *outCount = index;
        if (outNamedVariables != ZR_NULL) {
            *outNamedVariables = index;
        }
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

        receiverValue = zr_debug_frame_value_slot(agent->state, function, callInfo, receiverSlot);
        prototype = zr_debug_resolve_value_prototype(agent->state, receiverValue);
        if (prototype == ZR_NULL) {
            return ZR_FALSE;
        }

        if (scopeKind == ZR_DEBUG_SCOPE_KIND_PROTOTYPE) {
            TZrBool ok = zr_debug_expand_prototype_summary(agent, prototype, outValues, outCount);
            if (ok && outNamedVariables != ZR_NULL) {
                *outNamedVariables = *outCount;
            }
            return ok;
        }

        {
            TZrBool ok = zr_debug_expand_statics_scope(agent, prototype, outValues, outCount);
            if (ok && outNamedVariables != ZR_NULL) {
                *outNamedVariables = *outCount;
            }
            return ok;
        }
    }

    return ZR_FALSE;
}

void ZrDebug_Free(void *pointer) {
    if (pointer != ZR_NULL) {
        free(pointer);
    }
}

/**
 * @def ZR_DEBUG_TYPE_OBJECT_FIELD_COUNT
 *
 * ## Summary
 * 规定类型对象向变量视图公开的六个合成入口数量，使分配大小、namedVariables 与固定填充位置使用同一展示模型。
 *
 * ## Value model
 * 值为 TZrSize 的 6；仅约束调试展示层，不是 SZrObjectPrototype 的内存字段数、ABI 大小或任意描述符总数。
 *
 * ## Invariants and lifecycle
 * 宏没有资源和线程状态；改变其值必须同时核对 expand_type_object_handle 的六个写入位置，不能只调整数量。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1128 (zr_debug_expand_type_object_handle)。
 */

/**
 * @def ZR_DEBUG_STRING_CHUNK_SIZE
 *
 * ## Summary
 * 规定长字符串每个索引子项的原始字节跨度，供块数计算、区间名称和局部复制缓冲区共同使用。
 *
 * ## Value model
 * 值为 TZrSize 的 64，单位是字节而非字符；末块可短于 64。固定切片不保证 UTF-8 字符边界，既有 byte_slice BUG 仍保留。
 *
 * ## Invariants and lifecycle
 * 宏不分配、不改变 VM 字符串；分页数组和字符串借用寿命由各自函数负责。调整该值需一起核对块数、乘法、局部缓冲区和协议文本。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:186 (zr_debug_string_chunk_count)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1036 (zr_debug_expand_string_chunks_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1038 (zr_debug_expand_string_chunks_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1042 (zr_debug_expand_string_chunks_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1043 (zr_debug_expand_string_chunks_handle)。
 */

/**
 * @fn const SZrTypeValue *zr_debug_object_get_field(SZrState *state, SZrObject *object, const TZrChar *fieldName)
 *
 * ## Summary
 * 为快照枚举或异常元数据投影，把字段名转换为 Core 字符串键后读取原始对象字段；不走脚本成员求值或用户 getter。
 *
 * ## Parameters
 * @param[in,out] state 键驻留与原始字段查询使用的 VM 状态；可为 null，届时失败。
 * @param[in] object 被查询对象，需由同一 VM 保持存活。
 * @param[in] fieldName 以 NUL 结束的原生字段名。
 *
 * ## Returns
 * @return 找到时返回借用的 SZrTypeValue 指针；普通无效输入、键创建失败或查无字段返回 null，非局部 Throw 不转换为 null。
 *
 * ## Constraints and lifecycle
 * state/object/fieldName 为空时返回 null；字符串创建普通失败或字段缺失也返回 null。Core 字符串创建可分配并 Throw；Object_GetValue 可沿 prototype 查询，不能把本函数称为无分配或仅查自有哈希项。
 *
 * 返回指针借用对象存储，随 VM 对象/字段变更失效；调用方不得 free 或跨恢复执行保存。此处不建立异常保护或 GC 根。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:227 (zr_debug_exception_frames_object)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:275 (zr_debug_exception_read_frame)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:284 (zr_debug_exception_read_frame)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:291 (zr_debug_exception_read_frame)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:296 (zr_debug_exception_read_frame)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:872 (zr_debug_try_resolve_indexed_storage)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:887 (zr_debug_try_resolve_indexed_storage)。
 */

/**
 * @fn TZrBool zr_debug_make_string_key(SZrState *state, const TZrChar *name, SZrTypeValue *outKey)
 *
 * ## Summary
 * 把原生名称构造成仅用于 Core 对象查表的字符串值，供描述符视图及借用的安全成员读取复用。
 *
 * ## Parameters
 * @param[in,out] state 用于字符串驻留的 VM 状态。
 * @param[in] name NUL 结束的待查询名称。
 * @param[out] outKey 字符串查表键；普通失败时保持 null 值。
 *
 * ## Returns
 * @return 字符串创建并写入 STRING 类型键后返回 TRUE；普通参数或创建失败返回 FALSE。
 *
 * ## Constraints and lifecycle
 * outKey 非空时首先 ResetAsNull；state、name、outKey 任一为空普通返回 FALSE。字符串驻留可普通返回 null，也可经 Core Throw；不执行 getter。
 *
 * 输出仅引用 VM 管理的字符串，不创建可由 ZrDebug_Free 释放的独立内存；VM 与该字符串必须覆盖查表期间。失败不承诺捕获非局部退出。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1454 (zr_debug_expand_prototype_view)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2106 (zr_debug_safe_get_member_value)。
 */

/**
 * @fn void zr_debug_copy_value_snapshot(SZrTypeValue *destination, const SZrTypeValue *source)
 *
 * ## Summary
 * 为调试读取复制 SZrTypeValue 的展示快照，并去掉会使接收方误以为取得对象所有权的元数据。
 *
 * ## Parameters
 * @param[out] destination 待覆盖的快照槽；允许 null。
 * @param[in] source 借用源值；允许 null，且不得与 destination 别名以免先清零源。
 *
 * ## Returns
 * 无返回值；destination 非空时写入借用快照，destination 为 null 时无操作。
 *
 * ## Constraints and lifecycle
 * destination 为 null 时无操作；source 为 null 时输出被清为 null。非 GC 对象值或空对象清空所有权和弱引用数据；GC 对象仅在原 ownership 非 NONE 时降为 BORROWED，并清空 weakReference。
 *
 * 这是浅复制，不 retain、pin 或复制 VM 对象。目标仅在原对象仍存活的停点内读取；不能据该快照释放原对象，也不提供独立跨 GC/恢复执行保活。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:689 (zr_debug_register_string_chunks_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:830 (zr_debug_read_indexed_storage_value)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2020 (zr_debug_resolve_identifier_value)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2049 (zr_debug_resolve_identifier_value)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2057 (zr_debug_resolve_identifier_value)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2061 (zr_debug_resolve_identifier_value)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2065 (zr_debug_resolve_identifier_value)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2117 (zr_debug_safe_get_member_value)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2143 (zr_debug_safe_get_member_value)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2196 (zr_debug_safe_get_index_value)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2218 (zr_debug_safe_get_index_value)。
 */

/**
 * @fn const TZrChar *zr_debug_protocol_name(EZrProtocolId protocolId)
 *
 * ## Summary
 * 将原型协议枚举转成协议视图所用的稳定展示名称。
 *
 * ## Parameters
 * @param[in] protocolId 待显示的协议枚举值。
 *
 * ## Returns
 * @return 返回静态、NUL 结束的名称；未识别值返回 None。
 *
 * ## Constraints and lifecycle
 * 仅映射实现中列出的协议；NONE 或未知值回退为 None。该名称不是协议注册、有效性校验或脚本成员解析。
 *
 * 返回静态常量字符串，不分配、不释放，也不依赖停点句柄生命周期。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1339 (zr_debug_expand_prototype_view)。
 */

/**
 * @fn const TZrChar *zr_debug_value_type_name_safe(SZrState *state, const SZrTypeValue *value)
 *
 * ## Summary
 * 为变量预览、窗口和求值结果选择类型名，优先显示对象原型名称，其余回退 Core 值类型名称。
 *
 * ## Parameters
 * @param[in] state 查询原型使用的 VM 状态；需与 value 匹配。
 * @param[in] value 只读值，可为 null。
 *
 * ## Returns
 * @return 返回可供立即复制的类型名称；空 state/value 为 value，无可用原型名称时使用本地类型枚举回退。
 *
 * ## Constraints and lifecycle
 * state 或 value 为空时返回静态 value；对 OBJECT/ARRAY 尝试原型名，原型名存在才优先使用，否则回退 zr_debug_value_type_name。类型名选择不执行表达式，也不证明对象可安全跨停点借用。
 *
 * 结果可能借用 VM 原型字符串，也可能是静态名称；调用方通常立即复制到固定预览缓冲区，不应 free 或跨 VM/原型销毁缓存。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:764 (zr_debug_fill_value_preview)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2452 (zr_debug_try_evaluate_index_window)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2557 (zr_debug_evaluate_with_policy)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2763 (ZrDebug_ReadStack)。
 */

/**
 * @fn TZrBool zr_debug_value_is_expandable(SZrState *state, const SZrTypeValue *value)
 *
 * ## Summary
 * 在创建预览之前判定值是否具有对象/数组的可展开载体。
 *
 * ## Parameters
 * @param[in] state 为统一辅助接口保留，本实现不读取。
 * @param[in] value 待检查的只读值；允许 null。
 *
 * ## Returns
 * @return 满足 OBJECT/ARRAY 与非空载体条件时 TRUE，其余 FALSE。
 *
 * ## Constraints and lifecycle
 * value 必须为 OBJECT 或 ARRAY 且 value.object 非空；只据类型与载体判断，不枚举字段、不检查授权，也不保证后续句柄分配成功。state 参数在本实现中未使用。
 *
 * 不分配、不 retain、不注册句柄；返回 TRUE 不能延长对象或停点寿命。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:757 (zr_debug_fill_value_preview)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2562 (zr_debug_evaluate_with_policy)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2769 (ZrDebug_ReadStack)。
 */

/**
 * @fn SZrString *zr_debug_value_as_string(SZrState *state, const SZrTypeValue *value)
 *
 * ## Summary
 * 从类型正确的值中取得用于长字符串分页的 Core 字符串对象。
 *
 * ## Parameters
 * @param[in] state 用于 Core 字符串类型转换的 VM 状态。
 * @param[in] value 借用的候选字符串值。
 *
 * ## Returns
 * @return 返回借用 SZrString 指针；普通类型或参数不满足时返回 null。
 *
 * ## Constraints and lifecycle
 * state/value 为空、非 STRING 类型或载体为空时返回 null；不进行类型转换或脚本字符串化。
 *
 * 返回 VM 管理对象的借用指针；不分配、不取得所有权，使用者必须保证 VM 和值仍存活。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:192 (zr_debug_string_needs_chunk_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1006 (zr_debug_expand_string_chunks_handle)。
 */

/**
 * @fn TZrSize zr_debug_string_chunk_count(TZrSize byteLength)
 *
 * ## Summary
 * 按固定字节块大小计算字符串分页的逻辑子项总数。
 *
 * ## Parameters
 * @param[in] byteLength Core 字符串的字节长度。
 *
 * ## Returns
 * @return 返回以 ZR_DEBUG_STRING_CHUNK_SIZE 字节为单位的块数。
 *
 * ## Constraints and lifecycle
 * 0 字节对应 0 块；非零长度使用向上取整的字节算术。该数量不是 Unicode 字符数，未增加对 TZrSize 极端加法溢出的独立保护。
 *
 * 纯算术，不分配、不触碰字符串或句柄表；调用方提供实际 Core 字节长度。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:209 (zr_debug_string_needs_chunk_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1013 (zr_debug_expand_string_chunks_handle)。
 */

/**
 * @fn TZrBool zr_debug_string_needs_chunk_handle(SZrState *state, const SZrTypeValue *value, TZrSize *outChunkCount)
 *
 * ## Summary
 * 判定字符串原始字节长度是否达到预览容量，从而需要向客户端暴露分页句柄。
 *
 * ## Parameters
 * @param[in] state 字符串类型转换使用的 VM 状态。
 * @param[in] value 候选字符串值。
 * @param[out] outChunkCount 可选块数输出；普通不需分页时为 0。
 *
 * ## Returns
 * @return 达到阈值且块数大于 0 时 TRUE，否则 FALSE。
 *
 * ## Constraints and lifecycle
 * outChunkCount 非空时先清零；非字符串或字节长度小于 ZR_DEBUG_TEXT_CAPACITY 返回 FALSE。达到阈值后用固定块大小计算数量，不按字符边界切分。
 *
 * 不注册句柄或分配缓冲区；输入字符串借用，仅在当前 VM 值有效期间读取。后续注册仍可能普通失败。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:679 (zr_debug_register_string_chunks_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:704 (zr_debug_attach_string_chunk_handle)。
 */

/**
 * @fn const TZrChar *zr_debug_prototype_type_name(EZrObjectPrototypeType type)
 *
 * ## Summary
 * 把原型类别转换为原型摘要和安全格式化使用的文本类别。
 *
 * ## Parameters
 * @param[in] type 被显示的原型类别枚举。
 *
 * ## Returns
 * @return 返回静态类别名称，未知类别为 prototype。
 *
 * ## Constraints and lifecycle
 * 仅展示已列举的原型类型；未知枚举回退 prototype，不验证实际原型布局。
 *
 * 返回静态字符串，无分配和释放义务。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:447 (zr_debug_format_value_text_safe)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1237 (zr_debug_expand_prototype_summary)。
 */

/**
 * @fn SZrObjectPrototype *zr_debug_resolve_value_prototype(SZrState *state, const SZrTypeValue *value)
 *
 * ## Summary
 * 从借用值解析展示所需的原型：原型对象载体先返回自身，其余对象优先使用显式 prototype，再按值类型回退 global 的基础类型原型。
 *
 * ## Parameters
 * @param[in] state 拥有基础类型原型表的 VM 状态。
 * @param[in] value 只读的候选值。
 *
 * ## Returns
 * @return 返回借用原型；参数不成立或该类型无基础原型时返回 null。
 *
 * ## Constraints and lifecycle
 * state、global 或 value 为空时返回 null；OBJECT/ARRAY 的载体若为 OBJECT_PROTOTYPE，先返回载体自身；其余对象先取非空 prototype，再按类型枚举回退基础原型表。结果是元数据指针，不进行脚本类型转换、成员访问或注册。
 *
 * 无所有权转移；原型及其名称借用 VM/编译元数据，调用者需保证同一 VM 生命周期和安全读取时序。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_child_shape.c:101 (zr_debug_value_child_shape)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_child_shape.c:122 (zr_debug_semantic_type_name)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:154 (zr_debug_value_type_name_safe)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:442 (zr_debug_format_value_text_safe)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:612 (zr_debug_register_value_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:648 (zr_debug_register_index_window_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1726 (zr_debug_expand_value_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2125 (zr_debug_safe_get_member_value)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2204 (zr_debug_safe_get_index_value)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3340 (ZrDebug_ReadVariables)。
 */

/**
 * @fn void zr_debug_format_value_text_safe(SZrState *state, const SZrTypeValue *value, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 为协议变量和 logpoint 生成有限长度的值文本；以基本值、函数元数据或对象类别摘要替代任意脚本字符串化。
 *
 * ## Parameters
 * @param[in] state 读取对象/函数元数据所用 VM 状态，需与 value 一致。
 * @param[in] value 要显示的借用值；null 指针使有效缓冲区保持空串。
 * @param[out] buffer 调用方提供的文本缓冲区。
 * @param[in] bufferSize 缓冲区字节容量；0 时无操作。
 *
 * ## Returns
 * 无返回值；结果写入有效缓冲区，缓冲区为空或容量为 0 时不写。
 *
 * ## Constraints and lifecycle
 * buffer 为空或容量为 0 时不写；有效缓冲区先清空，state/value 为空时直接返回空串。实际 NULL 类型显示 null，其它已实现分支保留 NUL。字符串按字节拷贝并可能截断，不保证完整 UTF-8 字符、嵌入 NUL 的无损表示或对象全部内容。
 *
 * 调用方拥有缓冲区，输入值及元数据只借用。此函数不求值表达式或执行 getter；safe 是当前格式化策略，不等于对任意无效指针、并发 VM 或所有底层异常免疫。
 *
 * ## References
 * tests/debug/test_debug_truncation.c:51 (test_long_string_value_preview_marks_truncation)；zr_vm_lib_debug/src/zr_vm_lib_debug/breakpoint/debug_breakpoint_logpoint.c:89 (zr_debug_breakpoint_logpoint_format)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:772 (zr_debug_fill_value_preview)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1780 (zr_debug_expand_value_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2560 (zr_debug_evaluate_with_policy)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2764 (ZrDebug_ReadStack)。
 */

/**
 * @fn ZrDebugVariableHandle *zr_debug_find_variable_handle(ZrDebugAgent *agent, TZrUInt32 handleId)
 *
 * ## Summary
 * 按协议句柄号查找当前 stopStateId 的变量句柄，拒绝恢复执行前的旧停点引用。
 *
 * ## Parameters
 * @param[in] agent 拥有当前停点和句柄表的 Agent。
 * @param[in] handleId 客户端提供的变量句柄号。
 *
 * ## Returns
 * @return 匹配时返回内部条目，否则 null；不分配或修改表。
 *
 * ## Constraints and lifecycle
 * agent 为空或 ID 小于 ZR_DEBUG_VARIABLE_HANDLE_BASE 返回 null；遍历只接受 handle_id 与 agent 当前 state_id 同时相等的条目。无匹配时不退回低位作用域解码。
 *
 * 返回 Agent 动态数组内部借用地址；任何后续注册的 realloc、clear 或 AgentStop 都可能使地址失效。函数不加锁，调用方须已进入正确的暂停线程访问上下文。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3023 (ZrDebug_ReadVariables)。
 */

/**
 * @fn ZrDebugVariableHandle *zr_debug_alloc_variable_handle(ZrDebugAgent *agent)
 *
 * ## Summary
 * 为当前停点追加清零的句柄条目，统一生成所有动态变量视图的协议 ID。
 *
 * ## Parameters
 * @param[in,out] agent 当前安全访问上下文中的 Agent，句柄数组和下一个 ID 将被更新。
 *
 * ## Returns
 * @return 成功返回新的内部条目；无效 Agent、ID 区间或普通分配失败返回 null。
 *
 * ## Constraints and lifecycle
 * 无 Agent 或 nextVariableHandleId 已低于保留区时失败；容量不足以 16 起步、之后翻倍 realloc。普通 realloc 失败保留旧数组/数量；成功后数量递增并记录 stopStateId 与下一个 ID。
 *
 * 返回内部数组地址，只供本次注册立即填充。后续 realloc 可使旧条目指针失效；clear 不回退 ID。此处不加锁、pin VM 值，也未新增容量乘法极值保护。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:534 (zr_debug_register_prototype_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:555 (zr_debug_register_prototype_view_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:575 (zr_debug_register_global_state_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:591 (zr_debug_register_exception_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:618 (zr_debug_register_value_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:641 (zr_debug_register_index_window_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:663 (zr_debug_register_inline_union_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:683 (zr_debug_register_string_chunks_handle)。
 */

/**
 * @fn TZrUInt32 zr_debug_register_prototype_handle(ZrDebugAgent *agent, SZrObjectPrototype *prototype)
 *
 * ## Summary
 * 把原型元数据保存为停点内可展开的 PROTOTYPE 句柄，供类型对象和 $prototype 子项读取。
 *
 * ## Parameters
 * @param[in,out] agent 拥有当前停点句柄表及有效 VM state 的 Agent。
 * @param[in] prototype 由 VM/编译元数据保持存活的原型。
 *
 * ## Returns
 * @return 成功返回动态句柄 ID，普通参数或分配失败返回 0。
 *
 * ## Constraints and lifecycle
 * agent 或 prototype 为空返回 0；普通句柄分配失败返回 0。记录原型并初始化展示用 OBJECT 值，不复制原型内容。有效 Agent 的 state 必须符合 Core 初始化前提。
 *
 * 原型和其中的 VM 对象均借用；C 句柄表没有独立 retain/释放原型的职责。协议 ID 仅限创建停点，恢复/clear 后失效。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:615 (zr_debug_register_value_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1247 (zr_debug_expand_prototype_summary)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1807 (zr_debug_expand_value_handle)。
 */

/**
 * @fn TZrUInt32 zr_debug_register_prototype_view_handle(ZrDebugAgent *agent, SZrObjectPrototype *prototype, EZrDebugPrototypeViewKind viewKind)
 *
 * ## Summary
 * 为指定原型建立一个元数据、成员、方法、属性、静态项或协议的子视图句柄。
 *
 * ## Parameters
 * @param[in,out] agent 当前安全停点中的 Agent 及句柄表。
 * @param[in] prototype 被展示的借用原型。
 * @param[in] viewKind 要记录的非 NONE 子视图枚举。
 *
 * ## Returns
 * @return 注册成功的句柄 ID；普通参数或分配失败为 0。
 *
 * ## Constraints and lifecycle
 * agent/prototype 为空或 viewKind 为 NONE 时拒绝；非 NONE 的其它枚举值原样保存，具体展开由视图分支决定。普通句柄分配失败返回 0。
 *
 * 只保存借用原型与枚举，不提前枚举描述符或执行属性。有效 Agent state 为 Core 初始化前提；ID 和原型借用不得越过当前停点/元数据寿命。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1156 (zr_debug_expand_type_object_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1164 (zr_debug_expand_type_object_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1172 (zr_debug_expand_type_object_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1180 (zr_debug_expand_type_object_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1188 (zr_debug_expand_type_object_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1196 (zr_debug_expand_type_object_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1283 (zr_debug_expand_prototype_summary)。
 */

/**
 * @fn TZrUInt32 zr_debug_register_global_state_handle(ZrDebugAgent *agent)
 *
 * ## Summary
 * 为 Globals 中的 zrState 合成节点建立 GLOBAL_STATE 句柄，展开时再读取当前状态摘要。
 *
 * ## Parameters
 * @param[in,out] agent 拥有有效 state 的当前停点 Agent。
 *
 * ## Returns
 * @return 成功返回句柄 ID，普通无效输入或分配失败返回 0。
 *
 * ## Constraints and lifecycle
 * agent 或 state 为空拒绝；普通分配失败返回 0。句柄本身不复制全局状态，也不枚举脚本全局对象。
 *
 * 只给 Agent 当前 state 一个停点内入口；继续运行、换停点或停止时由清表失效，不取得 VM 状态所有权。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3289 (ZrDebug_ReadVariables)。
 */

/**
 * @fn TZrUInt32 zr_debug_register_exception_handle(ZrDebugAgent *agent)
 *
 * ## Summary
 * 在当前 VM 存在异常时建立 EXCEPTION 句柄，保存当前异常值供变量展示。
 *
 * ## Parameters
 * @param[in,out] agent 具有当前异常的暂停 Agent。
 *
 * ## Returns
 * @return 注册成功的异常句柄 ID，否则 0。
 *
 * ## Constraints and lifecycle
 * agent/state 为空或 hasCurrentException 为 FALSE 时返回 0；普通分配失败也返回 0。保存 currentException 的浅副本，不单独复制异常栈或消息。
 *
 * 值仍由 VM 异常状态保持存活；句柄清理仅释放 C 表，不按副本的 ownership 字段释放 VM 对象。只在创建停点内使用。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3314 (ZrDebug_ReadVariables)。
 */

/**
 * @fn TZrUInt32 zr_debug_register_value_handle(ZrDebugAgent *agent, const SZrTypeValue *value)
 *
 * ## Summary
 * 为 OBJECT/ARRAY 的变量预览建立 VALUE 句柄；原型对象转用专用 PROTOTYPE 句柄。
 *
 * ## Parameters
 * @param[in,out] agent 当前停点的 Agent/句柄表。
 * @param[in] value 要展开的借用对象或数组值。
 *
 * ## Returns
 * @return 成功返回 VALUE 或 PROTOTYPE 句柄 ID，普通无效输入或分配失败为 0。
 *
 * ## Constraints and lifecycle
 * 要求有效 agent/state、值类型和非空载体。普通分配失败返回 0；原型对象先委托原型注册。其它值存储浅拷贝及当前解析出的原型，不执行 getter。
 *
 * 不取得输入对象的独立所有权或 GC pin；即使保存原 ownership 位，清表也只 free C 数组。调用方保证对象在停点内存活且不并发恢复 VM。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:758 (zr_debug_fill_value_preview)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2563 (zr_debug_evaluate_with_policy)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2770 (ZrDebug_ReadStack)。
 */

/**
 * @fn TZrUInt32 zr_debug_register_index_window_handle(ZrDebugAgent *agent, const SZrTypeValue *value, TZrSize start, TZrSize count)
 *
 * ## Summary
 * 保存对象/数组基值及一个逻辑索引窗口，供后续 ReadVariables 在窗口内分页。
 *
 * ## Parameters
 * @param[in,out] agent 当前停点 Agent。
 * @param[in] value 窗口的借用容器基值。
 * @param[in] start 保存的底层窗口起点。
 * @param[in] count 保存的窗口长度。
 *
 * ## Returns
 * @return 成功返回索引窗口句柄 ID，普通拒绝或分配失败为 0。
 *
 * ## Constraints and lifecycle
 * 要求有效 agent/state、OBJECT/ARRAY 类型及非空载体；保存 start/count 而不在此验证底层索引长度，也不执行窗口表达式。普通分配失败返回 0。
 *
 * 输入值、原型为浅借用；窗口在后续读取时重解析存储，不能据此冻结容器内容。ID 随停点失效；表达式解析本身属于借用调用者，不计本单元覆盖。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2444 (zr_debug_try_evaluate_index_window)。
 */

/**
 * @fn TZrUInt32 zr_debug_register_inline_union_handle(ZrDebugAgent *agent, TZrUInt32 frameId, TZrUInt32 stackSlot)
 *
 * ## Summary
 * 仅记录帧 ID 和槽号，建立稍后由 Union 展开器解析的 INLINE_UNION 句柄。
 *
 * ## Parameters
 * @param[in,out] agent 当前停点 Agent。
 * @param[in] frameId 非零协议帧 ID。
 * @param[in] stackSlot 保存的帧槽索引，由后续展开器验证。
 *
 * ## Returns
 * @return 成功返回 INLINE_UNION 句柄 ID，普通输入或分配失败返回 0。
 *
 * ## Constraints and lifecycle
 * 要求有效 agent/state 和非零 frameId；不在此验证 stackSlot 是否可读，不物化 Union，不执行后续展开。普通分配失败返回 0。
 *
 * 该条目只保留标量定位信息，引用的帧/槽必须仍属于同一暂停状态；Union 物化实现和类型合同属于借用模块，不转移到本批信用。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1971 (zr_debug_fill_inline_union_preview)。
 */

/**
 * @fn TZrUInt32 zr_debug_register_string_chunks_handle(ZrDebugAgent *agent, const SZrTypeValue *value)
 *
 * ## Summary
 * 为达到预览阈值的字符串注册 STRING_CHUNKS 句柄，并保存借用字符串快照和逻辑块数。
 *
 * ## Parameters
 * @param[in,out] agent 具有有效 state 的当前停点 Agent。
 * @param[in] value 借用的候选长字符串值。
 *
 * ## Returns
 * @return 成功的字符串分页句柄 ID；不需分页或普通失败为 0。
 *
 * ## Constraints and lifecycle
 * 先复核实际字符串/阈值；短字符串、无效输入或普通句柄分配失败返回 0。窗口初始为 0 与完整块数，块计数按原始字节。
 *
 * copy_value_snapshot 不 retain 或 pin 字符串；只在 VM 保持源字符串且停点未失效时使用。注册成功不修正既有 UTF-8 字节切片问题。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:708 (zr_debug_attach_string_chunk_handle)。
 */

/**
 * @fn void zr_debug_attach_string_chunk_handle(ZrDebugAgent *agent, const SZrTypeValue *value, TZrUInt32 *variablesReference, TZrSize *namedVariables, TZrSize *indexedVariables)
 *
 * ## Summary
 * 在已有变量/求值预览上附加长字符串分页引用和索引子项数量。
 *
 * ## Parameters
 * @param[in,out] agent 当前停点 Agent；可能新增句柄。
 * @param[in] value 待检查的借用字符串值。
 * @param[in,out] variablesReference 可选引用输出；成功才覆盖。
 * @param[in,out] namedVariables 可选命名子项数；成功才设 0。
 * @param[in,out] indexedVariables 可选索引子项数；成功才写块数。
 *
 * ## Returns
 * 无返回值；仅在分页句柄注册成功后覆盖所提供的输出，未附加时保留原值。
 *
 * ## Constraints and lifecycle
 * 只在达到字符串阈值且注册成功后改写非空输出：引用为新 ID、named 为 0、indexed 为块数。不需分页或普通注册失败时保留调用者原输出值，不转换为错误返回。
 *
 * 会修改句柄表，可能 realloc；输出仅是协议标量，字符串仍借用并受当前停点限制。函数无错误返回，调用者不能据已完成预览假定句柄一定存在。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:774 (zr_debug_fill_value_preview)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2569 (zr_debug_evaluate_with_policy)。
 */

/**
 * @fn void zr_debug_fill_text_preview(ZrDebugValuePreview *preview, EZrDebugScopeKind scopeKind, const TZrChar *name, const TZrChar *typeName, const TZrChar *valueText, TZrUInt32 variablesReference)
 *
 * ## Summary
 * 初始化一个纯文本变量预览，复制名称、类型、值文本和调用者给出的子句柄引用。
 *
 * ## Parameters
 * @param[out] preview 可写预览结构；允许 null，届时无操作。
 * @param[in] scopeKind 展示所属作用域枚举。
 * @param[in] name 可为空的名称文本。
 * @param[in] typeName 可为空或空串的类型文本。
 * @param[in] valueText 可为空的展示文本。
 * @param[in] variablesReference 直接记录的子句柄号，0 表示无子引用。
 *
 * ## Returns
 * 无返回值；preview 为 null 时无操作，否则重置并填充预览。
 *
 * ## Constraints and lifecycle
 * preview 为 null 时无操作；非空时先 memset。name/typeName/valueText 为 null 时分别使用空名称、默认 value 类型和空值，非空空串原样保留；semantic_summary 取非空值文本否则取类型名。固定数组按 copy_text 截断。
 *
 * 预览持有文本副本和协议 ID，不拥有任何 VM 对象，也不注册/验证引用。调用者负责让给定句柄与本停点一致。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:761 (zr_debug_fill_value_preview)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1056 (zr_debug_expand_string_chunks_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1095 (zr_debug_expand_global_state)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1100 (zr_debug_expand_global_state)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1102 (zr_debug_expand_global_state)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1104 (zr_debug_expand_global_state)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1106 (zr_debug_expand_global_state)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1109 (zr_debug_expand_global_state)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1151 (zr_debug_expand_type_object_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1159 (zr_debug_expand_type_object_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1167 (zr_debug_expand_type_object_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1175 (zr_debug_expand_type_object_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1183 (zr_debug_expand_type_object_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1191 (zr_debug_expand_type_object_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1227 (zr_debug_expand_prototype_summary)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1233 (zr_debug_expand_prototype_summary)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1239 (zr_debug_expand_prototype_summary)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1253 (zr_debug_expand_prototype_summary)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1255 (zr_debug_expand_prototype_summary)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1257 (zr_debug_expand_prototype_summary)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1260 (zr_debug_expand_prototype_summary)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1266 (zr_debug_expand_prototype_summary)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1277 (zr_debug_expand_prototype_summary)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1337 (zr_debug_expand_prototype_view)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1362 (zr_debug_expand_prototype_view)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1465 (zr_debug_expand_prototype_view)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1569 (zr_debug_expand_union_value_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1590 (zr_debug_expand_union_value_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1649 (zr_debug_expand_inline_union_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1667 (zr_debug_expand_inline_union_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1802 (zr_debug_expand_value_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1973 (zr_debug_fill_inline_union_preview)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3284 (ZrDebug_ReadVariables)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3309 (ZrDebug_ReadVariables)。
 */

/**
 * @fn TZrBool zr_debug_fill_value_preview(ZrDebugAgent *agent, const TZrChar *name, EZrDebugScopeKind scopeKind, const SZrTypeValue *value, ZrDebugValuePreview *preview)
 *
 * ## Summary
 * 从同一个借用值生成类型、值文本、语义摘要和子节点数量，并在适用时注册对象或长字符串的停点句柄。
 *
 * ## Parameters
 * @param[in,out] agent 当前安全停点 Agent，句柄表可能增长。
 * @param[in] name 预览名称，可为空。
 * @param[in] scopeKind 预览归属作用域。
 * @param[in] value VM 保持存活的借用值。
 * @param[out] preview 可写预览结构。
 *
 * ## Returns
 * @return 必要指针非空并完成普通路径时 TRUE；显式无效指针守卫时 FALSE，不以子句柄分配结果作为全局成功判据。
 *
 * ## Constraints and lifecycle
 * agent/preview/value 为空时 FALSE；其它输入按各辅助函数前提读取。Union 格式化失败才回退普通格式化。对象注册普通 OOM 只留下引用 0，函数仍可返回 TRUE；child_shape/字符串及元数据辅助路径不因名称 safe 而保证不分配或不 Throw。
 *
 * 输出文本是副本，子引用和底层值仍受停点/VM 寿命限制。不取得新对象所有权，不对已成功注册的其它句柄做整体回滚，也不捕获跨层非局部退出。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:962 (zr_debug_expand_indexed_window)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1459 (zr_debug_expand_prototype_view)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1515 (zr_debug_expand_exception_value)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1567 (zr_debug_expand_union_value_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1588 (zr_debug_expand_union_value_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1665 (zr_debug_expand_inline_union_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1784 (zr_debug_expand_value_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1796 (zr_debug_expand_value_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3212 (ZrDebug_ReadVariables)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3258 (ZrDebug_ReadVariables)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3290 (ZrDebug_ReadVariables)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3299 (ZrDebug_ReadVariables)。
 */

/**
 * @fn void zr_debug_read_indexed_storage_value(SZrState *state, SZrObject *storage, TZrSize index, SZrTypeValue *outValue)
 *
 * ## Summary
 * 按整数索引读取内部数组存储中的一个值，转成不取得对象所有权的调试快照。
 *
 * ## Parameters
 * @param[in,out] state 原始索引查询使用的 VM 状态。
 * @param[in,out] storage 当前有效的内部数组对象，Core 读取可物化 raw 存储。
 * @param[in] index 要读取的实际底层数组索引。
 * @param[out] outValue 可选快照输出，缺失项为 null 值。
 *
 * ## Returns
 * 无返回值；outValue 非空时先清为 null，查询成功才写入借用快照；不捕获 Core 非局部退出。
 *
 * ## Constraints and lifecycle
 * outValue 非空时先清为 null；state/storage/outValue 缺失时无进一步读取。缺失索引保留 null。Core Object_GetValue 对活动 raw-array 索引可能先物化，不能承诺无分配或无 Throw。
 *
 * 输出由调用者栈槽持有但对象内容仍借用；不 retain、不释放原数组值，不捕获底层非局部退出。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:961 (zr_debug_expand_indexed_window)。
 */

/**
 * @fn TZrBool zr_debug_try_resolve_indexed_storage(SZrState *state, const SZrTypeValue *value, SZrObject **outStorage, const TZrChar **outSyntheticName, TZrSize *outLength)
 *
 * ## Summary
 * 为 child-shape、变量展开及借用的安全索引调用解析实际索引存储：直接数组，或容器内部 __zr_items/__zr_entries。
 *
 * ## Parameters
 * @param[in,out] state 同一 VM 的查表状态。
 * @param[in] value 候选对象/数组值。
 * @param[out] outStorage 可选内部存储对象输出。
 * @param[out] outSyntheticName 可选 data/entries 名称输出；直接数组为 null。
 * @param[out] outLength 可选逻辑长度输出，受合成分支 outStorage 前提限制。
 *
 * ## Returns
 * @return 找到直接数组或合成数组来源时 TRUE；普通无效参数或无相应来源时 FALSE。
 *
 * ## Constraints and lifecycle
 * 可选输出先清零；只处理有载体的 OBJECT/ARRAY。直接数组给逻辑长度；内部字段依次映射 data/entries，不执行索引运算符或脚本 getter。合成分支只有同时提供 outStorage 且转换结果为 ARRAY 才写出长度，TRUE 不保证所有可选输出都非空。
 *
 * storage 和合成名称分别借用 VM 对象与静态文本；字段名驻留/读取可分配、Throw 或物化 Core raw 存储。函数不锁定容器或取得所有权。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_child_shape.c:92 (zr_debug_value_child_shape)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1738 (zr_debug_expand_value_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2186 (zr_debug_safe_get_index_value)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2422 (zr_debug_try_evaluate_index_window)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3073 (ZrDebug_ReadVariables)。
 */

/**
 * @fn TZrBool zr_debug_expand_indexed_window(ZrDebugAgent *agent, SZrObject *storage, TZrSize baseIndex, TZrSize totalIndexed, TZrSize start, TZrSize countLimit, EZrDebugScopeKind scopeKind, ZrDebugValuePreview **outValues, TZrSize *outCount, TZrSize *outNamedVariables, TZrSize *outIndexedVariables)
 *
 * ## Summary
 * 在一个已解析的索引存储中生成分页预览；start/count 相对于窗口，baseIndex 才是底层绝对索引。
 *
 * ## Parameters
 * @param[in,out] agent 当前安全停点 Agent。
 * @param[in,out] storage 有效内部数组存储。
 * @param[in] baseIndex 底层窗口的绝对索引起点。
 * @param[in] totalIndexed 该窗口的逻辑索引总数。
 * @param[in] start 本页相对窗口的起点。
 * @param[in] countLimit 本页上限，0 表示剩余全部。
 * @param[in] scopeKind 每个预览的作用域类型。
 * @param[out] outValues 必须非空的 C 预览数组输出。
 * @param[out] outCount 必须非空的本页实际数量输出。
 * @param[out] outNamedVariables 可选命名数量，设 0。
 * @param[out] outIndexedVariables 可选完整窗口数量，不是本页数量。
 *
 * ## Returns
 * @return 成功或空页为 TRUE；普通参数/分配失败为 FALSE，空页允许数组为 null 且数量 0。
 *
 * ## Constraints and lifecycle
 * outValues/outCount 必须非空，进入即清为 null/0；可选 named 为 0、indexed 为 totalIndexed。start 超界夹到末尾，countLimit=0 表示剩余全部；非空页 calloc 普通失败返回 FALSE，但 indexed 可能已写完整数量。未新增极端加法溢出保护。
 *
 * 成功数组由调用者 ZrDebug_Free；VM 值与新增子句柄仍借用停点。逐项 Core 读取和预览可能分配/Throw，未对非局部退出或已注册子句柄承诺统一回滚。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1744 (zr_debug_expand_value_handle)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3088 (ZrDebug_ReadVariables)。
 */

/**
 * @fn TZrBool zr_debug_expand_string_chunks_handle(ZrDebugAgent *agent, const SZrTypeValue *value, TZrSize start, TZrSize countLimit, EZrDebugScopeKind scopeKind, ZrDebugValuePreview **outValues, TZrSize *outCount, TZrSize *outNamedVariables, TZrSize *outIndexedVariables)
 *
 * ## Summary
 * 把长字符串按块索引分页成命名为 [byteStart..byteEnd) 的子项，向协议层返回独立的 C 预览数组。
 *
 * ## Parameters
 * @param[in] agent 当前停点 Agent，仅提供字符串所属 state。
 * @param[in] value VM 保持存活的借用字符串值。
 * @param[in] start 本页开始的块索引。
 * @param[in] countLimit 块数上限，0 为剩余全部。
 * @param[in] scopeKind 每项预览的作用域。
 * @param[out] outValues 必须非空，成功接收 C 预览数组。
 * @param[out] outCount 必须非空，本页块数。
 * @param[out] outNamedVariables 可选，设 0。
 * @param[out] outIndexedVariables 可选，写完整块数，普通分配失败也可能已经写入。
 *
 * ## Returns
 * @return 普通成功或空页为 TRUE；无效输入、非字符串或非空页 calloc 失败为 FALSE。
 *
 * ## Constraints and lifecycle
 * 输出先清零；要求有效 agent/state、STRING 值和两个必需输出。start/countLimit 以块计，0 上限表示剩余全部；indexed 输出为完整块数。块内最多拷贝 64 字节并补 NUL，不做 Unicode 边界修整，也不保证嵌入 NUL 后的文本保真。
 *
 * 数组由 ZrDebug_Free 释放；只在调用期间借用字符串原生字节。BUG: 沿用既有 byte_slice 家族，63 个 ASCII 后接 é 可使首块仅含 C3，协议 JSON/客户端 UTF-8 解码产生 U+FFFD；本批只记录静态链，不修复、不声称已运行复现。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3110 (ZrDebug_ReadVariables)。
 * 本函数 1031–1057 的 byte_slice 旧块与函数行是同一既有 BUG 家族的两个台账镜像，不能重复计问题。
 */

/**
 * @fn TZrUInt32 zr_debug_loaded_module_count(ZrDebugAgent *agent)
 *
 * ## Summary
 * 读取 loadedModulesRegistry 原生数组的逻辑长度，供全局调试摘要展示。
 *
 * ## Parameters
 * @param[in] agent 提供当前 VM/global 的只读 Agent。
 *
 * ## Returns
 * @return 逻辑数组长度转换后的计数；无有效注册表时 0。
 *
 * ## Constraints and lifecycle
 * 无 agent/state/global 或注册表对象载体为空时为 0；实现没有独立的值类型 ARRAY 检查，调用前提是 global 注册表确为 Core 数组对象。调用 ZrCore_Object_SuperArrayLength，不能以 nodeMap.elementCount 替代逻辑长度；TZrUInt32 转换未新增超大长度保护。
 *
 * 只借用注册表，不枚举、加载或卸载模块，不分配/释放对象。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1099 (zr_debug_expand_global_state)。
 */

/**
 * @fn TZrBool zr_debug_expand_global_state(ZrDebugAgent *agent, ZrDebugValuePreview **outValues, TZrSize *outCount)
 *
 * ## Summary
 * 展开 zrState 合成节点的六个摘要：模块名、已加载模块数、帧深度、异常标志、线程状态和停止行。
 *
 * ## Parameters
 * @param[in] agent 当前暂停状态的 Agent。
 * @param[out] outValues 必须非空的六项数组输出。
 * @param[out] outCount 必须非空的实际数量输出。
 *
 * ## Returns
 * @return 六项构造完成为 TRUE，普通参数或分配失败为 FALSE。
 *
 * ## Constraints and lifecycle
 * 先清 outValues/outCount；要求有效 agent/state 及必需输出，六项 calloc 普通失败返回 FALSE。此处不是脚本全局变量全集，loadedModuleCount 使用逻辑长度。
 *
 * 成功数组归调用者并用 ZrDebug_Free；文本复制到预览，不转移 VM 状态所有权。当前线程/停点必须保持稳定，函数不加锁或暂停执行。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3041 (ZrDebug_ReadVariables)。
 */

/**
 * @fn TZrBool zr_debug_expand_type_object_handle(ZrDebugAgent *agent, SZrObjectPrototype *prototype, ZrDebugValuePreview **outValues, TZrSize *outCount, TZrSize *outNamedVariables, TZrSize *outIndexedVariables)
 *
 * ## Summary
 * 把一个类型对象展开成六个原型子视图入口：metadata、members、methods、properties、statics、protocols。
 *
 * ## Parameters
 * @param[in,out] agent 有效 state 的当前停点 Agent，可能注册六个句柄。
 * @param[in] prototype 存活的借用原型。
 * @param[out] outValues 必须非空的输出数组指针。
 * @param[out] outCount 必须非空，成功为六项。
 * @param[out] outNamedVariables 可选，普通参数/分配失败前也已可写六项。
 * @param[out] outIndexedVariables 可选，设 0。
 *
 * ## Returns
 * @return 六项 C 数组构造成功为 TRUE，不等价于六个子句柄全部分配成功；普通参数/calloc 失败为 FALSE。
 *
 * ## Constraints and lifecycle
 * 数组/数量先清，named 先写固定六项、indexed 为 0；无效必需参数或 calloc 失败为 FALSE。逐子视图注册失败只使对应 reference 为 0，不使整组返回 FALSE。
 *
 * 数组归调用者；子句柄保存借用原型并受停点失效规则限制。不会复制完整类型描述符、执行属性或取得原型所有权。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3047 (ZrDebug_ReadVariables)。
 */

/**
 * @fn TZrBool zr_debug_expand_prototype_summary(ZrDebugAgent *agent, SZrObjectPrototype *prototype, ZrDebugValuePreview **outValues, TZrSize *outCount)
 *
 * ## Summary
 * 为原型元数据构造固定九项摘要，并为父原型及 managedFields 在适用时提供停点子句柄。
 *
 * ## Parameters
 * @param[in,out] agent 当前安全停点且 state 有效的 Agent。
 * @param[in] prototype 只借用的原型元数据。
 * @param[out] outValues 必须非空的九项数组输出。
 * @param[out] outCount 必须非空的数量输出。
 *
 * ## Returns
 * @return 九项构造成功为 TRUE；普通必需参数或数组分配失败为 FALSE。
 *
 * ## Constraints and lifecycle
 * 必需输出先清；agent/prototype/输出缺失或 calloc 失败为 FALSE。摘要是当前实现选取的九个展示字段，不代表完整 ABI 布局；嵌套句柄普通失败可留下引用 0。
 *
 * 调用者释放 C 数组；原型、名称、描述符继续借用 VM/编译元数据。新增句柄不会独立保活原型，不承诺整个摘要的子句柄原子注册。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1315 (zr_debug_expand_prototype_view)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3346 (ZrDebug_ReadVariables)。
 */

/**
 * @fn TZrBool zr_debug_expand_prototype_view(ZrDebugAgent *agent, SZrObjectPrototype *prototype, EZrDebugPrototypeViewKind viewKind, EZrDebugScopeKind scopeKind, ZrDebugValuePreview **outValues, TZrSize *outCount)
 *
 * ## Summary
 * 按指定视图筛选协议、managed fields 或描述符，先计数再生成对应变量预览；元数据视图委托九项摘要。
 *
 * ## Parameters
 * @param[in,out] agent 当前安全停点 Agent 及有效 state。
 * @param[in] prototype 计数和填充期间必须稳定的借用原型。
 * @param[in] viewKind 筛选或委托的原型视图枚举。
 * @param[in] scopeKind 结果预览所属作用域。
 * @param[out] outValues 必须非空的 C 数组输出，空视图可为 null。
 * @param[out] outCount 必须非空的实际条目数量。
 *
 * ## Returns
 * @return 普通筛选/空视图成功为 TRUE；无效必需参数或数组分配失败为 FALSE，底层 Throw 不转换为 FALSE。
 *
 * ## Constraints and lifecycle
 * 协议遍历 NONE 之后到 ARRAY_LIKE 的已实现区间；成员仅非静态 field，方法包括 method，属性取 property，静态项取 staticMember 或 isStatic。通过直接 Core 字段查找显示现值，未找到时显示描述符信息，不调用属性 getter；未知空视图可成功返回空数组。
 *
 * 数组由 ZrDebug_Free；名称/描述符/原型只借用。字符串键驻留和 Core 查表可 Throw，且计数/填充期间要求元数据稳定；普通 calloc 失败返回 FALSE，非局部退出和已注册子句柄无整体回滚保证。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1486 (zr_debug_expand_statics_scope)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3054 (ZrDebug_ReadVariables)。
 */

/**
 * @fn TZrBool zr_debug_expand_statics_scope(ZrDebugAgent *agent, SZrObjectPrototype *prototype, ZrDebugValuePreview **outValues, TZrSize *outCount)
 *
 * ## Summary
 * 将帧作用域中的 statics 请求复用到原型 STATICS 视图，并统一标记静态作用域。
 *
 * ## Parameters
 * @param[in,out] agent 传给原型视图的当前停点 Agent。
 * @param[in] prototype 借用原型。
 * @param[out] outValues 原型视图的必需数组输出。
 * @param[out] outCount 原型视图的必需数量输出。
 *
 * ## Returns
 * @return 原样返回 STATICS 视图展开结果；非局部退出不转换。
 *
 * ## Constraints and lifecycle
 * 只做枚举参数绑定；输入检查、直接字段查找、描述符回退、普通失败与 Throw 语义均来自 expand_prototype_view。并非脚本静态属性求值器。
 *
 * 原型借用和输出 C 数组的 ZrDebug_Free 责任不变，不建立新的所有权层或异常保护。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3354 (ZrDebug_ReadVariables)。
 */

/**
 * @fn TZrBool zr_debug_expand_exception_value(ZrDebugAgent *agent, ZrDebugValuePreview **outValues, TZrSize *outCount)
 *
 * ## Summary
 * 把当前 VM 异常值生成名为 error 的单个变量预览，供异常作用域或 EXCEPTION 句柄展开。
 *
 * ## Parameters
 * @param[in,out] agent 拥有当前异常的安全停点 Agent。
 * @param[out] outValues 必须非空的单项数组输出。
 * @param[out] outCount 必须非空，成功为 1。
 *
 * ## Returns
 * @return 普通单项构造成功为 TRUE；无异常、无效参数或数组分配失败为 FALSE。
 *
 * ## Constraints and lifecycle
 * 先清必需输出；要求有效 agent/state、hasCurrentException 和非空输出。calloc 普通失败为 FALSE；不在此验证外部异常帧号，也不展开异常栈数组。
 *
 * 输出数组由 ZrDebug_Free；异常对象借用 VM 当前异常根。预览辅助可能注册句柄/Throw，不承诺非局部退出时局部数组自动释放。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3065 (ZrDebug_ReadVariables)；zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3133 (ZrDebug_ReadVariables)。
 */

/**
 * @fn TZrBool zr_debug_expand_value_handle(ZrDebugAgent *agent, const SZrTypeValue *value, TZrSize start, TZrSize countLimit, EZrDebugScopeKind scopeKind, ZrDebugValuePreview **outValues, TZrSize *outCount, TZrSize *outNamedVariables, TZrSize *outIndexedVariables)
 *
 * ## Summary
 * 把对象/数组句柄分流为 Union、原生数组窗口或普通对象可见成员，并为内部数组来源及原型附加合成节点。
 *
 * ## Parameters
 * @param[in,out] agent 当前安全停点 Agent。
 * @param[in] value 要展开的借用对象/数组值。
 * @param[in] start 原生数组/委托分支的分页起点。
 * @param[in] countLimit 原生数组/委托分支的分页上限，0 为全部。
 * @param[in] scopeKind 结果作用域。
 * @param[out] outValues 必须非空的预览数组输出。
 * @param[out] outCount 必须非空的实际返回数。
 * @param[out] outNamedVariables 可选命名数量；普通对象成功后为实际项数。
 * @param[out] outIndexedVariables 可选索引数量；委托分支按自身合同写入。
 *
 * ## Returns
 * @return 普通构造或委托成功为 TRUE；无效输入、普通数组分配失败或委托普通失败为 FALSE。
 *
 * ## Constraints and lifecycle
 * 输出先清；要求有效 agent/state、OBJECT/ARRAY 载体及必需输出。Union 分支借用专用展开器，原生数组尊重 start/count；普通对象枚举自身 nodeMap 并隐藏格式化名称以 __zr_ 开头的项，不在该分支分页。计数与填充期间要求对象布局稳定。
 *
 * 成功 C 数组由 ZrDebug_Free；值、哈希节点、原型及子句柄只借用当前停点。普通 calloc 失败为 FALSE；子句柄普通 OOM 可显示引用 0，Core 查表/辅助路径可能 Throw，无统一回滚或并发安全保证。Union 实现只借证，不扩大本批单元信用。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3031 (ZrDebug_ReadVariables)。
 */

/**
 * @fn void zr_debug_variable_handles_clear(ZrDebugAgent *agent)
 *
 * ## Summary
 * 释放本停点的 C 句柄表，供恢复执行、换停点及停止 Agent 时统一使旧变量引用失效。
 *
 * ## Parameters
 * @param[in,out] agent 要清理句柄表的 Agent，允许 null。
 *
 * ## Returns
 * 无返回值；释放并清空 Agent 的 C 句柄表，agent 为 null 时无操作。
 *
 * ## Constraints and lifecycle
 * agent 为 null 时无操作；释放后指针设 null、数量和容量清零。nextVariableHandleId 保持不变，避免旧数字 ID 在清表后立即重用。
 *
 * 只 free C 数组，不按条目 ownership 位释放 VM 值、原型或异常，也不调用 GC unpin。所有内部条目指针立即失效；调用方必须同步暂停/恢复状态，函数本身不加锁。
 *
 * ## References
 * zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1661 (ZrDebug_AgentStop)；zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1859 (ZrDebug_Continue)；zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1888 (zr_debug_begin_step)；zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:233 (zr_debug_agent_fill_stop_event)；zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:1432 (zr_debug_agent_pause_loop)。
 */

/**
 * @fn void ZrDebug_Free(void *pointer)
 *
 * ## Summary
 * 释放调试快照接口交给调用者的 malloc/calloc 兼容 C 缓冲区。
 *
 * ## Parameters
 * @param[in] pointer 由调试快照接口移交、尚未释放的 C 分配块，允许 null。
 *
 * ## Returns
 * 无返回值；将非空 C 分配块交给 free，pointer 为 null 时无操作。
 *
 * ## Constraints and lifecycle
 * 仅非空时调用标准 free，null 可安全传入。不检查来源、大小或重复释放；不能传入 VM 对象、栈内存、借用原型或内部句柄条目地址。
 *
 * 释放后调用方自己的指针不自动清零，不能再次读取或释放；对子句柄引用的 VM 值不做析构，不替代 Agent 的停点清理。
 *
 * ## References
 * tests/debug/test_debug_agent_protocol.c:490 (test_debug_agent_projects_typed_test_manifest)；tests/debug/test_debug_formal_evaluation_cases.h:144 (test_debug_formal_array_literal_requires_explicit_allocation)；tests/debug/test_debug_formal_evaluation_cases.h:145 (test_debug_formal_array_literal_requires_explicit_allocation)；tests/debug/test_debug_truncation.c:128 (test_long_string_value_preview_exposes_paged_chunks)；tests/debug/test_debug_truncation.c:129 (test_long_string_value_preview_exposes_paged_chunks)；zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:931 (zr_debug_agent_make_stack_trace_result)；zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:1056 (zr_debug_agent_make_stack_trace_result)；zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:1065 (zr_debug_agent_make_stack_trace_result)；zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:1098 (zr_debug_agent_make_scopes_result)；zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:1119 (zr_debug_agent_make_scopes_result)；zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:1165 (zr_debug_agent_make_variables_result)；zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:1191 (zr_debug_agent_make_variables_result)。
 */
