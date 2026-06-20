#include "zr_vm_parser/semantic_query.h"

#include <string.h>

static TZrBool semantic_query_has_offset(const SZrFilePosition *position) {
    return position != ZR_NULL && position->offset > 0;
}

static TZrBool semantic_query_same_source(SZrString *left, SZrString *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_TRUE;
    }
    if (left == right) {
        return ZR_TRUE;
    }
    return ZrCore_String_Equal(left, right);
}

static TZrBool semantic_query_range_contains_position(const SZrFileRange *range,
                                                      const SZrFileRange *position) {
    TZrSize queryOffset;
    TZrInt32 queryLine;
    TZrInt32 queryColumn;

    if (range == ZR_NULL || position == ZR_NULL ||
        !semantic_query_same_source(range->source, position->source)) {
        return ZR_FALSE;
    }

    if ((semantic_query_has_offset(&range->start) ||
         semantic_query_has_offset(&range->end)) &&
        (semantic_query_has_offset(&position->start) ||
         semantic_query_has_offset(&position->end))) {
        queryOffset = position->start.offset;
        return queryOffset >= range->start.offset && queryOffset <= range->end.offset;
    }

    queryLine = position->start.line;
    queryColumn = position->start.column;
    if (queryLine < range->start.line || queryLine > range->end.line) {
        return ZR_FALSE;
    }
    if (queryLine == range->start.line && queryColumn < range->start.column) {
        return ZR_FALSE;
    }
    if (queryLine == range->end.line && queryColumn > range->end.column) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool semantic_query_range_contains_range(const SZrFileRange *outer,
                                                   const SZrFileRange *inner) {
    SZrFileRange endPosition;

    if (outer == ZR_NULL || inner == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!semantic_query_range_contains_position(outer, inner)) {
        return ZR_FALSE;
    }

    endPosition = *inner;
    endPosition.start = inner->end;
    return semantic_query_range_contains_position(outer, &endPosition);
}

static TZrBool semantic_query_ranges_equal(const SZrFileRange *left,
                                           const SZrFileRange *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        !semantic_query_same_source(left->source, right->source)) {
        return ZR_FALSE;
    }

    if ((semantic_query_has_offset(&left->start) ||
         semantic_query_has_offset(&left->end) ||
         semantic_query_has_offset(&right->start) ||
         semantic_query_has_offset(&right->end))) {
        return left->start.offset == right->start.offset &&
               left->end.offset == right->end.offset;
    }

    return left->start.line == right->start.line &&
           left->start.column == right->start.column &&
           left->end.line == right->end.line &&
           left->end.column == right->end.column;
}

static TZrBool semantic_query_scope_allows_range(
        const SZrParserSemanticQueryScope *scope,
        const SZrFileRange *range) {
    if (scope == ZR_NULL || scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE) {
        return ZR_TRUE;
    }
    if (scope->kind != ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE || scope->root == ZR_NULL) {
        return ZR_FALSE;
    }
    return semantic_query_range_contains_range(&scope->root->location, range);
}

static TZrBool semantic_query_scope_allows_position(
        const SZrParserSemanticQueryScope *scope,
        const SZrFileRange *position) {
    if (scope == ZR_NULL || scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE) {
        return ZR_TRUE;
    }
    if (scope->kind != ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE || scope->root == ZR_NULL) {
        return ZR_FALSE;
    }
    return semantic_query_range_contains_position(&scope->root->location, position);
}

static const SZrSemanticNumericFact *semantic_query_find_numeric_at_position(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope) {
    TZrSize i;

    if (context == ZR_NULL || !context->numericFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->numericFacts.length; i++) {
        const SZrSemanticNumericFact *fact =
            (const SZrSemanticNumericFact *)ZrCore_Array_Get((SZrArray *)&context->numericFacts, i);
        if (fact != ZR_NULL &&
            semantic_query_range_contains_position(&fact->range, &position) &&
            semantic_query_scope_allows_range(scope, &fact->range)) {
            return fact;
        }
    }
    return ZR_NULL;
}

static const SZrSemanticReferenceFact *semantic_query_find_declaration_for_reference(
        const SZrSemanticContext *context,
        const SZrSemanticReferenceFact *reference,
        const SZrParserSemanticQueryScope *scope) {
    TZrSize i;

    if (context == ZR_NULL || reference == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_NULL;
    }
    if (reference->kind == ZR_SEMANTIC_REFERENCE_DECLARATION) {
        return reference;
    }
    if (!reference->isResolved) {
        return ZR_NULL;
    }

    for (i = 0; i < context->referenceFacts.length; i++) {
        const SZrSemanticReferenceFact *fact =
            (const SZrSemanticReferenceFact *)ZrCore_Array_Get((SZrArray *)&context->referenceFacts, i);
        if (fact == ZR_NULL ||
            fact->kind != ZR_SEMANTIC_REFERENCE_DECLARATION ||
            !semantic_query_scope_allows_range(scope, &fact->range)) {
            continue;
        }
        if (reference->symbolId != ZR_SEMANTIC_ID_INVALID &&
            fact->symbolId == reference->symbolId) {
            return fact;
        }
        if (semantic_query_ranges_equal(&fact->range, &reference->declarationRange)) {
            return fact;
        }
    }

    return ZR_NULL;
}

void ZrParser_SemanticQueryScope_Module(SZrParserSemanticQueryScope *scope) {
    if (scope == ZR_NULL) {
        return;
    }

    scope->kind = ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE;
    scope->root = ZR_NULL;
}

