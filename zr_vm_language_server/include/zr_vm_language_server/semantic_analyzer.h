//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_H
#define ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_H

#include "zr_vm_language_server/conf.h"
#include "zr_vm_language_server/symbol_table.h"
#include "zr_vm_language_server/reference_tracker.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/type_system.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"

// 诊断严重程度
enum EZrDiagnosticSeverity {
    ZR_DIAGNOSTIC_ERROR,
    ZR_DIAGNOSTIC_WARNING,
    ZR_DIAGNOSTIC_INFO,
    ZR_DIAGNOSTIC_HINT,
};

typedef enum EZrDiagnosticSeverity EZrDiagnosticSeverity;

typedef struct SZrDiagnosticRelatedInformation {
    SZrFileRange location;
    SZrString *message;
} SZrDiagnosticRelatedInformation;

// 诊断信息
typedef struct SZrDiagnostic {
    EZrDiagnosticSeverity severity;
    SZrFileRange location;
    SZrString *message;
    SZrString *code;                   // 错误代码（可选）
    SZrArray relatedInformation;       // SZrDiagnosticRelatedInformation
} SZrDiagnostic;

// 代码补全项
typedef struct SZrCompletionItem {
    SZrString *label;                  // 补全标签
    SZrString *kind;                  // 补全类型（variable, function, class等）
    SZrString *detail;                // 详细信息（类型签名等）
    SZrString *documentation;         // 文档（可选）
    SZrInferredType *typeInfo;        // 类型信息（可选）
} SZrCompletionItem;

// 悬停信息
typedef struct SZrHoverInfo {
    SZrString *contents;               // 内容（markdown格式）
    SZrFileRange range;                // 范围
    SZrInferredType *typeInfo;        // 类型信息（可选）
} SZrHoverInfo;

// 分析结果缓存
typedef struct SZrAnalysisCache {
    SZrFileRange cacheRange;           // 缓存范围
    SZrArray cachedDiagnostics;        // 缓存的诊断信息
    SZrArray cachedSymbols;            // 缓存的符号（用于补全等）
    TZrBool isValid;                     // 缓存是否有效
    TZrSize astHash;                   // AST 哈希（用于验证缓存有效性）
} SZrAnalysisCache;

// 语义分析器
typedef struct SZrSemanticAnalyzer {
    SZrState *state;
    SZrSymbolTable *symbolTable;
    SZrReferenceTracker *referenceTracker;
    SZrArray diagnostics;              // 诊断信息数组（SZrDiagnostic*）
    SZrAstNode *ast;                   // 当前分析的 AST
    SZrAnalysisCache *cache;           // 分析结果缓存
    TZrBool enableCache;                 // 是否启用缓存
    SZrCompilerState *compilerState;   // 编译器状态（用于类型推断）
    SZrSemanticContext *semanticContext; // 当前分析共享的语义上下文（借用）
    SZrHirModule *hirModule;           // 当前分析共享的 HIR 模块（借用）
} SZrSemanticAnalyzer;

// 语义分析器管理函数

// 创建语义分析器
ZR_LANGUAGE_SERVER_API SZrSemanticAnalyzer *ZrLanguageServer_SemanticAnalyzer_New(SZrState *state);

// 启用/禁用缓存
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_SemanticAnalyzer_SetCacheEnabled(SZrSemanticAnalyzer *analyzer, TZrBool enabled);

// 清除缓存
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_SemanticAnalyzer_ClearCache(SZrState *state, SZrSemanticAnalyzer *analyzer);

// 释放语义分析器
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_SemanticAnalyzer_Free(SZrState *state, SZrSemanticAnalyzer *analyzer);

// 分析 AST
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_SemanticAnalyzer_Analyze(SZrState *state, 
                                                         SZrSemanticAnalyzer *analyzer,
                                                         SZrAstNode *ast);

// 获取诊断信息
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_SemanticAnalyzer_GetDiagnostics(SZrState *state,
                                                                SZrSemanticAnalyzer *analyzer,
                                                                SZrArray *result);

// 获取位置的符号
ZR_LANGUAGE_SERVER_API SZrSymbol *ZrLanguageServer_SemanticAnalyzer_GetSymbolAt(SZrSemanticAnalyzer *analyzer,
                                                                 SZrFileRange position);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_SemanticAnalyzer_ResolveTypeAtPosition(
    SZrState *state,
    SZrSemanticAnalyzer *analyzer,
    SZrFileRange position,
    SZrInferredType *outType);

// 获取悬停信息
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(SZrState *state,
                                                             SZrSemanticAnalyzer *analyzer,
                                                             SZrFileRange position,
                                                             SZrHoverInfo **result);

// 获取代码补全
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_SemanticAnalyzer_GetCompletions(SZrState *state,
                                                               SZrSemanticAnalyzer *analyzer,
                                                               SZrFileRange position,
                                                               SZrArray *result);

// 添加诊断
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(SZrState *state,
                                                             SZrSemanticAnalyzer *analyzer,
                                                             EZrDiagnosticSeverity severity,
                                                             SZrFileRange location,
                                                             const TZrChar *message,
                                                             const TZrChar *code);

// 诊断管理函数

// 创建诊断
ZR_LANGUAGE_SERVER_API SZrDiagnostic *ZrLanguageServer_Diagnostic_New(SZrState *state,
                                                        EZrDiagnosticSeverity severity,
                                                        SZrFileRange location,
                                                        const TZrChar *message,
                                                        const TZrChar *code);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Diagnostic_AddRelatedInformation(
    SZrState *state,
    SZrDiagnostic *diagnostic,
    SZrFileRange location,
    const TZrChar *message);

// 释放诊断
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Diagnostic_Free(SZrState *state, SZrDiagnostic *diagnostic);

// 补全项管理函数

// 创建补全项
ZR_LANGUAGE_SERVER_API SZrCompletionItem *ZrLanguageServer_CompletionItem_New(SZrState *state,
                                                                 const TZrChar *label,
                                                                 const TZrChar *kind,
                                                                 const TZrChar *detail,
                                                                 const TZrChar *documentation,
                                                                 SZrInferredType *typeInfo);

// 释放补全项
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_CompletionItem_Free(SZrState *state, SZrCompletionItem *item);

// 悬停信息管理函数

// 创建悬停信息
ZR_LANGUAGE_SERVER_API SZrHoverInfo *ZrLanguageServer_HoverInfo_New(SZrState *state,
                                                      const TZrChar *contents,
                                                      SZrFileRange range,
                                                      SZrInferredType *typeInfo);

// 释放悬停信息
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_HoverInfo_Free(SZrState *state, SZrHoverInfo *info);

#endif //ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_H
