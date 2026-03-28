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
    TZrChar *content;                    // 文件内容
    TZrSize contentLength;            // 内容长度
    SZrAstNode *ast;                  // 解析后的 AST
    TZrBool isDirty;                    // 是否需要重新解析
    SZrFileRange lastChangeRange;     // 最后变更的范围（用于增量解析）
    TZrChar *lastContentHash;           // 内容哈希（用于快速比较，可选）
    TZrSize lastContentHashLength;    // 哈希长度
    TZrBool hasIncrementalInfo;         // 是否有增量信息
} SZrFileVersion;

// 增量解析器
typedef struct SZrIncrementalParser {
    SZrState *state;
    SZrHashSet uriToFileMap;          // URI 到文件版本的映射（值为SZrFileVersion*）
    SZrParserState *parserState;      // 解析器状态（共享）
    TZrBool enableIncrementalParse;     // 是否启用增量解析
    TZrBool enableContentHash;          // 是否使用内容哈希优化
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

// 解析文件（增量）
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_IncrementalParser_Parse(SZrState *state,
                                                        SZrIncrementalParser *parser,
                                                        SZrString *uri);

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
                                                         SZrFileRange changeRange);

#endif //ZR_VM_LANGUAGE_SERVER_INCREMENTAL_PARSER_H
