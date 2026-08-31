#include "zr_vm_parser/semantic_calls.h"

#include <string.h>

#include "semantic/semantic_scope_facts.h"
#include "zr_vm_parser/semantic_query.h"

static TZrBool semantic_calls_same_source(SZrString *left, SZrString *right) {
    return (TZrBool)(left == right ||
                     (left != ZR_NULL && right != ZR_NULL &&
                      ZrCore_String_Equal(left, right)));
}

static TZrBool semantic_calls_same_range(
        const SZrFileRange *left,
        const SZrFileRange *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        !semantic_calls_same_source(left->source, right->source)) {
        return ZR_FALSE;
    }
    if (left->start.offset > 0U || left->end.offset > 0U ||
        right->start.offset > 0U || right->end.offset > 0U) {
        return (TZrBool)(left->start.offset == right->start.offset &&
                         left->end.offset == right->end.offset);
    }
    return (TZrBool)(left->start.line == right->start.line &&
                     left->start.column == right->start.column &&
                     left->end.line == right->end.line &&
                     left->end.column == right->end.column);
}

static TZrBool semantic_calls_range_contains(
        const SZrFileRange *outer,
        const SZrFileRange *inner) {
    if (outer == ZR_NULL || inner == ZR_NULL ||
        !semantic_calls_same_source(outer->source, inner->source)) {
        return ZR_FALSE;
    }
    if ((outer->start.offset > 0U || outer->end.offset > 0U) &&
        (inner->start.offset > 0U || inner->end.offset > 0U)) {
        return (TZrBool)(outer->start.offset <= inner->start.offset &&
                         inner->end.offset <= outer->end.offset);
    }
    return (TZrBool)(
            (outer->start.line < inner->start.line ||
             (outer->start.line == inner->start.line &&
              outer->start.column <= inner->start.column)) &&
            (inner->end.line < outer->end.line ||
             (inner->end.line == outer->end.line &&
              inner->end.column <= outer->end.column)));
}

static TZrBool semantic_calls_range_is_known(const SZrFileRange *range) {
    return (TZrBool)(range != ZR_NULL &&
                      (range->source != ZR_NULL || range->start.offset > 0U ||
                       range->end.offset > 0U || range->start.line > 0 ||
                       range->end.line > 0 || range->start.column > 0 ||
                       range->end.column > 0));
}

static TZrSize semantic_calls_range_width(const SZrFileRange *range) {
    if (range == ZR_NULL || range->end.offset < range->start.offset) {
        return ZR_MAX_SIZE;
    }
    return range->end.offset - range->start.offset;
}

static TZrSymbolId semantic_calls_find_caller(
        const SZrSemanticContext *context,
        const SZrFileRange *callSiteRange) {
    const SZrSemanticScopeFact *bestScope = ZR_NULL;
    TZrSize bestWidth = ZR_MAX_SIZE;
    TZrBool isAmbiguous = ZR_FALSE;
    TZrSize index;

    if (context == ZR_NULL || callSiteRange == ZR_NULL ||
        !context->scopeFacts.isValid) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    for (index = 0U; index < context->scopeFacts.length; index++) {
        const SZrSemanticScopeFact *scope =
                (const SZrSemanticScopeFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->scopeFacts, index);
        TZrSize width;

        if (scope == ZR_NULL || scope->kind != ZR_SEMANTIC_SCOPE_KIND_FUNCTION ||
            !semantic_calls_range_contains(&scope->range, callSiteRange)) {
            continue;
        }
        width = semantic_calls_range_width(&scope->range);
        if (bestScope == ZR_NULL || width < bestWidth) {
            bestScope = scope;
            bestWidth = width;
            isAmbiguous = ZR_FALSE;
        } else if (width == bestWidth &&
                   bestScope->ownerSymbolId != scope->ownerSymbolId) {
            isAmbiguous = ZR_TRUE;
        }
    }
    if (!isAmbiguous && bestScope != ZR_NULL &&
        bestScope->ownerSymbolId != ZR_SEMANTIC_ID_INVALID) {
        const SZrSemanticSymbolRecord *owner = ZrParser_Semantic_FindSymbolById(
                context, bestScope->ownerSymbolId);
        if (owner != ZR_NULL && owner->kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION) {
            return owner->id;
        }
    }
    return ZR_SEMANTIC_ID_INVALID;
}

