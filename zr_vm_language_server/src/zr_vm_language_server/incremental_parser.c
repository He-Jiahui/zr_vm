//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_language_server/incremental_parser.h"
#include "incremental_change.h"
#include "incremental_token_equivalence.h"
#include "incremental/incremental_syntax_reparse.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server/semantic_analyzer.h"
#include "interface/lsp_interface_internal.h"
#include "zr_vm_parser/parser.h"

#include <string.h>
#include <stdio.h>

typedef struct SZrParserDiagnosticCollector {
    SZrState *state;
    SZrFileVersion *fileVersion;
    TZrBool suppressNextLegacyDiagnostic;
} SZrParserDiagnosticCollector;

static void clear_parser_diagnostics(SZrState *state, SZrFileVersion *fileVersion) {
    if (state == ZR_NULL || fileVersion == ZR_NULL || !fileVersion->parserDiagnostics.isValid) {
        return;
    }

    for (TZrSize index = 0; index < fileVersion->parserDiagnostics.length; index++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&fileVersion->parserDiagnostics, index);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            ZrLanguageServer_Diagnostic_Free(state, *diagPtr);
        }
    }

    fileVersion->parserDiagnostics.length = 0;
}

static TZrBool parser_diagnostics_have_errors(SZrFileVersion *fileVersion) {
    if (fileVersion == ZR_NULL || !fileVersion->parserDiagnostics.isValid) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < fileVersion->parserDiagnostics.length; index++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&fileVersion->parserDiagnostics, index);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL && (*diagPtr)->severity == ZR_DIAGNOSTIC_ERROR) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void collect_parser_diagnostic(TZrPtr userData,
                                      const SZrFileRange *location,
                                      const TZrChar *message,
                                      EZrToken token) {
    SZrParserDiagnosticCollector *collector = (SZrParserDiagnosticCollector *)userData;
    SZrDiagnostic *diagnostic;

    ZR_UNUSED_PARAMETER(token);

    if (collector == ZR_NULL || collector->state == ZR_NULL || collector->fileVersion == ZR_NULL ||
        location == ZR_NULL || message == ZR_NULL) {
        return;
    }

    if (collector->suppressNextLegacyDiagnostic) {
        collector->suppressNextLegacyDiagnostic = ZR_FALSE;
        return;
    }

    diagnostic = ZrLanguageServer_Diagnostic_New(collector->state,
                                                 ZR_DIAGNOSTIC_ERROR,
                                                 *location,
                                                 message,
                                                 "parser_syntax_error");
    if (diagnostic != ZR_NULL) {
        ZrCore_Array_Push(collector->state, &collector->fileVersion->parserDiagnostics, &diagnostic);
    }
}

static void collect_structured_parser_diagnostic(TZrPtr userData,
                                                 const SZrStructuredDiagnostic *structured,
                                                 EZrToken token) {
    SZrParserDiagnosticCollector *collector = (SZrParserDiagnosticCollector *)userData;
    SZrDiagnostic *diagnostic;

    ZR_UNUSED_PARAMETER(token);

    if (collector == ZR_NULL || collector->state == ZR_NULL || collector->fileVersion == ZR_NULL ||
        structured == ZR_NULL) {
        return;
    }

    diagnostic = ZrLanguageServer_Diagnostic_FromStructured(collector->state, structured);
    if (diagnostic != ZR_NULL) {
        ZrCore_Array_Push(collector->state, &collector->fileVersion->parserDiagnostics, &diagnostic);
        collector->suppressNextLegacyDiagnostic = structured->severity == ZR_STRUCTURED_DIAGNOSTIC_ERROR;
    }
}

static SZrFileVersionContentBlock *content_block_new(SZrState *state,
                                                     const TZrChar *content,
                                                     TZrSize contentLength,
                                                     TZrSize contentGeneration) {
    SZrFileVersionContentBlock *block;

    if (state == ZR_NULL || content == ZR_NULL) {
        return ZR_NULL;
    }

    block = (SZrFileVersionContentBlock *)ZrCore_Memory_RawMalloc(
        state->global,
        sizeof(SZrFileVersionContentBlock));
    if (block == ZR_NULL) {
        return ZR_NULL;
    }

    block->content = (TZrChar *)ZrCore_Memory_RawMalloc(state->global, contentLength + 1);
    if (block->content == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, block, sizeof(SZrFileVersionContentBlock));
        return ZR_NULL;
    }

    memcpy(block->content, content, contentLength);
    block->content[contentLength] = '\0';
    block->contentLength = contentLength;
    block->contentGeneration = contentGeneration;
    block->refCount = 1;
    return block;
}

