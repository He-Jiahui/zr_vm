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
#include "zr_vm_parser/parser.h"

#include <string.h>

// 创建文件版本
SZrFileVersion *ZrFileVersionNew(SZrState *state,
                                  SZrString *uri,
                                  const TChar *content,
                                  TZrSize contentLength,
                                  TZrSize version) {
    if (state == ZR_NULL || uri == ZR_NULL || content == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrFileVersion *fileVersion = (SZrFileVersion *)ZrMemoryRawMalloc(state->global, sizeof(SZrFileVersion));
    if (fileVersion == ZR_NULL) {
        return ZR_NULL;
    }
    
    fileVersion->uri = uri;
    fileVersion->version = version;
    fileVersion->contentLength = contentLength;
    fileVersion->ast = ZR_NULL;
    fileVersion->isDirty = ZR_TRUE;
    fileVersion->lastChangeRange = ZrFileRangeCreate(
        ZrFilePositionCreate(0, 0, 0),
        ZrFilePositionCreate(0, 0, 0),
        uri
    );
    fileVersion->lastContentHash = ZR_NULL;
    fileVersion->lastContentHashLength = 0;
    fileVersion->hasIncrementalInfo = ZR_FALSE;
    
    // 复制内容
    fileVersion->content = (TChar *)ZrMemoryRawMalloc(state->global, contentLength + 1);
    if (fileVersion->content == ZR_NULL) {
        ZrMemoryRawFree(state->global, fileVersion, sizeof(SZrFileVersion));
        return ZR_NULL;
    }
    memcpy(fileVersion->content, content, contentLength);
    fileVersion->content[contentLength] = '\0';
    
    return fileVersion;
}

// 释放文件版本
void ZrFileVersionFree(SZrState *state, SZrFileVersion *fileVersion) {
    if (state == ZR_NULL || fileVersion == ZR_NULL) {
        return;
    }
    
    if (fileVersion->content != ZR_NULL) {
        ZrMemoryRawFree(state->global, fileVersion->content, fileVersion->contentLength + 1);
    }
    
    if (fileVersion->ast != ZR_NULL) {
        ZrParserFreeAst(state, fileVersion->ast);
    }
    
    if (fileVersion->lastContentHash != ZR_NULL) {
        ZrMemoryRawFree(state->global, fileVersion->lastContentHash, fileVersion->lastContentHashLength + 1);
    }
    
    ZrMemoryRawFree(state->global, fileVersion, sizeof(SZrFileVersion));
}

// 更新文件版本内容
TBool ZrFileVersionUpdateContent(SZrState *state,
                                 SZrFileVersion *fileVersion,
                                 const TChar *content,
                                 TZrSize contentLength,
                                 TZrSize version,
                                 SZrFileRange changeRange) {
    if (state == ZR_NULL || fileVersion == ZR_NULL || content == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 释放旧内容
    if (fileVersion->content != ZR_NULL) {
        ZrMemoryRawFree(state->global, fileVersion->content, fileVersion->contentLength + 1);
    }
    
    // 复制新内容
    fileVersion->content = (TChar *)ZrMemoryRawMalloc(state->global, contentLength + 1);
    if (fileVersion->content == ZR_NULL) {
        return ZR_FALSE;
    }
    memcpy(fileVersion->content, content, contentLength);
    fileVersion->content[contentLength] = '\0';
    fileVersion->contentLength = contentLength;
    fileVersion->version = version;
    fileVersion->isDirty = ZR_TRUE;
    fileVersion->lastChangeRange = changeRange;
    fileVersion->hasIncrementalInfo = ZR_TRUE; // 标记有增量信息
    
    // 释放旧 AST
    if (fileVersion->ast != ZR_NULL) {
        ZrParserFreeAst(state, fileVersion->ast);
        fileVersion->ast = ZR_NULL;
    }
    
    // 释放旧哈希
    if (fileVersion->lastContentHash != ZR_NULL) {
        ZrMemoryRawFree(state->global, fileVersion->lastContentHash, fileVersion->lastContentHashLength + 1);
        fileVersion->lastContentHash = ZR_NULL;
        fileVersion->lastContentHashLength = 0;
    }
    
    return ZR_TRUE;
}

// 创建增量解析器
SZrIncrementalParser *ZrIncrementalParserNew(SZrState *state) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrIncrementalParser *parser = (SZrIncrementalParser *)ZrMemoryRawMalloc(state->global, sizeof(SZrIncrementalParser));
    if (parser == ZR_NULL) {
        return ZR_NULL;
    }
    
    parser->state = state;
    parser->uriToFileMap.buckets = ZR_NULL;
    parser->uriToFileMap.bucketSize = 0;
    parser->uriToFileMap.elementCount = 0;
    parser->uriToFileMap.capacity = 0;
    parser->uriToFileMap.resizeThreshold = 0;
    parser->uriToFileMap.isValid = ZR_FALSE;
    ZrHashSetInit(state, &parser->uriToFileMap, 4); // capacityLog2 = 4 (16 buckets)
    parser->parserState = ZR_NULL; // 延迟初始化
    parser->enableIncrementalParse = ZR_TRUE; // 默认启用增量解析
    parser->enableContentHash = ZR_TRUE; // 默认启用内容哈希
    
    return parser;
}

// 释放增量解析器
void ZrIncrementalParserFree(SZrState *state, SZrIncrementalParser *parser) {
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
                            ZrFileVersionFree(state, fileVersion);
                        }
                    }
                }
                // 释放节点本身
                SZrHashKeyValuePair *next = pair->next;
                ZrMemoryRawFreeWithType(state->global, pair, sizeof(SZrHashKeyValuePair), 
                                       ZR_MEMORY_NATIVE_TYPE_HASH_PAIR);
                pair = next;
            }
            parser->uriToFileMap.buckets[i] = ZR_NULL;
        }
        // 释放 buckets 数组
        ZrHashSetDeconstruct(state, &parser->uriToFileMap);
    }
    
    if (parser->parserState != ZR_NULL) {
        ZrParserStateFree(parser->parserState);
    }
    
    ZrMemoryRawFree(state->global, parser, sizeof(SZrIncrementalParser));
}

