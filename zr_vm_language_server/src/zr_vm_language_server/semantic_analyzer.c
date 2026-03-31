//
// Created by Auto on 2025/01/XX.
//

#include "semantic_analyzer_internal.h"

SZrSemanticAnalyzer *ZrLanguageServer_SemanticAnalyzer_New(SZrState *state) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrSemanticAnalyzer *analyzer = (SZrSemanticAnalyzer *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrSemanticAnalyzer));
    if (analyzer == ZR_NULL) {
        return ZR_NULL;
    }
    
    analyzer->state = state;
    analyzer->symbolTable = ZrLanguageServer_SymbolTable_New(state);
    analyzer->referenceTracker = ZR_NULL;
    analyzer->ast = ZR_NULL;
    analyzer->cache = ZR_NULL;
    analyzer->enableCache = ZR_TRUE; // 默认启用缓存
    analyzer->compilerState = ZR_NULL; // 延迟创建
    analyzer->semanticContext = ZR_NULL;
    analyzer->hirModule = ZR_NULL;
    
    if (analyzer->symbolTable == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, analyzer, sizeof(SZrSemanticAnalyzer));
        return ZR_NULL;
    }
    
    analyzer->referenceTracker = ZrLanguageServer_ReferenceTracker_New(state, analyzer->symbolTable);
    if (analyzer->referenceTracker == ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, analyzer->symbolTable);
        ZrCore_Memory_RawFree(state->global, analyzer, sizeof(SZrSemanticAnalyzer));
        return ZR_NULL;
    }
    
    ZrCore_Array_Init(state, &analyzer->diagnostics, sizeof(SZrDiagnostic *), 8);
    
    // 创建缓存
    analyzer->cache = (SZrAnalysisCache *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrAnalysisCache));
    if (analyzer->cache != ZR_NULL) {
        analyzer->cache->isValid = ZR_FALSE;
        analyzer->cache->astHash = 0;
        ZrCore_Array_Init(state, &analyzer->cache->cachedDiagnostics, sizeof(SZrDiagnostic *), 8);
        ZrCore_Array_Init(state, &analyzer->cache->cachedSymbols, sizeof(SZrSymbol *), 8);
    }
    
    return analyzer;
}

// 释放语义分析器
void ZrLanguageServer_SemanticAnalyzer_Free(SZrState *state, SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || analyzer == ZR_NULL) {
        return;
    }

    // 释放所有诊断
    for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            ZrLanguageServer_Diagnostic_Free(state, *diagPtr);
        }
    }
    
    ZrCore_Array_Free(state, &analyzer->diagnostics);
    
    // 释放缓存
    if (analyzer->cache != ZR_NULL) {
        // 释放缓存的诊断信息
        for (TZrSize i = 0; i < analyzer->cache->cachedDiagnostics.length; i++) {
            SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->cache->cachedDiagnostics, i);
            if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                // 注意：诊断可能在 analyzer->diagnostics 中，避免重复释放
            }
        }
        ZrCore_Array_Free(state, &analyzer->cache->cachedDiagnostics);
        ZrCore_Array_Free(state, &analyzer->cache->cachedSymbols);
        ZrCore_Memory_RawFree(state->global, analyzer->cache, sizeof(SZrAnalysisCache));
    }

    if (analyzer->referenceTracker != ZR_NULL) {
        ZrLanguageServer_ReferenceTracker_Free(state, analyzer->referenceTracker);
    }

    if (analyzer->symbolTable != ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, analyzer->symbolTable);
    }

    // 释放编译器状态
    if (analyzer->compilerState != ZR_NULL) {
        ZrParser_CompilerState_Free(analyzer->compilerState);
        ZrCore_Memory_RawFree(state->global, analyzer->compilerState, sizeof(SZrCompilerState));
    }

    analyzer->semanticContext = ZR_NULL;
    analyzer->hirModule = ZR_NULL;

    ZrCore_Memory_RawFree(state->global, analyzer, sizeof(SZrSemanticAnalyzer));
}

// 辅助函数：从 AST 节点提取标识符名称

