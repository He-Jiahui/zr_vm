#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_handler_result.h"
#include "stdio_json_builder.h"
#include "zr_vm_language_server/lsp_capability_registry.h"

#include "project/lsp_workspace.h"

/* 结果树构造成功后采用客户端私有的选中项目提示；显式 null/空值用于撤销选择，
 * 缺失字段保留上下文原值。项目发现流程稍后按该路径选择 .zrp。 */
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

/* 把 LSP 文件 URI 交给 workspace 层做原生路径规范化与去重；无效 URI 不成为根目录。 */
static void add_workspace_folder_uri(SZrStdioServer *server, const cJSON *uriJson) {
    const char *uriText;
    SZrString *uri;

    if (server == ZR_NULL || server->context == ZR_NULL || !cJSON_IsString((cJSON *)uriJson)) {
        return;
    }

    uriText = cJSON_GetStringValue((cJSON *)uriJson);
    uri = uriText != ZR_NULL ? server_get_cached_uri(server, uriText) : ZR_NULL;
    if (uri != ZR_NULL) {
        ZrLanguageServer_LspWorkspace_AddFolder(server->state, server->context, uri);
    }
}

/* 兼容旧客户端的 rootPath：先转为文件 URI，再复用 workspace 的根目录规则。 */
static void add_workspace_folder_path(SZrStdioServer *server, const cJSON *pathJson) {
    const char *pathText;
    SZrString *uri;

    if (server == ZR_NULL || server->context == ZR_NULL || !cJSON_IsString((cJSON *)pathJson)) {
        return;
    }

    pathText = cJSON_GetStringValue((cJSON *)pathJson);
    uri = pathText != ZR_NULL ? ZrLanguageServer_LspUri_FromNativePath(server->state, pathText) : ZR_NULL;
    if (uri != ZR_NULL) {
        ZrLanguageServer_LspWorkspace_AddFolder(server->state, server->context, uri);
    }
}

/* initialize 以 workspaceFolders 为权威列表，缺席时才依次回退 rootUri/rootPath；
 * 空数组也表示无根目录。重试时重新建立列表，避免旧根目录混入新会话。 */
static void apply_initialization_workspace_folders(SZrStdioServer *server, const cJSON *params) {
    const cJSON *workspaceFolders;
    const cJSON *rootUri;
    const cJSON *rootPath;

    if (server == ZR_NULL || server->context == ZR_NULL || params == ZR_NULL) {
        return;
    }

    ZrLanguageServer_LspWorkspace_Reset(server->state, server->context);
    workspaceFolders = get_object_item(params, ZR_LSP_FIELD_WORKSPACE_FOLDERS);
    if (cJSON_IsArray((cJSON *)workspaceFolders)) {
        for (int index = 0; index < cJSON_GetArraySize((cJSON *)workspaceFolders); index++) {
            const cJSON *folder = cJSON_GetArrayItem((cJSON *)workspaceFolders, index);
            add_workspace_folder_uri(server, get_object_item(folder, ZR_LSP_FIELD_URI));
        }
        return;
    }

    rootUri = get_object_item(params, ZR_LSP_FIELD_ROOT_URI);
    if (cJSON_IsString((cJSON *)rootUri)) {
        add_workspace_folder_uri(server, rootUri);
        return;
    }

    rootPath = get_object_item(params, ZR_LSP_FIELD_ROOT_PATH);
    add_workspace_folder_path(server, rootPath);
}

/* 仅由初始化响应后可处理普通消息的通知分派器调用，增删沿用 initialize 的 URI 规则；
 * 删除根目录还可能使 workspace 层清除该根下的选中项目与项目索引。 */
void handle_did_change_workspace_folders(SZrStdioServer *server, const cJSON *params) {
    const cJSON *event;
    const cJSON *added;
    const cJSON *removed;

    if (server == ZR_NULL || server->context == ZR_NULL || params == ZR_NULL) {
        return;
    }

    event = get_object_item(params, ZR_LSP_FIELD_EVENT);
    if (event == ZR_NULL) {
        return;
    }

    added = get_object_item(event, ZR_LSP_FIELD_ADDED);
    if (cJSON_IsArray((cJSON *)added)) {
        for (int index = 0; index < cJSON_GetArraySize((cJSON *)added); index++) {
            const cJSON *folder = cJSON_GetArrayItem((cJSON *)added, index);
            add_workspace_folder_uri(server, get_object_item(folder, ZR_LSP_FIELD_URI));
        }
    }

    removed = get_object_item(event, ZR_LSP_FIELD_REMOVED);
    if (cJSON_IsArray((cJSON *)removed)) {
        for (int index = 0; index < cJSON_GetArraySize((cJSON *)removed); index++) {
            const cJSON *folder = cJSON_GetArrayItem((cJSON *)removed, index);
            const cJSON *uriJson = get_object_item(folder, ZR_LSP_FIELD_URI);
            const char *uriText = cJSON_IsString((cJSON *)uriJson)
                                      ? cJSON_GetStringValue((cJSON *)uriJson)
                                      : ZR_NULL;
            SZrString *uri = uriText != ZR_NULL ? server_get_cached_uri(server, uriText) : ZR_NULL;

            if (uri != ZR_NULL) {
                ZrLanguageServer_LspWorkspace_RemoveFolder(server->state, server->context, uri);
            }
        }
    }
}

