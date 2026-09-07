#include "zr_vm_language_server_stdio_internal.h"

#include "stdio_request_progress.h"

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

void stdio_request_progress_clear(SZrStdioServer *server) {
    if (server == ZR_NULL) {
        return;
    }
    server->requestProgress.workDoneToken = ZR_NULL;
    server->requestProgress.partialResultToken = ZR_NULL;
    server->requestProgress.workDoneBegan = ZR_FALSE;
}

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

    cJSON_AddItemToObject(params, ZR_LSP_FIELD_TOKEN, token);
    cJSON_AddStringToObject(value, ZR_LSP_FIELD_KIND, kind);
    if (title != ZR_NULL) {
        cJSON_AddStringToObject(value, ZR_LSP_FIELD_TITLE, title);
        cJSON_AddBoolToObject(value, ZR_LSP_FIELD_CANCELLABLE, 1);
    }
    cJSON_AddItemToObject(params, ZR_LSP_FIELD_VALUE, value);
    send_notification(ZR_LSP_METHOD_PROGRESS, params);
    return ZR_TRUE;
}

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

void stdio_request_progress_end(SZrStdioServer *server) {
    if (server == ZR_NULL) {
        return;
    }
    if (server->requestProgress.workDoneBegan) {
        (void)stdio_request_progress_send(server, ZR_LSP_PROGRESS_KIND_END, ZR_NULL);
    }
    stdio_request_progress_clear(server);
}

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

        {
            cJSON *token = stdio_request_progress_token_duplicate(
                    server->requestProgress.partialResultToken);
            if (token == ZR_NULL) {
                cJSON_Delete(params);
                cJSON_Delete(batch);
                return ZR_FALSE;
            }
            cJSON_AddItemToObject(params, ZR_LSP_FIELD_TOKEN, token);
        }
        value = batch;
        if (itemsField != ZR_NULL) {
            value = cJSON_CreateObject();
            if (value == ZR_NULL) {
                cJSON_Delete(params);
                cJSON_Delete(batch);
                return ZR_FALSE;
            }
            cJSON_AddItemToObject(value, itemsField, batch);
        }
        cJSON_AddItemToObject(params, ZR_LSP_FIELD_VALUE, value);
        send_notification(ZR_LSP_METHOD_PROGRESS, params);
        if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(server->context)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

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
    if (!stdio_request_progress_send_array_partial(server, items, itemsField)) {
        return ZR_FALSE;
    }
    if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(server->context)) {
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
