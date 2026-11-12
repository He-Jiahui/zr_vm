#ifndef ZR_VM_LANGUAGE_SERVER_LSP_CANONICAL_HOVER_H
#define ZR_VM_LANGUAGE_SERVER_LSP_CANONICAL_HOVER_H

#include "zr_vm_language_server/lsp_interface.h"
#include "zr_vm_parser/semantic_query.h"

/**
 * @brief 将 parser 的符号身份投影为编辑器悬停，并合并同一源码声明的文档与 FFI 信息。
 * @pre symbol、semanticContext 和 referenceRange 来自同一语义快照；sourceSymbol 仅在身份一致时参与补充。
 * @note 创建的 hover 交给调用方；原始 AST、符号与文档内容在本调用期间必须仍有效。
 * 有签名时只展示签名。否则格式化同一快照的 canonical TypeId：排版成功写 Resolved Type。
 * 类型节点存在但排不成完整悬停文本时写 Type: unknown，与表达式悬停相同，不把排版失败说成无法推断。
 * 没有类型身份时仍写 Type: cannot infer exact type。这不是诊断 2020，也不附带自动修复。
 */
TZrBool ZrLanguageServer_LspCanonicalHover_BuildSymbol(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        const SZrSemanticContext *semanticContext,
        const SZrParserSemanticSymbolQuery *symbol,
        SZrFileRange referenceRange,
        SZrAstNode *documentAst,
        SZrSymbol *sourceSymbol,
        SZrLspHover **result);

#endif
