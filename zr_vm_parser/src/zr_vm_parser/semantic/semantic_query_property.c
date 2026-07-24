#include "zr_vm_parser/semantic_query.h"

#include <string.h>

static TZrBool semantic_property_same_source(
        SZrString *left,
        SZrString *right) {
    return (TZrBool)(left == ZR_NULL || right == ZR_NULL || left == right ||
                     ZrCore_String_Equal(left, right));
}

static TZrBool semantic_property_range_contains(
        const SZrFileRange *range,
        const SZrFileRange *position) {
    if (range == ZR_NULL || position == ZR_NULL ||
        !semantic_property_same_source(range->source, position->source)) {
        return ZR_FALSE;
    }
    if ((range->start.offset > 0U || range->end.offset > 0U) &&
        (position->start.offset > 0U || position->end.offset > 0U)) {
        return (TZrBool)(range->start.offset <= position->start.offset &&
                         position->end.offset <= range->end.offset);
    }
    return (TZrBool)(
            (range->start.line < position->start.line ||
             (range->start.line == position->start.line &&
              range->start.column <= position->start.column)) &&
            (position->end.line < range->end.line ||
             (position->end.line == range->end.line &&
              position->end.column <= range->end.column)));
}

static TZrBool semantic_property_scope_allows(
        const SZrParserSemanticQueryScope *scope,
        const SZrFileRange *range) {
    return (TZrBool)(
            scope == ZR_NULL ||
            scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE ||
            (scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE &&
             scope->root != ZR_NULL &&
             semantic_property_range_contains(&scope->root->location, range)));
}

static TZrBool semantic_property_matches_symbol(
        const SZrSemanticPropertyContract *contract,
        TZrSymbolId symbolId) {
    return (TZrBool)(
            contract != ZR_NULL && symbolId != ZR_SEMANTIC_ID_INVALID &&
            (contract->propertySymbolId == symbolId ||
             contract->getterSymbolId == symbolId ||
             contract->setterSymbolId == symbolId ||
             contract->initializerSymbolId == symbolId ||
             contract->setterValueSymbolId == symbolId ||
             contract->initializerValueSymbolId == symbolId));
}

TZrBool ZrParser_SemanticQuery_PropertyBySymbolId(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        SZrParserSemanticPropertyQuery *outQuery) {
    if (outQuery != ZR_NULL) {
        memset(outQuery, 0, sizeof(*outQuery));
    }
    if (context == ZR_NULL || outQuery == ZR_NULL ||
        symbolId == ZR_SEMANTIC_ID_INVALID ||
        !context->propertyContracts.isValid) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < context->propertyContracts.length; index++) {
        const SZrSemanticPropertyContract *contract =
                (const SZrSemanticPropertyContract *)ZrCore_Array_Get(
                        (SZrArray *)&context->propertyContracts,
                        index);
        if (semantic_property_matches_symbol(contract, symbolId)) {
            *outQuery = *contract;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

TZrBool ZrParser_SemanticQuery_PropertyAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticPropertyQuery *outQuery) {
    const SZrSemanticReferenceFact *reference;

    if (outQuery != ZR_NULL) {
        memset(outQuery, 0, sizeof(*outQuery));
    }
    if (context == ZR_NULL || outQuery == ZR_NULL ||
        !semantic_property_scope_allows(scope, &position)) {
        return ZR_FALSE;
    }
    if (context->referenceFacts.isValid) {
        for (TZrSize index = 0U; index < context->referenceFacts.length; index++) {
            const SZrSemanticReferenceFact *candidate =
                    (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                            (SZrArray *)&context->referenceFacts,
                            index);
            if (candidate != ZR_NULL && candidate->isResolved &&
                semantic_property_range_contains(&candidate->range, &position) &&
                semantic_property_scope_allows(scope, &candidate->range) &&
                ZrParser_SemanticQuery_PropertyBySymbolId(
                        context,
                        candidate->symbolId,
                        outQuery)) {
                return ZR_TRUE;
            }
        }
    }
    reference = ZrParser_SemanticFacts_FindReferenceAtPosition(
            context,
            position);
    if (reference != ZR_NULL &&
        semantic_property_scope_allows(scope, &reference->range) &&
        ZrParser_SemanticQuery_PropertyBySymbolId(
                context,
                reference->symbolId,
                outQuery)) {
        return ZR_TRUE;
    }
    if (!context->propertyContracts.isValid) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < context->propertyContracts.length; index++) {
        const SZrSemanticPropertyContract *contract =
                (const SZrSemanticPropertyContract *)ZrCore_Array_Get(
                        (SZrArray *)&context->propertyContracts,
                        index);
        if (contract != ZR_NULL &&
            semantic_property_scope_allows(scope, &contract->declarationRange) &&
            (semantic_property_range_contains(
                     &contract->selectionRange,
                     &position) ||
             semantic_property_range_contains(
                     &contract->declarationRange,
                     &position))) {
            *outQuery = *contract;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}
