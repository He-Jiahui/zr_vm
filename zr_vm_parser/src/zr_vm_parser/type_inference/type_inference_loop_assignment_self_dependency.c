#include "type_inference_loop_assignment_self_dependency.h"

#include <string.h>

#include "type_inference_loop_assignment_syntax.h"
#include "zr_vm_core/array.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_parser/compiler.h"

static TZrBool loop_assignment_self_dependency_identifier_is_name(SZrAstNode *node,
                                                                  SZrString *name) {
    return node != ZR_NULL &&
           name != ZR_NULL &&
           node->type == ZR_AST_IDENTIFIER_LITERAL &&
           node->data.identifier.name != ZR_NULL &&
           ZrCore_String_Equal(node->data.identifier.name, name);
}

static TZrBool loop_assignment_self_dependency_inferred_integer_range(
        SZrCompilerState *cs,
        SZrAstNode *node,
        TZrInt64 *outMin,
        TZrInt64 *outMax) {
    SZrInferredType inferredType;
    TZrBool success;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        node == ZR_NULL ||
        outMin == ZR_NULL ||
        outMax == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    success = ZrParser_ExpressionType_Infer(cs, node, &inferredType) &&
              inferredType.baseType == ZR_VALUE_TYPE_INT64 &&
              inferredType.hasRangeConstraint;
    if (success) {
        *outMin = inferredType.minValue;
        *outMax = inferredType.maxValue;
    }
    ZrParser_InferredType_Free(cs->state, &inferredType);
    return success;
}

static TZrBool loop_assignment_self_dependency_integer_range(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrString *targetName,
        TZrInt64 *outMin,
        TZrInt64 *outMax) {
    const SZrTypeBinding *binding;

    if (outMin != ZR_NULL) {
        *outMin = 0;
    }
    if (outMax != ZR_NULL) {
        *outMax = 0;
    }

    if (node == ZR_NULL ||
        outMin == ZR_NULL ||
        outMax == ZR_NULL) {
        return ZR_FALSE;
    }

    if (node->type == ZR_AST_INTEGER_LITERAL) {
        *outMin = node->data.integerLiteral.value;
        *outMax = node->data.integerLiteral.value;
        return ZR_TRUE;
    }

    if (cs == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        targetName == ZR_NULL ||
        !ZrParser_TypeInferenceLoopAssignment_DeltaExpressionIsSupported(node, targetName)) {
        return ZR_FALSE;
    }

    if (ZrParser_TypeInferenceLoopAssignment_TryCancelledSignedAdditiveDeltaRange(
            cs,
            node,
            targetName,
            outMin,
            outMax)) {
        return ZR_TRUE;
    }

    if (node->type != ZR_AST_IDENTIFIER_LITERAL) {
        return loop_assignment_self_dependency_inferred_integer_range(cs, node, outMin, outMax);
    }

    if (node->data.identifier.name == ZR_NULL ||
        ZrCore_String_Equal(node->data.identifier.name, targetName)) {
        return ZR_FALSE;
    }

    binding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, node->data.identifier.name);
    if (binding == ZR_NULL ||
        binding->type.baseType != ZR_VALUE_TYPE_INT64 ||
        !binding->type.hasRangeConstraint) {
        return ZR_FALSE;
    }

    *outMin = binding->type.minValue;
    *outMax = binding->type.maxValue;
    return ZR_TRUE;
}

