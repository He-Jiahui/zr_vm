//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_LANGUAGE_SERVER_INCREMENTAL_PARSER_H
#define ZR_VM_LANGUAGE_SERVER_INCREMENTAL_PARSER_H

#include "zr_vm_language_server/conf.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/string.h"

typedef struct SZrDiagnostic SZrDiagnostic;

typedef struct SZrFileVersionContentBlock {
    TZrChar *content;
    TZrSize contentLength;
    TZrSize contentGeneration;
    TZrSize refCount;
} SZrFileVersionContentBlock;

typedef struct SZrFileVersionHistoricalContent {
    SZrFileVersionContentBlock *contentBlock;
    TZrSize version;
    TZrBool isOpenDocument;
    TZrBool usesFallbackAst;
} SZrFileVersionHistoricalContent;

enum EZrFileChangeImpact {
    ZR_FILE_CHANGE_IMPACT_NONE,
    ZR_FILE_CHANGE_IMPACT_MODULE,
    ZR_FILE_CHANGE_IMPACT_DECLARATION_SIGNATURE,
    ZR_FILE_CHANGE_IMPACT_DECLARATION_BODY,
};

typedef enum EZrFileChangeImpact EZrFileChangeImpact;

enum EZrIncrementalParseMode {
    ZR_INCREMENTAL_PARSE_MODE_FULL_REPARSE = 0,
    ZR_INCREMENTAL_PARSE_MODE_TOKEN_EQUIVALENT,
    ZR_INCREMENTAL_PARSE_MODE_DECLARATION_REPARSE,
};

typedef enum EZrIncrementalParseMode EZrIncrementalParseMode;

typedef struct SZrFileChangeInfo {
    SZrFileRange oldRange;
    SZrFileRange newRange;
    EZrFileChangeImpact impact;
    EZrAstNodeType declarationType;
    SZrFileRange declarationRange;
    TZrBool hasDeclaration;
    TZrBool isTokenEquivalent;
} SZrFileChangeInfo;

// 文件版本
typedef struct SZrFileVersion {
    SZrString *uri;                   // 文件 URI
    TZrSize version;                  // 版本号
    TZrBool isOpenDocument;           // 客户端 overlay，而非 workspace disk cache
    SZrFileVersionContentBlock *textBlock; // 当前内容块
    SZrFileVersionHistoricalContent
            historicalContent[ZR_LSP_FILE_VERSION_HISTORICAL_CONTENT_CAPACITY];
    TZrSize historicalContentCount;   // 按新到旧保存，最多两份
    SZrAstNode *ast;                  // 解析后的 AST
    TZrBool usesFallbackAst;            // 当前 AST 是否是旧版本保留下来的 last-good 快照
    TZrBool isDesynchronized;           // 最后一次同步通知无效，语义请求必须 fail closed
    TZrBool isDirty;                    // 是否需要重新解析
    SZrFileRange lastChangeRange;     // 最后变更的范围（用于增量解析）
    SZrFileChangeInfo lastChangeInfo; // 最后变更的旧/新范围与声明级影响
    TZrChar *lastContentHash;           // 内容哈希（用于快速比较，可选）
    TZrSize lastContentHashLength;    // 哈希长度
    TZrBool hasIncrementalInfo;         // 是否有增量信息
    EZrIncrementalParseMode lastParseMode; // actual parse path used for the current AST
    SZrArray parserDiagnostics;      // 语法诊断信息（SZrDiagnostic*）
} SZrFileVersion;

typedef struct SZrFileVersionContentSnapshot {
    SZrString *uri;
    TZrSize version;
    TZrBool isOpenDocument;
    TZrChar *content;
    TZrSize contentLength;
    TZrSize contentGeneration;
    TZrBool usesFallbackAst;
    SZrFileVersionContentBlock *contentBlock;
} SZrFileVersionContentSnapshot;

