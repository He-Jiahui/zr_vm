/* 文件版本是 LSP 文档更新与语义查询之间的边界：文本快照独立持有字节，AST 按解析结果复用或回退。 */

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

/** 同一次完整解析的两种诊断回调共享状态，避免结构化错误被旧回调重复上报。 */
typedef struct SZrParserDiagnosticCollector {
    SZrState *state;
    SZrFileVersion *fileVersion;
    TZrBool suppressNextLegacyDiagnostic;
} SZrParserDiagnosticCollector;

/** @brief 内容真正变化时释放旧语法诊断，防止下一次发布混入旧版本位置。 */
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

/** @brief 完整解析据此决定采用新 AST，还是保留供编辑器查询的最后有效 AST。 */
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

/** @brief 接收 parser 旧式错误回调；结构化错误已入库时跳过其紧随的重复事件。 */
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

/** @brief 将 parser 的结构化诊断转成 LSP 诊断，并协调随后旧式回调的去重。 */
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

/** @brief 为文档版本创建自有文本块；快照和历史版本依赖它独立于更新存活。 */
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

/** @brief 借出当前或历史快照时增加文本块引用；调用方须配对释放快照。 */
static void content_block_retain(SZrFileVersionContentBlock *block) {
    if (block == ZR_NULL) {
        return;
    }

    block->refCount++;
}

/** @brief 版本槽或快照不再持有文本时归还引用，最后一个引用负责回收字节。 */
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

/** @brief 销毁文件版本时归还历史内容，已借出的快照仍由引用计数保活。 */
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

/** @brief 更新前将当前文本放入最近历史槽，供局部重解析和语义快照对照。 */
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
    /* 历史仅保留最近两版；更早文本如被外部快照借用，引用计数继续保活。 */
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

/** @brief 比较文本以便调用者仅推进版本号，并保留 AST、诊断和文本快照。 */
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

/**
 * @brief 为新 URI 建立未解析的文件状态；由解析器 URI 映射接管该对象。
 * @note 文本块由版本持有，URI 是借用对象，调用方负责维持其生命周期。
 */
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
    /* BUG: Array_Init 分配失败仍可能留下 isValid=true、head=NULL；诊断回调 Push
     * 随后断言 head 非空。需在数组构造边界和此处一起验证 OOM 语义。 */

    return fileVersion;
}

/** @brief 由解析器移除或析构路径回收文本、AST、诊断及历史引用；不释放借用 URI。 */
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

/**
 * @brief 借用当前版本的稳定字节供 LSP 查询使用，跨文档更新仍可读取同一文本。
 * @note 返回后须调用 Snapshot_Free；URI 仍借用文件对象，不随文本块延寿。
 */
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

/** @brief 归还一次内容快照并清空句柄，供查询的所有早退路径配对调用。 */
void ZrLanguageServer_FileVersionContentSnapshot_Free(SZrState *state,
                                                      SZrFileVersionContentSnapshot *snapshot) {
    if (state == ZR_NULL || snapshot == ZR_NULL) {
        return;
    }

    content_block_release(state, snapshot->contentBlock);

    memset(snapshot, 0, sizeof(SZrFileVersionContentSnapshot));
}

/** @brief 暴露可供增量解析和语义缓存对照的最近历史版本数量。 */
TZrSize ZrLanguageServer_FileVersionHistoricalContentSnapshot_Count(
        const SZrFileVersion *fileVersion) {
    if (fileVersion == ZR_NULL) {
        return 0;
    }

    return fileVersion->historicalContentCount;
}

/** @brief 借用指定历史文本；顺序从最新到更旧，调用者须释放返回的快照。 */
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

/**
 * @brief 将一次已接受的文档更新提交为新文本块，同时保留上一版供局部解析。
 * @note 分配失败不推进版本；token 等价时 AST 与旧诊断仍被视为可复用。
 */
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
    /* isDirty、变更信息和解析模式共同决定 AST 是否重算及下游缓存失效范围。 */
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

/**
 * @brief 为 LSP context 创建 URI 到文件版本的状态表，默认启用 token 与声明级复用。
 * BUG: HashSet_Init 失败仅写 isValid，构造仍返回 parser；后续新文件插入会失败。
 */
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
    parser->parserState = ZR_NULL; // TODO: 仓内仅在此赋空及析构，核对该字段是否仍由外部 ABI 使用。
    parser->enableIncrementalParse = ZR_TRUE; // 默认启用增量解析
    parser->enableContentHash = ZR_TRUE; // 默认启用内容哈希
    parser->retainedPreviousAstOutput = ZR_NULL;

    return parser;
}

