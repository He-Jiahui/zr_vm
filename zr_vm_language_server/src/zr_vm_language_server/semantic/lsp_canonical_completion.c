#include "semantic/lsp_canonical_completion.h"

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic_query.h"

#include <string.h>

static const TZrChar *canonical_completion_kind_text(
        EZrSemanticSymbolKind kind) {
    switch (kind) {
        case ZR_SEMANTIC_SYMBOL_KIND_FUNCTION:
            return "function";
        case ZR_SEMANTIC_SYMBOL_KIND_TYPE:
            return "class";
        case ZR_SEMANTIC_SYMBOL_KIND_FIELD:
            return "field";
        case ZR_SEMANTIC_SYMBOL_KIND_PROPERTY:
            return "property";
        case ZR_SEMANTIC_SYMBOL_KIND_PARAMETER:
        case ZR_SEMANTIC_SYMBOL_KIND_VARIABLE:
        case ZR_SEMANTIC_SYMBOL_KIND_UNKNOWN:
        default:
            return "variable";
    }
}

static TZrBool canonical_completion_symbol_is_exact(
        const SZrParserSemanticSymbolQuery *symbol) {
    return symbol != ZR_NULL &&
           symbol->symbolId != ZR_SEMANTIC_ID_INVALID &&
           symbol->role == ZR_SEMANTIC_REFERENCE_DECLARATION &&
           symbol->declarationNode != ZR_NULL &&
           symbol->displayName != ZR_NULL;
}

static const TZrChar *canonical_completion_detail(
        const SZrSemanticContext *semanticContext,
        const SZrParserSemanticSymbolQuery *symbol,
        TZrChar *typeBuffer,
        TZrSize typeBufferSize) {
    if (symbol->signatureDisplay != ZR_NULL &&
        ZrCore_String_GetByteLength(symbol->signatureDisplay) > 0U) {
        return ZrCore_String_GetNativeString(symbol->signatureDisplay);
    }
    if (ZrParser_CanonicalType_Format(
                semanticContext,
                symbol->typeId,
                typeBuffer,
                typeBufferSize)) {
        return typeBuffer;
    }
    return "cannot infer exact type";
}

TZrBool ZrLanguageServer_LspCanonicalCompletion_AppendVisibleSymbols(
        SZrState *state,
        const SZrSemanticContext *semanticContext,
        SZrFileRange position,
        SZrArray *result) {
    SZrParserSemanticVisibleSymbolOptions options;
    SZrArray symbols;
    TZrSize initialLength;

    if (state == ZR_NULL || semanticContext == ZR_NULL || result == ZR_NULL ||
        !result->isValid ||
        result->elementSize != sizeof(SZrCompletionItem *)) {
        return ZR_FALSE;
    }

    memset(&options, 0, sizeof(options));
    ZrCore_Array_Construct(&symbols);
    if (!ZrParser_SemanticQuery_VisibleSymbols(
                semanticContext,
                position,
                ZR_NULL,
                &options,
                &symbols)) {
        if (symbols.isValid) {
            ZrCore_Array_Free(state, &symbols);
        }
        return ZR_FALSE;
    }

    initialLength = result->length;
    for (TZrSize index = 0U; index < symbols.length; index++) {
        const SZrParserSemanticSymbolQuery *symbol =
                (const SZrParserSemanticSymbolQuery *)ZrCore_Array_Get(
                        &symbols, index);
        TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
        const TZrChar *label;
        const TZrChar *detail;
        SZrCompletionItem *item;

        if (!canonical_completion_symbol_is_exact(symbol)) {
            continue;
        }
        label = ZrCore_String_GetNativeString(symbol->displayName);
        detail = canonical_completion_detail(
                semanticContext, symbol, typeBuffer, sizeof(typeBuffer));
        if (detail == ZR_NULL) {
            continue;
        }
        item = ZrLanguageServer_CompletionItem_New(
                state,
                label,
                canonical_completion_kind_text(symbol->kind),
                detail,
                ZR_NULL,
                ZR_NULL);
        if (item != ZR_NULL) {
            ZrCore_Array_Push(state, result, &item);
        }
    }
    ZrCore_Array_Free(state, &symbols);
    return result->length > initialLength;
}
