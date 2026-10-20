#include "debug_internal.h"
#include "breakpoint/debug_breakpoint_condition.h"
#include "breakpoint/debug_breakpoint_logpoint.h"

#include <ctype.h>

static TZrDebugSignal zr_debug_agent_trace_observer(SZrState *state,
                                                    SZrFunction *function,
                                                    const TZrInstruction *programCounter,
                                                    TZrUInt32 instructionOffset,
                                                    TZrUInt32 sourceLine,
                                                    TZrPtr userData);

static TZrBool zr_debug_text_looks_like_path(const TZrChar *text) {
    return (TZrBool)(text != ZR_NULL && (strchr(text, '/') != ZR_NULL || strchr(text, '\\') != ZR_NULL));
}

static TZrSize zr_debug_format_truncation_marker(TZrChar *buffer, TZrSize bufferSize, TZrSize omittedCount) {
    int written;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return 0;
    }

    written = snprintf(buffer, bufferSize, "...[+%llu]", (unsigned long long)omittedCount);
    if (written < 0 || (TZrSize)written >= bufferSize) {
        buffer[0] = '\0';
        return 0;
    }

    return (TZrSize)written;
}

void zr_debug_copy_text(TZrChar *buffer, TZrSize bufferSize, const TZrChar *text) {
    TZrSize textLength;
    TZrChar marker[32];
    TZrSize markerLength;
    TZrSize visibleLength;
    TZrSize omittedCount;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }
    if (text == ZR_NULL) {
        buffer[0] = '\0';
        return;
    }

    textLength = strlen(text);
    if (textLength < bufferSize) {
        memcpy(buffer, text, textLength + 1u);
        return;
    }

    markerLength = zr_debug_format_truncation_marker(marker, sizeof(marker), 1u);
    if (markerLength == 0 || markerLength + 1u >= bufferSize) {
        memcpy(buffer, text, bufferSize - 1u);
        buffer[bufferSize - 1u] = '\0';
        return;
    }

    visibleLength = bufferSize - markerLength - 1u;
    omittedCount = textLength - visibleLength;
    markerLength = zr_debug_format_truncation_marker(marker, sizeof(marker), omittedCount);
    while (markerLength > 0 && markerLength + visibleLength + 1u > bufferSize && visibleLength > 0) {
        visibleLength--;
        omittedCount = textLength - visibleLength;
        markerLength = zr_debug_format_truncation_marker(marker, sizeof(marker), omittedCount);
    }

    if (markerLength == 0 || markerLength + visibleLength + 1u > bufferSize) {
        memcpy(buffer, text, bufferSize - 1u);
        buffer[bufferSize - 1u] = '\0';
        return;
    }

    // 可容纳标记时，路径保留末尾文件名，普通文本保留开头；两者都标出省略的字节数。
    // TODO: 快照是否要求合法 UTF-8 尚需确认；当前按 strlen 字节截断，下一步补长多字节协议字段测试。
    if (zr_debug_text_looks_like_path(text)) {
        memcpy(buffer, marker, markerLength);
        memcpy(buffer + markerLength, text + textLength - visibleLength, visibleLength);
        buffer[markerLength + visibleLength] = '\0';
    } else {
        memcpy(buffer, text, visibleLength);
        memcpy(buffer + visibleLength, marker, markerLength);
        buffer[visibleLength + markerLength] = '\0';
    }
}

const TZrChar *zr_debug_string_native(SZrString *value) {
    return value != ZR_NULL ? ZrCore_String_GetNativeString(value) : ZR_NULL;
}

const TZrChar *zr_debug_function_name(SZrFunction *function) {
    const TZrChar *name = function != ZR_NULL ? zr_debug_string_native(function->functionName) : ZR_NULL;
    return name != ZR_NULL && name[0] != '\0' ? name : "<anonymous>";
}

const TZrChar *zr_debug_function_source(SZrFunction *function) {
    const TZrChar *source = function != ZR_NULL ? zr_debug_string_native(function->sourceCodeList) : ZR_NULL;
    return source != ZR_NULL ? source : "";
}

TZrUInt32 zr_debug_instruction_offset(SZrCallInfo *callInfo, SZrFunction *function) {
    if (callInfo == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL ||
        callInfo->context.context.programCounter == ZR_NULL ||
        callInfo->context.context.programCounter < function->instructionsList) {
        return 0;
    }

    return (TZrUInt32)(callInfo->context.context.programCounter - function->instructionsList);
}

TZrUInt32 zr_debug_frame_depth(SZrState *state) {
    TZrUInt32 depth = 0;
    SZrCallInfo *callInfo;

    if (state == ZR_NULL) {
        return 0;
    }

    callInfo = state->callInfoList;
    while (callInfo != ZR_NULL) {
        if (ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo) != ZR_NULL) {
            depth++;
        }
        callInfo = callInfo->previous;
    }

    return depth;
}

const TZrChar *zr_debug_stop_reason_name(EZrDebugStopReason reason) {
    switch (reason) {
        case ZR_DEBUG_STOP_REASON_ENTRY:
            return "entry";
        case ZR_DEBUG_STOP_REASON_BREAKPOINT:
            return "breakpoint";
        case ZR_DEBUG_STOP_REASON_PAUSE:
            return "pause";
        case ZR_DEBUG_STOP_REASON_STEP:
            return "step";
        case ZR_DEBUG_STOP_REASON_EXCEPTION:
            return "exception";
        case ZR_DEBUG_STOP_REASON_TERMINATED:
            return "terminated";
        case ZR_DEBUG_STOP_REASON_DATA_BREAKPOINT:
            return "dataBreakpoint";
        case ZR_DEBUG_STOP_REASON_NONE:
        default:
            return "none";
    }
}

const TZrChar *zr_debug_exception_filter_name(EZrDebugExceptionFilter filter) {
    switch (filter) {
        case ZR_DEBUG_EXCEPTION_FILTER_CAUGHT:
            return "caught";
        case ZR_DEBUG_EXCEPTION_FILTER_UNCAUGHT:
            return "uncaught";
        case ZR_DEBUG_EXCEPTION_FILTER_NONE:
        default:
            return "none";
    }
}

const TZrChar *zr_debug_scope_kind_name(EZrDebugScopeKind kind) {
    switch (kind) {
        case ZR_DEBUG_SCOPE_KIND_ARGUMENTS:
            return "Arguments";
        case ZR_DEBUG_SCOPE_KIND_LOCALS:
            return "Locals";
        case ZR_DEBUG_SCOPE_KIND_CLOSURES:
            return "Closures";
        case ZR_DEBUG_SCOPE_KIND_GLOBALS:
            return "Globals";
        case ZR_DEBUG_SCOPE_KIND_PROTOTYPE:
            return "Prototype";
        case ZR_DEBUG_SCOPE_KIND_STATICS:
            return "Statics";
        case ZR_DEBUG_SCOPE_KIND_EXCEPTION:
            return "Exception";
        default:
            return "Unknown";
    }
}

const TZrChar *zr_debug_value_type_name(EZrValueType type) {
    switch (type) {
        case ZR_VALUE_TYPE_NULL:
            return "null";
        case ZR_VALUE_TYPE_BOOL:
            return "bool";
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            return "int";
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            return "uint";
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            return "float";
        case ZR_VALUE_TYPE_STRING:
            return "string";
        case ZR_VALUE_TYPE_OBJECT:
            return "object";
        case ZR_VALUE_TYPE_ARRAY:
            return "array";
        case ZR_VALUE_TYPE_FUNCTION:
            return "function";
        case ZR_VALUE_TYPE_CLOSURE:
            return "closure";
        case ZR_VALUE_TYPE_NATIVE_POINTER:
            return "native_pointer";
        case ZR_VALUE_TYPE_NATIVE_DATA:
            return "native_data";
        default:
            return "value";
    }
}

TZrUInt32 zr_debug_scope_id(TZrUInt32 frameId, EZrDebugScopeKind kind) {
    // BUG: 第 100 层及更深的 VM 帧会生成 >= 1000 的 scopeId，与动态变量句柄区间重叠。
    // ReadScopes 会返回该 ID，但 ReadVariables 把无匹配 handle 的高位 ID 拒绝，导致变量请求失败。
    // Core 的 GetStack 按链遍历帧且无 99 层上限；可在 100 层递归停点请求 scopes/variables 验证。
    return frameId * 10u + (TZrUInt32)kind;
}

ZrDebugThreadEntry *zr_debug_agent_find_thread_by_id(ZrDebugAgent *agent, TZrUInt32 threadId) {
    TZrSize index;

    if (agent == ZR_NULL || threadId == 0) {
        return ZR_NULL;
    }

    for (index = 0; index < agent->threadCount; index++) {
        if (agent->threads[index].thread_id == threadId) {
            return &agent->threads[index];
        }
    }

    return ZR_NULL;
}

ZrDebugThreadEntry *zr_debug_agent_find_thread_by_state(ZrDebugAgent *agent, SZrState *state) {
    TZrSize index;

    if (agent == ZR_NULL || state == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < agent->threadCount; index++) {
        if (agent->threads[index].state == state) {
            return &agent->threads[index];
        }
    }

    return ZR_NULL;
}

TZrUInt32 zr_debug_agent_thread_id_for_state(ZrDebugAgent *agent, SZrState *state) {
    ZrDebugThreadEntry *entry = zr_debug_agent_find_thread_by_state(agent, state);
    return entry != ZR_NULL ? entry->thread_id : 0u;
}

TZrUInt32 zr_debug_agent_effective_thread_id(ZrDebugAgent *agent, TZrUInt32 requestedThreadId) {
    if (requestedThreadId != 0) {
        return requestedThreadId;
    }
    if (agent == ZR_NULL) {
        return ZR_DEBUG_MAIN_THREAD_ID;
    }
    if (agent->lastStopEvent.thread_id != 0) {
        return agent->lastStopEvent.thread_id;
    }
    if (agent->currentThreadId != 0) {
        return agent->currentThreadId;
    }
    return ZR_DEBUG_MAIN_THREAD_ID;
}

TZrBool zr_debug_agent_register_thread_state(ZrDebugAgent *agent,
                                             SZrState *state,
                                             const TZrChar *name,
                                             TZrUInt32 *outThreadId) {
    ZrDebugThreadEntry *entry;
    ZrDebugThreadEntry *expandedThreads;
    TZrSize newCapacity;

    if (outThreadId != ZR_NULL) {
        *outThreadId = 0;
    }
    if (agent == ZR_NULL || state == ZR_NULL) {
        return ZR_FALSE;
    }

    entry = zr_debug_agent_find_thread_by_state(agent, state);
    if (entry != ZR_NULL) {
        if (outThreadId != ZR_NULL) {
            *outThreadId = entry->thread_id;
        }
        return ZR_TRUE;
    }

    if (agent->threadCount >= agent->threadCapacity) {
        newCapacity = agent->threadCapacity > 0 ? agent->threadCapacity * 2u : 4u;
        expandedThreads = (ZrDebugThreadEntry *)realloc(agent->threads, sizeof(*expandedThreads) * newCapacity);
        if (expandedThreads == ZR_NULL) {
            return ZR_FALSE;
        }
        agent->threads = expandedThreads;
        agent->threadCapacity = newCapacity;
    }

    entry = &agent->threads[agent->threadCount++];
    memset(entry, 0, sizeof(*entry));
    if (agent->nextThreadId == 0) {
        agent->nextThreadId = ZR_DEBUG_MAIN_THREAD_ID;
    }
    entry->thread_id = agent->nextThreadId++;
    entry->state = state;
    entry->previous_debug_hook_signal = state->debugHookSignal;
    if (name != ZR_NULL && name[0] != '\0') {
        zr_debug_copy_text(entry->name, sizeof(entry->name), name);
    } else if (entry->thread_id == ZR_DEBUG_MAIN_THREAD_ID) {
        zr_debug_copy_text(entry->name, sizeof(entry->name), "main");
    } else {
        snprintf(entry->name, sizeof(entry->name), "thread %u", entry->thread_id);
    }

    // 核心 VM 的 trace observer 是每个 state 独立注册的；记录旧掩码供 AgentStop 恢复。
    state->debugHookSignal |= ZR_DEBUG_HOOK_MASK_LINE;
    ZrCore_Debug_SetTraceObserver(state, zr_debug_agent_trace_observer, agent);
    if (agent->currentThreadId == 0) {
        agent->currentThreadId = entry->thread_id;
        agent->state = state;
    }
    if (outThreadId != ZR_NULL) {
        *outThreadId = entry->thread_id;
    }
    return ZR_TRUE;
}

SZrState *zr_debug_agent_begin_thread_access(ZrDebugAgent *agent,
                                             TZrUInt32 requestedThreadId,
                                             TZrUInt32 *outResolvedThreadId,
                                             SZrState **outPreviousState,
                                             TZrUInt32 *outPreviousThreadId) {
    TZrUInt32 threadId;
    ZrDebugThreadEntry *entry;

    if (outResolvedThreadId != ZR_NULL) {
        *outResolvedThreadId = 0;
    }
    if (outPreviousState != ZR_NULL) {
        *outPreviousState = agent != ZR_NULL ? agent->state : ZR_NULL;
    }
    if (outPreviousThreadId != ZR_NULL) {
        *outPreviousThreadId = agent != ZR_NULL ? agent->currentThreadId : 0u;
    }
    if (agent == ZR_NULL) {
        return ZR_NULL;
    }

    threadId = zr_debug_agent_effective_thread_id(agent, requestedThreadId);
    entry = zr_debug_agent_find_thread_by_id(agent, threadId);
    if (entry == ZR_NULL || entry->state == ZR_NULL) {
        return ZR_NULL;
    }

    // 协议读栈和求值沿用 agent->state；仅在当前请求期间切换，调用方须成对恢复。
    agent->state = entry->state;
    agent->currentThreadId = entry->thread_id;
    if (outResolvedThreadId != ZR_NULL) {
        *outResolvedThreadId = entry->thread_id;
    }
    return entry->state;
}

void zr_debug_agent_end_thread_access(ZrDebugAgent *agent, SZrState *previousState, TZrUInt32 previousThreadId) {
    if (agent == ZR_NULL) {
        return;
    }

    agent->state = previousState;
    agent->currentThreadId = previousThreadId;
}

