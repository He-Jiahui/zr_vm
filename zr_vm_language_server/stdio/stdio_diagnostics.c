#include "zr_vm_language_server_stdio_internal.h"

void publish_diagnostics(SZrStdioServer *server, SZrString *uri) {
    SZrArray diagnostics;
    cJSON *params;
    cJSON *diagnosticsJson;
    char *uriText;
    SZrFileVersion *fileVersion;

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

    params = cJSON_CreateObject();
    uriText = zr_string_to_c_string(uri);
    diagnosticsJson = serialize_diagnostics_array_for_uri(&diagnostics, uriText);
    fileVersion = get_file_version_for_uri(server, uri);

    if (params != NULL) {
        cJSON_AddStringToObject(params, ZR_LSP_FIELD_URI, uriText != NULL ? uriText : "");
        if (fileVersion != ZR_NULL) {
            cJSON_AddNumberToObject(params, ZR_LSP_FIELD_VERSION, (double)fileVersion->version);
        }
        cJSON_AddItemToObject(params, ZR_LSP_FIELD_DIAGNOSTICS, diagnosticsJson);
        send_notification(ZR_LSP_METHOD_TEXT_DOCUMENT_PUBLISH_DIAGNOSTICS, params);
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

    if (params != NULL && diagnostics != NULL) {
        cJSON_AddStringToObject(params, ZR_LSP_FIELD_URI, uriText != NULL ? uriText : "");
        cJSON_AddItemToObject(params, ZR_LSP_FIELD_DIAGNOSTICS, diagnostics);
        send_notification(ZR_LSP_METHOD_TEXT_DOCUMENT_PUBLISH_DIAGNOSTICS, params);
    } else {
        cJSON_Delete(params);
        cJSON_Delete(diagnostics);
    }

    free(uriText);
}

static void build_diagnostic_result_id(SZrStdioServer *server,
                                       SZrString *uri,
                                       char *buffer,
                                       size_t bufferSize) {
    SZrFileVersion *fileVersion;
    const TZrChar *content;
    unsigned long long hash = 1469598103934665603ULL;
    TZrSize version = 0;
    TZrSize length = 0;

    if (buffer == NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    fileVersion = get_file_version_for_uri(server, uri);
    if (fileVersion != ZR_NULL) {
        content = fileVersion->content;
        version = fileVersion->version;
        length = fileVersion->contentLength;
        if (content != ZR_NULL) {
            for (TZrSize index = 0; index < length; index++) {
                hash ^= (unsigned char)content[index];
                hash *= 1099511628211ULL;
            }
        }
    }

    snprintf(buffer,
             bufferSize,
             "zr:%llu:%llu:%llx",
             (unsigned long long)version,
             (unsigned long long)length,
             hash);
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
    char resultId[96];
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        result = cJSON_CreateObject();
        if (result != NULL) {
            cJSON_AddStringToObject(result, ZR_LSP_FIELD_KIND, ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_FULL);
            cJSON_AddItemToObject(result, ZR_LSP_FIELD_ITEMS, cJSON_CreateArray());
        }
        return result;
    }

    ZR_UNUSED_PARAMETER(uriText);
    build_diagnostic_result_id(server, uri, resultId, sizeof(resultId));
    previousResultIdJson = get_object_item(params, ZR_LSP_FIELD_PREVIOUS_RESULT_ID);
    if (cJSON_IsString((cJSON *)previousResultIdJson) &&
        strcmp(previousResultIdJson->valuestring, resultId) == 0) {
        result = cJSON_CreateObject();
        if (result != NULL) {
            cJSON_AddStringToObject(result,
                                    ZR_LSP_FIELD_KIND,
                                    ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_UNCHANGED);
            cJSON_AddStringToObject(result, ZR_LSP_FIELD_RESULT_ID, resultId);
        }
        return result;
    }

    ZrCore_Array_Init(server->state, &diagnostics, sizeof(SZrLspDiagnostic *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(server->state, server->context, uri, &diagnostics)) {
        free_diagnostics_array(server->state, &diagnostics);
        result = cJSON_CreateObject();
        if (result != NULL) {
            cJSON_AddStringToObject(result, ZR_LSP_FIELD_KIND, ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_FULL);
            cJSON_AddStringToObject(result, ZR_LSP_FIELD_RESULT_ID, resultId);
            cJSON_AddItemToObject(result, ZR_LSP_FIELD_ITEMS, cJSON_CreateArray());
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
    char resultId[96];
    SZrFileVersion *fileVersion;

    report = cJSON_CreateObject();
    if (report == NULL) {
        return NULL;
    }

    fileVersion = get_file_version_for_uri(server, uri);
    uriText = zr_string_to_c_string(uri);
    build_diagnostic_result_id(server, uri, resultId, sizeof(resultId));
    cJSON_AddStringToObject(report, ZR_LSP_FIELD_URI, uriText != NULL ? uriText : "");
    if (fileVersion != ZR_NULL) {
        cJSON_AddNumberToObject(report, ZR_LSP_FIELD_VERSION, (double)fileVersion->version);
    } else {
        cJSON_AddNullToObject(report, ZR_LSP_FIELD_VERSION);
    }
    cJSON_AddStringToObject(report, ZR_LSP_FIELD_RESULT_ID, resultId);
    if (workspace_previous_result_id_matches(previousResultIds, uriText, resultId)) {
        cJSON_AddStringToObject(report,
                                ZR_LSP_FIELD_KIND,
                                ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_UNCHANGED);
        free(uriText);
        return report;
    }
    cJSON_AddStringToObject(report, ZR_LSP_FIELD_KIND, ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_FULL);

    ZrCore_Array_Init(server->state, &diagnostics, sizeof(SZrLspDiagnostic *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (uri == ZR_NULL || !ZrLanguageServer_Lsp_GetDiagnostics(server->state, server->context, uri, &diagnostics)) {
        free_diagnostics_array(server->state, &diagnostics);
        cJSON_AddItemToObject(report, ZR_LSP_FIELD_ITEMS, cJSON_CreateArray());
        free(uriText);
        return report;
    }

    cJSON_AddItemToObject(report, ZR_LSP_FIELD_ITEMS, serialize_diagnostics_array_for_uri(&diagnostics, uriText));
    free_diagnostics_array(server->state, &diagnostics);
    free(uriText);
    return report;
}

cJSON *handle_workspace_diagnostic_request(SZrStdioServer *server, const cJSON *params) {
    cJSON *result;
    cJSON *items;
    SZrHashSet *fileMap;
    const cJSON *previousResultIds = get_object_item(params, ZR_LSP_FIELD_PREVIOUS_RESULT_IDS);

    result = cJSON_CreateObject();
    items = cJSON_CreateArray();
    if (result == NULL || items == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(items);
        return NULL;
    }

    if (server != ZR_NULL &&
        server->context != ZR_NULL &&
        server->context->parser != ZR_NULL) {
        fileMap = &server->context->parser->uriToFileMap;
        if (fileMap->isValid && fileMap->buckets != ZR_NULL) {
            for (TZrSize bucketIndex = 0; bucketIndex < fileMap->capacity; bucketIndex++) {
                SZrHashKeyValuePair *pair = fileMap->buckets[bucketIndex];
                while (pair != ZR_NULL) {
                    if (pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
                        SZrFileVersion *fileVersion =
                                (SZrFileVersion *)pair->value.value.nativeObject.nativePointer;
                        if (fileVersion != ZR_NULL && fileVersion->uri != ZR_NULL) {
                            cJSON *report =
                                    serialize_workspace_diagnostic_report_for_uri(server,
                                                                                  fileVersion->uri,
                                                                                  previousResultIds);
                            if (report != NULL) {
                                cJSON_AddItemToArray(items, report);
                            }
                        }
                    }
                    pair = pair->next;
                }
            }
        }
    }

    cJSON_AddItemToObject(result, ZR_LSP_FIELD_ITEMS, items);
    return result;
}
