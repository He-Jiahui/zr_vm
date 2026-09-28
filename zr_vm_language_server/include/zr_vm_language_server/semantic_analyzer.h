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
#include "zr_vm_parser/diagnostic_builder.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"

/** @brief 增量解析产生的编辑范围与影响分类，供缓存保留判定借用。 */
typedef struct SZrFileChangeInfo SZrFileChangeInfo;

/** @brief LSP 发布层使用的诊断等级，由 parser 结构化诊断投影而来。 */
enum EZrDiagnosticSeverity {
    ZR_DIAGNOSTIC_ERROR,
    ZR_DIAGNOSTIC_WARNING,
    ZR_DIAGNOSTIC_INFO,
    ZR_DIAGNOSTIC_HINT,
};

/** @brief 诊断等级枚举的公开类型别名，供诊断构造与发布 API 传递。 */
typedef enum EZrDiagnosticSeverity EZrDiagnosticSeverity;

/** @brief 诊断附加的跨位置线索；message 由状态中的 GC 管理。 */
typedef struct SZrDiagnosticRelatedInformation {
    SZrFileRange location;
    SZrString *message;
} SZrDiagnosticRelatedInformation;

/** @brief 可投影为 code action 的单项修复，范围与适用性来自 parser 诊断。 */
typedef struct SZrDiagnosticFix {
    SZrString *title;
    SZrFileRange editRange;
    SZrString *editText;
    EZrDiagnosticFixApplicability applicability;
} SZrDiagnosticFix;

/**
 * @brief 汇集 parser 错误、语义事实和可选修复，供发布诊断与 code action 复用。
 * @note 结构本体及两个附属数组由 Diagnostic_Free 释放；字符串由 GC 管理。
 */
typedef struct SZrDiagnostic {
    EZrDiagnosticSeverity severity;
    SZrFileRange location;
    SZrString *message;
    SZrString *code;                   // 错误代码（可选）
    SZrString *cause;                  // 具体原因（可选）
    SZrString *suggestion;             // 修复建议（可选）
    SZrArray relatedInformation;       // SZrDiagnosticRelatedInformation
    SZrArray fixes;                    // SZrDiagnosticFix
    TZrUInt32 descriptorId;
    SZrString *codeDescriptionHref;
    EZrDiagnosticNoFixReason noFixReason;
} SZrDiagnostic;

/**
 * @brief 补全查询交给 LSP 序列化层的展示项。
 * @note 字符串由 GC 管理；可选 typeInfo 只借用，不随 CompletionItem_Free 释放。
 */
typedef struct SZrCompletionItem {
    SZrString *label;                  // 补全标签
    SZrString *kind;                  // 补全类型（variable, function, class等）
    SZrString *detail;                // 详细信息（类型签名等）
    SZrString *documentation;         // 文档（可选）
    SZrInferredType *typeInfo;        // 类型信息（可选）
} SZrCompletionItem;

/**
 * @brief 悬停文本与对应源码范围的展示结果。
 * @note contents 由 GC 管理；typeInfo 只借用，调用方不得让它先于结果消费者失效。
 */
typedef struct SZrHoverInfo {
    SZrString *contents;               // 内容（markdown格式）
    SZrFileRange range;                // 范围
    SZrInferredType *typeInfo;        // 类型信息（可选）
} SZrHoverInfo;

/**
 * @brief 用 AST 内容哈希和声明范围验证可复用的语义查询结果。
 * @note 只有 isValid 且哈希、范围与旧 AST 保有关系均匹配时才能复用借用事实。
 */
typedef struct SZrAnalysisCache {
    SZrFileRange cacheRange;           // 缓存范围
    SZrArray cachedDiagnostics;        // 缓存的诊断信息
    SZrArray cachedSymbols;            // 缓存的符号（用于补全等）
    TZrBool isValid;                     // 缓存是否有效
    TZrSize astHash;                   // AST 哈希（用于验证缓存有效性）
    TZrSize scopeAstHash;              // scope AST 哈希（用于坐标稳定的跨快照复用）
} SZrAnalysisCache;