static void content_block_retain(SZrFileVersionContentBlock *block) {
    if (block == ZR_NULL) {
        return;
    }

    block->refCount++;
}

static void content_block_release(SZrState *state, SZrFileVersionContentBlock *block) {
    if (state == ZR_NULL || block == ZR_NULL) {
        return;
    }

    if (block->refCount > 0) {
        block->refCount--;
    }

    if (block->refCount != 0) {
        return;
    }

    if (block->content != ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, block->content, block->contentLength + 1);
    }
    ZrCore_Memory_RawFree(state->global, block, sizeof(SZrFileVersionContentBlock));
}

static void file_version_clear_historical_content(
        SZrState *state,
        SZrFileVersion *fileVersion) {
    TZrSize index;

    if (state == ZR_NULL || fileVersion == ZR_NULL) {
        return;
    }

    for (index = 0; index < fileVersion->historicalContentCount; index++) {
        content_block_release(state, fileVersion->historicalContent[index].contentBlock);
    }
    memset(fileVersion->historicalContent,
           0,
           sizeof(fileVersion->historicalContent));
    fileVersion->historicalContentCount = 0;
}

static void file_version_retain_current_content(
        SZrState *state,
        SZrFileVersion *fileVersion) {
    SZrFileVersionHistoricalContent retained;
    TZrSize index;
    TZrSize retainedCount;

    if (state == ZR_NULL || fileVersion == ZR_NULL ||
        fileVersion->textBlock == ZR_NULL ||
        fileVersion->textBlock->content == ZR_NULL) {
        return;
    }

    retained.contentBlock = fileVersion->textBlock;
    retained.version = fileVersion->version;
    retained.isOpenDocument = fileVersion->isOpenDocument;
    retained.usesFallbackAst = fileVersion->usesFallbackAst;
    retainedCount = fileVersion->historicalContentCount;
    if (retainedCount == ZR_LSP_FILE_VERSION_HISTORICAL_CONTENT_CAPACITY) {
        content_block_release(
                state,
                fileVersion->historicalContent[retainedCount - 1U].contentBlock);
        retainedCount--;
    }

    for (index = retainedCount; index > 0; index--) {
        fileVersion->historicalContent[index] =
                fileVersion->historicalContent[index - 1U];
    }
    fileVersion->historicalContent[0] = retained;
    fileVersion->historicalContentCount = retainedCount + 1U;
}

static TZrBool file_version_content_equals(
        const SZrFileVersion *fileVersion,
        const TZrChar *content,
        TZrSize contentLength) {
    if (fileVersion == ZR_NULL || fileVersion->textBlock == ZR_NULL ||
        fileVersion->textBlock->content == ZR_NULL || content == ZR_NULL ||
        fileVersion->textBlock->contentLength != contentLength) {
        return ZR_FALSE;
    }
    return contentLength == 0 ||
           memcmp(fileVersion->textBlock->content, content, contentLength) == 0;
}

// 创建文件版本
SZrFileVersion *ZrLanguageServer_FileVersion_New(SZrState *state,
                                  SZrString *uri,
                                  const TZrChar *content,
                                  TZrSize contentLength,
                                  TZrSize version) {
    if (state == ZR_NULL || uri == ZR_NULL || content == ZR_NULL) {
        return ZR_NULL;
    }

    SZrFileVersion *fileVersion = (SZrFileVersion *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrFileVersion));
    if (fileVersion == ZR_NULL) {
        return ZR_NULL;
    }

    fileVersion->uri = uri;
    fileVersion->version = version;
    fileVersion->isOpenDocument = ZR_FALSE;
    fileVersion->textBlock = content_block_new(state, content, contentLength, 1);
    if (fileVersion->textBlock == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, fileVersion, sizeof(SZrFileVersion));
        return ZR_NULL;
    }
    memset(fileVersion->historicalContent,
           0,
           sizeof(fileVersion->historicalContent));
    fileVersion->historicalContentCount = 0;
    fileVersion->ast = ZR_NULL;
    fileVersion->usesFallbackAst = ZR_FALSE;
    fileVersion->isDesynchronized = ZR_FALSE;
    fileVersion->isDirty = ZR_TRUE;
    fileVersion->lastChangeRange = ZrParser_FileRange_Create(
        ZrParser_FilePosition_Create(0, 0, 0),
        ZrParser_FilePosition_Create(0, 0, 0),
        uri
    );
    ZrLanguageServer_IncrementalChange_Reset(uri, &fileVersion->lastChangeInfo);
    fileVersion->lastContentHash = ZR_NULL;
    fileVersion->lastContentHashLength = 0;
    fileVersion->hasIncrementalInfo = ZR_FALSE;
    fileVersion->lastParseMode = ZR_INCREMENTAL_PARSE_MODE_FULL_REPARSE;
    ZrCore_Array_Init(state,
                      &fileVersion->parserDiagnostics,
                      sizeof(SZrDiagnostic *),
                      ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);

    return fileVersion;
}

