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

static cJSON *serialize_project_module_summary(const SZrLspProjectModuleSummary *summary) {
    cJSON *json;
    char *moduleNameText;
    char *displayNameText;
    char *descriptionText;
    char *navigationUriText;

    if (summary == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_SOURCE_KIND, summary->sourceKind);
    cJSON_AddBoolToObject(json, ZR_LSP_FIELD_IS_ENTRY, summary->isEntry ? 1 : 0);
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(summary->range));

    moduleNameText = zr_string_to_c_string(summary->moduleName);
    displayNameText = zr_string_to_c_string(summary->displayName);
    descriptionText = zr_string_to_c_string(summary->description);
    navigationUriText = zr_string_to_c_string(summary->navigationUri);

    if (moduleNameText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_MODULE_NAME, moduleNameText);
    }
    if (displayNameText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_DISPLAY_NAME, displayNameText);
    }
    if (descriptionText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_DESCRIPTION, descriptionText);
    }
    if (navigationUriText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_NAVIGATION_URI, navigationUriText);
    }

    free(moduleNameText);
    free(displayNameText);
    free(descriptionText);
    free(navigationUriText);
    return json;
}

static cJSON *serialize_project_modules_array(SZrArray *modules) {
    cJSON *json = cJSON_CreateArray();

    if (json == NULL || modules == ZR_NULL) {
        return json;
    }

    for (TZrSize index = 0; index < modules->length; index++) {
        SZrLspProjectModuleSummary **summaryPtr =
            (SZrLspProjectModuleSummary **)ZrCore_Array_Get(modules, index);
        if (summaryPtr != ZR_NULL && *summaryPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_project_module_summary(*summaryPtr));
        }
    }

    return json;
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

cJSON *handle_completion_request(SZrStdioServer *server, const cJSON *params) {
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
    if (cJSON_IsArray(result)) {
        int count = cJSON_GetArraySize(result);
        for (int index = 0; index < count; index++) {
            cJSON *item = cJSON_GetArrayItem(result, index);
            cJSON *data;

            if (!cJSON_IsObject(item)) {
                continue;
            }

            data = cJSON_CreateObject();
            if (data == NULL) {
                continue;
            }
            cJSON_AddStringToObject(data, ZR_LSP_FIELD_URI, uriText);
            cJSON_AddItemToObject(data, ZR_LSP_FIELD_POSITION, serialize_position(position));
            cJSON_AddItemToObject(item, ZR_LSP_FIELD_DATA, data);
        }
    }
    free_completion_items_array(server->state, &completions);
    return result;
}

cJSON *handle_hover_request(SZrStdioServer *server, const cJSON *params) {
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

cJSON *handle_rich_hover_request(SZrStdioServer *server, const cJSON *params) {
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    SZrLspRichHover *hover = ZR_NULL;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return NULL;
    }

    if (!ZrLanguageServer_Lsp_GetRichHover(server->state, server->context, uri, position, &hover) ||
        hover == ZR_NULL) {
        return cJSON_CreateNull();
    }

    result = serialize_rich_hover(hover);
    free_rich_hover(server->state, hover);
    return result != NULL ? result : cJSON_CreateNull();
}

cJSON *handle_signature_help_request(SZrStdioServer *server, const cJSON *params) {
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

cJSON *handle_inlay_hint_request(SZrStdioServer *server, const cJSON *params) {
    const cJSON *rangeJson;
    const char *uriText;
    SZrString *uri;
    SZrLspRange range;
    SZrArray hints = {0};
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return NULL;
    }

    rangeJson = get_object_item(params, ZR_LSP_FIELD_RANGE);
    if (!parse_range(rangeJson, &range)) {
        return NULL;
    }

    if (!ZrLanguageServer_Lsp_GetInlayHints(server->state, server->context, uri, range, &hints)) {
        return cJSON_CreateArray();
    }

    result = serialize_inlay_hints_array(&hints);
    free_inlay_hints_array(server->state, &hints);
    return result;
}

cJSON *handle_inlay_hint_resolve_request(SZrStdioServer *server, const cJSON *params) {
    ZR_UNUSED_PARAMETER(server);
    return params != NULL ? cJSON_Duplicate((cJSON *)params, 1) : cJSON_CreateObject();
}

