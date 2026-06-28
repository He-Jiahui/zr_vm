#include "type_inference_loop_assignment_sequence_symbolic_coefficient.h"

#include <string.h>

#include "zr_vm_parser/type_inference.h"

static TZrBool loop_assignment_sequence_symbolic_coefficient_string_equal(
        const TZrChar *left,
        const TZrChar *right) {
    return left != ZR_NULL && right != ZR_NULL && strcmp(left, right) == 0;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicInferInt64Range(
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

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsBinaryMultiply(
        SZrAstNode *node) {
    return node != ZR_NULL &&
           node->type == ZR_AST_BINARY_EXPRESSION &&
           loop_assignment_sequence_symbolic_coefficient_string_equal(
                   node->data.binaryExpression.op.op,
                   "*");
}

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsIdentifier(
        SZrAstNode *node) {
    return node != ZR_NULL && node->type == ZR_AST_IDENTIFIER_LITERAL;
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_range_crosses_zero(
        TZrInt64 minValue,
        TZrInt64 maxValue) {
    return minValue < 0 && maxValue > 0;
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_crossing_identifier(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    TZrInt64 minValue = 0;
    TZrInt64 maxValue = 0;

    return ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsIdentifier(node) &&
           ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicInferInt64Range(
                   cs,
                   node,
                   &minValue,
                   &maxValue) &&
           loop_assignment_sequence_symbolic_coefficient_range_crosses_zero(minValue, maxValue);
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_sign_crossing_scale_identifier(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    return loop_assignment_sequence_symbolic_coefficient_is_crossing_identifier(cs, node);
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_supported_scale_identifier(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    TZrInt64 minValue = 0;
    TZrInt64 maxValue = 0;

    return ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsIdentifier(node) &&
           ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicInferInt64Range(
                   cs,
                   node,
                   &minValue,
                   &maxValue) &&
           maxValue >= minValue &&
           (minValue > 0 ||
            (minValue == 0 && maxValue > 0) ||
            maxValue < 0 ||
            (minValue < 0 && maxValue == 0) ||
            loop_assignment_sequence_symbolic_coefficient_range_crosses_zero(
                    minValue,
                    maxValue));
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_identifier(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    TZrInt64 minValue = 0;
    TZrInt64 maxValue = 0;

    return ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsIdentifier(node) &&
           ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicInferInt64Range(
                   cs,
                   node,
                   &minValue,
                   &maxValue) &&
           maxValue >= minValue &&
           (minValue > 0 ||
            (minValue == 0 && maxValue > 0) ||
            maxValue < 0 ||
            (minValue < 0 && maxValue == 0) ||
            loop_assignment_sequence_symbolic_coefficient_range_crosses_zero(
                    minValue,
                    maxValue));
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_positive_singleton_scale_identifier(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    TZrInt64 minValue = 0;
    TZrInt64 maxValue = 0;

    return ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsIdentifier(node) &&
           ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicInferInt64Range(
                   cs,
                   node,
                   &minValue,
                   &maxValue) &&
           minValue == maxValue &&
           minValue > 0;
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_positive_scale_identifier(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    TZrInt64 minValue = 0;
    TZrInt64 maxValue = 0;

    return ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsIdentifier(node) &&
           ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicInferInt64Range(
                   cs,
                   node,
                   &minValue,
                   &maxValue) &&
           maxValue >= minValue &&
           minValue > 0;
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_nonzero_possible_scale_extension_identifier(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    TZrInt64 minValue = 0;
    TZrInt64 maxValue = 0;

    return ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsIdentifier(node) &&
           ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicInferInt64Range(
                   cs,
                   node,
                   &minValue,
                   &maxValue) &&
           maxValue >= minValue &&
           (minValue != 0 || maxValue != 0);
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_negative_scale_identifier(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    TZrInt64 minValue = 0;
    TZrInt64 maxValue = 0;

    return ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsIdentifier(node) &&
           ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicInferInt64Range(
                   cs,
                   node,
                   &minValue,
                   &maxValue) &&
           maxValue >= minValue &&
           maxValue < 0;
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_supported_scale_extension_identifier(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    TZrInt64 minValue = 0;
    TZrInt64 maxValue = 0;

    return ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsIdentifier(node) &&
           ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicInferInt64Range(
                   cs,
                   node,
                   &minValue,
                   &maxValue) &&
           maxValue >= minValue &&
           (minValue > 0 ||
            (minValue == 0 && maxValue > 0) ||
            (minValue < 0 && maxValue == 0) ||
            maxValue < 0);
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_zero_inclusive_positive_scale_identifier(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    TZrInt64 minValue = 0;
    TZrInt64 maxValue = 0;

    return ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsIdentifier(node) &&
           ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicInferInt64Range(
                   cs,
                   node,
                   &minValue,
                   &maxValue) &&
           minValue == 0 &&
           maxValue > 0;
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_positive_possible_scale_identifier(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    return loop_assignment_sequence_symbolic_coefficient_is_positive_scale_identifier(
                   cs,
                   node) ||
           loop_assignment_sequence_symbolic_coefficient_is_zero_inclusive_positive_scale_identifier(
                   cs,
                   node);
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_zero_inclusive_negative_scale_identifier(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    TZrInt64 minValue = 0;
    TZrInt64 maxValue = 0;

    return ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsIdentifier(node) &&
           ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicInferInt64Range(
                   cs,
                   node,
                   &minValue,
                   &maxValue) &&
           minValue < 0 &&
           maxValue == 0;
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_positive_possible_or_zero_inclusive_negative_scale_identifier(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    return loop_assignment_sequence_symbolic_coefficient_is_positive_possible_scale_identifier(
                   cs,
                   node) ||
           loop_assignment_sequence_symbolic_coefficient_is_zero_inclusive_negative_scale_identifier(
                   cs,
                   node);
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_nonzero_non_crossing_scale_identifier(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    return loop_assignment_sequence_symbolic_coefficient_is_positive_possible_or_zero_inclusive_negative_scale_identifier(
                   cs,
                   node) ||
           loop_assignment_sequence_symbolic_coefficient_is_negative_scale_identifier(
                   cs,
                   node);
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_nonzero_possible_extra_extra_scale_identifier(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    return loop_assignment_sequence_symbolic_coefficient_is_nonzero_non_crossing_scale_identifier(
                   cs,
                   node) ||
           loop_assignment_sequence_symbolic_coefficient_is_sign_crossing_scale_identifier(
                   cs,
                   node);
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_nonzero_possible_deeper_scale_extension_identifier(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    return loop_assignment_sequence_symbolic_coefficient_is_nonzero_possible_scale_extension_identifier(
            cs,
            node);
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_direct_positive_bounded_scale_product(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    SZrAstNode *left;
    SZrAstNode *right;
    TZrBool hasPositiveSingletonScale;
    TZrBool hasDoubleZeroInclusivePositiveScale;
    TZrBool hasDoubleZeroInclusiveNegativeScale;
    TZrBool hasDoubleNegativeScaleProduct;
    TZrBool hasDoubleSignCrossingScaleProduct;
    TZrBool hasMixedZeroInclusivePositiveSignCrossingScaleProduct;
    TZrBool hasMixedZeroInclusiveNegativeSignCrossingScaleProduct;
    TZrBool hasMixedZeroInclusivePositiveNegativeScaleProduct;

    if (!ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsBinaryMultiply(node)) {
        return ZR_FALSE;
    }

    left = node->data.binaryExpression.left;
    right = node->data.binaryExpression.right;
    if (!loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_identifier(
                cs,
                left) ||
        !loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_identifier(
                cs,
                right)) {
        return ZR_FALSE;
    }

    hasPositiveSingletonScale =
            loop_assignment_sequence_symbolic_coefficient_is_positive_singleton_scale_identifier(
                    cs,
                    left) ||
            loop_assignment_sequence_symbolic_coefficient_is_positive_singleton_scale_identifier(
                    cs,
                    right);
    hasDoubleZeroInclusivePositiveScale =
            loop_assignment_sequence_symbolic_coefficient_is_zero_inclusive_positive_scale_identifier(
                    cs,
                    left) &&
            loop_assignment_sequence_symbolic_coefficient_is_zero_inclusive_positive_scale_identifier(
                    cs,
                    right);
    hasDoubleZeroInclusiveNegativeScale =
            loop_assignment_sequence_symbolic_coefficient_is_zero_inclusive_negative_scale_identifier(
                    cs,
                    left) &&
            loop_assignment_sequence_symbolic_coefficient_is_zero_inclusive_negative_scale_identifier(
                    cs,
                    right);
    hasDoubleNegativeScaleProduct =
            loop_assignment_sequence_symbolic_coefficient_is_negative_scale_identifier(
                    cs,
                    left) &&
            loop_assignment_sequence_symbolic_coefficient_is_negative_scale_identifier(
                    cs,
                    right);
    hasDoubleSignCrossingScaleProduct =
            loop_assignment_sequence_symbolic_coefficient_is_sign_crossing_scale_identifier(
                    cs,
                    left) &&
            loop_assignment_sequence_symbolic_coefficient_is_sign_crossing_scale_identifier(
                    cs,
                    right);
    hasMixedZeroInclusivePositiveSignCrossingScaleProduct =
            (loop_assignment_sequence_symbolic_coefficient_is_zero_inclusive_positive_scale_identifier(
                     cs,
                     left) &&
             loop_assignment_sequence_symbolic_coefficient_is_sign_crossing_scale_identifier(
                     cs,
                     right)) ||
            (loop_assignment_sequence_symbolic_coefficient_is_sign_crossing_scale_identifier(
                     cs,
                     left) &&
             loop_assignment_sequence_symbolic_coefficient_is_zero_inclusive_positive_scale_identifier(
                     cs,
                     right));
    hasMixedZeroInclusiveNegativeSignCrossingScaleProduct =
            (loop_assignment_sequence_symbolic_coefficient_is_zero_inclusive_negative_scale_identifier(
                     cs,
                     left) &&
             loop_assignment_sequence_symbolic_coefficient_is_sign_crossing_scale_identifier(
                     cs,
                     right)) ||
            (loop_assignment_sequence_symbolic_coefficient_is_sign_crossing_scale_identifier(
                     cs,
                     left) &&
             loop_assignment_sequence_symbolic_coefficient_is_zero_inclusive_negative_scale_identifier(
                     cs,
                     right));
    hasMixedZeroInclusivePositiveNegativeScaleProduct =
            (loop_assignment_sequence_symbolic_coefficient_is_zero_inclusive_positive_scale_identifier(
                     cs,
                     left) &&
             loop_assignment_sequence_symbolic_coefficient_is_zero_inclusive_negative_scale_identifier(
                     cs,
                     right)) ||
            (loop_assignment_sequence_symbolic_coefficient_is_zero_inclusive_negative_scale_identifier(
                     cs,
                     left) &&
             loop_assignment_sequence_symbolic_coefficient_is_zero_inclusive_positive_scale_identifier(
                     cs,
                     right));

    return hasPositiveSingletonScale ||
           hasDoubleZeroInclusivePositiveScale ||
           hasDoubleZeroInclusiveNegativeScale ||
           hasDoubleNegativeScaleProduct ||
           hasDoubleSignCrossingScaleProduct ||
           hasMixedZeroInclusivePositiveSignCrossingScaleProduct ||
           hasMixedZeroInclusiveNegativeSignCrossingScaleProduct ||
           hasMixedZeroInclusivePositiveNegativeScaleProduct;
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_extension(
        SZrCompilerState *cs,
        SZrAstNode *node,
        TZrSize remainingScaleExtensions) {
    SZrAstNode *left;
    SZrAstNode *right;

    if (loop_assignment_sequence_symbolic_coefficient_is_direct_positive_bounded_scale_product(
                cs,
                node)) {
        return ZR_TRUE;
    }
    if (remainingScaleExtensions == 0 ||
        !ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsBinaryMultiply(node)) {
        return ZR_FALSE;
    }

    left = node->data.binaryExpression.left;
    right = node->data.binaryExpression.right;
    return (loop_assignment_sequence_symbolic_coefficient_is_supported_scale_extension_identifier(
                    cs,
                    left) &&
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_extension(
                    cs,
                    right,
                    remainingScaleExtensions - 1)) ||
           (loop_assignment_sequence_symbolic_coefficient_is_supported_scale_extension_identifier(
                    cs,
                    right) &&
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_extension(
                    cs,
                    left,
                    remainingScaleExtensions - 1));
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extension(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    SZrAstNode *left;
    SZrAstNode *right;

    if (!ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsBinaryMultiply(node)) {
        return ZR_FALSE;
    }

    left = node->data.binaryExpression.left;
    right = node->data.binaryExpression.right;
    return (loop_assignment_sequence_symbolic_coefficient_is_nonzero_possible_scale_extension_identifier(
                    cs,
                    left) &&
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_extension(
                    cs,
                    right,
                    1)) ||
           (loop_assignment_sequence_symbolic_coefficient_is_nonzero_possible_scale_extension_identifier(
                    cs,
                    right) &&
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_extension(
                    cs,
                    left,
                    1));
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_deeper_extension(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    SZrAstNode *left;
    SZrAstNode *right;

    if (!ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsBinaryMultiply(node)) {
        return ZR_FALSE;
    }

    left = node->data.binaryExpression.left;
    right = node->data.binaryExpression.right;
    return (loop_assignment_sequence_symbolic_coefficient_is_nonzero_possible_deeper_scale_extension_identifier(
                    cs,
                    left) &&
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extension(
                    cs,
                    right)) ||
           (loop_assignment_sequence_symbolic_coefficient_is_nonzero_possible_deeper_scale_extension_identifier(
                    cs,
                    right) &&
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extension(
                    cs,
                    left));
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extra_deeper_extension(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    SZrAstNode *left;
    SZrAstNode *right;

    if (!ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsBinaryMultiply(node)) {
        return ZR_FALSE;
    }

    left = node->data.binaryExpression.left;
    right = node->data.binaryExpression.right;
    return (loop_assignment_sequence_symbolic_coefficient_is_nonzero_possible_scale_extension_identifier(
                   cs,
                   left) &&
           loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_deeper_extension(
                   cs,
                   right)) ||
          (loop_assignment_sequence_symbolic_coefficient_is_nonzero_possible_scale_extension_identifier(
                   cs,
                   right) &&
           loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_deeper_extension(
                   cs,
                    left));
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extra_extra_deeper_extension(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    SZrAstNode *left;
    SZrAstNode *right;

    if (!ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsBinaryMultiply(node)) {
        return ZR_FALSE;
    }

    left = node->data.binaryExpression.left;
    right = node->data.binaryExpression.right;
    return (loop_assignment_sequence_symbolic_coefficient_is_nonzero_possible_extra_extra_scale_identifier(
                    cs,
                    left) &&
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extra_deeper_extension(
                    cs,
                    right)) ||
           (loop_assignment_sequence_symbolic_coefficient_is_nonzero_possible_extra_extra_scale_identifier(
                    cs,
                    right) &&
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extra_deeper_extension(
                    cs,
                    left));
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_positive_singleton_extra_extra_extra_deeper_extension(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    SZrAstNode *left;
    SZrAstNode *right;

    if (!ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsBinaryMultiply(node)) {
        return ZR_FALSE;
    }

    left = node->data.binaryExpression.left;
    right = node->data.binaryExpression.right;
    return (loop_assignment_sequence_symbolic_coefficient_is_positive_singleton_scale_identifier(
                    cs,
                    left) &&
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extra_extra_deeper_extension(
                    cs,
                    right)) ||
           (loop_assignment_sequence_symbolic_coefficient_is_positive_singleton_scale_identifier(
                    cs,
                    right) &&
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extra_extra_deeper_extension(
                    cs,
                    left));
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_positive_extra_extra_extra_deeper_extension(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    SZrAstNode *left;
    SZrAstNode *right;

    if (!ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsBinaryMultiply(node)) {
        return ZR_FALSE;
    }

    left = node->data.binaryExpression.left;
    right = node->data.binaryExpression.right;
    return (loop_assignment_sequence_symbolic_coefficient_is_positive_scale_identifier(
                    cs,
                    left) &&
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extra_extra_deeper_extension(
                    cs,
                    right)) ||
           (loop_assignment_sequence_symbolic_coefficient_is_positive_scale_identifier(
                    cs,
                    right) &&
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extra_extra_deeper_extension(
                    cs,
                    left));
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_positive_possible_extra_extra_extra_deeper_extension(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    SZrAstNode *left;
    SZrAstNode *right;

    if (!ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsBinaryMultiply(node)) {
        return ZR_FALSE;
    }

    left = node->data.binaryExpression.left;
    right = node->data.binaryExpression.right;
    return (loop_assignment_sequence_symbolic_coefficient_is_positive_possible_scale_identifier(
                    cs,
                    left) &&
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extra_extra_deeper_extension(
                    cs,
                    right)) ||
           (loop_assignment_sequence_symbolic_coefficient_is_positive_possible_scale_identifier(
                    cs,
                    right) &&
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extra_extra_deeper_extension(
                    cs,
                    left));
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_positive_possible_or_zero_inclusive_negative_extra_extra_extra_deeper_extension(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    SZrAstNode *left;
    SZrAstNode *right;

    if (!ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsBinaryMultiply(node)) {
        return ZR_FALSE;
    }

    left = node->data.binaryExpression.left;
    right = node->data.binaryExpression.right;
    return (loop_assignment_sequence_symbolic_coefficient_is_positive_possible_or_zero_inclusive_negative_scale_identifier(
                    cs,
                    left) &&
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extra_extra_deeper_extension(
                    cs,
                    right)) ||
           (loop_assignment_sequence_symbolic_coefficient_is_positive_possible_or_zero_inclusive_negative_scale_identifier(
                    cs,
                    right) &&
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extra_extra_deeper_extension(
                    cs,
                    left));
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_non_crossing_extra_extra_extra_deeper_extension(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    SZrAstNode *left;
    SZrAstNode *right;

    if (!ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsBinaryMultiply(node)) {
        return ZR_FALSE;
    }

    left = node->data.binaryExpression.left;
    right = node->data.binaryExpression.right;
    return (loop_assignment_sequence_symbolic_coefficient_is_nonzero_non_crossing_scale_identifier(
                    cs,
                    left) &&
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extra_extra_deeper_extension(
                    cs,
                    right)) ||
           (loop_assignment_sequence_symbolic_coefficient_is_nonzero_non_crossing_scale_identifier(
                    cs,
                    right) &&
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extra_extra_deeper_extension(
                    cs,
                    left));
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extra_extra_extra_deeper_extension(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    SZrAstNode *left;
    SZrAstNode *right;

    if (!ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsBinaryMultiply(node)) {
        return ZR_FALSE;
    }

    left = node->data.binaryExpression.left;
    right = node->data.binaryExpression.right;
    return (loop_assignment_sequence_symbolic_coefficient_is_nonzero_possible_extra_extra_scale_identifier(
                    cs,
                    left) &&
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extra_extra_deeper_extension(
                    cs,
                    right)) ||
           (loop_assignment_sequence_symbolic_coefficient_is_nonzero_possible_extra_extra_scale_identifier(
                    cs,
                    right) &&
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extra_extra_deeper_extension(
                    cs,
                    left));
}

static TZrBool loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    return loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_extension(
                   cs,
                   node,
                   1) ||
           loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extension(
                   cs,
                   node) ||
          loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_deeper_extension(
                  cs,
                  node) ||
          loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extra_deeper_extension(
                  cs,
                  node) ||
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extra_extra_deeper_extension(
                    cs,
                    node) ||
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_positive_singleton_extra_extra_extra_deeper_extension(
                    cs,
                    node) ||
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_positive_extra_extra_extra_deeper_extension(
                    cs,
                    node) ||
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_positive_possible_extra_extra_extra_deeper_extension(
                    cs,
                    node) ||
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_positive_possible_or_zero_inclusive_negative_extra_extra_extra_deeper_extension(
                    cs,
                    node) ||
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_non_crossing_extra_extra_extra_deeper_extension(
                    cs,
                    node) ||
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product_with_nonzero_possible_extra_extra_extra_deeper_extension(
                    cs,
                    node);
}

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsSupportedCrossingCoefficient(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    SZrAstNode *left;
    SZrAstNode *right;

    if (ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsIdentifier(node)) {
        return ZR_TRUE;
    }
    if (!ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsBinaryMultiply(node)) {
        return ZR_FALSE;
    }

    left = node->data.binaryExpression.left;
    right = node->data.binaryExpression.right;
    if (!ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsBinaryMultiply(left) &&
        !ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsBinaryMultiply(right)) {
        return (loop_assignment_sequence_symbolic_coefficient_is_crossing_identifier(cs, left) &&
                loop_assignment_sequence_symbolic_coefficient_is_supported_scale_identifier(cs, right)) ||
               (loop_assignment_sequence_symbolic_coefficient_is_supported_scale_identifier(cs, left) &&
                loop_assignment_sequence_symbolic_coefficient_is_crossing_identifier(cs, right));
    }

    return (loop_assignment_sequence_symbolic_coefficient_is_crossing_identifier(cs, left) &&
            loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product(cs, right)) ||
           (loop_assignment_sequence_symbolic_coefficient_is_bounded_scale_product(cs, left) &&
            loop_assignment_sequence_symbolic_coefficient_is_crossing_identifier(cs, right));
}
