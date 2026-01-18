//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_language_server/lsp_interface.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/string.h"

#include <string.h>

// 转换 FileRange 到 LspRange
SZrLspRange ZrLspRangeFromFileRange(SZrFileRange fileRange) {
    SZrLspRange lspRange;
    lspRange.start.line = fileRange.start.line;
    lspRange.start.character = fileRange.start.column;
    lspRange.end.line = fileRange.end.line;
    lspRange.end.character = fileRange.end.column;
    return lspRange;
}

// 辅助函数：从行号和列号计算偏移量
static TZrSize calculate_offset_from_line_column(const TChar *content, TZrSize contentLength, 
                                                   TInt32 line, TInt32 column) {
    if (content == ZR_NULL) {
        return 0;
    }
    
    TZrSize offset = 0;
    TInt32 currentLine = 0;
    TInt32 currentColumn = 0;
    
    // 遍历内容直到到达目标行和列
    for (TZrSize i = 0; i < contentLength && currentLine < line; i++) {
        if (content[i] == '\n') {
            currentLine++;
            currentColumn = 0;
            offset = i + 1; // 跳过换行符
        } else {
            currentColumn++;
        }
    }
    
    // 如果已经到达目标行，添加列偏移
    if (currentLine == line) {
        // 在当前行中查找列位置
        TZrSize lineStart = offset;
        for (TZrSize i = lineStart; i < contentLength && currentColumn < column; i++) {
            if (content[i] == '\n') {
                break; // 到达行尾
            }
            currentColumn++;
            offset = i + 1;
        }
    }
    
    return offset;
}

// 转换 LspRange 到 FileRange
SZrFileRange ZrLspRangeToFileRange(SZrLspRange lspRange, SZrString *uri) {
    SZrFileRange fileRange;
    fileRange.start.line = lspRange.start.line;
    fileRange.start.column = lspRange.start.character;
    fileRange.start.offset = 0; // 需要文件内容才能计算，暂时设为0
    fileRange.end.line = lspRange.end.line;
    fileRange.end.column = lspRange.end.character;
    fileRange.end.offset = 0; // TODO: 需要文件内容才能计算，暂时设为0
    fileRange.source = uri;
    return fileRange;
}

// 转换 LspRange 到 FileRange（带文件内容）
SZrFileRange ZrLspRangeToFileRangeWithContent(SZrLspRange lspRange, SZrString *uri, 
                                                const TChar *content, TZrSize contentLength) {
    SZrFileRange fileRange;
    fileRange.start.line = lspRange.start.line;
    fileRange.start.column = lspRange.start.character;
    fileRange.start.offset = calculate_offset_from_line_column(content, contentLength, 
                                                                 lspRange.start.line, 
                                                                 lspRange.start.character);
    fileRange.end.line = lspRange.end.line;
    fileRange.end.column = lspRange.end.character;
    fileRange.end.offset = calculate_offset_from_line_column(content, contentLength, 
                                                              lspRange.end.line, 
                                                              lspRange.end.character);
    fileRange.source = uri;
    return fileRange;
}

// 转换 FilePosition 到 LspPosition
SZrLspPosition ZrLspPositionFromFilePosition(SZrFilePosition filePosition) {
    SZrLspPosition lspPosition;
    lspPosition.line = filePosition.line;
    lspPosition.character = filePosition.column;
    return lspPosition;
}

// 转换 LspPosition 到 FilePosition
SZrFilePosition ZrLspPositionToFilePosition(SZrLspPosition lspPosition) {
    SZrFilePosition filePosition;
    filePosition.line = lspPosition.line;
    filePosition.column = lspPosition.character;
    filePosition.offset = 0; // TODO: 需要文件内容才能计算，暂时设为0
    return filePosition;
}

// 转换 LspPosition 到 FilePosition（带文件内容）
SZrFilePosition ZrLspPositionToFilePositionWithContent(SZrLspPosition lspPosition,
                                                        const TChar *content, TZrSize contentLength) {
    SZrFilePosition filePosition;
    filePosition.line = lspPosition.line;
    filePosition.column = lspPosition.character;
    filePosition.offset = calculate_offset_from_line_column(content, contentLength, 
                                                              lspPosition.line, 
                                                              lspPosition.character);
    return filePosition;
}

