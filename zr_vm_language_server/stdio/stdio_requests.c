#include "zr_vm_language_server_stdio_internal.h"

TZrBool ZrLanguageServer_LspProject_RemoveProjectByProjectUri(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrString *uri);
TZrBool ZrLanguageServer_LspProject_RemoveFileRecordByUri(SZrState *state,
                                                          SZrLspContext *context,
                                                          SZrString *uri);
TZrBool ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(SZrState *state,
                                                                     SZrLspContext *context,
                                                                     SZrString *uri);

void ZrLanguageServer_StdioTrace_Set(SZrStdioServer *server, const cJSON *params) {
    const cJSON *value;
    const char *valueText;

    if (server == ZR_NULL || !cJSON_IsObject((cJSON *)params)) {
        return;
    }
    value = cJSON_GetObjectItemCaseSensitive((cJSON *)params, ZR_LSP_FIELD_VALUE);
    valueText = cJSON_IsString((cJSON *)value) ? cJSON_GetStringValue((cJSON *)value) : ZR_NULL;
    if (valueText == ZR_NULL) {
        return;
    }
    if (strcmp(valueText, "off") == 0) {
        server->traceLevel = ZR_STDIO_TRACE_OFF;
    } else if (strcmp(valueText, "messages") == 0) {
        server->traceLevel = ZR_STDIO_TRACE_MESSAGES;
    } else if (strcmp(valueText, "verbose") == 0) {
        server->traceLevel = ZR_STDIO_TRACE_VERBOSE;
    }
}

void ZrLanguageServer_StdioTrace_Log(SZrStdioServer *server,
                                     const char *direction,
                                     const char *kind,
                                     const char *method,
                                     TZrBool isNotification) {
    if (server == ZR_NULL || direction == ZR_NULL || kind == ZR_NULL || method == ZR_NULL ||
        server->traceLevel == ZR_STDIO_TRACE_OFF ||
        (isNotification && server->traceLevel != ZR_STDIO_TRACE_VERBOSE)) {
        return;
    }
    fprintf(stderr, "LSP trace %s %s %s\n", direction, kind, method);
    fflush(stderr);
}