static TZrBool loop_assignment_self_dependency_find_delta_expression(SZrAstNode *right,
                                                                     SZrString *targetName,
                                                                     SZrAstNode **outNode,
                                                                     TZrInt32 *outSign) {
    if (outNode != ZR_NULL) {
        *outNode = ZR_NULL;
    }
    if (outSign != ZR_NULL) {
        *outSign = 0;
    }

    if (right == ZR_NULL ||
        targetName == ZR_NULL ||
        outNode == ZR_NULL ||
        outSign == ZR_NULL ||
        right->type != ZR_AST_BINARY_EXPRESSION ||
        right->data.binaryExpression.op.op == ZR_NULL) {
        return ZR_FALSE;
    }

    if (strcmp(right->data.binaryExpression.op.op, "+") == 0) {
        if (loop_assignment_self_dependency_identifier_is_name(
                right->data.binaryExpression.left,
                targetName)) {
            *outNode = right->data.binaryExpression.right;
            *outSign = 1;
            return ZR_TRUE;
        }
        if (loop_assignment_self_dependency_identifier_is_name(
                right->data.binaryExpression.right,
                targetName)) {
            *outNode = right->data.binaryExpression.left;
            *outSign = 1;
            return ZR_TRUE;
        }
    }

    if (strcmp(right->data.binaryExpression.op.op, "-") == 0 &&
        loop_assignment_self_dependency_identifier_is_name(
                right->data.binaryExpression.left,
                targetName)) {
        *outNode = right->data.binaryExpression.right;
        *outSign = -1;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool loop_assignment_self_dependency_normalize_delta_expression(
        SZrAstNode *node,
        TZrInt32 sign,
        SZrAstNode **outNode,
        TZrInt32 *outSign) {
    if (outNode != ZR_NULL) {
        *outNode = ZR_NULL;
    }
    if (outSign != ZR_NULL) {
        *outSign = 0;
    }
    if (node == ZR_NULL ||
        outNode == ZR_NULL ||
        outSign == ZR_NULL ||
        (sign != 1 && sign != -1)) {
        return ZR_FALSE;
    }

    if (node->type == ZR_AST_UNARY_EXPRESSION &&
        node->data.unaryExpression.op.op != ZR_NULL) {
        if (strcmp(node->data.unaryExpression.op.op, "-") == 0) {
            return loop_assignment_self_dependency_normalize_delta_expression(
                    node->data.unaryExpression.argument,
                    -sign,
                    outNode,
                    outSign);
        }
        if (strcmp(node->data.unaryExpression.op.op, "+") == 0) {
            return loop_assignment_self_dependency_normalize_delta_expression(
                    node->data.unaryExpression.argument,
                    sign,
                    outNode,
                    outSign);
        }
    }

    *outNode = node;
    *outSign = sign;
    return ZR_TRUE;
}

static TZrBool loop_assignment_self_dependency_int64_add(TZrInt64 left,
                                                         TZrInt64 right,
                                                         TZrInt64 *outValue) {
    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if ((right > 0 && left > ZR_TYPE_RANGE_INT64_MAX - right) ||
        (right < 0 && left < ZR_TYPE_RANGE_INT64_MIN - right)) {
        return ZR_FALSE;
    }

    *outValue = left + right;
    return ZR_TRUE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_TrySelfDependentDelta(SZrCompilerState *cs,
                                                                   SZrAstNode *assignmentNode,
                                                                   SZrString *targetName,
                                                                   TZrInt64 *outDeltaMin,
                                                                   TZrInt64 *outDeltaMax) {
    SZrString *name;
    SZrAstNode *right;
    SZrAstNode *deltaNode;
    TZrInt32 deltaSign;
    TZrInt64 deltaMin;
    TZrInt64 deltaMax;

    if (outDeltaMin != ZR_NULL) {
        *outDeltaMin = 0;
    }
    if (outDeltaMax != ZR_NULL) {
        *outDeltaMax = 0;
    }
    if (assignmentNode == ZR_NULL ||
        targetName == ZR_NULL ||
        outDeltaMin == ZR_NULL ||
        outDeltaMax == ZR_NULL ||
        !ZrParser_TypeInferenceLoopAssignment_AssignmentParts(assignmentNode, &name, &right) ||
        name == ZR_NULL ||
        !ZrCore_String_Equal(name, targetName) ||
        right == ZR_NULL ||
        !loop_assignment_self_dependency_find_delta_expression(
                right,
                targetName,
                &deltaNode,
                &deltaSign) ||
        !loop_assignment_self_dependency_normalize_delta_expression(
                deltaNode,
                deltaSign,
                &deltaNode,
                &deltaSign) ||
        !loop_assignment_self_dependency_integer_range(
                cs,
                deltaNode,
                targetName,
                &deltaMin,
                &deltaMax) ||
        !((deltaMin == 0 && deltaMax == 0) ||
          (deltaMin >= 0 && deltaMax > 0) ||
          (deltaMin < 0 && deltaMax <= 0) ||
          (deltaMin < 0 && deltaMax > 0))) {
        return ZR_FALSE;
    }

    if (deltaSign > 0) {
        *outDeltaMin = deltaMin;
        *outDeltaMax = deltaMax;
    } else {
        *outDeltaMin = -deltaMax;
        *outDeltaMax = -deltaMin;
    }
    return ZR_TRUE;
}

SZrAstNode *ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaExpression(SZrAstNode *right,
                                                                              SZrString *targetName) {
    SZrAstNode *deltaNode = ZR_NULL;
    TZrInt32 deltaSign = 0;

    if (!loop_assignment_self_dependency_find_delta_expression(
            right,
            targetName,
            &deltaNode,
            &deltaSign)) {
        return ZR_NULL;
    }
    if (!loop_assignment_self_dependency_normalize_delta_expression(
            deltaNode,
            deltaSign,
            &deltaNode,
            &deltaSign)) {
        return ZR_NULL;
    }
    return deltaNode;
}

SZrString *ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaIdentifier(SZrAstNode *right,
                                                                             SZrString *targetName) {
    SZrAstNode *deltaNode = ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaExpression(
            right,
            targetName);

    if (deltaNode == ZR_NULL ||
        deltaNode->type != ZR_AST_IDENTIFIER_LITERAL ||
        deltaNode->data.identifier.name == ZR_NULL ||
        ZrCore_String_Equal(deltaNode->data.identifier.name, targetName)) {
        return ZR_NULL;
    }

    return deltaNode->data.identifier.name;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaSign(SZrAstNode *right,
                                                                    SZrString *targetName,
                                                                    TZrInt32 *outSign) {
    SZrAstNode *deltaNode = ZR_NULL;
    TZrInt32 deltaSign = 0;

    if (outSign != ZR_NULL) {
        *outSign = 0;
    }
    if (outSign == ZR_NULL ||
        !loop_assignment_self_dependency_find_delta_expression(
                right,
                targetName,
                &deltaNode,
                &deltaSign) ||
        !loop_assignment_self_dependency_normalize_delta_expression(
                deltaNode,
                deltaSign,
                &deltaNode,
                &deltaSign) ||
        deltaNode == ZR_NULL ||
        deltaSign == 0) {
        return ZR_FALSE;
    }

    *outSign = deltaSign;
    return ZR_TRUE;
}

static TZrBool loop_assignment_self_dependency_target_array_contains(const SZrArray *targetNames,
                                                                     SZrString *name) {
    TZrSize index;

    if (targetNames == ZR_NULL || !targetNames->isValid || name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < targetNames->length; index++) {
        SZrString **target = (SZrString **)ZrCore_Array_Get((SZrArray *)targetNames, index);
        if (target != ZR_NULL &&
            *target != ZR_NULL &&
            ZrCore_String_Equal(*target, name)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool loop_assignment_self_dependency_expression_uses_any_target(SZrAstNode *node,
                                                                          const SZrArray *targetNames) {
    SZrAstNodeArray *members;

    if (node == ZR_NULL || targetNames == ZR_NULL || !targetNames->isValid) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            return loop_assignment_self_dependency_target_array_contains(
                    targetNames,
                    node->data.identifier.name);
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_BOOLEAN_LITERAL:
            return ZR_FALSE;
        case ZR_AST_BINARY_EXPRESSION:
            return loop_assignment_self_dependency_expression_uses_any_target(
                           node->data.binaryExpression.left,
                           targetNames) ||
                   loop_assignment_self_dependency_expression_uses_any_target(
                           node->data.binaryExpression.right,
                           targetNames);
        case ZR_AST_LOGICAL_EXPRESSION:
            return loop_assignment_self_dependency_expression_uses_any_target(
                           node->data.logicalExpression.left,
                           targetNames) ||
                   loop_assignment_self_dependency_expression_uses_any_target(
                           node->data.logicalExpression.right,
                           targetNames);
        case ZR_AST_UNARY_EXPRESSION:
            return loop_assignment_self_dependency_expression_uses_any_target(
                    node->data.unaryExpression.argument,
                    targetNames);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return loop_assignment_self_dependency_expression_uses_any_target(
                    node->data.typeCastExpression.expression,
                    targetNames);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return loop_assignment_self_dependency_expression_uses_any_target(
                           node->data.conditionalExpression.test,
                           targetNames) ||
                   loop_assignment_self_dependency_expression_uses_any_target(
                           node->data.conditionalExpression.consequent,
                           targetNames) ||
                   loop_assignment_self_dependency_expression_uses_any_target(
                           node->data.conditionalExpression.alternate,
                           targetNames);
        case ZR_AST_PRIMARY_EXPRESSION:
            members = node->data.primaryExpression.members;
            if (members != ZR_NULL && members->count > 0) {
                return ZR_TRUE;
            }
            return loop_assignment_self_dependency_expression_uses_any_target(
                    node->data.primaryExpression.property,
                    targetNames);
        default:
            return ZR_TRUE;
    }
}

TZrBool ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaUsesAnyTarget(
        SZrAstNode *right,
        SZrString *targetName,
        const SZrArray *targetNames) {
    SZrAstNode *deltaNode = ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaExpression(
            right,
            targetName);

    if (deltaNode == ZR_NULL || targetNames == ZR_NULL || !targetNames->isValid) {
        return ZR_FALSE;
    }
    return loop_assignment_self_dependency_expression_uses_any_target(deltaNode, targetNames);
}

TZrBool ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(SZrAstNode *node,
                                                                SZrString *name) {
    SZrAstNodeArray *members;

    if (node == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            return node->data.identifier.name != ZR_NULL &&
                   ZrCore_String_Equal(node->data.identifier.name, name);
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_BOOLEAN_LITERAL:
            return ZR_FALSE;
        case ZR_AST_BINARY_EXPRESSION:
            return ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                           node->data.binaryExpression.left,
                           name) ||
                   ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                           node->data.binaryExpression.right,
                           name);
        case ZR_AST_LOGICAL_EXPRESSION:
            return ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                           node->data.logicalExpression.left,
                           name) ||
                   ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                           node->data.logicalExpression.right,
                           name);
        case ZR_AST_UNARY_EXPRESSION:
            return ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                    node->data.unaryExpression.argument,
                    name);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                    node->data.typeCastExpression.expression,
                    name);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                           node->data.conditionalExpression.test,
                           name) ||
                   ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                           node->data.conditionalExpression.consequent,
                           name) ||
                   ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                           node->data.conditionalExpression.alternate,
                           name);
        case ZR_AST_PRIMARY_EXPRESSION:
            members = node->data.primaryExpression.members;
            if (members != ZR_NULL && members->count > 0) {
                return ZR_TRUE;
            }
            return ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                    node->data.primaryExpression.property,
                    name);
        default:
            return ZR_TRUE;
    }
}

