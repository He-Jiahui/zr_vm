#include "zr_vm_language_server_stdio_internal.h"
#include "project/lsp_project_internal.h"

/** 复用项目层文件事件资格判断，决定 didClose 后是否应恢复磁盘版本。 */
TZrBool ZrLanguageServer_LspWorkspace_CanProcessFileEvent(SZrLspContext *context,
                                                           SZrString *uri);

/** 在无可用 parser 版本时仍按等价 URI 查询失同步记录，供请求入口阻止陈旧结果。 */
static TZrBool desynchronized_document_set_contains(
        const SZrDesynchronizedDocumentSet *set,
        const SZrString *uri) {
    size_t index;

    if (set == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0; index < set->count; index++) {
        if (set->items[index] == uri ||
            ZrLanguageServer_LspUri_Equivalent(set->items[index], (SZrString *)uri)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/** @brief 记录等待全量替换的 URI，补足 parser 版本尚未建立时的失同步状态。
 * TODO: 扩容失败会静默丢失记录；核对分配失败时请求入口能否继续拒绝陈旧内容。 */
static void desynchronized_document_set_add(
        SZrDesynchronizedDocumentSet *set,
        SZrString *uri) {
    SZrString **items;
    size_t capacity;

    if (set == ZR_NULL || uri == ZR_NULL || desynchronized_document_set_contains(set, uri)) {
        return;
    }
    if (set->count == set->capacity) {
        capacity = set->capacity == 0 ? 4U : set->capacity * 2U;
        items = (SZrString **)realloc(set->items, capacity * sizeof(SZrString *));
        if (items == ZR_NULL) {
            return;
        }
        set->items = items;
        set->capacity = capacity;
    }
    set->items[set->count++] = uri;
}

/** 清除已恢复同步的 URI 记录；集合仅借用 URI，生命周期由服务器缓存维持。 */
static void desynchronized_document_set_remove(
        SZrDesynchronizedDocumentSet *set,
        const SZrString *uri) {
    size_t index;

    if (set == ZR_NULL || uri == ZR_NULL) {
        return;
    }
    for (index = 0; index < set->count; index++) {
        if (set->items[index] == uri ||
            ZrLanguageServer_LspUri_Equivalent(set->items[index], (SZrString *)uri)) {
            if (index + 1U < set->count) {
                memmove(&set->items[index],
                        &set->items[index + 1U],
                        (set->count - index - 1U) * sizeof(SZrString *));
            }
            set->count--;
            return;
        }
    }
}

/** 标记增量编辑链断裂，同时更新文件版本与服务器备用集合。 */
void mark_document_desynchronized(SZrStdioServer *server, SZrString *uri) {
    SZrFileVersion *fileVersion = get_file_version_for_uri(server, uri);

    if (fileVersion != ZR_NULL) {
        fileVersion->isDesynchronized = ZR_TRUE;
    }
    if (server != ZR_NULL) {
        desynchronized_document_set_add(&server->desynchronizedDocuments, uri);
    }
}

/** 完整文本已提交或文档已关闭时，释放两个层面的失同步门禁。 */
void clear_document_desynchronization(SZrStdioServer *server, SZrString *uri) {
    SZrFileVersion *fileVersion = get_file_version_for_uri(server, uri);

    if (fileVersion != ZR_NULL) {
        fileVersion->isDesynchronized = ZR_FALSE;
    }
    if (server != ZR_NULL) {
        desynchronized_document_set_remove(&server->desynchronizedDocuments, uri);
    }
}

/** 汇总 parser 版本和备用集合状态，供请求分派器拒绝失同步文档查询。 */
TZrBool document_is_desynchronized(SZrStdioServer *server, SZrString *uri) {
    SZrFileVersion *fileVersion = get_file_version_for_uri(server, uri);

    return (fileVersion != ZR_NULL && fileVersion->isDesynchronized) ||
           (server != ZR_NULL &&
            desynchronized_document_set_contains(&server->desynchronizedDocuments, uri));
}

/** 仅允许单个无 range 的全文替换解除失同步，避免基于失效版本应用增量编辑。 */
static TZrBool content_changes_is_single_full_replacement(const cJSON *changes) {
    const cJSON *change;

    if (!cJSON_IsArray((cJSON *)changes) || cJSON_GetArraySize((cJSON *)changes) != 1) {
        return ZR_FALSE;
    }
    change = cJSON_GetArrayItem((cJSON *)changes, 0);
    return cJSON_IsObject((cJSON *)change) &&
           cJSON_IsString(get_object_item(change, ZR_LSP_FIELD_TEXT)) &&
           get_object_item(change, ZR_LSP_FIELD_RANGE) == ZR_NULL &&
           get_object_item(change, ZR_LSP_FIELD_RANGE_LENGTH) == ZR_NULL;
}

/** @brief 从 textDocument 参数取得 URI，供文档通知与诊断请求共用入口校验。
 * outUriText 借用 params 的 JSON 字符串，outUri 借用服务器 URI 缓存；均不得由调用方释放。
 * BUG: 带 \\u0000 的 JSON URI 未拒绝，缓存层 strcmp/strlen 只见前缀，可能定位另一文档。 */
int get_uri_from_text_document(SZrStdioServer *server,
                               const cJSON *params,
                               const char **outUriText,
                               SZrString **outUri) {
    const cJSON *textDocument;
    const cJSON *uriJson;
    const char *uriText;

    if (server == ZR_NULL || params == NULL || outUriText == NULL || outUri == NULL) {
        return 0;
    }

    textDocument = get_object_item(params, ZR_LSP_FIELD_TEXT_DOCUMENT);
    uriJson = get_object_item(textDocument, ZR_LSP_FIELD_URI);
    if (!cJSON_IsString((cJSON *)uriJson)) {
        return 0;
    }

    uriText = cJSON_GetStringValue((cJSON *)uriJson);
    if (uriText == NULL) {
        return 0;
    }

    *outUriText = uriText;
    *outUri = server_get_cached_uri(server, uriText);
    return *outUri != ZR_NULL;
}

/** 先解析缓存 URI，再按当前协商的位置编码解释请求坐标。 */
int get_uri_and_position(SZrStdioServer *server,
                         const cJSON *params,
                         const char **outUriText,
                         SZrString **outUri,
                         SZrLspPosition *outPosition) {
    const cJSON *positionJson;

    if (!get_uri_from_text_document(server, params, outUriText, outUri) || outPosition == NULL) {
        return 0;
    }

    positionJson = get_object_item(params, ZR_LSP_FIELD_POSITION);
    return parse_position_for_uri(server, *outUri, positionJson, outPosition);
}

/** @brief 尝试以客户端全文和版本建立打开文档快照并推送 parser 当前诊断；成功后解除失同步。
 * BUG: 磁盘快照版本可能高于同一连接再次 didOpen 的版本，parser 拒绝提交后旧磁盘文本仍可被查询。
 * BUG: JSON 的 \\u0000 经 cJSON 解码后被 strlen 截断，成功通知却只提交前半段正文。 */
int handle_did_open(SZrStdioServer *server, const cJSON *params) {
    const cJSON *textDocument;
    const cJSON *textJson;
    const cJSON *versionJson;
    const char *uriText;
    SZrString *uri;
    SZrFileVersion *fileVersion;
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
    if (!parse_size_value_strict(versionJson, &version)) {
        return 0;
    }
    text = cJSON_GetStringValue((cJSON *)textJson);
    if (text == NULL) {
        text = "";
    }
    fileVersion = get_file_version_for_uri(server, uri);
    if (fileVersion != ZR_NULL && fileVersion->isOpenDocument) {
        return 0;
    }

    if (!update_document_contents(server, uri, text, strlen(text), version)) {
        return 0;
    }
    clear_document_desynchronization(server, uri);
    return 1;
}

/** @brief 在已打开快照上按顺序提交版本递增的编辑；失败时要求后续全文替换重新同步。
 * 入口由通知分派调用，返回值不会成为协议响应；中间文本只在成功提交前由本函数持有。 */
int handle_did_change(SZrStdioServer *server, const cJSON *params) {
    const cJSON *textDocument;
    const cJSON *versionJson;
    const cJSON *changes;
    const char *uriText;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    const char *originalContent;
    size_t originalLength;
    char *updatedContent;
    size_t updatedLength = 0;
    TZrSize version;
    TZrBool isFullReplacement;
    int success;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return 0;
    }

    textDocument = get_object_item(params, ZR_LSP_FIELD_TEXT_DOCUMENT);
    versionJson = get_object_item(textDocument, ZR_LSP_FIELD_VERSION);
    changes = get_object_item(params, ZR_LSP_FIELD_CONTENT_CHANGES);
    if (!parse_size_value_strict(versionJson, &version) ||
        !cJSON_IsArray((cJSON *)changes) || cJSON_GetArraySize((cJSON *)changes) == 0) {
        mark_document_desynchronized(server, uri);
        return 0;
    }

    fileVersion = get_file_version_for_uri(server, uri);
    if (fileVersion == ZR_NULL || !fileVersion->isOpenDocument) {
        mark_document_desynchronized(server, uri);
        return 0;
    }
    isFullReplacement = content_changes_is_single_full_replacement(changes);
    if (fileVersion->isDesynchronized && !isFullReplacement) {
        return 0;
    }
    /* 持有不可变快照直至整批编辑完成，确保失败不会提交前半段变更。 */
    if (ZrLanguageServer_FileVersionContentSnapshot_Acquire(server->state, fileVersion, &snapshot)) {
        originalContent = snapshot.content;
        originalLength = snapshot.contentLength;
        if (version <= snapshot.version) {
            ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
            mark_document_desynchronized(server, uri);
            return 0;
        }
    } else {
        mark_document_desynchronized(server, uri);
        return 0;
    }

    updatedContent = apply_content_changes(server, uri, originalContent, originalLength, changes, &updatedLength);
    ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
    if (updatedContent == NULL) {
        mark_document_desynchronized(server, uri);
        return 0;
    }

    success = update_document_contents(server, uri, updatedContent, updatedLength, version);
    free(updatedContent);
    if (!success) {
        mark_document_desynchronized(server, uri);
    } else if (isFullReplacement) {
        clear_document_desynchronization(server, uri);
    }
    return success;
}

/** @brief 结束客户端覆盖：优先恢复磁盘版本，失败时清除诊断及共享项目中的旧文档记录。
 * 所有项目索引可能共享同一 URI，因此失败清理须移除每个登记。
 * TODO: 手工释放 analyzer 未清语义历史快照；核对关闭后旧快照是否仍应可查询。 */
int handle_did_close(SZrStdioServer *server, const cJSON *params) {
    const char *uriText;
    SZrString *uri;
    SZrFileVersion *fileVersion;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return 0;
    }

    remove_semantic_token_cache_for_uri(server, uriText);
    clear_document_desynchronization(server, uri);
    fileVersion = get_file_version_for_uri(server, uri);
    if (fileVersion != ZR_NULL) {
        fileVersion->isOpenDocument = ZR_FALSE;
    }
    if (ZrLanguageServer_LspWorkspace_CanProcessFileEvent(server->context, uri) &&
        update_document_contents_from_disk(server, uri)) {
        fileVersion = get_file_version_for_uri(server, uri);
        if (fileVersion != ZR_NULL) {
            fileVersion->isOpenDocument = ZR_FALSE;
            clear_document_desynchronization(server, uri);
            return 1;
        }
    }
    publish_empty_diagnostics(server, uri);
    /* 共享源码可登记在多个工程索引中；关闭失败路径必须全部移除。 */
    while (ZrLanguageServer_LspProject_RemoveFileRecordByUri(server->state, server->context, uri)) {
    }
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

/** @brief 保存通知刷新已提交快照的诊断；关闭的文档从磁盘重新取版本。
 * 携带 text 时沿用当前 overlay，保存通知不单独创建新文档版本。 */
int handle_did_save(SZrStdioServer *server, const cJSON *params) {
    const cJSON *textJson;
    const char *uriText;
    SZrString *uri;
    SZrFileVersion *fileVersion;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return 0;
    }

    textJson = get_object_item(params, ZR_LSP_FIELD_TEXT);
    if (cJSON_IsString((cJSON *)textJson)) {
        fileVersion = get_file_version_for_uri(server, uri);
        if (fileVersion != ZR_NULL) {
            publish_diagnostics(server, uri);
        }
        return 1;
    }

    if (textJson != ZR_NULL && !cJSON_IsNull((cJSON *)textJson)) {
        return 0;
    }
    fileVersion = get_file_version_for_uri(server, uri);
    if (fileVersion != ZR_NULL && fileVersion->isOpenDocument) {
        publish_diagnostics(server, uri);
        return 1;
    }
    return update_document_contents_from_disk(server, uri);
}
