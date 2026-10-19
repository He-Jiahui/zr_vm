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
