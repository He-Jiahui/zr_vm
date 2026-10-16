#ifndef ZR_VM_LANGUAGE_SERVER_LSP_LOCAL_SEMANTIC_QUERY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_LOCAL_SEMANTIC_QUERY_H

#include "zr_vm_language_server/lsp_interface.h"
#include "zr_vm_parser/semantic_facts.h"

/** 区分无事实、可显示事实及当前位置被 parser 诊断遮挡的结果。 */
typedef enum EZrLspLocalSemanticQueryStatus {
    ZR_LSP_LOCAL_SEMANTIC_QUERY_UNKNOWN = 0,
    ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT,
    ZR_LSP_LOCAL_SEMANTIC_QUERY_DIAGNOSTIC_FAILURE
} EZrLspLocalSemanticQueryStatus;

/**
 * 查询范围与所有 fact 指针绑定到一次文档语义快照；本结构不拥有 fact 或诊断。
 * 清除只重置借用视图，不能延长 analyzer/文件版本的存活时间。
 */
typedef struct SZrLspLocalSemanticQueryResult {
    EZrLspLocalSemanticQueryStatus status;
    SZrFileRange queryRange;
    SZrDiagnostic *diagnostic;
    const SZrSemanticExpressionFact *expressionFact;
    const SZrSemanticNumericFact *numericFact;
    const SZrSemanticReferenceFact *referenceFact;
    const SZrSemanticReachabilityFact *reachabilityFact;
    const SZrSemanticLogicalFact *logicalFact;
    const SZrSemanticOwnershipFact *ownershipFact;
    const SZrOwnershipIntrinsicFact *ownershipIntrinsicFact;
} SZrLspLocalSemanticQueryResult;

/** @brief 将栈上查询结果置为可供首次查询的空借用视图。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspLocalSemanticQuery_Init(
    SZrLspLocalSemanticQueryResult *result);

/** @brief 在重用结果或退出请求前解除旧 fact/诊断引用。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspLocalSemanticQuery_Clear(
    SZrLspLocalSemanticQueryResult *result);

/**
 * @brief 为 hover 查询当前光标处的所有局部语义事实。
 * @pre context 和 uri 指向当前文档；结果只可在 analyzer 快照未失效时读取。
 * @note 成功执行查询也可能得到 UNKNOWN 或 DIAGNOSTIC_FAILURE 状态。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(
    SZrState *state,
    SZrLspContext *context,
    SZrString *uri,
    SZrLspPosition position,
    SZrLspLocalSemanticQueryResult *result);

/** @brief 只查询引用事实，供轻量导航路径跳过无关表达式事实收集。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspLocalSemanticQuery_ReferenceAt(
    SZrState *state,
    SZrLspContext *context,
    SZrString *uri,
    SZrLspPosition position,
    SZrLspLocalSemanticQueryResult *result);

/** @brief 在无文档上下文时从已取得的 facts 建立悬停；类型展示可能退化为 unknown。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspLocalSemanticQuery_BuildHover(
    SZrState *state,
    const SZrLspLocalSemanticQueryResult *query,
    SZrLspHover **result);

/** @brief 借当前文档 canonical 类型补充悬停；调用期间 query 的事实快照须仍有效。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspLocalSemanticQuery_BuildHoverForDocument(
    SZrState *state,
    SZrLspContext *context,
    SZrString *uri,
    const SZrLspLocalSemanticQueryResult *query,
    SZrLspHover **result);

/** @brief 把局部事实附到已有符号悬停，供多条解析路径共用同一事实呈现。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspLocalSemanticQuery_AppendFactsToHover(
    SZrState *state,
    const SZrLspLocalSemanticQueryResult *query,
    SZrLspHover *hover);

#endif
