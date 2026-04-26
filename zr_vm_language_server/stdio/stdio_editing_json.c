#include "zr_vm_language_server_stdio_internal.h"

cJSON *serialize_text_edit(const SZrLspTextEdit *edit) {
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

cJSON *serialize_text_edits_array(SZrArray *edits) {
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

static cJSON *serialize_versioned_document_change(const char *uriText,
                                                  TZrSize version,
                                                  SZrArray *edits) {
    cJSON *documentChange = cJSON_CreateObject();
    cJSON *textDocument = cJSON_CreateObject();

    if (documentChange == NULL || textDocument == NULL) {
        cJSON_Delete(documentChange);
        cJSON_Delete(textDocument);
        return NULL;
    }

    cJSON_AddStringToObject(textDocument, ZR_LSP_FIELD_URI, uriText != NULL ? uriText : "");
    cJSON_AddNumberToObject(textDocument, ZR_LSP_FIELD_VERSION, (double)version);
    cJSON_AddItemToObject(documentChange, ZR_LSP_FIELD_TEXT_DOCUMENT, textDocument);
    cJSON_AddItemToObject(documentChange, ZR_LSP_FIELD_EDITS, serialize_text_edits_array(edits));
    return documentChange;
}

static cJSON *serialize_workspace_edit(const char *uriText,
                                       SZrArray *edits,
                                       TZrBool hasVersion,
                                       TZrSize version) {
    cJSON *json = cJSON_CreateObject();
    cJSON *changes = cJSON_CreateObject();

    if (json == NULL || changes == NULL) {
        cJSON_Delete(json);
        cJSON_Delete(changes);
        return NULL;
    }

    cJSON_AddItemToObject(changes, uriText != NULL ? uriText : "", serialize_text_edits_array(edits));
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_CHANGES, changes);
    if (hasVersion) {
        cJSON *documentChanges = cJSON_CreateArray();
        if (documentChanges != NULL) {
            cJSON *documentChange = serialize_versioned_document_change(uriText, version, edits);
            if (documentChange != NULL) {
                cJSON_AddItemToArray(documentChanges, documentChange);
            }
            cJSON_AddItemToObject(json, ZR_LSP_FIELD_DOCUMENT_CHANGES, documentChanges);
        }
    }
    return json;
}

static cJSON *serialize_code_action(const char *uriText,
                                    TZrBool hasVersion,
                                    TZrSize version,
                                    const SZrLspCodeAction *action) {
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
    {
        cJSON *data = cJSON_CreateObject();
        if (data != NULL) {
            cJSON_AddStringToObject(data, ZR_LSP_FIELD_URI, uriText != NULL ? uriText : "");
            cJSON_AddStringToObject(data, ZR_LSP_FIELD_TITLE, titleText != NULL ? titleText : "");
            if (kindText != NULL) {
                cJSON_AddStringToObject(data, ZR_LSP_FIELD_KIND, kindText);
            }
            cJSON_AddBoolToObject(data, ZR_LSP_FIELD_IS_PREFERRED, action->isPreferred ? 1 : 0);
            cJSON_AddItemToObject(json, ZR_LSP_FIELD_DATA, data);
        }
    }
    cJSON_AddItemToObject(json,
                          ZR_LSP_FIELD_EDIT,
                          serialize_workspace_edit(uriText, (SZrArray *)&action->edits, hasVersion, version));

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

cJSON *serialize_code_actions_array(const char *uriText,
                                    TZrBool hasVersion,
                                    TZrSize version,
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
            cJSON_AddItemToArray(json, serialize_code_action(uriText, hasVersion, version, *actionPtr));
        }
    }

    return json;
}
