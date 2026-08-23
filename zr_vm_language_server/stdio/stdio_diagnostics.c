#include "zr_vm_language_server_stdio_internal.h"
#include "zr_vm_language_server/lsp_diagnostic_store.h"
#include "project/lsp_project_internal.h"

static TZrBool build_diagnostic_result_id(SZrStdioServer *server,
                                          SZrString *uri,
                                          const SZrArray *diagnostics,
                                          char *buffer,
                                          size_t bufferSize);

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

void free_diagnostic_push_cache(SZrDiagnosticPushCache *cache) {
    if (cache == ZR_NULL) {
        return;
    }
    for (size_t index = 0U; index < cache->count; index++) {
        free(cache->items[index].uriText);
        cache->items[index].uriText = ZR_NULL;
        cache->items[index].resultId[0] = '\0';
    }
    free(cache->items);
    memset(cache, 0, sizeof(*cache));
}

void publish_diagnostics(SZrStdioServer *server, SZrString *uri) {
    SZrArray diagnostics;
    cJSON *params;
    cJSON *diagnosticsJson;
    char *uriText;
    SZrFileVersion *fileVersion;
    char resultId[ZR_LSP_DIAGNOSTIC_RESULT_ID_MAX];

    /*
     * Diagnostics ranges use LSP UTF-16 code units; ZrLanguageServer_Lsp_GetDiagnostics must agree with
     * the same fileVersion->version that the client last sent on didChange/didOpen.
     */
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
    if (params != NULL) {
        cJSON_AddStringToObject(params, ZR_LSP_FIELD_URI, uriText != NULL ? uriText : "");
        if (fileVersion != ZR_NULL) {
            cJSON_AddNumberToObject(params, ZR_LSP_FIELD_VERSION, (double)fileVersion->version);
        }
        cJSON_AddItemToObject(params, ZR_LSP_FIELD_DIAGNOSTICS, diagnosticsJson);
        send_notification(ZR_LSP_METHOD_TEXT_DOCUMENT_PUBLISH_DIAGNOSTICS, params);
        (void)diagnostic_push_cache_store(server, uriText != NULL ? uriText : "", resultId, fileVersion);
    } else {
        cJSON_Delete(diagnosticsJson);
    }

    free(uriText);
    free_diagnostics_array(server->state, &diagnostics);
}

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

    if (params != NULL && diagnostics != NULL) {
        cJSON_AddStringToObject(params, ZR_LSP_FIELD_URI, uriText != NULL ? uriText : "");
        cJSON_AddItemToObject(params, ZR_LSP_FIELD_DIAGNOSTICS, diagnostics);
        send_notification(ZR_LSP_METHOD_TEXT_DOCUMENT_PUBLISH_DIAGNOSTICS, params);
        (void)diagnostic_push_cache_store(server, uriText != NULL ? uriText : "", "", ZR_NULL);
    } else {
        cJSON_Delete(params);
        cJSON_Delete(diagnostics);
    }

    free(uriText);
}

static TZrBool build_diagnostic_result_id(SZrStdioServer *server,
                                          SZrString *uri,
                                          const SZrArray *diagnostics,
                                          char *buffer,
                                          size_t bufferSize) {
    return server != ZR_NULL && ZrLanguageServer_LspDiagnosticStore_BuildResultId(
            server->state, server->context, uri, diagnostics, buffer, (TZrSize)bufferSize);
}

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

cJSON *handle_text_document_diagnostic_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray diagnostics = {0};
    const char *uriText;
    SZrString *uri;
    const cJSON *previousResultIdJson;
    char resultId[ZR_LSP_DIAGNOSTIC_RESULT_ID_MAX];
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return ZR_NULL;
    }

    ZR_UNUSED_PARAMETER(uriText);
    ZrCore_Array_Init(server->state, &diagnostics, sizeof(SZrLspDiagnostic *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(server->state, server->context, uri, &diagnostics)) {
        free_diagnostics_array(server->state, &diagnostics);
        return ZR_NULL;
    }
    if (!build_diagnostic_result_id(server, uri, &diagnostics, resultId, sizeof(resultId))) {
        free_diagnostics_array(server->state, &diagnostics);
        return ZR_NULL;
    }
    previousResultIdJson = get_object_item(params, ZR_LSP_FIELD_PREVIOUS_RESULT_ID);
    if (cJSON_IsString((cJSON *)previousResultIdJson) &&
        strcmp(previousResultIdJson->valuestring, resultId) == 0) {
        free_diagnostics_array(server->state, &diagnostics);
        result = cJSON_CreateObject();
        if (result != NULL) {
            cJSON_AddStringToObject(result,
                                    ZR_LSP_FIELD_KIND,
                                    ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_UNCHANGED);
            cJSON_AddStringToObject(result, ZR_LSP_FIELD_RESULT_ID, resultId);
        }
        return result;
    }

    result = cJSON_CreateObject();
    if (result != NULL) {
        cJSON_AddStringToObject(result, ZR_LSP_FIELD_KIND, ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_FULL);
        cJSON_AddStringToObject(result, ZR_LSP_FIELD_RESULT_ID, resultId);
        cJSON_AddItemToObject(result, ZR_LSP_FIELD_ITEMS, serialize_diagnostics_array_for_uri(&diagnostics, uriText));
    }
    free_diagnostics_array(server->state, &diagnostics);
    return result;
}

