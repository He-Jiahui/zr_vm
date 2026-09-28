#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_handler_result.h"
#include "stdio_json_builder.h"

#include "project/lsp_workspace.h"

/* stdio 只负责协议通知与项目索引的边界，项目层接口持有索引和分析器生命周期。 */
TZrBool ZrLanguageServer_LspProject_RemoveProjectByProjectUri(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrString *uri);
TZrBool ZrLanguageServer_LspProject_RemoveFileRecordByUri(SZrState *state,
                                                          SZrLspContext *context,
                                                          SZrString *uri);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspProject_PrepareSourceRename(
        SZrState *state,
        SZrLspContext *context,
        SZrString *oldUri,
        SZrString *newUri);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspProject_CollectSourceRenameEditPlan(
        SZrState *state,
        SZrLspContext *context,
        SZrString *oldUri,
        SZrString *newUri,
        SZrString **outNewModuleName,
        SZrArray *outLocations,
        SZrArray *outDocumentSnapshots);
TZrBool ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(SZrState *state,
                                                                     SZrLspContext *context,
                                                                     SZrString *uri);

/* 四种已实现文件操作共用扩展名过滤，避免客户端把未支持的文件纳入通知。 */
static cJSON *create_file_operation_registration(void) {
    cJSON *registration = cJSON_CreateObject();
    cJSON *filters;
    cJSON *filter;
    cJSON *pattern;

    if (registration == NULL ||
        (filters = cJSON_AddArrayToObject(registration, ZR_LSP_FIELD_FILTERS)) == NULL) {
        cJSON_Delete(registration);
        return NULL;
    }
    filter = cJSON_CreateObject();
    if (!stdio_json_add_owned_array_item(filters, filter) ||
        (pattern = cJSON_AddObjectToObject(filter, ZR_LSP_FIELD_PATTERN)) == NULL ||
        cJSON_AddStringToObject(pattern, ZR_LSP_FIELD_GLOB, "**/*.{zr,zrp,zro,dll,so,dylib}") == NULL) {
        cJSON_Delete(registration);
        return NULL;
    }
    return registration;
}

/* initialize 只发布 didCreate、willRename、didRename、didDelete；调用方持有 workspace JSON 树。 */
TZrBool add_workspace_file_operation_capabilities(cJSON *workspace) {
    cJSON *fileOperations;

    if (workspace == NULL) {
        return ZR_FALSE;
    }

    fileOperations = cJSON_AddObjectToObject(workspace, ZR_LSP_FIELD_FILE_OPERATIONS);
    if (fileOperations == NULL) {
        return ZR_FALSE;
    }

    return stdio_json_add_owned_item(fileOperations, ZR_LSP_FIELD_DID_CREATE, create_file_operation_registration()) &&
           stdio_json_add_owned_item(fileOperations, ZR_LSP_FIELD_WILL_RENAME, create_file_operation_registration()) &&
           stdio_json_add_owned_item(fileOperations, ZR_LSP_FIELD_DID_RENAME, create_file_operation_registration()) &&
           stdio_json_add_owned_item(fileOperations, ZR_LSP_FIELD_DID_DELETE, create_file_operation_registration());
}

/* 文件事件 URI 仍是 VM 字符串；扩展名判定覆盖短串和长串，不能依赖 NUL 结尾。
 * BUG: 这里比较未解码的 URI 后缀；file:///C:/workspace/a%2Ezr 可通过根检查并映射到 a.zr，
 * 却不匹配 .zr，watcher 不会刷新它，didRename 也不会走源文件迁移分支。 */
