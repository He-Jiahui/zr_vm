#include "zr_vm_parser/semantic_query.h"

#include <stdio.h>
#include <string.h>

#include "zr_vm_parser/canonical_type.h"

static TZrBool canonical_query_same_source(SZrString *left, SZrString *right) {
    return (TZrBool)(left == ZR_NULL || right == ZR_NULL || left == right ||
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
        TZrSize width;
        if (fact == ZR_NULL || !fact->hasCallInfo ||
            !canonical_query_contains(&fact->range, &position) ||
            !canonical_query_scope_allows(scope, &fact->range)) {
            continue;
        }
        width = canonical_query_width(&fact->range);
        if (best == ZR_NULL || width <= bestWidth) {
            best = fact;
            bestWidth = width;
        }
    }
    if (best == ZR_NULL) return ZR_FALSE;
    for (index = 0u; index < context->referenceFacts.length; ++index) {
        const SZrSemanticReferenceFact *reference =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts, index);
        const SZrCanonicalTypeNode *callableType;
        if (reference != ZR_NULL && reference->kind == ZR_SEMANTIC_REFERENCE_CALL &&
            reference->typeId != ZR_SEMANTIC_ID_INVALID &&
            canonical_query_contains(&best->callTargetRange, &reference->range)) {
            callableType = ZrParser_CanonicalType_Find(context, reference->typeId);
            if (callableType == ZR_NULL || callableType->kind != ZR_CANONICAL_TYPE_FUNCTION) {
                continue;
            }
            bestReference = reference;
            if (reference->signatureDisplay != ZR_NULL) {
                break;
            }
        }
    }
    if (bestReference == ZR_NULL) return ZR_FALSE;
    outQuery->callableTypeId = bestReference->typeId;
    outQuery->expression = best;
    outQuery->reference = bestReference;
    if (bestReference->isResolved &&
        bestReference->symbolId != ZR_SEMANTIC_ID_INVALID) {
        outQuery->hasResolvedTarget = ZR_TRUE;
        outQuery->targetSymbolId = bestReference->symbolId;
        outQuery->targetDeclarationRange = bestReference->declarationRange;
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

    if (context == ZR_NULL || query == ZR_NULL || query->expression == ZR_NULL ||
        !query->expression->hasCallInfo || buffer == ZR_NULL || bufferSize == 0u ||
        query->callableTypeId == ZR_SEMANTIC_ID_INVALID) {
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