// 更新文件内容
TBool ZrIncrementalParserUpdateFile(SZrState *state,
                                     SZrIncrementalParser *parser,
                                     SZrString *uri,
                                     const TChar *content,
                                     TZrSize contentLength,
                                     TZrSize version) {
    if (state == ZR_NULL || parser == ZR_NULL || uri == ZR_NULL || content == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 查找是否已存在
    SZrFileVersion *fileVersion = ZrIncrementalParserGetFileVersion(parser, uri);
    if (fileVersion != ZR_NULL) {
        // 更新现有文件
        SZrFileRange changeRange = ZrFileRangeCreate(
            ZrFilePositionCreate(0, 0, 0),
            ZrFilePositionCreate(contentLength, 0, 0),
            uri
        );
        return ZrFileVersionUpdateContent(state, fileVersion, content, contentLength, version, changeRange);
    } else {
        // 创建新文件
        fileVersion = ZrFileVersionNew(state, uri, content, contentLength, version);
        if (fileVersion == ZR_NULL) {
            return ZR_FALSE;
        }
        
        // 添加到哈希表
        SZrTypeValue key;
        ZrValueInitAsRawObject(state, &key, &uri->super);
        
        // 使用 ZrHashSetAdd 添加，然后设置值
        SZrHashKeyValuePair *pair = ZrHashSetAdd(state, &parser->uriToFileMap, &key);
        if (pair != ZR_NULL) {
            // 将 SZrFileVersion 指针存储为原生指针
            SZrTypeValue value;
            ZrValueInitAsNativePointer(state, &value, (TZrPtr)fileVersion);
            ZrValueCopy(state, &pair->value, &value);
        }
    }
    
    return ZR_TRUE;
}

// 辅助函数：计算内容哈希（简化实现）
static void compute_content_hash(SZrState *state, const TChar *content, TZrSize length, 
                                  TChar **hash, TZrSize *hashLength) {
    if (state == ZR_NULL || content == ZR_NULL || hash == ZR_NULL || hashLength == ZR_NULL) {
        return;
    }
    
    // TODO: 简化实现：使用简单的哈希算法
    TUInt64 hashValue = 0;
    for (TZrSize i = 0; i < length; i++) {
        hashValue = hashValue * 31 + (TUInt8)content[i];
    }
    
    // 转换为字符串
    TChar hashStr[32];
    snprintf(hashStr, sizeof(hashStr), "%llx", (unsigned long long)hashValue);
    TZrSize len = strlen(hashStr);
    
    *hash = (TChar *)ZrMemoryRawMalloc(state->global, len + 1);
    if (*hash != ZR_NULL) {
        memcpy(*hash, hashStr, len);
        (*hash)[len] = '\0';
        *hashLength = len;
    }
}

// 辅助函数：比较内容哈希
static TBool compare_content_hash(const TChar *hash1, TZrSize len1, 
                                   const TChar *hash2, TZrSize len2) {
    if (hash1 == ZR_NULL || hash2 == ZR_NULL) {
        return ZR_FALSE;
    }
    if (len1 != len2) {
        return ZR_FALSE;
    }
    return memcmp(hash1, hash2, len1) == 0;
}

// 解析文件（增量）
TBool ZrIncrementalParserParse(SZrState *state,
                                SZrIncrementalParser *parser,
                                SZrString *uri) {
    if (state == ZR_NULL || parser == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }
    
    SZrFileVersion *fileVersion = ZrIncrementalParserGetFileVersion(parser, uri);
    if (fileVersion == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 如果不需要重新解析，检查内容哈希（如果启用）
    if (!fileVersion->isDirty && fileVersion->ast != ZR_NULL) {
        if (parser->enableContentHash && fileVersion->lastContentHash != ZR_NULL) {
            // 计算当前内容哈希
            TChar *currentHash = ZR_NULL;
            TZrSize currentHashLength = 0;
            compute_content_hash(state, fileVersion->content, fileVersion->contentLength,
                                &currentHash, &currentHashLength);
            
            if (currentHash != ZR_NULL) {
                // 比较哈希
                TBool isSame = compare_content_hash(fileVersion->lastContentHash, 
                                                   fileVersion->lastContentHashLength,
                                                   currentHash, currentHashLength);
                ZrMemoryRawFree(state->global, currentHash, strlen(currentHash) + 1);
                
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
    SZrString *sourceName = uri; // 使用 URI 作为源文件名
    fileVersion->ast = ZrParserParse(state, fileVersion->content, fileVersion->contentLength, sourceName);
    
    if (fileVersion->ast != ZR_NULL) {
        fileVersion->isDirty = ZR_FALSE;
        
        // 计算并存储内容哈希
        if (parser->enableContentHash) {
            if (fileVersion->lastContentHash != ZR_NULL) {
                ZrMemoryRawFree(state->global, fileVersion->lastContentHash, fileVersion->lastContentHashLength + 1);
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
SZrAstNode *ZrIncrementalParserGetAST(SZrIncrementalParser *parser,
                                       SZrString *uri) {
    if (parser == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrFileVersion *fileVersion = ZrIncrementalParserGetFileVersion(parser, uri);
    if (fileVersion == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 如果 AST 不存在或需要重新解析，先解析
    if (fileVersion->ast == ZR_NULL || fileVersion->isDirty) {
        if (!ZrIncrementalParserParse(parser->state, parser, uri)) {
            return ZR_NULL;
        }
    }
    
    return fileVersion->ast;
}

// 移除文件
void ZrIncrementalParserRemoveFile(SZrState *state,
                                    SZrIncrementalParser *parser,
                                    SZrString *uri) {
    if (state == ZR_NULL || parser == ZR_NULL || uri == ZR_NULL) {
        return;
    }
    
    SZrFileVersion *fileVersion = ZrIncrementalParserGetFileVersion(parser, uri);
    if (fileVersion != ZR_NULL) {
        // 从哈希表中移除
        SZrTypeValue key;
        ZrValueInitAsRawObject(state, &key, &uri->super);
        ZrHashSetRemove(state, &parser->uriToFileMap, &key);
        
        // 释放文件版本
        ZrFileVersionFree(state, fileVersion);
    }
}

// 获取文件版本
SZrFileVersion *ZrIncrementalParserGetFileVersion(SZrIncrementalParser *parser,
                                                  SZrString *uri) {
    if (parser == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 从哈希表中查找
    SZrTypeValue key;
    ZrValueInitAsRawObject(parser->state, &key, &uri->super);
    
    SZrHashKeyValuePair *pair = ZrHashSetFind(parser->state, &parser->uriToFileMap, &key);
    if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
        // 从原生指针中获取 SZrFileVersion
        return (SZrFileVersion *)pair->value.value.nativeObject.nativePointer;
    }
    
    return ZR_NULL;
}