// 创建 LSP 上下文
SZrLspContext *ZrLspContextNew(SZrState *state) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrLspContext *context = (SZrLspContext *)ZrMemoryRawMalloc(state->global, sizeof(SZrLspContext));
    if (context == ZR_NULL) {
        return ZR_NULL;
    }
    
    context->state = state;
    context->parser = ZrIncrementalParserNew(state);
    context->analyzer = ZR_NULL; // 延迟创建，每个文件一个分析器
    context->uriToAnalyzerMap.buckets = ZR_NULL;
    context->uriToAnalyzerMap.bucketSize = 0;
    context->uriToAnalyzerMap.elementCount = 0;
    context->uriToAnalyzerMap.capacity = 0;
    context->uriToAnalyzerMap.resizeThreshold = 0;
    context->uriToAnalyzerMap.isValid = ZR_FALSE;
    ZrHashSetInit(state, &context->uriToAnalyzerMap, 4); // capacityLog2 = 4
    
    if (context->parser == ZR_NULL) {
        ZrMemoryRawFree(state->global, context, sizeof(SZrLspContext));
        return ZR_NULL;
    }
    
    return context;
}

// 释放 LSP 上下文
void ZrLspContextFree(SZrState *state, SZrLspContext *context) {
    if (state == ZR_NULL || context == ZR_NULL) {
        return;
    }
    
    // 释放所有分析器
    if (context->uriToAnalyzerMap.isValid && context->uriToAnalyzerMap.buckets != ZR_NULL) {
        // 遍历哈希表释放所有分析器和节点
        for (TZrSize i = 0; i < context->uriToAnalyzerMap.capacity; i++) {
            SZrHashKeyValuePair *pair = context->uriToAnalyzerMap.buckets[i];
            while (pair != ZR_NULL) {
                // 释放节点中存储的数据
                if (pair->key.type != ZR_VALUE_TYPE_NULL) {
                    if (pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
                        SZrSemanticAnalyzer *analyzer = 
                            (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
                        if (analyzer != ZR_NULL) {
                            ZrSemanticAnalyzerFree(state, analyzer);
                        }
                    }
                }
                // 释放节点本身
                SZrHashKeyValuePair *next = pair->next;
                ZrMemoryRawFreeWithType(state->global, pair, sizeof(SZrHashKeyValuePair), 
                                       ZR_MEMORY_NATIVE_TYPE_HASH_PAIR);
                pair = next;
            }
            context->uriToAnalyzerMap.buckets[i] = ZR_NULL;
        }
        // 释放 buckets 数组
        ZrHashSetDeconstruct(state, &context->uriToAnalyzerMap);
    }
    
    if (context->parser != ZR_NULL) {
        ZrIncrementalParserFree(state, context->parser);
    }
    
    if (context->analyzer != ZR_NULL) {
        ZrSemanticAnalyzerFree(state, context->analyzer);
    }
    
    ZrMemoryRawFree(state->global, context, sizeof(SZrLspContext));
}

// 获取或创建分析器
static SZrSemanticAnalyzer *get_or_create_analyzer(SZrState *state, SZrLspContext *context, SZrString *uri) {
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 从哈希表中查找
    SZrTypeValue key;
    ZrValueInitAsRawObject(state, &key, &uri->super);
    
    SZrHashKeyValuePair *pair = ZrHashSetFind(state, &context->uriToAnalyzerMap, &key);
    if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
        // 从原生指针中获取 SZrSemanticAnalyzer
        return (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
    }
    
    // 创建新分析器
    SZrSemanticAnalyzer *analyzer = ZrSemanticAnalyzerNew(state);
    if (analyzer != ZR_NULL) {
        // 添加到哈希表
        SZrHashKeyValuePair *newPair = ZrHashSetAdd(state, &context->uriToAnalyzerMap, &key);
        if (newPair != ZR_NULL) {
            // 将 SZrSemanticAnalyzer 指针存储为原生指针
            SZrTypeValue value;
            ZrValueInitAsNativePointer(state, &value, (TZrPtr)analyzer);
            ZrValueCopy(state, &newPair->value, &value);
        }
    }
    
    return analyzer;
}

// 更新文档
TBool ZrLspUpdateDocument(SZrState *state,
                          SZrLspContext *context,
                          SZrString *uri,
                          const TChar *content,
                          TZrSize contentLength,
                          TZrSize version) {
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || content == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 更新文件
    if (!ZrIncrementalParserUpdateFile(state, context->parser, uri, content, contentLength, version)) {
        return ZR_FALSE;
    }
    
    // 重新解析
    if (!ZrIncrementalParserParse(state, context->parser, uri)) {
        return ZR_FALSE;
    }
    
    // 获取或创建分析器
    SZrSemanticAnalyzer *analyzer = get_or_create_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 获取 AST 并分析
    SZrAstNode *ast = ZrIncrementalParserGetAST(context->parser, uri);
    if (ast == ZR_NULL) {
        return ZR_FALSE;
    }
    
    return ZrSemanticAnalyzerAnalyze(state, analyzer, ast);
}

// 获取诊断
TBool ZrLspGetDiagnostics(SZrState *state,
                          SZrLspContext *context,
                          SZrString *uri,
                          SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrArrayInit(state, result, sizeof(SZrLspDiagnostic *), 8);
    }
    
    // 获取分析器
    SZrSemanticAnalyzer *analyzer = get_or_create_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 获取诊断
    SZrArray diagnostics;
    ZrArrayInit(state, &diagnostics, sizeof(SZrDiagnostic *), 8);
    if (!ZrSemanticAnalyzerGetDiagnostics(state, analyzer, &diagnostics)) {
        ZrArrayFree(state, &diagnostics);
        return ZR_FALSE;
    }
    
    // 转换为 LSP 诊断
    for (TZrSize i = 0; i < diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrArrayGet(&diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            SZrDiagnostic *diag = *diagPtr;
            
                SZrLspDiagnostic *lspDiag = (SZrLspDiagnostic *)ZrMemoryRawMalloc(state->global, sizeof(SZrLspDiagnostic));
            if (lspDiag != ZR_NULL) {
                lspDiag->range = ZrLspRangeFromFileRange(diag->location);
                lspDiag->severity = (TInt32)diag->severity + 1; // LSP: 1=Error, 2=Warning, etc.
                lspDiag->code = diag->code;
                lspDiag->message = diag->message;
                ZrArrayInit(state, &lspDiag->relatedInformation, sizeof(SZrLspLocation), 0);
                
                ZrArrayPush(state, result, &lspDiag);
            }
        }
    }
    
    ZrArrayFree(state, &diagnostics);
    return ZR_TRUE;
}

