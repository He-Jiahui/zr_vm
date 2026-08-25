#include "zr_vm_parser/semantic_query.h"

#include <string.h>

typedef struct SZrSemanticVisibleSymbolCandidate {
    SZrParserSemanticSymbolQuery symbol;
    const SZrSemanticSymbolRecord *record;
    TZrUInt32 scopeDistance;
    TZrUInt32 declarationOrder;
    TZrOverloadSetId overloadSetId;
} SZrSemanticVisibleSymbolCandidate;

static TZrBool semantic_query_symbols_same_source(
        SZrString *left,
        SZrString *right) {
    return left == ZR_NULL || right == ZR_NULL || ZrCore_String_Equal(left, right);
}

static TZrBool semantic_query_symbols_range_contains(
        const SZrFileRange *range,
        const SZrFileRange *position) {
    if (range == ZR_NULL || position == ZR_NULL ||
        !semantic_query_symbols_same_source(range->source, position->source)) {
        return ZR_FALSE;
    }
    return range->start.offset <= position->start.offset &&
           range->end.offset >= position->start.offset;
}

static TZrSize semantic_query_symbols_range_width(const SZrFileRange *range) {
    if (range == ZR_NULL || range->end.offset < range->start.offset) {
        return (TZrSize)-1;
    }
    return range->end.offset - range->start.offset;
}

static TZrBool semantic_query_symbols_scope_allows_position(
        const SZrParserSemanticQueryScope *scope,
        SZrFileRange position) {
    if (scope == ZR_NULL || scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE) {
        return ZR_TRUE;
    }
    return scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE && scope->root != ZR_NULL &&
           semantic_query_symbols_range_contains(&scope->root->location, &position);
}

static TZrBool semantic_query_symbols_scope_descends_from(
        const SZrSemanticContext *context,
        const SZrSemanticScopeFact *candidate,
        const SZrSemanticScopeFact *ancestor) {
    const SZrSemanticScopeFact *current = candidate;
    TZrSize depth = 0U;

    if (context == ZR_NULL || candidate == ZR_NULL || ancestor == ZR_NULL) {
        return ZR_FALSE;
    }
    while (current != ZR_NULL && depth < context->scopeFacts.length) {
        if (current->parentScopeId == ancestor->id) {
            return ZR_TRUE;
        }
        current = ZrParser_Semantic_FindScopeFactById(context, current->parentScopeId);
        depth++;
    }
    return ZR_FALSE;
}

static const SZrSemanticScopeFact *semantic_query_symbols_find_innermost_scope(
        const SZrSemanticContext *context,
        SZrFileRange position) {
    const SZrSemanticScopeFact *best = ZR_NULL;
    TZrSize bestWidth = 0U;
    TZrSize index;

    if (context == ZR_NULL || !context->scopeFacts.isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->scopeFacts.length; index++) {
        const SZrSemanticScopeFact *candidate =
                (const SZrSemanticScopeFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->scopeFacts, index);
        TZrSize width;

        if (candidate == ZR_NULL ||
            !semantic_query_symbols_range_contains(&candidate->range, &position)) {
            continue;
        }
        width = semantic_query_symbols_range_width(&candidate->range);
        if (best == ZR_NULL || width < bestWidth ||
            (width == bestWidth &&
             semantic_query_symbols_scope_descends_from(context, candidate, best))) {
            best = candidate;
            bestWidth = width;
        }
    }
    return best;
}

static TZrBool semantic_query_symbols_is_available(
        const SZrSemanticVisibleSymbolFact *fact,
        SZrFileRange position) {
    if (fact == ZR_NULL) {
        return ZR_FALSE;
    }
    if (fact->isHoisted) {
        return ZR_TRUE;
    }
    return semantic_query_symbols_same_source(
                   fact->declarationRange.source, position.source) &&
           fact->declarationRange.start.offset <= position.start.offset;
}

