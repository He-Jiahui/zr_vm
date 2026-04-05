#include "debug_internal.h"

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

    ZrCore_Value_InitAsInt(agent->state, &key, (TZrInt64)(frameId - 1));
    frameValue = ZrCore_Object_GetValue(agent->state, framesObject, &key);
    if (frameValue == ZR_NULL || frameValue->type != ZR_VALUE_TYPE_OBJECT || frameValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    frameObject = ZR_CAST_OBJECT(agent->state, frameValue->value.object);
    outFrame->frame_id = frameId;

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

static SZrCallInfo *zr_debug_find_call_info_by_frame_id(ZrDebugAgent *agent, TZrUInt32 frameId, SZrFunction **outFunction) {
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

static TZrBool zr_debug_fill_value_preview(SZrState *state,
                                           const TZrChar *name,
                                           EZrDebugScopeKind scopeKind,
                                           const SZrTypeValue *value,
                                           ZrDebugValuePreview *preview) {
    SZrTypeValue stableValue;
    SZrString *stringValue;

    if (preview == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(preview, 0, sizeof(*preview));
    preview->scope_kind = scopeKind;
    zr_debug_copy_text(preview->name, sizeof(preview->name), name != ZR_NULL ? name : "");
    zr_debug_copy_text(preview->type_name, sizeof(preview->type_name), zr_debug_value_type_name(value->type));
    stableValue = *value;
    stringValue = ZrCore_Value_ConvertToString(state, &stableValue);
    zr_debug_copy_text(preview->value_text,
                       sizeof(preview->value_text),
                       stringValue != ZR_NULL ? ZrCore_String_GetNativeString(stringValue) : "<unprintable>");
    preview->variables_reference = 0;
    return ZR_TRUE;
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

TZrBool ZrDebug_ReadStack(ZrDebugAgent *agent, ZrDebugFrameSnapshot **outFrames, TZrSize *outCount) {
    SZrCallInfo *callInfo;
    ZrDebugFrameSnapshot *frames;
    TZrUInt32 frameCount = 0;
    TZrUInt32 frameId = 1;
    TZrSize exceptionFrameCount;

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
        if (ZrCore_Closure_GetMetadataFunctionFromCallInfo(agent->state, callInfo) != ZR_NULL) {
            frameCount++;
        }
    }

    frames = frameCount > 0 ? (ZrDebugFrameSnapshot *)calloc(frameCount, sizeof(*frames)) : ZR_NULL;
    if (frameCount > 0 && frames == ZR_NULL) {
        return ZR_FALSE;
    }

    for (callInfo = agent->state->callInfoList; callInfo != ZR_NULL; callInfo = callInfo->previous) {
        SZrFunction *function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(agent->state, callInfo);
        if (function == ZR_NULL) {
            continue;
        }

        frames[frameId - 1].frame_id = frameId;
        frames[frameId - 1].instruction_index = zr_debug_instruction_offset(callInfo, function);
        frames[frameId - 1].line = zr_debug_call_info_line(agent, callInfo, function, frameId == 1);
        zr_debug_copy_text(frames[frameId - 1].function_name,
                           sizeof(frames[frameId - 1].function_name),
                           zr_debug_function_name(function));
        zr_debug_copy_text(frames[frameId - 1].source_file,
                           sizeof(frames[frameId - 1].source_file),
                           zr_debug_function_source(function));
        frameId++;
    }

    *outFrames = frames;
    *outCount = frameCount;
    return ZR_TRUE;
}

TZrBool ZrDebug_ReadScopes(ZrDebugAgent *agent,
                           TZrUInt32 frameId,
                           ZrDebugScopeSnapshot **outScopes,
                           TZrSize *outCount) {
    SZrFunction *function = ZR_NULL;
    ZrDebugScopeSnapshot *scopes = ZR_NULL;
    TZrSize count = 0;
    TZrSize index = 0;

    if (outScopes != ZR_NULL) {
        *outScopes = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (agent == ZR_NULL || outScopes == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    if (zr_debug_find_call_info_by_frame_id(agent, frameId, &function) == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    count = 2;
    if (function->closureValueLength > 0) {
        count++;
    }

    scopes = (ZrDebugScopeSnapshot *)calloc(count, sizeof(*scopes));
    if (scopes == ZR_NULL) {
        return ZR_FALSE;
    }

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

    *outScopes = scopes;
    *outCount = count;
    return ZR_TRUE;
}

TZrBool ZrDebug_ReadVariables(ZrDebugAgent *agent,
                              TZrUInt32 scopeId,
                              ZrDebugValuePreview **outValues,
                              TZrSize *outCount) {
    TZrUInt32 frameId = scopeId / 10u;
    EZrDebugScopeKind scopeKind = (EZrDebugScopeKind)(scopeId % 10u);
    SZrFunction *function = ZR_NULL;
    SZrCallInfo *callInfo;
    ZrDebugValuePreview *values = ZR_NULL;
    TZrSize valueCount = 0;
    TZrSize index = 0;

    if (outValues != ZR_NULL) {
        *outValues = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (agent == ZR_NULL || outValues == ZR_NULL || outCount == ZR_NULL || frameId == 0) {
        return ZR_FALSE;
    }

    callInfo = zr_debug_find_call_info_by_frame_id(agent, frameId, &function);
    if (callInfo == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    if (scopeKind == ZR_DEBUG_SCOPE_KIND_LOCALS) {
        TZrUInt32 pc = zr_debug_instruction_offset(callInfo, function);
        TZrUInt32 slotIndex;

        for (slotIndex = 0; slotIndex < function->stackSize; slotIndex++) {
            if (ZrCore_Function_GetLocalVariableName(function, slotIndex, pc) != ZR_NULL) {
                valueCount++;
            }
        }

        values = valueCount > 0 ? (ZrDebugValuePreview *)calloc(valueCount, sizeof(*values)) : ZR_NULL;
        if (valueCount > 0 && values == ZR_NULL) {
            return ZR_FALSE;
        }

        for (slotIndex = 0; slotIndex < function->stackSize; slotIndex++) {
            SZrString *name = ZrCore_Function_GetLocalVariableName(function, slotIndex, pc);
            SZrTypeValue *value;
            if (name == ZR_NULL) {
                continue;
            }

            value = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 1 + slotIndex);
            zr_debug_fill_value_preview(agent->state, zr_debug_string_native(name), scopeKind, value, &values[index]);
            index++;
        }
    } else if (scopeKind == ZR_DEBUG_SCOPE_KIND_CLOSURES) {
        SZrTypeValue *closureValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
        SZrClosure *closure = ZR_CAST_VM_CLOSURE(agent->state, closureValue->value.object);
        TZrUInt32 closureIndex;

        valueCount = function->closureValueLength;
        values = valueCount > 0 ? (ZrDebugValuePreview *)calloc(valueCount, sizeof(*values)) : ZR_NULL;
        if (valueCount > 0 && values == ZR_NULL) {
            return ZR_FALSE;
        }

        for (closureIndex = 0; closureIndex < function->closureValueLength; closureIndex++) {
            const SZrTypeValue *value = closure != ZR_NULL && closureIndex < closure->closureValueCount
                                                ? ZrCore_ClosureValue_GetValue(closure->closureValuesExtend[closureIndex])
                                                : &agent->state->global->nullValue;
            zr_debug_fill_value_preview(agent->state,
                                        zr_debug_string_native(function->closureValueList[closureIndex].name),
                                        scopeKind,
                                        value,
                                        &values[index]);
            index++;
        }
    } else if (scopeKind == ZR_DEBUG_SCOPE_KIND_GLOBALS) {
        valueCount = 2;
        values = (ZrDebugValuePreview *)calloc(valueCount, sizeof(*values));
        if (values == ZR_NULL) {
            return ZR_FALSE;
        }

        zr_debug_fill_value_preview(agent->state, "zr", scopeKind, &agent->state->global->zrObject, &values[index++]);
        zr_debug_fill_value_preview(agent->state,
                                    "loadedModules",
                                    scopeKind,
                                    &agent->state->global->loadedModulesRegistry,
                                    &values[index++]);
    } else {
        return ZR_FALSE;
    }

    *outValues = values;
    *outCount = valueCount;
    return ZR_TRUE;
}

void ZrDebug_Free(void *pointer) {
    if (pointer != ZR_NULL) {
        free(pointer);
    }
}