TZrBool ZrLanguageServer_SemanticAnalyzer_Analyze(SZrState *state, 
                                 SZrSemanticAnalyzer *analyzer,
                                 SZrAstNode *ast) {
    if (state == ZR_NULL || analyzer == ZR_NULL || ast == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 检查缓存
    TZrSize astHash = 0;
    if (analyzer->enableCache && analyzer->cache != ZR_NULL) {
        astHash = ZrLanguageServer_SemanticAnalyzer_ComputeAstHash(ast);
        if (analyzer->cache->isValid && analyzer->cache->astHash == astHash) {
            // 缓存有效，使用缓存结果
            // 复制缓存的诊断信息
            for (TZrSize i = 0; i < analyzer->cache->cachedDiagnostics.length; i++) {
                SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->cache->cachedDiagnostics, i);
                if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                    ZrCore_Array_Push(state, &analyzer->diagnostics, diagPtr);
                }
            }
            return ZR_TRUE;
        }
    }
    
    analyzer->ast = ast;
    
    // 清除旧的诊断信息
    for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            ZrLanguageServer_Diagnostic_Free(state, *diagPtr);
        }
    }
    analyzer->diagnostics.length = 0;
    
    if (!ZrLanguageServer_SemanticAnalyzer_PrepareState(state, analyzer, ast)) {
        return ZR_FALSE;
    }
    
    // 第一阶段：收集符号定义
    ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, analyzer, ast);
    
    // 第二阶段：收集引用
    ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, ast);
    
    // 第三阶段：类型检查（集成类型推断系统）
    // 遍历 AST 进行类型检查
    if (analyzer->compilerState != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, ast);
    }
    
    // 更新缓存
    if (analyzer->enableCache && analyzer->cache != ZR_NULL) {
        analyzer->cache->astHash = astHash;
        analyzer->cache->isValid = ZR_TRUE;
        
        // 复制诊断信息到缓存
        analyzer->cache->cachedDiagnostics.length = 0;
        for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
            SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
            if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                ZrCore_Array_Push(state, &analyzer->cache->cachedDiagnostics, diagPtr);
            }
        }
    }
    
    return ZR_TRUE;
}

// 获取诊断信息
TZrBool ZrLanguageServer_SemanticAnalyzer_GetDiagnostics(SZrState *state,
                                        SZrSemanticAnalyzer *analyzer,
                                        SZrArray *result) {
    if (state == ZR_NULL || analyzer == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrDiagnostic *), analyzer->diagnostics.length);
    }
    
    // 复制所有诊断
    for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            ZrCore_Array_Push(state, result, diagPtr);
        }
    }
    
    return ZR_TRUE;
}

// 获取位置的符号
SZrSymbol *ZrLanguageServer_SemanticAnalyzer_GetSymbolAt(SZrSemanticAnalyzer *analyzer,
                                         SZrFileRange position) {
    SZrReference *reference;

    if (analyzer == ZR_NULL) {
        return ZR_NULL;
    }

    if (analyzer->referenceTracker != ZR_NULL) {
        reference = ZrLanguageServer_ReferenceTracker_FindReferenceAt(analyzer->referenceTracker, position);
        if (reference != ZR_NULL) {
            return reference->symbol;
        }
    }
    
    return ZrLanguageServer_SymbolTable_FindDefinition(analyzer->symbolTable, position);
}

