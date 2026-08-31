#include "zr_vm_parser/semantic_query.h"

#include <stdio.h>
#include <string.h>

#include "zr_vm_parser/canonical_type.h"

static TZrBool canonical_query_same_source(SZrString *left, SZrString *right) {
    return (TZrBool)(left == ZR_NULL || right == ZR_NULL || left == right ||
                     ZrCore_String_Equal(left, right));
}

static TZrBool canonical_query_same_optional_source_exact(
        SZrString *left,
        SZrString *right) {
    if ((left == ZR_NULL) != (right == ZR_NULL)) {
        return ZR_FALSE;
    }
    return (TZrBool)(left == ZR_NULL || left == right ||
                     ZrCore_String_Equal(left, right));
}

static TZrBool canonical_query_contains(const SZrFileRange *range,
                                        const SZrFileRange *position) {
    if (range == ZR_NULL || position == ZR_NULL ||
        !canonical_query_same_source(range->source, position->source)) {
        return ZR_FALSE;
    }
    if ((range->start.offset > 0u || range->end.offset > 0u) &&
        (position->start.offset > 0u || position->end.offset > 0u)) {
        return (TZrBool)(range->start.offset <= position->start.offset &&
                         position->end.offset <= range->end.offset);
    }
    return (TZrBool)((range->start.line < position->start.line ||
                      (range->start.line == position->start.line &&
                       range->start.column <= position->start.column)) &&
                     (position->end.line < range->end.line ||
                      (position->end.line == range->end.line &&
                       position->end.column <= range->end.column)));
}

static TZrBool canonical_query_scope_allows(
        const SZrParserSemanticQueryScope *scope,
        const SZrFileRange *range) {
    return (TZrBool)(scope == ZR_NULL ||
                     scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE ||
                     (scope->root != ZR_NULL && canonical_query_contains(&scope->root->location, range)));
}

static TZrSize canonical_query_width(const SZrFileRange *range) {
    if (range->end.offset >= range->start.offset) {
        return range->end.offset - range->start.offset;
    }
    return ZR_MAX_SIZE;
}

static TZrBool canonical_query_ranges_equal(const SZrFileRange *left,
                                             const SZrFileRange *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        !canonical_query_same_optional_source_exact(
                left->source, right->source)) {
        return ZR_FALSE;
    }
    return (TZrBool)(left->start.offset == right->start.offset &&
                     left->start.line == right->start.line &&
                     left->start.column == right->start.column &&
                     left->end.offset == right->end.offset &&
                     left->end.line == right->end.line &&
                     left->end.column == right->end.column);
}

static TZrBool canonical_query_optional_strings_equal(
        SZrString *left,
        SZrString *right) {
    return (TZrBool)(left == right ||
                     (left != ZR_NULL && right != ZR_NULL &&
                      ZrCore_String_Equal(left, right)));
}

static TZrBool canonical_query_call_expressions_equal(
        const SZrSemanticExpressionFact *left,
        const SZrSemanticExpressionFact *right) {
    return (TZrBool)(left != ZR_NULL && right != ZR_NULL &&
                     left->kind == right->kind &&
                     left->exactness == right->exactness &&
                     canonical_query_ranges_equal(&left->range, &right->range) &&
                     canonical_query_ranges_equal(
                             &left->callTargetRange, &right->callTargetRange) &&
                     canonical_query_optional_strings_equal(
                             left->callTargetName, right->callTargetName) &&
                     left->typeId == right->typeId &&
                     left->argumentCount == right->argumentCount &&
                     left->hasNamedArguments == right->hasNamedArguments &&
                     left->isMemberCall == right->isMemberCall);
}

static TZrBool canonical_query_call_reference_has_resolved_target(
        const SZrSemanticReferenceFact *reference) {
    return (TZrBool)(reference->isResolved &&
                     reference->symbolId != ZR_SEMANTIC_ID_INVALID);
}