// 获取补全
TBool ZrLspGetCompletion(SZrState *state,
                         SZrLspContext *context,
                         SZrString *uri,
                         SZrLspPosition position,
                         SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrArrayInit(state, result, sizeof(SZrLspCompletionItem *), 8);
    }
    
    // 获取分析器
    SZrSemanticAnalyzer *analyzer = get_or_create_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 转换位置
    SZrFilePosition filePos = ZrLspPositionToFilePosition(position);
    SZrFileRange fileRange = ZrFileRangeCreate(filePos, filePos, uri);
    
    // 获取补全
    SZrArray completions;
    ZrArrayInit(state, &completions, sizeof(SZrCompletionItem *), 8);
    if (!ZrSemanticAnalyzerGetCompletions(state, analyzer, fileRange, &completions)) {
        ZrArrayFree(state, &completions);
        return ZR_FALSE;
    }
    
    // 转换为 LSP 补全项
    for (TZrSize i = 0; i < completions.length; i++) {
        SZrCompletionItem **itemPtr = (SZrCompletionItem **)ZrArrayGet(&completions, i);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL) {
            SZrCompletionItem *item = *itemPtr;
            
            SZrLspCompletionItem *lspItem = (SZrLspCompletionItem *)ZrMemoryRawMalloc(state->global, sizeof(SZrLspCompletionItem));
            if (lspItem != ZR_NULL) {
                lspItem->label = item->label;
                
                // 转换 kind 字符串到整数
                TInt32 kindValue = 1; // 默认 Text
                if (item->kind != ZR_NULL) {
                    TNativeString kindStr;
                    TZrSize kindLen;
                    if (item->kind->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
                        kindStr = ZrStringGetNativeStringShort(item->kind);
                        kindLen = item->kind->shortStringLength;
                    } else {
                        kindStr = ZrStringGetNativeString(item->kind);
                        kindLen = item->kind->longStringLength;
                    }
                    
                    // LSP CompletionItemKind 映射
                    if (kindLen == 8 && memcmp(kindStr, "function", 8) == 0) {
                        kindValue = 3; // Function
                    } else if (kindLen == 5 && memcmp(kindStr, "class", 5) == 0) {
                        kindValue = 7; // Class
                    } else if (kindLen == 8 && memcmp(kindStr, "variable", 8) == 0) {
                        kindValue = 6; // Variable
                    } else if (kindLen == 6 && memcmp(kindStr, "struct", 6) == 0) {
                        kindValue = 22; // Struct
                    } else if (kindLen == 6 && memcmp(kindStr, "method", 6) == 0) {
                        kindValue = 2; // Method
                    } else if (kindLen == 9 && memcmp(kindStr, "interface", 9) == 0) {
                        kindValue = 8; // Interface
                    } else if (kindLen == 8 && memcmp(kindStr, "property", 8) == 0) {
                        kindValue = 10; // Property
                    } else if (kindLen == 5 && memcmp(kindStr, "field", 5) == 0) {
                        kindValue = 5; // Field
                    } else if (kindLen == 6 && memcmp(kindStr, "module", 6) == 0) {
                        kindValue = 9; // Module
                    } else if (kindLen == 4 && memcmp(kindStr, "enum", 4) == 0) {
                        kindValue = 13; // Enum
                    } else if (kindLen == 8 && memcmp(kindStr, "constant", 8) == 0) {
                        kindValue = 21; // Constant
                    } else {
                        kindValue = 1; // Text (默认)
                    }
                }
                lspItem->kind = kindValue;
                lspItem->detail = item->detail;
                lspItem->documentation = item->documentation;
                lspItem->insertText = item->label;
                lspItem->insertTextFormat = ZrStringCreate(state, "plaintext", 9);
                
                ZrArrayPush(state, result, &lspItem);
            }
        }
    }
    
    ZrArrayFree(state, &completions);
    return ZR_TRUE;
}

