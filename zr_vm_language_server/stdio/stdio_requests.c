#include "zr_vm_language_server_stdio_internal.h"

#include "stdio_request_progress.h"

/* TODO: 这三个项目索引入口的直接调用均在 stdio_workspace_files.c；本文件的
 * 重复前置声明无仓内消费者，需核查移除后编译依赖与声明归属。 */
TZrBool ZrLanguageServer_LspProject_RemoveProjectByProjectUri(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrString *uri);
TZrBool ZrLanguageServer_LspProject_RemoveFileRecordByUri(SZrState *state,
                                                          SZrLspContext *context,
                                                          SZrString *uri);
TZrBool ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(SZrState *state,
                                                                     SZrLspContext *context,
                                                                     SZrString *uri);

/* initialize 成功响应后至 shutdown 前可调整 trace；不合规值保留原设置。 */
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

/* 协议帧仍专用 stdout；可选的人可读流量记录只写 stderr。 */
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

/* 入站读线程可在处理期间取消 ID；响应前复查，避免发送过期成功结果。 */
static TZrBool send_active_request_lifecycle_error(SZrStdioServer *server, const cJSON *id) {
    if (ZrLanguageServer_StdioRequestInput_IsActiveCancelled(server)) {
        send_error_response(id, ZR_LSP_JSON_RPC_REQUEST_CANCELLED_CODE, "Request cancelled");
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

/* 把注册表的取消位接入语义层长运行查询；仅当前请求活动期间注册。 */
static TZrBool stdio_active_request_cancellation_check(void *userData) {
    return ZrLanguageServer_StdioRequestInput_IsActiveCancelled((SZrStdioServer *)userData);
}

/* 生命周期状态决定错误类型：尚未初始化与初始化后非法请求有不同协议码。 */
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

/* 文档请求在分发前固定语义视图；无 textDocument URI 的工作区请求不创建快照。 */
static SZrLspSemanticSnapshot *stdio_request_acquire_semantic_snapshot(SZrStdioServer *server,
                                                                         const cJSON *params) {
    const char *uriText;
    SZrString *uri;

    if (server == ZR_NULL || server->state == ZR_NULL || server->context == ZR_NULL ||
        !get_uri_from_text_document(server, params, &uriText, &uri)) {
        return ZR_NULL;
    }
    ZR_UNUSED_PARAMETER(uriText);
    return ZrLanguageServer_LspSemanticSnapshot_Acquire(server->state, server->context, uri);
}

/* 增量同步失序后拒绝该文档的查询，等待 didOpen/完整同步恢复可信内容。 */
static TZrBool stdio_request_targets_desynchronized_document(
        SZrStdioServer *server,
        const cJSON *params) {
    const char *uriText;
    SZrString *uri;

    return get_uri_from_text_document(server, params, &uriText, &uri) &&
           document_is_desynchronized(server, uri);
}

/* 先解除 context 对快照的借用，再归还快照本体；各请求出口必须配对调用。 */
static void stdio_request_release_semantic_snapshot(SZrStdioServer *server,
                                                     SZrLspSemanticSnapshot *snapshot) {
    if (server == ZR_NULL) {
        return;
    }
    ZrLanguageServer_LspSemanticSnapshot_SetActive(server->context, ZR_NULL);
    ZrLanguageServer_LspSemanticSnapshot_Release(server->state, snapshot);
}

/* 主线程的请求事务：生命周期门禁、取消、进度与语义快照均包围方法处理器，
 * 最后统一映射到 JSON-RPC 响应；id 和 params 借用主循环持有的入站树。 */
void handle_request_message(SZrStdioServer *server,
                            const cJSON *id,
                            const char *method,
                            const cJSON *params) {
    cJSON *result = NULL;
    EZrLspHandlerStatus handlerStatus = ZR_LSP_HANDLER_OK;
    SZrLspSemanticSnapshot *semanticSnapshot = ZR_NULL;

    if (server == ZR_NULL || id == NULL || method == NULL) {
        return;
    }

    /* initialize 的配置副作用与响应写出状态相关；成功发布后才推进生命周期。 */
    if (strcmp(method, ZR_LSP_METHOD_INITIALIZE) == 0) {
        SZrLspHandlerResult initializeResult;
        if (!cJSON_IsObject((cJSON *)params)) {
            send_error_response(id, ZR_LSP_JSON_RPC_INVALID_PARAMS_CODE, "Invalid params");
            return;
        }
        if (!ZrLanguageServer_StdioLifecycle_IsNew(&server->lifecycle)) {
            send_error_response(id, ZR_LSP_JSON_RPC_INVALID_REQUEST_CODE, "Invalid Request");
            return;
        }
        ZrLanguageServer_LspContext_SetRequestCancellationCheck(
                server->context, stdio_active_request_cancellation_check, server);
        initializeResult = handle_initialize_request(server, params);
        ZrLanguageServer_LspContext_SetRequestCancellationCheck(server->context, ZR_NULL, ZR_NULL);
        if (send_active_request_lifecycle_error(server, id)) {
            cJSON_Delete(initializeResult.result);
            return;
        }
        if (initializeResult.status == ZR_LSP_HANDLER_CANCELLED) {
            send_error_response(id, ZR_LSP_JSON_RPC_REQUEST_CANCELLED_CODE, "Request cancelled");
            return;
        }
        if (initializeResult.status != ZR_LSP_HANDLER_OK) {
            send_error_response(id, ZR_LSP_JSON_RPC_INTERNAL_ERROR_CODE, "Internal error");
            return;
        }
        if (send_result_response(id, initializeResult.result) == ZR_STDIO_SEND_OK) {
            (void)ZrLanguageServer_StdioLifecycle_BeginInitialize(&server->lifecycle);
        }
        return;
    }

    /* shutdown 成功响应后才推进生命周期；随后 exit 通知给出成功退出码。 */
    if (strcmp(method, ZR_LSP_METHOD_SHUTDOWN) == 0) {
        if (!ZrLanguageServer_StdioLifecycle_CanProcessRequest(&server->lifecycle)) {
            send_lifecycle_request_error(server, id);
            return;
        }
        if (send_active_request_lifecycle_error(server, id)) {
            return;
        }
        if (send_result_response(id, NULL) == ZR_STDIO_SEND_OK) {
            (void)ZrLanguageServer_StdioLifecycle_BeginShutdown(&server->lifecycle);
        }
        return;
    }

    if (!ZrLanguageServer_StdioLifecycle_CanProcessRequest(&server->lifecycle)) {
        send_lifecycle_request_error(server, id);
        return;
    }

    if (send_active_request_lifecycle_error(server, id)) {
        return;
    }

    /* 避免用旧文本回答编辑器当前版本的文档请求。 */
    if (stdio_request_targets_desynchronized_document(server, params)) {
        send_error_response(id, ZR_LSP_JSON_RPC_CONTENT_MODIFIED_CODE, "Content modified");
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

    /* 处理器和语义层在同一 active 快照下运行；所有提前返回都须解除绑定。 */
    semanticSnapshot = stdio_request_acquire_semantic_snapshot(server, params);
    ZrLanguageServer_LspSemanticSnapshot_SetActive(server->context, semanticSnapshot);
    ZrLanguageServer_LspContext_SetRequestCancellationCheck(
            server->context, stdio_active_request_cancellation_check, server);
    if (!dispatch_request_method(server, method, params, &result, &handlerStatus)) {
        ZrLanguageServer_LspContext_SetRequestCancellationCheck(server->context, ZR_NULL, ZR_NULL);
        stdio_request_progress_end(server);
        if (send_active_request_lifecycle_error(server, id)) {
            stdio_request_release_semantic_snapshot(server, semanticSnapshot);
            return;
        }
        stdio_request_release_semantic_snapshot(server, semanticSnapshot);
        send_error_response(id, ZR_LSP_JSON_RPC_METHOD_NOT_FOUND_CODE, "Method not found");
        return;
    }
    if (send_active_request_lifecycle_error(server, id)) {
        ZrLanguageServer_LspContext_SetRequestCancellationCheck(server->context, ZR_NULL, ZR_NULL);
        stdio_request_progress_end(server);
        cJSON_Delete(result);
        stdio_request_release_semantic_snapshot(server, semanticSnapshot);
        return;
    }
    /* 查询期间文档版本若已变化，丢弃结果而非发布旧位置。 */
    if (handlerStatus == ZR_LSP_HANDLER_OK && semanticSnapshot != ZR_NULL &&
        !ZrLanguageServer_LspSemanticSnapshot_Validate(server->state, server->context, semanticSnapshot)) {
        if (send_active_request_lifecycle_error(server, id)) {
            ZrLanguageServer_LspContext_SetRequestCancellationCheck(server->context, ZR_NULL, ZR_NULL);
            stdio_request_progress_end(server);
            cJSON_Delete(result);
            stdio_request_release_semantic_snapshot(server, semanticSnapshot);
            return;
        }
        ZrLanguageServer_LspContext_SetRequestCancellationCheck(server->context, ZR_NULL, ZR_NULL);
        stdio_request_progress_end(server);
        cJSON_Delete(result);
        stdio_request_release_semantic_snapshot(server, semanticSnapshot);
        send_error_response(id, ZR_LSP_JSON_RPC_CONTENT_MODIFIED_CODE, "Content modified");
        return;
    }
    /* BUG: 部分结果在这里先发出，最终结果到下方才转换坐标；协商 UTF-8 且
     * 文本含非 ASCII 时，partial Location 仍用内部 UTF-16 character。 */
    if (handlerStatus == ZR_LSP_HANDLER_OK &&
        !stdio_request_progress_publish_partial_result(server, method, &result)) {
        handlerStatus = ZrLanguageServer_LspContext_IsRequestCancellationRequested(server->context)
                                ? ZR_LSP_HANDLER_CANCELLED
                                : ZR_LSP_HANDLER_INTERNAL_ERROR;
    }
    if (handlerStatus == ZR_LSP_HANDLER_OK &&
        ZrLanguageServer_LspContext_IsRequestCancellationRequested(server->context)) {
        handlerStatus = ZR_LSP_HANDLER_CANCELLED;
    }
    ZrLanguageServer_LspContext_SetRequestCancellationCheck(server->context, ZR_NULL, ZR_NULL);
    stdio_request_release_semantic_snapshot(server, semanticSnapshot);

    /* TODO: EZrLspHandlerStatus 声明 CONTENT_MODIFIED，但目前处理器无此返回；
     * 若后续处理器使用它，本分支会归入 Internal error，需先明确状态映射契约。 */
    /* 错误处理器不应携带 JSON 结果；成功结果交给发送函数消费。 */
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

/* 通知不生成 JSON-RPC 响应；exit 先于一般生命周期门禁处理，其他工作区与
 * 文档变更只有在 initialize 响应成功后才可修改 LSP 状态。 */
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
        if (!ZrLanguageServer_StdioLifecycle_CanProcessRequest(&server->lifecycle)) {
            return;
        }
        ZrLanguageServer_StdioTrace_Set(server, params);
        return;
    }

    if (!ZrLanguageServer_StdioLifecycle_CanProcessRequest(&server->lifecycle)) {
        return;
    }

    if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_DID_CHANGE_CONFIGURATION) == 0 ||
        strcmp(method, ZR_LSP_METHOD_CANCEL_REQUEST) == 0) {
        return;
    }

    if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_DID_CHANGE_WORKSPACE_FOLDERS) == 0) {
        handle_did_change_workspace_folders(server, params);
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
