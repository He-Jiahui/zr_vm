#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_handler_result.h"
#include "stdio_json_builder.h"
#include "zr_vm_language_server/lsp_diagnostic_store.h"
#include "project/lsp_project_internal.h"

/** 统一 push、文档 pull 和工作区 pull 的结果 ID 生成入口。 */
static TZrBool build_diagnostic_result_id(SZrStdioServer *server,
                                          SZrString *uri,
                                          const SZrArray *diagnostics,
                                          char *buffer,
                                          size_t bufferSize);

/** 按原样 URI 文本查找本连接最近一次成功发出的诊断快照；返回服务器借用指针。 */
static SZrDiagnosticPushSnapshot *find_diagnostic_push_snapshot(SZrStdioServer *server, const char *uriText) {
    if (server == ZR_NULL || uriText == NULL) {
        return ZR_NULL;
    }
    for (size_t index = 0U; index < server->diagnosticPushCache.count; index++) {
        SZrDiagnosticPushSnapshot *snapshot = &server->diagnosticPushCache.items[index];
        if (snapshot->uriText != ZR_NULL && strcmp(snapshot->uriText, uriText) == 0) {
            return snapshot;
        }
    }
    return ZR_NULL;
}

/** 比较结果 ID 与打开文档版本，抑制对客户端无增量价值的重复 push。 */
static TZrBool diagnostic_push_is_current(SZrStdioServer *server,
                                          const char *uriText,
                                          const char *resultId,
                                          const SZrFileVersion *fileVersion) {
    SZrDiagnosticPushSnapshot *snapshot = find_diagnostic_push_snapshot(server, uriText);
    if (snapshot == ZR_NULL || resultId == ZR_NULL || strcmp(snapshot->resultId, resultId) != 0) {
        return ZR_FALSE;
    }
    if (fileVersion == ZR_NULL || !fileVersion->isOpenDocument) {
        return !snapshot->hasDocumentVersion;
    }
    return snapshot->hasDocumentVersion && snapshot->documentVersion == fileVersion->version;
}

/** 仅在发送成功后保存 push 快照；URI 文本在缓存内独立持有。 */
static TZrBool diagnostic_push_cache_store(SZrStdioServer *server,
                                           const char *uriText,
                                           const char *resultId,
                                           const SZrFileVersion *fileVersion) {
    SZrDiagnosticPushSnapshot *snapshot;

    if (server == ZR_NULL || uriText == NULL || resultId == NULL) {
        return ZR_FALSE;
    }
    snapshot = find_diagnostic_push_snapshot(server, uriText);
    if (snapshot == ZR_NULL) {
        if (server->diagnosticPushCache.count == server->diagnosticPushCache.capacity) {
            size_t newCapacity = server->diagnosticPushCache.capacity == 0U
                                     ? ZR_LSP_ARRAY_INITIAL_CAPACITY
                                     : server->diagnosticPushCache.capacity * ZR_LSP_DYNAMIC_CAPACITY_GROWTH_FACTOR;
            SZrDiagnosticPushSnapshot *items = (SZrDiagnosticPushSnapshot *)realloc(
                    server->diagnosticPushCache.items, newCapacity * sizeof(SZrDiagnosticPushSnapshot));
            if (items == ZR_NULL) {
                return ZR_FALSE;
            }
            memset(&items[server->diagnosticPushCache.capacity],
                   0,
                   (newCapacity - server->diagnosticPushCache.capacity) * sizeof(SZrDiagnosticPushSnapshot));
            server->diagnosticPushCache.items = items;
            server->diagnosticPushCache.capacity = newCapacity;
        }
        snapshot = &server->diagnosticPushCache.items[server->diagnosticPushCache.count];
        snapshot->uriText = duplicate_c_string(uriText);
        if (snapshot->uriText == ZR_NULL) {
            return ZR_FALSE;
        }
        server->diagnosticPushCache.count++;
    }
    (void)snprintf(snapshot->resultId, sizeof(snapshot->resultId), "%s", resultId);
    snapshot->hasDocumentVersion = fileVersion != ZR_NULL && fileVersion->isOpenDocument;
    snapshot->documentVersion = snapshot->hasDocumentVersion ? fileVersion->version : 0U;
    return ZR_TRUE;
}

