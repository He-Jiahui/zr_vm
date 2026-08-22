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

static TZrBool stdio_request_progress_token_is_valid(const cJSON *token) {
    double number;

    if (cJSON_IsString((cJSON *)token)) {
        return ZR_TRUE;
    }
    if (!cJSON_IsNumber((cJSON *)token)) {
        return ZR_FALSE;
    }

    number = token->valuedouble;
    return number >= -9007199254740991.0 &&
           number <= 9007199254740991.0 &&
           number == (double)(long long)number;
}

static TZrBool stdio_request_method_supports_progress(const char *method) {
    return method != ZR_NULL &&
           (strcmp(method, ZR_LSP_METHOD_WORKSPACE_SYMBOL) == 0 ||
            strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_REFERENCES) == 0 ||
            strcmp(method, ZR_LSP_METHOD_WORKSPACE_DIAGNOSTIC) == 0 ||
            strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_RENAME) == 0 ||
            strcmp(method, ZR_LSP_METHOD_CALL_HIERARCHY_INCOMING_CALLS) == 0 ||
            strcmp(method, ZR_LSP_METHOD_CALL_HIERARCHY_OUTGOING_CALLS) == 0 ||
            strcmp(method, ZR_LSP_METHOD_TYPE_HIERARCHY_SUPERTYPES) == 0 ||
            strcmp(method, ZR_LSP_METHOD_TYPE_HIERARCHY_SUBTYPES) == 0);
}

static TZrBool stdio_request_method_supports_array_partial_results(const char *method) {
    return method != ZR_NULL &&
           (strcmp(method, ZR_LSP_METHOD_WORKSPACE_SYMBOL) == 0 ||
            strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_REFERENCES) == 0 ||
            strcmp(method, ZR_LSP_METHOD_CALL_HIERARCHY_INCOMING_CALLS) == 0 ||
            strcmp(method, ZR_LSP_METHOD_CALL_HIERARCHY_OUTGOING_CALLS) == 0 ||
            strcmp(method, ZR_LSP_METHOD_TYPE_HIERARCHY_SUPERTYPES) == 0 ||
            strcmp(method, ZR_LSP_METHOD_TYPE_HIERARCHY_SUBTYPES) == 0);
}

static void stdio_request_progress_clear(SZrStdioServer *server) {
    if (server == ZR_NULL) {
        return;
    }
    server->requestProgress.workDoneToken = ZR_NULL;
    server->requestProgress.partialResultToken = ZR_NULL;
    server->requestProgress.workDoneBegan = ZR_FALSE;
}

static TZrBool stdio_request_progress_prepare(SZrStdioServer *server,
                                               const char *method,
                                               const cJSON *params) {
    const cJSON *workDoneToken;
    const cJSON *partialResultToken;

    stdio_request_progress_clear(server);
    if (server == ZR_NULL || !stdio_request_method_supports_progress(method)) {
        return ZR_TRUE;
    }

    workDoneToken = cJSON_GetObjectItemCaseSensitive((cJSON *)params, ZR_LSP_FIELD_WORK_DONE_TOKEN);
    partialResultToken =
        cJSON_GetObjectItemCaseSensitive((cJSON *)params, ZR_LSP_FIELD_PARTIAL_RESULT_TOKEN);
    if ((workDoneToken != ZR_NULL && !stdio_request_progress_token_is_valid(workDoneToken)) ||
        (partialResultToken != ZR_NULL && !stdio_request_progress_token_is_valid(partialResultToken))) {
        return ZR_FALSE;
    }

    server->requestProgress.workDoneToken = workDoneToken;
    server->requestProgress.partialResultToken = partialResultToken;
    return ZR_TRUE;
}

static TZrBool stdio_request_progress_send(SZrStdioServer *server,
                                           const char *kind,
                                           const char *title) {
    cJSON *params;
    cJSON *value;

    if (server == ZR_NULL || server->requestProgress.workDoneToken == ZR_NULL) {
        return ZR_TRUE;
    }

    params = cJSON_CreateObject();
    value = cJSON_CreateObject();
    if (params == ZR_NULL || value == ZR_NULL) {
        cJSON_Delete(params);
        cJSON_Delete(value);
        return ZR_FALSE;
    }

    cJSON_AddItemReferenceToObject(params, ZR_LSP_FIELD_TOKEN, (cJSON *)server->requestProgress.workDoneToken);
    cJSON_AddStringToObject(value, ZR_LSP_FIELD_KIND, kind);
    if (title != ZR_NULL) {
        cJSON_AddStringToObject(value, ZR_LSP_FIELD_TITLE, title);
        cJSON_AddBoolToObject(value, ZR_LSP_FIELD_CANCELLABLE, 1);
    }
    cJSON_AddItemToObject(params, ZR_LSP_FIELD_VALUE, value);
    send_notification(ZR_LSP_METHOD_PROGRESS, params);
    return ZR_TRUE;
}

static TZrBool stdio_request_progress_begin(SZrStdioServer *server, const char *method) {
    if (server == ZR_NULL || server->requestProgress.workDoneToken == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!stdio_request_progress_send(server, ZR_LSP_PROGRESS_KIND_BEGIN, method)) {
        return ZR_FALSE;
    }
    server->requestProgress.workDoneBegan = ZR_TRUE;
    return ZR_TRUE;
}

