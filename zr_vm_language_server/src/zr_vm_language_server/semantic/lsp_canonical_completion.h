#ifndef ZR_VM_LANGUAGE_SERVER_LSP_CANONICAL_COMPLETION_H
#define ZR_VM_LANGUAGE_SERVER_LSP_CANONICAL_COMPLETION_H

#include "zr_vm_language_server/semantic_analyzer.h"

/**
 * @brief 从当前 parser 语义快照向 LSP 补全结果追加可见的精确声明。
 * @pre semanticContext 与 position 必须属于同一文档快照；result 保存 SZrCompletionItem *。
 * @note 返回值表示本次是否追加项目，不能据此区分空作用域与解析失败。
 */
TZrBool ZrLanguageServer_LspCanonicalCompletion_AppendVisibleSymbols(
        SZrState *state,
        const SZrSemanticContext *semanticContext,
        SZrFileRange position,
        SZrArray *result);

#endif
