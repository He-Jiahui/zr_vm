#include "zr_vm_language_server_stdio_internal.h"

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
                cJSON_AddItemToObject(textEdit, "range", serialize_range((*locationPtr)->range));
                cJSON_AddStringToObject(textEdit, "newText", newNameText != NULL ? newNameText : "");
                cJSON_AddItemToArray(uriEdits, textEdit);
            } else {
                cJSON_Delete(textEdit);
            }

            free(uriText);
        }
    }

    cJSON_AddItemToObject(edit, "changes", changes);
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

    textDocument = get_object_item(params, "textDocument");
    textJson = get_object_item(textDocument, "text");
    if (!cJSON_IsString((cJSON *)textJson)) {
        return 0;
    }

    versionJson = get_object_item(textDocument, "version");
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

    textDocument = get_object_item(params, "textDocument");
    versionJson = get_object_item(textDocument, "version");
    changes = get_object_item(params, "contentChanges");
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

    textJson = get_object_item(params, "text");
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

    contextJson = get_object_item(params, "context");
    includeDeclarationJson = get_object_item(contextJson, "includeDeclaration");
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

    queryJson = get_object_item(params, "query");
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

    cJSON_AddItemToObject(legend, "tokenTypes", types);
    cJSON_AddItemToObject(legend, "tokenModifiers", modifiers);
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

    cJSON_AddItemToObject(result, "data", data);
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

    ZrCore_Array_Init(server->state, &tokens, sizeof(TZrUInt32), 32);
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
    cJSON_AddItemToObject(result, "range", serialize_range(range));
    cJSON_AddStringToObject(result, "placeholder", placeholderText != NULL ? placeholderText : "");
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

    newNameJson = get_object_item(params, "newName");
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

    cJSON_AddBoolToObject(textDocumentSync, "openClose", 1);
    cJSON_AddNumberToObject(textDocumentSync, "change", 2);
    cJSON_AddBoolToObject(saveOptions, "includeText", 0);
    cJSON_AddItemToObject(textDocumentSync, "save", saveOptions);

    cJSON_AddItemToArray(triggerCharacters, cJSON_CreateString("."));
    cJSON_AddItemToArray(triggerCharacters, cJSON_CreateString(":"));
    cJSON_AddBoolToObject(completionProvider, "resolveProvider", 0);
    cJSON_AddItemToObject(completionProvider, "triggerCharacters", triggerCharacters);
    cJSON_AddItemToArray(signatureTriggerCharacters, cJSON_CreateString("("));
    cJSON_AddItemToArray(signatureTriggerCharacters, cJSON_CreateString(","));
    cJSON_AddItemToObject(signatureHelpProvider, "triggerCharacters", signatureTriggerCharacters);

    cJSON_AddBoolToObject(renameProvider, "prepareProvider", 1);

    cJSON_AddItemToObject(capabilities, "textDocumentSync", textDocumentSync);
    cJSON_AddItemToObject(capabilities, "completionProvider", completionProvider);
    cJSON_AddBoolToObject(capabilities, "hoverProvider", 1);
    cJSON_AddItemToObject(capabilities, "signatureHelpProvider", signatureHelpProvider);
    cJSON_AddBoolToObject(capabilities, "definitionProvider", 1);
    cJSON_AddBoolToObject(capabilities, "referencesProvider", 1);
    cJSON_AddItemToObject(capabilities, "renameProvider", renameProvider);
    cJSON_AddBoolToObject(capabilities, "documentSymbolProvider", 1);
    cJSON_AddBoolToObject(capabilities, "workspaceSymbolProvider", 1);
    cJSON_AddBoolToObject(capabilities, "documentHighlightProvider", 1);
    cJSON_AddItemToObject(semanticTokensProvider, "legend", semanticLegend);
    cJSON_AddBoolToObject(semanticTokensProvider, "full", 1);
    cJSON_AddItemToObject(capabilities, "semanticTokensProvider", semanticTokensProvider);
    cJSON_AddBoolToObject(workspaceFolders, "supported", 1);
    cJSON_AddBoolToObject(workspaceFolders, "changeNotifications", 1);
    cJSON_AddItemToObject(workspace, "workspaceFolders", workspaceFolders);
    cJSON_AddItemToObject(capabilities, "workspace", workspace);

    cJSON_AddStringToObject(serverInfo, "name", "zr_vm_language_server_stdio");
    cJSON_AddStringToObject(serverInfo, "version", "0.0.1");

    cJSON_AddItemToObject(result, "capabilities", capabilities);
    cJSON_AddItemToObject(result, "serverInfo", serverInfo);
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

    if (strcmp(method, "initialize") == 0) {
        result = handle_initialize_request();
        send_result_response(id, result != NULL ? result : cJSON_CreateNull());
        return;
    }

    if (strcmp(method, "shutdown") == 0) {
        server->shutdownRequested = ZR_TRUE;
        send_result_response(id, NULL);
        return;
    }

    if (strcmp(method, "textDocument/completion") == 0) {
        result = handle_completion_request(server, params);
    } else if (strcmp(method, "textDocument/hover") == 0) {
        result = handle_hover_request(server, params);
    } else if (strcmp(method, "textDocument/signatureHelp") == 0) {
        result = handle_signature_help_request(server, params);
    } else if (strcmp(method, "textDocument/definition") == 0) {
        result = handle_definition_request(server, params);
    } else if (strcmp(method, "textDocument/references") == 0) {
        result = handle_references_request(server, params);
    } else if (strcmp(method, "textDocument/documentSymbol") == 0) {
        result = handle_document_symbols_request(server, params);
    } else if (strcmp(method, "workspace/symbol") == 0) {
        result = handle_workspace_symbols_request(server, params);
    } else if (strcmp(method, "textDocument/documentHighlight") == 0) {
        result = handle_document_highlights_request(server, params);
    } else if (strcmp(method, "textDocument/semanticTokens/full") == 0) {
        result = handle_semantic_tokens_full_request(server, params);
    } else if (strcmp(method, "textDocument/prepareRename") == 0) {
        result = handle_prepare_rename_request(server, params);
    } else if (strcmp(method, "textDocument/rename") == 0) {
        result = handle_rename_request(server, params);
    } else {
        send_error_response(id, -32601, "Method not found");
        return;
    }

    if (result == NULL) {
        send_error_response(id, -32602, "Invalid params");
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

    if (strcmp(method, "initialized") == 0 ||
        strcmp(method, "workspace/didChangeConfiguration") == 0 ||
        strcmp(method, "workspace/didChangeWatchedFiles") == 0 ||
        strcmp(method, "workspace/didChangeWorkspaceFolders") == 0 ||
        strcmp(method, "$/cancelRequest") == 0) {
        return;
    }

    if (strcmp(method, "textDocument/didOpen") == 0) {
        handle_did_open(server, params);
    } else if (strcmp(method, "textDocument/didChange") == 0) {
        handle_did_change(server, params);
    } else if (strcmp(method, "textDocument/didClose") == 0) {
        handle_did_close(server, params);
    } else if (strcmp(method, "textDocument/didSave") == 0) {
        handle_did_save(server, params);
    } else if (strcmp(method, "exit") == 0) {
        if (outShouldExit != NULL) {
            *outShouldExit = 1;
        }
        if (outExitCode != NULL) {
            *outExitCode = server->shutdownRequested ? 0 : 1;
        }
    }
}