/** @brief 记录请求、实际执行、命中及局部缓存失效原因，供回归用例验证重算边界。 */
typedef struct SZrSemanticAnalysisMetrics {
    TZrSize requestCount;
    TZrSize executionCount;
    TZrSize cacheHitCount;
    TZrSize scopedCacheInvalidationCount;
    TZrSize scopedCacheDirectDependencyInvalidationCount;
    TZrSize scopedCacheConservativeInvalidationCount;
    TZrSize scopedCachePreservationCount;
    SZrFileRange lastExecutionRange;
} SZrSemanticAnalysisMetrics;

/**
 * @brief 按文档版本聚合符号、引用、诊断和 parser 语义上下文，供 LSP 查询。
 * @note ast、semanticContext 与 hirModule 通常借用文档或编译器状态；
 * scopedQueryAnalyzer 和 ownedAst 由本对象拥有，borrowedAst 由历史快照持有。
 * 文档更新必须先完成旧 AST 的缓存保留判定，再交由解析器替换 AST。
 */
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
    TZrUInt64 externalProviderGeneration; // LSP provider generation to apply on the next analysis
    FZrSemanticVirtualDeclarationUriResolver virtualDeclarationUriResolver;
    TZrPtr virtualDeclarationUriResolverUserData;
    struct SZrSemanticAnalyzer *scopedQueryAnalyzer; // 独立的单作用域查询缓存（所有）
    SZrAstNode *ownedAst;              // scoped cache保留的旧AST（可选，所有）
    SZrAstNode *borrowedAst;           // 由历史semantic snapshot持有的旧AST（可选，借用）
    TZrBool preserveScopedQueryAnalyzerOnNextAstChange;
    TZrSize cacheStorageAccessOrder;   // workspace cache LRU access order
    SZrSemanticAnalysisMetrics metrics;
} SZrSemanticAnalyzer;

/**
 * @brief 为一个文档版本建立符号、引用与诊断的拥有者；首次分析时再建编译器事实。
 * @return 成功时返回需用 SemanticAnalyzer_Free 释放的分析器。
 */
ZR_LANGUAGE_SERVER_API SZrSemanticAnalyzer *ZrLanguageServer_SemanticAnalyzer_New(SZrState *state);

/** @brief 设定主分析与局部查询的缓存策略；关闭缓存不等于立即释放已分配存储。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_SemanticAnalyzer_SetCacheEnabled(SZrSemanticAnalyzer *analyzer, TZrBool enabled);

/** @brief 文档或外部 provider 变化时撤销可复用事实和局部查询分析器。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_SemanticAnalyzer_ClearCache(SZrState *state, SZrSemanticAnalyzer *analyzer);
/** @brief 估算主分析器与局部分析器占用的可回收缓存字节，供工作区 LRU 预算管理。 */
ZR_LANGUAGE_SERVER_API TZrSize
ZrLanguageServer_SemanticAnalyzer_GetCacheStorageBytes(
        const SZrSemanticAnalyzer *analyzer);
/** @brief 在工作区预算回收时释放可重建的缓存存储，同时保留分析器身份。 */
ZR_LANGUAGE_SERVER_API void
ZrLanguageServer_SemanticAnalyzer_ReleaseCacheStorage(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer);
/** @brief 拷贝分析请求与缓存指标，供测试比较编辑前后的重算范围。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_SemanticAnalyzer_GetMetrics(
    const SZrSemanticAnalyzer *analyzer,
    SZrSemanticAnalysisMetrics *outMetrics);
/**
 * @brief 获取主分析器拥有的局部查询分析器，避免局部请求覆盖全量事实。
 * @note 返回借用指针；主分析器重置缓存、失效或释放后不可保留此指针。
 */
ZR_LANGUAGE_SERVER_API SZrSemanticAnalyzer *
ZrLanguageServer_SemanticAnalyzer_GetOrCreateScopedQueryAnalyzer(
    SZrState *state,
    SZrSemanticAnalyzer *analyzer);