static const SZrSemanticExpressionFact *semantic_calls_find_expression(
        const SZrSemanticContext *context,
        const SZrSemanticReferenceFact *reference) {
    const SZrSemanticExpressionFact *best = ZR_NULL;
    TZrSize bestWidth = ZR_MAX_SIZE;
    TZrSize index;

    if (context == ZR_NULL || reference == ZR_NULL ||
        !context->expressionFacts.isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->expressionFacts.length; index++) {
        const SZrSemanticExpressionFact *expression =
                (const SZrSemanticExpressionFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->expressionFacts, index);
        TZrSize width;

        if (expression == ZR_NULL || !expression->hasCallInfo ||
            !semantic_calls_range_contains(&expression->callTargetRange, &reference->range)) {
            continue;
        }
        width = semantic_calls_range_width(&expression->range);
        if (best == ZR_NULL || width < bestWidth) {
            best = expression;
            bestWidth = width;
        }
    }
    return best;
}

static const SZrSemanticSymbolRecord *semantic_calls_canonical_function(
        const SZrSemanticContext *context,
        const SZrSemanticSymbolRecord *symbol) {
    TZrSize index;

    if (context == ZR_NULL || symbol == ZR_NULL || symbol->astNode == ZR_NULL ||
        !context->symbols.isValid) {
        return symbol;
    }
    for (index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *candidate =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);

        if (candidate != ZR_NULL &&
            candidate->kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION &&
            candidate->astNode == symbol->astNode &&
            candidate->id != ZR_SEMANTIC_ID_INVALID &&
            candidate->typeId != ZR_SEMANTIC_ID_INVALID) {
            return candidate;
        }
    }
    return symbol;
}

static TZrUInt32 semantic_calls_edge_completeness(
        const SZrSemanticCallEdgeFact *edge) {
    TZrUInt32 score = 0U;

    if (edge == ZR_NULL) {
        return 0U;
    }
    if (edge->callerSymbolId != ZR_SEMANTIC_ID_INVALID) {
        score += 8U;
    }
    if (edge->targetSymbolId != ZR_SEMANTIC_ID_INVALID) {
        score += 4U;
    }
    if (edge->hasTargetDeclarationRange) {
        score += 2U;
    }
    if (edge->callableTypeId != ZR_SEMANTIC_ID_INVALID) {
        score += 1U;
    }
    if (edge->resolution == ZR_SEMANTIC_CALL_EDGE_RESOLUTION_RESOLVED) {
        score += 16U;
    }
    return score;
}

