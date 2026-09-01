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
    return (TZrBool)(left == right ||
                     (left != ZR_NULL && right != ZR_NULL &&
                      ZrCore_String_Equal(left, right)));
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
        if (best == ZR_NULL ||
            semantic_query_symbols_scope_descends_from(context, candidate, best) ||
            (!semantic_query_symbols_scope_descends_from(context, best, candidate) &&
             width < bestWidth)) {
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

static TZrBool semantic_query_symbols_scope_allows_declaration(
        const SZrParserSemanticQueryScope *scope,
        const SZrSemanticReferenceFact *declaration) {
    if (declaration == ZR_NULL) {
        return ZR_FALSE;
    }
    if (scope == ZR_NULL || scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE) {
        return ZR_TRUE;
    }
    return scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE &&
           scope->root != ZR_NULL &&
           semantic_query_symbols_range_contains(
                   &scope->root->location, &declaration->range);
}

static TZrBool semantic_query_symbols_contains_id(
        const SZrArray *symbols,
        TZrSymbolId symbolId) {
    TZrSize index;

    if (symbols == ZR_NULL || symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    for (index = 0U; index < symbols->length; index++) {
        const SZrParserSemanticSymbolQuery *symbol =
                (const SZrParserSemanticSymbolQuery *)ZrCore_Array_Get(
                        (SZrArray *)symbols, index);
        if (symbol != ZR_NULL && symbol->symbolId == symbolId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool semantic_query_symbols_declaration_precedes(
        const SZrParserSemanticSymbolQuery *left,
        const SZrParserSemanticSymbolQuery *right) {
    if (left->declarationRange.start.offset != right->declarationRange.start.offset) {
        return left->declarationRange.start.offset < right->declarationRange.start.offset;
    }
    if (left->declarationRange.end.offset != right->declarationRange.end.offset) {
        return left->declarationRange.end.offset < right->declarationRange.end.offset;
    }
    return left->symbolId < right->symbolId;
}

static void semantic_query_symbols_sort_declarations(SZrArray *symbols) {
    TZrSize index;

    if (symbols == ZR_NULL) {
        return;
    }
    for (index = 1U; index < symbols->length; index++) {
        TZrSize current = index;
        while (current > 0U) {
            SZrParserSemanticSymbolQuery *before =
                    (SZrParserSemanticSymbolQuery *)ZrCore_Array_Get(
                            symbols, current - 1U);
            SZrParserSemanticSymbolQuery *after =
                    (SZrParserSemanticSymbolQuery *)ZrCore_Array_Get(
                            symbols, current);
            SZrParserSemanticSymbolQuery swap;

            if (before == ZR_NULL || after == ZR_NULL ||
                semantic_query_symbols_declaration_precedes(before, after)) {
                break;
            }
            swap = *before;
            *before = *after;
            *after = swap;
            current--;
        }
    }
}

static TZrBool semantic_query_external_reference_is_complete(
        const SZrSemanticReferenceFact *reference) {
    return reference != ZR_NULL && reference->isResolved &&
           reference->symbolId != ZR_SEMANTIC_ID_INVALID &&
           reference->hasExternalTarget &&
           reference->externalTargetKind != ZR_SEMANTIC_EXTERNAL_TARGET_UNKNOWN &&
           reference->externalOwnerIdentity != ZR_NULL &&
           ZrCore_String_GetByteLength(reference->externalOwnerIdentity) > 0U &&
           reference->externalMetadataToken != 0U &&
           reference->externalSignatureToken != 0U &&
           reference->externalSignatureHash != 0U;
}

static TZrBool semantic_query_external_reference_precedes(
        const SZrParserSemanticExternalReferenceQuery *left,
        const SZrParserSemanticExternalReferenceQuery *right) {
    const TZrChar *leftSource;
    const TZrChar *rightSource;
    TZrInt32 sourceOrder;

    if (left == ZR_NULL || right == ZR_NULL) {
        return left != ZR_NULL;
    }
    leftSource = left->referenceRange.source != ZR_NULL
            ? ZrCore_String_GetNativeString(left->referenceRange.source)
            : ZR_NULL;
    rightSource = right->referenceRange.source != ZR_NULL
            ? ZrCore_String_GetNativeString(right->referenceRange.source)
            : ZR_NULL;
    if (leftSource != ZR_NULL || rightSource != ZR_NULL) {
        if (leftSource == ZR_NULL || rightSource == ZR_NULL) {
            return leftSource != ZR_NULL;
        }
        sourceOrder = strcmp(leftSource, rightSource);
        if (sourceOrder != 0) {
            return sourceOrder < 0;
        }
    }
    if (left->referenceRange.start.offset != right->referenceRange.start.offset) {
        return left->referenceRange.start.offset < right->referenceRange.start.offset;
    }
    if (left->referenceRange.end.offset != right->referenceRange.end.offset) {
        return left->referenceRange.end.offset < right->referenceRange.end.offset;
    }
    if (left->externalMetadataToken != right->externalMetadataToken) {
        return left->externalMetadataToken < right->externalMetadataToken;
    }
    return left->symbolId <= right->symbolId;
}

static void semantic_query_external_references_sort(SZrArray *references) {
    TZrSize index;

    if (references == ZR_NULL) {
        return;
    }
    for (index = 1U; index < references->length; index++) {
        TZrSize current = index;
        while (current > 0U) {
            SZrParserSemanticExternalReferenceQuery *before =
                    (SZrParserSemanticExternalReferenceQuery *)ZrCore_Array_Get(
                            references, current - 1U);
            SZrParserSemanticExternalReferenceQuery *after =
                    (SZrParserSemanticExternalReferenceQuery *)ZrCore_Array_Get(
                            references, current);
            SZrParserSemanticExternalReferenceQuery swap;

            if (before == ZR_NULL || after == ZR_NULL ||
                semantic_query_external_reference_precedes(before, after)) {
                break;
            }
            swap = *before;
            *before = *after;
            *after = swap;
            current--;
        }
    }
}

static TZrBool semantic_query_symbols_apply_import_identity(
        const SZrSemanticContext *context,
        SZrParserSemanticSymbolQuery *symbol) {
    TZrSize index;

    if (context == ZR_NULL || symbol == ZR_NULL ||
        symbol->symbolId == ZR_SEMANTIC_ID_INVALID ||
        !context->visibleSymbolFacts.isValid) {
        return ZR_TRUE;
    }
    for (index = 0U; index < context->visibleSymbolFacts.length; index++) {
        const SZrSemanticVisibleSymbolFact *fact =
                (const SZrSemanticVisibleSymbolFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->visibleSymbolFacts, index);

        if (fact == ZR_NULL || fact->symbolId != symbol->symbolId ||
            !fact->isImport) {
            continue;
        }
        if (symbol->isImport) {
            if ((symbol->externalOriginUri == ZR_NULL) !=
                    (fact->externalOriginUri == ZR_NULL) ||
                (symbol->externalOriginUri != ZR_NULL &&
                 !ZrCore_String_Equal(symbol->externalOriginUri,
                                      fact->externalOriginUri))) {
                return ZR_FALSE;
            }
        }
        symbol->isImport = ZR_TRUE;
        symbol->externalOriginUri = fact->externalOriginUri;
    }
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
    outSymbol->referenceRange = reference->range;
    outSymbol->declarationRange = reference->declarationRange;
    outSymbol->displayName = reference->name;
    outSymbol->signatureDisplay = reference->signatureDisplay;
    outSymbol->externalOwnerIdentity = reference->externalOwnerIdentity;
    outSymbol->externalProviderGeneration = reference->externalProviderGeneration;
    outSymbol->externalMetadataToken = reference->externalMetadataToken;
    outSymbol->externalSignatureToken = reference->externalSignatureToken;
    outSymbol->externalSignatureHash = reference->externalSignatureHash;
    outSymbol->externalTargetKind = reference->externalTargetKind;
    outSymbol->hasExternalTarget = reference->hasExternalTarget;
    if (!semantic_query_symbols_apply_import_identity(context, outSymbol)) {
        memset(outSymbol, 0, sizeof(*outSymbol));
        return ZR_FALSE;
    }
    {
        const SZrSemanticSymbolRecord *record =
                ZrParser_Semantic_FindSymbolById(context, reference->symbolId);
        if (record != ZR_NULL && record->id == reference->symbolId) {
            outSymbol->kind = record->kind;
            outSymbol->declarationNode = record->astNode;
        }
    }
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

TZrBool ZrParser_SemanticQuery_DeclaredSymbols(
        const SZrSemanticContext *context,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outSymbols) {
    TZrSize index;

    if (!semantic_query_symbols_prepare_output(context, outSymbols) ||
        !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }

    for (index = 0U; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *declaration =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts, index);
        const SZrSemanticSymbolRecord *record;
        SZrParserSemanticSymbolQuery symbol;

        if (declaration == ZR_NULL ||
            declaration->kind != ZR_SEMANTIC_REFERENCE_DECLARATION ||
            !declaration->isResolved ||
            declaration->symbolId == ZR_SEMANTIC_ID_INVALID ||
            declaration->typeId == ZR_SEMANTIC_ID_INVALID ||
            declaration->node == ZR_NULL ||
            !semantic_query_symbols_scope_allows_declaration(scope, declaration) ||
            semantic_query_symbols_contains_id(outSymbols, declaration->symbolId)) {
            continue;
        }

        record = ZrParser_Semantic_FindSymbolById(context, declaration->symbolId);
        if (record == ZR_NULL || record->id != declaration->symbolId ||
            record->typeId != declaration->typeId ||
            record->astNode != declaration->node) {
            continue;
        }

        memset(&symbol, 0, sizeof(symbol));
        symbol.symbolId = declaration->symbolId;
        symbol.typeId = declaration->typeId;
        symbol.ownerSymbolId = ZR_SEMANTIC_ID_INVALID;
        symbol.kind = record->kind;
        symbol.role = ZR_SEMANTIC_REFERENCE_DECLARATION;
        symbol.referenceRange = declaration->range;
        symbol.declarationRange = declaration->declarationRange;
        symbol.definitionRange = declaration->hasDefinitionRange
                ? declaration->definitionRange
                : declaration->declarationRange;
        symbol.declarationNode = declaration->node;
        symbol.displayName = record->name;
        symbol.signatureDisplay = declaration->signatureDisplay;
        if (!semantic_query_symbols_apply_import_identity(context, &symbol)) {
            continue;
        }
        ZrCore_Array_Push(context->state, outSymbols, &symbol);
    }

    semantic_query_symbols_sort_declarations(outSymbols);
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticQuery_ExternalReferences(
        const SZrSemanticContext *context,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outReferences) {
    TZrSize index;

    if (context == ZR_NULL || outReferences == ZR_NULL ||
        !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }
    if (outReferences->isValid) {
        if (outReferences->elementSize !=
            sizeof(SZrParserSemanticExternalReferenceQuery)) {
            return ZR_FALSE;
        }
        outReferences->length = 0U;
    } else {
        ZrCore_Array_Init(
                context->state,
                outReferences,
                sizeof(SZrParserSemanticExternalReferenceQuery),
                ZR_PARSER_INITIAL_CAPACITY_SMALL);
    }

    for (index = 0U; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *reference =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts, index);
        SZrParserSemanticExternalReferenceQuery projected;

        if (!semantic_query_external_reference_is_complete(reference) ||
            !semantic_query_symbols_scope_allows_declaration(scope, reference)) {
            continue;
        }
        memset(&projected, 0, sizeof(projected));
        projected.referenceRange = reference->range;
        projected.symbolId = reference->symbolId;
        projected.typeId = reference->typeId;
        projected.role = reference->kind;
        projected.externalOwnerIdentity = reference->externalOwnerIdentity;
        projected.externalProviderGeneration =
                reference->externalProviderGeneration;
        projected.externalMetadataToken = reference->externalMetadataToken;
        projected.externalSignatureToken = reference->externalSignatureToken;
        projected.externalSignatureHash = reference->externalSignatureHash;
        projected.externalTargetKind = reference->externalTargetKind;
        ZrCore_Array_Push(context->state, outReferences, &projected);
    }

    semantic_query_external_references_sort(outReferences);
    return outReferences->length > 0U;
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
            candidate.symbol.kind = record->kind;
            candidate.symbol.role = ZR_SEMANTIC_REFERENCE_DECLARATION;
            candidate.symbol.declarationRange = fact->declarationRange;
            candidate.symbol.definitionRange = fact->hasDefinitionRange
                    ? fact->definitionRange
                    : fact->declarationRange;
            candidate.symbol.displayName = record->name;
            candidate.symbol.signatureDisplay = fact->signatureDisplay;
            if (!semantic_query_symbols_apply_import_identity(
                        context, &candidate.symbol)) {
                continue;
            }
            candidate.symbol.declarationNode = record->astNode;
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
