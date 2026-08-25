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

static TZrBool semantic_relations_range_is_known(const SZrFileRange *range) {
    return (TZrBool)(range != ZR_NULL &&
                      (range->source != ZR_NULL ||
                       range->start.offset > 0U || range->end.offset > 0U ||
                       range->start.line > 0 || range->end.line > 0 ||
                       range->start.column > 0 || range->end.column > 0));
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

static TZrBool semantic_relations_has_property_accessor(
        const SZrSemanticContext *context,
        TZrSymbolId propertySymbolId,
        TZrSymbolId accessorSymbolId) {
    TZrSize index;

    if (context == ZR_NULL || !context->relationFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->relationFacts.length; index++) {
        const SZrSemanticRelationFact *fact =
                (const SZrSemanticRelationFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->relationFacts, index);
        if (fact != ZR_NULL &&
            fact->kind == ZR_SEMANTIC_RELATION_PROPERTY_ACCESSOR &&
            fact->sourceSymbolId == propertySymbolId &&
            fact->targetSymbolId == accessorSymbolId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool semantic_relations_property_accessor_is_valid(
        const SZrSemanticContext *context,
        TZrSymbolId accessorSymbolId,
        TZrTypeId callableTypeId) {
    const SZrSemanticSymbolRecord *accessor;

    if (accessorSymbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_TRUE;
    }
    accessor = ZrParser_Semantic_FindSymbolById(context, accessorSymbolId);
    return (TZrBool)(accessor != ZR_NULL &&
                      accessor->kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION &&
                      callableTypeId != ZR_SEMANTIC_ID_INVALID &&
                      accessor->typeId == callableTypeId);
}

static TZrBool semantic_relations_property_contract_is_valid(
        const SZrSemanticContext *context,
        const SZrSemanticPropertyContract *contract) {
    const SZrSemanticSymbolRecord *property;

    if (context == ZR_NULL || contract == ZR_NULL ||
        contract->propertySymbolId == ZR_SEMANTIC_ID_INVALID ||
        contract->propertyTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    property = ZrParser_Semantic_FindSymbolById(
            context, contract->propertySymbolId);
    return (TZrBool)(property != ZR_NULL &&
                      property->kind == ZR_SEMANTIC_SYMBOL_KIND_PROPERTY &&
                      property->typeId == contract->propertyTypeId &&
                      semantic_relations_property_accessor_is_valid(
                              context,
                              contract->getterSymbolId,
                              contract->getterCallableTypeId) &&
                      semantic_relations_property_accessor_is_valid(
                              context,
                              contract->setterSymbolId,
                              contract->setterCallableTypeId) &&
                      semantic_relations_property_accessor_is_valid(
                              context,
                              contract->initializerSymbolId,
                              contract->initializerCallableTypeId));
}

static TZrBool semantic_relations_ranges_equal(
        const SZrFileRange *left,
        const SZrFileRange *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        left->start.offset != right->start.offset ||
        left->end.offset != right->end.offset ||
        left->start.line != right->start.line ||
        left->start.column != right->start.column ||
        left->end.line != right->end.line ||
        left->end.column != right->end.column) {
        return ZR_FALSE;
    }
    return (TZrBool)(left->source == right->source ||
                      (left->source != ZR_NULL && right->source != ZR_NULL &&
                       ZrCore_String_Equal(left->source, right->source)));
}

static TZrBool semantic_relations_has_reference_definition(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        const SZrFileRange *definitionRange) {
    TZrSize index;

    if (context == ZR_NULL || definitionRange == ZR_NULL ||
        !context->relationFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->relationFacts.length; index++) {
        const SZrSemanticRelationFact *fact =
                (const SZrSemanticRelationFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->relationFacts, index);
        if (fact != ZR_NULL &&
            fact->kind == ZR_SEMANTIC_RELATION_DECLARATION_DEFINITION &&
            fact->sourceSymbolId == symbolId &&
            fact->targetSymbolId == symbolId &&
            fact->hasTargetRange &&
            semantic_relations_ranges_equal(&fact->targetRange, definitionRange)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool semantic_relations_publish_property_accessor(
        SZrSemanticContext *context,
        const SZrSemanticPropertyContract *contract,
        TZrSymbolId accessorSymbolId,
        TZrTypeId callableTypeId) {
    const SZrSemanticSymbolRecord *accessor;
    SZrSemanticRelationFact fact;

    if (accessorSymbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_TRUE;
    }
    accessor = ZrParser_Semantic_FindSymbolById(context, accessorSymbolId);
    if (!semantic_relations_property_accessor_is_valid(
                context, accessorSymbolId, callableTypeId) ||
        accessor == ZR_NULL) {
        return ZR_FALSE;
    }
    if (semantic_relations_has_property_accessor(
                context, contract->propertySymbolId, accessorSymbolId)) {
        return ZR_TRUE;
    }

    memset(&fact, 0, sizeof(fact));
    fact.kind = ZR_SEMANTIC_RELATION_PROPERTY_ACCESSOR;
    fact.sourceSymbolId = contract->propertySymbolId;
    fact.targetSymbolId = accessorSymbolId;
    fact.sourceTypeId = contract->propertyTypeId;
    fact.targetTypeId = callableTypeId;
    fact.sourceRange = contract->declarationRange;
    fact.targetRange = accessor->location;
    fact.hasSourceRange = semantic_relations_range_is_known(&fact.sourceRange);
    fact.hasTargetRange = semantic_relations_range_is_known(&fact.targetRange);
    return ZrParser_SemanticRelations_Append(context, &fact);
}

TZrBool ZrParser_SemanticRelations_PublishPropertyContracts(
        SZrSemanticContext *context) {
    TZrSize index;

    if (context == ZR_NULL || !context->propertyContracts.isValid ||
        !context->relationFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->propertyContracts.length; index++) {
        const SZrSemanticPropertyContract *contract =
                (const SZrSemanticPropertyContract *)ZrCore_Array_Get(
                        &context->propertyContracts, index);

        if (!semantic_relations_property_contract_is_valid(context, contract)) {
            return ZR_FALSE;
        }
    }
    for (index = 0U; index < context->propertyContracts.length; index++) {
        const SZrSemanticPropertyContract *contract =
                (const SZrSemanticPropertyContract *)ZrCore_Array_Get(
                        &context->propertyContracts, index);

        if (!semantic_relations_publish_property_accessor(
                    context,
                    contract,
                    contract->getterSymbolId,
                    contract->getterCallableTypeId) ||
            !semantic_relations_publish_property_accessor(
                    context,
                    contract,
                    contract->setterSymbolId,
                    contract->setterCallableTypeId) ||
            !semantic_relations_publish_property_accessor(
                    context,
                    contract,
                    contract->initializerSymbolId,
                    contract->initializerCallableTypeId)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticRelations_PublishReferenceDefinitions(
        SZrSemanticContext *context) {
    TZrSize index;

    if (context == ZR_NULL || !context->referenceFacts.isValid ||
        !context->relationFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *reference =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        &context->referenceFacts, index);
        const SZrSemanticSymbolRecord *symbol;
        SZrSemanticRelationFact fact;

        if (reference == ZR_NULL || !reference->isResolved ||
            reference->kind != ZR_SEMANTIC_REFERENCE_WRITE ||
            reference->symbolId == ZR_SEMANTIC_ID_INVALID ||
            !reference->hasDefinitionRange ||
            !semantic_relations_range_is_known(&reference->definitionRange)) {
            continue;
        }
        symbol = ZrParser_Semantic_FindSymbolById(context, reference->symbolId);
        if (symbol == ZR_NULL || symbol->typeId == ZR_SEMANTIC_ID_INVALID ||
            !semantic_relations_range_is_known(&symbol->location) ||
            semantic_relations_has_reference_definition(
                    context, reference->symbolId, &reference->definitionRange)) {
            continue;
        }

        memset(&fact, 0, sizeof(fact));
        fact.kind = ZR_SEMANTIC_RELATION_DECLARATION_DEFINITION;
        fact.sourceSymbolId = reference->symbolId;
        fact.targetSymbolId = reference->symbolId;
        fact.sourceTypeId = symbol->typeId;
        fact.targetTypeId = symbol->typeId;
        fact.sourceRange = symbol->location;
        fact.targetRange = reference->definitionRange;
        fact.hasSourceRange = ZR_TRUE;
        fact.hasTargetRange = ZR_TRUE;
        if (!ZrParser_SemanticRelations_Append(context, &fact)) {
            return ZR_FALSE;
        }
    }
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
