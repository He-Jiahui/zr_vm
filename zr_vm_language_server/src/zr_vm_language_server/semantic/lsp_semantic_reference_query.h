#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_REFERENCE_QUERY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_REFERENCE_QUERY_H

#include "semantic/lsp_semantic_query.h"

/** @brief 为本地规范符号收集本快照的已解析引用，可选择包含声明。
 * @note 跨快照引用由上层 AppendReferences 另行合并，调用方须释放结果中的 Location。 */
TZrBool ZrLanguageServer_LspSemanticReferenceQuery_AppendReferences(
        SZrState *state,
        SZrLspContext *context,
        SZrLspSemanticQuery *query,
        TZrBool includeDeclaration,
        SZrArray *result);
/** @brief 将某分析器的源范围绑定到文档 URI 后加入去重的位置列表。
 * @note 跨快照收集器也调用此接口；analyzer 必须仍持有产生该范围的快照。 */
TZrBool ZrLanguageServer_LspSemanticReferenceQuery_AppendRange(
        SZrState *state,
        SZrLspContext *context,
        const SZrSemanticAnalyzer *analyzer,
        SZrArray *result,
        SZrFileRange range);
/** @brief 只为请求文档投影符号引用的读写高亮；外部目标先核验 provider 代次。
 * @note 已追加的高亮可由上层统一释放；取消请求可能留下部分结果。 */
TZrBool ZrLanguageServer_LspSemanticReferenceQuery_AppendHighlights(
        SZrState *state,
        SZrLspContext *context,
        SZrLspSemanticQuery *query,
        SZrArray *result);

#endif
