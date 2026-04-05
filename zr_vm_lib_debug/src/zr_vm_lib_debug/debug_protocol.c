#include "debug_internal.h"

static cJSON *zr_debug_json_make_message_id(const cJSON *requestId) {
    if (requestId == ZR_NULL) {
        return cJSON_CreateNull();
    }
    if (cJSON_IsNumber(requestId)) {
        return cJSON_CreateNumber(requestId->valuedouble);
    }
    if (cJSON_IsString(requestId) && requestId->valuestring != ZR_NULL) {
        return cJSON_CreateString(requestId->valuestring);
    }
    return cJSON_CreateNull();
}

static TZrBool zr_debug_agent_send_json(ZrDebugAgent *agent, cJSON *json) {
    TZrBool success = ZR_FALSE;
    char *text;

    if (agent == ZR_NULL || !agent->hasClient || !agent->client.isOpen || json == ZR_NULL) {
        return ZR_FALSE;
    }

    text = cJSON_PrintUnformatted(json);
    if (text != ZR_NULL) {
        success = ZrNetwork_StreamWriteFrame(&agent->client, text, strlen(text));
        cJSON_free(text);
    }
    return success;
}

static void zr_debug_agent_send_event(ZrDebugAgent *agent, const TZrChar *eventName, cJSON *params) {
    cJSON *message;

    if (agent == ZR_NULL || !agent->hasClient || !agent->client.isOpen || eventName == ZR_NULL) {
        if (params != ZR_NULL) {
            cJSON_Delete(params);
        }
        return;
    }

    message = cJSON_CreateObject();
    if (message == ZR_NULL) {
        if (params != ZR_NULL) {
            cJSON_Delete(params);
        }
        return;
    }

    cJSON_AddStringToObject(message, "jsonrpc", "2.0");
    cJSON_AddStringToObject(message, "method", eventName);
    cJSON_AddItemToObject(message, "params", params != ZR_NULL ? params : cJSON_CreateObject());
    zr_debug_agent_send_json(agent, message);
    cJSON_Delete(message);
}

static void zr_debug_agent_send_response(ZrDebugAgent *agent, const cJSON *requestId, cJSON *result) {
    cJSON *message;

    if (agent == ZR_NULL) {
        if (result != ZR_NULL) {
            cJSON_Delete(result);
        }
        return;
    }

    message = cJSON_CreateObject();
    if (message == ZR_NULL) {
        if (result != ZR_NULL) {
            cJSON_Delete(result);
        }
        return;
    }

    cJSON_AddStringToObject(message, "jsonrpc", "2.0");
    cJSON_AddItemToObject(message, "id", zr_debug_json_make_message_id(requestId));
    cJSON_AddItemToObject(message, "result", result != ZR_NULL ? result : cJSON_CreateObject());
    zr_debug_agent_send_json(agent, message);
    cJSON_Delete(message);
}

static void zr_debug_agent_send_error(ZrDebugAgent *agent,
                                      const cJSON *requestId,
                                      TZrInt32 code,
                                      const TZrChar *messageText) {
    cJSON *message = cJSON_CreateObject();
    cJSON *errorObject = cJSON_CreateObject();

    if (agent == ZR_NULL || message == ZR_NULL || errorObject == ZR_NULL) {
        if (message != ZR_NULL) {
            cJSON_Delete(message);
        }
        if (errorObject != ZR_NULL) {
            cJSON_Delete(errorObject);
        }
        return;
    }

    cJSON_AddStringToObject(message, "jsonrpc", "2.0");
    cJSON_AddItemToObject(message, "id", zr_debug_json_make_message_id(requestId));
    cJSON_AddNumberToObject(errorObject, "code", code);
    cJSON_AddStringToObject(errorObject, "message", messageText != ZR_NULL ? messageText : "debug error");
    cJSON_AddItemToObject(message, "error", errorObject);
    zr_debug_agent_send_json(agent, message);
    cJSON_Delete(message);
}