static cJSON *serialize_workspace_diagnostic_report_for_uri(SZrStdioServer *server,
                                                            SZrString *uri,
                                                            const cJSON *previousResultIds) {
    SZrArray diagnostics = {0};
    cJSON *report;
    char *uriText;
    char resultId[ZR_LSP_DIAGNOSTIC_RESULT_ID_MAX];
    SZrFileVersion *fileVersion;

    report = cJSON_CreateObject();
    if (report == NULL) {
        return NULL;
    }

    fileVersion = get_file_version_for_uri(server, uri);
    uriText = zr_string_to_c_string(uri);
    cJSON_AddStringToObject(report, ZR_LSP_FIELD_URI, uriText != NULL ? uriText : "");
    if (fileVersion != ZR_NULL && fileVersion->isOpenDocument) {
        cJSON_AddNumberToObject(report, ZR_LSP_FIELD_VERSION, (double)fileVersion->version);
    } else {
        cJSON_AddNullToObject(report, ZR_LSP_FIELD_VERSION);
    }
    ZrCore_Array_Init(server->state, &diagnostics, sizeof(SZrLspDiagnostic *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (uri == ZR_NULL || !ZrLanguageServer_Lsp_GetDiagnostics(server->state, server->context, uri, &diagnostics)) {
        free_diagnostics_array(server->state, &diagnostics);
        free(uriText);
        cJSON_Delete(report);
        return ZR_NULL;
    }
    if (!build_diagnostic_result_id(server, uri, &diagnostics, resultId, sizeof(resultId))) {
        free_diagnostics_array(server->state, &diagnostics);
        free(uriText);
        cJSON_Delete(report);
        return ZR_NULL;
    }
    cJSON_AddStringToObject(report, ZR_LSP_FIELD_RESULT_ID, resultId);
    if (workspace_previous_result_id_matches(previousResultIds, uriText, resultId)) {
        cJSON_AddStringToObject(report,
                                ZR_LSP_FIELD_KIND,
                                ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_UNCHANGED);
        free_diagnostics_array(server->state, &diagnostics);
        free(uriText);
        return report;
    }
    cJSON_AddStringToObject(report, ZR_LSP_FIELD_KIND, ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_FULL);

    cJSON_AddItemToObject(report, ZR_LSP_FIELD_ITEMS, serialize_diagnostics_array_for_uri(&diagnostics, uriText));
    free_diagnostics_array(server->state, &diagnostics);
    free(uriText);
    return report;
}

cJSON *handle_workspace_diagnostic_request(SZrStdioServer *server, const cJSON *params) {
    cJSON *result;
    cJSON *items;
    SZrArray uris = {0};
    const cJSON *previousResultIds = get_object_item(params, ZR_LSP_FIELD_PREVIOUS_RESULT_IDS);

    result = cJSON_CreateObject();
    items = cJSON_CreateArray();
    if (result == NULL || items == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(items);
        return NULL;
    }

    if (server != ZR_NULL && server->context != ZR_NULL) {
        ZrCore_Array_Init(server->state, &uris, sizeof(SZrString *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
        if (!ZrLanguageServer_LspProject_CollectDiagnosticDocumentUris(
                    server->state, server->context, &uris)) {
            ZrCore_Array_Free(server->state, &uris);
            cJSON_Delete(result);
            cJSON_Delete(items);
            return NULL;
        }
        for (TZrSize index = 0U; index < uris.length; index++) {
            SZrString *const *uri = (SZrString *const *)ZrCore_Array_Get(&uris, index);
            cJSON *report;

            if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(server->context)) {
                ZrCore_Array_Free(server->state, &uris);
                cJSON_Delete(result);
                cJSON_Delete(items);
                return NULL;
            }
            if (uri == ZR_NULL || *uri == ZR_NULL) {
                continue;
            }
            report = serialize_workspace_diagnostic_report_for_uri(server, *uri, previousResultIds);
            if (report != NULL) {
                cJSON_AddItemToArray(items, report);
            }
        }
        ZrCore_Array_Free(server->state, &uris);
    }

    cJSON_AddItemToObject(result, ZR_LSP_FIELD_ITEMS, items);
    return result;
}