static TZrSize canonical_query_call_reference_completeness(
        const SZrSemanticReferenceFact *reference) {
    TZrSize completeness = 0u;

    if (canonical_query_call_reference_has_resolved_target(reference)) {
        completeness += 8u;
    }
    if (reference->argumentMappings.isValid &&
        reference->argumentMappings.length > 0U) {
        completeness += 1u;
    }
    if (reference->declarationRange.source != ZR_NULL ||
        reference->declarationRange.start.offset > 0u ||
        reference->declarationRange.end.offset > 0u ||
        reference->declarationRange.start.line > 0 ||
        reference->declarationRange.end.line > 0 ||
        reference->declarationRange.start.column > 0 ||
        reference->declarationRange.end.column > 0) {
        completeness += 4u;
    }
    if (reference->signatureDisplay != ZR_NULL) {
        completeness += 2u;
    }
    return completeness;
}

static TZrBool canonical_query_call_argument_parameter_type_matches(
        const SZrSemanticContext *context,
        const SZrCanonicalParameterContract *contract,
        TZrTypeId parameterTypeId) {
    const SZrCanonicalTypeNode *contractType;

    if (context == ZR_NULL || contract == ZR_NULL ||
        parameterTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    if (parameterTypeId == contract->typeId) {
        return ZR_TRUE;
    }
    if (contract->passingForm == ZR_CANONICAL_PASSING_VALUE) {
        return ZR_FALSE;
    }
    contractType = ZrParser_CanonicalType_Find(context, contract->typeId);
    return (TZrBool)(contractType != ZR_NULL &&
                     contractType->kind == ZR_CANONICAL_TYPE_REF &&
                     contractType->data.refType.pointeeTypeId == parameterTypeId);
}

static TZrBool canonical_query_call_argument_passing_matches(
        const SZrCanonicalParameterContract *contract,
        EZrParameterPassingMode passingMode) {
    if (contract == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (contract->passingForm) {
        case ZR_CANONICAL_PASSING_VALUE:
            return passingMode == ZR_PARAMETER_PASSING_MODE_VALUE;
        case ZR_CANONICAL_PASSING_IN:
            return passingMode == ZR_PARAMETER_PASSING_MODE_IN;
        case ZR_CANONICAL_PASSING_REF:
        case ZR_CANONICAL_PASSING_REF_READONLY:
            return passingMode == ZR_PARAMETER_PASSING_MODE_REF;
        case ZR_CANONICAL_PASSING_OUT:
            return passingMode == ZR_PARAMETER_PASSING_MODE_OUT;
        default:
            return ZR_FALSE;
    }
}

static TZrBool canonical_query_call_argument_parameter_is_unique(
        const SZrArray *mappings,
        TZrSize mappingIndex,
        TZrSize parameterIndex) {
    for (TZrSize priorIndex = 0U; priorIndex < mappingIndex; priorIndex++) {
        const SZrSemanticCallArgumentFact *prior =
                (const SZrSemanticCallArgumentFact *)ZrCore_Array_Get(
                        (SZrArray *)mappings, priorIndex);
        if (prior == ZR_NULL || prior->parameterIndex == parameterIndex) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool canonical_query_call_argument_mappings_valid(
        const SZrSemanticContext *context,
        const SZrSemanticExpressionFact *expression,
        const SZrSemanticReferenceFact *reference) {
    const SZrCanonicalTypeNode *callableType;
    TZrSize index;

    if (context == ZR_NULL || expression == ZR_NULL || reference == ZR_NULL ||
        !reference->argumentMappings.isValid ||
        reference->argumentMappings.length == 0U) {
        return ZR_TRUE;
    }
    if (reference->argumentMappings.length != expression->argumentCount) {
        return ZR_FALSE;
    }
    callableType = ZrParser_CanonicalType_Find(context, reference->typeId);
    if (callableType == ZR_NULL ||
        callableType->kind != ZR_CANONICAL_TYPE_FUNCTION) {
        return ZR_FALSE;
    }
    for (index = 0U; index < reference->argumentMappings.length; index++) {
        const SZrSemanticCallArgumentFact *mapping =
                (const SZrSemanticCallArgumentFact *)ZrCore_Array_Get(
                        (SZrArray *)&reference->argumentMappings, index);
        const SZrCanonicalParameterContract *parameterContract =
                mapping != ZR_NULL &&
                                mapping->parameterIndex <
                                        callableType->data.function.parameterContracts.length
                        ? (const SZrCanonicalParameterContract *)ZrCore_Array_Get(
                                  (SZrArray *)&callableType->data.function.parameterContracts,
                                  mapping->parameterIndex)
                        : ZR_NULL;
        if (mapping == ZR_NULL || mapping->argumentIndex != index ||
            mapping->argumentTypeId == ZR_SEMANTIC_ID_INVALID ||
            mapping->parameterTypeId == ZR_SEMANTIC_ID_INVALID ||
            (mapping->conversion != ZR_SEMANTIC_CALL_CONVERSION_EXACT &&
             mapping->conversion != ZR_SEMANTIC_CALL_CONVERSION_IMPLICIT) ||
            !canonical_query_call_argument_parameter_is_unique(
                    &reference->argumentMappings,
                    index,
                    mapping->parameterIndex) ||
            ZrParser_CanonicalType_Find(context, mapping->argumentTypeId) == ZR_NULL ||
            !canonical_query_call_argument_parameter_type_matches(
                    context, parameterContract, mapping->parameterTypeId) ||
            !canonical_query_call_argument_passing_matches(
                    parameterContract, mapping->passingMode) ||
            ((mapping->argumentTypeId == mapping->parameterTypeId) !=
             (mapping->conversion == ZR_SEMANTIC_CALL_CONVERSION_EXACT)) ||
            !canonical_query_same_optional_source_exact(
                    expression->range.source, mapping->argumentRange.source) ||
            !canonical_query_contains(
                    &expression->range, &mapping->argumentRange)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool canonical_query_call_reference_is_candidate(
        const SZrSemanticContext *context,
        const SZrFileRange *callTargetRange,
        const SZrSemanticReferenceFact *reference) {
    const SZrCanonicalTypeNode *callableType;

    if (reference == ZR_NULL || reference->kind != ZR_SEMANTIC_REFERENCE_CALL ||
        reference->typeId == ZR_SEMANTIC_ID_INVALID ||
        !canonical_query_same_optional_source_exact(
                callTargetRange->source, reference->range.source) ||
        !canonical_query_contains(callTargetRange, &reference->range)) {
        return ZR_FALSE;
    }
    callableType = ZrParser_CanonicalType_Find(context, reference->typeId);
    return (TZrBool)(callableType != ZR_NULL &&
                     callableType->kind == ZR_CANONICAL_TYPE_FUNCTION);
}

TZrBool ZrParser_SemanticQuery_CanonicalTypeAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticTypeQuery *outQuery) {
    const SZrSemanticReferenceFact *reference;
    const SZrSemanticExpressionFact *expression;

    if (outQuery != ZR_NULL) memset(outQuery, 0, sizeof(*outQuery));
    if (context == ZR_NULL || outQuery == ZR_NULL ||
        !canonical_query_scope_allows(scope, &position)) {
        return ZR_FALSE;
    }
    reference = ZrParser_SemanticFacts_FindReferenceAtPosition(context, position);
    expression = ZrParser_SemanticFacts_FindExpressionAtPosition(context, position);
    outQuery->reference = reference;
    outQuery->expression = expression;
    if (reference != ZR_NULL && reference->typeId != ZR_SEMANTIC_ID_INVALID &&
        canonical_query_scope_allows(scope, &reference->range)) {
        outQuery->typeId = reference->typeId;
        return ZR_TRUE;
    }
    if (expression != ZR_NULL && expression->typeId != ZR_SEMANTIC_ID_INVALID &&
        ZrParser_SemanticQuery_ExactnessAllowsProjection(expression->exactness) &&
        canonical_query_scope_allows(scope, &expression->range)) {
        outQuery->typeId = expression->typeId;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

const SZrSemanticReferenceFact *ZrParser_SemanticQuery_DeclarationOf(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        const SZrParserSemanticQueryScope *scope) {
    const SZrSemanticReferenceFact *best = ZR_NULL;
    TZrSize bestWidth = ZR_MAX_SIZE;

    if (context == ZR_NULL || symbolId == ZR_SEMANTIC_ID_INVALID ||
        !context->referenceFacts.isValid) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts, index);
        TZrSize width;

        if (fact == ZR_NULL || fact->kind != ZR_SEMANTIC_REFERENCE_DECLARATION ||
            !fact->isResolved || fact->symbolId != symbolId ||
            !canonical_query_scope_allows(scope, &fact->range)) {
            continue;
        }

        width = canonical_query_width(&fact->range);
        if (best == ZR_NULL || width < bestWidth) {
            best = fact;
            bestWidth = width;
        }
    }

    return best;
}

TZrBool ZrParser_SemanticQuery_CallAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticCallQuery *outQuery) {
    const SZrSemanticExpressionFact *best = ZR_NULL;
    const SZrSemanticReferenceFact *bestReference = ZR_NULL;
    TZrSize bestWidth = ZR_MAX_SIZE;
    TZrSize bestReferenceCompleteness = 0u;
    TZrBool bestIsConflicting = ZR_FALSE;
    TZrSize index;

    if (outQuery != ZR_NULL) memset(outQuery, 0, sizeof(*outQuery));
    if (context == ZR_NULL || outQuery == ZR_NULL ||
        !context->expressionFacts.isValid || !context->referenceFacts.isValid ||
        !canonical_query_scope_allows(scope, &position)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < context->expressionFacts.length; ++index) {
        const SZrSemanticExpressionFact *fact =
                (const SZrSemanticExpressionFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->expressionFacts, index);
        TZrBool factIsExact;
        TZrBool bestIsExact;
        TZrSize width;
        if (fact == ZR_NULL || !fact->hasCallInfo ||
            !canonical_query_same_optional_source_exact(
                    fact->range.source, position.source) ||
            !canonical_query_contains(&fact->range, &position) ||
            !canonical_query_scope_allows(scope, &fact->range)) {
            continue;
        }
        width = canonical_query_width(&fact->range);
        factIsExact = ZrParser_SemanticQuery_ExactnessAllowsProjection(
                fact->exactness);
        bestIsExact = best != ZR_NULL &&
                      ZrParser_SemanticQuery_ExactnessAllowsProjection(
                              best->exactness);
        if (best == ZR_NULL || width < bestWidth ||
            (width == bestWidth && factIsExact && !bestIsExact)) {
            best = fact;
            bestWidth = width;
            bestIsConflicting = ZR_FALSE;
        } else if (width == bestWidth && factIsExact == bestIsExact &&
                   !canonical_query_call_expressions_equal(best, fact)) {
            bestIsConflicting = ZR_TRUE;
        }
    }
    if (best == ZR_NULL || bestIsConflicting) return ZR_FALSE;
    if (!canonical_query_same_optional_source_exact(
                best->range.source, best->callTargetRange.source)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < context->referenceFacts.length; ++index) {
        const SZrSemanticReferenceFact *reference =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts, index);
        TZrSize completeness;

        if (!canonical_query_call_reference_is_candidate(
                    context, &best->callTargetRange, reference)) {
            continue;
        }
        completeness = canonical_query_call_reference_completeness(reference);
        if (bestReference == ZR_NULL ||
            completeness > bestReferenceCompleteness) {
            bestReference = reference;
            bestReferenceCompleteness = completeness;
        }
    }
    if (bestReference == ZR_NULL) return ZR_FALSE;
    if (canonical_query_call_reference_has_resolved_target(bestReference)) {
        for (index = 0u; index < context->referenceFacts.length; ++index) {
            const SZrSemanticReferenceFact *reference =
                    (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                            (SZrArray *)&context->referenceFacts, index);

            if (reference == bestReference ||
                !canonical_query_call_reference_is_candidate(
                        context, &best->callTargetRange, reference) ||
                !canonical_query_ranges_equal(
                        &bestReference->range, &reference->range) ||
                !canonical_query_call_reference_has_resolved_target(reference)) {
                continue;
            }
            if (bestReference->symbolId != reference->symbolId) {
                return ZR_FALSE;
            }
        }
    }
    if (!canonical_query_call_argument_mappings_valid(
                context, best, bestReference)) {
        return ZR_FALSE;
    }
    outQuery->callableTypeId = bestReference->typeId;
    outQuery->expression = best;
    outQuery->reference = bestReference;
    outQuery->callSiteRange = best->range;
    outQuery->callTargetRange = best->callTargetRange;
    outQuery->argumentCount = best->argumentCount;
    outQuery->hasNamedArguments = best->hasNamedArguments;
    outQuery->isMemberCall = best->isMemberCall;
    if (canonical_query_call_reference_has_resolved_target(bestReference)) {
        outQuery->hasResolvedTarget = ZR_TRUE;
        outQuery->targetSymbolId = bestReference->symbolId;
        outQuery->targetDeclarationRange = bestReference->declarationRange;
    }
    if (bestReference->argumentMappings.isValid &&
        bestReference->argumentMappings.length > 0U) {
        outQuery->argumentMappings = &bestReference->argumentMappings;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticQuery_FormatCall(
        const SZrSemanticContext *context,
        const SZrParserSemanticCallQuery *query,
        TZrChar *buffer,
        TZrSize bufferSize) {
    TZrChar typeBuffer[512];
    const TZrChar *name = ZR_NULL;
    int written;

    if (buffer != ZR_NULL && bufferSize > 0u) {
        buffer[0] = '\0';
    }
    if (context == ZR_NULL || query == ZR_NULL || query->expression == ZR_NULL ||
        query->reference == ZR_NULL ||
        !query->expression->hasCallInfo ||
        !ZrParser_SemanticQuery_ExactnessAllowsProjection(
                query->expression->exactness) ||
        buffer == ZR_NULL || bufferSize == 0u ||
        query->callableTypeId == ZR_SEMANTIC_ID_INVALID ||
        query->reference->kind != ZR_SEMANTIC_REFERENCE_CALL ||
        query->reference->typeId != query->callableTypeId ||
        !canonical_query_contains(
                &query->expression->callTargetRange, &query->reference->range)) {
        return ZR_FALSE;
    }
    if (query->reference != ZR_NULL && query->reference->signatureDisplay != ZR_NULL) {
        const TZrChar *display = ZrCore_String_GetNativeString(query->reference->signatureDisplay);
        TZrSize length = ZrCore_String_GetByteLength(query->reference->signatureDisplay);
        if (display == ZR_NULL || length + 1u > bufferSize) return ZR_FALSE;
        memcpy(buffer, display, length);
        buffer[length] = '\0';
        return ZR_TRUE;
    }
    if (!ZrParser_CanonicalType_Format(
                context, query->callableTypeId, typeBuffer, sizeof(typeBuffer))) {
        return ZR_FALSE;
    }
    if (query->reference != ZR_NULL && query->reference->name != ZR_NULL) {
        name = ZrCore_String_GetNativeString(query->reference->name);
    } else if (query->expression != ZR_NULL && query->expression->callTargetName != ZR_NULL) {
        name = ZrCore_String_GetNativeString(query->expression->callTargetName);
    }
    written = name != ZR_NULL && name[0] != '\0'
                      ? snprintf(buffer, bufferSize, "%s: %s", name, typeBuffer)
                      : snprintf(buffer, bufferSize, "%s", typeBuffer);
    return (TZrBool)(written >= 0 && (TZrSize)written < bufferSize);
}
