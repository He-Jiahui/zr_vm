#include "zr_vm_language_server_stdio_internal.h"

TZrBool ZrLanguageServer_LspProject_RemoveProjectByProjectUri(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrString *uri);
TZrBool ZrLanguageServer_LspProject_RemoveFileRecordByUri(SZrState *state,
                                                          SZrLspContext *context,
                                                          SZrString *uri);
TZrBool ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(SZrState *state,
                                                                     SZrLspContext *context,
                                                                     SZrString *uri);

static cJSON *create_workspace_edit(SZrArray *locations, SZrString *newName) {
    cJSON *edit = cJSON_CreateObject();
    cJSON *changes = cJSON_CreateObject();
    TZrSize index;
    char *newNameText;

    if (edit == NULL || changes == NULL) {
        cJSON_Delete(edit);
        cJSON_Delete(changes);
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

            textEdit = cJSON_CreateObject();
            if (uriEdits != NULL && textEdit != NULL) {
                cJSON_AddItemToObject(textEdit, ZR_LSP_FIELD_RANGE, serialize_range((*locationPtr)->range));
                cJSON_AddStringToObject(textEdit, ZR_LSP_FIELD_NEW_TEXT, newNameText != NULL ? newNameText : "");
                cJSON_AddItemToArray(uriEdits, textEdit);
            } else {
                cJSON_Delete(textEdit);
            }

            free(uriText);
        }
    }

    cJSON_AddItemToObject(edit, ZR_LSP_FIELD_CHANGES, changes);
    free(newNameText);
    return edit;
}

static int handle_did_open(SZrStdioServer *server, const cJSON *params) {
    const cJSON *textDocument;
    const cJSON *textJson;
    const cJSON *versionJson;
    const char *uriText;
    SZrString *uri;
    const char *text;
    TZrSize version;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return 0;
    }

    textDocument = get_object_item(params, ZR_LSP_FIELD_TEXT_DOCUMENT);
    textJson = get_object_item(textDocument, ZR_LSP_FIELD_TEXT);
    if (!cJSON_IsString((cJSON *)textJson)) {
        return 0;
    }

    versionJson = get_object_item(textDocument, ZR_LSP_FIELD_VERSION);
    version = parse_size_value(versionJson, 0);
    text = cJSON_GetStringValue((cJSON *)textJson);
    if (text == NULL) {
        text = "";
    }

    return update_document_contents(server, uri, text, strlen(text), version);
}

static int handle_did_change(SZrStdioServer *server, const cJSON *params) {
    const cJSON *textDocument;
    const cJSON *versionJson;
    const cJSON *changes;
    const char *uriText;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    const char *originalContent;
    size_t originalLength;
    char *updatedContent;
    size_t updatedLength = 0;
    TZrSize version;
    int success;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return 0;
    }

    textDocument = get_object_item(params, ZR_LSP_FIELD_TEXT_DOCUMENT);
    versionJson = get_object_item(textDocument, ZR_LSP_FIELD_VERSION);
    changes = get_object_item(params, ZR_LSP_FIELD_CONTENT_CHANGES);
    if (!cJSON_IsArray((cJSON *)changes)) {
        return 0;
    }

    fileVersion = get_file_version_for_uri(server, uri);
    if (fileVersion != ZR_NULL && fileVersion->content != ZR_NULL) {
        originalContent = fileVersion->content;
        originalLength = (size_t)fileVersion->contentLength;
        version = parse_size_value(versionJson, fileVersion->version + 1);
    } else {
        originalContent = "";
        originalLength = 0;
        version = parse_size_value(versionJson, 0);
    }

    updatedContent = apply_content_changes(uri, originalContent, originalLength, changes, &updatedLength);
    if (updatedContent == NULL) {
        return 0;
    }

    success = update_document_contents(server, uri, updatedContent, updatedLength, version);
    free(updatedContent);
    return success;
}