/** @brief 撤销局部事实及旧 AST 借用，供编辑、provider 切换或历史快照退出调用。 */
ZR_LANGUAGE_SERVER_API void
ZrLanguageServer_SemanticAnalyzer_InvalidateScopedQueryAnalyzer(
    SZrState *state,
    SZrSemanticAnalyzer *analyzer);
/**
 * @brief 解析器替换旧树前，判断局部缓存能否跨编辑存活并报告是否要保留旧 AST。
 * @pre currentAst、changeInfo 对应当前文档版本；调用后须按 retainCurrentAst 选择解析入口。
 * @note 返回假时局部查询缓存已失效，retainCurrentAst 会置假。
 */
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_SemanticAnalyzer_PrepareScopedQueryCacheForChange(
    SZrState *state,
    SZrSemanticAnalyzer *analyzer,
    SZrAstNode *currentAst,
    const SZrFileChangeInfo *changeInfo,
    TZrBool *retainCurrentAst);
/**
 * @brief 解析新树后再次校验声明范围和内容，决定是否沿用旧局部语义事实。
 * @pre state 非空，newAst 属于替换后的文档版本。
 * @note retainedAst 非空时由本函数在成功路径接管，失败路径释放；调用方不得重复释放。
 */
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_SemanticAnalyzer_CommitScopedQueryCachePreservation(
    SZrState *state,
    SZrSemanticAnalyzer *analyzer,
    SZrAstNode *newAst,
    SZrAstNode *retainedAst);

/** @brief 释放分析器拥有的事实与缓存；普通 ast 仍由文档版本持有。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_SemanticAnalyzer_Free(SZrState *state, SZrSemanticAnalyzer *analyzer);

/**
 * @brief 让项目索引或打开文档发布整棵 AST 的符号、类型和诊断事实。
 * @pre ast 在分析器使用期间由文档或快照持有；不得在查询前释放。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_SemanticAnalyzer_Analyze(SZrState *state, 
                                                         SZrSemanticAnalyzer *analyzer,
                                                         SZrAstNode *ast);
/**
 * @brief 对一棵 AST 中的声明根进行局部语义查询；缓存命中时可复用旧事实。
 * @pre scopeRoot 必须为 ast 本身或其中受支持的声明节点；旧事实借用的 AST 须继续存活。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
    SZrState *state,
    SZrSemanticAnalyzer *analyzer,
    SZrAstNode *ast,
    SZrAstNode *scopeRoot);
/** @brief 按文档位置选出可独立分析的最内层声明，供编辑分类与局部查询共用。 */
ZR_LANGUAGE_SERVER_API SZrAstNode *
ZrLanguageServer_SemanticAnalyzer_FindAnalysisRootAtPosition(
    SZrAstNode *ast,
    SZrFileRange position);
/**
 * @brief 在增量解析前判定编辑影响声明正文、签名或整个模块，以免误用局部缓存。
 * @note 边界编辑或无法定位声明时保守地标为模块级影响。
 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_SemanticAnalyzer_ClassifyFileChange(
    SZrAstNode *ast,
    SZrFileChangeInfo *changeInfo);

/**
 * @brief 把当前分析的诊断借用指针追加到结果数组，供 LSP 立即序列化。
 * @pre result 经 Array_Construct 或 Array_Init 准备；调用方释放数组容器。
 * @note 元素仍归分析器所有；下一轮分析或分析器释放后不可继续使用。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_SemanticAnalyzer_GetDiagnostics(SZrState *state,
                                                                SZrSemanticAnalyzer *analyzer,
                                                                SZrArray *result);

/** @brief 通过 parser 规范 SymbolId 找到展示层符号；结果由分析器的符号表持有。 */
ZR_LANGUAGE_SERVER_API SZrSymbol *ZrLanguageServer_SemanticAnalyzer_GetSymbolAt(SZrSemanticAnalyzer *analyzer,
                                                                 SZrFileRange position);