void zr_debug_agent_close_client(ZrDebugAgent *agent) {
    if (agent == ZR_NULL) {
        return;
    }

    if (agent->hasClient) {
        ZrNetwork_StreamClose(&agent->client);
    }
    agent->hasClient = ZR_FALSE;
    agent->clientInitialized = ZR_FALSE;
}

static TZrBool zr_debug_agent_accept_client(ZrDebugAgent *agent, TZrUInt32 timeoutMs) {
    if (agent == ZR_NULL || agent->hasClient) {
        return ZR_FALSE;
    }

    if (!ZrNetwork_ListenerAccept(&agent->listener, timeoutMs, &agent->client)) {
        return ZR_FALSE;
    }

    agent->hasClient = ZR_TRUE;
    agent->clientInitialized = ZR_FALSE;
    return ZR_TRUE;
}

void zr_debug_agent_fill_stop_event(ZrDebugAgent *agent,
                                    EZrDebugStopReason reason,
                                    SZrFunction *function,
                                    TZrUInt32 instructionIndex,
                                    TZrUInt32 sourceLine) {
    TZrUInt32 index;

    if (agent == ZR_NULL) {
        return;
    }

    if (function != ZR_NULL && sourceLine == 0 &&
        function->executionLocationInfoList != ZR_NULL && function->executionLocationInfoLength > 0) {
        for (index = 0; index < function->executionLocationInfoLength; index++) {
            if (function->executionLocationInfoList[index].lineInSource != 0) {
                sourceLine = function->executionLocationInfoList[index].lineInSource;
                break;
            }
        }
    }

    memset(&agent->lastStopEvent, 0, sizeof(agent->lastStopEvent));
    agent->stopStateId++;
    agent->lastStopEvent.reason = reason;
    agent->lastStopEvent.line = sourceLine;
    agent->lastStopEvent.instruction_index = instructionIndex;
    agent->lastStopEvent.state_id = agent->stopStateId;
    zr_debug_copy_text(agent->lastStopEvent.source_file,
                       sizeof(agent->lastStopEvent.source_file),
                       zr_debug_function_source(function));
    zr_debug_copy_text(agent->lastStopEvent.function_name,
                       sizeof(agent->lastStopEvent.function_name),
                       zr_debug_function_name(function));
}

static void zr_debug_agent_emit_stopped(ZrDebugAgent *agent) {
    cJSON *params;

    if (agent == ZR_NULL || !agent->clientInitialized) {
        return;
    }

    params = cJSON_CreateObject();
    if (params == ZR_NULL) {
        return;
    }

    cJSON_AddStringToObject(params, "reason", zr_debug_stop_reason_name(agent->lastStopEvent.reason));
    cJSON_AddStringToObject(params, "sourceFile", agent->lastStopEvent.source_file);
    cJSON_AddStringToObject(params, "functionName", agent->lastStopEvent.function_name);
    cJSON_AddNumberToObject(params, "line", agent->lastStopEvent.line);
    cJSON_AddNumberToObject(params, "instructionIndex", agent->lastStopEvent.instruction_index);
    cJSON_AddNumberToObject(params, "stateId", (double)agent->lastStopEvent.state_id);
    zr_debug_agent_send_event(agent, "stopped", params);
}

static void zr_debug_agent_emit_continued(ZrDebugAgent *agent) {
    cJSON *params;

    if (agent == ZR_NULL || !agent->clientInitialized) {
        return;
    }

    params = cJSON_CreateObject();
    if (params == ZR_NULL) {
        return;
    }

    cJSON_AddNumberToObject(params, "stateId", (double)agent->stopStateId);
    zr_debug_agent_send_event(agent, "continued", params);
}

static void zr_debug_agent_emit_module_loaded(ZrDebugAgent *agent) {
    cJSON *params;

    if (agent == ZR_NULL || !agent->clientInitialized) {
        return;
    }

    params = cJSON_CreateObject();
    if (params == ZR_NULL) {
        return;
    }

    cJSON_AddStringToObject(params, "moduleName", agent->moduleName);
    cJSON_AddStringToObject(params, "sourceFile", zr_debug_function_source(agent->entryFunction));
    zr_debug_agent_send_event(agent, "moduleLoaded", params);
}