// 获取悬停信息
TBool ZrLspGetHover(SZrState *state,
                    SZrLspContext *context,
                    SZrString *uri,
                    SZrLspPosition position,
                    SZrLspHover **result) {
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 获取分析器
    SZrSemanticAnalyzer *analyzer = get_or_create_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 转换位置
    SZrFilePosition filePos = ZrLspPositionToFilePosition(position);
    SZrFileRange fileRange = ZrFileRangeCreate(filePos, filePos, uri);
    
    // 获取悬停信息
    SZrHoverInfo *hoverInfo = ZR_NULL;
    if (!ZrSemanticAnalyzerGetHoverInfo(state, analyzer, fileRange, &hoverInfo)) {
        return ZR_FALSE;
    }
    
    if (hoverInfo == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 转换为 LSP 悬停
    SZrLspHover *lspHover = (SZrLspHover *)ZrMemoryRawMalloc(state->global, sizeof(SZrLspHover));
    if (lspHover == ZR_NULL) {
        ZrHoverInfoFree(state, hoverInfo);
        return ZR_FALSE;
    }
    
    ZrArrayInit(state, &lspHover->contents, sizeof(SZrString *), 1);
    SZrString *content = hoverInfo->contents;
    ZrArrayPush(state, &lspHover->contents, &content);
    lspHover->range = ZrLspRangeFromFileRange(hoverInfo->range);
    
    *result = lspHover;
    return ZR_TRUE;
}

// 获取定义位置
TBool ZrLspGetDefinition(SZrState *state,
                         SZrLspContext *context,
                         SZrString *uri,
                         SZrLspPosition position,
                         SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrArrayInit(state, result, sizeof(SZrLspLocation *), 1);
    }
    
    // 获取分析器
    SZrSemanticAnalyzer *analyzer = get_or_create_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 转换位置
    SZrFilePosition filePos = ZrLspPositionToFilePosition(position);
    SZrFileRange fileRange = ZrFileRangeCreate(filePos, filePos, uri);
    
    // 查找符号
    SZrSymbol *symbol = ZrSemanticAnalyzerGetSymbolAt(analyzer, fileRange);
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 创建位置
    SZrLspLocation *location = (SZrLspLocation *)ZrMemoryRawMalloc(state->global, sizeof(SZrLspLocation));
    if (location == ZR_NULL) {
        return ZR_FALSE;
    }
    
    location->uri = symbol->location.source;
    location->range = ZrLspRangeFromFileRange(symbol->location);
    
    ZrArrayPush(state, result, &location);
    
    return ZR_TRUE;
}