static TZrBool workspace_file_string_ends_with(SZrString *value, const TZrChar *suffix) {
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

/* 二进制包与动态插件改变时要重建所属项目的元数据图，而非按源文件增量更新。 */
static TZrBool workspace_file_uri_has_metadata_extension(SZrString *uri) {
    return workspace_file_string_ends_with(uri, ZR_VM_BINARY_MODULE_FILE_EXTENSION) ||
           workspace_file_string_ends_with(uri, ".dll") ||
           workspace_file_string_ends_with(uri, ".so") ||
           workspace_file_string_ends_with(uri, ".dylib");
}

/* 文件监听到磁盘变更时，parser 的打开版本代表编辑器未保存覆盖层的所有权。 */
static TZrBool workspace_file_has_open_overlay(SZrStdioServer *server, SZrString *uri) {
    SZrFileVersion *fileVersion;

    fileVersion = get_file_version_for_uri(server, uri);
    return fileVersion != ZR_NULL && fileVersion->isOpenDocument;
}

/* 只处理工作区内文件及工作区外已打开的 file URI 文档；依赖原生路径的根归属检查。
 * BUG: file:///C:/workspace/../outside.zr 可通过词法前缀门禁，随后读取根外磁盘文件；
 * lsp_workspace.c 的根检查未折叠 ..，需要端到端监听事件回归。 */
static TZrBool workspace_file_event_is_allowed(SZrStdioServer *server, SZrString *uri) {
    return server != ZR_NULL && server->context != ZR_NULL &&
           ZrLanguageServer_LspWorkspace_CanProcessFileEvent(server->context, uri);
}

/* 将 watcher 与 fileOperations 归一到同一项目事件：删除撤销索引，其余事件刷新未打开源文件或元数据。 */
static int handle_single_workspace_file_change(SZrStdioServer *server, SZrString *uri, TZrSize changeType) {
    if (server == ZR_NULL || uri == ZR_NULL || !workspace_file_event_is_allowed(server, uri)) {
        return 0;
    }

    /* TODO: 删除事件先于打开覆盖层检查；若客户端在 didClose 前通知删除仍打开的 URI，
     * 项目层可能移除该文档的 parser/analyzer，需核对通知顺序并补交错回归。 */
    if (changeType == 3) {
        if (workspace_file_string_ends_with(uri, ".zrp")) {
            ZrLanguageServer_LspProject_RemoveProjectByProjectUri(server->state, server->context, uri);
            publish_empty_diagnostics(server, uri);
            return 1;
        }

        /* BUG: RemoveFileRecordByUri 一次只移除一个项目记录；共享源码的其他项目仍可能保留旧符号。
         * didClose 的失败清理会循环调用同一接口，见 stdio_documents.c。 */
        if (workspace_file_string_ends_with(uri, ".zr")) {
            ZrLanguageServer_LspProject_RemoveFileRecordByUri(server->state, server->context, uri);
            publish_empty_diagnostics(server, uri);
            return 1;
        }

        if (workspace_file_uri_has_metadata_extension(uri)) {
            return ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(server->state,
                                                                                server->context,
                                                                                uri)
                       ? 1
                       : 0;
        }

        return 1;
    }

    if (workspace_file_string_ends_with(uri, ".zrp") || workspace_file_string_ends_with(uri, ".zr")) {
        /* didOpen/didChange 持有编辑器覆盖层；磁盘监听不能替换尚未保存的内容。 */
        if (workspace_file_has_open_overlay(server, uri)) {
            return 1;
        }
        return update_document_contents_from_disk(server, uri);
    }

    if (workspace_file_uri_has_metadata_extension(uri)) {
        return ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(server->state,
                                                                            server->context,
                                                                            uri)
                   ? 1
                   : 0;
    }

    return 1;
}

/* workspace/didChangeWatchedFiles 批量消费创建、修改、删除；单条无效通知不阻止后续合法事件。 */
int handle_did_change_watched_files(SZrStdioServer *server, const cJSON *params) {
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
        /* TODO: 目前任意正整数都会进入事件分流；需核对未知 FileChangeType 是否应直接拒绝，
         * 并用越界类型通知确认不会意外重读磁盘。 */
        if (changeType == 0) {
            continue;
        }

        handledAny = handle_single_workspace_file_change(server, uri, changeType) || handledAny;
    }

    return handledAny;
}

/* 客户端文件操作的 URI 先进入服务器缓存，再复用 watcher 的工作区归属与更新路径。 */
static int handle_file_operation_uri(SZrStdioServer *server, const cJSON *uriJson, TZrSize changeType) {
    const char *uriText;
    SZrString *uri;

    if (!cJSON_IsString((cJSON *)uriJson)) {
        return 0;
    }

    uriText = cJSON_GetStringValue((cJSON *)uriJson);
    if (uriText == NULL) {
        return 0;
    }

    uri = server_get_cached_uri(server, uriText);
    if (uri == ZR_NULL) {
        return 0;
    }

    return handle_single_workspace_file_change(server, uri, changeType);
}

