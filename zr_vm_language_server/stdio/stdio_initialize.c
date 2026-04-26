#include "zr_vm_language_server_stdio_internal.h"

typedef struct SZrInitializeJsonParts {
    cJSON *result;
    cJSON *capabilities;
    cJSON *textDocumentSync;
    cJSON *completionProvider;
    cJSON *signatureHelpProvider;
    cJSON *renameProvider;
    cJSON *workspaceSymbolProvider;
    cJSON *inlayHintProvider;
    cJSON *saveOptions;
    cJSON *triggerCharacters;
    cJSON *signatureTriggerCharacters;
    cJSON *semanticTokensProvider;
    cJSON *semanticTokensFullProvider;
    cJSON *serverInfo;
    cJSON *workspace;
    cJSON *workspaceFolders;
} SZrInitializeJsonParts;

static void delete_initialize_json(SZrInitializeJsonParts *parts) {
    if (parts == NULL) {
        return;
    }

    cJSON_Delete(parts->result);
    cJSON_Delete(parts->capabilities);
    cJSON_Delete(parts->textDocumentSync);
    cJSON_Delete(parts->completionProvider);
    cJSON_Delete(parts->signatureHelpProvider);
    cJSON_Delete(parts->renameProvider);
    cJSON_Delete(parts->workspaceSymbolProvider);
    cJSON_Delete(parts->inlayHintProvider);
    cJSON_Delete(parts->saveOptions);
    cJSON_Delete(parts->triggerCharacters);
    cJSON_Delete(parts->signatureTriggerCharacters);
    cJSON_Delete(parts->semanticTokensProvider);
    cJSON_Delete(parts->semanticTokensFullProvider);
    cJSON_Delete(parts->serverInfo);
    cJSON_Delete(parts->workspace);
    cJSON_Delete(parts->workspaceFolders);
}