/**
 * @brief 从规范类型事实复制位置处的精确类型，供表达式悬停展示。
 * @pre outType 已由调用方初始化；成功后的复制内容也由调用方释放。
 * @return 缺少精确事实或已解析类型引用时返回假。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_SemanticAnalyzer_ResolveTypeAtPosition(
    SZrState *state,
    SZrSemanticAnalyzer *analyzer,
    SZrFileRange position,
    SZrInferredType *outType);

/** @brief 用 parser 的规范类型格式化规则写入调用方缓冲区，保持各 LSP 展示入口一致。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_SemanticAnalyzer_FormatTypeId(
    const SZrSemanticContext *semanticContext,
    TZrTypeId typeId,
    TZrChar *buffer,
    TZrSize bufferSize);

/**
 * @brief 把符号或精确表达式事实转成悬停文本；成功结果由调用方用 HoverInfo_Free 释放。
 * @note 可选 typeInfo 通常借用分析器符号，须在分析器重算前消费。
 * BUG: 无符号但解析出精确表达式类型时，实现把栈上 resolvedType 借给结果并立即释放其内部资源；
 * 读取返回结果的 typeInfo 会遇到悬空指针，见 semantic_analyzer.c 中该分支。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(SZrState *state,
                                                             SZrSemanticAnalyzer *analyzer,
                                                             SZrFileRange position,
                                                             SZrHoverInfo **result);

/**
 * @brief 创建可直接发布的诊断，包括原始错误或分析不可用提示。
 * @note 返回对象由调用方用 Diagnostic_Free 释放；message 必须非空。
 */
ZR_LANGUAGE_SERVER_API SZrDiagnostic *ZrLanguageServer_Diagnostic_New(SZrState *state,
                                                         EZrDiagnosticSeverity severity,
                                                         SZrFileRange location,
                                                         const TZrChar *message,
                                                         const TZrChar *code);
/**
 * @brief 复制 parser 结构化诊断的文本、附加位置与修复，供 LSP 响应独立持有。
 * @note 位置中的 source 仍借用原有 GC 字符串。
 * @return 失败时返回 ZR_NULL，已分配的中间对象由本函数清理。
 */
ZR_LANGUAGE_SERVER_API SZrDiagnostic *ZrLanguageServer_Diagnostic_FromStructured(
    SZrState *state,
    const SZrStructuredDiagnostic *structured);
/** @brief 为诊断补充跨位置线索，供编辑器解释错误因果关系。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Diagnostic_AddRelatedInformation(
    SZrState *state,
    SZrDiagnostic *diagnostic,
    SZrFileRange location,
    const TZrChar *message);
/** @brief 把 parser 的结构化修复复制到诊断，供 code action 投影读取。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Diagnostic_AddFix(
    SZrState *state,
    SZrDiagnostic *diagnostic,
    const SZrStructuredDiagnosticFix *structuredFix);

/** @brief 释放诊断本体与附属数组；GC 字符串仍归状态管理。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Diagnostic_Free(SZrState *state, SZrDiagnostic *diagnostic);

/**
 * @brief 创建供补全响应合并与序列化的展示项。
 * @note 可选 typeInfo 只借用；项本体由调用方用 CompletionItem_Free 释放。
 */
ZR_LANGUAGE_SERVER_API SZrCompletionItem *ZrLanguageServer_CompletionItem_New(SZrState *state,
                                                                 const TZrChar *label,
                                                                 const TZrChar *kind,
                                                                 const TZrChar *detail,
                                                                 const TZrChar *documentation,
                                                                 SZrInferredType *typeInfo);

/** @brief 释放补全项本体；借用类型与 GC 字符串不在此释放。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_CompletionItem_Free(SZrState *state, SZrCompletionItem *item);

/**
 * @brief 为悬停查询创建文本与范围结果；typeInfo 仅借用。
 * @pre 若传入 typeInfo，其对象及内部资源须活到结果最后一次被读取。
 */
ZR_LANGUAGE_SERVER_API SZrHoverInfo *ZrLanguageServer_HoverInfo_New(SZrState *state,
                                                      const TZrChar *contents,
                                                      SZrFileRange range,
                                                      SZrInferredType *typeInfo);

/** @brief 释放悬停结果本体；不释放借用类型及 GC 字符串。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_HoverInfo_Free(SZrState *state, SZrHoverInfo *info);

#endif //ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_H
