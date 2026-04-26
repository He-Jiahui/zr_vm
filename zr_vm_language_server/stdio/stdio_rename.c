#include "zr_vm_language_server_stdio_internal.h"

static cJSON *create_rename_text_edit(SZrLspRange range, const char *newNameText) {
    cJSON *textEdit = cJSON_CreateObject();

    if (textEdit == NULL) {
        return NULL;
    }

    cJSON_AddItemToObject(textEdit, ZR_LSP_FIELD_RANGE, serialize_range(range));
    cJSON_AddStringToObject(textEdit, ZR_LSP_FIELD_NEW_TEXT, newNameText != NULL ? newNameText : "");
    return textEdit;
}

static cJSON *find_document_change_edits(cJSON *documentChanges, const char *uriText) {
    cJSON *documentChange;

    if (!cJSON_IsArray(documentChanges) || uriText == NULL) {
        return NULL;
    }

    cJSON_ArrayForEach(documentChange, documentChanges) {
        const cJSON *textDocument = get_object_item(documentChange, ZR_LSP_FIELD_TEXT_DOCUMENT);
        const cJSON *uriJson = get_object_item(textDocument, ZR_LSP_FIELD_URI);
        if (cJSON_IsString((cJSON *)uriJson) &&
            uriJson->valuestring != NULL &&
            strcmp(uriJson->valuestring, uriText) == 0) {
            return cJSON_GetObjectItemCaseSensitive(documentChange, ZR_LSP_FIELD_EDITS);
        }
    }

    return NULL;
}

static cJSON *create_document_change(SZrStdioServer *server,
                                     const SZrLspLocation *location,
                                     const char *uriText) {
    cJSON *documentChange = cJSON_CreateObject();
    cJSON *textDocument = cJSON_CreateObject();
    cJSON *edits = cJSON_CreateArray();
    SZrFileVersion *fileVersion;

    if (documentChange == NULL || textDocument == NULL || edits == NULL) {
        cJSON_Delete(documentChange);
        cJSON_Delete(textDocument);
        cJSON_Delete(edits);
        return NULL;
    }

    fileVersion = location != NULL ? get_file_version_for_uri(server, location->uri) : ZR_NULL;
    cJSON_AddStringToObject(textDocument, ZR_LSP_FIELD_URI, uriText != NULL ? uriText : "");
    if (fileVersion != ZR_NULL) {
        cJSON_AddNumberToObject(textDocument, ZR_LSP_FIELD_VERSION, (double)fileVersion->version);
    } else {
        cJSON_AddNullToObject(textDocument, ZR_LSP_FIELD_VERSION);
    }
    cJSON_AddItemToObject(documentChange, ZR_LSP_FIELD_TEXT_DOCUMENT, textDocument);
    cJSON_AddItemToObject(documentChange, ZR_LSP_FIELD_EDITS, edits);
    return documentChange;
}

static cJSON *ensure_document_change_edits(SZrStdioServer *server,
                                           cJSON *documentChanges,
                                           const SZrLspLocation *location,
                                           const char *uriText) {
    cJSON *documentChange;
    cJSON *edits;

    edits = find_document_change_edits(documentChanges, uriText);
    if (edits != NULL) {
        return edits;
    }

    documentChange = create_document_change(server, location, uriText);
    if (documentChange == NULL) {
        return NULL;
    }

    edits = cJSON_GetObjectItemCaseSensitive(documentChange, ZR_LSP_FIELD_EDITS);
    cJSON_AddItemToArray(documentChanges, documentChange);
    return edits;
}

