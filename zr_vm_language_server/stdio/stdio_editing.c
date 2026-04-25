#include "zr_vm_language_server_stdio_internal.h"

static cJSON *serialize_text_edit(const SZrLspTextEdit *edit) {
    cJSON *json;
    char *text;

    if (edit == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(edit->range));
    text = zr_string_to_c_string(edit->newText);
    cJSON_AddStringToObject(json, ZR_LSP_FIELD_NEW_TEXT, text != NULL ? text : "");
    free(text);
    return json;
}

static cJSON *serialize_text_edits_array(SZrArray *edits) {
    cJSON *json = cJSON_CreateArray();

    if (json == NULL || edits == NULL) {
        return json;
    }

    for (TZrSize index = 0; index < edits->length; index++) {
        SZrLspTextEdit **editPtr = (SZrLspTextEdit **)ZrCore_Array_Get(edits, index);
        if (editPtr != ZR_NULL && *editPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_text_edit(*editPtr));
        }
    }

    return json;
}

static cJSON *serialize_workspace_edit(const char *uriText, SZrArray *edits) {
    cJSON *json = cJSON_CreateObject();
    cJSON *changes = cJSON_CreateObject();

    if (json == NULL || changes == NULL) {
        cJSON_Delete(json);
        cJSON_Delete(changes);
        return NULL;
    }

    cJSON_AddItemToObject(changes, uriText != NULL ? uriText : "", serialize_text_edits_array(edits));
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_CHANGES, changes);
    return json;
}

static cJSON *serialize_code_action(const char *uriText, const SZrLspCodeAction *action) {
    cJSON *json;
    char *titleText;
    char *kindText;

    if (action == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    titleText = zr_string_to_c_string(action->title);
    kindText = zr_string_to_c_string(action->kind);
    cJSON_AddStringToObject(json, ZR_LSP_FIELD_TITLE, titleText != NULL ? titleText : "");
    if (kindText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_KIND, kindText);
    }
    cJSON_AddBoolToObject(json, ZR_LSP_FIELD_IS_PREFERRED, action->isPreferred ? 1 : 0);
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_EDIT, serialize_workspace_edit(uriText, (SZrArray *)&action->edits));

    free(titleText);
    free(kindText);
    return json;
}

static int code_action_kind_matches_filter(const char *actionKind, const char *requestedKind) {
    size_t requestedLength;

    if (actionKind == NULL || requestedKind == NULL) {
        return 0;
    }
    if (strcmp(actionKind, requestedKind) == 0) {
        return 1;
    }

    requestedLength = strlen(requestedKind);
    return strncmp(actionKind, requestedKind, requestedLength) == 0 &&
           actionKind[requestedLength] == '.';
}

static int code_action_allowed_by_context_only(const SZrLspCodeAction *action, const cJSON *params) {
    const cJSON *context;
    const cJSON *only;
    const cJSON *requestedKind;
    char *kindText;
    int allowed = 0;

    context = get_object_item(params, ZR_LSP_FIELD_CONTEXT);
    only = get_object_item(context, ZR_LSP_FIELD_ONLY);
    if (!cJSON_IsArray(only) || cJSON_GetArraySize(only) == 0) {
        return 1;
    }

    kindText = zr_string_to_c_string(action != NULL ? action->kind : ZR_NULL);
    cJSON_ArrayForEach(requestedKind, only) {
        if (cJSON_IsString(requestedKind) &&
            code_action_kind_matches_filter(kindText, requestedKind->valuestring)) {
            allowed = 1;
            break;
        }
    }
    free(kindText);
    return allowed;
}

static cJSON *serialize_code_actions_array(const char *uriText,
                                           SZrArray *actions,
                                           const cJSON *params) {
    cJSON *json = cJSON_CreateArray();

    if (json == NULL || actions == NULL) {
        return json;
    }

    for (TZrSize index = 0; index < actions->length; index++) {
        SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(actions, index);
        if (actionPtr != ZR_NULL &&
            *actionPtr != ZR_NULL &&
            code_action_allowed_by_context_only(*actionPtr, params)) {
            cJSON_AddItemToArray(json, serialize_code_action(uriText, *actionPtr));
        }
    }

    return json;
}

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

    result = serialize_code_actions_array(uriText, &actions, params);
    ZrLanguageServer_Lsp_FreeCodeActions(server->state, &actions);
    return result;
}

cJSON *handle_code_action_resolve_request(SZrStdioServer *server, const cJSON *params) {
    ZR_UNUSED_PARAMETER(server);
    return params != NULL ? cJSON_Duplicate((cJSON *)params, 1) : cJSON_CreateObject();
}