/** @brief context 析构时释放 URI 表内的文件版本，再释放哈希表和解析器。 */
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

/**
 * @brief 汇合磁盘缓存和打开文档更新，先验证版本序，再选择相同文本、token 复用或重解析。
 * @note 打开文档可接管同版本的磁盘合成快照；其他同版或倒退版本均被拒绝。
 */
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
        /* 同内容接管或推进版本不改变 AST；无差异意味着没有局部失效范围。 */
        if (file_version_content_equals(fileVersion, content, contentLength)) {
            fileVersion->version = version;
            fileVersion->isOpenDocument = isOpenDocument;
            ZrLanguageServer_IncrementalChange_Reset(uri, &fileVersion->lastChangeInfo);
            fileVersion->lastChangeRange = fileVersion->lastChangeInfo.newRange;
            fileVersion->hasIncrementalInfo = ZR_FALSE;
            return ZR_TRUE;
        }
        /* 仅有非回退 AST 才允许 token 等价快速路径，避免延续已知过时语法树。 */
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

        /* BUG: Add 可因初始化、扩容或分配失败返回 NULL；仍返回成功且泄漏 fileVersion，
         * 后续 GetFileVersion 找不到此 URI，上层误以为文档更新成功。 */
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

/** @brief 接收 workspace/provider 的磁盘文本，供项目索引按版本更新并解析。 */
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

/** @brief 接收 LSP 打开文档的内存文本，其版本可在首次打开时接管同版磁盘快照。 */
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

/** @brief 为干净 AST 的重复读取保存内容指纹；调用方拥有返回的分配内存。 */
static void compute_content_hash(SZrState *state, const TZrChar *content, TZrSize length,
                                  TZrChar **hash, TZrSize *hashLength) {
    if (state == ZR_NULL || content == ZR_NULL || hash == ZR_NULL || hashLength == ZR_NULL) {
        return;
    }

    /* 仅作已有内容快速判断，哈希冲突不能证明文本真正相同。 */
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

/** @brief 比较缓存指纹，供无需重解析的快速读取分支使用。 */
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

/**
 * @brief 为指定 URI 更新解析结果：优先 token 复用，再尝试单声明替换，最后完整解析。
 * @note 语法错误可保留上一棵有效 AST，并以 usesFallbackAst 告知编辑器查询不能当成当前结果。
 * BUG: 完整解析在 State_Init 预读后才设置 suppressErrorOutput；未终止字符串可直接写 stdout，
 *      stdio 文档更新因此可能破坏 JSON-RPC 帧流。
 */
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

    /* token 与位置均等价时继续借用旧 AST；内容代际仍须更新供语义快照识别。 */
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

    /* 未标脏的 AST 仅在缓存指纹仍匹配时继续使用；此处不是内容正确性的独立证明。 */
    if (!fileVersion->isDirty && fileVersion->ast != ZR_NULL) {
        if (parser->enableContentHash && fileVersion->lastContentHash != ZR_NULL) {
            /* 重读当前快照以守护干净 AST 的快捷返回。 */
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

    /* 只有声明级重解析证明未触及节点位置稳定，才能原位替换并保留其余 AST。
     * BUG: 此门槛未排除 usesFallbackAst；V1 有效、V2 等长语法错保留 V1 AST、V3
     *      等长改另一声明时，局部成功可把 V1 当成 V3 并清除 V2 的错误诊断。 */
    if (parser->enableIncrementalParse && parser->retainedPreviousAstOutput == ZR_NULL &&
        fileVersion->ast != ZR_NULL &&
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

    /* 完整解析负责收集当前语法诊断；保留旧 AST 时须明确标记回退供下游避用。 */
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

        /* 交出旧 AST 仅用于调用方的语义快照迁移，否则由本层释放。 */
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

/** @brief 为语义缓存迁移临时接收上一棵 AST；非重入，调用方接管输出节点。 */
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

/**
 * @brief 按需解析当前 URI；已失败且有诊断的版本不反复解析，成功时借出 AST。
 * TODO: 项目依赖扫描直接读取返回 AST，尚未确认语法错误的 fallback AST 是否会被当成
 *       当前导入图；核对项目 refresh 入口对 usesFallbackAst 的隔离。
 */
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

/** @brief 文档关闭或项目移除时按 URI 等价规则删除版本；调用方不得再持有 AST 指针。 */
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

/** @brief 查询 URI 对应的借用文件状态；兼容等价但拼写不同的 file URI。 */
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
