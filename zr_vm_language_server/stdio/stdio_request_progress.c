#include "zr_vm_language_server_stdio_internal.h"

#include "stdio_request_progress.h"
#include "stdio_json_builder.h"

/* 数字 token 须可经 cJSON 的 double 无损往返，避免客户端按不同 ID 匹配通知。 */
static TZrBool stdio_request_progress_token_is_valid(const cJSON *token) {
    double number;

    if (cJSON_IsString((cJSON *)token)) {
        return ZR_TRUE;
    }
    if (!cJSON_IsNumber((cJSON *)token)) {
        return ZR_FALSE;
    }

    number = token->valuedouble;
    return number >= -ZR_LSP_JSON_SAFE_INTEGER_MAX &&
           number <= ZR_LSP_JSON_SAFE_INTEGER_MAX &&
           number == (double)(long long)number;
}

/* BUG: 字符串 token 含 U+0000 时 cJSON_Duplicate 经 strlen 截断，通知 token 与请求 token 不匹配。 */
static cJSON *stdio_request_progress_token_duplicate(const cJSON *token) {
    char number[32];
    int length;

    if (token == ZR_NULL) {
        return ZR_NULL;
    }
    if (!cJSON_IsNumber((cJSON *)token)) {
        return cJSON_Duplicate((cJSON *)token, 1);
    }

    length = snprintf(number, sizeof(number), "%.17g", token->valuedouble);
    if (length < 0 || (size_t)length >= sizeof(number)) {
        return ZR_NULL;
    }
    return cJSON_CreateRaw(number);
}

/* 仅当前请求分发层能分批处理或报告长运行状态的方法接受进度 token。 */
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

/* 数组响应直接分批；workspace diagnostic 的对象响应另由 items 分支处理。 */
static TZrBool stdio_request_method_supports_array_partial_results(const char *method) {
    return method != ZR_NULL &&
           (strcmp(method, ZR_LSP_METHOD_WORKSPACE_SYMBOL) == 0 ||
            strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_REFERENCES) == 0 ||
            strcmp(method, ZR_LSP_METHOD_CALL_HIERARCHY_INCOMING_CALLS) == 0 ||
            strcmp(method, ZR_LSP_METHOD_CALL_HIERARCHY_OUTGOING_CALLS) == 0 ||
            strcmp(method, ZR_LSP_METHOD_TYPE_HIERARCHY_SUPERTYPES) == 0 ||
            strcmp(method, ZR_LSP_METHOD_TYPE_HIERARCHY_SUBTYPES) == 0);
}

/* 消息树释放前终止借用，防止下一请求误用上个请求的 token。 */
void stdio_request_progress_clear(SZrStdioServer *server) {
    if (server == ZR_NULL) {
        return;
    }
    server->requestProgress.workDoneToken = ZR_NULL;
    server->requestProgress.partialResultToken = ZR_NULL;
    server->requestProgress.workDoneBegan = ZR_FALSE;
}