// 释放文件版本
void ZrLanguageServer_FileVersion_Free(SZrState *state, SZrFileVersion *fileVersion) {
    if (state == ZR_NULL || fileVersion == ZR_NULL) {
        return;
    }

    content_block_release(state, fileVersion->textBlock);
    fileVersion->textBlock = ZR_NULL;
    file_version_clear_historical_content(state, fileVersion);

    if (fileVersion->ast != ZR_NULL) {
        ZrParser_Ast_Free(state, fileVersion->ast);
    }

    clear_parser_diagnostics(state, fileVersion);
    ZrCore_Array_Free(state, &fileVersion->parserDiagnostics);

    if (fileVersion->lastContentHash != ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, fileVersion->lastContentHash, fileVersion->lastContentHashLength + 1);
    }

    ZrCore_Memory_RawFree(state->global, fileVersion, sizeof(SZrFileVersion));
}

TZrBool ZrLanguageServer_FileVersionContentSnapshot_Acquire(
    SZrState *state,
    SZrFileVersion *fileVersion,
    SZrFileVersionContentSnapshot *outSnapshot) {
    if (outSnapshot != ZR_NULL) {
        memset(outSnapshot, 0, sizeof(SZrFileVersionContentSnapshot));
    }

    if (state == ZR_NULL || fileVersion == ZR_NULL || outSnapshot == ZR_NULL ||
        fileVersion->textBlock == ZR_NULL || fileVersion->textBlock->content == ZR_NULL) {
        return ZR_FALSE;
    }

    content_block_retain(fileVersion->textBlock);
    outSnapshot->contentBlock = fileVersion->textBlock;
    outSnapshot->content = fileVersion->textBlock->content;
    outSnapshot->uri = fileVersion->uri;
    outSnapshot->version = fileVersion->version;
    outSnapshot->isOpenDocument = fileVersion->isOpenDocument;
    outSnapshot->contentLength = fileVersion->textBlock->contentLength;
    outSnapshot->contentGeneration = fileVersion->textBlock->contentGeneration;
    outSnapshot->usesFallbackAst = fileVersion->usesFallbackAst;

    return ZR_TRUE;
}

void ZrLanguageServer_FileVersionContentSnapshot_Free(SZrState *state,
                                                      SZrFileVersionContentSnapshot *snapshot) {
    if (state == ZR_NULL || snapshot == ZR_NULL) {
        return;
    }

    content_block_release(state, snapshot->contentBlock);

    memset(snapshot, 0, sizeof(SZrFileVersionContentSnapshot));
}

TZrSize ZrLanguageServer_FileVersionHistoricalContentSnapshot_Count(
        const SZrFileVersion *fileVersion) {
    if (fileVersion == ZR_NULL) {
        return 0;
    }

    return fileVersion->historicalContentCount;
}

