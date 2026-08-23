#include "zr_vm_parser/semantic_query.h"

#include <string.h>

TZrBool ZrParser_SemanticQuery_SymbolAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticSymbolQuery *outSymbol) {
    SZrParserSemanticQueryFacts facts;
    const SZrSemanticReferenceFact *reference;
    const SZrSemanticReferenceFact *definition;

    if (outSymbol != ZR_NULL) {
        memset(outSymbol, 0, sizeof(*outSymbol));
    }
    if (context == ZR_NULL || outSymbol == ZR_NULL ||
        !ZrParser_SemanticQuery_FactsAt(context, position, scope, &facts)) {
        return ZR_FALSE;
    }

    reference = facts.reference;
    if (reference == ZR_NULL || !reference->isResolved ||
        reference->symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    outSymbol->symbolId = reference->symbolId;
    outSymbol->typeId = reference->typeId;
    outSymbol->ownerSymbolId = ZR_SEMANTIC_ID_INVALID;
    outSymbol->role = reference->kind;
    outSymbol->declarationRange = reference->declarationRange;
    outSymbol->displayName = reference->name;
    outSymbol->signatureDisplay = reference->signatureDisplay;
    if (reference->hasDefinitionRange) {
        outSymbol->definitionRange = reference->definitionRange;
        return ZR_TRUE;
    }

    definition = ZrParser_SemanticQuery_DefinitionOf(context, position, scope);
    if (definition != ZR_NULL) {
        outSymbol->definitionRange = definition->range;
    }
    return ZR_TRUE;
}
