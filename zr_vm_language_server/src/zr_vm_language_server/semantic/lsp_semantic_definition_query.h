#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_DEFINITION_QUERY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_DEFINITION_QUERY_H

#include "semantic/lsp_semantic_query.h"

/** @brief 为本地符号导航追加当前使用点可达的赋值定义；无对应定义时退回声明。
 * @pre query 已解析为本地符号并持有有效语义上下文；调用方统一释放 Location 结果。 */
TZrBool ZrLanguageServer_LspSemanticDefinitionQuery_AppendReachingDefinition(
    SZrState *state,
    SZrLspContext *context,
    SZrLspSemanticQuery *query,
    SZrArray *result);

#endif
