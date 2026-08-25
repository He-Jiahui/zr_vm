#include "zr_vm_parser/semantic_query.h"

#include <string.h>

static SZrString *semantic_relations_clone_string(
        SZrSemanticContext *context,
        SZrString *value) {
    TZrNativeString text;

    if (context == ZR_NULL || context->state == ZR_NULL || value == ZR_NULL) {
        return ZR_NULL;
    }
    text = ZrCore_String_GetNativeString(value);
    if (text == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrCore_String_Create(
            context->state, text, ZrCore_String_GetByteLength(value));
}

static TZrBool semantic_relations_range_contains(
        const SZrFileRange *outer,
        const SZrFileRange *inner) {
    if (outer == ZR_NULL || inner == ZR_NULL ||
        (outer->source != ZR_NULL && inner->source != ZR_NULL &&
         outer->source != inner->source &&
         !ZrCore_String_Equal(outer->source, inner->source))) {
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

static TZrBool semantic_relations_scope_allows(
        const SZrParserSemanticQueryScope *scope,
        const SZrSemanticRelationFact *fact) {
    if (scope == ZR_NULL || scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE) {
        return ZR_TRUE;
    }
    if (scope->kind != ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE ||
        scope->root == ZR_NULL || fact == ZR_NULL) {
        return ZR_FALSE;
    }
    return (TZrBool)((fact->hasSourceRange &&
                       semantic_relations_range_contains(
                               &scope->root->location, &fact->sourceRange)) ||
                      (fact->hasTargetRange &&
                       semantic_relations_range_contains(
                               &scope->root->location, &fact->targetRange)));
}

static TZrBool semantic_relations_prepare_output(
        const SZrSemanticContext *context,
        SZrArray *outRelations) {
    if (context == ZR_NULL || outRelations == ZR_NULL) {
        return ZR_FALSE;
    }
    if (outRelations->isValid) {
        if (outRelations->elementSize != sizeof(SZrParserSemanticRelationQuery)) {
            return ZR_FALSE;
        }
        outRelations->length = 0U;
        return ZR_TRUE;
    }
    ZrCore_Array_Init(context->state,
                      outRelations,
                      sizeof(SZrParserSemanticRelationQuery),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
    return ZR_TRUE;
}

static TZrBool semantic_relations_query_precedes(
        const SZrParserSemanticRelationQuery *left,
        const SZrParserSemanticRelationQuery *right) {
    if (left->kind != right->kind) {
        return left->kind < right->kind;
    }
    if (left->sourceSymbolId != right->sourceSymbolId) {
        return left->sourceSymbolId < right->sourceSymbolId;
    }
    if (left->targetSymbolId != right->targetSymbolId) {
        return left->targetSymbolId < right->targetSymbolId;
    }
    if (left->sourceTypeId != right->sourceTypeId) {
        return left->sourceTypeId < right->sourceTypeId;
    }
    if (left->targetTypeId != right->targetTypeId) {
        return left->targetTypeId < right->targetTypeId;
    }
    if (left->sourceRange.start.offset != right->sourceRange.start.offset) {
        return left->sourceRange.start.offset < right->sourceRange.start.offset;
    }
    return left->targetRange.start.offset <= right->targetRange.start.offset;
}

static void semantic_relations_sort(SZrArray *relations) {
    TZrSize index;

    if (relations == ZR_NULL) {
        return;
    }
    for (index = 1U; index < relations->length; index++) {
        TZrSize current = index;
        while (current > 0U) {
            SZrParserSemanticRelationQuery *before =
                    (SZrParserSemanticRelationQuery *)ZrCore_Array_Get(
                            relations, current - 1U);
            SZrParserSemanticRelationQuery *after =
                    (SZrParserSemanticRelationQuery *)ZrCore_Array_Get(
                            relations, current);
            SZrParserSemanticRelationQuery swap;

            if (before == ZR_NULL || after == ZR_NULL ||
                semantic_relations_query_precedes(before, after)) {
                break;
            }
            swap = *before;
            *before = *after;
            *after = swap;
            current--;
        }
    }
}

static void semantic_relations_append_query(
        const SZrSemanticContext *context,
        SZrArray *outRelations,
        const SZrSemanticRelationFact *fact) {
    SZrParserSemanticRelationQuery query;

    if (context == ZR_NULL || outRelations == ZR_NULL || fact == ZR_NULL) {
        return;
    }
    memset(&query, 0, sizeof(query));
    query.kind = fact->kind;
    query.sourceSymbolId = fact->sourceSymbolId;
    query.targetSymbolId = fact->targetSymbolId;
    query.sourceTypeId = fact->sourceTypeId;
    query.targetTypeId = fact->targetTypeId;
    query.sourceRange = fact->sourceRange;
    query.targetRange = fact->targetRange;
    query.externalOriginUri = fact->externalOriginUri;
    query.hasSourceRange = fact->hasSourceRange;
    query.hasTargetRange = fact->hasTargetRange;
    query.isExternal = fact->isExternal;
    ZrCore_Array_Push(context->state, outRelations, &query);
}

void ZrParser_SemanticRelations_Init(SZrSemanticContext *context) {
    if (context == ZR_NULL || context->state == ZR_NULL) {
        return;
    }
    ZrCore_Array_Init(context->state,
                      &context->relationFacts,
                      sizeof(SZrSemanticRelationFact),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
}

void ZrParser_SemanticRelations_Reset(SZrSemanticContext *context) {
    if (context != ZR_NULL && context->relationFacts.isValid) {
        context->relationFacts.length = 0U;
    }
}

void ZrParser_SemanticRelations_Free(SZrSemanticContext *context) {
    if (context == ZR_NULL || context->state == ZR_NULL) {
        return;
    }
    ZrParser_SemanticRelations_Reset(context);
    ZrCore_Array_Free(context->state, &context->relationFacts);
}

TZrBool ZrParser_SemanticRelations_Append(
        SZrSemanticContext *context,
        const SZrSemanticRelationFact *fact) {
    SZrSemanticRelationFact copy;

    if (context == ZR_NULL || fact == ZR_NULL || !context->relationFacts.isValid ||
        fact->kind == ZR_SEMANTIC_RELATION_UNKNOWN ||
        (fact->sourceSymbolId == ZR_SEMANTIC_ID_INVALID &&
         fact->targetSymbolId == ZR_SEMANTIC_ID_INVALID &&
         fact->sourceTypeId == ZR_SEMANTIC_ID_INVALID &&
         fact->targetTypeId == ZR_SEMANTIC_ID_INVALID)) {
        return ZR_FALSE;
    }
    copy = *fact;
    copy.externalOriginUri = semantic_relations_clone_string(context, fact->externalOriginUri);
    if (fact->externalOriginUri != ZR_NULL && copy.externalOriginUri == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Array_Push(context->state, &context->relationFacts, &copy);
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticQuery_RelationsOfSymbol(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outRelations) {
    TZrSize index;

    if (context == ZR_NULL || symbolId == ZR_SEMANTIC_ID_INVALID ||
        !context->relationFacts.isValid ||
        !semantic_relations_prepare_output(context, outRelations)) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->relationFacts.length; index++) {
        const SZrSemanticRelationFact *fact =
                (const SZrSemanticRelationFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->relationFacts, index);
        if (fact != ZR_NULL &&
            (fact->sourceSymbolId == symbolId || fact->targetSymbolId == symbolId) &&
            semantic_relations_scope_allows(scope, fact)) {
            semantic_relations_append_query(context, outRelations, fact);
        }
    }
    semantic_relations_sort(outRelations);
    return outRelations->length > 0U;
}

TZrBool ZrParser_SemanticQuery_ImplementationsOf(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outRelations) {
    TZrSize index;

    if (context == ZR_NULL || symbolId == ZR_SEMANTIC_ID_INVALID ||
        !context->relationFacts.isValid ||
        !semantic_relations_prepare_output(context, outRelations)) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->relationFacts.length; index++) {
        const SZrSemanticRelationFact *fact =
                (const SZrSemanticRelationFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->relationFacts, index);
        if (fact != ZR_NULL && fact->kind == ZR_SEMANTIC_RELATION_IMPLEMENTATION &&
            fact->targetSymbolId == symbolId &&
            semantic_relations_scope_allows(scope, fact)) {
            semantic_relations_append_query(context, outRelations, fact);
        }
    }
    semantic_relations_sort(outRelations);
    return outRelations->length > 0U;
}

TZrBool ZrParser_SemanticQuery_BaseTypesOf(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        SZrArray *outRelations) {
    TZrSize index;

    if (context == ZR_NULL || typeId == ZR_SEMANTIC_ID_INVALID ||
        !context->relationFacts.isValid ||
        !semantic_relations_prepare_output(context, outRelations)) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->relationFacts.length; index++) {
        const SZrSemanticRelationFact *fact =
                (const SZrSemanticRelationFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->relationFacts, index);
        if (fact != ZR_NULL && fact->kind == ZR_SEMANTIC_RELATION_BASE_TYPE &&
            fact->sourceTypeId == typeId) {
            semantic_relations_append_query(context, outRelations, fact);
        }
    }
    semantic_relations_sort(outRelations);
    return outRelations->length > 0U;
}

TZrBool ZrParser_SemanticQuery_DerivedTypesOf(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        SZrArray *outRelations) {
    TZrSize index;

    if (context == ZR_NULL || typeId == ZR_SEMANTIC_ID_INVALID ||
        !context->relationFacts.isValid ||
        !semantic_relations_prepare_output(context, outRelations)) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->relationFacts.length; index++) {
        const SZrSemanticRelationFact *fact =
                (const SZrSemanticRelationFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->relationFacts, index);
        if (fact != ZR_NULL && fact->kind == ZR_SEMANTIC_RELATION_BASE_TYPE &&
            fact->targetTypeId == typeId) {
            semantic_relations_append_query(context, outRelations, fact);
        }
    }
    semantic_relations_sort(outRelations);
    return outRelations->length > 0U;
}