TZrBool ZrLanguageServer_FileVersionHistoricalContentSnapshot_Acquire(
        SZrState *state,
        SZrFileVersion *fileVersion,
        TZrSize historyIndex,
        SZrFileVersionContentSnapshot *outSnapshot) {
    SZrFileVersionHistoricalContent *historicalContent;

    if (outSnapshot != ZR_NULL) {
        memset(outSnapshot, 0, sizeof(SZrFileVersionContentSnapshot));
    }
    if (state == ZR_NULL || fileVersion == ZR_NULL || outSnapshot == ZR_NULL ||
        historyIndex >= fileVersion->historicalContentCount) {
        return ZR_FALSE;
    }

    historicalContent = &fileVersion->historicalContent[historyIndex];
    if (historicalContent->contentBlock == ZR_NULL ||
        historicalContent->contentBlock->content == ZR_NULL) {
        return ZR_FALSE;
    }

    content_block_retain(historicalContent->contentBlock);
    outSnapshot->contentBlock = historicalContent->contentBlock;
    outSnapshot->content = historicalContent->contentBlock->content;
    outSnapshot->uri = fileVersion->uri;
    outSnapshot->version = historicalContent->version;
    outSnapshot->isOpenDocument = historicalContent->isOpenDocument;
    outSnapshot->contentLength = historicalContent->contentBlock->contentLength;
    outSnapshot->contentGeneration =
            historicalContent->contentBlock->contentGeneration;
    outSnapshot->usesFallbackAst = historicalContent->usesFallbackAst;
    return ZR_TRUE;
}

// 更新文件版本内容
TZrBool ZrLanguageServer_FileVersion_UpdateContent(SZrState *state,
                                 SZrFileVersion *fileVersion,
                                 const TZrChar *content,
                                 TZrSize contentLength,
                                 TZrSize version,
                                 const SZrFileChangeInfo *changeInfo) {
    SZrFileVersionContentBlock *newBlock;
    TZrSize contentGeneration;

    if (state == ZR_NULL || fileVersion == ZR_NULL || content == ZR_NULL ||
        changeInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    contentGeneration = fileVersion->textBlock != ZR_NULL ?
        fileVersion->textBlock->contentGeneration + 1 :
        1;
    newBlock = content_block_new(state, content, contentLength, contentGeneration);
    if (newBlock == ZR_NULL) {
        return ZR_FALSE;
    }

    file_version_retain_current_content(state, fileVersion);
    fileVersion->textBlock = newBlock;
    fileVersion->version = version;
    fileVersion->isDirty = !changeInfo->isTokenEquivalent;
    fileVersion->lastChangeInfo = *changeInfo;
    fileVersion->lastChangeRange = changeInfo->newRange;
    fileVersion->hasIncrementalInfo = ZR_TRUE; /* 标记有增量信息 */
    fileVersion->lastParseMode = ZR_INCREMENTAL_PARSE_MODE_FULL_REPARSE;

    if (!changeInfo->isTokenEquivalent) {
        clear_parser_diagnostics(state, fileVersion);
    }

    if (fileVersion->lastContentHash != ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, fileVersion->lastContentHash, fileVersion->lastContentHashLength + 1);
        fileVersion->lastContentHash = ZR_NULL;
        fileVersion->lastContentHashLength = 0;
    }

    return ZR_TRUE;
}

// 创建增量解析器
SZrIncrementalParser *ZrLanguageServer_IncrementalParser_New(SZrState *state) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    SZrIncrementalParser *parser = (SZrIncrementalParser *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrIncrementalParser));
    if (parser == ZR_NULL) {
        return ZR_NULL;
    }

    parser->state = state;
    ZrCore_HashSet_Construct(&parser->uriToFileMap);
    ZrCore_HashSet_Init(state, &parser->uriToFileMap, ZR_LSP_HASH_TABLE_INITIAL_SIZE_LOG2);
    parser->parserState = ZR_NULL; // 延迟初始化
    parser->enableIncrementalParse = ZR_TRUE; // 默认启用增量解析
    parser->enableContentHash = ZR_TRUE; // 默认启用内容哈希
    parser->retainedPreviousAstOutput = ZR_NULL;

    return parser;
}