static void zr_debug_agent_emit_initialized(ZrDebugAgent *agent) {
    zr_debug_agent_send_event(agent, "initialized", cJSON_CreateObject());
    zr_debug_agent_emit_module_loaded(agent);
}

static void zr_debug_agent_emit_breakpoint_resolved(ZrDebugAgent *agent, const ZrDebugBreakpoint *breakpoint) {
    cJSON *params;

    if (agent == ZR_NULL || breakpoint == ZR_NULL || !agent->clientInitialized) {
        return;
    }

    params = cJSON_CreateObject();
    if (params == ZR_NULL) {
        return;
    }

    cJSON_AddStringToObject(params, "moduleName", breakpoint->module_name);
    cJSON_AddStringToObject(params, "sourceFile", breakpoint->source_file);
    cJSON_AddNumberToObject(params, "line", breakpoint->line);
    cJSON_AddStringToObject(params, "functionName", breakpoint->function_name);
    cJSON_AddBoolToObject(params, "resolved", breakpoint->resolved ? 1 : 0);
    cJSON_AddNumberToObject(params, "instructionIndex", breakpoint->resolved_instruction_index);
    zr_debug_agent_send_event(agent, "breakpointResolved", params);
}

static TZrBool zr_debug_agent_process_initialize(ZrDebugAgent *agent, const cJSON *requestId, const cJSON *params) {
    const cJSON *authTokenItem;
    const TZrChar *authToken = ZR_NULL;
    cJSON *result;
    cJSON *capabilities;

    authTokenItem = params != ZR_NULL ? cJSON_GetObjectItemCaseSensitive((cJSON *)params, "authToken") : ZR_NULL;
    if (authTokenItem != ZR_NULL && cJSON_IsString(authTokenItem)) {
        authToken = authTokenItem->valuestring;
    }

    if (!ZrNetwork_TokenMatches(agent->config.auth_token, authToken)) {
        zr_debug_agent_send_error(agent, requestId, -32001, "invalid auth token");
        zr_debug_agent_close_client(agent);
        return ZR_FALSE;
    }

    result = cJSON_CreateObject();
    capabilities = cJSON_CreateObject();
    if (result == ZR_NULL || capabilities == ZR_NULL) {
        if (result != ZR_NULL) {
            cJSON_Delete(result);
        }
        if (capabilities != ZR_NULL) {
            cJSON_Delete(capabilities);
        }
        return ZR_FALSE;
    }

    cJSON_AddStringToObject(result, "protocol", ZR_DEBUG_PROTOCOL_NAME);
    cJSON_AddBoolToObject(capabilities, "supportsSetBreakpoints", 1);
    cJSON_AddBoolToObject(capabilities, "supportsPause", 1);
    cJSON_AddItemToObject(result, "capabilities", capabilities);
    zr_debug_agent_send_response(agent, requestId, result);
    agent->waitForClientPending = ZR_FALSE;
    agent->clientInitialized = ZR_TRUE;
    zr_debug_agent_emit_initialized(agent);
    if (agent->runMode == ZR_DEBUG_RUN_MODE_PAUSED && agent->lastStopEvent.reason != ZR_DEBUG_STOP_REASON_NONE) {
        zr_debug_agent_emit_stopped(agent);
    }
    return ZR_TRUE;
}

static void zr_debug_agent_process_continue(ZrDebugAgent *agent, const cJSON *requestId) {
    ZrDebug_Continue(agent);
    zr_debug_agent_send_response(agent, requestId, cJSON_CreateObject());
    zr_debug_agent_emit_continued(agent);
}

static void zr_debug_agent_process_pause(ZrDebugAgent *agent, const cJSON *requestId) {
    ZrDebug_Pause(agent);
    zr_debug_agent_send_response(agent, requestId, cJSON_CreateObject());
}