/** @brief 文档更新尝试后按 parser 当前诊断与版本发送 publishDiagnostics，并缓存已发送快照。
 * BUG: URI 文本转换失败时仍以空 URI 发出诊断，客户端会把结果归到错误文档。
 * BUG: Array_Init 分配失败仍标记有效，后续 GetDiagnostics 可向空缓冲 Push。 */
void publish_diagnostics(SZrStdioServer *server, SZrString *uri) {
    SZrArray diagnostics;
    cJSON *params;
    cJSON *diagnosticsJson;
    char *uriText;
    SZrFileVersion *fileVersion;
    char resultId[ZR_LSP_DIAGNOSTIC_RESULT_ID_MAX];

    /* 内部诊断范围以 UTF-16 为基准，输出前按客户端协商编码转换；版本来自 parser 当前快照。 */
    if (server == ZR_NULL || uri == ZR_NULL) {
        return;
    }

    ZrCore_Array_Init(server->state,
                      &diagnostics,
                      sizeof(SZrLspDiagnostic *),
                      ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(server->state, server->context, uri, &diagnostics)) {
        ZrCore_Array_Free(server->state, &diagnostics);
        return;
    }

    if (!build_diagnostic_result_id(server, uri, &diagnostics, resultId, sizeof(resultId))) {
        free_diagnostics_array(server->state, &diagnostics);
        return;
    }

    params = cJSON_CreateObject();
    uriText = zr_string_to_c_string(uri);
    fileVersion = get_file_version_for_uri(server, uri);
    if (diagnostic_push_is_current(server, uriText, resultId, fileVersion)) {
        free(uriText);
        cJSON_Delete(params);
        free_diagnostics_array(server->state, &diagnostics);
        return;
    }
    diagnosticsJson = serialize_diagnostics_array_for_uri(&diagnostics, uriText);
    apply_position_encoding_to_json_for_uri(server, uriText, diagnosticsJson);
    /* 发帧成功后才推进去重基线；失败时下一次更新仍可重试。 */
    if (params == ZR_NULL || diagnosticsJson == ZR_NULL ||
        cJSON_AddStringToObject(params, ZR_LSP_FIELD_URI,
                                uriText != NULL ? uriText : "") == ZR_NULL ||
        (fileVersion != ZR_NULL &&
         cJSON_AddNumberToObject(params, ZR_LSP_FIELD_VERSION,
                                 (double)fileVersion->version) == ZR_NULL)) {
        cJSON_Delete(params);
        cJSON_Delete(diagnosticsJson);
    } else if (!stdio_json_add_owned_item(params, ZR_LSP_FIELD_DIAGNOSTICS, diagnosticsJson)) {
        cJSON_Delete(params);
    } else if (send_notification(ZR_LSP_METHOD_TEXT_DOCUMENT_PUBLISH_DIAGNOSTICS, params) ==
               ZR_STDIO_SEND_OK) {
        (void)diagnostic_push_cache_store(server, uriText != NULL ? uriText : "", resultId, fileVersion);
    }

    free(uriText);
    free_diagnostics_array(server->state, &diagnostics);
}

/** @brief 关闭或移除文档时推送空诊断，使客户端清除先前发布的结果。
 * BUG: URI 文本转换失败时仍以空 URI 发送清理通知，原 URI 的旧诊断得不到清除。 */