static int handle_did_close(SZrStdioServer *server, const cJSON *params) {
    const char *uriText;
    SZrString *uri;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return 0;
    }

    publish_empty_diagnostics(server, uri);
    {
        SZrTypeValue key;
        SZrHashKeyValuePair *pair;

        if (server->context != ZR_NULL) {
            if (server->context->parser != ZR_NULL) {
                ZrLanguageServer_IncrementalParser_RemoveFile(server->state, server->context->parser, uri);
            }

            ZrCore_Value_InitAsRawObject(server->state, &key, &uri->super);
            pair = ZrCore_HashSet_Find(server->state, &server->context->uriToAnalyzerMap, &key);
            if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
                SZrSemanticAnalyzer *analyzer = (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
                if (analyzer != ZR_NULL) {
                    ZrLanguageServer_SemanticAnalyzer_Free(server->state, analyzer);
                }
            }
            ZrCore_HashSet_Remove(server->state, &server->context->uriToAnalyzerMap, &key);
        }
    }
    return 1;
}

static int handle_did_save(SZrStdioServer *server, const cJSON *params) {
    const cJSON *textJson;
    const char *uriText;
    SZrString *uri;
    SZrFileVersion *fileVersion;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return 0;
    }

    textJson = get_object_item(params, ZR_LSP_FIELD_TEXT);
    if (cJSON_IsString((cJSON *)textJson)) {
        const char *text = cJSON_GetStringValue((cJSON *)textJson);
        fileVersion = get_file_version_for_uri(server, uri);
        return update_document_contents(
            server,
            uri,
            text != NULL ? text : "",
            text != NULL ? strlen(text) : 0,
            fileVersion != ZR_NULL ? fileVersion->version : 0
        );
    }

    publish_diagnostics(server, uri);
    return 1;
}

static TZrBool watched_string_ends_with(SZrString *value, const TZrChar *suffix) {
    TZrNativeString text;
    TZrSize length;
    TZrSize suffixLength;

    if (value == ZR_NULL || suffix == ZR_NULL) {
        return ZR_FALSE;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        text = ZrCore_String_GetNativeStringShort(value);
        length = value->shortStringLength;
    } else {
        text = ZrCore_String_GetNativeString(value);
        length = value->longStringLength;
    }

    suffixLength = strlen(suffix);
    return text != ZR_NULL && length >= suffixLength &&
           memcmp(text + length - suffixLength, suffix, suffixLength) == 0;
}

static TZrBool watched_uri_has_metadata_extension(SZrString *uri) {
    return watched_string_ends_with(uri, ZR_VM_BINARY_MODULE_FILE_EXTENSION) ||
           watched_string_ends_with(uri, ".dll") ||
           watched_string_ends_with(uri, ".so") ||
           watched_string_ends_with(uri, ".dylib");
}

static int handle_single_watched_file_change(SZrStdioServer *server, SZrString *uri, TZrSize changeType) {
    if (server == ZR_NULL || uri == ZR_NULL) {
        return 0;
    }

    if (changeType == 3) {
        if (watched_string_ends_with(uri, ".zrp")) {
            ZrLanguageServer_LspProject_RemoveProjectByProjectUri(server->state, server->context, uri);
            publish_empty_diagnostics(server, uri);
            return 1;
        }

        if (watched_string_ends_with(uri, ".zr")) {
            ZrLanguageServer_LspProject_RemoveFileRecordByUri(server->state, server->context, uri);
            publish_empty_diagnostics(server, uri);
            return 1;
        }

        if (watched_uri_has_metadata_extension(uri)) {
            return ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(server->state,
                                                                                server->context,
                                                                                uri)
                       ? 1
                       : 0;
        }

        return 1;
    }

    if (watched_string_ends_with(uri, ".zrp") || watched_string_ends_with(uri, ".zr")) {
        return update_document_contents_from_disk(server, uri);
    }

    if (watched_uri_has_metadata_extension(uri)) {
        return ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(server->state,
                                                                            server->context,
                                                                            uri)
                   ? 1
                   : 0;
    }

    return 1;
}

static int handle_did_change_watched_files(SZrStdioServer *server, const cJSON *params) {
    const cJSON *changes;
    int handledAny = 0;

    if (server == ZR_NULL || params == NULL) {
        return 0;
    }

    changes = get_object_item(params, ZR_LSP_FIELD_CHANGES);
    if (!cJSON_IsArray((cJSON *)changes)) {
        return 0;
    }

    for (int index = 0; index < cJSON_GetArraySize((cJSON *)changes); index++) {
        const cJSON *change = cJSON_GetArrayItem((cJSON *)changes, index);
        const cJSON *uriJson = get_object_item(change, ZR_LSP_FIELD_URI);
        const cJSON *typeJson = get_object_item(change, ZR_LSP_FIELD_TYPE);
        const char *uriText;
        SZrString *uri;
        TZrSize changeType;

        if (!cJSON_IsString((cJSON *)uriJson) || !cJSON_IsNumber((cJSON *)typeJson)) {
            continue;
        }

        uriText = cJSON_GetStringValue((cJSON *)uriJson);
        if (uriText == NULL) {
            continue;
        }

        uri = server_get_cached_uri(server, uriText);
        if (uri == ZR_NULL) {
            continue;
        }

        changeType = parse_size_value(typeJson, 0);
        if (changeType == 0) {
            continue;
        }

        handledAny = handle_single_watched_file_change(server, uri, changeType) || handledAny;
    }

    return handledAny;
}

