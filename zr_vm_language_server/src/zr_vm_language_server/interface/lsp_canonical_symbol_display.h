#ifndef ZR_VM_LANGUAGE_SERVER_LSP_CANONICAL_SYMBOL_DISPLAY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_CANONICAL_SYMBOL_DISPLAY_H

#include "zr_vm_language_server/semantic_analyzer.h"
#include "zr_vm_parser/semantic_query.h"

TZrBool ZrLanguageServer_Lsp_FormatSymbolCanonicalDeclarationType(
        SZrSemanticAnalyzer *analyzer,
        SZrSymbol *symbol,
        TZrChar *buffer,
        TZrSize bufferSize);

TZrBool ZrLanguageServer_Lsp_FormatCanonicalDeclarationType(
        SZrSemanticAnalyzer *analyzer,
        const SZrParserSemanticSymbolQuery *declaration,
        TZrChar *buffer,
        TZrSize bufferSize);

TZrBool ZrLanguageServer_Lsp_FormatExactExpressionType(
        SZrSemanticAnalyzer *analyzer,
        const SZrAstNode *expression,
        TZrChar *buffer,
        TZrSize bufferSize);

#endif
