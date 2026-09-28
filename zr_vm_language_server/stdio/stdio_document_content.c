#include "zr_vm_language_server_stdio_internal.h"
#include "zr_vm_core/utf8.h"

/** 借用 parser 中当前 URI 的文件版本，供通知入口判断编辑是否真正提交。 */
SZrFileVersion *get_file_version_for_uri(SZrStdioServer *server, SZrString *uri) {
    if (server == ZR_NULL || server->context == ZR_NULL || server->context->parser == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrLanguageServer_IncrementalParser_GetFileVersion(server->context->parser, uri);
}

/** @brief 按客户端位置编码验证单次 LSP 编辑；成功返回 malloc 缓冲，失败不改原文。
 * BUG: JSON 的 \\u0000 会被 cJSON 解码成 NUL，strlen 静默截断替换文本，客户端与服务端失同步。 */
static char *apply_single_change(SZrStdioServer *server,
                                 SZrString *uri,
                                 const char *original,
                                 size_t originalLength,
                                 const cJSON *change,
                                 size_t *outLength) {
    const cJSON *textJson;
    const cJSON *rangeJson;
    const cJSON *rangeLengthJson;
    const char *replacement;
    size_t replacementLength;
    char *updated;
    size_t prefixLength;
    size_t suffixLength;
    size_t startOffset = 0;
    size_t endOffset = originalLength;

    if (uri == ZR_NULL || original == NULL || change == NULL || outLength == NULL) {
        return NULL;
    }

    textJson = get_object_item(change, ZR_LSP_FIELD_TEXT);
    if (!cJSON_IsString(textJson)) {
        return NULL;
    }

    replacement = cJSON_GetStringValue(textJson);
    replacementLength = replacement != NULL ? strlen(replacement) : 0;
    if (replacement == NULL ||
        !ZrCore_Utf8_IsValid((TZrNativeString)replacement, (TZrSize)replacementLength)) {
        return NULL;
    }
    rangeJson = get_object_item(change, ZR_LSP_FIELD_RANGE);
    rangeLengthJson = get_object_item(change, ZR_LSP_FIELD_RANGE_LENGTH);

    if (rangeJson != NULL && !cJSON_IsNull(rangeJson)) {
        TZrSize clientRangeLength;

        if (!content_change_range_to_byte_offsets(server,
                                                   original,
                                                   originalLength,
                                                   rangeJson,
                                                   &startOffset,
                                                   &endOffset,
                                                   &clientRangeLength)) {
            return NULL;
        }
        if (rangeLengthJson != ZR_NULL) {
            TZrSize declaredRangeLength;

            if (!parse_size_value_strict(rangeLengthJson, &declaredRangeLength) ||
                declaredRangeLength != clientRangeLength) {
                return NULL;
            }
        }
    } else if (rangeJson != ZR_NULL || rangeLengthJson != ZR_NULL) {
        return NULL;
    }

    prefixLength = startOffset;
    suffixLength = originalLength - endOffset;
    *outLength = prefixLength + replacementLength + suffixLength;

    updated = (char *)malloc(*outLength + 1);
    if (updated == NULL) {
        return NULL;
    }

    if (prefixLength > 0) {
        memcpy(updated, original, prefixLength);
    }
    if (replacementLength > 0) {
        memcpy(updated + prefixLength, replacement, replacementLength);
    }
    if (suffixLength > 0) {
        memcpy(updated + prefixLength + replacementLength, original + endOffset, suffixLength);
    }
    updated[*outLength] = '\0';
    return updated;
}

/** 顺序应用一批 didChange 编辑；任一失败丢弃中间结果，由调用方标记失同步。 */
char *apply_content_changes(SZrStdioServer *server,
                            SZrString *uri,
                            const char *original,
                            size_t originalLength,
                            const cJSON *changes,
                            size_t *outLength) {
    char *current;
    size_t currentLength;
    int index;

    if (uri == ZR_NULL || original == NULL || changes == NULL || outLength == NULL) {
        return NULL;
    }

    current = duplicate_string_range(original, originalLength);
    if (current == NULL) {
        return NULL;
    }
    currentLength = originalLength;

    for (index = 0; index < cJSON_GetArraySize((cJSON *)changes); index++) {
        const cJSON *change = cJSON_GetArrayItem((cJSON *)changes, index);
        char *updated;
        size_t updatedLength;

        updated = apply_single_change(server, uri, current, currentLength, change, &updatedLength);
        if (updated == NULL) {
            free(current);
            return NULL;
        }

        free(current);
        current = updated;
        currentLength = updatedLength;
    }

    *outLength = currentLength;
    return current;
}

/** 用版本与内容快照确认提交，避免语义分析失败掩盖已保存的语法错误文本。 */
static int document_contents_were_committed(SZrStdioServer *server,
                                            SZrString *uri,
                                            const char *content,
                                            size_t contentLength,
                                            TZrSize version) {
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    int isCommitted;

    if (server == ZR_NULL || uri == ZR_NULL || content == ZR_NULL) {
        return 0;
    }
    fileVersion = get_file_version_for_uri(server, uri);
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(server->state, fileVersion, &snapshot)) {
        return 0;
    }
    isCommitted = snapshot.version == version && snapshot.contentLength == (TZrSize)contentLength &&
                  (contentLength == 0 || memcmp(snapshot.content, content, contentLength) == 0);
    ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
    return isCommitted;
}