static cJSON *handle_completion_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray completions = {0};
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return NULL;
    }

    if (!ZrLanguageServer_Lsp_GetCompletion(server->state, server->context, uri, position, &completions)) {
        return cJSON_CreateArray();
    }

    result = serialize_completion_items_array(&completions);
    free_completion_items_array(server->state, &completions);
    return result;
}

static cJSON *handle_hover_request(SZrStdioServer *server, const cJSON *params) {
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    SZrLspHover *hover = ZR_NULL;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return NULL;
    }

    if (!ZrLanguageServer_Lsp_GetHover(server->state, server->context, uri, position, &hover) || hover == ZR_NULL) {
        return cJSON_CreateNull();
    }

    result = serialize_hover(hover);
    free_hover(server->state, hover);
    return result != NULL ? result : cJSON_CreateNull();
}

static cJSON *handle_signature_help_request(SZrStdioServer *server, const cJSON *params) {
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    SZrLspSignatureHelp *help = ZR_NULL;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return NULL;
    }

    if (!ZrLanguageServer_Lsp_GetSignatureHelp(server->state, server->context, uri, position, &help) ||
        help == ZR_NULL) {
        return cJSON_CreateNull();
    }

    result = serialize_signature_help(help);
    free_signature_help(server->state, help);
    return result != NULL ? result : cJSON_CreateNull();
}

static cJSON *handle_definition_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray locations = {0};
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return NULL;
    }

    if (!ZrLanguageServer_Lsp_GetDefinition(server->state, server->context, uri, position, &locations)) {
        return cJSON_CreateArray();
    }

    result = serialize_locations_array(&locations);
    free_locations_array(server->state, &locations);
    return result;
}

static cJSON *handle_references_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray locations = {0};
    SZrLspPosition position;
    const cJSON *contextJson;
    const cJSON *includeDeclarationJson;
    const char *uriText;
    SZrString *uri;
    TZrBool includeDeclaration = ZR_FALSE;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return NULL;
    }

    contextJson = get_object_item(params, ZR_LSP_FIELD_CONTEXT);
    includeDeclarationJson = get_object_item(contextJson, ZR_LSP_FIELD_INCLUDE_DECLARATION);
    if (cJSON_IsBool((cJSON *)includeDeclarationJson)) {
        includeDeclaration = cJSON_IsTrue((cJSON *)includeDeclarationJson) ? ZR_TRUE : ZR_FALSE;
    }

    if (!ZrLanguageServer_Lsp_FindReferences(
            server->state,
            server->context,
            uri,
            position,
            includeDeclaration,
            &locations)) {
        return cJSON_CreateArray();
    }

    result = serialize_locations_array(&locations);
    free_locations_array(server->state, &locations);
    return result;
}

static cJSON *handle_document_symbols_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray symbols = {0};
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return NULL;
    }

    if (!ZrLanguageServer_Lsp_GetDocumentSymbols(server->state, server->context, uri, &symbols)) {
        return cJSON_CreateArray();
    }

    result = serialize_symbols_array(&symbols);
    free_symbols_array(server->state, &symbols);
    return result;
}

static cJSON *handle_workspace_symbols_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray symbols = {0};
    const cJSON *queryJson;
    const char *queryText = "";
    SZrString *query;
    cJSON *result;

    if (server == ZR_NULL || params == NULL) {
        return NULL;
    }

    queryJson = get_object_item(params, ZR_LSP_FIELD_QUERY);
    if (cJSON_IsString((cJSON *)queryJson)) {
        const char *text = cJSON_GetStringValue((cJSON *)queryJson);
        if (text != NULL) {
            queryText = text;
        }
    }

    query = ZrCore_String_Create(server->state, (TZrNativeString)queryText, (TZrSize)strlen(queryText));
    if (query == ZR_NULL) {
        return NULL;
    }

    if (!ZrLanguageServer_Lsp_GetWorkspaceSymbols(server->state, server->context, query, &symbols)) {
        return cJSON_CreateArray();
    }

    result = serialize_symbols_array(&symbols);
    free_symbols_array(server->state, &symbols);
    return result;
}

