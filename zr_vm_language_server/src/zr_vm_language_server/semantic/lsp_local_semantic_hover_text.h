#ifndef ZR_VM_LANGUAGE_SERVER_LSP_LOCAL_SEMANTIC_HOVER_TEXT_H
#define ZR_VM_LANGUAGE_SERVER_LSP_LOCAL_SEMANTIC_HOVER_TEXT_H

#include "semantic/lsp_local_semantic_query.h"

/**
 * @brief 按统一顺序合并表达式、数值、逻辑、可达性和所有权事实。
 * @pre query 中各 fact 均借自同一个仍有效的 analyzer 快照。
 */
TZrBool ZrLanguageServer_LspLocalSemanticHoverText_AppendFacts(
    TZrChar *buffer,
    TZrSize bufferSize,
    TZrSize *used,
    const SZrLspLocalSemanticQueryResult *query);

/**
 * @brief 为已有符号 hover 生成可选事实附录；无 fact 时返回空指针。
 * @pre query 的借用指针在调用期间有效；返回字符串由 state 管理。
 */
SZrString *ZrLanguageServer_LspLocalSemanticHoverText_BuildFactMarkdown(
    SZrState *state,
    const SZrLspLocalSemanticQueryResult *query);

/**
 * @brief 避免重复内容后连接两个 Markdown 段，返回原段或新建字符串。
 * @note 缓冲区不足时保留 base；调用方据此仍可返回已有悬停内容。
 */
SZrString *ZrLanguageServer_LspLocalSemanticHoverText_AppendMarkdownSection(
    SZrState *state,
    SZrString *base,
    SZrString *appendix);

#endif