static void zr_debug_agent_process_step(ZrDebugAgent *agent, const cJSON *requestId, const TZrChar *method) {
    if (strcmp(method, "stepIn") == 0) {
        ZrDebug_StepInto(agent);
    } else if (strcmp(method, "stepOut") == 0) {
        ZrDebug_StepOut(agent);
    } else {
        ZrDebug_StepOver(agent);
    }
    zr_debug_agent_send_response(agent, requestId, cJSON_CreateObject());
    zr_debug_agent_emit_continued(agent);
}

static TZrBool zr_debug_agent_process_set_breakpoints(ZrDebugAgent *agent,
                                                      const cJSON *requestId,
                                                      const cJSON *params) {
    const cJSON *sourceFileItem;
    const cJSON *moduleNameItem;
    const cJSON *functionNameItem;
    const cJSON *linesItem;
    ZrDebugBreakpointSpec *specs = ZR_NULL;
    TZrSize count = 0;
    TZrSize index;
    cJSON *result;
    cJSON *breakpointsArray;

    if (agent == ZR_NULL || params == ZR_NULL) {
        return ZR_FALSE;
    }

    sourceFileItem = cJSON_GetObjectItemCaseSensitive((cJSON *)params, "sourceFile");
    linesItem = cJSON_GetObjectItemCaseSensitive((cJSON *)params, "lines");
    moduleNameItem = cJSON_GetObjectItemCaseSensitive((cJSON *)params, "moduleName");
    functionNameItem = cJSON_GetObjectItemCaseSensitive((cJSON *)params, "functionName");
    if (!cJSON_IsString(sourceFileItem) || !cJSON_IsArray(linesItem)) {
        zr_debug_agent_send_error(agent, requestId, -32602, "setBreakpoints requires sourceFile and lines");
        return ZR_FALSE;
    }

    count = (TZrSize)cJSON_GetArraySize(linesItem);
    specs = (ZrDebugBreakpointSpec *)calloc(count > 0 ? count : 1, sizeof(*specs));
    if (specs == ZR_NULL) {
        zr_debug_agent_send_error(agent, requestId, -32002, "out of memory");
        return ZR_FALSE;
    }

    for (index = 0; index < count; index++) {
        cJSON *lineItem = cJSON_GetArrayItem((cJSON *)linesItem, (int)index);
        specs[index].module_name = cJSON_IsString(moduleNameItem) ? moduleNameItem->valuestring : agent->moduleName;
        specs[index].source_file = sourceFileItem->valuestring;
        specs[index].function_name = cJSON_IsString(functionNameItem) ? functionNameItem->valuestring : ZR_NULL;
        specs[index].line = cJSON_IsNumber(lineItem) ? (TZrUInt32)lineItem->valuedouble : 0;
    }

    if (!ZrDebug_SetBreakpoints(agent, specs, count)) {
        free(specs);
        zr_debug_agent_send_error(agent, requestId, -32002, "failed to set breakpoints");
        return ZR_FALSE;
    }
    free(specs);

    result = cJSON_CreateObject();
    breakpointsArray = cJSON_CreateArray();
    if (result == ZR_NULL || breakpointsArray == ZR_NULL) {
        if (result != ZR_NULL) {
            cJSON_Delete(result);
        }
        if (breakpointsArray != ZR_NULL) {
            cJSON_Delete(breakpointsArray);
        }
        return ZR_FALSE;
    }

    for (index = 0; index < agent->breakpointCount; index++) {
        cJSON *breakpointObject = cJSON_CreateObject();
        if (breakpointObject == ZR_NULL) {
            continue;
        }

        cJSON_AddBoolToObject(breakpointObject, "verified", agent->breakpoints[index].resolved ? 1 : 0);
        cJSON_AddNumberToObject(breakpointObject, "line", agent->breakpoints[index].line);
        cJSON_AddNumberToObject(breakpointObject, "instructionIndex", agent->breakpoints[index].resolved_instruction_index);
        cJSON_AddItemToArray(breakpointsArray, breakpointObject);
        zr_debug_agent_emit_breakpoint_resolved(agent, &agent->breakpoints[index]);
    }

    cJSON_AddItemToObject(result, "breakpoints", breakpointsArray);
    zr_debug_agent_send_response(agent, requestId, result);
    return ZR_TRUE;
}

