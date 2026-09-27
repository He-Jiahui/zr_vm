#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_IMPLEMENTATION_QUERY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_IMPLEMENTATION_QUERY_H

#include "semantic/lsp_semantic_query.h"

/** @brief 为 implementation 请求投影当前模块语义关系中的实现和覆盖位置。
 * @note 仅处理可解析为本地符号的光标目标；返回值表示是否找到可输出的位置。 */
TZrBool ZrLanguageServer_LspSemanticImplementationQuery_Append(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrLspPosition position,
        SZrArray *result);

#endif