/* 生成与本地请求分派能力一致的 initialize Result；resolve 位从共享能力注册表读取，
 * 客户端可选能力只在协商成功后公开。返回的整棵树交给结果封装/响应发送方接管。 */
static cJSON *create_initialize_result(SZrStdioServer *server, const cJSON *params) {
    const char *completionTriggers[] = {
            ZR_LSP_COMPLETION_TRIGGER_CHARACTER_MEMBER_ACCESS,
            ZR_LSP_COMPLETION_TRIGGER_CHARACTER_NAMESPACE_ACCESS
    };
    const char *signatureTriggers[] = {
            ZR_LSP_SIGNATURE_TRIGGER_CHARACTER_OPEN_PAREN,
            ZR_LSP_SIGNATURE_TRIGGER_CHARACTER_ARGUMENT_SEPARATOR
    };
    cJSON *result = cJSON_CreateObject();
    cJSON *capabilities;
    cJSON *textDocumentSync;
    cJSON *saveOptions;
    cJSON *completionProvider;
    cJSON *signatureHelpProvider;
    cJSON *renameProvider;
    cJSON *workspaceSymbolProvider;
    cJSON *inlayHintProvider;
    cJSON *semanticTokensProvider;
    cJSON *semanticTokensFullProvider;
    cJSON *workspace;
    cJSON *workspaceFolders;
    cJSON *serverInfo;

    /* 子节点立即挂到 result，任一后续分配失败都由同一根节点回收。 */
    if (result == NULL ||
        (capabilities = cJSON_AddObjectToObject(result, ZR_LSP_FIELD_CAPABILITIES)) == NULL ||
        (textDocumentSync = cJSON_AddObjectToObject(capabilities, ZR_LSP_FIELD_TEXT_DOCUMENT_SYNC)) == NULL ||
        (saveOptions = cJSON_AddObjectToObject(textDocumentSync, ZR_LSP_FIELD_SAVE)) == NULL ||
        cJSON_AddBoolToObject(textDocumentSync, ZR_LSP_FIELD_OPEN_CLOSE, 1) == NULL ||
        cJSON_AddNumberToObject(textDocumentSync, ZR_LSP_FIELD_CHANGE,
                                ZR_LSP_TEXT_DOCUMENT_SYNC_KIND_INCREMENTAL) == NULL ||
        cJSON_AddBoolToObject(textDocumentSync, ZR_LSP_FIELD_WILL_SAVE_WAIT_UNTIL, 1) == NULL ||
        cJSON_AddBoolToObject(saveOptions, ZR_LSP_FIELD_INCLUDE_TEXT, 0) == NULL ||
        cJSON_AddStringToObject(capabilities, ZR_LSP_FIELD_POSITION_ENCODING, position_encoding_name(server)) == NULL) {
        goto allocation_failed;
    }

    if ((completionProvider = cJSON_AddObjectToObject(capabilities, ZR_LSP_FIELD_COMPLETION_PROVIDER)) == NULL ||
        cJSON_AddBoolToObject(completionProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER,
                              ZrLanguageServer_LspCapabilityRegistry_HasResolveForRuntime(
                                      ZR_LSP_FIELD_COMPLETION_PROVIDER, ZR_LSP_RUNTIME_NATIVE)) == NULL ||
        !stdio_json_add_owned_item(completionProvider, ZR_LSP_FIELD_TRIGGER_CHARACTERS,
                                   cJSON_CreateStringArray(completionTriggers,
                                           (int)(sizeof(completionTriggers) / sizeof(completionTriggers[0])))) ||
        !stdio_json_add_owned_item(completionProvider, ZR_LSP_FIELD_ALL_COMMIT_CHARACTERS,
                                   create_completion_commit_characters_array()) ||
        (signatureHelpProvider = cJSON_AddObjectToObject(capabilities, ZR_LSP_FIELD_SIGNATURE_HELP_PROVIDER)) == NULL ||
        !stdio_json_add_owned_item(signatureHelpProvider, ZR_LSP_FIELD_TRIGGER_CHARACTERS,
                                   cJSON_CreateStringArray(signatureTriggers,
                                           (int)(sizeof(signatureTriggers) / sizeof(signatureTriggers[0]))))) {
        goto allocation_failed;
    }

    if ((renameProvider = cJSON_AddObjectToObject(capabilities, ZR_LSP_FIELD_RENAME_PROVIDER)) == NULL ||
        cJSON_AddBoolToObject(renameProvider, ZR_LSP_FIELD_PREPARE_PROVIDER, 1) == NULL ||
        (workspaceSymbolProvider = cJSON_AddObjectToObject(capabilities, ZR_LSP_FIELD_WORKSPACE_SYMBOL_PROVIDER)) == NULL ||
        cJSON_AddBoolToObject(workspaceSymbolProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER,
                              ZrLanguageServer_LspCapabilityRegistry_HasResolveForRuntime(
                                      ZR_LSP_FIELD_WORKSPACE_SYMBOL_PROVIDER, ZR_LSP_RUNTIME_NATIVE)) == NULL ||
        (inlayHintProvider = cJSON_AddObjectToObject(capabilities, ZR_LSP_FIELD_INLAY_HINT_PROVIDER)) == NULL ||
        cJSON_AddBoolToObject(inlayHintProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER,
                              ZrLanguageServer_LspCapabilityRegistry_HasResolveForRuntime(
                                      ZR_LSP_FIELD_INLAY_HINT_PROVIDER, ZR_LSP_RUNTIME_NATIVE)) == NULL ||
        cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_HOVER_PROVIDER, 1) == NULL ||
        cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_DEFINITION_PROVIDER, 1) == NULL ||
        cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_REFERENCES_PROVIDER, 1) == NULL ||
        cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_DOCUMENT_SYMBOL_PROVIDER, 1) == NULL ||
        cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_DOCUMENT_HIGHLIGHT_PROVIDER, 1) == NULL) {
        goto allocation_failed;
    }

    if ((semanticTokensProvider = cJSON_AddObjectToObject(capabilities, ZR_LSP_FIELD_SEMANTIC_TOKENS_PROVIDER)) == NULL ||
        !stdio_json_add_owned_item(semanticTokensProvider, ZR_LSP_FIELD_LEGEND, create_semantic_token_legend_json()) ||
        (semanticTokensFullProvider = cJSON_AddObjectToObject(semanticTokensProvider, ZR_LSP_FIELD_FULL)) == NULL ||
        cJSON_AddBoolToObject(semanticTokensFullProvider, ZR_LSP_FIELD_DELTA, 1) == NULL ||
        cJSON_AddBoolToObject(semanticTokensProvider, ZR_LSP_FIELD_RANGE, 1) == NULL ||
        !add_advanced_editor_capabilities(server, params, capabilities)) {
        goto allocation_failed;
    }

    if ((workspace = cJSON_AddObjectToObject(capabilities, ZR_LSP_FIELD_WORKSPACE)) == NULL ||
        (workspaceFolders = cJSON_AddObjectToObject(workspace, ZR_LSP_FIELD_WORKSPACE_FOLDERS)) == NULL ||
        cJSON_AddBoolToObject(workspaceFolders, ZR_LSP_FIELD_SUPPORTED, 1) == NULL ||
        cJSON_AddBoolToObject(workspaceFolders, ZR_LSP_FIELD_CHANGE_NOTIFICATIONS,
                              server->context->workspace != ZR_NULL) == NULL ||
        !add_workspace_file_operation_capabilities(workspace) ||
        (serverInfo = cJSON_AddObjectToObject(result, ZR_LSP_FIELD_SERVER_INFO)) == NULL ||
        cJSON_AddStringToObject(serverInfo, ZR_LSP_FIELD_NAME, ZR_LSP_SERVER_NAME) == NULL ||
        cJSON_AddStringToObject(serverInfo, ZR_LSP_FIELD_VERSION, ZR_LSP_SERVER_VERSION) == NULL) {
        goto allocation_failed;
    }
    return result;