static cJSON *zr_debug_agent_make_stack_trace_result(ZrDebugAgent *agent) {
    ZrDebugFrameSnapshot *frames = ZR_NULL;
    TZrSize count = 0;
    cJSON *result = ZR_NULL;
    cJSON *framesArray = ZR_NULL;
    TZrSize index;

    if (!ZrDebug_ReadStack(agent, &frames, &count)) {
        return ZR_NULL;
    }

    result = cJSON_CreateObject();
    framesArray = cJSON_CreateArray();
    if (result == ZR_NULL || framesArray == ZR_NULL) {
        if (result != ZR_NULL) {
            cJSON_Delete(result);
        }
        if (framesArray != ZR_NULL) {
            cJSON_Delete(framesArray);
        }
        ZrDebug_Free(frames);
        return ZR_NULL;
    }

    for (index = 0; index < count; index++) {
        cJSON *frameObject = cJSON_CreateObject();
        if (frameObject == ZR_NULL) {
            continue;
        }

        cJSON_AddNumberToObject(frameObject, "frameId", frames[index].frame_id);
        cJSON_AddStringToObject(frameObject, "functionName", frames[index].function_name);
        cJSON_AddStringToObject(frameObject, "sourceFile", frames[index].source_file);
        cJSON_AddNumberToObject(frameObject, "line", frames[index].line);
        cJSON_AddNumberToObject(frameObject, "instructionIndex", frames[index].instruction_index);
        cJSON_AddItemToArray(framesArray, frameObject);
    }

    cJSON_AddItemToObject(result, "frames", framesArray);
    ZrDebug_Free(frames);
    return result;
}

static cJSON *zr_debug_agent_make_scopes_result(ZrDebugAgent *agent, TZrUInt32 frameId) {
    ZrDebugScopeSnapshot *scopes = ZR_NULL;
    TZrSize count = 0;
    cJSON *result = ZR_NULL;
    cJSON *scopesArray = ZR_NULL;
    TZrSize index;

    if (!ZrDebug_ReadScopes(agent, frameId, &scopes, &count)) {
        return ZR_NULL;
    }

    result = cJSON_CreateObject();
    scopesArray = cJSON_CreateArray();
    if (result == ZR_NULL || scopesArray == ZR_NULL) {
        if (result != ZR_NULL) {
            cJSON_Delete(result);
        }
        if (scopesArray != ZR_NULL) {
            cJSON_Delete(scopesArray);
        }
        ZrDebug_Free(scopes);
        return ZR_NULL;
    }

    for (index = 0; index < count; index++) {
        cJSON *scopeObject = cJSON_CreateObject();
        if (scopeObject == ZR_NULL) {
            continue;
        }

        cJSON_AddNumberToObject(scopeObject, "scopeId", scopes[index].scope_id);
        cJSON_AddNumberToObject(scopeObject, "frameId", scopes[index].frame_id);
        cJSON_AddStringToObject(scopeObject, "name", scopes[index].name);
        cJSON_AddItemToArray(scopesArray, scopeObject);
    }

    cJSON_AddItemToObject(result, "scopes", scopesArray);
    ZrDebug_Free(scopes);
    return result;
}

static cJSON *zr_debug_agent_make_variables_result(ZrDebugAgent *agent, TZrUInt32 scopeId) {
    ZrDebugValuePreview *values = ZR_NULL;
    TZrSize count = 0;
    cJSON *result = ZR_NULL;
    cJSON *valuesArray = ZR_NULL;
    TZrSize index;

    if (!ZrDebug_ReadVariables(agent, scopeId, &values, &count)) {
        return ZR_NULL;
    }

    result = cJSON_CreateObject();
    valuesArray = cJSON_CreateArray();
    if (result == ZR_NULL || valuesArray == ZR_NULL) {
        if (result != ZR_NULL) {
            cJSON_Delete(result);
        }
        if (valuesArray != ZR_NULL) {
            cJSON_Delete(valuesArray);
        }
        ZrDebug_Free(values);
        return ZR_NULL;
    }

    for (index = 0; index < count; index++) {
        cJSON *valueObject = cJSON_CreateObject();
        if (valueObject == ZR_NULL) {
            continue;
        }

        cJSON_AddStringToObject(valueObject, "name", values[index].name);
        cJSON_AddStringToObject(valueObject, "type", values[index].type_name);
        cJSON_AddStringToObject(valueObject, "value", values[index].value_text);
        cJSON_AddNumberToObject(valueObject, "variablesReference", values[index].variables_reference);
        cJSON_AddItemToArray(valuesArray, valueObject);
    }

    cJSON_AddItemToObject(result, "variables", valuesArray);
    ZrDebug_Free(values);
    return result;
}