static TZrBool initialize_json_parts_are_valid(const SZrInitializeJsonParts *parts) {
    return parts != NULL && parts->result != NULL && parts->capabilities != NULL &&
           parts->textDocumentSync != NULL && parts->completionProvider != NULL &&
           parts->signatureHelpProvider != NULL && parts->renameProvider != NULL &&
           parts->saveOptions != NULL && parts->workspaceSymbolProvider != NULL &&
           parts->inlayHintProvider != NULL && parts->triggerCharacters != NULL &&
           parts->signatureTriggerCharacters != NULL && parts->semanticTokensProvider != NULL &&
           parts->semanticTokensFullProvider != NULL && parts->serverInfo != NULL &&
           parts->workspace != NULL && parts->workspaceFolders != NULL;
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

cJSON *handle_initialize_request(SZrStdioServer *server, const cJSON *params) {
    SZrInitializeJsonParts parts;
    cJSON *semanticLegend;

    parts.result = cJSON_CreateObject();
    parts.capabilities = cJSON_CreateObject();
    parts.textDocumentSync = cJSON_CreateObject();
    parts.completionProvider = cJSON_CreateObject();
    parts.signatureHelpProvider = cJSON_CreateObject();
    parts.renameProvider = cJSON_CreateObject();
    parts.workspaceSymbolProvider = cJSON_CreateObject();
    parts.inlayHintProvider = cJSON_CreateObject();
    parts.saveOptions = cJSON_CreateObject();
    parts.triggerCharacters = cJSON_CreateArray();
    parts.signatureTriggerCharacters = cJSON_CreateArray();
    parts.semanticTokensProvider = cJSON_CreateObject();
    parts.semanticTokensFullProvider = cJSON_CreateObject();
    parts.serverInfo = cJSON_CreateObject();
    parts.workspace = cJSON_CreateObject();
    parts.workspaceFolders = cJSON_CreateObject();

    if (!initialize_json_parts_are_valid(&parts)) {
        delete_initialize_json(&parts);
        return NULL;
    }

    semanticLegend = create_semantic_token_legend_json();
    if (semanticLegend == NULL) {
        delete_initialize_json(&parts);
        return NULL;
    }

    cJSON_AddBoolToObject(parts.textDocumentSync, ZR_LSP_FIELD_OPEN_CLOSE, 1);
    cJSON_AddNumberToObject(parts.textDocumentSync, ZR_LSP_FIELD_CHANGE, ZR_LSP_TEXT_DOCUMENT_SYNC_KIND_INCREMENTAL);
    cJSON_AddBoolToObject(parts.textDocumentSync, ZR_LSP_FIELD_WILL_SAVE, 1);
    cJSON_AddBoolToObject(parts.textDocumentSync, ZR_LSP_FIELD_WILL_SAVE_WAIT_UNTIL, 1);
    cJSON_AddBoolToObject(parts.saveOptions, ZR_LSP_FIELD_INCLUDE_TEXT, 0);
    cJSON_AddItemToObject(parts.textDocumentSync, ZR_LSP_FIELD_SAVE, parts.saveOptions);

    cJSON_AddItemToArray(parts.triggerCharacters,
                         cJSON_CreateString(ZR_LSP_COMPLETION_TRIGGER_CHARACTER_MEMBER_ACCESS));
    cJSON_AddItemToArray(parts.triggerCharacters,
                         cJSON_CreateString(ZR_LSP_COMPLETION_TRIGGER_CHARACTER_NAMESPACE_ACCESS));
    cJSON_AddBoolToObject(parts.completionProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER, 1);
    cJSON_AddItemToObject(parts.completionProvider, ZR_LSP_FIELD_TRIGGER_CHARACTERS, parts.triggerCharacters);
    cJSON_AddItemToObject(parts.completionProvider,
                          ZR_LSP_FIELD_ALL_COMMIT_CHARACTERS,
                          create_completion_commit_characters_array());

    cJSON_AddItemToArray(parts.signatureTriggerCharacters,
                         cJSON_CreateString(ZR_LSP_SIGNATURE_TRIGGER_CHARACTER_OPEN_PAREN));
    cJSON_AddItemToArray(parts.signatureTriggerCharacters,
                         cJSON_CreateString(ZR_LSP_SIGNATURE_TRIGGER_CHARACTER_ARGUMENT_SEPARATOR));
    cJSON_AddItemToObject(parts.signatureHelpProvider,
                          ZR_LSP_FIELD_TRIGGER_CHARACTERS,
                          parts.signatureTriggerCharacters);

    cJSON_AddBoolToObject(parts.renameProvider, ZR_LSP_FIELD_PREPARE_PROVIDER, 1);
    cJSON_AddBoolToObject(parts.workspaceSymbolProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER, 1);
    cJSON_AddBoolToObject(parts.inlayHintProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER, 1);

    cJSON_AddItemToObject(parts.capabilities, ZR_LSP_FIELD_TEXT_DOCUMENT_SYNC, parts.textDocumentSync);
    cJSON_AddItemToObject(parts.capabilities, ZR_LSP_FIELD_COMPLETION_PROVIDER, parts.completionProvider);
    cJSON_AddBoolToObject(parts.capabilities, ZR_LSP_FIELD_HOVER_PROVIDER, 1);
    cJSON_AddItemToObject(parts.capabilities, ZR_LSP_FIELD_SIGNATURE_HELP_PROVIDER, parts.signatureHelpProvider);
    cJSON_AddBoolToObject(parts.capabilities, ZR_LSP_FIELD_DEFINITION_PROVIDER, 1);
    cJSON_AddBoolToObject(parts.capabilities, ZR_LSP_FIELD_REFERENCES_PROVIDER, 1);
    cJSON_AddItemToObject(parts.capabilities, ZR_LSP_FIELD_RENAME_PROVIDER, parts.renameProvider);
    cJSON_AddBoolToObject(parts.capabilities, ZR_LSP_FIELD_DOCUMENT_SYMBOL_PROVIDER, 1);
    cJSON_AddItemToObject(parts.capabilities, ZR_LSP_FIELD_WORKSPACE_SYMBOL_PROVIDER, parts.workspaceSymbolProvider);
    cJSON_AddBoolToObject(parts.capabilities, ZR_LSP_FIELD_DOCUMENT_HIGHLIGHT_PROVIDER, 1);
    cJSON_AddItemToObject(parts.capabilities, ZR_LSP_FIELD_INLAY_HINT_PROVIDER, parts.inlayHintProvider);
    cJSON_AddItemToObject(parts.semanticTokensProvider, ZR_LSP_FIELD_LEGEND, semanticLegend);
    cJSON_AddBoolToObject(parts.semanticTokensFullProvider, ZR_LSP_FIELD_DELTA, 1);
    cJSON_AddItemToObject(parts.semanticTokensProvider, ZR_LSP_FIELD_FULL, parts.semanticTokensFullProvider);
    cJSON_AddBoolToObject(parts.semanticTokensProvider, ZR_LSP_FIELD_RANGE, 1);
    cJSON_AddItemToObject(parts.capabilities,
                          ZR_LSP_FIELD_SEMANTIC_TOKENS_PROVIDER,
                          parts.semanticTokensProvider);
    add_advanced_editor_capabilities(parts.capabilities);

    cJSON_AddBoolToObject(parts.workspaceFolders, ZR_LSP_FIELD_SUPPORTED, 1);
    cJSON_AddBoolToObject(parts.workspaceFolders, ZR_LSP_FIELD_CHANGE_NOTIFICATIONS, 1);
    cJSON_AddItemToObject(parts.workspace, ZR_LSP_FIELD_WORKSPACE_FOLDERS, parts.workspaceFolders);
    add_workspace_file_operation_capabilities(parts.workspace);
    cJSON_AddItemToObject(parts.capabilities, ZR_LSP_FIELD_WORKSPACE, parts.workspace);

    cJSON_AddStringToObject(parts.serverInfo, ZR_LSP_FIELD_NAME, ZR_LSP_SERVER_NAME);
    cJSON_AddStringToObject(parts.serverInfo, ZR_LSP_FIELD_VERSION, ZR_LSP_SERVER_VERSION);

    cJSON_AddItemToObject(parts.result, ZR_LSP_FIELD_CAPABILITIES, parts.capabilities);
    cJSON_AddItemToObject(parts.result, ZR_LSP_FIELD_SERVER_INFO, parts.serverInfo);

    apply_initialization_selected_project(server, params);
    return parts.result;
}