void publish_empty_diagnostics(SZrStdioServer *server, SZrString *uri) {
    cJSON *params;
    cJSON *diagnostics;
    char *uriText;

    if (server == ZR_NULL || uri == ZR_NULL) {
        return;
    }

    params = cJSON_CreateObject();
    diagnostics = cJSON_CreateArray();
    uriText = zr_string_to_c_string(uri);

    if (diagnostic_push_is_current(server, uriText, "", ZR_NULL)) {
        free(uriText);
        cJSON_Delete(params);
        cJSON_Delete(diagnostics);
        return;
    }

    if (params == ZR_NULL || diagnostics == ZR_NULL ||
        cJSON_AddStringToObject(params, ZR_LSP_FIELD_URI,
                                uriText != NULL ? uriText : "") == ZR_NULL) {
        cJSON_Delete(params);
        cJSON_Delete(diagnostics);
    } else if (!stdio_json_add_owned_item(params, ZR_LSP_FIELD_DIAGNOSTICS, diagnostics)) {
        cJSON_Delete(params);
    } else if (send_notification(ZR_LSP_METHOD_TEXT_DOCUMENT_PUBLISH_DIAGNOSTICS, params) ==
               ZR_STDIO_SEND_OK) {
        (void)diagnostic_push_cache_store(server, uriText != NULL ? uriText : "", "", ZR_NULL);
    }

    free(uriText);
}

/** 由共享诊断存储层生成稳定 ID，保持 push 与两种 pull 的快照判等一致。 */
static TZrBool build_diagnostic_result_id(SZrStdioServer *server,
                                          SZrString *uri,
                                          const SZrArray *diagnostics,
                                          char *buffer,
                                          size_t bufferSize) {
    return server != ZR_NULL && ZrLanguageServer_LspDiagnosticStore_BuildResultId(
            server->state, server->context, uri, diagnostics, buffer, (TZrSize)bufferSize);
}