static TZrBool send_active_request_lifecycle_error(SZrStdioServer *server, const cJSON *id) {
    if (ZrLanguageServer_StdioRequestInput_IsActiveCancelled(server)) {
        send_error_response(id, ZR_LSP_JSON_RPC_REQUEST_CANCELLED_CODE, "Request cancelled");
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool stdio_active_request_cancellation_check(void *userData) {
    return ZrLanguageServer_StdioRequestInput_IsActiveCancelled((SZrStdioServer *)userData);
}

static TZrBool send_lifecycle_request_error(SZrStdioServer *server, const cJSON *id) {
    if (server == ZR_NULL || id == ZR_NULL) {
        return ZR_TRUE;
    }

    if (ZrLanguageServer_StdioLifecycle_IsNew(&server->lifecycle)) {
        send_error_response(id,
                            ZR_LSP_JSON_RPC_SERVER_NOT_INITIALIZED_CODE,
                            "Server not initialized");
    } else {
        send_error_response(id, ZR_LSP_JSON_RPC_INVALID_REQUEST_CODE, "Invalid Request");
    }
    return ZR_TRUE;
}

void handle_request_message(SZrStdioServer *server,
                            const cJSON *id,
                            const char *method,
                            const cJSON *params) {
    cJSON *result = NULL;
    EZrLspHandlerStatus handlerStatus = ZR_LSP_HANDLER_OK;

    if (server == ZR_NULL || id == NULL || method == NULL) {
        return;
    }

    if (strcmp(method, ZR_LSP_METHOD_INITIALIZE) == 0) {
        if (!ZrLanguageServer_StdioLifecycle_BeginInitialize(&server->lifecycle)) {
            send_error_response(id, ZR_LSP_JSON_RPC_INVALID_REQUEST_CODE, "Invalid Request");
            return;
        }
        result = handle_initialize_request(server, params);
        if (send_active_request_lifecycle_error(server, id)) {
            cJSON_Delete(result);
            return;
        }
        send_result_response(id, result != NULL ? result : cJSON_CreateNull());
        return;
    }

    if (strcmp(method, ZR_LSP_METHOD_SHUTDOWN) == 0) {
        if (!ZrLanguageServer_StdioLifecycle_BeginShutdown(&server->lifecycle)) {
            send_lifecycle_request_error(server, id);
            return;
        }
        if (send_active_request_lifecycle_error(server, id)) {
            return;
        }
        send_result_response(id, NULL);
        return;
    }

    if (!ZrLanguageServer_StdioLifecycle_CanProcessRequest(&server->lifecycle)) {
        send_lifecycle_request_error(server, id);
        return;
    }

    if (send_active_request_lifecycle_error(server, id)) {
        return;
    }

    ZrLanguageServer_LspContext_SetRequestCancellationCheck(
            server->context, stdio_active_request_cancellation_check, server);
    if (!dispatch_request_method(server, method, params, &result, &handlerStatus)) {
        ZrLanguageServer_LspContext_SetRequestCancellationCheck(server->context, ZR_NULL, ZR_NULL);
        if (send_active_request_lifecycle_error(server, id)) {
            return;
        }
        send_error_response(id, ZR_LSP_JSON_RPC_METHOD_NOT_FOUND_CODE, "Method not found");
        return;
    }
    ZrLanguageServer_LspContext_SetRequestCancellationCheck(server->context, ZR_NULL, ZR_NULL);

    if (send_active_request_lifecycle_error(server, id)) {
        cJSON_Delete(result);
        return;
    }

    if (handlerStatus == ZR_LSP_HANDLER_INVALID_PARAMS) {
        send_error_response(id, ZR_LSP_JSON_RPC_INVALID_PARAMS_CODE, "Invalid params");
    } else if (handlerStatus == ZR_LSP_HANDLER_CANCELLED) {
        cJSON_Delete(result);
        send_error_response(id, ZR_LSP_JSON_RPC_REQUEST_CANCELLED_CODE, "Request cancelled");
    } else if (handlerStatus != ZR_LSP_HANDLER_OK) {
        cJSON_Delete(result);
        send_error_response(id, ZR_LSP_JSON_RPC_INTERNAL_ERROR_CODE, "Internal error");
    } else {
        apply_position_encoding_to_response(server, method, params, result);
        send_result_response(id, result);
    }
}

void handle_notification_message(SZrStdioServer *server,
                                 const char *method,
                                 const cJSON *params,
                                 int *outShouldExit,
                                 int *outExitCode) {
    if (outShouldExit != NULL) {
        *outShouldExit = 0;
    }
    if (outExitCode != NULL) {
        *outExitCode = 0;
    }

    if (server == ZR_NULL || method == NULL) {
        return;
    }

    if (strcmp(method, ZR_LSP_METHOD_EXIT) == 0) {
        if (outShouldExit != NULL) {
            *outShouldExit = 1;
        }
        if (outExitCode != NULL) {
            *outExitCode = ZrLanguageServer_StdioLifecycle_Exit(&server->lifecycle);
        }
        return;
    }

    if (strcmp(method, ZR_LSP_METHOD_INITIALIZED) == 0) {
        ZrLanguageServer_StdioLifecycle_MarkInitialized(&server->lifecycle);
        return;
    }

    if (strcmp(method, ZR_LSP_METHOD_SET_TRACE) == 0) {
        ZrLanguageServer_StdioTrace_Set(server, params);
        return;
    }

    if (!ZrLanguageServer_StdioLifecycle_CanProcessRequest(&server->lifecycle)) {
        return;
    }

    if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_DID_CHANGE_CONFIGURATION) == 0 ||
        strcmp(method, ZR_LSP_METHOD_WORKSPACE_DID_CHANGE_WORKSPACE_FOLDERS) == 0 ||
        strcmp(method, ZR_LSP_METHOD_CANCEL_REQUEST) == 0) {
        return;
    }

    if (strcmp(method, ZR_LSP_METHOD_ZR_SELECTED_PROJECT) == 0) {
        handle_zr_selected_project_notification(server, params);
        return;
    }

    if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DID_OPEN) == 0) {
        handle_did_open(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DID_CHANGE) == 0) {
        handle_did_change(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DID_CLOSE) == 0) {
        handle_did_close(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DID_SAVE) == 0) {
        handle_did_save(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_DID_CHANGE_WATCHED_FILES) == 0) {
        handle_did_change_watched_files(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_DID_CREATE_FILES) == 0) {
        handle_did_create_files(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_DID_RENAME_FILES) == 0) {
        handle_did_rename_files(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_DID_DELETE_FILES) == 0) {
        handle_did_delete_files(server, params);
    }
}
