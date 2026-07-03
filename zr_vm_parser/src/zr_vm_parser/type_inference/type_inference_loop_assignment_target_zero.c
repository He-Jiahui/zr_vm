#include "type_inference_loop_assignment_target_zero.h"

#include <string.h>

#include "type_inference_loop_assignment_self_dependency.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_parser/compiler.h"

static TZrBool loop_assignment_target_zero_identifier_is_name(SZrAstNode *node,
                                                              SZrString *name) {
    return node != ZR_NULL &&
           name != ZR_NULL &&
           node->type == ZR_AST_IDENTIFIER_LITERAL &&
           node->data.identifier.name != ZR_NULL &&
           ZrCore_String_Equal(node->data.identifier.name, name);
}

static TZrBool loop_assignment_target_zero_node_is_binary_op(SZrAstNode *node,
                                                             const TZrChar *op) {
    return node != ZR_NULL &&
           op != ZR_NULL &&
           node->type == ZR_AST_BINARY_EXPRESSION &&
           node->data.binaryExpression.op.op != ZR_NULL &&
           strcmp(node->data.binaryExpression.op.op, op) == 0;
}

static TZrBool loop_assignment_target_zero_direct_self_canceling(SZrAstNode *node,
                                                                 SZrString *targetName) {
    return loop_assignment_target_zero_node_is_binary_op(node, "-") &&
           loop_assignment_target_zero_identifier_is_name(
                   node->data.binaryExpression.left,
                   targetName) &&
           loop_assignment_target_zero_identifier_is_name(
                   node->data.binaryExpression.right,
                   targetName);
}