static void zr_debug_agent_process_stack_trace(ZrDebugAgent *agent, const cJSON *requestId) {
    cJSON *result = zr_debug_agent_make_stack_trace_result(agent);
    if (result == ZR_NULL) {
        zr_debug_agent_send_error(agent, requestId, -32002, "failed to read stack");
        return;
    }

    zr_debug_agent_send_response(agent, requestId, result);
}

static void zr_debug_agent_process_scopes(ZrDebugAgent *agent, const cJSON *requestId, const cJSON *params) {
    cJSON *frameIdItem = params != ZR_NULL ? cJSON_GetObjectItemCaseSensitive((cJSON *)params, "frameId") : ZR_NULL;
    cJSON *result;

    if (!cJSON_IsNumber(frameIdItem)) {
        zr_debug_agent_send_error(agent, requestId, -32602, "scopes requires frameId");
        return;
    }

    result = zr_debug_agent_make_scopes_result(agent, (TZrUInt32)frameIdItem->valuedouble);
    if (result == ZR_NULL) {
        zr_debug_agent_send_error(agent, requestId, -32002, "failed to read scopes");
        return;
    }

    zr_debug_agent_send_response(agent, requestId, result);
}

static void zr_debug_agent_process_variables(ZrDebugAgent *agent, const cJSON *requestId, const cJSON *params) {
    cJSON *scopeIdItem = params != ZR_NULL ? cJSON_GetObjectItemCaseSensitive((cJSON *)params, "scopeId") : ZR_NULL;
    cJSON *result;

    if (!cJSON_IsNumber(scopeIdItem)) {
        zr_debug_agent_send_error(agent, requestId, -32602, "variables requires scopeId");
        return;
    }

    result = zr_debug_agent_make_variables_result(agent, (TZrUInt32)scopeIdItem->valuedouble);
    if (result == ZR_NULL) {
        zr_debug_agent_send_error(agent, requestId, -32002, "failed to read variables");
        return;
    }

    zr_debug_agent_send_response(agent, requestId, result);
}

static void zr_debug_agent_process_disconnect(ZrDebugAgent *agent, const cJSON *requestId) {
    zr_debug_agent_send_response(agent, requestId, cJSON_CreateObject());
    zr_debug_agent_close_client(agent);
}