cJSON *handle_definition_request(SZrStdioServer *server, const cJSON *params) {
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

cJSON *handle_native_declaration_document_request(SZrStdioServer *server, const cJSON *params) {
    const cJSON *uriJson;
    const char *uriText;
    SZrString *uri;
    SZrString *documentText = ZR_NULL;
    char *renderedText;
    cJSON *result;

    if (server == ZR_NULL || params == NULL) {
        return NULL;
    }

    uriJson = get_object_item(params, ZR_LSP_FIELD_URI);
    if (!cJSON_IsString((cJSON *)uriJson)) {
        return NULL;
    }

    uriText = cJSON_GetStringValue((cJSON *)uriJson);
    if (uriText == NULL) {
        return NULL;
    }

    uri = server_get_cached_uri(server, uriText);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_GetNativeDeclarationDocument(server->state, server->context, uri, &documentText) ||
        documentText == ZR_NULL) {
        return cJSON_CreateNull();
    }

    renderedText = zr_string_to_c_string(documentText);
    result = renderedText != NULL ? cJSON_CreateString(renderedText) : cJSON_CreateString("");
    free(renderedText);
    return result;
}

cJSON *handle_project_modules_request(SZrStdioServer *server, const cJSON *params) {
    const cJSON *uriJson;
    const char *uriText;
    SZrString *projectUri;
    SZrArray modules = {0};
    cJSON *result;

    if (server == ZR_NULL || params == NULL) {
        return NULL;
    }

    uriJson = get_object_item(params, ZR_LSP_FIELD_URI);
    if (!cJSON_IsString((cJSON *)uriJson)) {
        return NULL;
    }

    uriText = cJSON_GetStringValue((cJSON *)uriJson);
    if (uriText == NULL) {
        return NULL;
    }

    projectUri = server_get_cached_uri(server, uriText);
    if (projectUri == ZR_NULL) {
        return cJSON_CreateArray();
    }

    ZrCore_Array_Init(server->state, &modules, sizeof(SZrLspProjectModuleSummary *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetProjectModules(server->state, server->context, projectUri, &modules)) {
        ZrLanguageServer_Lsp_FreeProjectModules(server->state, &modules);
        return cJSON_CreateArray();
    }

    result = serialize_project_modules_array(&modules);
    ZrLanguageServer_Lsp_FreeProjectModules(server->state, &modules);
    return result;
}

cJSON *handle_references_request(SZrStdioServer *server, const cJSON *params) {
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

cJSON *handle_document_symbols_request(SZrStdioServer *server, const cJSON *params) {
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

cJSON *handle_workspace_symbols_request(SZrStdioServer *server, const cJSON *params) {
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

cJSON *handle_workspace_symbol_resolve_request(SZrStdioServer *server, const cJSON *params) {
    ZR_UNUSED_PARAMETER(server);
    return params != NULL ? cJSON_Duplicate((cJSON *)params, 1) : cJSON_CreateObject();
}

cJSON *handle_document_highlights_request(SZrStdioServer *server, const cJSON *params) {
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

    result = create_workspace_edit(&locations, newName);
    free_locations_array(server->state, &locations);
    return result != NULL ? result : cJSON_CreateNull();
}

static void apply_initialization_selected_project(SZrStdioServer *server, const cJSON *params) {
    const cJSON *initializationOptions;
    const cJSON *uriJson;
    const char *uriText;
    SZrString *cachedUri;

    if (server == ZR_NULL || server->context == ZR_NULL || params == ZR_NULL) {
        return;
    }

    initializationOptions = get_object_item(params, ZR_LSP_FIELD_INITIALIZATION_OPTIONS);
    if (initializationOptions == ZR_NULL) {
        return;
    }

    uriJson = get_object_item(initializationOptions, ZR_LSP_INITIALIZATION_OPTION_SELECTED_PROJECT_URI);
    if (uriJson == ZR_NULL) {
        return;
    }

    if (cJSON_IsNull((cJSON *)uriJson)) {
        ZrLanguageServer_LspContext_SetClientSelectedZrpUri(server->state, server->context, ZR_NULL);
        return;
    }

    if (!cJSON_IsString((cJSON *)uriJson)) {
        return;
    }

    uriText = cJSON_GetStringValue((cJSON *)uriJson);
    if (uriText == ZR_NULL || uriText[0] == '\0') {
        ZrLanguageServer_LspContext_SetClientSelectedZrpUri(server->state, server->context, ZR_NULL);
        return;
    }

    cachedUri = server_get_cached_uri(server, uriText);
    if (cachedUri == ZR_NULL) {
        return;
    }

    ZrLanguageServer_LspContext_SetClientSelectedZrpUri(server->state, server->context, cachedUri);
}

static void handle_zr_selected_project_notification(SZrStdioServer *server, const cJSON *params) {
    const cJSON *uriJson;
    const char *uriText;
    SZrString *cachedUri;

    if (server == ZR_NULL || server->context == ZR_NULL || params == ZR_NULL) {
        return;
    }

    uriJson = get_object_item(params, ZR_LSP_FIELD_URI);
    if (cJSON_IsNull((cJSON *)uriJson)) {
        ZrLanguageServer_LspContext_SetClientSelectedZrpUri(server->state, server->context, ZR_NULL);
        return;
    }

    if (!cJSON_IsString((cJSON *)uriJson)) {
        return;
    }

    uriText = cJSON_GetStringValue((cJSON *)uriJson);
    if (uriText == ZR_NULL || uriText[0] == '\0') {
        ZrLanguageServer_LspContext_SetClientSelectedZrpUri(server->state, server->context, ZR_NULL);
        return;
    }

    cachedUri = server_get_cached_uri(server, uriText);
    if (cachedUri == ZR_NULL) {
        return;
    }

    ZrLanguageServer_LspContext_SetClientSelectedZrpUri(server->state, server->context, cachedUri);
}

static cJSON *handle_initialize_request(SZrStdioServer *server, const cJSON *params) {
    cJSON *result = cJSON_CreateObject();
    cJSON *capabilities = cJSON_CreateObject();
    cJSON *textDocumentSync = cJSON_CreateObject();
    cJSON *completionProvider = cJSON_CreateObject();
    cJSON *signatureHelpProvider = cJSON_CreateObject();
    cJSON *renameProvider = cJSON_CreateObject();
    cJSON *workspaceSymbolProvider = cJSON_CreateObject();
    cJSON *inlayHintProvider = cJSON_CreateObject();
    cJSON *saveOptions = cJSON_CreateObject();
    cJSON *triggerCharacters = cJSON_CreateArray();
    cJSON *signatureTriggerCharacters = cJSON_CreateArray();
    cJSON *semanticTokensProvider = cJSON_CreateObject();
    cJSON *semanticTokensFullProvider = cJSON_CreateObject();
    cJSON *serverInfo = cJSON_CreateObject();
    cJSON *workspace = cJSON_CreateObject();
    cJSON *workspaceFolders = cJSON_CreateObject();
    cJSON *semanticLegend;

    if (result == NULL || capabilities == NULL || textDocumentSync == NULL ||
        completionProvider == NULL || signatureHelpProvider == NULL || renameProvider == NULL || saveOptions == NULL ||
        workspaceSymbolProvider == NULL || inlayHintProvider == NULL || triggerCharacters == NULL ||
        signatureTriggerCharacters == NULL || semanticTokensProvider == NULL || semanticTokensFullProvider == NULL ||
        serverInfo == NULL || workspace == NULL || workspaceFolders == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(capabilities);
        cJSON_Delete(textDocumentSync);
        cJSON_Delete(completionProvider);
        cJSON_Delete(signatureHelpProvider);
        cJSON_Delete(renameProvider);
        cJSON_Delete(workspaceSymbolProvider);
        cJSON_Delete(inlayHintProvider);
        cJSON_Delete(saveOptions);
        cJSON_Delete(triggerCharacters);
        cJSON_Delete(signatureTriggerCharacters);
        cJSON_Delete(semanticTokensProvider);
        cJSON_Delete(semanticTokensFullProvider);
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
        cJSON_Delete(workspaceSymbolProvider);
        cJSON_Delete(inlayHintProvider);
        cJSON_Delete(saveOptions);
        cJSON_Delete(triggerCharacters);
        cJSON_Delete(signatureTriggerCharacters);
        cJSON_Delete(semanticTokensProvider);
        cJSON_Delete(semanticTokensFullProvider);
        cJSON_Delete(serverInfo);
        cJSON_Delete(workspace);
        cJSON_Delete(workspaceFolders);
        return NULL;
    }

    cJSON_AddBoolToObject(textDocumentSync, ZR_LSP_FIELD_OPEN_CLOSE, 1);
    cJSON_AddNumberToObject(textDocumentSync, ZR_LSP_FIELD_CHANGE, ZR_LSP_TEXT_DOCUMENT_SYNC_KIND_INCREMENTAL);
    cJSON_AddBoolToObject(textDocumentSync, ZR_LSP_FIELD_WILL_SAVE, 1);
    cJSON_AddBoolToObject(textDocumentSync, ZR_LSP_FIELD_WILL_SAVE_WAIT_UNTIL, 1);
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
    cJSON_AddBoolToObject(completionProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER, 1);
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
    cJSON_AddBoolToObject(workspaceSymbolProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER, 1);
    cJSON_AddBoolToObject(inlayHintProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER, 1);

    cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_TEXT_DOCUMENT_SYNC, textDocumentSync);
    cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_COMPLETION_PROVIDER, completionProvider);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_HOVER_PROVIDER, 1);
    cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_SIGNATURE_HELP_PROVIDER, signatureHelpProvider);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_DEFINITION_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_REFERENCES_PROVIDER, 1);
    cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_RENAME_PROVIDER, renameProvider);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_DOCUMENT_SYMBOL_PROVIDER, 1);
    cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_WORKSPACE_SYMBOL_PROVIDER, workspaceSymbolProvider);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_DOCUMENT_HIGHLIGHT_PROVIDER, 1);
    cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_INLAY_HINT_PROVIDER, inlayHintProvider);
    cJSON_AddItemToObject(semanticTokensProvider, ZR_LSP_FIELD_LEGEND, semanticLegend);
    cJSON_AddBoolToObject(semanticTokensFullProvider, ZR_LSP_FIELD_DELTA, 1);
    cJSON_AddItemToObject(semanticTokensProvider, ZR_LSP_FIELD_FULL, semanticTokensFullProvider);
    cJSON_AddBoolToObject(semanticTokensProvider, ZR_LSP_FIELD_RANGE, 1);
    cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_SEMANTIC_TOKENS_PROVIDER, semanticTokensProvider);
    add_advanced_editor_capabilities(capabilities);
    cJSON_AddBoolToObject(workspaceFolders, ZR_LSP_FIELD_SUPPORTED, 1);
    cJSON_AddBoolToObject(workspaceFolders, ZR_LSP_FIELD_CHANGE_NOTIFICATIONS, 1);
    cJSON_AddItemToObject(workspace, ZR_LSP_FIELD_WORKSPACE_FOLDERS, workspaceFolders);
    add_workspace_file_operation_capabilities(workspace);
    cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_WORKSPACE, workspace);

    cJSON_AddStringToObject(serverInfo, ZR_LSP_FIELD_NAME, ZR_LSP_SERVER_NAME);
    cJSON_AddStringToObject(serverInfo, ZR_LSP_FIELD_VERSION, ZR_LSP_SERVER_VERSION);

    cJSON_AddItemToObject(result, ZR_LSP_FIELD_CAPABILITIES, capabilities);
    cJSON_AddItemToObject(result, ZR_LSP_FIELD_SERVER_INFO, serverInfo);

    apply_initialization_selected_project(server, params);

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
        result = handle_initialize_request(server, params);
        send_result_response(id, result != NULL ? result : cJSON_CreateNull());
        return;
    }

    if (strcmp(method, ZR_LSP_METHOD_SHUTDOWN) == 0) {
        server->shutdownRequested = ZR_TRUE;
        send_result_response(id, NULL);
        return;
    }

    if (!dispatch_request_method(server, method, params, &result)) {
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

    if (strcmp(method, ZR_LSP_METHOD_ZR_SELECTED_PROJECT) == 0) {
        handle_zr_selected_project_notification(server, params);
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
    } else if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_DID_CREATE_FILES) == 0) {
        handle_did_create_files(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_DID_RENAME_FILES) == 0) {
        handle_did_rename_files(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_DID_DELETE_FILES) == 0) {
        handle_did_delete_files(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_EXIT) == 0) {
        if (outShouldExit != NULL) {
            *outShouldExit = 1;
        }
        if (outExitCode != NULL) {
            *outExitCode = server->shutdownRequested ? 0 : 1;
        }
    }
}