/* 预重命名和事后重命名共用缓存 URI；返回的是借用对象，不由事件处理器释放。 */
static SZrString *workspace_file_operation_uri(SZrStdioServer *server,
                                               const cJSON *uriJson) {
    const char *uriText;

    if (server == ZR_NULL || !cJSON_IsString((cJSON *)uriJson)) {
        return ZR_NULL;
    }

    uriText = cJSON_GetStringValue((cJSON *)uriJson);
    return uriText != NULL ? server_get_cached_uri(server, uriText) : ZR_NULL;
}

/* didCreate/didDelete 的文件列表逐项映射到同一事件语义，保留合法项的处理结果。 */
static int handle_file_operation_list(SZrStdioServer *server,
                                      const cJSON *params,
                                      TZrSize changeType,
                                      const char *uriField) {
    const cJSON *files;
    int handledAny = 0;

    if (server == ZR_NULL || params == NULL || uriField == NULL) {
        return 0;
    }

    files = get_object_item(params, ZR_LSP_FIELD_FILES);
    if (!cJSON_IsArray((cJSON *)files)) {
        return 0;
    }

    for (int index = 0; index < cJSON_GetArraySize((cJSON *)files); index++) {
        const cJSON *file = cJSON_GetArrayItem((cJSON *)files, index);
        handledAny = handle_file_operation_uri(server, get_object_item(file, uriField), changeType) || handledAny;
    }

    return handledAny;
}

/* 文件已落盘后纳入未打开的项目和源文件；不依赖 didOpen 建立首次索引。 */
int handle_did_create_files(SZrStdioServer *server, const cJSON *params) {
    return handle_file_operation_list(server, params, 1, ZR_LSP_FIELD_URI);
}

/* 文件已被客户端删除后撤销项目/源文件记录，并清除对应诊断。 */
int handle_did_delete_files(SZrStdioServer *server, const cJSON *params) {
    return handle_file_operation_list(server, params, 3, ZR_LSP_FIELD_URI);
}

/* didRenameFiles 先让项目层迁移同源根 .zr 的 URI 与模块身份，再从新磁盘路径重建分析；
 * 不满足迁移约束的文件仍按旧 URI 删除、新 URI 创建处理。 */
int handle_did_rename_files(SZrStdioServer *server, const cJSON *params) {
    const cJSON *files;
    int handledAny = 0;

    if (server == ZR_NULL || params == NULL) {
        return 0;
    }

    files = get_object_item(params, ZR_LSP_FIELD_FILES);
    if (!cJSON_IsArray((cJSON *)files)) {
        return 0;
    }

    for (int index = 0; index < cJSON_GetArraySize((cJSON *)files); index++) {
        const cJSON *file = cJSON_GetArrayItem((cJSON *)files, index);
        SZrString *oldUri = workspace_file_operation_uri(
                server, get_object_item(file, ZR_LSP_FIELD_OLD_URI));
        SZrString *newUri = workspace_file_operation_uri(
                server, get_object_item(file, ZR_LSP_FIELD_NEW_URI));

        if (workspace_file_event_is_allowed(server, oldUri) &&
            workspace_file_event_is_allowed(server, newUri) &&
            workspace_file_string_ends_with(oldUri, ".zr") &&
            workspace_file_string_ends_with(newUri, ".zr") &&
            ZrLanguageServer_LspProject_PrepareSourceRename(
                    server->state, server->context, oldUri, newUri)) {
            publish_empty_diagnostics(server, oldUri);
            /* TODO: 若客户端已对 newUri 建立未保存覆盖层，直接重读磁盘会替换该版本；
             * 核对文件重命名与 didOpen 的通知顺序，并补交错场景回归。 */
            handledAny = update_document_contents_from_disk(server, newUri) || handledAny;
            continue;
        }

        handledAny = handle_file_operation_uri(server, get_object_item(file, ZR_LSP_FIELD_OLD_URI), 3) || handledAny;
        handledAny = handle_file_operation_uri(server, get_object_item(file, ZR_LSP_FIELD_NEW_URI), 1) || handledAny;
    }

    return handledAny;
}