/** 按工作区请求提供的 URI 与值对判断某文件可返回 unchanged 报告。 */
static TZrBool workspace_previous_result_id_matches(const cJSON *previousResultIds,
                                                    const char *uriText,
                                                    const char *resultId) {
    if (!cJSON_IsArray((cJSON *)previousResultIds) || uriText == NULL || resultId == NULL) {
        return ZR_FALSE;
    }

    for (const cJSON *entry = previousResultIds->child; entry != NULL; entry = entry->next) {
        const cJSON *entryUri = get_object_item(entry, ZR_LSP_FIELD_URI);
        const cJSON *entryValue = get_object_item(entry, ZR_LSP_FIELD_VALUE);
        if (cJSON_IsString((cJSON *)entryUri) &&
            cJSON_IsString((cJSON *)entryValue) &&
            strcmp(entryUri->valuestring, uriText) == 0 &&
            strcmp(entryValue->valuestring, resultId) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/** 校验可选字符串字段，避免把无效诊断协议参数当成空值。 */
static TZrBool optional_string_field_is_valid(const cJSON *params, const char *field) {
    const cJSON *value = get_object_item(params, field);

    return value == ZR_NULL ||
           (cJSON_IsString((cJSON *)value) &&
            cJSON_GetStringValue((cJSON *)value) != ZR_NULL);
}

/** 校验工作区增量请求的 previousResultIds 形状，保证逐 URI 比对可用。 */
static TZrBool workspace_previous_result_ids_are_valid(const cJSON *previousResultIds) {
    const cJSON *entry;

    if (previousResultIds == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!cJSON_IsArray((cJSON *)previousResultIds)) {
        return ZR_FALSE;
    }

    cJSON_ArrayForEach(entry, previousResultIds) {
        const cJSON *entryUri;
        const cJSON *entryValue;

        if (!cJSON_IsObject((cJSON *)entry)) {
            return ZR_FALSE;
        }
        entryUri = get_object_item(entry, ZR_LSP_FIELD_URI);
        entryValue = get_object_item(entry, ZR_LSP_FIELD_VALUE);
        if (!cJSON_IsString((cJSON *)entryUri) ||
            !cJSON_IsString((cJSON *)entryValue) ||
            cJSON_GetStringValue((cJSON *)entryUri) == ZR_NULL ||
            cJSON_GetStringValue((cJSON *)entryValue) == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

/** @brief 响应单文档诊断 pull；客户端 ID 匹配时返回 unchanged，否则返回完整诊断。
 * 结果 JSON 交给 handler result，诊断数组在本函数返回前释放。
 * BUG: Array_Init 分配失败仍标记有效，后续 GetDiagnostics 可向空缓冲 Push。 */
SZrLspHandlerResult handle_text_document_diagnostic_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray diagnostics = {0};
    const char *uriText;
    SZrString *uri;
    const cJSON *previousResultIdJson;
    char resultId[ZR_LSP_DIAGNOSTIC_RESULT_ID_MAX];
    cJSON *result;
    TZrBool unchanged;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    previousResultIdJson = get_object_item(params, ZR_LSP_FIELD_PREVIOUS_RESULT_ID);
    if (!optional_string_field_is_valid(params, ZR_LSP_FIELD_PREVIOUS_RESULT_ID)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    ZrCore_Array_Init(server->state, &diagnostics, sizeof(SZrLspDiagnostic *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(server->state, server->context, uri, &diagnostics)) {
        free_diagnostics_array(server->state, &diagnostics);
        return stdio_handler_result_from_json(server->context, ZR_NULL);
    }
    if (!build_diagnostic_result_id(server, uri, &diagnostics, resultId, sizeof(resultId))) {
        free_diagnostics_array(server->state, &diagnostics);
        return stdio_handler_result_from_json(server->context, ZR_NULL);
    }
    unchanged = cJSON_IsString((cJSON *)previousResultIdJson) &&
                strcmp(previousResultIdJson->valuestring, resultId) == 0;
    result = cJSON_CreateObject();
    if (result == NULL ||
        cJSON_AddStringToObject(result, ZR_LSP_FIELD_KIND,
                                unchanged ? ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_UNCHANGED
                                          : ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_FULL) == NULL ||
        cJSON_AddStringToObject(result, ZR_LSP_FIELD_RESULT_ID, resultId) == NULL ||
        (!unchanged && !stdio_json_add_owned_item(result, ZR_LSP_FIELD_ITEMS,
                                                  serialize_diagnostics_array_for_uri(&diagnostics, uriText)))) {
        cJSON_Delete(result);
        result = NULL;
    }
    free_diagnostics_array(server->state, &diagnostics);
    return stdio_handler_result_from_json(server->context, result);
}

/** @brief 为工作区中的单个 URI 构造 full/unchanged 报告，失败时释放其诊断及 JSON。
 * BUG: Array_Init 分配失败仍标记有效，后续 GetDiagnostics 可向空缓冲 Push。 */
static cJSON *serialize_workspace_diagnostic_report_for_uri(SZrStdioServer *server,
                                                            SZrString *uri,
                                                            const cJSON *previousResultIds) {
    SZrArray diagnostics = {0};
    cJSON *report;
    char *uriText;
    char resultId[ZR_LSP_DIAGNOSTIC_RESULT_ID_MAX];
    SZrFileVersion *fileVersion;
    TZrBool unchanged;

    report = cJSON_CreateObject();
    if (report == NULL) {
        return NULL;
    }

    fileVersion = get_file_version_for_uri(server, uri);
    uriText = zr_string_to_c_string(uri);
    if (uriText == NULL || cJSON_AddStringToObject(report, ZR_LSP_FIELD_URI, uriText) == NULL) {
        goto failed;
    }
    if (fileVersion != ZR_NULL && fileVersion->isOpenDocument) {
        if (cJSON_AddNumberToObject(report, ZR_LSP_FIELD_VERSION, (double)fileVersion->version) == NULL) {
            goto failed;
        }
    } else {
        if (cJSON_AddNullToObject(report, ZR_LSP_FIELD_VERSION) == NULL) {
            goto failed;
        }
    }
    ZrCore_Array_Init(server->state, &diagnostics, sizeof(SZrLspDiagnostic *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (uri == ZR_NULL || !ZrLanguageServer_Lsp_GetDiagnostics(server->state, server->context, uri, &diagnostics)) {
        goto failed;
    }
    if (!build_diagnostic_result_id(server, uri, &diagnostics, resultId, sizeof(resultId))) {
        goto failed;
    }
    unchanged = workspace_previous_result_id_matches(previousResultIds, uriText, resultId);
    if (cJSON_AddStringToObject(report, ZR_LSP_FIELD_RESULT_ID, resultId) == NULL ||
        cJSON_AddStringToObject(report, ZR_LSP_FIELD_KIND,
                                unchanged ? ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_UNCHANGED
                                          : ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_FULL) == NULL ||
        (!unchanged && !stdio_json_add_owned_item(report, ZR_LSP_FIELD_ITEMS,
                                                  serialize_diagnostics_array_for_uri(&diagnostics, uriText)))) {
        goto failed;
    }
    free_diagnostics_array(server->state, &diagnostics);
    free(uriText);
    return report;

failed:
    free_diagnostics_array(server->state, &diagnostics);
    free(uriText);
    cJSON_Delete(report);
    return NULL;
}

/** @brief 收集项目与打开文档 URI，逐文件返回工作区诊断，并在枚举中响应取消。
 * TODO: 失同步 overlay 仍可进入此列表并返回旧版本报告；核对协议期望是否应跳过。
 * BUG: Array_Init 分配失败仍标记有效，CollectDiagnosticDocumentUris 可向空缓冲 Push。 */
SZrLspHandlerResult handle_workspace_diagnostic_request(SZrStdioServer *server, const cJSON *params) {
    cJSON *result;
    cJSON *items;
    SZrArray uris = {0};
    const cJSON *previousResultIds;

    if (server == ZR_NULL || params == ZR_NULL || !cJSON_IsObject((cJSON *)params)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    previousResultIds = get_object_item(params, ZR_LSP_FIELD_PREVIOUS_RESULT_IDS);
    if (!optional_string_field_is_valid(params, ZR_LSP_FIELD_IDENTIFIER) ||
        !workspace_previous_result_ids_are_valid(previousResultIds)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    result = cJSON_CreateObject();
    if (result == NULL || (items = cJSON_AddArrayToObject(result, ZR_LSP_FIELD_ITEMS)) == NULL) {
        cJSON_Delete(result);
        return stdio_handler_result_from_json(server->context, ZR_NULL);
    }

    /* URI 集合覆盖工程记录与打开叠层，逐项检查取消以限制大型工作区开销。 */
    if (server != ZR_NULL && server->context != ZR_NULL) {
        ZrCore_Array_Init(server->state, &uris, sizeof(SZrString *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
        if (!ZrLanguageServer_LspProject_CollectDiagnosticDocumentUris(
                    server->state, server->context, &uris)) {
            ZrCore_Array_Free(server->state, &uris);
            cJSON_Delete(result);
            return stdio_handler_result_from_json(server->context, ZR_NULL);
        }
        for (TZrSize index = 0U; index < uris.length; index++) {
            SZrString *const *uri = (SZrString *const *)ZrCore_Array_Get(&uris, index);
            cJSON *report;

            if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(server->context)) {
                ZrCore_Array_Free(server->state, &uris);
                cJSON_Delete(result);
                return stdio_handler_error(ZR_LSP_HANDLER_CANCELLED);
            }
            if (uri == ZR_NULL || *uri == ZR_NULL) {
                continue;
            }
            report = serialize_workspace_diagnostic_report_for_uri(server, *uri, previousResultIds);
            if (!stdio_json_add_owned_array_item(items, report)) {
                ZrCore_Array_Free(server->state, &uris);
                cJSON_Delete(result);
                return stdio_handler_result_from_json(server->context, ZR_NULL);
            }
        }
        ZrCore_Array_Free(server->state, &uris);
    }

    return stdio_handler_result_from_json(server->context, result);
}