static TZrBool semantic_calls_merge_edge(
        SZrSemanticContext *context,
        const SZrSemanticCallEdgeFact *candidate) {
    TZrSize index;

    if (context == ZR_NULL || candidate == ZR_NULL || !context->callEdgeFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->callEdgeFacts.length; index++) {
        SZrSemanticCallEdgeFact *edge =
                (SZrSemanticCallEdgeFact *)ZrCore_Array_Get(
                        &context->callEdgeFacts, index);
        if (edge == ZR_NULL) {
            continue;
        }
        if (edge->callerSymbolId == candidate->callerSymbolId &&
            semantic_calls_same_range(
                    &edge->callSiteRange, &candidate->callSiteRange)) {
            if (edge->resolution == ZR_SEMANTIC_CALL_EDGE_RESOLUTION_RESOLVED &&
                candidate->resolution == ZR_SEMANTIC_CALL_EDGE_RESOLUTION_RESOLVED &&
                edge->targetSymbolId != candidate->targetSymbolId) {
                continue;
            }
            if (semantic_calls_edge_completeness(candidate) >=
                semantic_calls_edge_completeness(edge)) {
                *edge = *candidate;
            }
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

void ZrParser_SemanticCalls_Init(SZrSemanticContext *context) {
    if (context != ZR_NULL && context->state != ZR_NULL) {
        ZrCore_Array_Init(context->state,
                          &context->callEdgeFacts,
                          sizeof(SZrSemanticCallEdgeFact),
                          ZR_PARSER_INITIAL_CAPACITY_SMALL);
    }
}

void ZrParser_SemanticCalls_Reset(SZrSemanticContext *context) {
    if (context != ZR_NULL && context->callEdgeFacts.isValid) {
        context->callEdgeFacts.length = 0U;
    }
}

void ZrParser_SemanticCalls_Free(SZrSemanticContext *context) {
    if (context != ZR_NULL && context->state != ZR_NULL) {
        ZrParser_SemanticCalls_Reset(context);
        ZrCore_Array_Free(context->state, &context->callEdgeFacts);
    }
}

TZrBool ZrParser_SemanticCalls_Publish(SZrSemanticContext *context) {
    TZrSize index;

    if (context == ZR_NULL || !context->referenceFacts.isValid ||
        !context->callEdgeFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *reference =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        &context->referenceFacts, index);
        const SZrSemanticExpressionFact *expression;
        const SZrSemanticSymbolRecord *target = ZR_NULL;
        SZrSemanticCallEdgeFact edge;

        if (reference == ZR_NULL || reference->kind != ZR_SEMANTIC_REFERENCE_CALL ||
            !semantic_calls_range_is_known(&reference->range)) {
            continue;
        }
        memset(&edge, 0, sizeof(edge));
        expression = semantic_calls_find_expression(context, reference);
        if (expression != ZR_NULL &&
            !ZrParser_SemanticQuery_ExactnessAllowsProjection(
                    expression->exactness)) {
            continue;
        }
        edge.callSiteRange = expression != ZR_NULL ? expression->range : reference->range;
        edge.callableTypeId = reference->typeId;
        edge.callerSymbolId = semantic_calls_find_caller(context, &edge.callSiteRange);
        edge.targetSymbolId = reference->symbolId;
        edge.resolution = ZR_SEMANTIC_CALL_EDGE_RESOLUTION_RESOLVED;

        if (!reference->isResolved || edge.targetSymbolId == ZR_SEMANTIC_ID_INVALID) {
            edge.targetSymbolId = ZR_SEMANTIC_ID_INVALID;
            edge.resolution = ZR_SEMANTIC_CALL_EDGE_RESOLUTION_TARGET_UNRESOLVED;
        } else {
            target = ZrParser_Semantic_FindSymbolById(context, edge.targetSymbolId);
            if (target == ZR_NULL || target->kind != ZR_SEMANTIC_SYMBOL_KIND_FUNCTION) {
                edge.targetSymbolId = ZR_SEMANTIC_ID_INVALID;
                edge.resolution = ZR_SEMANTIC_CALL_EDGE_RESOLUTION_TARGET_UNRESOLVED;
            } else {
                target = semantic_calls_canonical_function(context, target);
                edge.targetSymbolId = target->id;
                edge.targetDeclarationRange = semantic_calls_range_is_known(
                        &reference->declarationRange)
                        ? reference->declarationRange
                        : target->location;
                edge.hasTargetDeclarationRange = semantic_calls_range_is_known(
                        &edge.targetDeclarationRange);
                if (!edge.hasTargetDeclarationRange) {
                    edge.resolution =
                            ZR_SEMANTIC_CALL_EDGE_RESOLUTION_TARGET_DECLARATION_UNAVAILABLE;
                }
            }
        }
        if (edge.callerSymbolId == ZR_SEMANTIC_ID_INVALID) {
            edge.resolution = ZR_SEMANTIC_CALL_EDGE_RESOLUTION_CALLER_UNAVAILABLE;
        }
        if (!semantic_calls_merge_edge(context, &edge)) {
            ZrCore_Array_Push(context->state, &context->callEdgeFacts, &edge);
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticCalls_PublishSource(
        SZrSemanticContext *context,
        SZrAstNode *root) {
    if (context == ZR_NULL || root == ZR_NULL ||
        !ZrParser_Semantic_BuildSourceScopeFacts(context, root)) {
        return ZR_FALSE;
    }
    return ZrParser_SemanticCalls_Publish(context);
}

static TZrBool semantic_calls_scope_allows(
        const SZrParserSemanticQueryScope *scope,
        const SZrSemanticCallEdgeFact *edge) {
    if (scope == ZR_NULL || scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE) {
        return ZR_TRUE;
    }
    return (TZrBool)(scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE &&
                     scope->root != ZR_NULL && edge != ZR_NULL &&
                     semantic_calls_range_contains(
                             &scope->root->location, &edge->callSiteRange));
}

static TZrBool semantic_calls_prepare_output(
        const SZrSemanticContext *context,
        SZrArray *outEdges) {
    if (context == ZR_NULL || outEdges == ZR_NULL) {
        return ZR_FALSE;
    }
    if (outEdges->isValid) {
        if (outEdges->elementSize != sizeof(SZrParserSemanticCallEdgeQuery)) {
            return ZR_FALSE;
        }
        outEdges->length = 0U;
        return ZR_TRUE;
    }
    ZrCore_Array_Init(context->state,
                      outEdges,
                      sizeof(SZrParserSemanticCallEdgeQuery),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
    return ZR_TRUE;
}

static TZrInt32 semantic_calls_compare_sources(
        const SZrString *left,
        const SZrString *right) {
    if (left == right) {
        return 0;
    }
    if (left == ZR_NULL) {
        return -1;
    }
    if (right == ZR_NULL) {
        return 1;
    }
    return strcmp(ZrCore_String_GetNativeString(left),
                  ZrCore_String_GetNativeString(right));
}

static TZrInt32 semantic_calls_compare_ranges(
        const SZrFileRange *left,
        const SZrFileRange *right) {
    TZrInt32 sourceOrder = semantic_calls_compare_sources(
            left->source, right->source);

    if (sourceOrder != 0) {
        return sourceOrder;
    }
    if (left->start.offset > 0U || left->end.offset > 0U ||
        right->start.offset > 0U || right->end.offset > 0U) {
        if (left->start.offset != right->start.offset) {
            return left->start.offset < right->start.offset ? -1 : 1;
        }
        if (left->end.offset != right->end.offset) {
            return left->end.offset < right->end.offset ? -1 : 1;
        }
        return 0;
    }
    if (left->start.line != right->start.line) {
        return left->start.line < right->start.line ? -1 : 1;
    }
    if (left->start.column != right->start.column) {
        return left->start.column < right->start.column ? -1 : 1;
    }
    if (left->end.line != right->end.line) {
        return left->end.line < right->end.line ? -1 : 1;
    }
    if (left->end.column != right->end.column) {
        return left->end.column < right->end.column ? -1 : 1;
    }
    return 0;
}

static TZrBool semantic_calls_query_precedes(
        const SZrParserSemanticCallEdgeQuery *left,
        const SZrParserSemanticCallEdgeQuery *right) {
    TZrInt32 rangeOrder = semantic_calls_compare_ranges(
            &left->callSiteRange, &right->callSiteRange);

    if (rangeOrder != 0) {
        return rangeOrder < 0;
    }
    if (left->callerSymbolId != right->callerSymbolId) {
        return left->callerSymbolId < right->callerSymbolId;
    }
    if (left->targetSymbolId != right->targetSymbolId) {
        return left->targetSymbolId < right->targetSymbolId;
    }
    return left->resolution <= right->resolution;
}

static void semantic_calls_sort(SZrArray *edges) {
    TZrSize index;

    if (edges == ZR_NULL) {
        return;
    }
    for (index = 1U; index < edges->length; index++) {
        TZrSize current = index;
        while (current > 0U) {
            SZrParserSemanticCallEdgeQuery *before =
                    (SZrParserSemanticCallEdgeQuery *)ZrCore_Array_Get(edges, current - 1U);
            SZrParserSemanticCallEdgeQuery *after =
                    (SZrParserSemanticCallEdgeQuery *)ZrCore_Array_Get(edges, current);
            SZrParserSemanticCallEdgeQuery swap;

            if (before == ZR_NULL || after == ZR_NULL ||
                semantic_calls_query_precedes(before, after)) {
                break;
            }
            swap = *before;
            *before = *after;
            *after = swap;
            current--;
        }
    }
}

static void semantic_calls_append_query(
        const SZrSemanticContext *context,
        SZrArray *outEdges,
        const SZrSemanticCallEdgeFact *edge) {
    SZrParserSemanticCallEdgeQuery query;

    if (context == ZR_NULL || outEdges == ZR_NULL || edge == ZR_NULL) {
        return;
    }
    memset(&query, 0, sizeof(query));
    query.callerSymbolId = edge->callerSymbolId;
    query.targetSymbolId = edge->targetSymbolId;
    query.callableTypeId = edge->callableTypeId;
    query.callSiteRange = edge->callSiteRange;
    query.targetDeclarationRange = edge->targetDeclarationRange;
    query.resolution = edge->resolution;
    query.hasTargetDeclarationRange = edge->hasTargetDeclarationRange;
    ZrCore_Array_Push(context->state, outEdges, &query);
}

TZrBool ZrParser_SemanticQuery_CallEdgesAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outEdges) {
    TZrSize index;

    if (!semantic_calls_prepare_output(context, outEdges) ||
        (scope != ZR_NULL && scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE &&
         (scope->root == ZR_NULL ||
          !semantic_calls_range_contains(&scope->root->location, &position)))) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->callEdgeFacts.length; index++) {
        const SZrSemanticCallEdgeFact *edge =
                (const SZrSemanticCallEdgeFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->callEdgeFacts, index);
        if (edge != ZR_NULL && semantic_calls_range_contains(&edge->callSiteRange, &position) &&
            semantic_calls_scope_allows(scope, edge)) {
            semantic_calls_append_query(context, outEdges, edge);
        }
    }
    semantic_calls_sort(outEdges);
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticQuery_OutgoingCalls(
        const SZrSemanticContext *context,
        TZrSymbolId callerSymbolId,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outEdges) {
    TZrSize index;

    if (!semantic_calls_prepare_output(context, outEdges) ||
        callerSymbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->callEdgeFacts.length; index++) {
        const SZrSemanticCallEdgeFact *edge =
                (const SZrSemanticCallEdgeFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->callEdgeFacts, index);
        if (edge != ZR_NULL && edge->callerSymbolId == callerSymbolId &&
            semantic_calls_scope_allows(scope, edge)) {
            semantic_calls_append_query(context, outEdges, edge);
        }
    }
    semantic_calls_sort(outEdges);
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticQuery_IncomingCalls(
        const SZrSemanticContext *context,
        TZrSymbolId targetSymbolId,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outEdges) {
    TZrSize index;

    if (!semantic_calls_prepare_output(context, outEdges) ||
        targetSymbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->callEdgeFacts.length; index++) {
        const SZrSemanticCallEdgeFact *edge =
                (const SZrSemanticCallEdgeFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->callEdgeFacts, index);
        if (edge != ZR_NULL && edge->targetSymbolId == targetSymbolId &&
            semantic_calls_scope_allows(scope, edge)) {
            semantic_calls_append_query(context, outEdges, edge);
        }
    }
    semantic_calls_sort(outEdges);
    return ZR_TRUE;
}

static TZrBool semantic_calls_prepare_candidates(
        const SZrSemanticContext *context,
        SZrArray *outCandidates) {
    if (context == ZR_NULL || outCandidates == ZR_NULL) {
        return ZR_FALSE;
    }
    if (outCandidates->isValid) {
        if (outCandidates->elementSize != sizeof(SZrParserSemanticCallCandidateQuery)) {
            return ZR_FALSE;
        }
        outCandidates->length = 0U;
        return ZR_TRUE;
    }
    ZrCore_Array_Init(context->state,
                      outCandidates,
                      sizeof(SZrParserSemanticCallCandidateQuery),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
    return ZR_TRUE;
}

static TZrBool semantic_calls_has_candidate(
        const SZrArray *candidates,
        TZrSymbolId symbolId) {
    TZrSize index;

    if (candidates == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0U; index < candidates->length; index++) {
        const SZrParserSemanticCallCandidateQuery *candidate =
                (const SZrParserSemanticCallCandidateQuery *)ZrCore_Array_Get(
                        (SZrArray *)candidates, index);
        if (candidate != ZR_NULL && candidate->symbolId == symbolId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool semantic_calls_append_candidate(
        const SZrSemanticContext *context,
        SZrArray *outCandidates,
        const SZrSemanticSymbolRecord *symbol,
        TZrSymbolId selectedSymbolId) {
    SZrParserSemanticCallCandidateQuery candidate;

    if (context == ZR_NULL || outCandidates == ZR_NULL || symbol == ZR_NULL ||
        symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_FUNCTION ||
        symbol->id == ZR_SEMANTIC_ID_INVALID ||
        symbol->typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    if (semantic_calls_has_candidate(outCandidates, symbol->id)) {
        return ZR_TRUE;
    }
    memset(&candidate, 0, sizeof(candidate));
    candidate.symbolId = symbol->id;
    candidate.callableTypeId = symbol->typeId;
    candidate.declarationRange = symbol->location;
    candidate.isSelected = symbol->id == selectedSymbolId;
    ZrCore_Array_Push(context->state, outCandidates, &candidate);
    return ZR_TRUE;
}

static void semantic_calls_sort_candidates(SZrArray *candidates) {
    TZrSize index;

    if (candidates == ZR_NULL) {
        return;
    }
    for (index = 1U; index < candidates->length; index++) {
        TZrSize current = index;
        while (current > 0U) {
            SZrParserSemanticCallCandidateQuery *before =
                    (SZrParserSemanticCallCandidateQuery *)ZrCore_Array_Get(
                            candidates, current - 1U);
            SZrParserSemanticCallCandidateQuery *after =
                    (SZrParserSemanticCallCandidateQuery *)ZrCore_Array_Get(
                            candidates, current);
            SZrParserSemanticCallCandidateQuery swap;

            if (before == ZR_NULL || after == ZR_NULL ||
                before->symbolId <= after->symbolId) {
                break;
            }
            swap = *before;
            *before = *after;
            *after = swap;
            current--;
        }
    }
}

TZrBool ZrParser_SemanticQuery_CallCandidatesAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outCandidates) {
    SZrParserSemanticCallQuery call;
    const SZrSemanticSymbolRecord *selected;
    const SZrSemanticOverloadSetRecord *overloads = ZR_NULL;
    TZrSize index;

    if (!semantic_calls_prepare_candidates(context, outCandidates)) {
        return ZR_FALSE;
    }
    memset(&call, 0, sizeof(call));
    if (!ZrParser_SemanticQuery_CallAt(context, position, scope, &call) ||
        call.expression == ZR_NULL ||
        !ZrParser_SemanticQuery_ExactnessAllowsProjection(
                call.expression->exactness) ||
        !call.hasResolvedTarget ||
        call.targetSymbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    selected = ZrParser_Semantic_FindSymbolById(context, call.targetSymbolId);
    if (selected == ZR_NULL || selected->kind != ZR_SEMANTIC_SYMBOL_KIND_FUNCTION ||
        selected->typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    if (selected->overloadSetId != ZR_SEMANTIC_ID_INVALID) {
        for (index = 0U; index < context->overloadSets.length; index++) {
            const SZrSemanticOverloadSetRecord *candidateSet =
                    (const SZrSemanticOverloadSetRecord *)ZrCore_Array_Get(
                            (SZrArray *)&context->overloadSets, index);
            if (candidateSet != ZR_NULL && candidateSet->id == selected->overloadSetId) {
                overloads = candidateSet;
                break;
            }
        }
        if (overloads == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    if (overloads == ZR_NULL) {
        if (!semantic_calls_append_candidate(
                    context, outCandidates, selected, call.targetSymbolId)) {
            return ZR_FALSE;
        }
    } else {
        for (index = 0U; index < overloads->members.length; index++) {
            const TZrSymbolId *symbolId = (const TZrSymbolId *)ZrCore_Array_Get(
                    (SZrArray *)&overloads->members, index);
            if (symbolId == ZR_NULL ||
                !semantic_calls_append_candidate(
                        context,
                        outCandidates,
                        ZrParser_Semantic_FindSymbolById(context, *symbolId),
                        call.targetSymbolId)) {
                outCandidates->length = 0U;
                return ZR_FALSE;
            }
        }
    }
    if (!semantic_calls_has_candidate(outCandidates, call.targetSymbolId)) {
        outCandidates->length = 0U;
        return ZR_FALSE;
    }
    semantic_calls_sort_candidates(outCandidates);
    return outCandidates->length > 0U;
}