// 释放增量解析器
void ZrLanguageServer_IncrementalParser_Free(SZrState *state, SZrIncrementalParser *parser) {
    if (state == ZR_NULL || parser == ZR_NULL) {
        return;
    }

    // 释放所有文件版本
    if (parser->uriToFileMap.isValid && parser->uriToFileMap.buckets != ZR_NULL) {
        // 遍历哈希表释放所有文件版本和节点
        for (TZrSize i = 0; i < parser->uriToFileMap.capacity; i++) {
            SZrHashKeyValuePair *pair = parser->uriToFileMap.buckets[i];
            while (pair != ZR_NULL) {
                // 释放节点中存储的数据
                if (pair->key.type != ZR_VALUE_TYPE_NULL) {
                    if (pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
                        SZrFileVersion *fileVersion =
                            (SZrFileVersion *)pair->value.value.nativeObject.nativePointer;
                        if (fileVersion != ZR_NULL) {
                            ZrLanguageServer_FileVersion_Free(state, fileVersion);
                        }
                    }
                }
                SZrHashKeyValuePair *next = pair->next;
                /* HashSet pairs are owned by the pair pool released by Deconstruct. */
                pair = next;
            }
        }
        ZrCore_HashSet_Deconstruct(state, &parser->uriToFileMap);
    }

    if (parser->parserState != ZR_NULL) {
        ZrParser_State_Free(parser->parserState);
    }

    ZrCore_Memory_RawFree(state->global, parser, sizeof(SZrIncrementalParser));
}

static TZrBool incremental_parser_update_file(
        SZrState *state,
        SZrIncrementalParser *parser,
        SZrString *uri,
        const TZrChar *content,
        TZrSize contentLength,
        TZrSize version,
        TZrBool isOpenDocument) {
    if (state == ZR_NULL || parser == ZR_NULL || uri == ZR_NULL || content == ZR_NULL) {
        return ZR_FALSE;
    }

    // 查找是否已存在
    SZrFileVersion *fileVersion = ZrLanguageServer_IncrementalParser_GetFileVersion(parser, uri);
    if (fileVersion != ZR_NULL) {
        TZrBool isOpeningSyntheticSnapshot =
                isOpenDocument && !fileVersion->isOpenDocument &&
                version == fileVersion->version;
        if (!isOpeningSyntheticSnapshot && version <= fileVersion->version) {
            return ZR_FALSE;
        }
        if (file_version_content_equals(fileVersion, content, contentLength)) {
            fileVersion->version = version;
            fileVersion->isOpenDocument = isOpenDocument;
            ZrLanguageServer_IncrementalChange_Reset(uri, &fileVersion->lastChangeInfo);
            fileVersion->lastChangeRange = fileVersion->lastChangeInfo.newRange;
            fileVersion->hasIncrementalInfo = ZR_FALSE;
            return ZR_TRUE;
        }
        // 更新现有文件
        SZrFileChangeInfo changeInfo;
        ZrLanguageServer_IncrementalChange_Compute(
                uri,
                fileVersion->textBlock->content,
                fileVersion->textBlock->contentLength,
                content,
                contentLength,
                &changeInfo);
        changeInfo.isTokenEquivalent = fileVersion->ast != ZR_NULL &&
                                       !fileVersion->usesFallbackAst &&
                                       ZrLanguageServer_IncrementalTokenStreams_AreEquivalent(
                                               state,
                                               uri,
                                               fileVersion->textBlock->content,
                                               fileVersion->textBlock->contentLength,
                                               content,
                                               contentLength);
        TZrBool updated = ZrLanguageServer_FileVersion_UpdateContent(
                state,
                fileVersion,
                content,
                contentLength,
                version,
                &changeInfo);
        if (updated) {
            fileVersion->isOpenDocument = isOpenDocument;
        }
        return updated;
    } else {
        // 创建新文件
        fileVersion = ZrLanguageServer_FileVersion_New(state, uri, content, contentLength, version);
        if (fileVersion == ZR_NULL) {
            return ZR_FALSE;
        }
        fileVersion->isOpenDocument = isOpenDocument;

        // 添加到哈希表
        SZrTypeValue key;
        ZrCore_Value_InitAsRawObject(state, &key, &uri->super);

        // 使用 ZrCore_HashSet_Add 添加，然后设置值
        SZrHashKeyValuePair *pair = ZrCore_HashSet_Add(state, &parser->uriToFileMap, &key);
        if (pair != ZR_NULL) {
            // 将 SZrFileVersion 指针存储为原生指针
            SZrTypeValue value;
            ZrCore_Value_InitAsNativePointer(state, &value, (TZrPtr)fileVersion);
            ZrCore_Value_Copy(state, &pair->value, &value);
        }
    }

    return ZR_TRUE;
}

// 更新 workspace 或 provider cache 中的文件内容。
TZrBool ZrLanguageServer_IncrementalParser_UpdateFile(SZrState *state,
                                      SZrIncrementalParser *parser,
                                      SZrString *uri,
                                      const TZrChar *content,
                                      TZrSize contentLength,
                                      TZrSize version) {
    return incremental_parser_update_file(
            state,
            parser,
            uri,
            content,
            contentLength,
            version,
            ZR_FALSE);
}