static cJSON *create_workspace_edit(SZrStdioServer *server, SZrArray *locations, SZrString *newName) {
    cJSON *edit = cJSON_CreateObject();
    cJSON *changes = cJSON_CreateObject();
    cJSON *documentChanges = cJSON_CreateArray();
    TZrSize index;
    char *newNameText;

    if (edit == NULL || changes == NULL || documentChanges == NULL) {
        cJSON_Delete(edit);
        cJSON_Delete(changes);
        cJSON_Delete(documentChanges);
        return NULL;
    }

    newNameText = zr_string_to_c_string(newName);
    if (newNameText == NULL) {
        newNameText = duplicate_c_string("");
    }

    for (index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        if (locationPtr != ZR_NULL && *locationPtr != ZR_NULL) {
            char *uriText = zr_string_to_c_string((*locationPtr)->uri);
            cJSON *uriEdits;
            cJSON *documentEdits;
            cJSON *textEdit;

            if (uriText == NULL) {
                continue;
            }

            uriEdits = cJSON_GetObjectItemCaseSensitive(changes, uriText);
            if (uriEdits == NULL) {
                uriEdits = cJSON_CreateArray();
                if (uriEdits != NULL) {
                    cJSON_AddItemToObject(changes, uriText, uriEdits);
                }
            }
            documentEdits = ensure_document_change_edits(server, documentChanges, *locationPtr, uriText);

            textEdit = create_rename_text_edit((*locationPtr)->range, newNameText);
            if (uriEdits != NULL && textEdit != NULL) {
                cJSON_AddItemToArray(uriEdits, textEdit);
            } else {
                cJSON_Delete(textEdit);
            }
            textEdit = create_rename_text_edit((*locationPtr)->range, newNameText);
            if (documentEdits != NULL && textEdit != NULL) {
                cJSON_AddItemToArray(documentEdits, textEdit);
            } else {
                cJSON_Delete(textEdit);
            }

            free(uriText);
        }
    }

    cJSON_AddItemToObject(edit, ZR_LSP_FIELD_CHANGES, changes);
    cJSON_AddItemToObject(edit, ZR_LSP_FIELD_DOCUMENT_CHANGES, documentChanges);
    free(newNameText);
    return edit;
}

cJSON *handle_prepare_rename_request(SZrStdioServer *server, const cJSON *params) {
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    SZrLspRange range;
    SZrString *placeholder = ZR_NULL;
    cJSON *result;
    char *placeholderText;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return NULL;
    }

    if (!ZrLanguageServer_Lsp_PrepareRename(
            server->state,
            server->context,
            uri,
            position,
            &range,
            &placeholder)) {
        return cJSON_CreateNull();
    }

    result = cJSON_CreateObject();
    if (result == NULL) {
        return cJSON_CreateNull();
    }

    placeholderText = zr_string_to_c_string(placeholder);
    cJSON_AddItemToObject(result, ZR_LSP_FIELD_RANGE, serialize_range(range));
    cJSON_AddStringToObject(result, ZR_LSP_FIELD_PLACEHOLDER, placeholderText != NULL ? placeholderText : "");
    free(placeholderText);
    return result;
}

cJSON *handle_rename_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray locations = {0};
    SZrLspPosition position;
    const cJSON *newNameJson;
    const char *newNameText;
    const char *uriText;
    SZrString *uri;
    SZrString *newName;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return NULL;
    }

    newNameJson = get_object_item(params, ZR_LSP_FIELD_NEW_NAME);
    if (!cJSON_IsString((cJSON *)newNameJson)) {
        return NULL;
    }

    newNameText = cJSON_GetStringValue((cJSON *)newNameJson);
    if (newNameText == NULL) {
        return NULL;
    }

    newName = ZrCore_String_Create(server->state, (TZrNativeString)newNameText, (TZrSize)strlen(newNameText));
    if (newName == ZR_NULL) {
        return NULL;
    }

    if (!ZrLanguageServer_Lsp_Rename(
            server->state,
            server->context,
            uri,
            position,
            newName,
            &locations)) {
        return cJSON_CreateNull();
    }

    result = create_workspace_edit(server, &locations, newName);
    free_locations_array(server->state, &locations);
    return result != NULL ? result : cJSON_CreateNull();
}
