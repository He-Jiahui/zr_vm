#ifndef ZR_VM_LANGUAGE_SERVER_LSP_LOCAL_SEMANTIC_HOVER_TEXT_H
#define ZR_VM_LANGUAGE_SERVER_LSP_LOCAL_SEMANTIC_HOVER_TEXT_H

#include "semantic/lsp_local_semantic_query.h"

TZrBool ZrLanguageServer_LspLocalSemanticHoverText_AppendFacts(
    TZrChar *buffer,
    TZrSize bufferSize,
    TZrSize *used,
    const SZrLspLocalSemanticQueryResult *query);

SZrString *ZrLanguageServer_LspLocalSemanticHoverText_BuildFactMarkdown(
    SZrState *state,
    const SZrLspLocalSemanticQueryResult *query);

SZrString *ZrLanguageServer_LspLocalSemanticHoverText_AppendMarkdownSection(
    SZrState *state,
    SZrString *base,
    SZrString *appendix);

#endif