TZrBool ZrLanguageServer_IncrementalParser_UpdateOpenDocument(
        SZrState *state,
        SZrIncrementalParser *parser,
        SZrString *uri,
        const TZrChar *content,
        TZrSize contentLength,
        TZrSize version) {
    return incremental_parser_update_file(
            state,
            parser,
            uri,
            content,
            contentLength,
            version,
            ZR_TRUE);
}

// 辅助函数：计算内容哈希（简化实现）
static void compute_content_hash(SZrState *state, const TZrChar *content, TZrSize length,
                                  TZrChar **hash, TZrSize *hashLength) {
    if (state == ZR_NULL || content == ZR_NULL || hash == ZR_NULL || hashLength == ZR_NULL) {
        return;
    }

    // TODO: 简化实现：使用简单的哈希算法
    TZrUInt64 hashValue = 0;
    for (TZrSize i = 0; i < length; i++) {
        hashValue = hashValue * ZR_LSP_HASH_MULTIPLIER + (TZrUInt8)content[i];
    }

    // 转换为字符串
    TZrChar hashStr[ZR_LSP_SHORT_TEXT_BUFFER_LENGTH];
    snprintf(hashStr, sizeof(hashStr), "%llx", (unsigned long long)hashValue);
    TZrSize len = strlen(hashStr);

    *hash = (TZrChar *)ZrCore_Memory_RawMalloc(state->global, len + 1);
    if (*hash != ZR_NULL) {
        memcpy(*hash, hashStr, len);
        (*hash)[len] = '\0';
        *hashLength = len;
    }
}

// 辅助函数：比较内容哈希
static TZrBool compare_content_hash(const TZrChar *hash1, TZrSize len1,
                                   const TZrChar *hash2, TZrSize len2) {
    if (hash1 == ZR_NULL || hash2 == ZR_NULL) {
        return ZR_FALSE;
    }
    if (len1 != len2) {
        return ZR_FALSE;
    }
    return memcmp(hash1, hash2, len1) == 0;
}

