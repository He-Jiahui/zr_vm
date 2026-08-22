#ifndef ZR_VM_LANGUAGE_SERVER_LSP_PROPERTY_CONTRACT_H
#define ZR_VM_LANGUAGE_SERVER_LSP_PROPERTY_CONTRACT_H

#include "zr_vm_language_server/semantic_analyzer.h"
#include "zr_vm_parser/semantic_query.h"

TZrBool ZrLanguageServer_LspPropertyContract_RegisterSourceSymbol(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *ownerTypeNode,
        SZrAstNode *propertyNode,
        SZrSymbol **outSymbol);

SZrString *ZrLanguageServer_LspPropertyContract_FormatSignature(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        const SZrSymbol *symbol);

SZrString *ZrLanguageServer_LspPropertyContract_FormatQuery(
        SZrState *state,
        SZrString *name,
        const TZrChar *typeText,
        const SZrParserSemanticPropertyQuery *query);

ZR_LANGUAGE_SERVER_API SZrSymbol *ZrLanguageServer_LspPropertyContract_FindSourceSymbolAt(
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange position);

#endif // ZR_VM_LANGUAGE_SERVER_LSP_PROPERTY_CONTRACT_H