static void stdio_request_progress_end(SZrStdioServer *server) {
    if (server == ZR_NULL) {
        return;
    }
    if (server->requestProgress.workDoneBegan) {
        (void)stdio_request_progress_send(server, ZR_LSP_PROGRESS_KIND_END, ZR_NULL);
    }
    stdio_request_progress_clear(server);
}

static TZrBool stdio_request_progress_send_array_partial(SZrStdioServer *server,
                                                          cJSON *result) {
    int resultCount;
    int resultIndex;

    if (server == ZR_NULL || server->requestProgress.partialResultToken == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!cJSON_IsArray(result)) {
        return ZR_FALSE;
    }

    resultCount = cJSON_GetArraySize(result);
    for (resultIndex = 0; resultIndex < resultCount; resultIndex += ZR_LSP_PARTIAL_RESULT_BATCH_SIZE) {
        cJSON *params;
        cJSON *batch;
        int batchEnd = resultIndex + ZR_LSP_PARTIAL_RESULT_BATCH_SIZE;

        if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(server->context)) {
            return ZR_FALSE;
        }

        params = cJSON_CreateObject();
        batch = cJSON_CreateArray();
        if (params == ZR_NULL || batch == ZR_NULL) {
            cJSON_Delete(params);
            cJSON_Delete(batch);
            return ZR_FALSE;
        }
        if (batchEnd > resultCount) {
            batchEnd = resultCount;
        }
        for (int itemIndex = resultIndex; itemIndex < batchEnd; itemIndex++) {
            cJSON *copy = cJSON_Duplicate(cJSON_GetArrayItem(result, itemIndex), 1);
            if (copy == ZR_NULL) {
                cJSON_Delete(params);
                cJSON_Delete(batch);
                return ZR_FALSE;
            }
            cJSON_AddItemToArray(batch, copy);
        }

        cJSON_AddItemReferenceToObject(params,
                                       ZR_LSP_FIELD_TOKEN,
                                       (cJSON *)server->requestProgress.partialResultToken);
        cJSON_AddItemToObject(params, ZR_LSP_FIELD_VALUE, batch);
        send_notification(ZR_LSP_METHOD_PROGRESS, params);
    }
    return ZR_TRUE;
}

static TZrBool stdio_request_progress_publish_partial_result(SZrStdioServer *server,
                                                              const char *method,
                                                              cJSON **inOutResult) {
    cJSON *completedResult;

    if (server == ZR_NULL || inOutResult == ZR_NULL ||
        server->requestProgress.partialResultToken == ZR_NULL ||
        !stdio_request_method_supports_array_partial_results(method)) {
        return ZR_TRUE;
    }
    if (!stdio_request_progress_send_array_partial(server, *inOutResult)) {
        return ZR_FALSE;
    }

    completedResult = cJSON_CreateNull();
    if (completedResult == ZR_NULL) {
        return ZR_FALSE;
    }
    cJSON_Delete(*inOutResult);
    *inOutResult = completedResult;
    return ZR_TRUE;
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

    if (!stdio_request_progress_prepare(server, method, params)) {
        send_error_response(id, ZR_LSP_JSON_RPC_INVALID_PARAMS_CODE, "Invalid params");
        return;
    }

    if (!stdio_request_progress_begin(server, method)) {
        stdio_request_progress_clear(server);
        send_error_response(id, ZR_LSP_JSON_RPC_INTERNAL_ERROR_CODE, "Internal error");
        return;
    }

    ZrLanguageServer_LspContext_SetRequestCancellationCheck(
            server->context, stdio_active_request_cancellation_check, server);
    if (!dispatch_request_method(server, method, params, &result, &handlerStatus)) {
        ZrLanguageServer_LspContext_SetRequestCancellationCheck(server->context, ZR_NULL, ZR_NULL);
        stdio_request_progress_end(server);
        if (send_active_request_lifecycle_error(server, id)) {
            return;
        }
        send_error_response(id, ZR_LSP_JSON_RPC_METHOD_NOT_FOUND_CODE, "Method not found");
        return;
    }
    if (handlerStatus == ZR_LSP_HANDLER_OK &&
        !stdio_request_progress_publish_partial_result(server, method, &result)) {
        handlerStatus = ZrLanguageServer_LspContext_IsRequestCancellationRequested(server->context)
                                ? ZR_LSP_HANDLER_CANCELLED
                                : ZR_LSP_HANDLER_INTERNAL_ERROR;
    }
    ZrLanguageServer_LspContext_SetRequestCancellationCheck(server->context, ZR_NULL, ZR_NULL);

    if (send_active_request_lifecycle_error(server, id)) {
        stdio_request_progress_end(server);
        cJSON_Delete(result);
        return;
    }

    if (handlerStatus == ZR_LSP_HANDLER_INVALID_PARAMS) {
        stdio_request_progress_end(server);
        send_error_response(id, ZR_LSP_JSON_RPC_INVALID_PARAMS_CODE, "Invalid params");
    } else if (handlerStatus == ZR_LSP_HANDLER_CANCELLED) {
        stdio_request_progress_end(server);
        cJSON_Delete(result);
        send_error_response(id, ZR_LSP_JSON_RPC_REQUEST_CANCELLED_CODE, "Request cancelled");
    } else if (handlerStatus != ZR_LSP_HANDLER_OK) {
        stdio_request_progress_end(server);
        cJSON_Delete(result);
        send_error_response(id, ZR_LSP_JSON_RPC_INTERNAL_ERROR_CODE, "Internal error");
    } else {
        stdio_request_progress_end(server);
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