static TZrBool semantic_query_symbols_is_eligible(
        const SZrSemanticVisibleSymbolFact *fact,
        const SZrSemanticScopeFact *activeScope,
        SZrFileRange position,
        const SZrParserSemanticVisibleSymbolOptions *options) {
    if (fact == ZR_NULL || activeScope == ZR_NULL ||
        !semantic_query_symbols_is_available(fact, position)) {
        return ZR_FALSE;
    }
    if (fact->isReceiverMember &&
        (options == ZR_NULL || !options->includeReceiverMembers)) {
        return ZR_FALSE;
    }
    if ((fact->isImport || fact->isAlias) &&
        (options == ZR_NULL || !options->includeImports)) {
        return ZR_FALSE;
    }
    if (!fact->isAccessible &&
        (options == ZR_NULL || !options->includeInaccessible)) {
        return ZR_FALSE;
    }
    return !(activeScope->isStaticContext && fact->isReceiverMember && !fact->isStatic);
}

static TZrBool semantic_query_symbols_same_namespace(
        const SZrSemanticSymbolRecord *left,
        const SZrSemanticSymbolRecord *right) {
    TZrBool leftIsType;
    TZrBool rightIsType;

    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }
    leftIsType = left->kind == ZR_SEMANTIC_SYMBOL_KIND_TYPE;
    rightIsType = right->kind == ZR_SEMANTIC_SYMBOL_KIND_TYPE;
    return leftIsType == rightIsType;
}