allocation_failed:
    cJSON_Delete(result);
    return NULL;
}

/* initialize 请求的构造阶段：校验参数、协商响应能力，再设置工作区提示。
 * 调用者负责发送结果，并且仅在写出成功后推进生命周期；失败响应不得持有 JSON 树。 */
SZrLspHandlerResult handle_initialize_request(SZrStdioServer *server, const cJSON *params) {
    EZrStdioPositionEncoding previousPositionEncoding;
    TZrBool previousInlineCompletion;
    TZrBool previousRangesFormatting;
    SZrLspHandlerResult response;

    if (!cJSON_IsObject(params)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    if (server == ZR_NULL || server->context == ZR_NULL) {
        return stdio_handler_error(ZR_LSP_HANDLER_INTERNAL_ERROR);
    }
    if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(server->context)) {
        return stdio_handler_error(ZR_LSP_HANDLER_CANCELLED);
    }

    previousPositionEncoding = server->positionEncoding;
    previousInlineCompletion = server->supportsInlineCompletion;
    previousRangesFormatting = server->supportsRangesFormatting;
    negotiate_position_encoding(server, params);
    response = stdio_handler_result_from_json(server->context, create_initialize_result(server, params));
    if (response.status != ZR_LSP_HANDLER_OK) {
        server->positionEncoding = previousPositionEncoding;
        server->supportsInlineCompletion = previousInlineCompletion;
        server->supportsRangesFormatting = previousRangesFormatting;
        return response;
    }

    /* BUG: 此处在 stdio_requests.c 写出响应之前提交工作区与选中项目。
     * 若响应封装或写出失败，生命周期仍为 NEW；重试时省略选中项目字段会沿用
     * 失败请求留下的项目选择，后续项目发现会读取该路径。 */
    apply_initialization_workspace_folders(server, params);
    apply_initialization_selected_project(server, params);
    return response;
}