static cJSON *handle_document_highlights_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray highlights = {0};
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return NULL;
    }

    if (!ZrLanguageServer_Lsp_GetDocumentHighlights(server->state, server->context, uri, position, &highlights)) {
        return cJSON_CreateArray();
    }

    result = serialize_highlights_array(&highlights);
    free_highlights_array(server->state, &highlights);
    return result;
}

static cJSON *create_semantic_token_legend_json(void) {
    cJSON *legend = cJSON_CreateObject();
    cJSON *types = cJSON_CreateArray();
    cJSON *modifiers = cJSON_CreateArray();

    if (legend == NULL || types == NULL || modifiers == NULL) {
        cJSON_Delete(legend);
        cJSON_Delete(types);
        cJSON_Delete(modifiers);
        return NULL;
    }

    for (TZrSize index = 0; index < ZrLanguageServer_Lsp_SemanticTokenTypeCount(); index++) {
        const TZrChar *typeName = ZrLanguageServer_Lsp_SemanticTokenTypeName(index);
        if (typeName != ZR_NULL) {
            cJSON_AddItemToArray(types, cJSON_CreateString(typeName));
        }
    }

    cJSON_AddItemToObject(legend, ZR_LSP_FIELD_TOKEN_TYPES, types);
    cJSON_AddItemToObject(legend, ZR_LSP_FIELD_TOKEN_MODIFIERS, modifiers);
    return legend;
}

static cJSON *serialize_semantic_tokens_result(SZrArray *tokens) {
    cJSON *result = cJSON_CreateObject();
    cJSON *data = cJSON_CreateArray();

    if (result == NULL || data == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(data);
        return NULL;
    }

    for (TZrSize index = 0; tokens != ZR_NULL && index < tokens->length; index++) {
        TZrUInt32 *valuePtr = (TZrUInt32 *)ZrCore_Array_Get(tokens, index);
        if (valuePtr != ZR_NULL) {
            cJSON_AddItemToArray(data, cJSON_CreateNumber((double)(*valuePtr)));
        }
    }

    cJSON_AddItemToObject(result, ZR_LSP_FIELD_DATA, data);
    return result;
}

static cJSON *handle_semantic_tokens_full_request(SZrStdioServer *server, const cJSON *params) {
    const char *uriText;
    SZrString *uri;
    SZrArray tokens = {0};
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return NULL;
    }

    ZrCore_Array_Init(server->state, &tokens, sizeof(TZrUInt32), ZR_LSP_SEMANTIC_TOKEN_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetSemanticTokens(server->state, server->context, uri, &tokens)) {
        ZrCore_Array_Free(server->state, &tokens);
        return cJSON_CreateNull();
    }

    result = serialize_semantic_tokens_result(&tokens);
    ZrCore_Array_Free(server->state, &tokens);
    return result != NULL ? result : cJSON_CreateNull();
}