/** 将 UTF-8 文本交给 LSP 并验证提交；发布 parser 当前版本的诊断，失败由各调用方处置。 */
int update_document_contents(SZrStdioServer *server,
                             SZrString *uri,
                             const char *content,
                             size_t contentLength,
                             TZrSize version) {
    int updateOk;

    if (server == ZR_NULL || uri == ZR_NULL || content == NULL ||
        !ZrCore_Utf8_IsValid((TZrNativeString)content, (TZrSize)contentLength)) {
        return 0;
    }

    (void)ZrLanguageServer_Lsp_UpdateDocument(server->state,
                                               server->context,
                                               uri,
                                               content,
                                               (TZrSize)contentLength,
                                               version);
    updateOk = document_contents_were_committed(server, uri, content, contentLength, version);
    publish_diagnostics(server, uri);
    return updateOk;
}

/** @brief 用磁盘内容重建关闭文档或文件事件的版本，成功后清除失同步状态。
 * BUG: 合成版本递增后，同一连接再次 didOpen 的客户端版本可能更低，parser 因单调门禁拒绝新全文。
 * TODO: 读取失败前已移除旧 parser/analyzer 状态；核对文件事件和 didSave 是否应保留旧状态。
 * TODO: 手工清理未调用语义历史快照的 RemoveUri；核对关闭后旧快照是否仍应可查询。 */
int update_document_contents_from_disk(SZrStdioServer *server, SZrString *uri) {
    char *sourceCode;
    size_t sourceLength;
    SZrFileVersion *fileVersion;
    TZrSize version;
    int success;

    if (server == ZR_NULL || uri == ZR_NULL || server->state == ZR_NULL || server->state->global == ZR_NULL) {
        return 0;
    }

    fileVersion = get_file_version_for_uri(server, uri);
    version = fileVersion != ZR_NULL ? fileVersion->version + 1 : 0;
    if (server->context != ZR_NULL) {
        SZrTypeValue key;
        SZrHashKeyValuePair *pair;

        ZrCore_Value_InitAsRawObject(server->state, &key, &uri->super);
        pair = ZrCore_HashSet_Find(server->state, &server->context->uriToAnalyzerMap, &key);
        if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
            SZrSemanticAnalyzer *analyzer = (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
            if (analyzer != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_Free(server->state, analyzer);
            }
        }
        ZrCore_HashSet_Remove(server->state, &server->context->uriToAnalyzerMap, &key);

        if (server->context->parser != ZR_NULL) {
            ZrLanguageServer_IncrementalParser_RemoveFile(server->state, server->context->parser, uri);
        }
    }

    sourceCode = read_document_text_from_uri(uri, &sourceLength);
    if (sourceCode == ZR_NULL) {
        return 0;
    }

    success = update_document_contents(server, uri, sourceCode, sourceLength, version);
    free(sourceCode);
    if (success) {
        fileVersion = get_file_version_for_uri(server, uri);
        if (fileVersion != ZR_NULL) {
            fileVersion->isOpenDocument = ZR_FALSE;
            clear_document_desynchronization(server, uri);
        }
    }
    return success;
}
