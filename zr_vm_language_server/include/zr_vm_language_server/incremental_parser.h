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

// 文件版本
typedef struct SZrFileVersion {
    SZrString *uri;                   // 文件 URI
    TZrSize version;                  // 版本号
    TChar *content;                    // 文件内容
    TZrSize contentLength;            // 内容长度
    SZrAstNode *ast;                  // 解析后的 AST
    TBool isDirty;                    // 是否需要重新解析
    SZrFileRange lastChangeRange;     // 最后变更的范围（用于增量解析）
    TChar *lastContentHash;           // 内容哈希（用于快速比较，可选）
    TZrSize lastContentHashLength;    // 哈希长度
    TBool hasIncrementalInfo;         // 是否有增量信息
} SZrFileVersion;

// 增量解析器
typedef struct SZrIncrementalParser {
    SZrState *state;
    SZrHashSet uriToFileMap;          // URI 到文件版本的映射（值为SZrFileVersion*）
    SZrParserState *parserState;      // 解析器状态（共享）
    TBool enableIncrementalParse;     // 是否启用增量解析
    TBool enableContentHash;          // 是否使用内容哈希优化
} SZrIncrementalParser;

// 增量解析器管理函数

// 创建增量解析器
ZR_LANGUAGE_SERVER_API SZrIncrementalParser *ZrIncrementalParserNew(SZrState *state);

// 释放增量解析器
ZR_LANGUAGE_SERVER_API void ZrIncrementalParserFree(SZrState *state, 
                                                     SZrIncrementalParser *parser);

// 更新文件内容
ZR_LANGUAGE_SERVER_API TBool ZrIncrementalParserUpdateFile(SZrState *state,
                                                              SZrIncrementalParser *parser,
                                                              SZrString *uri,
                                                              const TChar *content,
                                                              TZrSize contentLength,
                                                              TZrSize version);

// 解析文件（增量）
ZR_LANGUAGE_SERVER_API TBool ZrIncrementalParserParse(SZrState *state,
                                                        SZrIncrementalParser *parser,
                                                        SZrString *uri);

// 获取 AST
ZR_LANGUAGE_SERVER_API SZrAstNode *ZrIncrementalParserGetAST(SZrIncrementalParser *parser,
                                                               SZrString *uri);

// 移除文件
ZR_LANGUAGE_SERVER_API void ZrIncrementalParserRemoveFile(SZrState *state,
                                                           SZrIncrementalParser *parser,
                                                           SZrString *uri);

// 获取文件版本
ZR_LANGUAGE_SERVER_API SZrFileVersion *ZrIncrementalParserGetFileVersion(SZrIncrementalParser *parser,
                                                                           SZrString *uri);

// 文件版本管理函数

// 创建文件版本
ZR_LANGUAGE_SERVER_API SZrFileVersion *ZrFileVersionNew(SZrState *state,
                                                          SZrString *uri,
                                                          const TChar *content,
                                                          TZrSize contentLength,
                                                          TZrSize version);

// 释放文件版本
ZR_LANGUAGE_SERVER_API void ZrFileVersionFree(SZrState *state, SZrFileVersion *fileVersion);

// 更新文件版本内容
ZR_LANGUAGE_SERVER_API TBool ZrFileVersionUpdateContent(SZrState *state,
                                                         SZrFileVersion *fileVersion,
                                                         const TChar *content,
                                                         TZrSize contentLength,
                                                         TZrSize version,
                                                         SZrFileRange changeRange);

#endif //ZR_VM_LANGUAGE_SERVER_INCREMENTAL_PARSER_H