static cJSON *handle_prepare_rename_request(SZrStdioServer *server, const cJSON *params) {
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

static cJSON *handle_rename_request(SZrStdioServer *server, const cJSON *params) {
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

    result = create_workspace_edit(&locations, newName);
    free_locations_array(server->state, &locations);
    return result != NULL ? result : cJSON_CreateNull();
}

static cJSON *handle_initialize_request(void) {
    cJSON *result = cJSON_CreateObject();
    cJSON *capabilities = cJSON_CreateObject();
    cJSON *textDocumentSync = cJSON_CreateObject();
    cJSON *completionProvider = cJSON_CreateObject();
    cJSON *signatureHelpProvider = cJSON_CreateObject();
    cJSON *renameProvider = cJSON_CreateObject();
    cJSON *saveOptions = cJSON_CreateObject();
    cJSON *triggerCharacters = cJSON_CreateArray();
    cJSON *signatureTriggerCharacters = cJSON_CreateArray();
    cJSON *semanticTokensProvider = cJSON_CreateObject();
    cJSON *serverInfo = cJSON_CreateObject();
    cJSON *workspace = cJSON_CreateObject();
    cJSON *workspaceFolders = cJSON_CreateObject();
    cJSON *semanticLegend;

    if (result == NULL || capabilities == NULL || textDocumentSync == NULL ||
        completionProvider == NULL || signatureHelpProvider == NULL || renameProvider == NULL || saveOptions == NULL ||
        triggerCharacters == NULL || signatureTriggerCharacters == NULL || semanticTokensProvider == NULL ||
        serverInfo == NULL || workspace == NULL || workspaceFolders == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(capabilities);
        cJSON_Delete(textDocumentSync);
        cJSON_Delete(completionProvider);
        cJSON_Delete(signatureHelpProvider);
        cJSON_Delete(renameProvider);
        cJSON_Delete(saveOptions);
        cJSON_Delete(triggerCharacters);
        cJSON_Delete(signatureTriggerCharacters);
        cJSON_Delete(semanticTokensProvider);
        cJSON_Delete(serverInfo);
        cJSON_Delete(workspace);
        cJSON_Delete(workspaceFolders);
        return NULL;
    }

    semanticLegend = create_semantic_token_legend_json();
    if (semanticLegend == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(capabilities);
        cJSON_Delete(textDocumentSync);
        cJSON_Delete(completionProvider);
        cJSON_Delete(signatureHelpProvider);
        cJSON_Delete(renameProvider);
        cJSON_Delete(saveOptions);
        cJSON_Delete(triggerCharacters);
        cJSON_Delete(signatureTriggerCharacters);
        cJSON_Delete(semanticTokensProvider);
        cJSON_Delete(serverInfo);
        cJSON_Delete(workspace);
        cJSON_Delete(workspaceFolders);
        return NULL;
    }

    cJSON_AddBoolToObject(textDocumentSync, ZR_LSP_FIELD_OPEN_CLOSE, 1);
    cJSON_AddNumberToObject(textDocumentSync, ZR_LSP_FIELD_CHANGE, ZR_LSP_TEXT_DOCUMENT_SYNC_KIND_INCREMENTAL);
    cJSON_AddBoolToObject(saveOptions, ZR_LSP_FIELD_INCLUDE_TEXT, 0);
    cJSON_AddItemToObject(textDocumentSync, ZR_LSP_FIELD_SAVE, saveOptions);

    cJSON_AddItemToArray(
        triggerCharacters,
        cJSON_CreateString(ZR_LSP_COMPLETION_TRIGGER_CHARACTER_MEMBER_ACCESS)
    );
    cJSON_AddItemToArray(
        triggerCharacters,
        cJSON_CreateString(ZR_LSP_COMPLETION_TRIGGER_CHARACTER_NAMESPACE_ACCESS)
    );
    cJSON_AddBoolToObject(completionProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER, 0);
    cJSON_AddItemToObject(completionProvider, ZR_LSP_FIELD_TRIGGER_CHARACTERS, triggerCharacters);
    cJSON_AddItemToArray(
        signatureTriggerCharacters,
        cJSON_CreateString(ZR_LSP_SIGNATURE_TRIGGER_CHARACTER_OPEN_PAREN)
    );
    cJSON_AddItemToArray(
        signatureTriggerCharacters,
        cJSON_CreateString(ZR_LSP_SIGNATURE_TRIGGER_CHARACTER_ARGUMENT_SEPARATOR)
    );
    cJSON_AddItemToObject(signatureHelpProvider, ZR_LSP_FIELD_TRIGGER_CHARACTERS, signatureTriggerCharacters);

    cJSON_AddBoolToObject(renameProvider, ZR_LSP_FIELD_PREPARE_PROVIDER, 1);

    cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_TEXT_DOCUMENT_SYNC, textDocumentSync);
    cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_COMPLETION_PROVIDER, completionProvider);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_HOVER_PROVIDER, 1);
    cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_SIGNATURE_HELP_PROVIDER, signatureHelpProvider);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_DEFINITION_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_REFERENCES_PROVIDER, 1);
    cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_RENAME_PROVIDER, renameProvider);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_DOCUMENT_SYMBOL_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_WORKSPACE_SYMBOL_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_DOCUMENT_HIGHLIGHT_PROVIDER, 1);
    cJSON_AddItemToObject(semanticTokensProvider, ZR_LSP_FIELD_LEGEND, semanticLegend);
    cJSON_AddBoolToObject(semanticTokensProvider, ZR_LSP_FIELD_FULL, 1);
    cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_SEMANTIC_TOKENS_PROVIDER, semanticTokensProvider);
    cJSON_AddBoolToObject(workspaceFolders, ZR_LSP_FIELD_SUPPORTED, 1);
    cJSON_AddBoolToObject(workspaceFolders, ZR_LSP_FIELD_CHANGE_NOTIFICATIONS, 1);
    cJSON_AddItemToObject(workspace, ZR_LSP_FIELD_WORKSPACE_FOLDERS, workspaceFolders);
    cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_WORKSPACE, workspace);

    cJSON_AddStringToObject(serverInfo, ZR_LSP_FIELD_NAME, ZR_LSP_SERVER_NAME);
    cJSON_AddStringToObject(serverInfo, ZR_LSP_FIELD_VERSION, ZR_LSP_SERVER_VERSION);

    cJSON_AddItemToObject(result, ZR_LSP_FIELD_CAPABILITIES, capabilities);
    cJSON_AddItemToObject(result, ZR_LSP_FIELD_SERVER_INFO, serverInfo);
    return result;
}