// 获取悬停信息
TZrBool ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(SZrState *state,
                                     SZrSemanticAnalyzer *analyzer,
                                     SZrFileRange position,
                                     SZrHoverInfo **result) {
    if (state == ZR_NULL || analyzer == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 查找位置的符号
    SZrSymbol *symbol = ZrLanguageServer_SemanticAnalyzer_GetSymbolAt(analyzer, position);
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 构建悬停信息
    TZrChar buffer[512];
    
    // 符号类型
    const TZrChar *typeName = "unknown";
    switch (symbol->type) {
        case ZR_SYMBOL_VARIABLE: typeName = "variable"; break;
        case ZR_SYMBOL_FUNCTION: typeName = "function"; break;
        case ZR_SYMBOL_CLASS: typeName = "class"; break;
        case ZR_SYMBOL_STRUCT: typeName = "struct"; break;
        default: break;
    }
    
    // 符号名称
    TZrNativeString nameStr;
    TZrSize nameLen;
    if (symbol->name->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        nameStr = ZrCore_String_GetNativeStringShort(symbol->name);
        nameLen = symbol->name->shortStringLength;
    } else {
        nameStr = ZrCore_String_GetNativeString(symbol->name);
        nameLen = symbol->name->longStringLength;
    }
    
    snprintf(buffer, sizeof(buffer), "**%s**: %.*s\n\nType: %s",
             typeName, (int)nameLen, nameStr, typeName);
    
    SZrString *contents = ZrCore_String_Create(state, buffer, strlen(buffer));
    if (contents == ZR_NULL) {
        return ZR_FALSE;
    }
    
    *result = ZrLanguageServer_HoverInfo_New(state, buffer, symbol->selectionRange, symbol->typeInfo);
    return *result != ZR_NULL;
}

// 获取代码补全
TZrBool ZrLanguageServer_SemanticAnalyzer_GetCompletions(SZrState *state,
                                       SZrSemanticAnalyzer *analyzer,
                                       SZrFileRange position,
                                       SZrArray *result) {
    ZR_UNUSED_PARAMETER(position);
    if (state == ZR_NULL || analyzer == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrCompletionItem *), 8);
    }
    
    // 获取当前作用域的所有符号
    SZrSymbolScope *scope = ZrLanguageServer_SymbolTable_GetCurrentScope(analyzer->symbolTable);
    if (scope != ZR_NULL) {
        for (TZrSize i = 0; i < scope->symbols.length; i++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&scope->symbols, i);
            if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                SZrSymbol *symbol = *symbolPtr;
                
                // 获取符号名称
                TZrNativeString nameStr;
                TZrSize nameLen;
                if (symbol->name->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
                    nameStr = ZrCore_String_GetNativeStringShort(symbol->name);
                    nameLen = symbol->name->shortStringLength;
                } else {
                    nameStr = ZrCore_String_GetNativeString(symbol->name);
                    nameLen = symbol->name->longStringLength;
                }
                
                // 确定补全类型
                const TZrChar *kind = "variable";
                switch (symbol->type) {
                    case ZR_SYMBOL_FUNCTION: kind = "function"; break;
                    case ZR_SYMBOL_CLASS: kind = "class"; break;
                    case ZR_SYMBOL_STRUCT: kind = "struct"; break;
                    case ZR_SYMBOL_METHOD: kind = "method"; break;
                    case ZR_SYMBOL_PROPERTY: kind = "property"; break;
                    case ZR_SYMBOL_FIELD: kind = "field"; break;
                    default: break;
                }
                
                TZrChar label[256];
                snprintf(label, sizeof(label), "%.*s", (int)nameLen, nameStr);
                
                SZrCompletionItem *item = ZrLanguageServer_CompletionItem_New(state, label, kind, ZR_NULL, ZR_NULL, symbol->typeInfo);
                if (item != ZR_NULL) {
                    ZrCore_Array_Push(state, result, &item);
                }
            }
        }
    }
    
    return ZR_TRUE;
}

// 添加诊断
TZrBool ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(SZrState *state,
                                     SZrSemanticAnalyzer *analyzer,
                                     EZrDiagnosticSeverity severity,
                                     SZrFileRange location,
                                     const TZrChar *message,
                                     const TZrChar *code) {
    if (state == ZR_NULL || analyzer == ZR_NULL || message == ZR_NULL) {
        return ZR_FALSE;
    }
    
    SZrDiagnostic *diagnostic = ZrLanguageServer_Diagnostic_New(state, severity, location, message, code);
    if (diagnostic == ZR_NULL) {
        return ZR_FALSE;
    }
    
    ZrCore_Array_Push(state, &analyzer->diagnostics, &diagnostic);
    
    return ZR_TRUE;
}

// 创建诊断
SZrDiagnostic *ZrLanguageServer_Diagnostic_New(SZrState *state,
                                EZrDiagnosticSeverity severity,
                                SZrFileRange location,
                                const TZrChar *message,
                                const TZrChar *code) {
    if (state == ZR_NULL || message == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrDiagnostic *diagnostic = (SZrDiagnostic *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrDiagnostic));
    if (diagnostic == ZR_NULL) {
        return ZR_NULL;
    }
    
    diagnostic->severity = severity;
    diagnostic->location = location;
    diagnostic->message = ZrCore_String_Create(state, (TZrNativeString)message, strlen(message));
    diagnostic->code = code != ZR_NULL ? ZrCore_String_Create(state, (TZrNativeString)code, strlen(code)) : ZR_NULL;
    
    if (diagnostic->message == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, diagnostic, sizeof(SZrDiagnostic));
        return ZR_NULL;
    }
    
    return diagnostic;
}

