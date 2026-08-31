#include "semantic/lsp_semantic_token_canonical.h"

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic_facts.h"

static TZrBool semantic_token_ranges_equal(SZrFileRange left,
                                           SZrFileRange right) {
    TZrBool sourcesEqual = left.source == right.source;

    if (!sourcesEqual && left.source != ZR_NULL && right.source != ZR_NULL) {
        sourcesEqual = ZrCore_String_Equal(left.source, right.source);
    }
    return sourcesEqual &&
           left.start.offset == right.start.offset &&
           left.end.offset == right.end.offset;
}

TZrInt32 ZrLanguageServer_LspSemanticToken_TypeFromCanonicalSymbol(
        const SZrParserSemanticSymbolQuery *symbol) {
    if (symbol == ZR_NULL || symbol->symbolId == ZR_SEMANTIC_ID_INVALID ||
        symbol->role == ZR_SEMANTIC_REFERENCE_UNKNOWN) {
        return ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN;
    }

    if (symbol->declarationNode != ZR_NULL) {
        switch (symbol->declarationNode->type) {
            case ZR_AST_CLASS_DECLARATION:
                return ZR_LSP_SEMANTIC_TOKEN_CLASS;
            case ZR_AST_STRUCT_DECLARATION:
                return ZR_LSP_SEMANTIC_TOKEN_STRUCT;
            case ZR_AST_INTERFACE_DECLARATION:
                return ZR_LSP_SEMANTIC_TOKEN_INTERFACE;
            case ZR_AST_ENUM_DECLARATION:
                return ZR_LSP_SEMANTIC_TOKEN_ENUM;
            case ZR_AST_FUNCTION_DECLARATION:
            case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            case ZR_AST_EXTERN_DELEGATE_DECLARATION:
                return ZR_LSP_SEMANTIC_TOKEN_FUNCTION;
            case ZR_AST_STRUCT_METHOD:
            case ZR_AST_CLASS_METHOD:
            case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            case ZR_AST_STRUCT_META_FUNCTION:
            case ZR_AST_CLASS_META_FUNCTION:
            case ZR_AST_INTERFACE_META_SIGNATURE:
                return ZR_LSP_SEMANTIC_TOKEN_METHOD;
            case ZR_AST_STRUCT_FIELD:
            case ZR_AST_CLASS_FIELD:
            case ZR_AST_INTERFACE_FIELD_DECLARATION:
            case ZR_AST_CLASS_PROPERTY:
            case ZR_AST_INTERFACE_PROPERTY_SIGNATURE:
                return ZR_LSP_SEMANTIC_TOKEN_PROPERTY;
            default:
                break;
        }
    }

    switch (symbol->kind) {
        case ZR_SEMANTIC_SYMBOL_KIND_TYPE:
            return ZR_LSP_SEMANTIC_TOKEN_CLASS;
        case ZR_SEMANTIC_SYMBOL_KIND_FUNCTION:
            return ZR_LSP_SEMANTIC_TOKEN_FUNCTION;
        case ZR_SEMANTIC_SYMBOL_KIND_PARAMETER:
            return ZR_LSP_SEMANTIC_TOKEN_PARAMETER;
        case ZR_SEMANTIC_SYMBOL_KIND_FIELD:
        case ZR_SEMANTIC_SYMBOL_KIND_PROPERTY:
            return ZR_LSP_SEMANTIC_TOKEN_PROPERTY;
        case ZR_SEMANTIC_SYMBOL_KIND_VARIABLE:
            return ZR_LSP_SEMANTIC_TOKEN_VARIABLE;
        default:
            return ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN;
    }
}

TZrInt32 ZrLanguageServer_LspSemanticToken_ResolveCanonical(
        SZrSemanticAnalyzer *analyzer,
        SZrString *uri,
        TZrSize startOffset,
        TZrSize length,
        TZrUInt32 line,
        TZrUInt32 character,
        TZrUInt32 *outModifiers) {
    SZrParserSemanticSymbolQuery symbol;
    SZrParserSemanticTypeQuery typeQuery;
    const SZrCanonicalTypeNode *type;
    SZrFilePosition start;
    SZrFilePosition end;
    SZrFileRange range;

    if (outModifiers != ZR_NULL) {
        *outModifiers = 0U;
    }
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        uri == ZR_NULL || length == 0U) {
        return ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN;
    }

    start = ZrParser_FilePosition_Create(startOffset, line, character);
    end = ZrParser_FilePosition_Create(
            startOffset + length, line, character + (TZrUInt32)length);
    range = ZrParser_FileRange_Create(start, end, uri);
    if (ZrParser_SemanticQuery_SymbolAt(
                analyzer->semanticContext, range, ZR_NULL, &symbol)) {
        if (outModifiers != ZR_NULL &&
            symbol.role == ZR_SEMANTIC_REFERENCE_DECLARATION) {
            *outModifiers = ZR_LSP_SEMANTIC_TOKEN_MODIFIER_DECLARATION;
        }
        return ZrLanguageServer_LspSemanticToken_TypeFromCanonicalSymbol(&symbol);
    }

    if (!ZrParser_SemanticQuery_CanonicalTypeAt(
                analyzer->semanticContext, range, ZR_NULL, &typeQuery) ||
        typeQuery.typeId == ZR_SEMANTIC_ID_INVALID ||
        typeQuery.reference == ZR_NULL ||
        typeQuery.reference->kind != ZR_SEMANTIC_REFERENCE_TYPE ||
        !typeQuery.reference->isResolved ||
        typeQuery.reference->typeId != typeQuery.typeId ||
        !semantic_token_ranges_equal(typeQuery.reference->range, range)) {
        return ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN;
    }

    type = ZrParser_CanonicalType_Find(
            analyzer->semanticContext, typeQuery.typeId);
    return type != ZR_NULL && type->kind == ZR_CANONICAL_TYPE_OWNER
                   ? ZR_LSP_SEMANTIC_TOKEN_CLASS
                   : ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN;
}
