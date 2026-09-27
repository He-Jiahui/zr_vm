#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_TYPE_HIERARCHY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_TYPE_HIERARCHY_H

#include "semantic/lsp_semantic_query.h"

/** @brief 为光标下的已解析类型创建带快照身份的层级项目。
 * @note 编辑后必须重新 prepare；结果由 FreeHierarchyItems 释放。 */
TZrBool ZrLanguageServer_LspSemanticTypeHierarchy_Prepare(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrLspPosition position,
        SZrArray *result);
/** @brief 只投影当前语义快照中可验证的直接基类型；空集合也是正常结果。
 * @note 输入项目需来自当前文档版本，结果由 FreeHierarchyItems 释放。 */
TZrBool ZrLanguageServer_LspSemanticTypeHierarchy_AppendSupertypes(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrArray *result);
/** @brief 只投影当前语义快照中可验证的直接派生类型；空集合也是正常结果。
 * @note 输入项目需来自当前文档版本，结果由 FreeHierarchyItems 释放。 */
TZrBool ZrLanguageServer_LspSemanticTypeHierarchy_AppendSubtypes(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrArray *result);

#endif