// 释放诊断
void ZrLanguageServer_Diagnostic_Free(SZrState *state, SZrDiagnostic *diagnostic) {
    if (state == ZR_NULL || diagnostic == ZR_NULL) {
        return;
    }
    
    if (diagnostic->message != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (diagnostic->code != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    ZrCore_Memory_RawFree(state->global, diagnostic, sizeof(SZrDiagnostic));
}

// 创建补全项
SZrCompletionItem *ZrLanguageServer_CompletionItem_New(SZrState *state,
                                       const TZrChar *label,
                                       const TZrChar *kind,
                                       const TZrChar *detail,
                                       const TZrChar *documentation,
                                       SZrInferredType *typeInfo) {
    if (state == ZR_NULL || label == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrCompletionItem *item = (SZrCompletionItem *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrCompletionItem));
    if (item == ZR_NULL) {
        return ZR_NULL;
    }
    
    item->label = ZrCore_String_Create(state, (TZrNativeString)label, strlen(label));
    item->kind = kind != ZR_NULL ? ZrCore_String_Create(state, (TZrNativeString)kind, strlen(kind)) : ZR_NULL;
    item->detail = detail != ZR_NULL ? ZrCore_String_Create(state, (TZrNativeString)detail, strlen(detail)) : ZR_NULL;
    item->documentation = documentation != ZR_NULL ? ZrCore_String_Create(state, (TZrNativeString)documentation, strlen(documentation)) : ZR_NULL;
    item->typeInfo = typeInfo; // 不复制，只是引用
    
    if (item->label == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, item, sizeof(SZrCompletionItem));
        return ZR_NULL;
    }
    
    return item;
}

// 释放补全项
void ZrLanguageServer_CompletionItem_Free(SZrState *state, SZrCompletionItem *item) {
    if (state == ZR_NULL || item == ZR_NULL) {
        return;
    }
    
    if (item->label != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (item->kind != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (item->detail != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (item->documentation != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    ZrCore_Memory_RawFree(state->global, item, sizeof(SZrCompletionItem));
}

// 创建悬停信息
SZrHoverInfo *ZrLanguageServer_HoverInfo_New(SZrState *state,
                              const TZrChar *contents,
                              SZrFileRange range,
                              SZrInferredType *typeInfo) {
    if (state == ZR_NULL || contents == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrHoverInfo *info = (SZrHoverInfo *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrHoverInfo));
    if (info == ZR_NULL) {
        return ZR_NULL;
    }
    
    info->contents = ZrCore_String_Create(state, (TZrNativeString)contents, strlen(contents));
    info->range = range;
    info->typeInfo = typeInfo; // 不复制，只是引用
    
    if (info->contents == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, info, sizeof(SZrHoverInfo));
        return ZR_NULL;
    }
    
    return info;
}

// 释放悬停信息
void ZrLanguageServer_HoverInfo_Free(SZrState *state, SZrHoverInfo *info) {
    if (state == ZR_NULL || info == ZR_NULL) {
        return;
    }
    
    if (info->contents != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    ZrCore_Memory_RawFree(state->global, info, sizeof(SZrHoverInfo));
}

// 启用/禁用缓存
void ZrLanguageServer_SemanticAnalyzer_SetCacheEnabled(SZrSemanticAnalyzer *analyzer, TZrBool enabled) {
    if (analyzer == ZR_NULL) {
        return;
    }
    analyzer->enableCache = enabled;
}

// 清除缓存
void ZrLanguageServer_SemanticAnalyzer_ClearCache(SZrState *state, SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->cache == ZR_NULL) {
        return;
    }
    
    analyzer->cache->isValid = ZR_FALSE;
    analyzer->cache->astHash = 0;
    
    // 清除缓存的诊断信息
    for (TZrSize i = 0; i < analyzer->cache->cachedDiagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->cache->cachedDiagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            // 注意：诊断可能在 analyzer->diagnostics 中，避免重复释放
        }
    }
    analyzer->cache->cachedDiagnostics.length = 0;
    analyzer->cache->cachedSymbols.length = 0;
}