// 查找引用
TBool ZrLspFindReferences(SZrState *state,
                          SZrLspContext *context,
                          SZrString *uri,
                          SZrLspPosition position,
                          TBool includeDeclaration,
                          SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrArrayInit(state, result, sizeof(SZrLspLocation *), 8);
    }
    
    // 获取分析器
    SZrSemanticAnalyzer *analyzer = get_or_create_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 转换位置
    SZrFilePosition filePos = ZrLspPositionToFilePosition(position);
    SZrFileRange fileRange = ZrFileRangeCreate(filePos, filePos, uri);
    
    // 查找符号
    SZrSymbol *symbol = ZrSemanticAnalyzerGetSymbolAt(analyzer, fileRange);
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 获取所有引用
    SZrArray references;
    ZrArrayInit(state, &references, sizeof(SZrReference *), 8);
    if (!ZrReferenceTrackerFindReferences(state, analyzer->referenceTracker, symbol, &references)) {
        ZrArrayFree(state, &references);
        return ZR_FALSE;
    }
    
    // 转换为 LSP 位置
    for (TZrSize i = 0; i < references.length; i++) {
        SZrReference **refPtr = (SZrReference **)ZrArrayGet(&references, i);
        if (refPtr != ZR_NULL && *refPtr != ZR_NULL) {
            SZrReference *ref = *refPtr;
            
            // 如果是定义引用且不包含定义，跳过
            if (ref->type == ZR_REFERENCE_DEFINITION && !includeDeclaration) {
                continue;
            }
            
            SZrLspLocation *location = (SZrLspLocation *)ZrMemoryRawMalloc(state->global, sizeof(SZrLspLocation));
            if (location != ZR_NULL) {
                location->uri = ref->location.source;
                location->range = ZrLspRangeFromFileRange(ref->location);
                
                ZrArrayPush(state, result, &location);
            }
        }
    }
    
    ZrArrayFree(state, &references);
    return ZR_TRUE;
}

// 重命名符号
TBool ZrLspRename(SZrState *state,
                  SZrLspContext *context,
                  SZrString *uri,
                  SZrLspPosition position,
                  SZrString *newName,
                  SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || newName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrArrayInit(state, result, sizeof(SZrLspLocation *), 8);
    }
    
    // 获取分析器
    SZrSemanticAnalyzer *analyzer = get_or_create_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 转换位置
    SZrFilePosition filePos = ZrLspPositionToFilePosition(position);
    SZrFileRange fileRange = ZrFileRangeCreate(filePos, filePos, uri);
    
    // 查找符号
    SZrSymbol *symbol = ZrSemanticAnalyzerGetSymbolAt(analyzer, fileRange);
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 获取所有引用（包括定义）
    SZrArray references;
    ZrArrayInit(state, &references, sizeof(SZrReference *), 8);
    if (!ZrReferenceTrackerFindReferences(state, analyzer->referenceTracker, symbol, &references)) {
        ZrArrayFree(state, &references);
        return ZR_FALSE;
    }
    
    // 转换为 LSP 位置（所有需要重命名的位置）
    for (TZrSize i = 0; i < references.length; i++) {
        SZrReference **refPtr = (SZrReference **)ZrArrayGet(&references, i);
        if (refPtr != ZR_NULL && *refPtr != ZR_NULL) {
            SZrReference *ref = *refPtr;
            
            SZrLspLocation *location = (SZrLspLocation *)ZrMemoryRawMalloc(state->global, sizeof(SZrLspLocation));
            if (location != ZR_NULL) {
                location->uri = ref->location.source;
                location->range = ZrLspRangeFromFileRange(ref->location);
                
                ZrArrayPush(state, result, &location);
            }
        }
    }
    
    ZrArrayFree(state, &references);
    return ZR_TRUE;
}