static TZrBool semantic_query_symbols_is_shadowed(
        const SZrArray *candidates,
        const SZrSemanticVisibleSymbolCandidate *candidate) {
    TZrSize index;

    if (candidates == ZR_NULL || candidate == ZR_NULL || candidate->record == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0U; index < candidates->length; index++) {
        const SZrSemanticVisibleSymbolCandidate *existing =
                (const SZrSemanticVisibleSymbolCandidate *)ZrCore_Array_Get(
                        (SZrArray *)candidates, index);
        if (existing == ZR_NULL || existing->record == ZR_NULL ||
            !semantic_query_symbols_same_namespace(existing->record, candidate->record) ||
            existing->record->name == ZR_NULL || candidate->record->name == ZR_NULL ||
            !ZrCore_String_Equal(existing->record->name, candidate->record->name)) {
            continue;
        }
        if (existing->overloadSetId != ZR_SEMANTIC_ID_INVALID &&
            existing->overloadSetId == candidate->overloadSetId) {
            continue;
        }
        if (existing->scopeDistance <= candidate->scopeDistance) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool semantic_query_symbols_candidate_precedes(
        const SZrSemanticVisibleSymbolCandidate *left,
        const SZrSemanticVisibleSymbolCandidate *right) {
    if (left->scopeDistance != right->scopeDistance) {
        return left->scopeDistance < right->scopeDistance;
    }
    if (left->declarationOrder != right->declarationOrder) {
        return left->declarationOrder < right->declarationOrder;
    }
    return left->symbol.symbolId < right->symbol.symbolId;
}

static void semantic_query_symbols_sort(SZrArray *candidates) {
    TZrSize index;

    if (candidates == ZR_NULL) {
        return;
    }
    for (index = 1U; index < candidates->length; index++) {
        TZrSize current = index;
        while (current > 0U) {
            SZrSemanticVisibleSymbolCandidate *before =
                    (SZrSemanticVisibleSymbolCandidate *)ZrCore_Array_Get(
                            candidates, current - 1U);
            SZrSemanticVisibleSymbolCandidate *after =
                    (SZrSemanticVisibleSymbolCandidate *)ZrCore_Array_Get(candidates, current);
            SZrSemanticVisibleSymbolCandidate swap;

            if (before == ZR_NULL || after == ZR_NULL ||
                semantic_query_symbols_candidate_precedes(before, after)) {
                break;
            }
            swap = *before;
            *before = *after;
            *after = swap;
            current--;
        }
    }
}

static TZrBool semantic_query_symbols_prepare_output(
        const SZrSemanticContext *context,
        SZrArray *outSymbols) {
    if (context == ZR_NULL || outSymbols == ZR_NULL) {
        return ZR_FALSE;
    }
    if (outSymbols->isValid) {
        if (outSymbols->elementSize != sizeof(SZrParserSemanticSymbolQuery)) {
            return ZR_FALSE;
        }
        outSymbols->length = 0U;
        return ZR_TRUE;
    }
    ZrCore_Array_Init(context->state,
                      outSymbols,
                      sizeof(SZrParserSemanticSymbolQuery),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
    return ZR_TRUE;
}

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

TZrBool ZrParser_SemanticQuery_VisibleSymbols(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        const SZrParserSemanticVisibleSymbolOptions *options,
        SZrArray *outSymbols) {
    const SZrSemanticScopeFact *activeScope;
    const SZrSemanticScopeFact *currentScope;
    SZrArray candidates;
    TZrUInt32 distance;
    TZrSize index;

    if (!semantic_query_symbols_prepare_output(context, outSymbols) ||
        !semantic_query_symbols_scope_allows_position(scope, position)) {
        return ZR_FALSE;
    }
    activeScope = semantic_query_symbols_find_innermost_scope(context, position);
    if (activeScope == ZR_NULL || !context->visibleSymbolFacts.isValid) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&candidates);
    ZrCore_Array_Init(context->state,
                      &candidates,
                      sizeof(SZrSemanticVisibleSymbolCandidate),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
    currentScope = activeScope;
    distance = 0U;
    while (currentScope != ZR_NULL && distance < context->scopeFacts.length) {
        for (index = 0U; index < context->visibleSymbolFacts.length; index++) {
            const SZrSemanticVisibleSymbolFact *fact =
                    (const SZrSemanticVisibleSymbolFact *)ZrCore_Array_Get(
                            (SZrArray *)&context->visibleSymbolFacts, index);
            const SZrSemanticSymbolRecord *record;
            SZrSemanticVisibleSymbolCandidate candidate;

            if (fact == ZR_NULL || fact->scopeId != currentScope->id ||
                !semantic_query_symbols_is_eligible(fact, activeScope, position, options)) {
                continue;
            }
            record = ZrParser_Semantic_FindSymbolById(context, fact->symbolId);
            if (record == ZR_NULL || record->name == ZR_NULL) {
                continue;
            }

            memset(&candidate, 0, sizeof(candidate));
            candidate.symbol.symbolId = record->id;
            candidate.symbol.typeId = record->typeId;
            candidate.symbol.ownerSymbolId = fact->ownerSymbolId;
            candidate.symbol.role = ZR_SEMANTIC_REFERENCE_DECLARATION;
            candidate.symbol.declarationRange = fact->declarationRange;
            candidate.symbol.definitionRange = fact->hasDefinitionRange
                    ? fact->definitionRange
                    : fact->declarationRange;
            candidate.symbol.displayName = record->name;
            candidate.symbol.signatureDisplay = fact->signatureDisplay;
            candidate.record = record;
            candidate.scopeDistance = distance;
            candidate.declarationOrder = fact->declarationOrder;
            candidate.overloadSetId = record->overloadSetId;
            if (!semantic_query_symbols_is_shadowed(&candidates, &candidate)) {
                ZrCore_Array_Push(context->state, &candidates, &candidate);
            }
        }
        currentScope = ZrParser_Semantic_FindScopeFactById(
                context, currentScope->parentScopeId);
        distance++;
    }

    semantic_query_symbols_sort(&candidates);
    for (index = 0U; index < candidates.length; index++) {
        const SZrSemanticVisibleSymbolCandidate *candidate =
                (const SZrSemanticVisibleSymbolCandidate *)ZrCore_Array_Get(&candidates, index);
        if (candidate != ZR_NULL) {
            SZrParserSemanticSymbolQuery symbol = candidate->symbol;
            ZrCore_Array_Push(context->state, outSymbols, &symbol);
        }
    }
    ZrCore_Array_Free(context->state, &candidates);
    return outSymbols->length > 0U;
}