// 增量解析器
typedef struct SZrIncrementalParser {
    SZrState *state;
    SZrHashSet uriToFileMap;          // URI 到文件版本的映射（值为SZrFileVersion*）
    SZrParserState *parserState;      // 解析器状态（共享）
    TZrBool enableIncrementalParse;     // 是否启用增量解析
    TZrBool enableContentHash;          // 是否使用内容哈希优化
    SZrAstNode **retainedPreviousAstOutput; // 单次parse的旧AST所有权移交槽
} SZrIncrementalParser;

// 增量解析器管理函数

// 创建增量解析器
ZR_LANGUAGE_SERVER_API SZrIncrementalParser *ZrLanguageServer_IncrementalParser_New(SZrState *state);

// 释放增量解析器
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_IncrementalParser_Free(SZrState *state, 
                                                     SZrIncrementalParser *parser);

// 更新文件内容
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_IncrementalParser_UpdateFile(SZrState *state,
                                                              SZrIncrementalParser *parser,
                                                              SZrString *uri,
                                                              const TZrChar *content,
                                                              TZrSize contentLength,
                                                              TZrSize version);
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_IncrementalParser_UpdateOpenDocument(
        SZrState *state,
        SZrIncrementalParser *parser,
        SZrString *uri,
        const TZrChar *content,
        TZrSize contentLength,
        TZrSize version);

// 解析文件（增量）
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_IncrementalParser_Parse(SZrState *state,
                                                        SZrIncrementalParser *parser,
                                                        SZrString *uri);
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_IncrementalParser_ParseRetainingPreviousAst(
    SZrState *state,
    SZrIncrementalParser *parser,
    SZrString *uri,
    /* Non-null result transfers the previous AST to the caller. */
    SZrAstNode **retainedPreviousAst);

// 获取 AST
ZR_LANGUAGE_SERVER_API SZrAstNode *ZrLanguageServer_IncrementalParser_GetAST(SZrIncrementalParser *parser,
                                                               SZrString *uri);

// 移除文件
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_IncrementalParser_RemoveFile(SZrState *state,
                                                           SZrIncrementalParser *parser,
                                                           SZrString *uri);

// 获取文件版本
ZR_LANGUAGE_SERVER_API SZrFileVersion *ZrLanguageServer_IncrementalParser_GetFileVersion(SZrIncrementalParser *parser,
                                                                           SZrString *uri);

// 文件版本管理函数

// 创建文件版本
ZR_LANGUAGE_SERVER_API SZrFileVersion *ZrLanguageServer_FileVersion_New(SZrState *state,
                                                          SZrString *uri,
                                                          const TZrChar *content,
                                                          TZrSize contentLength,
                                                          TZrSize version);

// 释放文件版本
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_FileVersion_Free(SZrState *state, SZrFileVersion *fileVersion);

// 更新文件版本内容
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_FileVersion_UpdateContent(SZrState *state,
                                                         SZrFileVersion *fileVersion,
                                                         const TZrChar *content,
                                                         TZrSize contentLength,
                                                         TZrSize version,
                                                         const SZrFileChangeInfo *changeInfo);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_FileVersionContentSnapshot_Acquire(
    SZrState *state,
    SZrFileVersion *fileVersion,
    SZrFileVersionContentSnapshot *outSnapshot);
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_FileVersionContentSnapshot_Free(
    SZrState *state,
    SZrFileVersionContentSnapshot *snapshot);
ZR_LANGUAGE_SERVER_API TZrSize
ZrLanguageServer_FileVersionHistoricalContentSnapshot_Count(
    const SZrFileVersion *fileVersion);
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_FileVersionHistoricalContentSnapshot_Acquire(
    SZrState *state,
    SZrFileVersion *fileVersion,
    TZrSize historyIndex,
    SZrFileVersionContentSnapshot *outSnapshot);

#endif //ZR_VM_LANGUAGE_SERVER_INCREMENTAL_PARSER_H
