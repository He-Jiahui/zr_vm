#ifndef ZR_VM_LANGUAGE_SERVER_LSP_CANONICAL_SYMBOL_DISPLAY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_CANONICAL_SYMBOL_DISPLAY_H

#include "zr_vm_language_server/semantic_analyzer.h"

TZrBool ZrLanguageServer_Lsp_FormatSymbolCanonicalDeclarationType(
        SZrSemanticAnalyzer *analyzer,
        SZrSymbol *symbol,
        TZrChar *buffer,
        TZrSize bufferSize);

TZrBool ZrLanguageServer_Lsp_FormatExactExpressionType(
        SZrSemanticAnalyzer *analyzer,
        const SZrAstNode *expression,
        TZrChar *buffer,
        TZrSize bufferSize);

#endif
