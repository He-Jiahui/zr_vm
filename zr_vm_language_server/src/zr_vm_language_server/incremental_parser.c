//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_language_server/incremental_parser.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server/semantic_analyzer.h"
#include "lsp_interface_internal.h"
#include "zr_vm_parser/parser.h"

#include <string.h>
#include <stdio.h>

typedef struct SZrParserDiagnosticCollector {
    SZrState *state;
    SZrFileVersion *fileVersion;
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

    diagnostic = ZrLanguageServer_Diagnostic_New(collector->state,
                                                 ZR_DIAGNOSTIC_ERROR,
                                                 *location,
                                                 message,
                                                 "parser_syntax_error");
    if (diagnostic != ZR_NULL) {
        ZrCore_Array_Push(collector->state, &collector->fileVersion->parserDiagnostics, &diagnostic);
    }
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
    fileVersion->contentLength = contentLength;
    fileVersion->ast = ZR_NULL;
    fileVersion->usesFallbackAst = ZR_FALSE;
    fileVersion->isDirty = ZR_TRUE;
    fileVersion->lastChangeRange = ZrParser_FileRange_Create(
        ZrParser_FilePosition_Create(0, 0, 0),
        ZrParser_FilePosition_Create(0, 0, 0),
        uri
    );
    fileVersion->lastContentHash = ZR_NULL;
    fileVersion->lastContentHashLength = 0;
    fileVersion->hasIncrementalInfo = ZR_FALSE;
    ZrCore_Array_Init(state,
                      &fileVersion->parserDiagnostics,
                      sizeof(SZrDiagnostic *),
                      ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    
    // 复制内容
    fileVersion->content = (TZrChar *)ZrCore_Memory_RawMalloc(state->global, contentLength + 1);
    if (fileVersion->content == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, fileVersion, sizeof(SZrFileVersion));
        return ZR_NULL;
    }
    memcpy(fileVersion->content, content, contentLength);
    fileVersion->content[contentLength] = '\0';
    
    return fileVersion;
}

// 释放文件版本
void ZrLanguageServer_FileVersion_Free(SZrState *state, SZrFileVersion *fileVersion) {
    if (state == ZR_NULL || fileVersion == ZR_NULL) {
        return;
    }

    if (fileVersion->content != ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, fileVersion->content, fileVersion->contentLength + 1);
    }

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

// 更新文件版本内容
TZrBool ZrLanguageServer_FileVersion_UpdateContent(SZrState *state,
                                 SZrFileVersion *fileVersion,
                                 const TZrChar *content,
                                 TZrSize contentLength,
                                 TZrSize version,
                                 SZrFileRange changeRange) {
    if (state == ZR_NULL || fileVersion == ZR_NULL || content == ZR_NULL) {
        return ZR_FALSE;
    }

    /*
     * 调用方可能传入 fileVersion->content（例如 LSP 从 fileVersion 取文本再交给 UpdateFile）。
     * 若先释放 fileVersion->content 再 memcpy(content)，则 memcpy 读取的已是被释放内存。
     */
    if (fileVersion->content == ZR_NULL || fileVersion->content != (TZrChar *)content) {
        if (fileVersion->content != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, fileVersion->content, fileVersion->contentLength + 1);
        }

        fileVersion->content = (TZrChar *)ZrCore_Memory_RawMalloc(state->global, contentLength + 1);
        if (fileVersion->content == ZR_NULL) {
            return ZR_FALSE;
        }
        memcpy(fileVersion->content, content, contentLength);
        fileVersion->content[contentLength] = '\0';
    }

    fileVersion->contentLength = contentLength;
    fileVersion->version = version;
    fileVersion->isDirty = ZR_TRUE;
    fileVersion->lastChangeRange = changeRange;
    fileVersion->hasIncrementalInfo = ZR_TRUE; /* 标记有增量信息 */

    clear_parser_diagnostics(state, fileVersion);

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
                // 释放节点本身
                SZrHashKeyValuePair *next = pair->next;
                ZrCore_Memory_RawFreeWithType(state->global, pair, sizeof(SZrHashKeyValuePair), 
                                       ZR_MEMORY_NATIVE_TYPE_HASH_PAIR);
                pair = next;
            }
            parser->uriToFileMap.buckets[i] = ZR_NULL;
        }
        // 释放 buckets 数组
        ZrCore_HashSet_Deconstruct(state, &parser->uriToFileMap);
    }

    if (parser->parserState != ZR_NULL) {
        ZrParser_State_Free(parser->parserState);
    }

    ZrCore_Memory_RawFree(state->global, parser, sizeof(SZrIncrementalParser));
}

