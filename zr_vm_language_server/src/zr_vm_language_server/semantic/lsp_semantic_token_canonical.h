#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_TOKEN_CANONICAL_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_TOKEN_CANONICAL_H

#include "zr_vm_language_server/semantic_analyzer.h"
#include "zr_vm_parser/semantic_query.h"

#define ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN ((TZrInt32)-1)
#define ZR_LSP_SEMANTIC_TOKEN_MODIFIER_DECLARATION ((TZrUInt32)1U)

typedef enum EZrLspSemanticTokenType {
    ZR_LSP_SEMANTIC_TOKEN_NAMESPACE = 0,
    ZR_LSP_SEMANTIC_TOKEN_CLASS = 1,
    ZR_LSP_SEMANTIC_TOKEN_STRUCT = 2,
    ZR_LSP_SEMANTIC_TOKEN_INTERFACE = 3,
    ZR_LSP_SEMANTIC_TOKEN_ENUM = 4,
    ZR_LSP_SEMANTIC_TOKEN_FUNCTION = 5,
    ZR_LSP_SEMANTIC_TOKEN_METHOD = 6,
    ZR_LSP_SEMANTIC_TOKEN_PROPERTY = 7,
    ZR_LSP_SEMANTIC_TOKEN_VARIABLE = 8,
    ZR_LSP_SEMANTIC_TOKEN_PARAMETER = 9,
    ZR_LSP_SEMANTIC_TOKEN_KEYWORD = 10,
    ZR_LSP_SEMANTIC_TOKEN_DECORATOR = 11,
    ZR_LSP_SEMANTIC_TOKEN_META_METHOD = 12
} EZrLspSemanticTokenType;

TZrInt32 ZrLanguageServer_LspSemanticToken_TypeFromCanonicalSymbol(
        const SZrParserSemanticSymbolQuery *symbol);

TZrInt32 ZrLanguageServer_LspSemanticToken_ResolveCanonical(
        SZrSemanticAnalyzer *analyzer,
        SZrString *uri,
        TZrSize startOffset,
        TZrSize length,
        TZrUInt32 line,
        TZrUInt32 character,
        TZrUInt32 *outModifiers);

#endif