/* handle_request_message 在分发前调用；无进度能力的方法忽略扩展 token。 */
TZrBool stdio_request_progress_prepare(SZrStdioServer *server,
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

/* 输出函数消费 params；本层必须在发送前建成完整通知并保留原 token 树。 */
static TZrBool stdio_request_progress_send(SZrStdioServer *server,
                                            const char *kind,
                                            const char *title) {
    cJSON *params;
    cJSON *value;
    cJSON *token;

    if (server == ZR_NULL || server->requestProgress.workDoneToken == ZR_NULL) {
        return ZR_TRUE;
    }

    params = cJSON_CreateObject();
    value = cJSON_CreateObject();
    token = stdio_request_progress_token_duplicate(server->requestProgress.workDoneToken);
    if (params == ZR_NULL || value == ZR_NULL || token == ZR_NULL) {
        cJSON_Delete(params);
        cJSON_Delete(value);
        cJSON_Delete(token);
        return ZR_FALSE;
    }

    if (!stdio_json_add_owned_item(params, ZR_LSP_FIELD_TOKEN, token) ||
        cJSON_AddStringToObject(value, ZR_LSP_FIELD_KIND, kind) == ZR_NULL) {
        cJSON_Delete(params);
        cJSON_Delete(value);
        return ZR_FALSE;
    }
    if (title != ZR_NULL) {
        if (cJSON_AddStringToObject(value, ZR_LSP_FIELD_TITLE, title) == ZR_NULL ||
            cJSON_AddBoolToObject(value, ZR_LSP_FIELD_CANCELLABLE, 1) == ZR_NULL) {
            cJSON_Delete(params);
            cJSON_Delete(value);
            return ZR_FALSE;
        }
    }
    if (!stdio_json_add_owned_item(params, ZR_LSP_FIELD_VALUE, value)) {
        cJSON_Delete(params);
        return ZR_FALSE;
    }
    return send_notification(ZR_LSP_METHOD_PROGRESS, params) == ZR_STDIO_SEND_OK;
}

/* begin 成功后才允许 end；发送失败则请求路径回传内部错误而不留下已开始标记。 */
TZrBool stdio_request_progress_begin(SZrStdioServer *server, const char *method) {
    if (server == ZR_NULL || server->requestProgress.workDoneToken == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!stdio_request_progress_send(server, ZR_LSP_PROGRESS_KIND_BEGIN, method)) {
        return ZR_FALSE;
    }
    server->requestProgress.workDoneBegan = ZR_TRUE;
    return ZR_TRUE;
}

/* BUG: begin 已发布时，end 构造若因一次性 OOM 失败仍被忽略且状态被清空；
 * 内存恢复后 stdio_requests.c 可继续发送最终响应，客户端只收到 begin 而没有 end。 */
void stdio_request_progress_end(SZrStdioServer *server) {
    if (server == ZR_NULL) {
        return;
    }
    if (server->requestProgress.workDoneBegan) {
        (void)stdio_request_progress_send(server, ZR_LSP_PROGRESS_KIND_END, ZR_NULL);
    }
    stdio_request_progress_clear(server);
}

/* 保留原 result 树直至所有批次发布成功；逐批复制使发送端独占通知树。 */
static TZrBool stdio_request_progress_send_array_partial(SZrStdioServer *server,
                                                          cJSON *result,
                                                          const char *itemsField) {
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
        cJSON *value;
        int batchEnd = resultIndex + ZR_LSP_PARTIAL_RESULT_BATCH_SIZE;

        /* 每批发送前后都观察读线程的取消位，包含最后一批与最终响应之间的窗口。 */
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
            if (!stdio_json_add_owned_array_item(batch, copy)) {
                cJSON_Delete(params);
                cJSON_Delete(batch);
                return ZR_FALSE;
            }
        }

        {
            cJSON *token = stdio_request_progress_token_duplicate(
                    server->requestProgress.partialResultToken);
            if (token == ZR_NULL) {
                cJSON_Delete(params);
                cJSON_Delete(batch);
                return ZR_FALSE;
            }
            if (!stdio_json_add_owned_item(params, ZR_LSP_FIELD_TOKEN, token)) {
                cJSON_Delete(params);
                cJSON_Delete(batch);
                return ZR_FALSE;
            }
        }
        /* WorkspaceDiagnosticReport 的 partial value 仍须包在 items 对象里。 */
        value = batch;
        if (itemsField != ZR_NULL) {
            value = cJSON_CreateObject();
            if (value == ZR_NULL) {
                cJSON_Delete(params);
                cJSON_Delete(batch);
                return ZR_FALSE;
            }
            if (!stdio_json_add_owned_item(value, itemsField, batch)) {
                cJSON_Delete(value);
                cJSON_Delete(params);
                return ZR_FALSE;
            }
        }
        if (!stdio_json_add_owned_item(params, ZR_LSP_FIELD_VALUE, value)) {
            cJSON_Delete(params);
            return ZR_FALSE;
        }
        if (send_notification(ZR_LSP_METHOD_PROGRESS, params) != ZR_STDIO_SEND_OK) {
            return ZR_FALSE;
        }
        if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(server->context)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/* handle_request_message 在响应编码之前调用；成功后替换调用方持有的完成结果。 */
TZrBool stdio_request_progress_publish_partial_result(SZrStdioServer *server,
                                                      const char *method,
                                                      cJSON **inOutResult) {
    cJSON *completedResult;
    cJSON *items;
    const char *itemsField = ZR_NULL;

    if (server == ZR_NULL || inOutResult == ZR_NULL ||
        server->requestProgress.partialResultToken == ZR_NULL) {
        return ZR_TRUE;
    }
    items = *inOutResult;
    if (!stdio_request_method_supports_array_partial_results(method)) {
        if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_DIAGNOSTIC) != 0 ||
            !cJSON_IsObject(*inOutResult)) {
            return ZR_TRUE;
        }
        items = cJSON_GetObjectItemCaseSensitive(*inOutResult, ZR_LSP_FIELD_ITEMS);
        itemsField = ZR_LSP_FIELD_ITEMS;
    }
    /* BUG: references 等结果含内部 UTF-16 坐标，分批通知在此直接发出；
     * 协商 UTF-8 时，stdio_requests.c 仅在最终响应分支转换坐标，非 ASCII
     * 文本下 partial Location 的 character 仍是 UTF-16 列号。 */
    if (!stdio_request_progress_send_array_partial(server, items, itemsField)) {
        return ZR_FALSE;
    }
    if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(server->context)) {
        return ZR_FALSE;
    }

    /* WorkspaceDiagnosticReport 即使所有 items 已流式发送，最终响应仍须是对象。 */
    completedResult = itemsField == ZR_NULL ? cJSON_CreateNull() : cJSON_CreateObject();
    if (completedResult == ZR_NULL) {
        return ZR_FALSE;
    }
    if (itemsField != ZR_NULL && cJSON_AddArrayToObject(completedResult, itemsField) == ZR_NULL) {
        cJSON_Delete(completedResult);
        return ZR_FALSE;
    }
    cJSON_Delete(*inOutResult);
    *inOutResult = completedResult;
    return ZR_TRUE;
}