// 更新文件内容
TZrBool ZrLanguageServer_IncrementalParser_UpdateFile(SZrState *state,
                                      SZrIncrementalParser *parser,
                                      SZrString *uri,
                                      const TZrChar *content,
                                      TZrSize contentLength,
                                      TZrSize version) {
    if (state == ZR_NULL || parser == ZR_NULL || uri == ZR_NULL || content == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 查找是否已存在
    SZrFileVersion *fileVersion = ZrLanguageServer_IncrementalParser_GetFileVersion(parser, uri);
    if (fileVersion != ZR_NULL) {
        // 更新现有文件
        SZrFileRange changeRange = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(0, 0, 0),
            ZrParser_FilePosition_Create(contentLength, 0, 0),
            uri
        );
        return ZrLanguageServer_FileVersion_UpdateContent(state, fileVersion, content, contentLength, version, changeRange);
    } else {
        // 创建新文件
        fileVersion = ZrLanguageServer_FileVersion_New(state, uri, content, contentLength, version);
        if (fileVersion == ZR_NULL) {
            return ZR_FALSE;
        }
        
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
    if (fileVersion == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 如果不需要重新解析，检查内容哈希（如果启用）
    if (!fileVersion->isDirty && fileVersion->ast != ZR_NULL) {
        if (parser->enableContentHash && fileVersion->lastContentHash != ZR_NULL) {
            // 计算当前内容哈希
            TZrChar *currentHash = ZR_NULL;
            TZrSize currentHashLength = 0;
            compute_content_hash(state, fileVersion->content, fileVersion->contentLength,
                                &currentHash, &currentHashLength);
            
            if (currentHash != ZR_NULL) {
                // 比较哈希
                TZrBool isSame = compare_content_hash(fileVersion->lastContentHash, 
                                                   fileVersion->lastContentHashLength,
                                                   currentHash, currentHashLength);
                ZrCore_Memory_RawFree(state->global, currentHash, strlen(currentHash) + 1);
                
                if (isSame) {
                    // 内容未改变，不需要重新解析
                    return ZR_TRUE;
                }
            }
        } else {
            // 未启用内容哈希，直接返回
            return ZR_TRUE;
        }
    }
    
    // 启用增量解析时，尝试增量更新 AST
    if (parser->enableIncrementalParse && fileVersion->ast != ZR_NULL && 
        fileVersion->hasIncrementalInfo) {
        // TODO: 实现增量 AST 更新算法（简化版本）
        // 策略：如果变更范围较小且不影响语法结构，可以尝试增量更新
        // 否则回退到完全重新解析
        
        SZrFileRange changeRange = fileVersion->lastChangeRange;
        TZrSize changeSize = 0;
        if (changeRange.end.offset > changeRange.start.offset) {
            changeSize = changeRange.end.offset - changeRange.start.offset;
        }
        
        // 如果变更范围太大（超过文件长度的10%），完全重新解析
        TZrSize threshold = fileVersion->contentLength / 10;
        if (changeSize > threshold || changeSize == 0) {
            // 变更太大，完全重新解析
        } else {
            // 尝试增量更新：检查变更是否在注释或字符串字面量中
            // 如果是，可能不需要重新解析
            // TODO: 简化实现：暂时总是完全重新解析
            // 完整实现需要：
            // 1. 分析变更范围是否影响语法结构
            // 2. 如果只影响注释或字符串内容，可以保留AST结构
            // 3. 如果影响语法，需要重新解析受影响的部分
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
        ZrParser_State_Init(&parserState, state, fileVersion->content, fileVersion->contentLength, sourceName);
        if (parserState.hasError) {
            ZrParser_State_Free(&parserState);
            return ZR_FALSE;
        }

        collector.state = state;
        collector.fileVersion = fileVersion;
        parserState.errorCallback = collect_parser_diagnostic;
        parserState.errorUserData = &collector;
        parserState.suppressErrorOutput = ZR_TRUE;
        parsedAst = ZrParser_ParseWithState(&parserState);
        ZrParser_State_Free(&parserState);

        if (parsedAst != ZR_NULL) {
            if (fileVersion->parserDiagnostics.length == 0 || previousAst == ZR_NULL) {
                if (previousAst != ZR_NULL && previousAst != parsedAst) {
                    ZrParser_Ast_Free(state, previousAst);
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
                ZrParser_Ast_Free(state, previousAst);
            }
            fileVersion->ast = ZR_NULL;
            fileVersion->usesFallbackAst = ZR_FALSE;
        }
    }
    
    if (fileVersion->ast != ZR_NULL || fileVersion->parserDiagnostics.length > 0) {
        fileVersion->isDirty = ZR_FALSE;
        
        // 计算并存储内容哈希
        if (parser->enableContentHash) {
            if (fileVersion->lastContentHash != ZR_NULL) {
                ZrCore_Memory_RawFree(state->global, fileVersion->lastContentHash, fileVersion->lastContentHashLength + 1);
            }
            compute_content_hash(state, fileVersion->content, fileVersion->contentLength,
                                &fileVersion->lastContentHash, 
                                &fileVersion->lastContentHashLength);
        }
        
        return ZR_TRUE;
    }
    
    return ZR_FALSE;
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