void handle_request_message(SZrStdioServer *server,
                            const cJSON *id,
                            const char *method,
                            const cJSON *params) {
    cJSON *result = NULL;

    if (server == ZR_NULL || id == NULL || method == NULL) {
        return;
    }

    if (strcmp(method, ZR_LSP_METHOD_INITIALIZE) == 0) {
        result = handle_initialize_request();
        send_result_response(id, result != NULL ? result : cJSON_CreateNull());
        return;
    }

    if (strcmp(method, ZR_LSP_METHOD_SHUTDOWN) == 0) {
        server->shutdownRequested = ZR_TRUE;
        send_result_response(id, NULL);
        return;
    }

    if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_COMPLETION) == 0) {
        result = handle_completion_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_HOVER) == 0) {
        result = handle_hover_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_SIGNATURE_HELP) == 0) {
        result = handle_signature_help_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DEFINITION) == 0) {
        result = handle_definition_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_REFERENCES) == 0) {
        result = handle_references_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_SYMBOL) == 0) {
        result = handle_document_symbols_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_SYMBOL) == 0) {
        result = handle_workspace_symbols_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_HIGHLIGHT) == 0) {
        result = handle_document_highlights_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_SEMANTIC_TOKENS_FULL) == 0) {
        result = handle_semantic_tokens_full_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_PREPARE_RENAME) == 0) {
        result = handle_prepare_rename_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_RENAME) == 0) {
        result = handle_rename_request(server, params);
    } else {
        send_error_response(id, ZR_LSP_JSON_RPC_METHOD_NOT_FOUND_CODE, "Method not found");
        return;
    }

    if (result == NULL) {
        send_error_response(id, ZR_LSP_JSON_RPC_INVALID_PARAMS_CODE, "Invalid params");
    } else {
        send_result_response(id, result);
    }
}

void handle_notification_message(SZrStdioServer *server,
                                 const char *method,
                                 const cJSON *params,
                                 int *outShouldExit,
                                 int *outExitCode) {
    if (outShouldExit != NULL) {
        *outShouldExit = 0;
    }
    if (outExitCode != NULL) {
        *outExitCode = 0;
    }

    if (server == ZR_NULL || method == NULL) {
        return;
    }

    if (strcmp(method, ZR_LSP_METHOD_INITIALIZED) == 0 ||
        strcmp(method, ZR_LSP_METHOD_WORKSPACE_DID_CHANGE_CONFIGURATION) == 0 ||
        strcmp(method, ZR_LSP_METHOD_WORKSPACE_DID_CHANGE_WORKSPACE_FOLDERS) == 0 ||
        strcmp(method, ZR_LSP_METHOD_CANCEL_REQUEST) == 0) {
        return;
    }

    if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DID_OPEN) == 0) {
        handle_did_open(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DID_CHANGE) == 0) {
        handle_did_change(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DID_CLOSE) == 0) {
        handle_did_close(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DID_SAVE) == 0) {
        handle_did_save(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_DID_CHANGE_WATCHED_FILES) == 0) {
        handle_did_change_watched_files(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_EXIT) == 0) {
        if (outShouldExit != NULL) {
            *outShouldExit = 1;
        }
        if (outExitCode != NULL) {
            *outExitCode = server->shutdownRequested ? 0 : 1;
        }
    }
}
