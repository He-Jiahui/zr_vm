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
                                     const char *uriText,
                                     const SZrArray *documentSnapshots) {
    cJSON *documentChange = cJSON_CreateObject();
    cJSON *textDocument = cJSON_CreateObject();
    cJSON *edits = cJSON_CreateArray();
    SZrFileVersion *fileVersion;
    const SZrLspWorkspaceEditDocumentSnapshot *documentSnapshot = ZR_NULL;

    if (documentChange == NULL || textDocument == NULL || edits == NULL) {
        cJSON_Delete(documentChange);
        cJSON_Delete(textDocument);
        cJSON_Delete(edits);
        return NULL;
    }

    if (documentSnapshots != ZR_NULL) {
        documentSnapshot =
                location != ZR_NULL
                        ? ZrLanguageServer_LspWorkspaceEdit_FindDocumentSnapshot(
                                  documentSnapshots, location->uri)
                        : ZR_NULL;
        if (documentSnapshot == ZR_NULL) {
            cJSON_Delete(documentChange);
            cJSON_Delete(textDocument);
            cJSON_Delete(edits);
            return NULL;
        }
    }
    fileVersion = documentSnapshots == ZR_NULL && location != NULL
                          ? get_file_version_for_uri(server, location->uri)
                          : ZR_NULL;
    cJSON_AddStringToObject(textDocument, ZR_LSP_FIELD_URI, uriText != NULL ? uriText : "");
    if (documentSnapshot != ZR_NULL && documentSnapshot->isOpenDocument) {
        cJSON_AddNumberToObject(textDocument,
                                ZR_LSP_FIELD_VERSION,
                                (double)documentSnapshot->version);
    } else if (documentSnapshots == ZR_NULL && fileVersion != ZR_NULL) {
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
                                           const char *uriText,
                                           const SZrArray *documentSnapshots) {
    cJSON *documentChange;
    cJSON *edits;

    edits = find_document_change_edits(documentChanges, uriText);
    if (edits != NULL) {
        return edits;
    }

    documentChange = create_document_change(
            server, location, uriText, documentSnapshots);
    if (documentChange == NULL) {
        return NULL;
    }

    edits = cJSON_GetObjectItemCaseSensitive(documentChange, ZR_LSP_FIELD_EDITS);
    cJSON_AddItemToArray(documentChanges, documentChange);
    return edits;
}

TZrBool append_workspace_edit_locations(SZrStdioServer *server,
                                        cJSON *edit,
                                        SZrArray *locations,
                                        SZrString *newName,
                                        const SZrArray *documentSnapshots) {
    cJSON *documentChanges;
    TZrSize index;
    char *newNameText;

    if (server == ZR_NULL || edit == NULL || locations == ZR_NULL ||
        newName == ZR_NULL) {
        return ZR_FALSE;
    }

    documentChanges = cJSON_GetObjectItemCaseSensitive(
            edit, ZR_LSP_FIELD_DOCUMENT_CHANGES);
    if (!cJSON_IsArray(documentChanges)) {
        return ZR_FALSE;
    }
    newNameText = zr_string_to_c_string(newName);
    if (newNameText == NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        if (locationPtr != ZR_NULL && *locationPtr != ZR_NULL) {
            char *uriText = zr_string_to_c_string((*locationPtr)->uri);
            cJSON *documentEdits;
            cJSON *textEdit;

            if (uriText == NULL) {
                free(newNameText);
                return ZR_FALSE;
            }

            documentEdits = ensure_document_change_edits(server,
                                                          documentChanges,
                                                          *locationPtr,
                                                          uriText,
                                                          documentSnapshots);

            if (documentEdits == NULL) {
                free(uriText);
                free(newNameText);
                return ZR_FALSE;
            }

            textEdit = create_rename_text_edit((*locationPtr)->range, newNameText);
            if (textEdit == NULL) {
                free(uriText);
                free(newNameText);
                return ZR_FALSE;
            }
            cJSON_AddItemToArray(documentEdits, textEdit);

            free(uriText);
        }
    }

    free(newNameText);
    return ZR_TRUE;
}

cJSON *create_workspace_edit_for_locations(SZrStdioServer *server,
                                           SZrArray *locations,
                                           SZrString *newName,
                                           const SZrArray *documentSnapshots) {
    cJSON *edit = cJSON_CreateObject();
    cJSON *documentChanges = cJSON_CreateArray();

    if (edit == NULL || documentChanges == NULL) {
        cJSON_Delete(edit);
        cJSON_Delete(documentChanges);
        return NULL;
    }

    cJSON_AddItemToObject(edit, ZR_LSP_FIELD_DOCUMENT_CHANGES, documentChanges);
    if (!append_workspace_edit_locations(
                server, edit, locations, newName, documentSnapshots)) {
        cJSON_Delete(edit);
        return NULL;
    }
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
    SZrArray documentSnapshots = {0};
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

    if (!ZrLanguageServer_LspWorkspaceEdit_CaptureDocumentSnapshots(
                server->state,
                server->context,
                &locations,
                &documentSnapshots) ||
        !ZrLanguageServer_LspWorkspaceEdit_ValidateDocumentSnapshots(
                server->state,
                server->context,
                &documentSnapshots)) {
        free_locations_array(server->state, &locations);
        if (documentSnapshots.isValid) {
            ZrCore_Array_Free(server->state, &documentSnapshots);
        }
        return cJSON_CreateNull();
    }

    result = create_workspace_edit_for_locations(
            server, &locations, newName, &documentSnapshots);
    free_locations_array(server->state, &locations);
    ZrCore_Array_Free(server->state, &documentSnapshots);
    return result != NULL ? result : cJSON_CreateNull();
}
