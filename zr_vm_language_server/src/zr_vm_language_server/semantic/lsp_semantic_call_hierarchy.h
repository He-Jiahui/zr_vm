#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_CALL_HIERARCHY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_CALL_HIERARCHY_H

#include "semantic/lsp_semantic_query.h"

/** @brief 将光标下已解析的可调用声明变成可供后续层级请求回传的项目。
 * @note 项目绑定当前文档版本与语义 ID；调用方应在编辑后重新 prepare，并用 FreeHierarchyItems 释放结果。 */
TZrBool ZrLanguageServer_LspSemanticCallHierarchy_Prepare(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrLspPosition position,
        SZrArray *result);
/** @brief 根据 prepare 项目查询直接调用者及其调用点；过期项目会被拒绝。
 * @note 结果中的调用项由 FreeHierarchyCalls 释放；请求取消时可能已有部分结果。 */
TZrBool ZrLanguageServer_LspSemanticCallHierarchy_AppendIncoming(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrArray *result);
/** @brief 根据 prepare 项目查询直接被调函数及其调用点；仅接受当前快照中的项目。
 * @note 结果中的调用项由 FreeHierarchyCalls 释放；请求取消时可能已有部分结果。 */
TZrBool ZrLanguageServer_LspSemanticCallHierarchy_AppendOutgoing(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrArray *result);

#endif
