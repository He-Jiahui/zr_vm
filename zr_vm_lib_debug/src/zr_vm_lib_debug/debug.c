#include "debug_internal.h"

void zr_debug_copy_text(TZrChar *buffer, TZrSize bufferSize, const TZrChar *text) {
    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }
    if (text == ZR_NULL) {
        buffer[0] = '\0';
        return;
    }

    snprintf(buffer, bufferSize, "%s", text);
    buffer[bufferSize - 1] = '\0';
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
        case ZR_DEBUG_STOP_REASON_NONE:
        default:
            return "none";
    }
}

const TZrChar *zr_debug_scope_kind_name(EZrDebugScopeKind kind) {
    switch (kind) {
        case ZR_DEBUG_SCOPE_KIND_LOCALS:
            return "Locals";
        case ZR_DEBUG_SCOPE_KIND_CLOSURES:
            return "Closures";
        case ZR_DEBUG_SCOPE_KIND_GLOBALS:
            return "Globals";
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
    return frameId * 10u + (TZrUInt32)kind;
}

static TZrBool zr_debug_breakpoint_matches_function(const ZrDebugBreakpoint *breakpoint, SZrFunction *function) {
    if (breakpoint == ZR_NULL || function == ZR_NULL || breakpoint->source_file[0] == '\0') {
        return ZR_FALSE;
    }

    if (strcmp(zr_debug_function_source(function), breakpoint->source_file) != 0) {
        return ZR_FALSE;
    }

    if (breakpoint->function_name[0] != '\0' &&
        strcmp(zr_debug_function_name(function), breakpoint->function_name) != 0) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static void zr_debug_resolve_breakpoint_recursive(ZrDebugBreakpoint *breakpoint, SZrFunction *function) {
    TZrUInt32 index;

    if (breakpoint == ZR_NULL || function == ZR_NULL || breakpoint->resolved) {
        return;
    }

    if (zr_debug_breakpoint_matches_function(breakpoint, function) &&
        function->executionLocationInfoList != ZR_NULL && function->executionLocationInfoLength > 0) {
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

    if (function->childFunctionList != ZR_NULL) {
        for (index = 0; index < function->childFunctionLength; index++) {
            zr_debug_resolve_breakpoint_recursive(breakpoint, &function->childFunctionList[index]);
        }
    }
}

static ZrDebugBreakpoint *zr_debug_find_breakpoint_hit(ZrDebugAgent *agent, SZrFunction *function, TZrUInt32 instructionIndex) {
    TZrSize index;

    if (agent == ZR_NULL || function == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < agent->breakpointCount; index++) {
        if (agent->breakpoints[index].resolved &&
            agent->breakpoints[index].resolved_function == function &&
            agent->breakpoints[index].resolved_instruction_index == instructionIndex) {
            return &agent->breakpoints[index];
        }
    }

    return ZR_NULL;
}

static TZrBool zr_debug_agent_should_stop_for_step(ZrDebugAgent *agent,
                                                   SZrFunction *function,
                                                   TZrUInt32 instructionIndex,
                                                   TZrUInt32 sourceLine) {
    TZrUInt32 currentDepth;
    TZrBool advancedPosition;

    if (agent == ZR_NULL) {
        return ZR_FALSE;
    }

    currentDepth = zr_debug_frame_depth(agent->state);
    advancedPosition =
            (TZrBool)(function != agent->resumeFunction ||
                      sourceLine != agent->lastStopEvent.line ||
                      (sourceLine == 0 && instructionIndex != agent->resumeInstruction));
    switch (agent->runMode) {
        case ZR_DEBUG_RUN_MODE_STEP_INTO:
            return advancedPosition;

        case ZR_DEBUG_RUN_MODE_STEP_OVER:
            return (TZrBool)(advancedPosition && currentDepth <= agent->stepDepth);

        case ZR_DEBUG_RUN_MODE_STEP_OUT:
            return (TZrBool)(currentDepth < agent->stepDepth);

        case ZR_DEBUG_RUN_MODE_RUNNING:
        case ZR_DEBUG_RUN_MODE_PAUSED:
        default:
            return ZR_FALSE;
    }
}

static TZrDebugSignal zr_debug_agent_trace_observer(SZrState *state,
                                                    SZrFunction *function,
                                                    const TZrInstruction *programCounter,
                                                    TZrUInt32 instructionOffset,
                                                    TZrUInt32 sourceLine,
                                                    TZrPtr userData) {
    ZrDebugAgent *agent = (ZrDebugAgent *)userData;
    EZrDebugStopReason reason = ZR_DEBUG_STOP_REASON_NONE;

    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(programCounter);

    if (agent == ZR_NULL || function == ZR_NULL) {
        return ZR_DEBUG_SIGNAL_NONE;
    }

    zr_debug_agent_poll_messages(agent, 0, ZR_NULL);

    if (zr_debug_bool_load(&agent->exceptionStopPending)) {
        reason = ZR_DEBUG_STOP_REASON_EXCEPTION;
        zr_debug_bool_store(&agent->exceptionStopPending, ZR_FALSE);
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
    } else if (zr_debug_find_breakpoint_hit(agent, function, instructionOffset) != ZR_NULL) {
        reason = ZR_DEBUG_STOP_REASON_BREAKPOINT;
    } else if (zr_debug_agent_should_stop_for_step(agent, function, instructionOffset, sourceLine)) {
        reason = ZR_DEBUG_STOP_REASON_STEP;
    }

    if (reason == ZR_DEBUG_STOP_REASON_NONE) {
        return ZR_DEBUG_SIGNAL_NONE;
    }

    zr_debug_agent_fill_stop_event(agent, reason, function, instructionOffset, sourceLine);
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

    if (!ZrNetwork_ParseEndpoint(effectiveConfig.address, &endpoint, errorBuffer, errorBufferSize) ||
        !ZrNetwork_ListenerOpenLoopback(&endpoint, &agent->listener, errorBuffer, errorBufferSize) ||
        !ZrNetwork_FormatEndpoint(&agent->listener.endpoint, agent->endpoint, sizeof(agent->endpoint))) {
        ZrDebug_AgentStop(agent);
        return ZR_FALSE;
    }

    agent->state = state;
    agent->entryFunction = entryFunction;
    agent->config = effectiveConfig;
    agent->runMode = ZR_DEBUG_RUN_MODE_RUNNING;
    agent->clientDisconnectContinue = ZR_TRUE;
    agent->waitForClientPending = effectiveConfig.wait_for_client;
    agent->startStopPending = (TZrBool)(effectiveConfig.suspend_on_start || effectiveConfig.wait_for_client);
    agent->previousDebugHookSignal = state->debugHookSignal;
    zr_debug_copy_text(agent->moduleName, sizeof(agent->moduleName), moduleName != ZR_NULL ? moduleName : "");
    state->debugHookSignal |= ZR_DEBUG_HOOK_MASK_LINE;
    ZrCore_Debug_SetTraceObserver(state, zr_debug_agent_trace_observer, agent);
    *outAgent = agent;
    return ZR_TRUE;
}

void ZrDebug_AgentStop(ZrDebugAgent *agent) {
    if (agent == ZR_NULL) {
        return;
    }

    if (agent->state != ZR_NULL && agent->state->debugTraceUserData == agent) {
        ZrCore_Debug_SetTraceObserver(agent->state, ZR_NULL, ZR_NULL);
        agent->state->debugHookSignal = agent->previousDebugHookSignal;
    }

    zr_debug_agent_close_client(agent);
    ZrNetwork_ListenerClose(&agent->listener);
    if (agent->breakpoints != ZR_NULL) {
        free(agent->breakpoints);
        agent->breakpoints = ZR_NULL;
    }
    free(agent);
}

TZrBool ZrDebug_AgentGetEndpoint(ZrDebugAgent *agent, TZrChar *buffer, TZrSize bufferSize) {
    if (agent == ZR_NULL || buffer == ZR_NULL || bufferSize == 0 || agent->endpoint[0] == '\0') {
        return ZR_FALSE;
    }

    zr_debug_copy_text(buffer, bufferSize, agent->endpoint);
    return ZR_TRUE;
}

TZrBool ZrDebug_SetBreakpoints(ZrDebugAgent *agent, const ZrDebugBreakpointSpec *specs, TZrSize count) {
    TZrSize index;
    ZrDebugBreakpoint *breakpoints = ZR_NULL;

    if (agent == ZR_NULL) {
        return ZR_FALSE;
    }

    if (count > 0) {
        breakpoints = (ZrDebugBreakpoint *)calloc(count, sizeof(*breakpoints));
        if (breakpoints == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    for (index = 0; index < count; index++) {
        zr_debug_copy_text(breakpoints[index].module_name,
                           sizeof(breakpoints[index].module_name),
                           specs[index].module_name != ZR_NULL ? specs[index].module_name : agent->moduleName);
        zr_debug_copy_text(breakpoints[index].source_file,
                           sizeof(breakpoints[index].source_file),
                           specs[index].source_file != ZR_NULL ? specs[index].source_file : "");
        zr_debug_copy_text(breakpoints[index].function_name,
                           sizeof(breakpoints[index].function_name),
                           specs[index].function_name != ZR_NULL ? specs[index].function_name : "");
        breakpoints[index].line = specs[index].line;
        zr_debug_resolve_breakpoint_recursive(&breakpoints[index], agent->entryFunction);
    }

    if (agent->breakpoints != ZR_NULL) {
        free(agent->breakpoints);
    }
    agent->breakpoints = breakpoints;
    agent->breakpointCount = count;
    return ZR_TRUE;
}

void ZrDebug_Continue(ZrDebugAgent *agent) {
    if (agent == ZR_NULL) {
        return;
    }

    zr_debug_bool_store(&agent->pauseRequested, ZR_FALSE);
    agent->runMode = ZR_DEBUG_RUN_MODE_RUNNING;
}

void ZrDebug_Pause(ZrDebugAgent *agent) {
    if (agent == ZR_NULL) {
        return;
    }

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
    agent->resumeFunction = function;
    agent->resumeInstruction = zr_debug_instruction_offset(callInfo, function);
    agent->stepDepth = zr_debug_frame_depth(agent->state);
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

    if (agent == ZR_NULL || agent->state == ZR_NULL || !agent->config.stop_on_uncaught_exception) {
        return;
    }
    if (!agent->hasClient && !agent->config.wait_for_client) {
        return;
    }

    if (zr_debug_exception_read_frame(agent, 1, &frame)) {
        zr_debug_agent_fill_stop_event(agent,
                                       ZR_DEBUG_STOP_REASON_EXCEPTION,
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
    zr_debug_agent_fill_stop_event(agent, ZR_DEBUG_STOP_REASON_EXCEPTION, function, instructionIndex, sourceLine);
    zr_debug_agent_pause_loop(agent);
}