/* willRenameFiles 只规划 AST 绑定处的跨文档编辑，不改文件或索引；
 * 每个目标的内容/版本快照在追加对应 JSON 前复核，任一失效就丢弃整份 WorkspaceEdit。 */
SZrLspHandlerResult handle_will_rename_files_request(SZrStdioServer *server, const cJSON *params) {
    const cJSON *files;
    cJSON *workspaceEdit = NULL;

    if (server == ZR_NULL || params == ZR_NULL || !cJSON_IsObject((cJSON *)params)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    files = get_object_item(params, ZR_LSP_FIELD_FILES);
    if (!cJSON_IsArray((cJSON *)files)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    for (int index = 0; index < cJSON_GetArraySize((cJSON *)files); index++) {
        const cJSON *file = cJSON_GetArrayItem((cJSON *)files, index);
        const cJSON *oldUriJson;
        const cJSON *newUriJson;
        SZrString *oldUri;
        SZrString *newUri;
        SZrString *newModuleName = ZR_NULL;
        SZrArray locations = {0};
        SZrArray documentSnapshots = {0};

        if (!cJSON_IsObject((cJSON *)file)) {
            cJSON_Delete(workspaceEdit);
            return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
        }
        oldUriJson = get_object_item(file, ZR_LSP_FIELD_OLD_URI);
        newUriJson = get_object_item(file, ZR_LSP_FIELD_NEW_URI);
        if (!cJSON_IsString((cJSON *)oldUriJson) ||
            !cJSON_IsString((cJSON *)newUriJson) ||
            cJSON_GetStringValue((cJSON *)oldUriJson) == NULL ||
            cJSON_GetStringValue((cJSON *)newUriJson) == NULL) {
            cJSON_Delete(workspaceEdit);
            return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
        }
        oldUri = workspace_file_operation_uri(server, oldUriJson);
        newUri = workspace_file_operation_uri(server, newUriJson);

        /* 不支持或无需编辑的单个 rename 可跳过；已收集部分计划后的失败不得发布半份编辑。 */
        if (!workspace_file_event_is_allowed(server, oldUri) ||
            !workspace_file_event_is_allowed(server, newUri) ||
            !ZrLanguageServer_LspProject_CollectSourceRenameEditPlan(
                    server->state,
                    server->context,
                    oldUri,
                    newUri,
                    &newModuleName,
                    &locations,
                    &documentSnapshots)) {
            TZrBool failedPlan =
                    locations.length > 0U || documentSnapshots.length > 0U;
            free_locations_array(server->state, &locations);
            if (documentSnapshots.isValid) {
                ZrCore_Array_Free(server->state, &documentSnapshots);
            }
            if (failedPlan) {
                cJSON_Delete(workspaceEdit);
                return stdio_handler_result_from_json(server->context, cJSON_CreateNull());
            }
            continue;
        }

        if (!ZrLanguageServer_LspWorkspaceEdit_ValidateDocumentSnapshots(
                    server->state,
                    server->context,
                    &documentSnapshots)) {
            free_locations_array(server->state, &locations);
            ZrCore_Array_Free(server->state, &documentSnapshots);
            cJSON_Delete(workspaceEdit);
            return stdio_handler_result_from_json(server->context, cJSON_CreateNull());
        }

        if (workspaceEdit == NULL) {
            workspaceEdit = create_workspace_edit_for_locations(
                    server,
                    &locations,
                    newModuleName,
                    &documentSnapshots);
        } else if (!append_workspace_edit_locations(
                           server,
                           workspaceEdit,
                           &locations,
                           newModuleName,
                           &documentSnapshots)) {
            cJSON_Delete(workspaceEdit);
            workspaceEdit = NULL;
        }
        free_locations_array(server->state, &locations);
        ZrCore_Array_Free(server->state, &documentSnapshots);

        if (workspaceEdit == NULL) {
            return stdio_handler_result_from_json(server->context, ZR_NULL);
        }
    }

    return stdio_handler_result_from_json(server->context, workspaceEdit != NULL ? workspaceEdit : cJSON_CreateNull());
}