static TZrBool zr_debug_agent_process_message(ZrDebugAgent *agent, const TZrChar *messageText) {
    cJSON *request;
    cJSON *methodItem;
    cJSON *idItem;
    cJSON *paramsItem;
    const TZrChar *method;

    if (agent == ZR_NULL || messageText == ZR_NULL || messageText[0] == '\0') {
        return ZR_TRUE;
    }

    request = cJSON_Parse(messageText);
    if (request == ZR_NULL) {
        return ZR_TRUE;
    }

    methodItem = cJSON_GetObjectItemCaseSensitive(request, "method");
    idItem = cJSON_GetObjectItemCaseSensitive(request, "id");
    paramsItem = cJSON_GetObjectItemCaseSensitive(request, "params");
    method = cJSON_IsString(methodItem) ? methodItem->valuestring : ZR_NULL;
    if (method == ZR_NULL) {
        cJSON_Delete(request);
        return ZR_TRUE;
    }

    if (!agent->clientInitialized && strcmp(method, "initialize") != 0) {
        zr_debug_agent_send_error(agent, idItem, -32000, "initialize must be the first request");
        cJSON_Delete(request);
        return ZR_TRUE;
    }

    if (strcmp(method, "initialize") == 0) {
        zr_debug_agent_process_initialize(agent, idItem, paramsItem);
    } else if (strcmp(method, "setBreakpoints") == 0) {
        zr_debug_agent_process_set_breakpoints(agent, idItem, paramsItem);
    } else if (strcmp(method, "continue") == 0) {
        zr_debug_agent_process_continue(agent, idItem);
    } else if (strcmp(method, "pause") == 0) {
        zr_debug_agent_process_pause(agent, idItem);
    } else if (strcmp(method, "next") == 0 || strcmp(method, "stepIn") == 0 || strcmp(method, "stepOut") == 0) {
        zr_debug_agent_process_step(agent, idItem, method);
    } else if (strcmp(method, "stackTrace") == 0) {
        zr_debug_agent_process_stack_trace(agent, idItem);
    } else if (strcmp(method, "scopes") == 0) {
        zr_debug_agent_process_scopes(agent, idItem, paramsItem);
    } else if (strcmp(method, "variables") == 0) {
        zr_debug_agent_process_variables(agent, idItem, paramsItem);
    } else if (strcmp(method, "disconnect") == 0) {
        zr_debug_agent_process_disconnect(agent, idItem);
    } else {
        zr_debug_agent_send_error(agent, idItem, -32601, "unsupported debug method");
    }

    cJSON_Delete(request);
    return ZR_TRUE;
}

void zr_debug_agent_poll_messages(ZrDebugAgent *agent, TZrUInt32 timeoutMs, TZrBool *outDisconnected) {
    TZrChar frame[ZR_NETWORK_FRAME_BUFFER_CAPACITY];
    TZrSize frameLength = 0;

    if (outDisconnected != ZR_NULL) {
        *outDisconnected = ZR_FALSE;
    }
    if (agent == ZR_NULL) {
        return;
    }

    if (!agent->hasClient) {
        if (!zr_debug_agent_accept_client(agent, timeoutMs)) {
            return;
        }
    }

    if (!agent->hasClient) {
        return;
    }

    if (!ZrNetwork_StreamReadFrame(&agent->client, timeoutMs, frame, sizeof(frame), &frameLength)) {
        if (timeoutMs == ZR_DEBUG_WAIT_INFINITE || !agent->client.isOpen) {
            zr_debug_agent_close_client(agent);
            if (outDisconnected != ZR_NULL) {
                *outDisconnected = ZR_TRUE;
            }
        }
        return;
    }

    zr_debug_agent_process_message(agent, frame);
    if (!agent->hasClient && outDisconnected != ZR_NULL) {
        *outDisconnected = ZR_TRUE;
    }
}

void zr_debug_agent_pause_loop(ZrDebugAgent *agent) {
    TZrBool disconnected = ZR_FALSE;

    if (agent == ZR_NULL) {
        return;
    }

    agent->runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    zr_debug_agent_emit_stopped(agent);

    while (agent->runMode == ZR_DEBUG_RUN_MODE_PAUSED) {
        disconnected = ZR_FALSE;
        zr_debug_agent_poll_messages(agent, ZR_DEBUG_WAIT_INFINITE, &disconnected);
        if (disconnected && agent->clientDisconnectContinue && !agent->waitForClientPending) {
            agent->runMode = ZR_DEBUG_RUN_MODE_RUNNING;
            zr_debug_bool_store(&agent->pauseRequested, ZR_FALSE);
            break;
        }
    }
}

void ZrDebug_NotifyTerminated(ZrDebugAgent *agent, TZrBool success) {
    cJSON *params;

    if (agent == ZR_NULL || agent->terminatedNotified) {
        return;
    }

    agent->terminatedNotified = ZR_TRUE;
    if (!agent->clientInitialized) {
        return;
    }

    params = cJSON_CreateObject();
    if (params == ZR_NULL) {
        return;
    }

    cJSON_AddBoolToObject(params, "success", success ? 1 : 0);
    zr_debug_agent_send_event(agent, "terminated", params);
}
