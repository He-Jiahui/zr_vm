#include "zr_vm_parser/semantic.h"

static TZrBool semantic_facts_has_offset(const SZrFilePosition *position) {
    return position != ZR_NULL && position->offset > 0;
}

static TZrBool semantic_facts_same_source(SZrString *left, SZrString *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_TRUE;
    }
    if (left == right) {
        return ZR_TRUE;
    }
    return ZrCore_String_Equal(left, right);
}

static TZrBool semantic_facts_range_contains_position(const SZrFileRange *range,
                                                      const SZrFileRange *position) {
    TZrSize queryOffset;
    TZrInt32 queryLine;
    TZrInt32 queryColumn;

    if (range == ZR_NULL || position == ZR_NULL ||
        !semantic_facts_same_source(range->source, position->source)) {
        return ZR_FALSE;
    }

    if ((semantic_facts_has_offset(&range->start) ||
         semantic_facts_has_offset(&range->end)) &&
        (semantic_facts_has_offset(&position->start) ||
         semantic_facts_has_offset(&position->end))) {
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

static TZrSize semantic_facts_range_width(const SZrFileRange *range) {
    if (range == ZR_NULL) {
        return 0;
    }
    if (range->end.offset >= range->start.offset) {
        return range->end.offset - range->start.offset;
    }
    return 0;
}

static TZrInt32 semantic_facts_reference_priority(EZrSemanticReferenceKind kind) {
    switch (kind) {
        case ZR_SEMANTIC_REFERENCE_WRITE:
        case ZR_SEMANTIC_REFERENCE_MEMBER_WRITE:
            return 4;
        case ZR_SEMANTIC_REFERENCE_CALL:
            return 3;
        case ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS:
            return 2;
        case ZR_SEMANTIC_REFERENCE_READ:
            return 1;
        case ZR_SEMANTIC_REFERENCE_DECLARATION:
        case ZR_SEMANTIC_REFERENCE_UNKNOWN:
        default:
            return 0;
    }
}

static void semantic_facts_free_expression_facts(SZrSemanticContext *context) {
    TZrSize i;

    if (context == ZR_NULL || !context->expressionFacts.isValid) {
        return;
    }

    for (i = 0; i < context->expressionFacts.length; i++) {
        SZrSemanticExpressionFact *fact =
            (SZrSemanticExpressionFact *)ZrCore_Array_Get(&context->expressionFacts, i);
        if (fact != ZR_NULL) {
            ZrParser_InferredType_Free(context->state, &fact->inferredType);
        }
    }
}

static SZrString *semantic_facts_clone_string(SZrSemanticContext *context, SZrString *value) {
    TZrNativeString text;

    if (context == ZR_NULL || context->state == ZR_NULL || value == ZR_NULL) {
        return ZR_NULL;
    }

    text = ZrCore_String_GetNativeString(value);
    if (text == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(context->state, text, ZrCore_String_GetByteLength(value));
}

static void semantic_facts_init_array(SZrSemanticContext *context,
                                      SZrArray *array,
                                      TZrSize elementSize) {
    ZrCore_Array_Init(context->state,
                      array,
                      elementSize,
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
}

void ZrParser_SemanticFacts_Init(SZrSemanticContext *context) {
    if (context == ZR_NULL || context->state == ZR_NULL) {
        return;
    }

    semantic_facts_init_array(context, &context->expressionFacts, sizeof(SZrSemanticExpressionFact));
    semantic_facts_init_array(context, &context->referenceFacts, sizeof(SZrSemanticReferenceFact));
    semantic_facts_init_array(context, &context->numericFacts, sizeof(SZrSemanticNumericFact));
    semantic_facts_init_array(context, &context->reachabilityFacts, sizeof(SZrSemanticReachabilityFact));
    semantic_facts_init_array(context, &context->logicalFacts, sizeof(SZrSemanticLogicalFact));
    semantic_facts_init_array(context, &context->ownershipFacts, sizeof(SZrSemanticOwnershipFact));
}

void ZrParser_SemanticFacts_Reset(SZrSemanticContext *context) {
    if (context == ZR_NULL) {
        return;
    }

    semantic_facts_free_expression_facts(context);
    if (context->expressionFacts.isValid) {
        context->expressionFacts.length = 0;
    }
    if (context->referenceFacts.isValid) {
        context->referenceFacts.length = 0;
    }
    if (context->numericFacts.isValid) {
        context->numericFacts.length = 0;
    }
    if (context->reachabilityFacts.isValid) {
        context->reachabilityFacts.length = 0;
    }
    if (context->logicalFacts.isValid) {
        context->logicalFacts.length = 0;
    }
    if (context->ownershipFacts.isValid) {
        context->ownershipFacts.length = 0;
    }
}

void ZrParser_SemanticFacts_Free(SZrSemanticContext *context) {
    if (context == ZR_NULL || context->state == ZR_NULL) {
        return;
    }

    ZrParser_SemanticFacts_Reset(context);
    ZrCore_Array_Free(context->state, &context->expressionFacts);
    ZrCore_Array_Free(context->state, &context->referenceFacts);
    ZrCore_Array_Free(context->state, &context->numericFacts);
    ZrCore_Array_Free(context->state, &context->reachabilityFacts);
    ZrCore_Array_Free(context->state, &context->logicalFacts);
    ZrCore_Array_Free(context->state, &context->ownershipFacts);
}

TZrBool ZrParser_SemanticFacts_AppendExpression(SZrSemanticContext *context,
                                                const SZrSemanticExpressionFact *fact) {
    SZrSemanticExpressionFact copy;

    if (context == ZR_NULL || fact == ZR_NULL || !context->expressionFacts.isValid) {
        return ZR_FALSE;
    }

    copy = *fact;
    ZrParser_InferredType_Copy(context->state, &copy.inferredType, &fact->inferredType);
    copy.callTargetName = semantic_facts_clone_string(context, fact->callTargetName);
    copy.memberName = semantic_facts_clone_string(context, fact->memberName);
    copy.diagnosticMessage = semantic_facts_clone_string(context, fact->diagnosticMessage);
    ZrCore_Array_Push(context->state, &context->expressionFacts, &copy);
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticFacts_AppendReference(SZrSemanticContext *context,
                                               const SZrSemanticReferenceFact *fact) {
    SZrSemanticReferenceFact copy;

    if (context == ZR_NULL || fact == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }

    copy = *fact;
    ZrCore_Array_Push(context->state, &context->referenceFacts, &copy);
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticFacts_AppendNumeric(SZrSemanticContext *context,
                                             const SZrSemanticNumericFact *fact) {
    SZrSemanticNumericFact copy;

    if (context == ZR_NULL || fact == ZR_NULL || !context->numericFacts.isValid) {
        return ZR_FALSE;
    }

    copy = *fact;
    ZrCore_Array_Push(context->state, &context->numericFacts, &copy);
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticFacts_AppendReachability(SZrSemanticContext *context,
                                                  const SZrSemanticReachabilityFact *fact) {
    SZrSemanticReachabilityFact copy;

    if (context == ZR_NULL || fact == ZR_NULL || !context->reachabilityFacts.isValid) {
        return ZR_FALSE;
    }

    copy = *fact;
    ZrCore_Array_Push(context->state, &context->reachabilityFacts, &copy);
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticFacts_AppendLogical(SZrSemanticContext *context,
                                             const SZrSemanticLogicalFact *fact) {
    SZrSemanticLogicalFact copy;

    if (context == ZR_NULL || fact == ZR_NULL || !context->logicalFacts.isValid) {
        return ZR_FALSE;
    }

    copy = *fact;
    ZrCore_Array_Push(context->state, &context->logicalFacts, &copy);
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticFacts_AppendOwnership(SZrSemanticContext *context,
                                               const SZrSemanticOwnershipFact *fact) {
    SZrSemanticOwnershipFact copy;

    if (context == ZR_NULL || fact == ZR_NULL || !context->ownershipFacts.isValid) {
        return ZR_FALSE;
    }

    copy = *fact;
    if (fact->diagnosticMessage != ZR_NULL) {
        copy.diagnosticMessage = semantic_facts_clone_string(context, fact->diagnosticMessage);
        if (copy.diagnosticMessage == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    ZrCore_Array_Push(context->state, &context->ownershipFacts, &copy);
    return ZR_TRUE;
}

const SZrSemanticExpressionFact *ZrParser_SemanticFacts_FindExpressionByNode(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    TZrSize i;

    if (context == ZR_NULL || node == ZR_NULL || !context->expressionFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->expressionFacts.length; i++) {
        const SZrSemanticExpressionFact *fact =
            (const SZrSemanticExpressionFact *)ZrCore_Array_Get((SZrArray *)&context->expressionFacts, i);
        if (fact != ZR_NULL && fact->node == node) {
            return fact;
        }
    }
    return ZR_NULL;
}

const SZrSemanticExpressionFact *ZrParser_SemanticFacts_FindExpressionAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position) {
    TZrSize i;
    const SZrSemanticExpressionFact *best = ZR_NULL;
    TZrSize bestWidth = 0;

    if (context == ZR_NULL || !context->expressionFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->expressionFacts.length; i++) {
        const SZrSemanticExpressionFact *fact =
            (const SZrSemanticExpressionFact *)ZrCore_Array_Get((SZrArray *)&context->expressionFacts, i);
        if (fact != ZR_NULL && semantic_facts_range_contains_position(&fact->range, &position)) {
            TZrSize width = semantic_facts_range_width(&fact->range);
            if (best == ZR_NULL || width <= bestWidth) {
                best = fact;
                bestWidth = width;
            }
        }
    }
    return best;
}

const SZrSemanticReferenceFact *ZrParser_SemanticFacts_FindReferenceAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position) {
    TZrSize i;
    const SZrSemanticReferenceFact *best = ZR_NULL;
    TZrSize bestWidth = 0;
    TZrInt32 bestPriority = 0;

    if (context == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->referenceFacts.length; i++) {
        const SZrSemanticReferenceFact *fact =
            (const SZrSemanticReferenceFact *)ZrCore_Array_Get((SZrArray *)&context->referenceFacts, i);
        if (fact != ZR_NULL && semantic_facts_range_contains_position(&fact->range, &position)) {
            TZrSize width = semantic_facts_range_width(&fact->range);
            TZrInt32 priority = semantic_facts_reference_priority(fact->kind);
            if (best == ZR_NULL ||
                width < bestWidth ||
                (width == bestWidth && priority > bestPriority)) {
                best = fact;
                bestWidth = width;
                bestPriority = priority;
            }
        }
    }
    return best;
}

const SZrSemanticReferenceFact *ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
        const SZrSemanticContext *context,
        SZrFileRange position,
        EZrSemanticReferenceKind kind) {
    TZrSize i;
    const SZrSemanticReferenceFact *best = ZR_NULL;
    TZrSize bestWidth = 0;

    if (context == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->referenceFacts.length; i++) {
        const SZrSemanticReferenceFact *fact =
            (const SZrSemanticReferenceFact *)ZrCore_Array_Get((SZrArray *)&context->referenceFacts, i);
        if (fact != ZR_NULL &&
            fact->kind == kind &&
            semantic_facts_range_contains_position(&fact->range, &position)) {
            TZrSize width = semantic_facts_range_width(&fact->range);
            if (best == ZR_NULL || width < bestWidth) {
                best = fact;
                bestWidth = width;
            }
        }
    }
    return best;
}

const SZrSemanticReferenceFact *ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
        const SZrSemanticContext *context,
        const SZrAstNode *node,
        EZrSemanticReferenceKind kind) {
    TZrSize i;

    if (context == ZR_NULL || node == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->referenceFacts.length; i++) {
        const SZrSemanticReferenceFact *fact =
            (const SZrSemanticReferenceFact *)ZrCore_Array_Get((SZrArray *)&context->referenceFacts, i);
        if (fact != ZR_NULL && fact->node == node && fact->kind == kind) {
            return fact;
        }
    }
    return ZR_NULL;
}

const SZrSemanticNumericFact *ZrParser_SemanticFacts_FindNumericByNode(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    TZrSize i;

    if (context == ZR_NULL || node == ZR_NULL || !context->numericFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->numericFacts.length; i++) {
        const SZrSemanticNumericFact *fact =
            (const SZrSemanticNumericFact *)ZrCore_Array_Get((SZrArray *)&context->numericFacts, i);
        if (fact != ZR_NULL && fact->node == node) {
            return fact;
        }
    }
    return ZR_NULL;
}

const SZrSemanticReachabilityFact *ZrParser_SemanticFacts_FindReachabilityAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position) {
    TZrSize i;

    if (context == ZR_NULL || !context->reachabilityFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->reachabilityFacts.length; i++) {
        const SZrSemanticReachabilityFact *fact =
            (const SZrSemanticReachabilityFact *)ZrCore_Array_Get((SZrArray *)&context->reachabilityFacts, i);
        if (fact != ZR_NULL && semantic_facts_range_contains_position(&fact->range, &position)) {
            return fact;
        }
    }
    return ZR_NULL;
}

const SZrSemanticLogicalFact *ZrParser_SemanticFacts_FindLogicalByNode(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    TZrSize i;

    if (context == ZR_NULL || node == ZR_NULL || !context->logicalFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->logicalFacts.length; i++) {
        const SZrSemanticLogicalFact *fact =
            (const SZrSemanticLogicalFact *)ZrCore_Array_Get((SZrArray *)&context->logicalFacts, i);
        if (fact != ZR_NULL && fact->node == node) {
            return fact;
        }
    }
    return ZR_NULL;
}

const SZrSemanticLogicalFact *ZrParser_SemanticFacts_FindLogicalAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position) {
    TZrSize i;
    const SZrSemanticLogicalFact *best = ZR_NULL;
    TZrSize bestWidth = 0;

    if (context == ZR_NULL || !context->logicalFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->logicalFacts.length; i++) {
        const SZrSemanticLogicalFact *fact =
            (const SZrSemanticLogicalFact *)ZrCore_Array_Get((SZrArray *)&context->logicalFacts, i);
        if (fact != ZR_NULL && semantic_facts_range_contains_position(&fact->range, &position)) {
            TZrSize width = semantic_facts_range_width(&fact->range);
            if (best == ZR_NULL || width <= bestWidth) {
                best = fact;
                bestWidth = width;
            }
        }
    }
    return best;
}

const SZrSemanticOwnershipFact *ZrParser_SemanticFacts_FindOwnershipByNode(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    TZrSize i;

    if (context == ZR_NULL || node == ZR_NULL || !context->ownershipFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->ownershipFacts.length; i++) {
        const SZrSemanticOwnershipFact *fact =
            (const SZrSemanticOwnershipFact *)ZrCore_Array_Get((SZrArray *)&context->ownershipFacts, i);
        if (fact != ZR_NULL && fact->node == node) {
            return fact;
        }
    }
    return ZR_NULL;
}

const SZrSemanticOwnershipFact *ZrParser_SemanticFacts_FindOwnershipAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position) {
    TZrSize i;

    if (context == ZR_NULL || !context->ownershipFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->ownershipFacts.length; i++) {
        const SZrSemanticOwnershipFact *fact =
            (const SZrSemanticOwnershipFact *)ZrCore_Array_Get((SZrArray *)&context->ownershipFacts, i);
        if (fact != ZR_NULL && semantic_facts_range_contains_position(&fact->range, &position)) {
            return fact;
        }
    }
    return ZR_NULL;
}
