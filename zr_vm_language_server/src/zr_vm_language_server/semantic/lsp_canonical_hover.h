#ifndef ZR_VM_LANGUAGE_SERVER_LSP_CANONICAL_HOVER_H
#define ZR_VM_LANGUAGE_SERVER_LSP_CANONICAL_HOVER_H

#include "zr_vm_language_server/lsp_interface.h"
#include "zr_vm_parser/semantic_query.h"

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