static void zr_debug_data_breakpoint_copy_value_snapshot(SZrTypeValue *destination, const SZrTypeValue *source) {
    if (destination == ZR_NULL) {
        return;
    }

    ZrCore_Value_ResetAsNull(destination);
    if (source == ZR_NULL) {
        return;
    }

    // 上次值只用于后续比较，不能继承 VM 值的拥有权并参与释放。
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

static TZrBool zr_debug_data_breakpoint_decode_scope_id(TZrUInt32 variablesReference,
                                                        TZrUInt32 *outFrameId,
                                                        EZrDebugScopeKind *outScopeKind) {
    TZrUInt32 frameId;
    EZrDebugScopeKind scopeKind;

    if (outFrameId != ZR_NULL) {
        *outFrameId = 0;
    }
    if (outScopeKind != ZR_NULL) {
        *outScopeKind = ZR_DEBUG_SCOPE_KIND_GLOBALS;
    }
    if (variablesReference == 0 || variablesReference >= ZR_DEBUG_VARIABLE_HANDLE_BASE) {
        return ZR_FALSE;
    }

    frameId = variablesReference / 10u;
    scopeKind = (EZrDebugScopeKind)(variablesReference % 10u);
    if (frameId == 0) {
        return ZR_FALSE;
    }

    switch (scopeKind) {
        case ZR_DEBUG_SCOPE_KIND_ARGUMENTS:
        case ZR_DEBUG_SCOPE_KIND_LOCALS:
        case ZR_DEBUG_SCOPE_KIND_CLOSURES:
            if (outFrameId != ZR_NULL) {
                *outFrameId = frameId;
            }
            if (outScopeKind != ZR_NULL) {
                *outScopeKind = scopeKind;
            }
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool zr_debug_data_breakpoint_local_scope_accepts_slot(EZrDebugScopeKind scopeKind,
                                                                 const SZrFunction *function,
                                                                 TZrUInt32 stackSlot) {
    if (function == ZR_NULL || stackSlot >= function->stackSize) {
        return ZR_FALSE;
    }

    if (scopeKind == ZR_DEBUG_SCOPE_KIND_ARGUMENTS) {
        return (TZrBool)(stackSlot < function->parameterCount);
    }
    if (scopeKind == ZR_DEBUG_SCOPE_KIND_LOCALS) {
        return (TZrBool)(stackSlot >= function->parameterCount);
    }

    return ZR_FALSE;
}

static SZrClosure *zr_debug_data_breakpoint_try_get_vm_closure_from_value(const SZrTypeValue *value) {
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

static TZrBool zr_debug_data_breakpoint_resolve_local(ZrDebugAgent *agent,
                                                      TZrUInt32 frameId,
                                                      EZrDebugScopeKind scopeKind,
                                                      const TZrChar *name,
                                                      TZrInt32 *outLocalIndex,
                                                      TZrUInt32 *outStackSlot,
                                                      SZrTypeValue *outValue) {
    SZrDebugActivation activation;
    SZrFunction *function;
    SZrCallInfo *callInfo;
    TZrUInt32 pc;
    TZrUInt32 localMetadataIndex;
    TZrInt32 activeLocalIndex = 0;

    if (outLocalIndex != ZR_NULL) {
        *outLocalIndex = 0;
    }
    if (outStackSlot != ZR_NULL) {
        *outStackSlot = 0;
    }
    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (agent == ZR_NULL ||
        agent->state == ZR_NULL ||
        frameId == 0 ||
        name == ZR_NULL ||
        name[0] == '\0' ||
        (scopeKind != ZR_DEBUG_SCOPE_KIND_ARGUMENTS && scopeKind != ZR_DEBUG_SCOPE_KIND_LOCALS)) {
        return ZR_FALSE;
    }

    memset(&activation, 0, sizeof(activation));
    if (!ZrCore_Debug_GetStack(agent->state, frameId - 1u, &activation)) {
        return ZR_FALSE;
    }

    function = activation.function;
    callInfo = activation.callInfo;
    if (function == ZR_NULL || callInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    // 调试局部变量序号按当前 PC 的活跃元数据计数，和源码声明序号不同。
    pc = zr_debug_instruction_offset(callInfo, function);
    for (localMetadataIndex = 0u;
         localMetadataIndex < function->localVariableLength &&
         function->localVariableList[localMetadataIndex].offsetActivate <= pc;
         localMetadataIndex++) {
        const SZrFunctionLocalVariable *local = &function->localVariableList[localMetadataIndex];
        TZrNativeString localName;
        SZrTypeValue localValue;

        if (pc < local->offsetActivate || pc >= local->offsetDead || local->name == ZR_NULL) {
            continue;
        }

        activeLocalIndex++;
        if (!zr_debug_data_breakpoint_local_scope_accepts_slot(scopeKind, function, local->stackSlot)) {
            continue;
        }

        ZrCore_Value_ResetAsNull(&localValue);
        localName = ZrCore_Debug_GetLocal(agent->state, &activation, activeLocalIndex, &localValue);
        if (localName == ZR_NULL || strcmp(localName, name) != 0) {
            continue;
        }

        if (outLocalIndex != ZR_NULL) {
            *outLocalIndex = activeLocalIndex;
        }
        if (outStackSlot != ZR_NULL) {
            *outStackSlot = local->stackSlot;
        }
        if (outValue != ZR_NULL) {
            zr_debug_data_breakpoint_copy_value_snapshot(outValue, &localValue);
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool zr_debug_data_breakpoint_resolve_upvalue(ZrDebugAgent *agent,
                                                        TZrUInt32 frameId,
                                                        const TZrChar *name,
                                                        TZrInt32 *outUpvalueIndex,
                                                        SZrTypeValue *outValue) {
    SZrDebugActivation activation;
    SZrFunction *function;
    SZrCallInfo *callInfo;
    SZrTypeValue *closureValue;
    SZrClosure *closure;
    TZrUInt32 upvalueIndex;

    if (outUpvalueIndex != ZR_NULL) {
        *outUpvalueIndex = 0;
    }
    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (agent == ZR_NULL || agent->state == ZR_NULL || frameId == 0 || name == ZR_NULL || name[0] == '\0') {
        return ZR_FALSE;
    }

    memset(&activation, 0, sizeof(activation));
    if (!ZrCore_Debug_GetStack(agent->state, frameId - 1u, &activation)) {
        return ZR_FALSE;
    }

    function = activation.function;
    callInfo = activation.callInfo;
    if (function == ZR_NULL || callInfo == ZR_NULL || function->closureValueLength == 0) {
        return ZR_FALSE;
    }

    closureValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    closure = zr_debug_data_breakpoint_try_get_vm_closure_from_value(closureValue);
    if (closure == ZR_NULL) {
        return ZR_FALSE;
    }

    for (upvalueIndex = 0; upvalueIndex < function->closureValueLength; upvalueIndex++) {
        SZrTypeValue upvalue;
        TZrNativeString upvalueName;

        ZrCore_Value_ResetAsNull(&upvalue);
        upvalueName = ZrCore_Debug_GetUpvalue(agent->state, closure, (TZrInt32)upvalueIndex + 1, &upvalue);
        if (upvalueName == ZR_NULL || strcmp(upvalueName, name) != 0) {
            continue;
        }
        if (ZrCore_Debug_GetUpvalueId(agent->state, closure, (TZrInt32)upvalueIndex + 1) == ZR_NULL) {
            continue;
        }

        if (outUpvalueIndex != ZR_NULL) {
            *outUpvalueIndex = (TZrInt32)upvalueIndex + 1;
        }
        if (outValue != ZR_NULL) {
            zr_debug_data_breakpoint_copy_value_snapshot(outValue, &upvalue);
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool zr_debug_data_breakpoint_parse_id(const TZrChar *dataId, ZrDebugDataBreakpoint *outBreakpoint) {
    unsigned int threadId = 0;
    unsigned int frameId = 0;
    unsigned int scopeKind = 0;
    int valueIndex = 0;
    TZrChar name[ZR_DEBUG_NAME_CAPACITY];

    if (outBreakpoint != ZR_NULL) {
        memset(outBreakpoint, 0, sizeof(*outBreakpoint));
    }
    if (dataId == ZR_NULL || dataId[0] == '\0' || outBreakpoint == ZR_NULL) {
        return ZR_FALSE;
    }

    // dataId 由 dataBreakpointInfo 发给客户端，setDataBreakpoints 再解析并重新定位目标。
    name[0] = '\0';
    if (sscanf(dataId, "local:%u:%u:%u:%d:%127s", &threadId, &frameId, &scopeKind, &valueIndex, name) == 5 &&
        threadId > 0 &&
        frameId > 0 &&
        valueIndex > 0 &&
        (scopeKind == (unsigned int)ZR_DEBUG_SCOPE_KIND_ARGUMENTS ||
         scopeKind == (unsigned int)ZR_DEBUG_SCOPE_KIND_LOCALS) &&
        name[0] != '\0') {
        outBreakpoint->kind = ZR_DEBUG_DATA_BREAKPOINT_KIND_LOCAL;
        outBreakpoint->thread_id = (TZrUInt32)threadId;
        outBreakpoint->frame_id = (TZrUInt32)frameId;
        outBreakpoint->scope_kind = (EZrDebugScopeKind)scopeKind;
        outBreakpoint->value_index = (TZrInt32)valueIndex;
        zr_debug_copy_text(outBreakpoint->name, sizeof(outBreakpoint->name), name);
        zr_debug_copy_text(outBreakpoint->data_id, sizeof(outBreakpoint->data_id), dataId);
        return ZR_TRUE;
    }

    name[0] = '\0';
    if (sscanf(dataId, "upvalue:%u:%u:%d:%127s", &threadId, &frameId, &valueIndex, name) == 4 &&
        threadId > 0 &&
        frameId > 0 &&
        valueIndex > 0 &&
        name[0] != '\0') {
        outBreakpoint->kind = ZR_DEBUG_DATA_BREAKPOINT_KIND_UPVALUE;
        outBreakpoint->thread_id = (TZrUInt32)threadId;
        outBreakpoint->frame_id = (TZrUInt32)frameId;
        outBreakpoint->scope_kind = ZR_DEBUG_SCOPE_KIND_CLOSURES;
        outBreakpoint->value_index = (TZrInt32)valueIndex;
        zr_debug_copy_text(outBreakpoint->name, sizeof(outBreakpoint->name), name);
        zr_debug_copy_text(outBreakpoint->data_id, sizeof(outBreakpoint->data_id), dataId);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

TZrBool zr_debug_agent_data_breakpoint_info(ZrDebugAgent *agent,
                                            TZrUInt32 requestedThreadId,
                                            TZrUInt32 variablesReference,
                                            const TZrChar *name,
                                            TZrChar *outDataId,
                                            TZrSize outDataIdSize,
                                            TZrChar *outDescription,
                                            TZrSize outDescriptionSize) {
    TZrUInt32 frameId = 0;
    EZrDebugScopeKind scopeKind = ZR_DEBUG_SCOPE_KIND_GLOBALS;
    SZrState *previousState = ZR_NULL;
    TZrUInt32 previousThreadId = 0;
    TZrUInt32 resolvedThreadId = 0;
    TZrBool ok = ZR_FALSE;

    zr_debug_copy_text(outDataId, outDataIdSize, "");
    zr_debug_copy_text(outDescription, outDescriptionSize, "");
    if (!zr_debug_data_breakpoint_decode_scope_id(variablesReference, &frameId, &scopeKind) ||
        name == ZR_NULL ||
        name[0] == '\0') {
        return ZR_FALSE;
    }

    if (zr_debug_agent_begin_thread_access(agent,
                                           requestedThreadId,
                                           &resolvedThreadId,
                                           &previousState,
                                           &previousThreadId) == ZR_NULL) {
        return ZR_FALSE;
    }

    if (scopeKind == ZR_DEBUG_SCOPE_KIND_ARGUMENTS || scopeKind == ZR_DEBUG_SCOPE_KIND_LOCALS) {
        TZrInt32 localIndex = 0;
        TZrUInt32 stackSlot = 0;
        if (zr_debug_data_breakpoint_resolve_local(agent, frameId, scopeKind, name, &localIndex, &stackSlot, ZR_NULL)) {
            snprintf(outDataId,
                     outDataIdSize,
                     "local:%u:%u:%u:%d:%s",
                     (unsigned int)resolvedThreadId,
                     (unsigned int)frameId,
                     (unsigned int)scopeKind,
                     (int)localIndex,
                     name);
            if (outDataId != ZR_NULL && outDataIdSize > 0) {
                outDataId[outDataIdSize - 1u] = '\0';
            }
            snprintf(outDescription,
                     outDescriptionSize,
                     "%s %s",
                     scopeKind == ZR_DEBUG_SCOPE_KIND_ARGUMENTS ? "argument" : "local",
                     name);
            if (outDescription != ZR_NULL && outDescriptionSize > 0) {
                outDescription[outDescriptionSize - 1u] = '\0';
            }
            ZR_UNUSED_PARAMETER(stackSlot);
            ok = ZR_TRUE;
        }
    } else if (scopeKind == ZR_DEBUG_SCOPE_KIND_CLOSURES) {
        TZrInt32 upvalueIndex = 0;
        if (zr_debug_data_breakpoint_resolve_upvalue(agent, frameId, name, &upvalueIndex, ZR_NULL)) {
            snprintf(outDataId,
                     outDataIdSize,
                     "upvalue:%u:%u:%d:%s",
                     (unsigned int)resolvedThreadId,
                     (unsigned int)frameId,
                     (int)upvalueIndex,
                     name);
            if (outDataId != ZR_NULL && outDataIdSize > 0) {
                outDataId[outDataIdSize - 1u] = '\0';
            }
            snprintf(outDescription, outDescriptionSize, "upvalue %s", name);
            if (outDescription != ZR_NULL && outDescriptionSize > 0) {
                outDescription[outDescriptionSize - 1u] = '\0';
            }
            ok = ZR_TRUE;
        }
    }

    zr_debug_agent_end_thread_access(agent, previousState, previousThreadId);
    return ok;
}

void zr_debug_agent_clear_data_breakpoints(ZrDebugAgent *agent) {
    if (agent == ZR_NULL) {
        return;
    }

    if (agent->dataBreakpoints != ZR_NULL) {
        free(agent->dataBreakpoints);
    }
    agent->dataBreakpoints = ZR_NULL;
    agent->dataBreakpointCount = 0;
    agent->dataBreakpointCapacity = 0;
}

TZrBool zr_debug_agent_add_data_breakpoint(ZrDebugAgent *agent,
                                           const TZrChar *dataId,
                                           const TZrChar *accessType,
                                           TZrChar *outDescription,
                                           TZrSize outDescriptionSize) {
    ZrDebugDataBreakpoint parsed;
    ZrDebugDataBreakpoint *expandedBreakpoints;
    ZrDebugDataBreakpoint *entry;
    SZrTypeValue currentValue;
    SZrState *previousState = ZR_NULL;
    TZrUInt32 previousThreadId = 0;
    TZrUInt32 resolvedThreadId = 0;
    TZrBool resolved = ZR_FALSE;

    zr_debug_copy_text(outDescription, outDescriptionSize, "");
    if (agent == ZR_NULL ||
        !zr_debug_data_breakpoint_parse_id(dataId, &parsed) ||
        (accessType != ZR_NULL &&
         accessType[0] != '\0' &&
         strcmp(accessType, "write") != 0 &&
         strcmp(accessType, "readWrite") != 0)) {
        return ZR_FALSE;
    }

    if (zr_debug_agent_begin_thread_access(agent,
                                           parsed.thread_id,
                                           &resolvedThreadId,
                                           &previousState,
                                           &previousThreadId) == ZR_NULL ||
        resolvedThreadId != parsed.thread_id) {
        zr_debug_agent_end_thread_access(agent, previousState, previousThreadId);
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(&currentValue);
    if (parsed.kind == ZR_DEBUG_DATA_BREAKPOINT_KIND_LOCAL) {
        resolved = zr_debug_data_breakpoint_resolve_local(agent,
                                                          parsed.frame_id,
                                                          parsed.scope_kind,
                                                          parsed.name,
                                                          &parsed.value_index,
                                                          &parsed.stack_slot,
                                                          &currentValue);
        snprintf(parsed.description,
                 sizeof(parsed.description),
                 "%s %s",
                 parsed.scope_kind == ZR_DEBUG_SCOPE_KIND_ARGUMENTS ? "argument" : "local",
                 parsed.name);
        parsed.description[sizeof(parsed.description) - 1u] = '\0';
    } else if (parsed.kind == ZR_DEBUG_DATA_BREAKPOINT_KIND_UPVALUE) {
        resolved = zr_debug_data_breakpoint_resolve_upvalue(agent,
                                                            parsed.frame_id,
                                                            parsed.name,
                                                            &parsed.value_index,
                                                            &currentValue);
        snprintf(parsed.description, sizeof(parsed.description), "upvalue %s", parsed.name);
        parsed.description[sizeof(parsed.description) - 1u] = '\0';
    }

    zr_debug_agent_end_thread_access(agent, previousState, previousThreadId);
    if (!resolved) {
        return ZR_FALSE;
    }

    if (agent->dataBreakpointCount >= agent->dataBreakpointCapacity) {
        TZrSize newCapacity = agent->dataBreakpointCapacity > 0 ? agent->dataBreakpointCapacity * 2u : 4u;
        expandedBreakpoints = (ZrDebugDataBreakpoint *)realloc(agent->dataBreakpoints,
                                                               sizeof(*expandedBreakpoints) * newCapacity);
        if (expandedBreakpoints == ZR_NULL) {
            return ZR_FALSE;
        }
        agent->dataBreakpoints = expandedBreakpoints;
        agent->dataBreakpointCapacity = newCapacity;
    }

    // 安装时保存基线值，下一次 trace 才把变化报告为写入命中。
    entry = &agent->dataBreakpoints[agent->dataBreakpointCount++];
    *entry = parsed;
    entry->verified = ZR_TRUE;
    entry->has_last_value = ZR_TRUE;
    ZrCore_Value_ResetAsNull(&entry->last_value);
    zr_debug_data_breakpoint_copy_value_snapshot(&entry->last_value, &currentValue);
    zr_debug_copy_text(outDescription, outDescriptionSize, entry->description);
    return ZR_TRUE;
}

ZrDebugDataBreakpoint *zr_debug_agent_check_data_breakpoints(ZrDebugAgent *agent) {
    TZrSize index;

    if (agent == ZR_NULL || agent->state == ZR_NULL || agent->dataBreakpointCount == 0) {
        return ZR_NULL;
    }

    for (index = 0; index < agent->dataBreakpointCount; index++) {
        ZrDebugDataBreakpoint *breakpoint = &agent->dataBreakpoints[index];
        SZrTypeValue currentValue;
        TZrBool resolved = ZR_FALSE;

        if (!breakpoint->verified ||
            breakpoint->thread_id != zr_debug_agent_effective_thread_id(agent, agent->currentThreadId)) {
            continue;
        }

        ZrCore_Value_ResetAsNull(&currentValue);
        if (breakpoint->kind == ZR_DEBUG_DATA_BREAKPOINT_KIND_LOCAL) {
            resolved = zr_debug_data_breakpoint_resolve_local(agent,
                                                              breakpoint->frame_id,
                                                              breakpoint->scope_kind,
                                                              breakpoint->name,
                                                              ZR_NULL,
                                                              ZR_NULL,
                                                              &currentValue);
        } else if (breakpoint->kind == ZR_DEBUG_DATA_BREAKPOINT_KIND_UPVALUE) {
            resolved = zr_debug_data_breakpoint_resolve_upvalue(agent,
                                                                breakpoint->frame_id,
                                                                breakpoint->name,
                                                                ZR_NULL,
                                                                &currentValue);
        }
        // 帧或捕获值暂时不可解析时保留旧基线，避免把失效目标当作一次写入。
        if (!resolved) {
            continue;
        }

        if (!breakpoint->has_last_value) {
            zr_debug_data_breakpoint_copy_value_snapshot(&breakpoint->last_value, &currentValue);
            breakpoint->has_last_value = ZR_TRUE;
            continue;
        }

        if (!ZrCore_Value_CompareDirectly(agent->state, &breakpoint->last_value, &currentValue)) {
            zr_debug_data_breakpoint_copy_value_snapshot(&breakpoint->last_value, &currentValue);
            return breakpoint;
        }
    }

    return ZR_NULL;
}

static void zr_debug_normalize_path_for_compare(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize) {
    TZrSize writeIndex = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (path == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; path[index] != '\0' && writeIndex + 1 < bufferSize; index++) {
        TZrChar current = path[index];
        if (current == '\\') {
            current = '/';
        }
#ifdef ZR_VM_PLATFORM_IS_WIN
        current = (TZrChar)tolower((unsigned char)current);
#endif
        buffer[writeIndex++] = current;
    }

    while (writeIndex > 1 && buffer[writeIndex - 1] == '/') {
        writeIndex--;
    }
    buffer[writeIndex] = '\0';
}

static TZrBool zr_debug_normalized_path_is_module_like(const TZrChar *path) {
    if (path == ZR_NULL || path[0] == '\0') {
        return ZR_FALSE;
    }

    return strchr(path, '/') == ZR_NULL ? ZR_TRUE : ZR_FALSE;
}

static void zr_debug_normalized_path_copy_stem(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize) {
    const TZrChar *nameStart;
    TZrSize length;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (path == ZR_NULL || path[0] == '\0') {
        return;
    }

    nameStart = strrchr(path, '/');
    nameStart = nameStart != ZR_NULL ? nameStart + 1 : path;
    length = strlen(nameStart);
    if (length >= 3 && strcmp(nameStart + length - 3, ".zr") == 0) {
        length -= 3;
    } else if (length >= 4 &&
               (strcmp(nameStart + length - 4, ".zri") == 0 || strcmp(nameStart + length - 4, ".zro") == 0)) {
        length -= 4;
    }
    if (length >= bufferSize) {
        length = bufferSize - 1u;
    }

    memcpy(buffer, nameStart, length);
    buffer[length] = '\0';
}

static TZrBool zr_debug_source_paths_equal(const TZrChar *left, const TZrChar *right) {
    TZrChar normalizedLeft[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar normalizedRight[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar leftStem[ZR_DEBUG_NAME_CAPACITY];
    TZrChar rightStem[ZR_DEBUG_NAME_CAPACITY];

    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_debug_normalize_path_for_compare(left, normalizedLeft, sizeof(normalizedLeft));
    zr_debug_normalize_path_for_compare(right, normalizedRight, sizeof(normalizedRight));
    if (strcmp(normalizedLeft, normalizedRight) == 0) {
        return ZR_TRUE;
    }

    if (!zr_debug_normalized_path_is_module_like(normalizedLeft) &&
        !zr_debug_normalized_path_is_module_like(normalizedRight)) {
        return ZR_FALSE;
    }

    // 编译产物中的模块名可能没有目录与扩展名；只有至少一侧像模块名时才按 stem 匹配。
    zr_debug_normalized_path_copy_stem(normalizedLeft, leftStem, sizeof(leftStem));
    zr_debug_normalized_path_copy_stem(normalizedRight, rightStem, sizeof(rightStem));
    return (TZrBool)(leftStem[0] != '\0' && strcmp(leftStem, rightStem) == 0);
}

static TZrBool zr_debug_breakpoint_matches_module(const ZrDebugAgent *agent, const ZrDebugBreakpoint *breakpoint) {
    if (agent == ZR_NULL || breakpoint == ZR_NULL || breakpoint->module_name[0] == '\0') {
        return ZR_TRUE;
    }

    return strcmp(agent->moduleName, breakpoint->module_name) == 0;
}

static TZrBool zr_debug_breakpoint_matches_function(const ZrDebugBreakpoint *breakpoint, SZrFunction *function) {
    if (breakpoint == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    if (breakpoint->source_file[0] != '\0' &&
        !zr_debug_source_paths_equal(zr_debug_function_source(function), breakpoint->source_file)) {
        return ZR_FALSE;
    }

    if (breakpoint->function_name[0] != '\0' &&
        strcmp(zr_debug_function_name(function), breakpoint->function_name) != 0) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static SZrFunction *zr_debug_function_from_constant(const SZrTypeValue *constant) {
    SZrRawObject *rawObject;

    if (constant == ZR_NULL || constant->value.object == ZR_NULL ||
        (constant->type != ZR_VALUE_TYPE_FUNCTION && constant->type != ZR_VALUE_TYPE_CLOSURE)) {
        return ZR_NULL;
    }

    rawObject = constant->value.object;
    if (rawObject->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
        return ZR_CAST(SZrFunction *, rawObject);
    }

    if (rawObject->type == ZR_RAW_OBJECT_TYPE_CLOSURE && !constant->isNative) {
        SZrClosure *closure = ZR_CAST(SZrClosure *, rawObject);
        return closure != ZR_NULL ? closure->function : ZR_NULL;
    }

    return ZR_NULL;
}

static TZrUInt32 zr_debug_function_line_span(const SZrFunction *function) {
    if (function == ZR_NULL ||
        function->lineInSourceStart == 0 ||
        function->lineInSourceEnd == 0 ||
        function->lineInSourceEnd < function->lineInSourceStart) {
        return UINT32_MAX;
    }

    return function->lineInSourceEnd - function->lineInSourceStart;
}

static TZrBool zr_debug_line_breakpoint_candidate_is_better(const ZrDebugBreakpoint *current,
                                                            const ZrDebugBreakpoint *candidate,
                                                            const SZrFunction *entryFunction) {
    TZrUInt32 currentSpan;
    TZrUInt32 candidateSpan;

    if (candidate == ZR_NULL || !candidate->resolved || candidate->resolved_function == ZR_NULL) {
        return ZR_FALSE;
    }
    if (current == ZR_NULL || !current->resolved || current->resolved_function == ZR_NULL) {
        return ZR_TRUE;
    }

    if (current->resolved_function == entryFunction && candidate->resolved_function != entryFunction) {
        return ZR_TRUE;
    }
    if (current->resolved_function != entryFunction && candidate->resolved_function == entryFunction) {
        return ZR_FALSE;
    }

    currentSpan = zr_debug_function_line_span(current->resolved_function);
    candidateSpan = zr_debug_function_line_span(candidate->resolved_function);
    if (candidateSpan < currentSpan) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool zr_debug_breakpoint_resolve_function_entry(SZrFunction *function,
                                                          TZrUInt32 *outInstructionIndex,
                                                          TZrUInt32 *outLine) {
    TZrUInt32 index;

    if (outInstructionIndex != ZR_NULL) {
        *outInstructionIndex = 0;
    }
    if (outLine != ZR_NULL) {
        *outLine = 0;
    }
    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->executionLocationInfoList != ZR_NULL && function->executionLocationInfoLength > 0) {
        for (index = 0; index < function->executionLocationInfoLength; index++) {
            const SZrFunctionExecutionLocationInfo *location = &function->executionLocationInfoList[index];
            if (location->lineInSource == 0 && index + 1u < function->executionLocationInfoLength) {
                continue;
            }
            if (outInstructionIndex != ZR_NULL) {
                *outInstructionIndex = (TZrUInt32)location->currentInstructionOffset;
            }
            if (outLine != ZR_NULL) {
                *outLine = location->lineInSource != 0 ? location->lineInSource : function->lineInSourceStart;
            }
            return ZR_TRUE;
        }
    }

    if (outLine != ZR_NULL) {
        *outLine = function->lineInSourceStart;
    }
    return function->instructionsLength > 0 ? ZR_TRUE : ZR_FALSE;
}

static void zr_debug_resolve_breakpoint_recursive(ZrDebugBreakpoint *breakpoint, SZrFunction *function) {
    TZrUInt32 index;

    if (breakpoint == ZR_NULL || function == ZR_NULL || breakpoint->resolved) {
        return;
    }

    // 行断点先搜子函数以选更内层的执行位置；函数断点则先考虑当前函数。
    if (breakpoint->kind == ZR_DEBUG_BREAKPOINT_KIND_LINE && function->childFunctionList != ZR_NULL) {
        for (index = 0; index < function->childFunctionLength; index++) {
            zr_debug_resolve_breakpoint_recursive(breakpoint, &function->childFunctionList[index]);
        }
        if (breakpoint->resolved) {
            return;
        }
    }

    if (zr_debug_breakpoint_matches_function(breakpoint, function)) {
        if (breakpoint->kind == ZR_DEBUG_BREAKPOINT_KIND_FUNCTION) {
            TZrUInt32 entryLine = 0;
            if (zr_debug_breakpoint_resolve_function_entry(function,
                                                           &breakpoint->resolved_instruction_index,
                                                           &entryLine)) {
                breakpoint->resolved_function = function;
                breakpoint->resolved = ZR_TRUE;
                if (breakpoint->line == 0) {
                    breakpoint->line = entryLine;
                }
            }
        } else if (breakpoint->line > 0 &&
                   function->executionLocationInfoList != ZR_NULL &&
                   function->executionLocationInfoLength > 0) {
            for (index = 0; index < function->executionLocationInfoLength; index++) {
                if (function->executionLocationInfoList[index].lineInSource == breakpoint->line) {
                    breakpoint->resolved_function = function;
                    breakpoint->resolved_instruction_index =
                            (TZrUInt32)function->executionLocationInfoList[index].currentInstructionOffset;
                    breakpoint->resolved = ZR_TRUE;
                    break;
                }
            }
        }
    }

    if (breakpoint->kind != ZR_DEBUG_BREAKPOINT_KIND_LINE && function->childFunctionList != ZR_NULL) {
        for (index = 0; index < function->childFunctionLength; index++) {
            zr_debug_resolve_breakpoint_recursive(breakpoint, &function->childFunctionList[index]);
        }
    }
}

static void zr_debug_refine_line_breakpoint_with_constant_roots(ZrDebugBreakpoint *breakpoint, SZrFunction *entryFunction) {
    TZrUInt32 constantIndex;

    if (breakpoint == ZR_NULL || entryFunction == ZR_NULL || breakpoint->kind != ZR_DEBUG_BREAKPOINT_KIND_LINE ||
        entryFunction->constantValueList == ZR_NULL || entryFunction->constantValueLength == 0) {
        return;
    }

    // 部分可执行函数存于常量表而非 childFunctionList；以较窄的行跨度选归属函数。
    for (constantIndex = 0; constantIndex < entryFunction->constantValueLength; constantIndex++) {
        SZrFunction *candidateFunction =
                zr_debug_function_from_constant(&entryFunction->constantValueList[constantIndex]);
        ZrDebugBreakpoint candidate;

        if (candidateFunction == ZR_NULL || candidateFunction == entryFunction) {
            continue;
        }

        candidate = *breakpoint;
        candidate.resolved = ZR_FALSE;
        candidate.resolved_function = ZR_NULL;
        candidate.resolved_instruction_index = 0;
        zr_debug_resolve_breakpoint_recursive(&candidate, candidateFunction);
        if (zr_debug_line_breakpoint_candidate_is_better(breakpoint, &candidate, entryFunction)) {
            *breakpoint = candidate;
        }
    }
}

static TZrBool zr_debug_breakpoint_try_resolve_with_function_root(ZrDebugBreakpoint *breakpoint,
                                                                  SZrFunction *rootFunction) {
    ZrDebugBreakpoint candidate;

    if (breakpoint == ZR_NULL || rootFunction == ZR_NULL || breakpoint->resolved) {
        return ZR_FALSE;
    }

    candidate = *breakpoint;
    candidate.resolved = ZR_FALSE;
    candidate.resolved_function = ZR_NULL;
    candidate.resolved_instruction_index = 0;
    zr_debug_resolve_breakpoint_recursive(&candidate, rootFunction);
    zr_debug_refine_line_breakpoint_with_constant_roots(&candidate, rootFunction);
    if (!candidate.resolved) {
        return ZR_FALSE;
    }

    breakpoint->resolved_function = candidate.resolved_function;
    breakpoint->resolved_instruction_index = candidate.resolved_instruction_index;
    breakpoint->resolved = ZR_TRUE;
    breakpoint->line = candidate.line;
    return ZR_TRUE;
}

static void zr_debug_agent_try_resolve_pending_breakpoints_for_function(ZrDebugAgent *agent, SZrFunction *function) {
    TZrSize index;

    if (agent == ZR_NULL || function == ZR_NULL) {
        return;
    }

    for (index = 0; index < agent->breakpointCount; index++) {
        ZrDebugBreakpoint *breakpoint = &agent->breakpoints[index];

        if (breakpoint->resolved || !zr_debug_breakpoint_matches_module(agent, breakpoint)) {
            continue;
        }

        if (zr_debug_breakpoint_try_resolve_with_function_root(breakpoint, function)) {
            zr_debug_agent_emit_breakpoint_resolved(agent, breakpoint);
        }
    }
}

static TZrBool zr_debug_parse_uint_text(const TZrChar *text, TZrUInt32 *outValue) {
    const TZrChar *cursor = text;
    TZrUInt32 value = 0;

    if (outValue != ZR_NULL) {
        *outValue = 0;
    }
    if (cursor == ZR_NULL || *cursor == '\0') {
        return ZR_FALSE;
    }

    while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    if (*cursor == '\0') {
        return ZR_FALSE;
    }

    while (*cursor >= '0' && *cursor <= '9') {
        value = value * 10u + (TZrUInt32)(*cursor - '0');
        cursor++;
    }
    while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    if (*cursor != '\0') {
        return ZR_FALSE;
    }

    if (outValue != ZR_NULL) {
        *outValue = value;
    }
    return ZR_TRUE;
}

static TZrBool zr_debug_hit_condition_satisfied(const TZrChar *expression, TZrUInt32 hitCount) {
    TZrUInt32 threshold = 0;

    if (expression == ZR_NULL || expression[0] == '\0') {
        return ZR_TRUE;
    }

    if (expression[0] == '%' && zr_debug_parse_uint_text(expression + 1, &threshold) && threshold > 0) {
        return (TZrBool)((hitCount % threshold) == 0);
    }
    if (strncmp(expression, ">=", 2) == 0 && zr_debug_parse_uint_text(expression + 2, &threshold)) {
        return (TZrBool)(hitCount >= threshold);
    }
    if (strncmp(expression, "<=", 2) == 0 && zr_debug_parse_uint_text(expression + 2, &threshold)) {
        return (TZrBool)(hitCount <= threshold);
    }
    if (strncmp(expression, "==", 2) == 0 && zr_debug_parse_uint_text(expression + 2, &threshold)) {
        return (TZrBool)(hitCount == threshold);
    }
    if (expression[0] == '>' && zr_debug_parse_uint_text(expression + 1, &threshold)) {
        return (TZrBool)(hitCount > threshold);
    }
    if (expression[0] == '<' && zr_debug_parse_uint_text(expression + 1, &threshold)) {
        return (TZrBool)(hitCount < threshold);
    }
    if (zr_debug_parse_uint_text(expression, &threshold)) {
        return (TZrBool)(hitCount == threshold);
    }

    return ZR_FALSE;
}

static TZrBool zr_debug_breakpoint_condition_satisfied(ZrDebugAgent *agent, const ZrDebugBreakpoint *breakpoint) {
    TZrChar error[256];
    TZrBool satisfied = ZR_FALSE;

    if (agent == ZR_NULL || breakpoint == ZR_NULL || breakpoint->condition[0] == '\0') {
        return ZR_TRUE;
    }

    if (!zr_debug_breakpoint_condition_evaluate(
                agent, breakpoint->condition, &satisfied, error, sizeof(error))) {
        zr_debug_agent_emit_output(agent, "stderr", error);
        zr_debug_agent_emit_output(agent, "stderr", "\n");
        return ZR_FALSE;
    }

    return satisfied;
}

static void zr_debug_breakpoint_emit_logpoint(ZrDebugAgent *agent, const ZrDebugBreakpoint *breakpoint) {
    TZrChar output[ZR_DEBUG_TEXT_CAPACITY];

    if (agent == ZR_NULL || breakpoint == ZR_NULL || breakpoint->log_message[0] == '\0') {
        return;
    }

    if (zr_debug_breakpoint_logpoint_format(
                agent, breakpoint->log_message, output, sizeof(output))) {
        zr_debug_agent_emit_output(agent, "console", output);
    }
}

static TZrBool zr_debug_agent_should_stop_for_breakpoint(ZrDebugAgent *agent,
                                                         SZrFunction *function,
                                                         TZrUInt32 instructionIndex,
                                                         TZrUInt32 sourceLine) {
    TZrSize index;
    TZrBool shouldStop = ZR_FALSE;

    if (agent == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < agent->breakpointCount; index++) {
        ZrDebugBreakpoint *breakpoint = &agent->breakpoints[index];

        if (!breakpoint->resolved ||
            !zr_debug_breakpoint_matches_module(agent, breakpoint)) {
            continue;
        }
        if (breakpoint->resolved_function != function ||
            breakpoint->resolved_instruction_index != instructionIndex) {
            if (breakpoint->kind != ZR_DEBUG_BREAKPOINT_KIND_LINE ||
                breakpoint->resolved_function == function ||
                breakpoint->line == 0 ||
                breakpoint->line != sourceLine ||
                !zr_debug_breakpoint_matches_function(breakpoint, function)) {
                continue;
            }

            breakpoint->resolved_function = function;
            breakpoint->resolved_instruction_index = instructionIndex;
        }

        breakpoint->hit_count++;
        if (!zr_debug_hit_condition_satisfied(breakpoint->hit_condition, breakpoint->hit_count)) {
            continue;
        }
        if (!zr_debug_breakpoint_condition_satisfied(agent, breakpoint)) {
            continue;
        }
        if (breakpoint->log_message[0] != '\0') {
            zr_debug_breakpoint_emit_logpoint(agent, breakpoint);
            continue;
        }

        shouldStop = ZR_TRUE;
    }

    return shouldStop;
}

static TZrBool zr_debug_agent_should_stop_for_step(ZrDebugAgent *agent,
                                                   SZrFunction *function,
                                                   const TZrInstruction *programCounter,
                                                   TZrUInt32 instructionIndex,
                                                   TZrUInt32 sourceLine) {
    TZrUInt32 currentDepth;
    SZrCallInfo *currentCallInfo;
    TZrBool advancedPosition;
    TZrBool tailCalleeAtStepDepth;

    if (agent == ZR_NULL) {
        return ZR_FALSE;
    }

    currentDepth = zr_debug_frame_depth(agent->state);
    currentCallInfo = agent->state != ZR_NULL ? agent->state->callInfoList : ZR_NULL;
    tailCalleeAtStepDepth =
            (TZrBool)(currentDepth == agent->stepDepth &&
                      currentCallInfo != ZR_NULL &&
                      (currentCallInfo != agent->stepCallInfo || function != agent->resumeFunction));
    advancedPosition =
            (TZrBool)(function != agent->resumeFunction ||
                      sourceLine != agent->lastStopEvent.line ||
                      (sourceLine == 0 && instructionIndex != agent->resumeInstruction));
    switch (agent->runMode) {
        case ZR_DEBUG_RUN_MODE_STEP_INTO:
            return advancedPosition;

        case ZR_DEBUG_RUN_MODE_STEP_OVER:
            if (currentDepth > agent->stepDepth || tailCalleeAtStepDepth) {
                return ZR_FALSE;
            }
            return (TZrBool)(advancedPosition && currentDepth <= agent->stepDepth);

        case ZR_DEBUG_RUN_MODE_STEP_OUT:
            if (agent->state != ZR_NULL &&
                agent->state->hasCurrentException &&
                (programCounter == ZR_NULL ||
                 programCounter->instruction.operationCode != ZR_INSTRUCTION_ENUM(CATCH))) {
                return ZR_FALSE;
            }
            return (TZrBool)(currentDepth < agent->stepDepth);

        case ZR_DEBUG_RUN_MODE_RUNNING:
        case ZR_DEBUG_RUN_MODE_PAUSED:
        default:
            return ZR_FALSE;
    }
}

static TZrBool zr_debug_agent_should_defer_breakpoint_for_step(ZrDebugAgent *agent, SZrFunction *function) {
    TZrUInt32 currentDepth;
    SZrCallInfo *currentCallInfo;

    if (agent == ZR_NULL || agent->state == ZR_NULL || agent->runMode != ZR_DEBUG_RUN_MODE_STEP_OVER) {
        return ZR_FALSE;
    }

    currentDepth = zr_debug_frame_depth(agent->state);
    if (currentDepth > agent->stepDepth) {
        return ZR_TRUE;
    }

    currentCallInfo = agent->state->callInfoList;
    return (TZrBool)(currentDepth == agent->stepDepth &&
                     currentCallInfo != ZR_NULL &&
                     (currentCallInfo != agent->stepCallInfo || function != agent->resumeFunction));
}

static TZrBool zr_debug_agent_should_stop_for_caught_exception(ZrDebugAgent *agent, const TZrInstruction *programCounter) {
    if (agent == ZR_NULL || agent->state == ZR_NULL || !agent->exceptionBreakOnCaught || !agent->state->hasCurrentException ||
        programCounter == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(programCounter->instruction.operationCode == ZR_INSTRUCTION_ENUM(CATCH));
}

static TZrDebugSignal zr_debug_agent_trace_observer(SZrState *state,
                                                    SZrFunction *function,
                                                    const TZrInstruction *programCounter,
                                                    TZrUInt32 instructionOffset,
                                                    TZrUInt32 sourceLine,
                                                    TZrPtr userData) {
    ZrDebugAgent *agent = (ZrDebugAgent *)userData;
    EZrDebugStopReason reason = ZR_DEBUG_STOP_REASON_NONE;
    EZrDebugExceptionFilter exceptionFilter = ZR_DEBUG_EXCEPTION_FILTER_NONE;
    ZrDebugDataBreakpoint *dataBreakpoint = ZR_NULL;
    TZrUInt32 threadId;

    if (agent == ZR_NULL || state == ZR_NULL || function == ZR_NULL) {
        return ZR_DEBUG_SIGNAL_NONE;
    }

    threadId = zr_debug_agent_thread_id_for_state(agent, state);
    if (threadId == 0 && !zr_debug_agent_register_thread_state(agent, state, ZR_NULL, &threadId)) {
        return ZR_DEBUG_SIGNAL_NONE;
    }
    agent->state = state;
    agent->currentThreadId = threadId;

    zr_debug_agent_poll_messages(agent, 0, ZR_NULL);
    zr_debug_agent_try_resolve_pending_breakpoints_for_function(agent, function);

    // 停止原因有固定优先级：异常和显式暂停优先于断点，再考虑单步。
    if (zr_debug_bool_load(&agent->exceptionStopPending)) {
        reason = ZR_DEBUG_STOP_REASON_EXCEPTION;
        exceptionFilter = ZR_DEBUG_EXCEPTION_FILTER_UNCAUGHT;
        zr_debug_bool_store(&agent->exceptionStopPending, ZR_FALSE);
    } else if (zr_debug_agent_should_stop_for_caught_exception(agent, programCounter)) {
        reason = ZR_DEBUG_STOP_REASON_EXCEPTION;
        exceptionFilter = ZR_DEBUG_EXCEPTION_FILTER_CAUGHT;
    } else if (zr_debug_bool_load(&agent->pauseRequested)) {
        reason = ZR_DEBUG_STOP_REASON_PAUSE;
        zr_debug_bool_store(&agent->pauseRequested, ZR_FALSE);
    } else if (agent->startStopPending) {
        if (!agent->hasClient && !agent->config.wait_for_client) {
            agent->startStopPending = ZR_FALSE;
            return ZR_DEBUG_SIGNAL_NONE;
        }
        reason = ZR_DEBUG_STOP_REASON_ENTRY;
        agent->startStopPending = ZR_FALSE;
    } else if (!zr_debug_agent_should_defer_breakpoint_for_step(agent, function) &&
               zr_debug_agent_should_stop_for_breakpoint(agent, function, instructionOffset, sourceLine)) {
        reason = ZR_DEBUG_STOP_REASON_BREAKPOINT;
    } else if ((dataBreakpoint = zr_debug_agent_check_data_breakpoints(agent)) != ZR_NULL) {
        reason = ZR_DEBUG_STOP_REASON_DATA_BREAKPOINT;
    } else if (zr_debug_agent_should_stop_for_step(agent, function, programCounter, instructionOffset, sourceLine)) {
        reason = ZR_DEBUG_STOP_REASON_STEP;
    }

    if (reason == ZR_DEBUG_STOP_REASON_NONE) {
        return ZR_DEBUG_SIGNAL_NONE;
    }

    zr_debug_agent_fill_stop_event(agent, reason, exceptionFilter, function, instructionOffset, sourceLine);
    if (dataBreakpoint != ZR_NULL) {
        zr_debug_copy_text(agent->lastStopEvent.data_id,
                           sizeof(agent->lastStopEvent.data_id),
                           dataBreakpoint->data_id);
        zr_debug_copy_text(agent->lastStopEvent.data_description,
                           sizeof(agent->lastStopEvent.data_description),
                           dataBreakpoint->description);
    }
    zr_debug_agent_pause_loop(agent);
    return ZR_DEBUG_SIGNAL_NONE;
}

TZrBool ZrDebug_AgentStart(SZrState *state,
                           SZrFunction *entryFunction,
                           const TZrChar *moduleName,
                           const ZrDebugAgentConfig *config,
                           ZrDebugAgent **outAgent,
                           TZrChar *errorBuffer,
                           TZrSize errorBufferSize) {
    ZrDebugAgent *agent;
    SZrNetworkEndpoint endpoint;
    ZrDebugAgentConfig effectiveConfig;

    if (outAgent != ZR_NULL) {
        *outAgent = ZR_NULL;
    }
    if (state == ZR_NULL || entryFunction == ZR_NULL || outAgent == ZR_NULL) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "debug agent requires state, entryFunction, and outAgent");
        return ZR_FALSE;
    }

    memset(&effectiveConfig, 0, sizeof(effectiveConfig));
    if (config != ZR_NULL) {
        effectiveConfig = *config;
    }
    if (effectiveConfig.address == ZR_NULL || effectiveConfig.address[0] == '\0') {
        effectiveConfig.address = "127.0.0.1:0";
    }

    agent = (ZrDebugAgent *)calloc(1, sizeof(*agent));
    if (agent == ZR_NULL) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "failed to allocate debug agent");
        return ZR_FALSE;
    }
    agent->nextVariableHandleId = ZR_DEBUG_VARIABLE_HANDLE_BASE;
    agent->state = state;
    agent->entryFunction = entryFunction;
    // TestManifest 在 agent 生命周期内解码并由 ReadTestManifest 复制给调用者。
    if (entryFunction->testManifestDataLength > 0U) {
        if (entryFunction->testManifestData == ZR_NULL ||
            entryFunction->testManifestDataLength > (TZrSize)UINT32_MAX ||
            !ZrParser_TestManifest_Decode(state,
                                          entryFunction->testManifestData,
                                          (TZrUInt32)entryFunction->testManifestDataLength,
                                          &agent->testManifest)) {
            zr_debug_copy_text(errorBuffer, errorBufferSize,
                               "debug agent rejected invalid TestManifest metadata");
            ZrDebug_AgentStop(agent);
            return ZR_FALSE;
        }
        agent->hasTestManifest = ZR_TRUE;
    }

    if (!ZrNetwork_ParseEndpoint(effectiveConfig.address, &endpoint, errorBuffer, errorBufferSize) ||
        !ZrNetwork_ListenerOpenLoopback(&endpoint, &agent->listener, errorBuffer, errorBufferSize) ||
        !ZrNetwork_FormatEndpoint(&agent->listener.endpoint, agent->endpoint, sizeof(agent->endpoint))) {
        ZrDebug_AgentStop(agent);
        return ZR_FALSE;
    }

    agent->config = effectiveConfig;
    agent->runMode = ZR_DEBUG_RUN_MODE_RUNNING;
    agent->clientDisconnectContinue = ZR_TRUE;
    agent->waitForClientPending = effectiveConfig.wait_for_client;
    agent->startStopPending = (TZrBool)(effectiveConfig.suspend_on_start || effectiveConfig.wait_for_client);
    agent->exceptionBreakOnCaught = ZR_FALSE;
    agent->exceptionBreakOnUncaught = effectiveConfig.stop_on_uncaught_exception;
    agent->nextThreadId = ZR_DEBUG_MAIN_THREAD_ID;
    zr_debug_copy_text(agent->moduleName, sizeof(agent->moduleName), moduleName != ZR_NULL ? moduleName : "");
    if (!zr_debug_agent_register_thread_state(agent, state, "main", ZR_NULL)) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "failed to register main debug thread");
        ZrDebug_AgentStop(agent);
        return ZR_FALSE;
    }
    *outAgent = agent;
    return ZR_TRUE;
}

void ZrDebug_AgentStop(ZrDebugAgent *agent) {
    TZrSize index;

    if (agent == ZR_NULL) {
        return;
    }

    // 先摘除所有 state 上指向 agent 的回调，避免释放后 VM 再访问 userData。
    for (index = 0; index < agent->threadCount; index++) {
        SZrState *threadState = agent->threads[index].state;
        if (threadState != ZR_NULL && threadState->debugTraceUserData == agent) {
            ZrCore_Debug_SetTraceObserver(threadState, ZR_NULL, ZR_NULL);
            threadState->debugHookSignal = agent->threads[index].previous_debug_hook_signal;
        }
    }

    zr_debug_agent_close_client(agent);
    ZrNetwork_ListenerClose(&agent->listener);
    if (agent->breakpoints != ZR_NULL) {
        free(agent->breakpoints);
        agent->breakpoints = ZR_NULL;
    }
    zr_debug_agent_clear_data_breakpoints(agent);
    zr_debug_variable_handles_clear(agent);
    if (agent->threads != ZR_NULL) {
        free(agent->threads);
        agent->threads = ZR_NULL;
    }
    if (agent->hasTestManifest && agent->state != ZR_NULL) {
        ZrParser_TestManifest_Free(agent->state, &agent->testManifest);
        agent->hasTestManifest = ZR_FALSE;
    }
    free(agent);
}

TZrBool ZrDebug_ReadTestManifest(ZrDebugAgent *agent,
                                 ZrDebugTestEntrySnapshot **outEntries,
                                 TZrSize *outCount) {
    ZrDebugTestEntrySnapshot *entries;

    if (outEntries != ZR_NULL) {
        *outEntries = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0U;
    }
    if (agent == ZR_NULL || outEntries == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!agent->hasTestManifest || agent->testManifest.entryCount == 0U) {
        return ZR_TRUE;
    }

    entries = (ZrDebugTestEntrySnapshot *)calloc(agent->testManifest.entryCount,
                                                  sizeof(*entries));
    if (entries == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0U; index < agent->testManifest.entryCount; index++) {
        const SZrParserTestEntry *source = &agent->testManifest.entries[index];
        entries[index].function_symbol_id = source->functionSymbolId;
        entries[index].function_type_id = source->functionTypeId;
        entries[index].callable_child_index = source->callableChildIndex;
        entries[index].case_count = source->caseCount;
        entries[index].is_async = source->isAsync;
        zr_debug_copy_text(entries[index].module_id,
                           sizeof(entries[index].module_id),
                           source->moduleId != ZR_NULL ? source->moduleId : "");
        zr_debug_copy_text(entries[index].qualified_name,
                           sizeof(entries[index].qualified_name),
                           source->qualifiedName != ZR_NULL ? source->qualifiedName : "");
        zr_debug_copy_text(entries[index].skip_reason,
                           sizeof(entries[index].skip_reason),
                           source->skipReason != ZR_NULL ? source->skipReason : "");
    }
    *outEntries = entries;
    *outCount = agent->testManifest.entryCount;
    return ZR_TRUE;
}

TZrBool ZrDebug_AgentGetEndpoint(ZrDebugAgent *agent, TZrChar *buffer, TZrSize bufferSize) {
    if (agent == ZR_NULL || buffer == ZR_NULL || bufferSize == 0 || agent->endpoint[0] == '\0') {
        return ZR_FALSE;
    }

    zr_debug_copy_text(buffer, bufferSize, agent->endpoint);
    return ZR_TRUE;
}

static void zr_debug_copy_breakpoint_from_existing(ZrDebugBreakpoint *destination, const ZrDebugBreakpoint *source) {
    if (destination == ZR_NULL || source == ZR_NULL) {
        return;
    }

    *destination = *source;
}

static void zr_debug_copy_breakpoint_from_spec(ZrDebugAgent *agent,
                                               ZrDebugBreakpoint *destination,
                                               const ZrDebugBreakpointSpec *spec,
                                               EZrDebugBreakpointKind fallbackKind) {
    if (destination == ZR_NULL || spec == ZR_NULL) {
        return;
    }

    memset(destination, 0, sizeof(*destination));
    destination->kind = spec->kind != 0 ? spec->kind : fallbackKind;
    zr_debug_copy_text(destination->module_name,
                       sizeof(destination->module_name),
                       spec->module_name != ZR_NULL ? spec->module_name : (agent != ZR_NULL ? agent->moduleName : ""));
    zr_debug_copy_text(destination->source_file,
                       sizeof(destination->source_file),
                       spec->source_file != ZR_NULL ? spec->source_file : "");
    zr_debug_copy_text(destination->function_name,
                       sizeof(destination->function_name),
                       spec->function_name != ZR_NULL ? spec->function_name : "");
    zr_debug_copy_text(destination->condition,
                       sizeof(destination->condition),
                       spec->condition != ZR_NULL ? spec->condition : "");
    zr_debug_copy_text(destination->hit_condition,
                       sizeof(destination->hit_condition),
                       spec->hit_condition != ZR_NULL ? spec->hit_condition : "");
    zr_debug_copy_text(destination->log_message,
                       sizeof(destination->log_message),
                       spec->log_message != ZR_NULL ? spec->log_message : "");
    destination->line = spec->line;
}

static void zr_debug_recount_breakpoints(ZrDebugAgent *agent) {
    TZrSize index;

    if (agent == ZR_NULL) {
        return;
    }

    agent->lineBreakpointCount = 0;
    agent->functionBreakpointCount = 0;
    for (index = 0; index < agent->breakpointCount; index++) {
        if (agent->breakpoints[index].kind == ZR_DEBUG_BREAKPOINT_KIND_FUNCTION) {
            agent->functionBreakpointCount++;
        } else {
            agent->lineBreakpointCount++;
        }
    }
}

static TZrBool zr_debug_replace_breakpoints_of_kind(ZrDebugAgent *agent,
                                                    EZrDebugBreakpointKind kind,
                                                    const ZrDebugBreakpointSpec *specs,
                                                    TZrSize count) {
    TZrSize index;
    TZrSize preservedCount = 0;
    TZrSize newCount;
    ZrDebugBreakpoint *breakpoints = ZR_NULL;

    if (agent == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < agent->breakpointCount; index++) {
        if (agent->breakpoints[index].kind != kind) {
            preservedCount++;
        }
    }

    // 先构造完整替代数组，分配失败时旧断点集合仍可用。
    newCount = preservedCount + count;
    if (newCount > 0) {
        breakpoints = (ZrDebugBreakpoint *)calloc(newCount, sizeof(*breakpoints));
        if (breakpoints == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    preservedCount = 0;
    for (index = 0; index < agent->breakpointCount; index++) {
        if (agent->breakpoints[index].kind != kind) {
            zr_debug_copy_breakpoint_from_existing(&breakpoints[preservedCount++], &agent->breakpoints[index]);
        }
    }

    for (index = 0; index < count; index++) {
        zr_debug_copy_breakpoint_from_spec(agent, &breakpoints[preservedCount + index], &specs[index], kind);
        zr_debug_resolve_breakpoint_recursive(&breakpoints[preservedCount + index], agent->entryFunction);
        zr_debug_refine_line_breakpoint_with_constant_roots(&breakpoints[preservedCount + index], agent->entryFunction);
    }

    if (agent->breakpoints != ZR_NULL) {
        free(agent->breakpoints);
    }

    agent->breakpoints = breakpoints;
    agent->breakpointCount = newCount;
    zr_debug_recount_breakpoints(agent);
    return ZR_TRUE;
}

TZrBool ZrDebug_SetBreakpoints(ZrDebugAgent *agent, const ZrDebugBreakpointSpec *specs, TZrSize count) {
    return zr_debug_replace_breakpoints_of_kind(agent, ZR_DEBUG_BREAKPOINT_KIND_LINE, specs, count);
}

TZrBool ZrDebug_SetFunctionBreakpoints(ZrDebugAgent *agent,
                                       const ZrDebugBreakpointSpec *specs,
                                       TZrSize count) {
    return zr_debug_replace_breakpoints_of_kind(agent, ZR_DEBUG_BREAKPOINT_KIND_FUNCTION, specs, count);
}

void ZrDebug_SetExceptionBreakpoints(ZrDebugAgent *agent, TZrBool caught, TZrBool uncaught) {
    if (agent == ZR_NULL) {
        return;
    }

    agent->exceptionBreakOnCaught = caught ? ZR_TRUE : ZR_FALSE;
    agent->exceptionBreakOnUncaught = uncaught ? ZR_TRUE : ZR_FALSE;
}

void ZrDebug_Continue(ZrDebugAgent *agent) {
    if (agent == ZR_NULL) {
        return;
    }

    zr_debug_variable_handles_clear(agent);
    zr_debug_bool_store(&agent->pauseRequested, ZR_FALSE);
    agent->runMode = ZR_DEBUG_RUN_MODE_RUNNING;
}

void ZrDebug_Pause(ZrDebugAgent *agent) {
    if (agent == ZR_NULL) {
        return;
    }

    // 外部线程只提交暂停请求，由执行线程在下一个 trace 安全点进入暂停循环。
    zr_debug_bool_store(&agent->pauseRequested, ZR_TRUE);
}

static void zr_debug_begin_step(ZrDebugAgent *agent, EZrDebugRunMode runMode) {
    SZrCallInfo *callInfo;
    SZrFunction *function;

    if (agent == ZR_NULL || agent->state == ZR_NULL) {
        return;
    }

    callInfo = agent->state->callInfoList;
    function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(agent->state, callInfo);
    // 记录恢复时的帧身份和位置，单步判断才能区分尾调用与真正返回。
    agent->stepCallInfo = callInfo;
    agent->resumeFunction = function;
    agent->resumeInstruction = zr_debug_instruction_offset(callInfo, function);
    agent->stepDepth = zr_debug_frame_depth(agent->state);
    zr_debug_variable_handles_clear(agent);
    zr_debug_bool_store(&agent->pauseRequested, ZR_FALSE);
    agent->runMode = runMode;
}

void ZrDebug_StepInto(ZrDebugAgent *agent) {
    zr_debug_begin_step(agent, ZR_DEBUG_RUN_MODE_STEP_INTO);
}

void ZrDebug_StepOver(ZrDebugAgent *agent) {
    zr_debug_begin_step(agent, ZR_DEBUG_RUN_MODE_STEP_OVER);
}

void ZrDebug_StepOut(ZrDebugAgent *agent) {
    zr_debug_begin_step(agent, ZR_DEBUG_RUN_MODE_STEP_OUT);
}

void ZrDebug_NotifyException(ZrDebugAgent *agent) {
    SZrCallInfo *callInfo;
    SZrFunction *function;
    TZrUInt32 instructionIndex;
    TZrUInt32 sourceLine;
    ZrDebugFrameSnapshot frame;

    if (agent == ZR_NULL || agent->state == ZR_NULL || !agent->exceptionBreakOnUncaught) {
        return;
    }
    if (!agent->hasClient && !agent->config.wait_for_client) {
        return;
    }

    if (zr_debug_exception_read_frame(agent, 1, &frame)) {
        zr_debug_agent_fill_stop_event(agent,
                                       ZR_DEBUG_STOP_REASON_EXCEPTION,
                                       ZR_DEBUG_EXCEPTION_FILTER_UNCAUGHT,
                                       ZR_NULL,
                                       frame.instruction_index,
                                       frame.line);
        zr_debug_copy_text(agent->lastStopEvent.source_file, sizeof(agent->lastStopEvent.source_file), frame.source_file);
        zr_debug_copy_text(agent->lastStopEvent.function_name,
                           sizeof(agent->lastStopEvent.function_name),
                           frame.function_name);
        zr_debug_agent_pause_loop(agent);
        return;
    }

    callInfo = agent->state->callInfoList;
    function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(agent->state, callInfo);
    instructionIndex = zr_debug_instruction_offset(callInfo, function);
    sourceLine = zr_debug_call_info_line(agent, callInfo, function, ZR_TRUE);
    zr_debug_agent_fill_stop_event(agent,
                                   ZR_DEBUG_STOP_REASON_EXCEPTION,
                                   ZR_DEBUG_EXCEPTION_FILTER_UNCAUGHT,
                                   function,
                                   instructionIndex,
                                   sourceLine);
    zr_debug_agent_pause_loop(agent);
}

/**
 * @fn TZrUInt32 zr_debug_instruction_offset(SZrCallInfo *callInfo, SZrFunction *function)
 *
 * ## Summary
 * 把当前借用调用帧的 PC 换算为指定函数的指令偏移。
 *
 * ## Parameters
 * @param[in] callInfo 当前有效调用帧，可为 NULL。
 * @param[in] function 与 PC 对应的函数元数据，可为 NULL。
 *
 * ## Returns
 * @return 无帧、函数、指令起点、PC 或 PC 在起点之前时返回 0；否则返回指针差转 uint32。
 *
 * ## Constraints and lifecycle
 * 有效 PC 必须与函数指令数组具有合法的指针比较/相减关系；这里不检查数组上界，0 也可能是合法首指令。结果不延长任何对象生命周期。
 *
 * ## References
 * - \ref zr_debug_instruction_offset：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:104）。
 * - \ref zr_debug_begin_step：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1873）。
 */
/**
 * @fn TZrUInt32 zr_debug_frame_depth(SZrState *state)
 *
 * ## Summary
 * 统计当前调用链中可取得元数据函数的调试帧。
 *
 * ## Parameters
 * @param[in] state 借用且调用链稳定的 VM 状态；NULL 视为无帧。
 *
 * ## Returns
 * @return 返回可调试帧个数；NULL 返回 0。
 *
 * ## Constraints and lifecycle
 * 遍历 previous 链，忽略不能解析为元数据函数的帧；不检测链环、不持锁、不拥有帧。单步把该逻辑深度与帧身份一并比较。
 *
 * ## References
 * - \ref zr_debug_frame_depth：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:114）。
 * - \ref zr_debug_agent_should_stop_for_step：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1414）。
 */
/**
 * @fn static void zr_debug_normalize_path_for_compare(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 为断点比较生成有限容量的词法路径拼写。
 *
 * ## Parameters
 * @param[in] path 借用的 NUL 路径；NULL 生成空文本。
 * @param[out] buffer 调用方可写输出；NULL 不访问。
 * @param[in] bufferSize 输出容量，含终止 NUL；0 不访问。
 *
 * ## Returns
 * 无返回值；有效正容量输出始终以 NUL 结束。
 *
 * ## Constraints and lifecycle
 * 反斜杠换为斜杠，Windows 分支折小写，并删除长度大于一时的尾斜杠；可能截断。不访问文件系统，不解析 ..、链接或真实物理身份；输入输出不应使用会破坏待读文本的重叠区域。
 *
 * ## References
 * - \ref zr_debug_normalize_path_for_compare：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:923）。
 * - \ref zr_debug_source_paths_equal：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:990）。
 */
/**
 * @fn static TZrBool zr_debug_normalized_path_is_module_like(const TZrChar *path)
 *
 * ## Summary
 * 判定规范化标识是否可作为模块名参与退化匹配。
 *
 * ## Parameters
 * @param[in] path 借用的规范化 NUL 文本，可为 NULL。
 *
 * ## Returns
 * @return 非空且不含斜杠时返回真，否则返回假。
 *
 * ## Constraints and lifecycle
 * 只检查拼写形状，不证明模块存在、唯一或可加载。
 *
 * ## References
 * - \ref zr_debug_normalized_path_is_module_like：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:952）。
 * - \ref zr_debug_source_paths_equal：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:990）。
 */
/**
 * @fn static void zr_debug_normalized_path_copy_stem(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 复制规范化路径的末段并去掉已知源码或二进制后缀。
 *
 * ## Parameters
 * @param[in] path 借用的规范化 NUL 路径；NULL 视为空。
 * @param[out] buffer 可写输出；NULL 不访问。
 * @param[in] bufferSize 含终止 NUL 的容量；0 不访问。
 *
 * ## Returns
 * 无返回值；有效正容量输出为可能截断的 NUL 文本。
 *
 * ## Constraints and lifecycle
 * 只去掉 .zr/.zri/.zro 后缀；不进行额外文件解析或所有权转移。调用方使用独立栈缓冲区，避免不受支持的输入输出重叠。
 *
 * ## References
 * - \ref zr_debug_normalized_path_copy_stem：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:960）。
 * - \ref zr_debug_source_paths_equal：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:990）。
 */
/**
 * @fn static TZrBool zr_debug_source_paths_equal(const TZrChar *left, const TZrChar *right)
 *
 * ## Summary
 * 在完整词法路径比较之外允许模块名对源码 stem 的兼容匹配。
 *
 * ## Parameters
 * @param[in] left 借用的 NUL 标识，可为 NULL。
 * @param[in] right 借用的 NUL 标识，可为 NULL。
 *
 * ## Returns
 * @return NULL 输入返回假；规范化文本相同或允许的非空 stem 相同时返回真。
 *
 * ## Constraints and lifecycle
 * 仅至少一侧没有目录时才退化为 stem 匹配，两个带目录的不同路径不退化。固定缓冲区可能截断，不等同于文件系统同一性或全局唯一模块认证。
 *
 * ## References
 * - \ref zr_debug_source_paths_equal：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:990）。
 * - \ref zr_debug_breakpoint_matches_function：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1025）。
 */
/**
 * @fn static TZrBool zr_debug_breakpoint_matches_module(const ZrDebugAgent *agent, const ZrDebugBreakpoint *breakpoint)
 *
 * ## Summary
 * 判断断点的可选模块约束是否允许当前 Agent。
 *
 * ## Parameters
 * @param[in] agent 借用的 Agent，可为 NULL。
 * @param[in] breakpoint 借用的记录，可为 NULL。
 *
 * ## Returns
 * @return 任一指针为空或记录模块名为空时返回真，否则进行模块名精确比较。
 *
 * ## Constraints and lifecycle
 * 空参数在此是通配语义而非错误报告；外层命中与解析入口负责检查必需对象，不单凭此结果解引用记录。
 *
 * ## References
 * - \ref zr_debug_breakpoint_matches_module：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1017）。
 * - \ref zr_debug_agent_try_resolve_pending_breakpoints_for_function：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1246）。
 */
/**
 * @fn static TZrBool zr_debug_breakpoint_matches_function(const ZrDebugBreakpoint *breakpoint, SZrFunction *function)
 *
 * ## Summary
 * 按可选源码标识和函数名筛选一个候选函数。
 *
 * ## Parameters
 * @param[in] breakpoint 借用的规格记录。
 * @param[in] function 借用的候选函数。
 *
 * ## Returns
 * @return 空指针或非空约束不匹配返回假，全部适用约束匹配返回真。
 *
 * ## Constraints and lifecycle
 * 源码约束使用有限词法/stem 比较；函数名使用精确文本比较。不执行函数、不持有对象，空约束为通配。
 *
 * ## References
 * - \ref zr_debug_breakpoint_matches_function：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1025）。
 * - \ref zr_debug_resolve_breakpoint_recursive：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1141）。
 */
/**
 * @fn static SZrFunction *zr_debug_function_from_constant(const SZrTypeValue *constant)
 *
 * ## Summary
 * 从函数或非 native closure 常量借出解析用函数根。
 *
 * ## Parameters
 * @param[in] constant 借用的类型值，可为 NULL。
 *
 * ## Returns
 * @return 返回借用函数指针；空值、其它类型或不适用 closure 返回 NULL。
 *
 * ## Constraints and lifecycle
 * 按实际 raw type 和 isNative 分支访问，不把任意对象当函数。返回值不增根、不释放常量或 closure，调用期间元数据必须存活。
 *
 * ## References
 * - \ref zr_debug_function_from_constant：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1043）。
 * - \ref zr_debug_refine_line_breakpoint_with_constant_roots：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1192）。
 */
/**
 * @fn static TZrUInt32 zr_debug_function_line_span(const SZrFunction *function)
 *
 * ## Summary
 * 为行断点候选比较计算有效源码跨度。
 *
 * ## Parameters
 * @param[in] function 借用函数，可为 NULL。
 *
 * ## Returns
 * @return 有效非零起止行返回 end-start；无效或逆序范围返回 UINT32_MAX。
 *
 * ## Constraints and lifecycle
 * 该值只是排序启发式，不证明指令覆盖每一源码行；不修改函数。
 *
 * ## References
 * - \ref zr_debug_function_line_span：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1064）。
 * - \ref zr_debug_line_breakpoint_candidate_is_better：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1075）。
 */
/**
 * @fn static TZrBool zr_debug_line_breakpoint_candidate_is_better(const ZrDebugBreakpoint *current,
                                                            const ZrDebugBreakpoint *candidate,
                                                            const SZrFunction *entryFunction)
 *
 * ## Summary
 * 在已解析候选之间优先选择非入口根及较窄源码跨度。
 *
 * ## Parameters
 * @param[in] current 现有候选，可为 NULL 或未解析。
 * @param[in] candidate 本次候选，可为 NULL。
 * @param[in] entryFunction 用于区分入口根的借用身份。
 *
 * ## Returns
 * @return 候选有效且按既定优先级胜出时返回真；无效候选或平局返回假。
 *
 * ## Constraints and lifecycle
 * 先排除未解析候选，再偏好非 entryFunction，最后比较跨度。不会发布或持有候选指针，平局保留先前结果。
 *
 * ## References
 * - \ref zr_debug_line_breakpoint_candidate_is_better：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1075）。
 * - \ref zr_debug_refine_line_breakpoint_with_constant_roots：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1192）。
 */
/**
 * @fn static TZrBool zr_debug_breakpoint_resolve_function_entry(SZrFunction *function,
                                                          TZrUInt32 *outInstructionIndex,
                                                          TZrUInt32 *outLine)
 *
 * ## Summary
 * 从函数源码映射选择函数断点的首个可执行位置。
 *
 * ## Parameters
 * @param[in] function 借用候选函数，可为 NULL。
 * @param[out] outInstructionIndex 可选指令偏移输出，先置零。
 * @param[out] outLine 可选源码行输出，先置零。
 *
 * ## Returns
 * @return 找到适用映射或无映射但 instructionsLength>0 时返回真；空函数或无可用入口返回假。
 *
 * ## Constraints and lifecycle
 * 跳过非末尾的零行映射，行号必要时回退 startLine。无映射分支的零偏移不表示有完整调试信息；函数和映射必须有效，不分配或持有资源。
 *
 * ## References
 * - \ref zr_debug_breakpoint_resolve_function_entry：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1104）。
 * - \ref zr_debug_resolve_breakpoint_recursive：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1141）。
 */
/**
 * @fn static void zr_debug_resolve_breakpoint_recursive(ZrDebugBreakpoint *breakpoint, SZrFunction *function)
 *
 * ## Summary
 * 在一个函数树中按断点类别的优先级解析执行位置。
 *
 * ## Parameters
 * @param[in,out] breakpoint 待解析记录；已解析或 NULL 时不修改。
 * @param[in] function 借用的函数树根，可为 NULL。
 *
 * ## Returns
 * 无返回值；成功把函数、指令、行号和 resolved 发布到记录。
 *
 * ## Constraints and lifecycle
 * 行断点先查子函数，函数断点先查当前函数，首个适用位置生效。要求函数树及映射有效且递归可终止；不登记 GC 根，不搜索任意对象图，也不承诺递归深度限制。
 *
 * ## References
 * - \ref zr_debug_resolve_breakpoint_recursive：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1141）。
 * - \ref zr_debug_breakpoint_try_resolve_with_function_root：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1221）。
 */
/**
 * @fn static void zr_debug_refine_line_breakpoint_with_constant_roots(ZrDebugBreakpoint *breakpoint, SZrFunction *entryFunction)
 *
 * ## Summary
 * 用入口常量表中的函数根补充行断点的归属选择。
 *
 * ## Parameters
 * @param[in,out] breakpoint 行断点记录，其整体候选可能被较优结果替换。
 * @param[in] entryFunction 借用入口函数和其常量表。
 *
 * ## Returns
 * 无返回值；无效参数、非行断点或空常量表时不修改。
 *
 * ## Constraints and lifecycle
 * 逐常量提取函数/非 native closure，在局部记录解析并比较；不证明遍历所有可达函数。被采用的函数指针仍借用原元数据生命周期。
 *
 * ## References
 * - \ref zr_debug_refine_line_breakpoint_with_constant_roots：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1192）。
 * - \ref zr_debug_replace_breakpoints_of_kind：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1784）。
 */
/**
 * @fn static TZrBool zr_debug_breakpoint_try_resolve_with_function_root(ZrDebugBreakpoint *breakpoint,
                                                                  SZrFunction *rootFunction)
 *
 * ## Summary
 * 在局部副本解析延迟断点，成功后发布定位字段。
 *
 * ## Parameters
 * @param[in,out] breakpoint 未解析记录；失败保持该记录。
 * @param[in] rootFunction 本次可见的借用函数根。
 *
 * ## Returns
 * @return 成功解析返回真；空参数、原已解析或仍无位置返回假。
 *
 * ## Constraints and lifecycle
 * 先清局部副本定位，再执行树与常量根解析；只在成功时写回 resolved/function/index/line。局部副本并非整个 Agent 的事务或线程同步保证。
 *
 * ## References
 * - \ref zr_debug_breakpoint_try_resolve_with_function_root：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1221）。
 * - \ref zr_debug_agent_try_resolve_pending_breakpoints_for_function：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1246）。
 */
/**
 * @fn static void zr_debug_agent_try_resolve_pending_breakpoints_for_function(ZrDebugAgent *agent, SZrFunction *function)
 *
 * ## Summary
 * 在 trace 看到函数时补解仍待定且模块允许的断点。
 *
 * ## Parameters
 * @param[in,out] agent 拥有断点数组和事件通道的 Agent。
 * @param[in] function 本次 trace 的借用函数根。
 *
 * ## Returns
 * 无返回值；空参数不处理。
 *
 * ## Constraints and lifecycle
 * 只处理尚未解析的记录；成功定位后发送 resolved 事件。事件物化或发送并不与定位提交构成事务，不把输出失败解释为回滚，也不转移函数所有权。
 *
 * ## References
 * - \ref zr_debug_agent_try_resolve_pending_breakpoints_for_function：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1246）。
 * - \ref zr_debug_agent_trace_observer：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1492）。
 */
/**
 * @fn static TZrBool zr_debug_parse_uint_text(const TZrChar *text, TZrUInt32 *outValue)
 *
 * ## Summary
 * 解析命中条件中的十进制 uint32 文本并允许两端空格或制表符。
 *
 * ## Parameters
 * @param[in] text 借用的 NUL 文本。
 * @param[out] outValue 可选数值输出，入口先置零。
 *
 * ## Returns
 * @return 有效非空数字文本返回真并写值；空、纯空白或其它残留字符返回假。
 *
 * ## Constraints and lifecycle
 * 乘加使用 uint32 运算，不检测十进制溢出；调用方不能把真返回解释为任意精度或范围验证。无分配。
 *
 * ## References
 * - \ref zr_debug_parse_uint_text：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1266）。
 * - \ref zr_debug_hit_condition_satisfied：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1301）。
 */
/**
 * @fn static TZrBool zr_debug_hit_condition_satisfied(const TZrChar *expression, TZrUInt32 hitCount)
 *
 * ## Summary
 * 把当前 uint32 命中次数与有限语法的阈值条件比较。
 *
 * ## Parameters
 * @param[in] expression 借用条件；NULL 或空表示无过滤。
 * @param[in] hitCount 本次已累加的 uint32 命中次数。
 *
 * ## Returns
 * @return 支持 %N、>=、<=、==、>、< 和裸整数相等；适用比较成立返回真，格式不适用返回假。
 *
 * ## Constraints and lifecycle
 * %N 要求 N>0；运算数沿用 uint32 解析和回绕边界，不是一般表达式求值，运算符前额外空白不被统一消除。
 *
 * ## References
 * - \ref zr_debug_hit_condition_satisfied：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1301）。
 * - \ref zr_debug_agent_should_stop_for_breakpoint：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1364）。
 */
/**
 * @fn static TZrBool zr_debug_breakpoint_condition_satisfied(ZrDebugAgent *agent, const ZrDebugBreakpoint *breakpoint)
 *
 * ## Summary
 * 在匹配位置使用受限形式求值判断条件是否允许命中。
 *
 * ## Parameters
 * @param[in,out] agent 借用 Agent；可能经求值和输出访问 VM/协议状态。
 * @param[in] breakpoint 借用记录。
 *
 * ## Returns
 * @return 空对象或空条件返回真；普通求值失败输出 stderr 后返回假，否则返回求值得到的布尔值。
 *
 * ## Constraints and lifecycle
 * 内部使用独立错误缓冲区；具体能力/转换由 condition evaluator 约束。普通 false 与 Throw/abort 非局部退出不同，此函数不安装恢复点或保证非局部清理。
 *
 * ## References
 * - \ref zr_debug_breakpoint_condition_satisfied：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1333）。
 * - \ref zr_debug_agent_should_stop_for_breakpoint：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1364）。
 */
/**
 * @fn static void zr_debug_breakpoint_emit_logpoint(ZrDebugAgent *agent, const ZrDebugBreakpoint *breakpoint)
 *
 * ## Summary
 * 把命中的日志断点格式化到栈缓冲区并发送 console 输出。
 *
 * ## Parameters
 * @param[in,out] agent 借用 Agent 和输出通道。
 * @param[in] breakpoint 借用且文本终止的记录。
 *
 * ## Returns
 * 无返回值；空对象、空日志或普通格式化失败时不发送。
 *
 * ## Constraints and lifecycle
 * 输出受固定容量截断和格式化器普通失败规则限制；栈缓冲区只在同步发送期间有效，不保证完整文本/换行、网络交付或非局部异常回滚。
 *
 * ## References
 * - \ref zr_debug_breakpoint_emit_logpoint：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1351）。
 * - \ref zr_debug_agent_should_stop_for_breakpoint：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1364）。
 */
/**
 * @fn static TZrBool zr_debug_agent_should_stop_for_breakpoint(ZrDebugAgent *agent,
                                                         SZrFunction *function,
                                                         TZrUInt32 instructionIndex,
                                                         TZrUInt32 sourceLine)
 *
 * ## Summary
 * 处理当前位置的全部适用断点并汇总是否需要停止。
 *
 * ## Parameters
 * @param[in,out] agent 拥有记录、计数和输出通道的 Agent。
 * @param[in] function 当前借用函数。
 * @param[in] instructionIndex 当前函数内指令偏移。
 * @param[in] sourceLine 当前源码映射行。
 *
 * ## Returns
 * @return 至少一个非日志断点通过命中和条件过滤时返回真；空对象或无适用命中返回假。
 *
 * ## Constraints and lifecycle
 * 先匹配位置，再增加 hit_count，再过滤；日志断点输出后继续，不独自停住。行断点允许同源行/函数约束的重绑定。计数可能回绕；条件/日志失败不恢复已经增加的计数。
 *
 * ## References
 * - \ref zr_debug_agent_should_stop_for_breakpoint：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1364）。
 * - \ref zr_debug_agent_trace_observer：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1492）。
 */
/**
 * @fn static TZrBool zr_debug_agent_should_stop_for_step(ZrDebugAgent *agent,
                                                   SZrFunction *function,
                                                   const TZrInstruction *programCounter,
                                                   TZrUInt32 instructionIndex,
                                                   TZrUInt32 sourceLine)
 *
 * ## Summary
 * 把单步起点、帧身份、逻辑深度与当前位置比较以选择下一停点。
 *
 * ## Parameters
 * @param[in] agent 含单步恢复点的借用 Agent。
 * @param[in] function 当前借用函数。
 * @param[in] programCounter 当前借用指令，异常 step-out 用于识别 CATCH。
 * @param[in] instructionIndex 当前指令偏移。
 * @param[in] sourceLine 当前源码行。
 *
 * ## Returns
 * @return 当前模式满足 into/over/out 的停止规则时返回真；空 Agent 或其它模式返回假。
 *
 * ## Constraints and lifecycle
 * into 看位置推进；over 跳过更深帧及同深度换帧尾调用；out 等待深度下降，异常仍存在时避免在非 CATCH 点停下。只作判断，不清句柄或执行 VM。
 *
 * ## References
 * - \ref zr_debug_agent_should_stop_for_step：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1414）。
 * - \ref zr_debug_agent_trace_observer：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1492）。
 */
/**
 * @fn static TZrBool zr_debug_agent_should_defer_breakpoint_for_step(ZrDebugAgent *agent, SZrFunction *function)
 *
 * ## Summary
 * 在 step-over 的被跳过逻辑调用内暂缓普通断点。
 *
 * ## Parameters
 * @param[in] agent 借用 Agent 与稳定调用链。
 * @param[in] function 当前函数身份。
 *
 * ## Returns
 * @return step-over 中更深或同深度但帧/函数身份已变时返回真；缺失状态或其它模式返回假。
 *
 * ## Constraints and lifecycle
 * 与单步停止判断共享起点语义；不删除断点、不改变计数，也不抑制独立异常或暂停请求分支。
 *
 * ## References
 * - \ref zr_debug_agent_should_defer_breakpoint_for_step：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1464）。
 * - \ref zr_debug_agent_trace_observer：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1492）。
 */
/**
 * @fn static TZrBool zr_debug_agent_should_stop_for_caught_exception(ZrDebugAgent *agent, const TZrInstruction *programCounter)
 *
 * ## Summary
 * 判断当前指令是否是已订阅且存在异常的 CATCH 观察点。
 *
 * ## Parameters
 * @param[in] agent 借用 Agent 和其当前状态。
 * @param[in] programCounter 当前借用指令，可为 NULL。
 *
 * ## Returns
 * @return caught 订阅、异常状态和 CATCH 指令条件同时成立返回真，否则返回假。
 *
 * ## Constraints and lifecycle
 * 只检查当前异常状态与 opcode，不执行 catch、不拥有异常对象，不证明所有 Throw 都会到达该观察点。
 *
 * ## References
 * - \ref zr_debug_agent_should_stop_for_caught_exception：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1483）。
 * - \ref zr_debug_agent_trace_observer：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1492）。
 */
/**
 * @fn static TZrDebugSignal zr_debug_agent_trace_observer(SZrState *state,
                                                    SZrFunction *function,
                                                    const TZrInstruction *programCounter,
                                                    TZrUInt32 instructionOffset,
                                                    TZrUInt32 sourceLine,
                                                    TZrPtr userData)
 *
 * ## Summary
 * 在 VM trace 回调中轮询协议、补解断点并按优先级进入暂停循环。
 *
 * ## Parameters
 * @param[in,out] state 本次 trace 的活动 VM 状态。
 * @param[in] function 本次借用函数。
 * @param[in] programCounter 本次借用指令。
 * @param[in] instructionOffset 本次函数内偏移。
 * @param[in] sourceLine 本次源码行。
 * @param[in,out] userData 由注册链借出的 Agent；回调期间须仍存活。
 *
 * ## Returns
 * @return 所有普通返回路径均为 ZR_DEBUG_SIGNAL_NONE；实际暂停通过内部阻塞循环实现。
 *
 * ## Constraints and lifecycle
 * 空对象或线程登记普通失败直接返回。先轮询/补解，再按 uncaught、caught、pause、entry、普通断点、data breakpoint、step 选择原因。无客户端且不等待的启动分支清标志并提前返回。没有外围恢复点或通用回滚，volatile 同步 TODO 仍开放；停止/回调拆卸必须由会话生命周期协调。
 *
 * ## References
 * - \ref zr_debug_agent_trace_observer：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1492）。
 * - \ref zr_debug_agent_register_thread_state：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:286）。
 */
/**
 * @fn static void zr_debug_copy_breakpoint_from_existing(ZrDebugBreakpoint *destination, const ZrDebugBreakpoint *source)
 *
 * ## Summary
 * 在替换集合时保留其它类别的完整断点记录。
 *
 * ## Parameters
 * @param[out] destination 有效可写目标；NULL 不处理。
 * @param[in] source 有效借用源；NULL 不处理。
 *
 * ## Returns
 * 无返回值。
 *
 * ## Constraints and lifecycle
 * 按结构赋值保留计数与解析状态；内部函数指针仍借用，不产生深拷贝或新的 GC 根。
 *
 * ## References
 * - \ref zr_debug_copy_breakpoint_from_existing：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1727）。
 * - \ref zr_debug_replace_breakpoints_of_kind：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1784）。
 */
/**
 * @fn static void zr_debug_copy_breakpoint_from_spec(ZrDebugAgent *agent,
                                               ZrDebugBreakpoint *destination,
                                               const ZrDebugBreakpointSpec *spec,
                                               EZrDebugBreakpointKind fallbackKind)
 *
 * ## Summary
 * 把短期请求文本复制为 Agent 数组中的独立固定容量记录。
 *
 * ## Parameters
 * @param[in] agent 仅借用默认模块名，可为 NULL。
 * @param[out] destination 有效可写目标，入口清零。
 * @param[in] spec 本次有效借用规格。
 * @param[in] fallbackKind spec.kind 为零时使用的入口类别。
 *
 * ## Returns
 * 无返回值；空目标或规格时不处理。
 *
 * ## Constraints and lifecycle
 * 非零 spec.kind 覆盖 fallback；缺失模块可使用 Agent 默认名，其它缺失文本为空。复制可能截断，不保留请求文本指针，解析留给后续步骤。
 *
 * ## References
 * - \ref zr_debug_copy_breakpoint_from_spec：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1735）。
 * - \ref zr_debug_replace_breakpoints_of_kind：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1784）。
 */
/**
 * @fn static void zr_debug_recount_breakpoints(ZrDebugAgent *agent)
 *
 * ## Summary
 * 根据当前记录数组重新统计行和函数断点数量。
 *
 * ## Parameters
 * @param[in,out] agent 拥有有效数组及长度的 Agent；NULL 不处理。
 *
 * ## Returns
 * 无返回值。
 *
 * ## Constraints and lifecycle
 * 先清两个计数，FUNCTION 计为函数，其它 kind 落入行计数；不验证任意枚举、分配或同步数组。
 *
 * ## References
 * - \ref zr_debug_recount_breakpoints：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1766）。
 * - \ref zr_debug_replace_breakpoints_of_kind：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1784）。
 */
/**
 * @fn static TZrBool zr_debug_replace_breakpoints_of_kind(ZrDebugAgent *agent,
                                                    EZrDebugBreakpointKind kind,
                                                    const ZrDebugBreakpointSpec *specs,
                                                    TZrSize count)
 *
 * ## Summary
 * 构造指定类别的新集合，并保留其它类别的记录。
 *
 * ## Parameters
 * @param[in,out] agent 拥有旧数组的有效 Agent。
 * @param[in] kind 本次要整体替换的类别。
 * @param[in] specs count 个有效规格；count=0 时不读取。
 * @param[in] count 新规格数，须与可读数组及可表示的总容量一致。
 *
 * ## Returns
 * @return NULL Agent 或普通 calloc 失败返回假；完整普通路径提交新数组并重计后返回真。
 *
 * ## Constraints and lifecycle
 * 先算保留数量、分配替代数组、复制和解析，然后 free 旧数组并发布。保证仅针对普通分配失败发生在提交前；不是并发原子替换、完整事务或非局部异常清理。调用方须串行化集合访问，尺寸加法没有独立溢出检查。
 *
 * ## References
 * - \ref zr_debug_replace_breakpoints_of_kind：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1784）。
 * - \ref ZrDebug_SetBreakpoints：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1835）。
 */
/**
 * @fn TZrBool ZrDebug_SetBreakpoints(ZrDebugAgent *agent, const ZrDebugBreakpointSpec *specs, TZrSize count)
 *
 * ## Summary
 * 以新规格整体替换行断点类别并保留其它类别。
 *
 * ## Parameters
 * @param[in,out] agent 借用且由会话拥有的 Agent。
 * @param[in] specs count 个有效规格；字符串在调用期间稳定。
 * @param[in] count 规格数；0 用于清空此类别。
 *
 * ## Returns
 * @return NULL Agent 或替代数组普通分配失败返回假；正常提交返回真。
 *
 * ## Constraints and lifecycle
 * 同步复制文本并尽力解析，不要求提交时全部 resolved。保留的是其它 kind，不是其它 source 的行断点；调用方应提供 LINE 或零 kind。只保证普通分配失败前旧集合仍可用，不证明线程安全或全事务。
 *
 * ## References
 * - \ref ZrDebug_SetBreakpoints：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1835）。
 * - \ref zr_debug_agent_process_set_breakpoints：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:553）。
 */
/**
 * @fn TZrBool ZrDebug_SetFunctionBreakpoints(ZrDebugAgent *agent,
                                       const ZrDebugBreakpointSpec *specs,
                                       TZrSize count)
 *
 * ## Summary
 * 以新规格整体替换函数入口断点类别并保留行断点。
 *
 * ## Parameters
 * @param[in,out] agent 借用且由会话拥有的 Agent。
 * @param[in] specs count 个有效规格，字符串在同步调用期间稳定。
 * @param[in] count 规格数；0 清空此类别。
 *
 * ## Returns
 * @return NULL Agent 或替代数组普通分配失败返回假；正常提交返回真。
 *
 * ## Constraints and lifecycle
 * 使用 FUNCTION fallback，但非零 spec.kind 仍由实现采用。文本复制可能截断、位置可延迟解析；不拥有函数、不提供并发原子或非局部回滚保证。
 *
 * ## References
 * - \ref ZrDebug_SetFunctionBreakpoints：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1839）。
 * - \ref zr_debug_agent_process_set_function_breakpoints：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:598）。
 */
/**
 * @fn void ZrDebug_SetExceptionBreakpoints(ZrDebugAgent *agent, TZrBool caught, TZrBool uncaught)
 *
 * ## Summary
 * 独立设置 caught 与 uncaught 两种异常停点订阅。
 *
 * ## Parameters
 * @param[in,out] agent 借用 Agent；NULL 不处理。
 * @param[in] caught 是否在有当前异常的 CATCH 观察点停下。
 * @param[in] uncaught 是否响应未捕获异常通知或待处理标志。
 *
 * ## Returns
 * 无返回值。
 *
 * ## Constraints and lifecycle
 * 输入规范化为布尔值；仅改变订阅，不立即抛出、清理或暂停异常，也不把枚举当位掩码。
 *
 * ## References
 * - \ref ZrDebug_SetExceptionBreakpoints：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1845）。
 * - \ref zr_debug_agent_process_set_exception_breakpoints：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:681）。
 */
/**
 * @fn void ZrDebug_Continue(ZrDebugAgent *agent)
 *
 * ## Summary
 * 清除当前变量句柄、撤销暂停请求并切回连续运行意图。
 *
 * ## Parameters
 * @param[in,out] agent 借用 Agent；NULL 不处理。
 *
 * ## Returns
 * 无返回值。
 *
 * ## Constraints and lifecycle
 * 通常由暂停协议请求调用；函数本身不验证已暂停、不等待 VM 完成，也不保留旧句柄有效性。不等同于线程加入或原子状态提交。
 *
 * ## References
 * - \ref ZrDebug_Continue：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1854）。
 * - \ref zr_debug_agent_process_continue：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:420）。
 */
/**
 * @fn void ZrDebug_Pause(ZrDebugAgent *agent)
 *
 * ## Summary
 * 提交由下一 trace 观察点消费的暂停请求。
 *
 * ## Parameters
 * @param[in,out] agent 借用且回调仍可访问的 Agent；NULL 不处理。
 *
 * ## Returns
 * 无返回值。
 *
 * ## Constraints and lifecycle
 * 只经 volatile/store 辅助函数置请求，不立即挂起 VM、不等待确认；需继续产生 trace 点。外部线程同步仍是既有 TODO，不据网络测试静态入口宣称线程安全。
 *
 * ## References
 * - \ref ZrDebug_Pause：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1864）。
 * - \ref zr_debug_agent_process_pause：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:426）。
 */
/**
 * @fn static void zr_debug_begin_step(ZrDebugAgent *agent, EZrDebugRunMode runMode)
 *
 * ## Summary
 * 保存当前帧和位置作为单步恢复点，并切换运行意图。
 *
 * ## Parameters
 * @param[in,out] agent 借用 Agent 和其当前状态；缺失时不处理。
 * @param[in] runMode 调用者提供的 into/over/out 意图。
 *
 * ## Returns
 * 无返回值。
 *
 * ## Constraints and lifecycle
 * 记录借用帧/函数、指令、逻辑深度，再清句柄、清暂停请求和设置模式；不执行 VM，不延长帧寿命，不提供跨线程整组原子提交。
 *
 * ## References
 * - \ref zr_debug_begin_step：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1873）。
 * - \ref ZrDebug_StepInto：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1893）。
 */
/**
 * @fn void ZrDebug_StepInto(ZrDebugAgent *agent)
 *
 * ## Summary
 * 请求继续到下一可观察位置，允许进入调用函数。
 *
 * ## Parameters
 * @param[in,out] agent 含活动 VM 状态的借用 Agent；缺失时不处理。
 *
 * ## Returns
 * 无返回值。
 *
 * ## Constraints and lifecycle
 * 委托单步初始化，清旧变量句柄并保存起点；实际停止由 trace 的位置推进判断决定，不同步等待且不保证每条机器指令都观察。
 *
 * ## References
 * - \ref ZrDebug_StepInto：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1893）。
 * - \ref zr_debug_agent_process_step：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:431）。
 */
/**
 * @fn void ZrDebug_StepOver(ZrDebugAgent *agent)
 *
 * ## Summary
 * 请求跳过当前逻辑调用并在起点帧后续位置停下。
 *
 * ## Parameters
 * @param[in,out] agent 含活动 VM 状态的借用 Agent；缺失时不处理。
 *
 * ## Returns
 * 无返回值。
 *
 * ## Constraints and lifecycle
 * 委托单步初始化；更深帧与同深度尾调用按帧/函数身份暂缓普通断点，独立异常和暂停仍有优先级。不承诺忽略所有事件或同步完成。
 *
 * ## References
 * - \ref ZrDebug_StepOver：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1897）。
 * - \ref zr_debug_agent_process_step：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:431）。
 */
/**
 * @fn void ZrDebug_StepOut(ZrDebugAgent *agent)
 *
 * ## Summary
 * 请求继续到逻辑调用深度小于单步起点。
 *
 * ## Parameters
 * @param[in,out] agent 含活动 VM 状态的借用 Agent；缺失时不处理。
 *
 * ## Returns
 * 无返回值。
 *
 * ## Constraints and lifecycle
 * 委托单步初始化；异常未处理时需到 CATCH 可观察点，实际停止依赖 trace 和调用链。清旧句柄，不拥有原帧或同步等待返回。
 *
 * ## References
 * - \ref ZrDebug_StepOut：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1901）。
 * - \ref zr_debug_agent_process_step：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:431）。
 */
/**
 * @fn void ZrDebug_NotifyException(ZrDebugAgent *agent)
 *
 * ## Summary
 * 在宿主获知执行失败时按 uncaught 订阅建立异常停点。
 *
 * ## Parameters
 * @param[in,out] agent 借用 Agent 及其活动状态。
 *
 * ## Returns
 * 无返回值；缺失 Agent/状态、未订阅或无客户端且不等待时直接返回。
 *
 * ## Constraints and lifecycle
 * 优先读保存异常帧，再回退当前调用帧，构造快照并进入暂停循环。该通知不执行 Throw、不消费或释放异常；可能阻塞等待协议。CLI 应在函数/状态释放之前调用，不承诺分配/非局部失败清理。
 *
 * ## References
 * - \ref ZrDebug_NotifyException：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1905）。
 * - \ref ZrDebug_NotifyException：配套调用或事件构造入口（zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:1086）。
 */

/**
 * @fn ZrDebugThreadEntry *zr_debug_agent_find_thread_by_id(ZrDebugAgent *agent, TZrUInt32 threadId)
 *
 * ## Summary
 * 在协议请求已经得到线程号后，从 Agent 线程表取得对应 VM 状态条目；begin_thread_access 用它在切换上下文前验证线程是否已登记。
 *
 * ## Parameters
 * @param[in] agent 待查询的存活 Agent；可为 null，此时查询失败。
 * @param[in] threadId 非零协议线程号；零作为无有效线程的输入处理。
 *
 * ## Returns
 * @return 找到时返回 Agent 数组内部的借用条目；Agent 为空、线程号为零或未匹配时返回 null，线程表不变。
 *
 * ## Constraints and lifecycle
 * 不分配、不取得锁，也不暂停 VM。返回指针不可释放；后续注册触发 realloc 或 AgentStop 后失效，调用方不得跨越这些变更缓存条目地址。
 *
 * ## References
 * zr_debug_agent_begin_thread_access 使用条目选择请求状态；zr_debug_agent_register_thread_state 可能移动同一线程数组。
 */

/**
 * @fn ZrDebugThreadEntry *zr_debug_agent_find_thread_by_state(ZrDebugAgent *agent, SZrState *state)
 *
 * ## Summary
 * 以 VM state 指针身份查询现有线程登记，供 trace observer 取协议线程号及注册入口去重；不按操作系统线程号或状态内容合并。
 *
 * ## Parameters
 * @param[in] agent 持有线程表的 Agent；null 返回未找到。
 * @param[in] state 借用的 VM 状态指针；null 返回未找到，不解引用该参数。
 *
 * ## Returns
 * @return 匹配时返回 Agent 数组内部的借用条目，否则返回 null；不改变登记或回调。
 *
 * ## Constraints and lifecycle
 * 结果有效期受线程数组 realloc 和 AgentStop 限制。查找本身不锁定数组；注册、查询和销毁须由调用方按 Agent 会话顺序协调。
 *
 * ## References
 * zr_debug_agent_thread_id_for_state 将结果转换为标量 ID；zr_debug_agent_register_thread_state 用它避免重复安装同一 state。
 */

/**
 * @fn TZrUInt32 zr_debug_agent_thread_id_for_state(ZrDebugAgent *agent, SZrState *state)
 *
 * ## Summary
 * 把 trace observer 收到的 VM 状态转换为本会话协议线程号，使停止事件可以携带线程身份；未登记状态由调用方决定是否注册。
 *
 * ## Parameters
 * @param[in] agent 待查询会话；可为 null。
 * @param[in] state 待匹配的借用 VM 状态；可为 null。
 *
 * ## Returns
 * @return 返回已登记条目的 thread_id；任一空输入或查找失败时返回零。返回的是标量，不延长 state 或 Agent 的生命周期。
 *
 * ## Constraints and lifecycle
 * 只做查找，不创建线程条目、不安装 observer，也不保证非零 ID 在会话外有效。调用期间线程表须稳定。
 *
 * ## References
 * zr_debug_agent_trace_observer 先查询该 ID，必要时调用 zr_debug_agent_register_thread_state；zr_debug_agent_find_thread_by_state 执行实际匹配。
 */

/**
 * @fn TZrUInt32 zr_debug_agent_effective_thread_id(ZrDebugAgent *agent, TZrUInt32 requestedThreadId)
 *
 * ## Summary
 * 统一协议请求省略线程号时的选择规则，供栈、作用域、变量和停止事件构造共用；选择身份与验证登记是两个步骤。
 *
 * ## Parameters
 * @param[in] agent 用于读取上次停止线程和当前线程的会话；null 时使用主线程默认值。
 * @param[in] requestedThreadId 请求提供的协议线程号；非零原样优先返回，零表示需要默认选择。
 *
 * ## Returns
 * @return 依次返回显式非零请求、lastStopEvent.thread_id、currentThreadId 或 ZR_DEBUG_MAIN_THREAD_ID；不验证所选 ID 是否存在于线程表。
 *
 * ## Constraints and lifecycle
 * 不修改 Agent、不分配、不暂停目标线程。需要状态指针的调用方仍须通过 begin_thread_access 或线程表查找检查有效性。
 *
 * ## References
 * zr_debug_agent_begin_thread_access 在选择后校验登记；zr_debug_agent_make_stack_trace_result、zr_debug_agent_make_scopes_result 和 ZrDebug_ReadStack 消费同一默认规则。
 */

/**
 * @fn TZrBool zr_debug_agent_register_thread_state(ZrDebugAgent *agent, SZrState *state, const TZrChar *name, TZrUInt32 *outThreadId)
 *
 * ## Summary
 * 为新观察到的 VM 状态建立协议线程登记，并把该 Agent 安装为 state 的 trace observer；AgentStart 登记主状态，后续 trace 路径可登记其它状态。
 *
 * ## Parameters
 * @param[in,out] agent 存活且由调用方串行维护的会话；持有并可能扩容线程数组。
 * @param[in,out] state 待登记且须保持存活的 VM 状态；首次登记会增加 LINE hook 位并覆写其 observer/userData。
 * @param[in] name 可选名称；非空文本按字节复制到固定数组，空名称按 ID 生成默认名，可截断。
 * @param[out] outThreadId 可选 ID 输出；进入时先清零，成功时写入既有或新分配的协议 ID。
 *
 * ## Returns
 * @return 已登记或新登记成功返回 true；空 Agent/state 或 realloc 返回 null 时返回 false。扩容失败不增加条目数、不改 state hook，既有线程表仍由 Agent 持有。
 *
 * ## Constraints and lifecycle
 * 已登记的 state 直接复用旧条目，不改名、不重复安装回调。新条目保存旧 debugHookSignal，但不保存先前 observer 函数或 userData；AgentStop 仅在 userData 仍属于本 Agent 时卸载并恢复该掩码。条目只借用 state，不建立 OS 线程或 VM 所有权；调用方负责状态存活、observer 归属和同步。ID 递增及容量乘法没有额外回绕/溢出检测，不承诺无限登记。
 *
 * ## References
 * ZrDebug_AgentStart 注册 main；zr_debug_agent_trace_observer 处理尚无 ID 的状态；ZrCore_Debug_SetTraceObserver 写入回调，ZrDebug_AgentStop 执行归属检查后卸载。
 */

/**
 * @fn SZrState *zr_debug_agent_begin_thread_access(ZrDebugAgent *agent, TZrUInt32 requestedThreadId, TZrUInt32 *outResolvedThreadId, SZrState **outPreviousState, TZrUInt32 *outPreviousThreadId)
 *
 * ## Summary
 * 在单个协议读栈、变量或求值请求期间，把 Agent 的当前 state/线程号暂时指向所选登记状态，使既有读取接口可以复用该上下文。
 *
 * ## Parameters
 * @param[in,out] agent 请求使用的存活会话；成功时临时改写当前上下文，null 时失败。
 * @param[in] requestedThreadId 显式线程号，或零以使用 effective_thread_id 的默认顺序。
 * @param[out] outResolvedThreadId 可选输出；先写零，成功后写实际登记 ID。
 * @param[out] outPreviousState 可选输出；在验证前保存旧借用 state，空 Agent 时为 null。需要恢复的调用方应提供独立局部存储。
 * @param[out] outPreviousThreadId 可选输出；在验证前保存旧线程号，空 Agent 时为零。
 *
 * ## Returns
 * @return 成功返回所选登记 state 的借用指针，并切换 Agent 当前上下文；Agent 为空、ID 未登记或条目 state 为空时返回 null，Agent 上下文保持原值。
 *
 * ## Constraints and lifecycle
 * 该操作不暂停线程、不取得锁，也不提供异常清理屏障。成功后调用方须在所有普通退出分支用 end_thread_access 恢复保存值；嵌套访问按逆序结束。非局部退出不会由本辅助函数自动恢复，读取/求值的线程稳定性仍由上层保证。
 *
 * ## References
 * zr_debug_agent_make_stack_trace_result、zr_debug_agent_make_scopes_result、zr_debug_agent_make_variables_result 与 zr_debug_protocol_make_evaluate_result_detailed 使用该配对；zr_debug_agent_end_thread_access 恢复保存值。
 */

/**
 * @fn void zr_debug_agent_end_thread_access(ZrDebugAgent *agent, SZrState *previousState, TZrUInt32 previousThreadId)
 *
 * ## Summary
 * 结束一次临时线程访问，将 Agent 上下文恢复为 begin_thread_access 前保存的值；不恢复 VM 栈、句柄或其它请求副作用。
 *
 * ## Parameters
 * @param[in,out] agent 待恢复的会话；null 为无操作。
 * @param[in] previousState 配对 begin 调用保存的借用状态，可为 null；本函数不验证或释放。
 * @param[in] previousThreadId 同次 begin 保存的线程号；按原值恢复，不重新查找登记。
 *
 * ## Returns
 * 结果通过 agent->state/currentThreadId 的赋值体现；无返回值、无分配，null Agent 不产生副作用。
 *
 * ## Constraints and lifecycle
 * 调用方负责使用正确配对的保存值，并在会话仍存活且访问顺序受控时调用；本函数不是锁释放或异常处理器。恢复后不得继续假定 Agent 指向刚读取的目标线程。
 *
 * ## References
 * zr_debug_agent_begin_thread_access 提供保存值；协议 stackTrace/scopes/variables/evaluate 的普通成功和失败分支调用本函数。
 */

/**
 * @fn TZrBool ZrDebug_AgentStart(SZrState *state, SZrFunction *entryFunction, const TZrChar *moduleName, const ZrDebugAgentConfig *config, ZrDebugAgent **outAgent, TZrChar *errorBuffer, TZrSize errorBufferSize)
 *
 * ## Summary
 * 在入口函数执行前创建调试会话，解码入口 TestManifest、打开 loopback 监听器并登记主 VM 状态；只建立调试设施，不执行入口函数，也不创建执行线程。
 *
 * ## Parameters
 * @param[in,out] state 必需的存活 VM 状态；成功后其 trace observer/hook 由会话使用，宿主保留状态和 global。
 * @param[in] entryFunction 必需的入口函数；元数据在启动时读取，函数指针仍借用到 AgentStop，宿主负责其有效性和 GC 根。
 * @param[in] moduleName 可选显示名称；null 视为空串，启动时复制到 Agent 固定数组，可截断。
 * @param[in] config 可选配置；null 使用零值配置与 127.0.0.1:0。结构按值复制，其中 auth_token 仍借用。
 * @param[out] outAgent 必需的会话输出；进入时清零，成功后移交新 Agent，必须以 AgentStop 释放。
 * @param[out] errorBuffer 可选普通失败诊断缓冲；可为 null，与容量一起交给有界文本复制，不保证成功时清空。
 * @param[in] errorBufferSize 诊断缓冲字节容量；零表示不写，非零且缓冲非空时须提供相应可写空间。
 *
 * ## Returns
 * @return 成功返回 true 并发布 *outAgent；必需参数缺失、C 分配失败、Manifest 不合法或监听器/线程登记失败时普通返回 false。已取得的会话资源在这些失败分支经部分初始化安全的 AgentStop 清理，输出保持 null。
 *
 * ## Constraints and lifecycle
 * state、entryFunction、注册的其它 state 和鉴权令牌在会话使用期间保持有效；配置不替宿主管理 VM/函数根。启动覆写 state observer，不保存先前 observer，只保存其 hook 掩码。调用方须协调 observer 归属，停止执行/请求访问后再 Stop；本接口没有并发或重入保护。原生分配失败走 bool/NULL 路径，不借此承诺任意宿主分配器回调的非局部异常清理。
 *
 * ## References
 * ZrCli_Runtime_RunPreparedProjectCapture 在执行前启动并在执行后停止；ZrParser_TestManifest_Decode 解码副本，ZrNetwork_ListenerOpenLoopback 建立监听，zr_debug_agent_register_thread_state 安装主状态回调，ZrDebug_AgentStop 配对释放。
 */

/**
 * @fn void ZrDebug_AgentStop(ZrDebugAgent *agent)
 *
 * ## Summary
 * 销毁启动成功或启动途中构造的 Agent，先解除仍归属于它的 VM 回调，再关闭网络和释放会话拥有的数组、句柄表及解码清单，防止释放后回调继续使用会话。
 *
 * ## Parameters
 * @param[in,out] agent 由 AgentStart 分配的会话；null 为无操作。非空调用后该指针及其内部借用视图失效，调用方变量不会被自动清零。
 *
 * ## Returns
 * 无返回值；卸载满足归属条件的 observer、恢复对应旧 hook 掩码并释放会话。不会销毁借用 VM 状态、入口函数或鉴权令牌。
 *
 * ## Constraints and lifecycle
 * 调用前须确保 VM 执行、协议请求和临时线程访问已结束，且所有登记 state/global 仍存活；本函数不 join、不等待回调、不提供并发销毁保护。仅当 state->debugTraceUserData 仍等于本 Agent 才卸载并恢复掩码；不恢复注册前的 observer，也不触碰已改由其它 owner 持有的回调。非空会话只能销毁一次。
 *
 * ## References
 * ZrDebug_AgentStart 的普通失败分支调用本函数；CLI 在入口执行结束后调用，test_debug_threads_enumerates_main_thread_and_routes_snapshots 先 join 执行线程再 Stop；zr_debug_agent_close_client、ZrParser_TestManifest_Free 释放所属资源。
 */

/**
 * @fn TZrBool ZrDebug_ReadTestManifest(ZrDebugAgent *agent, ZrDebugTestEntrySnapshot **outEntries, TZrSize *outCount)
 *
 * ## Summary
 * 把启动时已解码的入口测试清单投影为调试客户端可读取的平面快照，供测试发现列出标识、用例数量和显示文本；不执行测试或复制完整参数描述。
 *
 * ## Parameters
 * @param[in] agent 存活会话；读取其解码清单但不改变它。
 * @param[out] outEntries 必需数组输出；进入时先清零，成功非空数组由调用方用 ZrDebug_Free 释放。
 * @param[out] outCount 必需元素数输出；进入时先置零，成功后与数组长度对应。
 *
 * ## Returns
 * @return 无清单或条目数为零时 true 并交付 null/0；复制成功时 true 并交付独立 calloc 数组；空必需输入或分配失败时 false，能够写入的输出保持空值。
 *
 * ## Constraints and lifecycle
 * 快照的标量和固定文本数组均复制，成功结果不借用 Agent 内部字符串，可在 AgentStop 后继续持有至 ZrDebug_Free。文本按字节截断到固定容量；case_count/is_async 是元数据，不表示实际执行结果。读取须与会话销毁协调，无内部同步。
 *
 * ## References
 * ZrDebug_AgentStart 负责解码并持有源清单；test_debug_agent_projects_typed_test_manifest 读取并释放快照；ZrDebugTestEntrySnapshot 定义投影字段，ZrDebug_Free 释放结果。
 */

/**
 * @fn TZrBool ZrDebug_AgentGetEndpoint(ZrDebugAgent *agent, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 把监听器实际绑定端点复制给 CLI 握手输出或测试 TCP 连接代码，避免客户端继续使用配置中的临时端口零；不建立连接。
 *
 * ## Parameters
 * @param[in] agent 已完成启动且仍存活的会话。
 * @param[out] buffer 调用方提供的文本缓冲；非空且容量大于零时才可复制。
 * @param[in] bufferSize 可写字节容量，包含终止零空间。
 *
 * ## Returns
 * @return 空 Agent/缓冲、容量零或会话端点为空时 false，缓冲不改；其它情况 true 并复制一个零终止串。true 仅表示进行了有界复制，不保证端点未截断，容量为一时也可能只得到空串。
 *
 * ## Constraints and lifecycle
 * 不分配、不转移网络资源。完整端点要求调用方提供足够容量并在使用前保持会话/监听器有效；截断按字节发生，不承诺 UTF-8 字符边界。与 Stop 并发读取不受保护。
 *
 * ## References
 * zr_cli_runtime_emit_debug_endpoint 发布握手信息；test_debug_threads 和 test_debug_agent_protocol 的 connect helper 取得该地址；ZrNetwork_FormatEndpoint 在 AgentStart 中生成实际地址。
 */

/**
 * @fn TZrBool zr_debug_text_looks_like_path(const TZrChar *text)
 *
 * ## Summary
 * 为定长文本截断选择保留方向：只把含目录分隔符的文本识别为路径形态。
 *
 * ## Parameters
 * @param[in] text 调用期间可读的 NUL 结尾文本；允许 NULL。
 *
 * ## Returns
 * @return text 非 NULL 且含 / 或反斜杠时为 TRUE；否则为 FALSE。
 *
 * ## Constraints and lifecycle
 * 这是截断启发式，不检查路径存在性、盘符、规范化、编码或安全性；含分隔符的普通文本也会被按路径处理。
 *
 * 不分配、不保存输入；结果仅供 copy_text 决定保留前缀或尾部。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:79
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:14
 */

/**
 * @fn TZrSize zr_debug_format_truncation_marker(TZrChar *buffer, TZrSize bufferSize, TZrSize omittedCount)
 *
 * ## Summary
 * 在调用方提供的缓冲区内生成带省略字节数的截断标记，供 copy_text 计算文本与标记的容量分配。
 *
 * ## Parameters
 * @param[out] buffer 可写字符缓冲区；NULL 表示无法输出。
 * @param[in] bufferSize 缓冲区实际容量，包含终止 NUL 的空间。
 * @param[in] omittedCount 显示在标记中的省略字节数；不是 Unicode 码点数。
 *
 * ## Returns
 * @return 成功时为标记字节数，不含末尾 NUL；缓冲区无效、容量为零、snprintf 失败或标记放不下时为零。
 *
 * ## Constraints and lifecycle
 * 格式固定为 ...[+N]；对非 NULL 且容量非零的格式化失败会写入空串。返回零不承诺写出完整标记。
 *
 * 不分配、不持有 buffer；由 copy_text 使用局部标记缓冲区，可按最新省略量重复计算。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:55
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:64
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:68
 */

/**
 * @fn void zr_debug_copy_text(TZrChar *buffer, TZrSize bufferSize, const TZrChar *text)
 *
 * ## Summary
 * 把借用文本写入定长调试字段；空间不足时尽量附加省略字节标记，并为路径形态保留更有用的尾部。
 *
 * ## Parameters
 * @param[out] buffer 调用期间可写的目标缓冲区；NULL 时不写。
 * @param[in] bufferSize 目标实际字节容量；零时不写，非零输出均保留终止 NUL。
 * @param[in] text 调用期间可读且与目标写入区不重叠的 NUL 结尾文本；NULL 生成空串。
 *
 * ## Returns
 * void；结果通过 buffer 输出，不报告是否截断。
 *
 * ## Constraints and lifecycle
 * 容量足够时复制完整文本；可放下标记时，含 / 或反斜杠的文本采用标记加尾部，其他文本采用前缀加标记。极小缓冲区或标记格式失败退回容量减一的前缀。strlen 和截断量按字节，不能承诺 UTF-8 码点完整；既有 TODO 保持。
 *
 * 不分配、不保存指针；调用者拥有输入与输出。不能借此延长 VM 字符串生命周期，也不提供重叠区域复制保证。
 *
 * @note TODO: 多字节协议字段要求尚待核实；保留既有 UTF-8 字节截断家族，不新增已复现缺陷。
 *
 * ## References
 * - tests/debug/test_debug_truncation.c:12
 * - tests/debug/test_debug_truncation.c:24
 * - tests/debug/test_debug_truncation.c:33
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:78
 */

/**
 * @fn const TZrChar *zr_debug_string_native(SZrString *value)
 *
 * ## Summary
 * 把可选的 VM 字符串转换为调试显示所用的借用 NUL 字节视图。
 *
 * ## Parameters
 * @param[in] value 借用的有效 VM 字符串对象；允许 NULL。
 *
 * ## Returns
 * @return value 为 NULL 时返回 NULL，否则返回 Core 按短串或长串布局取得的字节指针。
 *
 * ## Constraints and lifecycle
 * 不执行编码转换，不以 strlen 代替对象的完整字节长度；对象可含内嵌 NUL，因此以 C 字符串消费的调用者只看到其前缀。
 *
 * 返回指针不归调用者释放；对象释放或移动 GC 后应重新取得，调用者须保持对象有效。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/string.h:218
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:95
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:100
 */

/**
 * @fn const TZrChar *zr_debug_function_name(SZrFunction *function)
 *
 * ## Summary
 * 为函数调试展示提供可读名称，在缺失或空名称时采用匿名函数占位文字。
 *
 * ## Parameters
 * @param[in] function 借用的可选函数对象；允许 NULL，非 NULL 时须在调用期间有效，函数名字段也允许 NULL。
 *
 * ## Returns
 * @return 非空函数名的借用字节视图，或静态字符串 <anonymous>。
 *
 * ## Constraints and lifecycle
 * 仅供展示和既有断点匹配流程消费，不生成稳定函数身份；不验证名称编码或唯一性。
 *
 * 不分配、不复制函数名；VM 名称视图受其字符串生命周期约束，静态占位串不得释放。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1036
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2831
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:248
 */

/**
 * @fn const TZrChar *zr_debug_function_source(SZrFunction *function)
 *
 * ## Summary
 * 为栈帧和断点流程取得函数携带的源码标识文本。
 *
 * ## Parameters
 * @param[in] function 借用的可选函数对象；允许 NULL，非 NULL 时须在调用期间有效，源码字符串字段也允许 NULL。
 *
 * ## Returns
 * @return 源码字段的借用字节视图；function 或源码字符串字段为 NULL 时返回静态空串，已有空字符串保持为空。
 *
 * ## Constraints and lifecycle
 * 源码标识可能是虚拟编译标签，不证明文件存在、绝对路径或与模块名相同；规范化和匹配由上层负责。
 *
 * 不分配或保留对象；VM 字节视图在对象释放或移动后失效，静态空串不得释放。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1031
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2834
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:245
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:334
 */

/**
 * @fn const TZrChar *zr_debug_stop_reason_name(EZrDebugStopReason reason)
 *
 * ## Summary
 * 把 Agent 停止原因映射为当前调试协议使用的固定文本。
 *
 * ## Parameters
 * @param[in] reason 待显示的停止原因枚举。
 *
 * ## Returns
 * @return entry、breakpoint、pause、step、exception、terminated 或 dataBreakpoint；NONE 及未知值返回 none。
 *
 * ## Constraints and lifecycle
 * 只映射枚举，不触发暂停、不决定停止优先级；优先级在 trace observer 中实现。
 *
 * 返回静态只读字符串，无分配、无状态修改，调用者不得释放或写入。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:266
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1518
 */

/**
 * @fn const TZrChar *zr_debug_exception_filter_name(EZrDebugExceptionFilter filter)
 *
 * ## Summary
 * 把停止事件中的异常捕获类别映射为协议字段。
 *
 * ## Parameters
 * @param[in] filter 异常过滤类别枚举。
 *
 * ## Returns
 * @return caught 或 uncaught；NONE 及未知值返回 none。
 *
 * ## Constraints and lifecycle
 * 不判断异常是否已被处理，也不配置异常断点；仅反映上层已选择的类别。
 *
 * 返回静态只读字符串，不分配、不持有状态，调用者不得释放。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:267
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1520
 */

/**
 * @fn const TZrChar *zr_debug_scope_kind_name(EZrDebugScopeKind kind)
 *
 * ## Summary
 * 为作用域列表生成客户端展示名称。
 *
 * ## Parameters
 * @param[in] kind 待显示的作用域类别。
 *
 * ## Returns
 * @return Arguments、Locals、Closures、Globals、Prototype、Statics 或 Exception；未知值为 Unknown。
 *
 * ## Constraints and lifecycle
 * 名称不代表该作用域在当前帧中存在或可监视，实际枚举与读取资格由 ReadScopes 和 data breakpoint 解码各自决定。
 *
 * 返回静态只读字符串，无分配；调用者不得释放或写入。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2901
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2934
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2967
 */

/**
 * @fn const TZrChar *zr_debug_value_type_name(EZrValueType type)
 *
 * ## Summary
 * 为调试预览和错误诊断提供 VM 值类型的简短展示名。
 *
 * ## Parameters
 * @param[in] type 原始 VM 值类型枚举。
 *
 * ## Returns
 * @return 已识别类型返回固定名称；整数和浮点宽度合并为展示类别，未覆盖类型返回 value。
 *
 * ## Constraints and lifecycle
 * 这是展示映射，不提供反射类型名、泛型信息或可逆序列化；不读取实际值或对象。
 *
 * 返回静态只读字符串，无分配、不延长 VM 对象生命周期。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:19
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_child_shape.c:129
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:457
 */

/**
 * @fn TZrUInt32 zr_debug_scope_id(TZrUInt32 frameId, EZrDebugScopeKind kind)
 *
 * ## Summary
 * 把一基帧号与作用域类别编码为旧式 variablesReference，供作用域和协议栈帧输出往返使用。
 *
 * ## Parameters
 * @param[in] frameId 调用者提供的一基帧号；本函数不校验范围。
 * @param[in] kind 编码到末位的作用域枚举；本函数不验证枚举范围。
 *
 * ## Returns
 * @return 按无符号运算得到 frameId * 10 + kind。
 *
 * ## Constraints and lifecycle
 * 没有溢出、动态句柄区间或帧存在性检查。既有 BUG：第 100 层起编码达到 1000，与动态变量句柄分派域重叠；不得将其描述为所有深度均可逆的稳定 ID。
 *
 * 纯算术不分配；ID 不含线程或停点代次，不能脱离相应帧与暂停状态持久使用。
 *
 * @note BUG: 深帧作用域 ID 与动态变量句柄区间重叠，保留既有同族问题。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:227
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2900
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:3024
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:290
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h:36
 */

/**
 * @fn void zr_debug_data_breakpoint_copy_value_snapshot(SZrTypeValue *destination, const SZrTypeValue *source)
 *
 * ## Summary
 * 复制用于下一次 trace 比较的值描述，并将拥有型对象引用降为非拥有借用。
 *
 * ## Parameters
 * @param[out] destination 非拥有用途的可写目标值；允许 NULL，此时不操作；不得与 source 为同一对象。
 * @param[in] source 本次读取的借用值；允许 NULL，此时目标保持 null。
 *
 * ## Returns
 * void；通过 destination 输出浅快照，不返回对象所有权。
 *
 * ## Constraints and lifecycle
 * 先 ResetAsNull 再按值复制；非 GC 值清除 ownership 控制字段，拥有型 GC 值改为 BORROWED 并去掉 weakOwner。已有 ownershipControl 可保留；这不是深拷贝、GC root 或可跨任意对象释放/移动的固定地址。
 *
 * 不申请、不增加拥有计数、不释放旧拥有值；调用者须提供可覆盖的非拥有目标并在有效 VM/trace 生命周期内消费。相关 block 与函数共用同一基线契约。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:403
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:565
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:629
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:865
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:915
 */

/**
 * @fn TZrBool zr_debug_data_breakpoint_decode_scope_id(TZrUInt32 variablesReference, TZrUInt32 *outFrameId, EZrDebugScopeKind *outScopeKind)
 *
 * ## Summary
 * 从旧式作用域引用中解出可用于数据断点的帧号与作用域类别。
 *
 * ## Parameters
 * @param[in] variablesReference 客户端返回的旧式作用域引用。
 * @param[out] outFrameId 可选帧号输出；进入时先写零，允许 NULL。
 * @param[out] outScopeKind 可选类别输出；进入时先写 Globals，允许 NULL。
 *
 * ## Returns
 * @return 只有 1..999 范围内、帧号非零且末位为 Arguments、Locals 或 Closures 时为 TRUE；其余为 FALSE。
 *
 * ## Constraints and lifecycle
 * 不接受动态变量句柄、Globals、Prototype 等类别；这里只验证编码，不检查线程、帧是否仍存在或当前变量是否可读。深帧碰撞仍受 scope_id 既有 BUG 限制。
 *
 * 不分配、不持有引用；失败保留初始化后的可选输出，成功仅写数值。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:706
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:226
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h:36
 */

/**
 * @fn TZrBool zr_debug_data_breakpoint_local_scope_accepts_slot(EZrDebugScopeKind scopeKind, const SZrFunction *function, TZrUInt32 stackSlot)
 *
 * ## Summary
 * 按函数参数数目把待监视栈槽划分到参数或局部变量作用域。
 *
 * ## Parameters
 * @param[in] scopeKind 要求 Arguments 或 Locals 的作用域类别。
 * @param[in] function 借用函数元数据；允许 NULL，NULL 不接受。
 * @param[in] stackSlot 待检查的零基函数栈槽。
 *
 * ## Returns
 * @return 槽小于 stackSize 且满足相应参数边界时为 TRUE；函数为空、槽越界或类别不支持时为 FALSE。
 *
 * ## Constraints and lifecycle
 * Arguments 使用槽小于 parameterCount，Locals 使用槽大于等于 parameterCount；不另行识别 this/self，也不核对当前 PC 活跃性。不能套用 Snapshot 其他非排他 Locals 视图的规则。
 *
 * 只读元数据，不分配或改变槽值；活跃元数据筛选由 resolve_local 在调用前后完成。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:548
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:533
 */

/**
 * @fn SZrClosure *zr_debug_data_breakpoint_try_get_vm_closure_from_value(const SZrTypeValue *value)
 *
 * ## Summary
 * 从当前帧的闭包值中识别能由 Core upvalue API 读取的非原生 VM 闭包。
 *
 * ## Parameters
 * @param[in] value 调用期间有效的借用值描述；允许 NULL。
 *
 * ## Returns
 * @return 值类型为 Closure、值与闭包均非 native 且对象存在时返回借用 SZrClosure 指针；否则返回 NULL。
 *
 * ## Constraints and lifecycle
 * 只检查表示类别与指针，不验证任意悬空地址、闭包与当前函数的完整一致性或捕获项数量；后续 Core API 继续检查具体索引。
 *
 * 不分配、不保活闭包；调用者须在当前 VM 帧有效期间立即使用，不能释放返回值。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:607
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:897
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:958
 */

/**
 * @fn TZrBool zr_debug_data_breakpoint_resolve_local(ZrDebugAgent *agent, TZrUInt32 frameId, EZrDebugScopeKind scopeKind, const TZrChar *name, TZrInt32 *outLocalIndex, TZrUInt32 *outStackSlot, SZrTypeValue *outValue)
 *
 * ## Summary
 * 在当前线程选定帧的活跃变量中按名称解析参数或局部变量，并产生供数据断点比较的借用值。
 *
 * ## Parameters
 * @param[in,out] agent 借用 Agent；读取其当前 state，本函数不负责切换或恢复线程；允许无效输入并返回 FALSE。
 * @param[in] frameId 一基可见 VM 帧号，必须大于零。
 * @param[in] scopeKind 仅允许 Arguments 或 Locals。
 * @param[in] name 非空、NUL 结尾且调用期间有效的变量名。
 * @param[out] outLocalIndex 可选 Core 活跃变量一基序号输出；进入时写零。
 * @param[out] outStackSlot 可选元数据零基栈槽输出；进入时写零。
 * @param[out] outValue 可选非拥有快照输出；进入时重置为 null，不转移 VM 对象所有权。
 *
 * ## Returns
 * @return 活跃元数据、Core 局部值名称和要求的作用域均匹配时为 TRUE；缺帧、无效输入或找不到名称时为 FALSE。
 *
 * ## Constraints and lifecycle
 * 活跃变量序号在作用域过滤前递增，不能用 metadata 下标或 stackSlot 代替；循环依赖元数据按激活偏移排列。Core 可能物化 inline 值，既有 Core 读取失败语义原样传播，不把 TRUE 泛化为无分配或无异常。
 *
 * 不缓存 activation 或名称；调用方在 info/add 的正常返回路径成对切换/恢复线程，trace 中直接使用当前 state。本函数不建立异常捕获或全事务回滚，输出对象仍是借用。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:533
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:553
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:804
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:723
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:820
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:889
 */

/**
 * @fn TZrBool zr_debug_data_breakpoint_resolve_upvalue(ZrDebugAgent *agent, TZrUInt32 frameId, const TZrChar *name, TZrInt32 *outUpvalueIndex, SZrTypeValue *outValue)
 *
 * ## Summary
 * 在当前帧的 VM 闭包中按捕获名解析 upvalue，供数据断点安装与后续比较。
 *
 * ## Parameters
 * @param[in,out] agent 借用 Agent 及其当前 state；本函数不切换线程，允许无效输入返回 FALSE。
 * @param[in] frameId 一基可见 VM 帧号，必须大于零。
 * @param[in] name 非空、NUL 结尾的捕获名称。
 * @param[out] outUpvalueIndex 可选一基 upvalue 索引输出；进入时写零。
 * @param[out] outValue 可选非拥有快照输出；进入时重置为 null。
 *
 * ## Returns
 * @return 找到当前 VM 闭包中同名且具有有效 upvalue ID 的捕获项时为 TRUE；其余为 FALSE。
 *
 * ## Constraints and lifecycle
 * 只接受非原生 VM 闭包；按函数捕获元数据逐项调用 Core，索引是一基且名称重新解析，不证明 dataId 能跨帧更替或重新编译持久定位同一对象。
 *
 * 不持有 closureValue 或捕获对象，不注册 GC root；借用值只在相应 VM 生命周期内使用。Core 调用未被局部异常处理包装，正常失败保持初始化输出。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:607
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:617
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:623
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:897
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:958
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:897
 */

/**
 * @fn TZrBool zr_debug_data_breakpoint_parse_id(const TZrChar *dataId, ZrDebugDataBreakpoint *outBreakpoint)
 *
 * ## Summary
 * 把客户端回传的 local 或 upvalue 文本标识解析为待安装的数据断点描述。
 *
 * ## Parameters
 * @param[in] dataId 调用期间可读的非空 NUL 结尾文本。
 * @param[out] outBreakpoint 必须可写；若非 NULL，解析前先清零整个描述。
 *
 * ## Returns
 * @return sscanf 解析到相应字段数且线程、帧、索引为正，名称非空、local 类别受支持时为 TRUE；其余为 FALSE。
 *
 * ## Constraints and lifecycle
 * 名称采用 %127s，空白截断且有长度上限；没有全串耗尽检查，不是严格完整语法或认证令牌。解析索引只作描述，add 会按当前线程、帧和名称再次解析；复制文本也可按定长规则截断。
 *
 * 不分配、不引用原 dataId；成功字段保存在调用方结构内，原文本只需在调用期间有效。dataId 不包含停点代次。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:651
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:654
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:672
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:800
 */

/**
 * @fn TZrBool zr_debug_agent_data_breakpoint_info(ZrDebugAgent *agent, TZrUInt32 requestedThreadId, TZrUInt32 variablesReference, const TZrChar *name, TZrChar *outDataId, TZrSize outDataIdSize, TZrChar *outDescription, TZrSize outDescriptionSize)
 *
 * ## Summary
 * 把当前可读参数、局部变量或 upvalue 转成客户端可回传的数据断点标识与说明。
 *
 * ## Parameters
 * @param[in,out] agent Agent 与已登记线程状态；正常路径临时切换其 state/currentThreadId 后恢复。
 * @param[in] requestedThreadId 要求的线程 ID；零值由 Agent 的有效线程规则解析。
 * @param[in] variablesReference 由旧式作用域编码得到的引用，不能是动态子变量句柄。
 * @param[in] name 非空的借用变量名，按当前帧重新解析。
 * @param[out] outDataId 标识输出；非零容量必须对应可写缓冲区，NULL 只可与零容量配对。
 * @param[in] outDataIdSize 标识缓冲区实际字节容量，包含 NUL 空间。
 * @param[out] outDescription 可选说明输出；非零容量必须对应可写缓冲区，NULL 只可与零容量配对。
 * @param[in] outDescriptionSize 说明缓冲区实际字节容量。
 *
 * ## Returns
 * @return 当前作用域和变量解析成功时为 TRUE；不支持的引用、缺少线程或目标未找到时为 FALSE。TRUE 不代表标识缓冲区足够或 snprintf 没有截断。
 *
 * ## Constraints and lifecycle
 * 正常路径生成 local:thread:frame:scope:index:name 或 upvalue:thread:frame:index:name。当前实现不检查 snprintf 返回值；不能承诺任意容量都可往返。协议调用者提供完整固定数组，只向客户端公布 write 类别。
 *
 * 普通返回前成对恢复临时线程访问；本函数不保存持久 VM 值，也不直接分配数据断点 C 数组，但 Core 局部值查询可能物化并分配 VM 对象。Core 非局部异常未由此函数捕获，不能声称异常路径也有 finally 式恢复。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:706
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:713
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:767
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:750
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:778
 */

/**
 * @fn void zr_debug_agent_clear_data_breakpoints(ZrDebugAgent *agent)
 *
 * ## Summary
 * 释放 Agent 持有的 C 数据断点数组，并将集合恢复为空状态。
 *
 * ## Parameters
 * @param[in,out] agent 待清理 Agent；允许 NULL，NULL 时不操作。
 *
 * ## Returns
 * void；将数组指针、数量和容量归零。
 *
 * ## Constraints and lifecycle
 * 数组中的 last_value 为非拥有快照，不逐项释放 VM 值；不改变 Core 栈值、线程注册或停止事件。
 *
 * 数组由 add/realloc 建立，此处或 AgentStop 负责 free。任何先前返回的条目指针在清理后失效；协议替换先清空再安装，不承诺整体替换原子性。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1660
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:811
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:852
 */

/**
 * @fn TZrBool zr_debug_agent_add_data_breakpoint(ZrDebugAgent *agent, const TZrChar *dataId, const TZrChar *accessType, TZrChar *outDescription, TZrSize outDescriptionSize)
 *
 * ## Summary
 * 在当前可解析的线程与帧中验证 dataId，安装一项以当前值为初始基线的数据变化监视。
 *
 * ## Parameters
 * @param[in,out] agent 拥有数据断点数组的 Agent；NULL 返回 FALSE。
 * @param[in] dataId 借用的非空 dataId；安装时再次按名称解析目标。
 * @param[in] accessType 允许 NULL、空串、write 或 readWrite；其他非空值拒绝。
 * @param[out] outDescription 可选结果说明缓冲区，进入时先清空。
 * @param[in] outDescriptionSize 说明缓冲区实际字节容量。
 *
 * ## Returns
 * @return 语法、访问类别、线程与目标解析通过且数组空间取得成功时为 TRUE；普通失败为 FALSE，未增加条目。
 *
 * ## Constraints and lifecycle
 * readWrite 只被接受，不实现读取事件或硬件写入陷阱；命中来自 trace 值比较。只支持参数、局部变量和 VM upvalue，无重复项去重或代次身份保证。realloc 使用临时指针，失败保留原数组/count/capacity；不宣称容量乘法具备溢出检查。
 *
 * 先正常恢复临时线程访问，再按需扩容和提交条目。成功条目与 C 数组由 Agent 持有，last_value 仅借用 VM 对象；clear/AgentStop 释放数组，后续 realloc 可使条目地址失效。Core 非局部异常不由此函数回滚。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:800
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:810
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:850
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:859
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:831
 */

/**
 * @fn ZrDebugDataBreakpoint *zr_debug_agent_check_data_breakpoints(ZrDebugAgent *agent)
 *
 * ## Summary
 * 在当前线程的 trace 观察点重新解析已验证监视项，返回首个与借用基线不同的条目。
 *
 * ## Parameters
 * @param[in,out] agent 当前 trace 所属 Agent；读取当前 state/thread，并可更新条目基线。
 *
 * ## Returns
 * @return 首个检测到值变化的数组内借用条目；无 Agent/state、空集合、无适用项或未发现变化时返回 NULL。
 *
 * ## Constraints and lifecycle
 * 只处理当前有效线程的 verified 项，按数组顺序检查；目标暂时解析失败时保留基线，下一次继续尝试。比较采用 Core CompareDirectly：类型不同即不同、字符串按内容、对象按地址等既有规则，不是递归对象内容或每次写入事件检测。命中后先更新该项基线再返回，后续条目留待下一次 trace。
 *
 * 返回指针仅在数组未 realloc/clear 前有效，由 trace observer 立即复制 dataId/description 到停止事件；不移交所有权、不注册 VM root、不提供跨线程同步。其他更高优先级停止原因可使本次 trace 不检查数据断点；无局部异常捕获。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:889
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:903
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:914
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1539
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1551
 * - zr_vm_core/src/zr_vm_core/value/value.c:275
 */