// 解析文件（增量）
TZrBool ZrLanguageServer_IncrementalParser_Parse(SZrState *state,
                                 SZrIncrementalParser *parser,
                                 SZrString *uri) {
    if (state == ZR_NULL || parser == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    SZrFileVersion *fileVersion = ZrLanguageServer_IncrementalParser_GetFileVersion(parser, uri);
    SZrFileVersionContentSnapshot snapshot;
    if (fileVersion == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        return ZR_FALSE;
    }

    if (!fileVersion->isDirty &&
        fileVersion->ast != ZR_NULL &&
        fileVersion->hasIncrementalInfo &&
        fileVersion->lastChangeInfo.isTokenEquivalent) {
        if (parser->enableContentHash) {
            if (fileVersion->lastContentHash != ZR_NULL) {
                ZrCore_Memory_RawFree(
                        state->global,
                        fileVersion->lastContentHash,
                        fileVersion->lastContentHashLength + 1);
            }
            compute_content_hash(
                    state,
                    snapshot.content,
                    snapshot.contentLength,
                    &fileVersion->lastContentHash,
                    &fileVersion->lastContentHashLength);
        }
        fileVersion->lastParseMode = ZR_INCREMENTAL_PARSE_MODE_TOKEN_EQUIVALENT;
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_TRUE;
    }

    // 如果不需要重新解析，检查内容哈希（如果启用）
    if (!fileVersion->isDirty && fileVersion->ast != ZR_NULL) {
        if (parser->enableContentHash && fileVersion->lastContentHash != ZR_NULL) {
            // 计算当前内容哈希
            TZrChar *currentHash = ZR_NULL;
            TZrSize currentHashLength = 0;
            compute_content_hash(state, snapshot.content, snapshot.contentLength,
                                &currentHash, &currentHashLength);

            if (currentHash != ZR_NULL) {
                // 比较哈希
                TZrBool isSame = compare_content_hash(fileVersion->lastContentHash,
                                                   fileVersion->lastContentHashLength,
                                                   currentHash, currentHashLength);
                ZrCore_Memory_RawFree(state->global, currentHash, strlen(currentHash) + 1);

                if (isSame) {
                    // 内容未改变，不需要重新解析
                    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
                    return ZR_TRUE;
                }
            }
        } else {
            // 未启用内容哈希，直接返回
            ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
            return ZR_TRUE;
        }
    }

    // Reuse the script and untouched top-level declarations only after a
    // declaration-local parse proves that every retained source range is stable.
    if (parser->enableIncrementalParse && fileVersion->ast != ZR_NULL &&
        fileVersion->hasIncrementalInfo) {
        SZrFileVersionContentSnapshot previousSnapshot = {0};

        if (ZrLanguageServer_FileVersionHistoricalContentSnapshot_Acquire(
                    state, fileVersion, 0U, &previousSnapshot)) {
            TZrBool reparsed = ZrLanguageServer_IncrementalSyntaxReparse_TryDeclaration(
                    state,
                    uri,
                    previousSnapshot.content,
                    previousSnapshot.contentLength,
                    snapshot.content,
                    snapshot.contentLength,
                    fileVersion->ast,
                    &fileVersion->lastChangeInfo);

            ZrLanguageServer_FileVersionContentSnapshot_Free(state, &previousSnapshot);
            if (reparsed) {
                fileVersion->usesFallbackAst = ZR_FALSE;
                fileVersion->isDirty = ZR_FALSE;
                fileVersion->lastParseMode = ZR_INCREMENTAL_PARSE_MODE_DECLARATION_REPARSE;
                if (parser->enableContentHash) {
                    if (fileVersion->lastContentHash != ZR_NULL) {
                        ZrCore_Memory_RawFree(
                                state->global,
                                fileVersion->lastContentHash,
                                fileVersion->lastContentHashLength + 1);
                    }
                    compute_content_hash(
                            state,
                            snapshot.content,
                            snapshot.contentLength,
                            &fileVersion->lastContentHash,
                            &fileVersion->lastContentHashLength);
                }
                ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
                return ZR_TRUE;
            }
        }
    }

    // 完全重新解析
    {
        SZrParserState parserState;
        SZrParserDiagnosticCollector collector;
        SZrString *sourceName = uri; // 使用 URI 作为源文件名
        SZrAstNode *previousAst = fileVersion->ast;
        SZrAstNode *parsedAst;

        clear_parser_diagnostics(state, fileVersion);
        ZrParser_State_Init(&parserState, state, snapshot.content, snapshot.contentLength, sourceName);
        if (parserState.hasError) {
            ZrParser_State_Free(&parserState);
            ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
            return ZR_FALSE;
        }

        collector.state = state;
        collector.fileVersion = fileVersion;
        collector.suppressNextLegacyDiagnostic = ZR_FALSE;
        parserState.errorCallback = collect_parser_diagnostic;
        parserState.structuredErrorCallback = collect_structured_parser_diagnostic;
        parserState.errorUserData = &collector;
        parserState.suppressErrorOutput = ZR_TRUE;
        parsedAst = ZrParser_ParseWithState(&parserState);
        ZrParser_State_Free(&parserState);

        if (parsedAst != ZR_NULL) {
            if (!parser_diagnostics_have_errors(fileVersion) || previousAst == ZR_NULL) {
                if (previousAst != ZR_NULL && previousAst != parsedAst) {
                    if (parser->retainedPreviousAstOutput != ZR_NULL) {
                        *parser->retainedPreviousAstOutput = previousAst;
                    } else {
                        ZrParser_Ast_Free(state, previousAst);
                    }
                }
                fileVersion->ast = parsedAst;
                fileVersion->usesFallbackAst = ZR_FALSE;
            } else {
                ZrParser_Ast_Free(state, parsedAst);
                fileVersion->usesFallbackAst = ZR_TRUE;
            }
        } else if (fileVersion->parserDiagnostics.length > 0 && previousAst != ZR_NULL) {
            fileVersion->usesFallbackAst = ZR_TRUE;
        } else {
            if (previousAst != ZR_NULL) {
                if (parser->retainedPreviousAstOutput != ZR_NULL) {
                    *parser->retainedPreviousAstOutput = previousAst;
                } else {
                    ZrParser_Ast_Free(state, previousAst);
                }
            }
            fileVersion->ast = ZR_NULL;
            fileVersion->usesFallbackAst = ZR_FALSE;
        }
    }

    if (fileVersion->ast != ZR_NULL || fileVersion->parserDiagnostics.length > 0) {
        fileVersion->isDirty = ZR_FALSE;
        fileVersion->lastParseMode = ZR_INCREMENTAL_PARSE_MODE_FULL_REPARSE;

        // 计算并存储内容哈希
        if (parser->enableContentHash) {
            if (fileVersion->lastContentHash != ZR_NULL) {
                ZrCore_Memory_RawFree(state->global, fileVersion->lastContentHash, fileVersion->lastContentHashLength + 1);
            }
            compute_content_hash(state, snapshot.content, snapshot.contentLength,
                                &fileVersion->lastContentHash,
                                &fileVersion->lastContentHashLength);
        }

        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_TRUE;
    }

    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    return ZR_FALSE;
}

TZrBool ZrLanguageServer_IncrementalParser_ParseRetainingPreviousAst(
        SZrState *state,
        SZrIncrementalParser *parser,
        SZrString *uri,
        SZrAstNode **retainedPreviousAst) {
    TZrBool result;

    if (state == ZR_NULL || parser == ZR_NULL || uri == ZR_NULL ||
        retainedPreviousAst == ZR_NULL ||
        parser->retainedPreviousAstOutput != ZR_NULL) {
        return ZR_FALSE;
    }

    *retainedPreviousAst = ZR_NULL;
    parser->retainedPreviousAstOutput = retainedPreviousAst;
    result = ZrLanguageServer_IncrementalParser_Parse(state, parser, uri);
    parser->retainedPreviousAstOutput = ZR_NULL;
    return result;
}

// 获取 AST
SZrAstNode *ZrLanguageServer_IncrementalParser_GetAST(SZrIncrementalParser *parser,
                                       SZrString *uri) {
    if (parser == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }

    SZrFileVersion *fileVersion = ZrLanguageServer_IncrementalParser_GetFileVersion(parser, uri);
    if (fileVersion == ZR_NULL) {
        return ZR_NULL;
    }

    // 如果 AST 不存在或需要重新解析，先解析
    if (fileVersion->ast == ZR_NULL && !fileVersion->isDirty && fileVersion->parserDiagnostics.length > 0) {
        return ZR_NULL;
    }

    if (fileVersion->ast == ZR_NULL || fileVersion->isDirty) {
        if (!ZrLanguageServer_IncrementalParser_Parse(parser->state, parser, uri)) {
            return ZR_NULL;
        }
    }

    return fileVersion->ast;
}

// 移除文件
void ZrLanguageServer_IncrementalParser_RemoveFile(SZrState *state,
                                    SZrIncrementalParser *parser,
                                    SZrString *uri) {
    SZrTypeValue key;
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || parser == ZR_NULL || uri == ZR_NULL) {
        return;
    }

    ZrCore_Value_InitAsRawObject(state, &key, &uri->super);
    pair = ZrCore_HashSet_Find(state, &parser->uriToFileMap, &key);
    if (pair == ZR_NULL) {
        pair = ZrLanguageServer_Lsp_FindEquivalentUriKeyPair(state, &parser->uriToFileMap, uri);
    }
    if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
        SZrFileVersion *fileVersion = (SZrFileVersion *)pair->value.value.nativeObject.nativePointer;

        key = pair->key;
        ZrCore_HashSet_Remove(state, &parser->uriToFileMap, &key);
        ZrLanguageServer_FileVersion_Free(state, fileVersion);
    }
}

// 获取文件版本
SZrFileVersion *ZrLanguageServer_IncrementalParser_GetFileVersion(SZrIncrementalParser *parser,
                                                  SZrString *uri) {
    if (parser == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }

    // 从哈希表中查找
    SZrTypeValue key;
    ZrCore_Value_InitAsRawObject(parser->state, &key, &uri->super);

    SZrHashKeyValuePair *pair = ZrCore_HashSet_Find(parser->state, &parser->uriToFileMap, &key);
    if (pair == ZR_NULL) {
        pair = ZrLanguageServer_Lsp_FindEquivalentUriKeyPair(parser->state, &parser->uriToFileMap, uri);
    }
    if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
        // 从原生指针中获取 SZrFileVersion
        return (SZrFileVersion *)pair->value.value.nativeObject.nativePointer;
    }

    return ZR_NULL;
}