void ZrParser_SemanticQueryScope_Node(SZrParserSemanticQueryScope *scope,
                                      const SZrAstNode *root) {
    if (scope == ZR_NULL) {
        return;
    }

    scope->kind = ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE;
    scope->root = root;
}

TZrBool ZrParser_SemanticQuery_TypeAt(const SZrSemanticContext *context,
                                      SZrFileRange position,
                                      const SZrParserSemanticQueryScope *scope,
                                      SZrInferredType *outType) {
    const SZrSemanticExpressionFact *fact;

    if (context == ZR_NULL || outType == ZR_NULL ||
        !semantic_query_scope_allows_position(scope, &position)) {
        return ZR_FALSE;
    }

    fact = ZrParser_SemanticFacts_FindExpressionAtPosition(context, position);
    if (fact == ZR_NULL || !semantic_query_scope_allows_range(scope, &fact->range)) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Copy(context->state, outType, &fact->inferredType);
    return ZR_TRUE;
}

const SZrSemanticReferenceFact *ZrParser_SemanticQuery_DefinitionOf(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope) {
    const SZrSemanticReferenceFact *reference;

    if (context == ZR_NULL || !semantic_query_scope_allows_position(scope, &position)) {
        return ZR_NULL;
    }

    reference = ZrParser_SemanticFacts_FindReferenceAtPosition(context, position);
    if (reference == ZR_NULL || !semantic_query_scope_allows_range(scope, &reference->range)) {
        return ZR_NULL;
    }

    return semantic_query_find_declaration_for_reference(context, reference, scope);
}

TZrBool ZrParser_SemanticQuery_ReferencesOf(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outReferences) {
    TZrSize i;

    if (context == ZR_NULL ||
        symbolId == ZR_SEMANTIC_ID_INVALID ||
        outReferences == ZR_NULL ||
        !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }

    if (!outReferences->isValid) {
        ZrCore_Array_Init(context->state,
                          outReferences,
                          sizeof(const SZrSemanticReferenceFact *),
                          ZR_PARSER_INITIAL_CAPACITY_SMALL);
    } else {
        outReferences->length = 0;
    }

    for (i = 0; i < context->referenceFacts.length; i++) {
        const SZrSemanticReferenceFact *fact =
            (const SZrSemanticReferenceFact *)ZrCore_Array_Get((SZrArray *)&context->referenceFacts, i);
        if (fact != ZR_NULL &&
            fact->symbolId == symbolId &&
            semantic_query_scope_allows_range(scope, &fact->range)) {
            ZrCore_Array_Push(context->state, outReferences, &fact);
        }
    }

    return outReferences->length > 0;
}

TZrBool ZrParser_SemanticQuery_FactsAt(const SZrSemanticContext *context,
                                       SZrFileRange position,
                                       const SZrParserSemanticQueryScope *scope,
                                       SZrParserSemanticQueryFacts *outFacts) {
    if (outFacts == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outFacts, 0, sizeof(*outFacts));
    if (context == ZR_NULL || !semantic_query_scope_allows_position(scope, &position)) {
        return ZR_FALSE;
    }

    outFacts->expression = ZrParser_SemanticFacts_FindExpressionAtPosition(context, position);
    if (outFacts->expression != ZR_NULL &&
        !semantic_query_scope_allows_range(scope, &outFacts->expression->range)) {
        outFacts->expression = ZR_NULL;
    }

    outFacts->reference = ZrParser_SemanticFacts_FindReferenceAtPosition(context, position);
    if (outFacts->reference != ZR_NULL &&
        !semantic_query_scope_allows_range(scope, &outFacts->reference->range)) {
        outFacts->reference = ZR_NULL;
    }

    outFacts->numeric = semantic_query_find_numeric_at_position(context, position, scope);

    outFacts->reachability = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, position);
    if (outFacts->reachability != ZR_NULL &&
        !semantic_query_scope_allows_range(scope, &outFacts->reachability->range)) {
        outFacts->reachability = ZR_NULL;
    }

    outFacts->logical = ZrParser_SemanticFacts_FindLogicalAtPosition(context, position);
    if (outFacts->logical != ZR_NULL &&
        !semantic_query_scope_allows_range(scope, &outFacts->logical->range)) {
        outFacts->logical = ZR_NULL;
    }

    outFacts->ownership = ZrParser_SemanticFacts_FindOwnershipAtPosition(context, position);
    if (outFacts->ownership != ZR_NULL &&
        !semantic_query_scope_allows_range(scope, &outFacts->ownership->range)) {
        outFacts->ownership = ZR_NULL;
    }

    return outFacts->expression != ZR_NULL ||
           outFacts->reference != ZR_NULL ||
           outFacts->numeric != ZR_NULL ||
           outFacts->reachability != ZR_NULL ||
           outFacts->logical != ZR_NULL ||
           outFacts->ownership != ZR_NULL;
}

TZrBool ZrParser_SemanticQuery_Diagnostics(
        const SZrSemanticContext *context,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticQueryDiagnostics *outDiagnostics) {
    ZR_UNUSED_PARAMETER(context);
    ZR_UNUSED_PARAMETER(scope);

    if (outDiagnostics == ZR_NULL) {
        return ZR_FALSE;
    }

    outDiagnostics->items = ZR_NULL;
    outDiagnostics->count = 0;
    return ZR_TRUE;
}