static TZrBool loop_assignment_target_zero_inferred_integer_range(
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

static TZrBool loop_assignment_target_zero_identifier_range(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrString *targetName,
        TZrInt64 *outMin,
        TZrInt64 *outMax) {
    const SZrTypeBinding *binding;

    if (cs == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        node == ZR_NULL ||
        targetName == ZR_NULL ||
        outMin == ZR_NULL ||
        outMax == ZR_NULL ||
        node->type != ZR_AST_IDENTIFIER_LITERAL ||
        node->data.identifier.name == ZR_NULL ||
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

static TZrBool loop_assignment_target_zero_int64_add(TZrInt64 left,
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

static TZrBool loop_assignment_target_zero_subtract_int64(TZrInt64 left,
                                                          TZrInt64 right,
                                                          TZrInt64 *outValue) {
    if (right == ZR_TYPE_RANGE_INT64_MIN) {
        return ZR_FALSE;
    }
    return loop_assignment_target_zero_int64_add(left, -right, outValue);
}

static TZrBool loop_assignment_target_zero_binary_additive_range(
        SZrAstNode *node,
        TZrInt64 leftMin,
        TZrInt64 leftMax,
        TZrInt64 rightMin,
        TZrInt64 rightMax,
        TZrInt64 *outMin,
        TZrInt64 *outMax) {
    if (loop_assignment_target_zero_node_is_binary_op(node, "+")) {
        return loop_assignment_target_zero_int64_add(leftMin, rightMin, outMin) &&
               loop_assignment_target_zero_int64_add(leftMax, rightMax, outMax);
    }
    if (loop_assignment_target_zero_node_is_binary_op(node, "-")) {
        return loop_assignment_target_zero_subtract_int64(leftMin, rightMax, outMin) &&
               loop_assignment_target_zero_subtract_int64(leftMax, rightMin, outMax);
    }
    return ZR_FALSE;
}

static TZrBool loop_assignment_target_zero_non_target_integer_range(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrString *targetName,
        TZrInt64 *outMin,
        TZrInt64 *outMax) {
    if (node == ZR_NULL ||
        targetName == ZR_NULL ||
        ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(node, targetName)) {
        return ZR_FALSE;
    }
    return loop_assignment_target_zero_inferred_integer_range(cs, node, outMin, outMax);
}

static TZrBool loop_assignment_target_zero_range_is_zero(TZrInt64 minValue,
                                                         TZrInt64 maxValue) {
    return minValue == 0 && maxValue == 0;
}

static TZrBool loop_assignment_target_zero_product_range(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrString *targetName,
        TZrInt64 *outMin,
        TZrInt64 *outMax) {
    TZrInt64 leftMin;
    TZrInt64 leftMax;
    TZrInt64 rightMin;
    TZrInt64 rightMax;
    TZrInt64 factorMin;
    TZrInt64 factorMax;
    TZrBool leftKnown;
    TZrBool rightKnown;

    if (!loop_assignment_target_zero_node_is_binary_op(node, "*") ||
        outMin == ZR_NULL ||
        outMax == ZR_NULL) {
        return ZR_FALSE;
    }

    leftKnown = ZrParser_TypeInferenceLoopAssignment_TargetSelfCancelingRange(
            cs,
            node->data.binaryExpression.left,
            targetName,
            &leftMin,
            &leftMax);
    rightKnown = ZrParser_TypeInferenceLoopAssignment_TargetSelfCancelingRange(
            cs,
            node->data.binaryExpression.right,
            targetName,
            &rightMin,
            &rightMax);

    if (leftKnown &&
        loop_assignment_target_zero_range_is_zero(leftMin, leftMax) &&
        ((rightKnown && loop_assignment_target_zero_range_is_zero(rightMin, rightMax)) ||
         loop_assignment_target_zero_non_target_integer_range(
                 cs,
                 node->data.binaryExpression.right,
                 targetName,
                 &factorMin,
                 &factorMax))) {
        *outMin = 0;
        *outMax = 0;
        return ZR_TRUE;
    }

    if (rightKnown &&
        loop_assignment_target_zero_range_is_zero(rightMin, rightMax) &&
        ((leftKnown && loop_assignment_target_zero_range_is_zero(leftMin, leftMax)) ||
         loop_assignment_target_zero_non_target_integer_range(
                 cs,
                 node->data.binaryExpression.left,
                 targetName,
                 &factorMin,
                 &factorMax))) {
        *outMin = 0;
        *outMax = 0;
        return ZR_TRUE;
    }

    return loop_assignment_target_zero_non_target_integer_range(
            cs,
            node,
            targetName,
            outMin,
            outMax);
}

TZrBool ZrParser_TypeInferenceLoopAssignment_TargetSelfCancelingRange(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrString *targetName,
        TZrInt64 *outMin,
        TZrInt64 *outMax) {
    TZrInt64 leftMin;
    TZrInt64 leftMax;
    TZrInt64 rightMin;
    TZrInt64 rightMax;

    if (outMin != ZR_NULL) {
        *outMin = 0;
    }
    if (outMax != ZR_NULL) {
        *outMax = 0;
    }
    if (cs == ZR_NULL ||
        node == ZR_NULL ||
        targetName == ZR_NULL ||
        outMin == ZR_NULL ||
        outMax == ZR_NULL) {
        return ZR_FALSE;
    }

    if (node->type == ZR_AST_INTEGER_LITERAL) {
        *outMin = node->data.integerLiteral.value;
        *outMax = node->data.integerLiteral.value;
        return ZR_TRUE;
    }

    if (loop_assignment_target_zero_direct_self_canceling(node, targetName)) {
        *outMin = 0;
        *outMax = 0;
        return ZR_TRUE;
    }
    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        return loop_assignment_target_zero_identifier_range(
                cs,
                node,
                targetName,
                outMin,
                outMax);
    }

    if (loop_assignment_target_zero_node_is_binary_op(node, "*")) {
        return loop_assignment_target_zero_product_range(
                cs,
                node,
                targetName,
                outMin,
                outMax);
    }

    if ((loop_assignment_target_zero_node_is_binary_op(node, "+") ||
         loop_assignment_target_zero_node_is_binary_op(node, "-")) &&
        ZrParser_TypeInferenceLoopAssignment_TargetSelfCancelingRange(
                cs,
                node->data.binaryExpression.left,
                targetName,
                &leftMin,
                &leftMax) &&
        ZrParser_TypeInferenceLoopAssignment_TargetSelfCancelingRange(
                cs,
                node->data.binaryExpression.right,
                targetName,
                &rightMin,
                &rightMax)) {
        return loop_assignment_target_zero_binary_additive_range(
                node,
                leftMin,
                leftMax,
                rightMin,
                rightMax,
                outMin,
                outMax);
    }

    return loop_assignment_target_zero_non_target_integer_range(
            cs,
            node,
            targetName,
            outMin,
            outMax);
}

static TZrBool loop_assignment_target_zero_array_contains(const SZrArray *targetNames,
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

static TZrBool loop_assignment_target_zero_expression_uses_any_target(
        SZrAstNode *node,
        const SZrArray *targetNames) {
    SZrAstNodeArray *members;

    if (node == ZR_NULL || targetNames == ZR_NULL || !targetNames->isValid) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            return loop_assignment_target_zero_array_contains(
                    targetNames,
                    node->data.identifier.name);
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_BOOLEAN_LITERAL:
            return ZR_FALSE;
        case ZR_AST_BINARY_EXPRESSION:
            return loop_assignment_target_zero_expression_uses_any_target(
                           node->data.binaryExpression.left,
                           targetNames) ||
                   loop_assignment_target_zero_expression_uses_any_target(
                           node->data.binaryExpression.right,
                           targetNames);
        case ZR_AST_LOGICAL_EXPRESSION:
            return loop_assignment_target_zero_expression_uses_any_target(
                           node->data.logicalExpression.left,
                           targetNames) ||
                   loop_assignment_target_zero_expression_uses_any_target(
                           node->data.logicalExpression.right,
                           targetNames);
        case ZR_AST_UNARY_EXPRESSION:
            return loop_assignment_target_zero_expression_uses_any_target(
                    node->data.unaryExpression.argument,
                    targetNames);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return loop_assignment_target_zero_expression_uses_any_target(
                    node->data.typeCastExpression.expression,
                    targetNames);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return loop_assignment_target_zero_expression_uses_any_target(
                           node->data.conditionalExpression.test,
                           targetNames) ||
                   loop_assignment_target_zero_expression_uses_any_target(
                           node->data.conditionalExpression.consequent,
                           targetNames) ||
                   loop_assignment_target_zero_expression_uses_any_target(
                           node->data.conditionalExpression.alternate,
                           targetNames);
        case ZR_AST_PRIMARY_EXPRESSION:
            members = node->data.primaryExpression.members;
            if (members != ZR_NULL && members->count > 0) {
                return ZR_TRUE;
            }
            return loop_assignment_target_zero_expression_uses_any_target(
                    node->data.primaryExpression.property,
                    targetNames);
        default:
            return ZR_TRUE;
    }
}

static TZrBool loop_assignment_target_zero_syntactic_product(
        SZrAstNode *node,
        SZrString *targetName,
        const SZrArray *targetNames) {
    if (node == ZR_NULL || targetName == ZR_NULL || targetNames == ZR_NULL) {
        return ZR_FALSE;
    }

    if (loop_assignment_target_zero_direct_self_canceling(node, targetName)) {
        return ZR_TRUE;
    }

    if (loop_assignment_target_zero_node_is_binary_op(node, "*")) {
        SZrAstNode *left = node->data.binaryExpression.left;
        SZrAstNode *right = node->data.binaryExpression.right;

        if (loop_assignment_target_zero_syntactic_product(left, targetName, targetNames) &&
            !loop_assignment_target_zero_expression_uses_any_target(right, targetNames)) {
            return ZR_TRUE;
        }
        if (loop_assignment_target_zero_syntactic_product(right, targetName, targetNames) &&
            !loop_assignment_target_zero_expression_uses_any_target(left, targetNames)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_TargetSelfCancelingDeltaUsesNonCancelingTarget(
        SZrAstNode *deltaNode,
        SZrString *targetName,
        const SZrArray *targetNames) {
    SZrAstNodeArray *members;

    if (deltaNode == ZR_NULL ||
        targetName == ZR_NULL ||
        targetNames == ZR_NULL ||
        !targetNames->isValid) {
        return ZR_FALSE;
    }

    if (loop_assignment_target_zero_syntactic_product(deltaNode, targetName, targetNames)) {
        return ZR_FALSE;
    }

    switch (deltaNode->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            return loop_assignment_target_zero_array_contains(
                    targetNames,
                    deltaNode->data.identifier.name);
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_BOOLEAN_LITERAL:
            return ZR_FALSE;
        case ZR_AST_BINARY_EXPRESSION:
            return ZrParser_TypeInferenceLoopAssignment_TargetSelfCancelingDeltaUsesNonCancelingTarget(
                           deltaNode->data.binaryExpression.left,
                           targetName,
                           targetNames) ||
                   ZrParser_TypeInferenceLoopAssignment_TargetSelfCancelingDeltaUsesNonCancelingTarget(
                           deltaNode->data.binaryExpression.right,
                           targetName,
                           targetNames);
        case ZR_AST_LOGICAL_EXPRESSION:
            return ZrParser_TypeInferenceLoopAssignment_TargetSelfCancelingDeltaUsesNonCancelingTarget(
                           deltaNode->data.logicalExpression.left,
                           targetName,
                           targetNames) ||
                   ZrParser_TypeInferenceLoopAssignment_TargetSelfCancelingDeltaUsesNonCancelingTarget(
                           deltaNode->data.logicalExpression.right,
                           targetName,
                           targetNames);
        case ZR_AST_UNARY_EXPRESSION:
            return ZrParser_TypeInferenceLoopAssignment_TargetSelfCancelingDeltaUsesNonCancelingTarget(
                    deltaNode->data.unaryExpression.argument,
                    targetName,
                    targetNames);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return ZrParser_TypeInferenceLoopAssignment_TargetSelfCancelingDeltaUsesNonCancelingTarget(
                    deltaNode->data.typeCastExpression.expression,
                    targetName,
                    targetNames);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return ZrParser_TypeInferenceLoopAssignment_TargetSelfCancelingDeltaUsesNonCancelingTarget(
                           deltaNode->data.conditionalExpression.test,
                           targetName,
                           targetNames) ||
                   ZrParser_TypeInferenceLoopAssignment_TargetSelfCancelingDeltaUsesNonCancelingTarget(
                           deltaNode->data.conditionalExpression.consequent,
                           targetName,
                           targetNames) ||
                   ZrParser_TypeInferenceLoopAssignment_TargetSelfCancelingDeltaUsesNonCancelingTarget(
                           deltaNode->data.conditionalExpression.alternate,
                           targetName,
                           targetNames);
        case ZR_AST_PRIMARY_EXPRESSION:
            members = deltaNode->data.primaryExpression.members;
            if (members != ZR_NULL && members->count > 0) {
                return ZR_TRUE;
            }
            return ZrParser_TypeInferenceLoopAssignment_TargetSelfCancelingDeltaUsesNonCancelingTarget(
                    deltaNode->data.primaryExpression.property,
                    targetName,
                    targetNames);
        default:
            return ZR_TRUE;
    }
}