TZrBool ZrParser_TypeInferenceLoopAssignment_WidenSelfDependentType(SZrCompilerState *cs,
                                                                    const SZrInferredType *sourceType,
                                                                    TZrInt64 deltaMin,
                                                                    TZrInt64 deltaMax,
                                                                    SZrInferredType *outType) {
    TZrInt64 oneIterationMax = 0;
    TZrInt64 oneIterationMin = 0;
    TZrBool widensUpper;
    TZrBool widensLower;
    TZrBool widensBoth;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        sourceType == ZR_NULL ||
        outType == ZR_NULL ||
        sourceType->baseType != ZR_VALUE_TYPE_INT64 ||
        !sourceType->hasRangeConstraint) {
        return ZR_FALSE;
    }

    widensUpper = deltaMin >= 0 && deltaMax > 0;
    widensLower = deltaMin < 0 && deltaMax <= 0;
    widensBoth = deltaMin < 0 && deltaMax > 0;

    if (deltaMin == 0 && deltaMax == 0) {
        ZrParser_InferredType_Copy(cs->state, outType, sourceType);
        return ZR_TRUE;
    }

    if (widensUpper &&
        !loop_assignment_self_dependency_int64_add(sourceType->minValue, deltaMin, &oneIterationMin)) {
        return ZR_FALSE;
    }

    if (widensLower &&
        !loop_assignment_self_dependency_int64_add(sourceType->maxValue, deltaMax, &oneIterationMax)) {
        return ZR_FALSE;
    }

    if (!(widensUpper || widensLower || widensBoth)) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Copy(cs->state, outType, sourceType);
    ZrParser_NumericRangeSegments_Free(cs->state,
                                       &outType->rangeSegmentCount,
                                       outType->rangeSegments,
                                       &outType->rangeExtraSegments);
    outType->baseType = ZR_VALUE_TYPE_INT64;
    outType->hasRangeConstraint = ZR_TRUE;
    if (widensBoth) {
        outType->minValue = ZR_TYPE_RANGE_INT64_MIN;
        outType->maxValue = ZR_TYPE_RANGE_INT64_MAX;
    } else if (widensUpper) {
        outType->minValue = oneIterationMin;
        outType->maxValue = ZR_TYPE_RANGE_INT64_MAX;
    } else {
        outType->minValue = ZR_TYPE_RANGE_INT64_MIN;
        outType->maxValue = oneIterationMax;
    }
    return ZR_TRUE;
}
