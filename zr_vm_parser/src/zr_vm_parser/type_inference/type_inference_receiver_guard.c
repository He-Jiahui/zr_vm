#include "type_inference_internal.h"

#include <string.h>

static TZrBool receiver_guard_segment_access_mode(
        const SZrAstNode *segment,
        EZrPostfixAccessMode *outMode) {
    if (segment == ZR_NULL || outMode == ZR_NULL) {
        return ZR_FALSE;
    }
    if (segment->type == ZR_AST_MEMBER_EXPRESSION) {
        *outMode = segment->data.memberExpression.accessMode;
        return ZR_TRUE;
    }
    if (segment->type == ZR_AST_FUNCTION_CALL) {
        *outMode = segment->data.functionCall.accessMode;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool receiver_guard_type_is_unknown(
        const SZrInferredType *type) {
    return type != ZR_NULL &&
           (type->baseType == ZR_VALUE_TYPE_UNKNOWN ||
            (type->baseType == ZR_VALUE_TYPE_OBJECT &&
             type->typeName == ZR_NULL &&
             type->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_NONE));
}

static SZrAstNode *receiver_guard_previous_node(
        SZrAstNode *primaryNode,
        SZrAstNodeArray *segments,
        TZrSize segmentIndex) {
    if (primaryNode == ZR_NULL ||
        primaryNode->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_NULL;
    }
    if (segmentIndex == 0u || segments == ZR_NULL ||
        segments->nodes == ZR_NULL || segmentIndex > segments->count) {
        return primaryNode->data.primaryExpression.property;
    }
    return segments->nodes[segmentIndex - 1u];
}

TZrBool infer_receiver_guard_for_segment(
        SZrCompilerState *cs,
        SZrAstNode *primaryNode,
        SZrAstNodeArray *segments,
        TZrSize segmentIndex,
        SZrAstNode *segment,
        SZrInferredType *receiverType,
        TZrBool *outResultLifted) {
    EZrPostfixAccessMode accessMode;
    EZrReceiverGuardKind guardKind;
    SZrInferredType guardedType;
    SZrReceiverGuardFact fact;
    TZrBool isWeak;
    TZrBool isNullable;
    TZrBool isOptional;

    if (cs == ZR_NULL || primaryNode == ZR_NULL || segment == ZR_NULL ||
        receiverType == ZR_NULL || outResultLifted == ZR_NULL ||
        !receiver_guard_segment_access_mode(segment, &accessMode)) {
        return ZR_FALSE;
    }

    isWeak = receiverType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_WEAK;
    isNullable = receiverType->isNullable;
    isOptional = accessMode == ZR_POSTFIX_ACCESS_OPTIONAL;

    if (!isWeak && !isNullable) {
        if (!isOptional) {
            return ZR_TRUE;
        }
        ZrParser_Compiler_Error(
                cs,
                receiver_guard_type_is_unknown(receiverType)
                        ? "unsupported_optional_receiver"
                        : "redundant_optional_access",
                segment->location);
        return ZR_FALSE;
    }

    guardKind = isWeak ? ZR_RECEIVER_GUARD_WEAK_WAKE
                       : ZR_RECEIVER_GUARD_NULL;
    ZrParser_InferredType_Init(
            cs->state, &guardedType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Copy(cs->state, &guardedType, receiverType);
    guardedType.isNullable = ZR_FALSE;
    if (isWeak) {
        guardedType.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_SHARED;
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = segment;
    fact.receiver = receiver_guard_previous_node(
            primaryNode, segments, segmentIndex);
    fact.firstSegment = segment;
    fact.range = primaryNode->location;
    fact.kind = guardKind;
    fact.mode = isOptional ? ZR_RECEIVER_GUARD_OPTIONAL
                           : ZR_RECEIVER_GUARD_DIRECT;
    fact.resultLift = isOptional ? ZR_RECEIVER_GUARD_RESULT_NULLABLE
                                 : ZR_RECEIVER_GUARD_RESULT_UNCHANGED;
    fact.chainSegmentStart = segmentIndex;
    fact.chainSegmentEnd = segments != ZR_NULL ? segments->count : segmentIndex + 1u;
    fact.receiverType = *receiverType;
    fact.guardedType = guardedType;
    type_inference_record_expression_fact(
            cs, fact.receiver, receiverType);
    if (!ZrParser_SemanticFacts_AppendReceiverGuard(
                cs->semanticContext, &fact)) {
        ZrParser_InferredType_Free(cs->state, &guardedType);
        return ZR_FALSE;
    }

    ZrParser_InferredType_Free(cs->state, receiverType);
    ZrParser_InferredType_Init(
            cs->state, receiverType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Copy(cs->state, receiverType, &guardedType);
    ZrParser_InferredType_Free(cs->state, &guardedType);
    if (isOptional) {
        *outResultLifted = ZR_TRUE;
    }
    return ZR_TRUE;
}

void infer_receiver_guard_finalize_result_lift(
        SZrCompilerState *cs,
        SZrAstNodeArray *segments,
        const SZrInferredType *resultType) {
    EZrReceiverGuardResultLift lift;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || segments == ZR_NULL ||
        segments->nodes == ZR_NULL || resultType == ZR_NULL ||
        !cs->semanticContext->receiverGuardFacts.isValid) {
        return;
    }

    lift = resultType->baseType == ZR_VALUE_TYPE_NULL
                   ? ZR_RECEIVER_GUARD_RESULT_VOID_NOOP
                   : ZR_RECEIVER_GUARD_RESULT_NULLABLE;
    for (TZrSize segmentIndex = 0u;
         segmentIndex < segments->count;
         segmentIndex++) {
        SZrAstNode *segment = segments->nodes[segmentIndex];

        for (TZrSize factIndex = 0u;
             factIndex < cs->semanticContext->receiverGuardFacts.length;
             factIndex++) {
            SZrReceiverGuardFact *fact =
                    (SZrReceiverGuardFact *)ZrCore_Array_Get(
                            &cs->semanticContext->receiverGuardFacts,
                            factIndex);
            if (fact != ZR_NULL && fact->node == segment &&
                fact->mode == ZR_RECEIVER_GUARD_OPTIONAL) {
                fact->resultLift = lift;
            }
        }
    }
}
