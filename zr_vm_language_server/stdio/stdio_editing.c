#include "zr_vm_language_server_stdio_internal.h"

cJSON *handle_formatting_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray edits = {0};
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return cJSON_CreateArray();
    }

    ZrCore_Array_Init(server->state, &edits, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetFormatting(server->state, server->context, uri, &edits)) {
        ZrLanguageServer_Lsp_FreeTextEdits(server->state, &edits);
        return cJSON_CreateArray();
    }

    ZR_UNUSED_PARAMETER(uriText);
    result = serialize_text_edits_array(&edits);
    ZrLanguageServer_Lsp_FreeTextEdits(server->state, &edits);
    return result;
}

cJSON *handle_range_formatting_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray edits = {0};
    SZrLspRange range;
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri) ||
        !parse_range(get_object_item(params, ZR_LSP_FIELD_RANGE), &range)) {
        return cJSON_CreateArray();
    }

    ZrCore_Array_Init(server->state, &edits, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetRangeFormatting(server->state, server->context, uri, range, &edits)) {
        ZrLanguageServer_Lsp_FreeTextEdits(server->state, &edits);
        return cJSON_CreateArray();
    }

    ZR_UNUSED_PARAMETER(uriText);
    result = serialize_text_edits_array(&edits);
    ZrLanguageServer_Lsp_FreeTextEdits(server->state, &edits);
    return result;
}

cJSON *handle_ranges_formatting_request(SZrStdioServer *server, const cJSON *params) {
    const cJSON *rangesJson;
    const cJSON *rangeJson;
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return cJSON_CreateArray();
    }
    rangesJson = get_object_item(params, ZR_LSP_FIELD_RANGES);
    if (!cJSON_IsArray(rangesJson)) {
        return cJSON_CreateArray();
    }

    result = cJSON_CreateArray();
    if (result == NULL) {
        return NULL;
    }

    cJSON_ArrayForEach(rangeJson, rangesJson) {
        SZrArray edits = {0};
        SZrLspRange range;

        if (!parse_range(rangeJson, &range)) {
            continue;
        }

        ZrCore_Array_Init(server->state, &edits, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
        if (ZrLanguageServer_Lsp_GetRangeFormatting(server->state, server->context, uri, range, &edits)) {
            for (TZrSize index = 0; index < edits.length; index++) {
                SZrLspTextEdit **editPtr = (SZrLspTextEdit **)ZrCore_Array_Get(&edits, index);
                if (editPtr != ZR_NULL && *editPtr != ZR_NULL) {
                    cJSON_AddItemToArray(result, serialize_text_edit(*editPtr));
                }
            }
        }
        ZrLanguageServer_Lsp_FreeTextEdits(server->state, &edits);
    }

    ZR_UNUSED_PARAMETER(uriText);
    return result;
}

cJSON *handle_on_type_formatting_request(SZrStdioServer *server, const cJSON *params) {
    const cJSON *chJson;
    SZrArray edits = {0};
    const char *uriText;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspRange range;
    cJSON *result;

    chJson = get_object_item(params, ZR_LSP_FIELD_CH);
    if (!cJSON_IsString(chJson) ||
        (strcmp(chJson->valuestring, "}") != 0 && strcmp(chJson->valuestring, ";") != 0) ||
        !parse_position(get_object_item(params, ZR_LSP_FIELD_POSITION), &position) ||
        !get_uri_from_text_document(server, params, &uriText, &uri)) {
        return cJSON_CreateArray();
    }

    ZR_UNUSED_PARAMETER(uriText);
    range.start = position;
    range.start.character = 0;
    range.end = position;

    ZrCore_Array_Init(server->state, &edits, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetRangeFormatting(server->state, server->context, uri, range, &edits)) {
        ZrLanguageServer_Lsp_FreeTextEdits(server->state, &edits);
        return cJSON_CreateArray();
    }

    result = serialize_text_edits_array(&edits);
    ZrLanguageServer_Lsp_FreeTextEdits(server->state, &edits);
    return result;
}

cJSON *handle_code_action_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray actions = {0};
    SZrLspRange range = {{0, 0}, {0, 0}};
    const char *uriText;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return cJSON_CreateArray();
    }
    parse_range(get_object_item(params, ZR_LSP_FIELD_RANGE), &range);

    ZrCore_Array_Init(server->state, &actions, sizeof(SZrLspCodeAction *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetCodeActions(server->state, server->context, uri, range, &actions)) {
        ZrLanguageServer_Lsp_FreeCodeActions(server->state, &actions);
        return cJSON_CreateArray();
    }

    fileVersion = get_file_version_for_uri(server, uri);
    result = serialize_code_actions_array(uriText,
                                          fileVersion != ZR_NULL ? ZR_TRUE : ZR_FALSE,
                                          fileVersion != ZR_NULL ? fileVersion->version : 0,
                                          &actions,
                                          params);
    ZrLanguageServer_Lsp_FreeCodeActions(server->state, &actions);
    return result;
}

cJSON *handle_code_action_resolve_request(SZrStdioServer *server, const cJSON *params) {
    ZR_UNUSED_PARAMETER(server);
    return params != NULL ? cJSON_Duplicate((cJSON *)params, 1) : cJSON_CreateObject();
}
