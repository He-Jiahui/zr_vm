#include "type_inference_loop_assignment_sequence_symbolic_delta.h"

#include <string.h>

#include "type_inference_loop_assignment_self_dependency.h"
#include "type_inference_loop_assignment_sequence_symbolic_coefficient.h"
#include "type_inference_loop_assignment_sequence_symbolic_delta_terms.h"
#include "type_inference_loop_assignment_symbolic_math.h"

#include "zr_vm_common/zr_parser_conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_parser/compiler.h"

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicDeltaCurrentRange(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentSequenceDeltaTracker *tracker,
        TZrInt64 *outMin,
        TZrInt64 *outMax) {
    TZrInt64 minValue;
    TZrInt64 maxValue;
    TZrSize index;

    if (cs == ZR_NULL ||
        tracker == ZR_NULL ||
        outMin == ZR_NULL ||
        outMax == ZR_NULL ||
        !tracker->canTrackSymbolicDelta) {
        return ZR_FALSE;
    }

    minValue = tracker->symbolicResidualIntegerLiteralSum;
    maxValue = tracker->symbolicResidualIntegerLiteralSum;
    for (index = 0; index < tracker->symbolicResidualTermCount; index++) {
        const SZrTypeInferenceLoopAssignmentSequenceSymbolicTerm *term =
                &tracker->symbolicResidualTerms[index];
        TZrInt64 termMin;
        TZrInt64 termMax;
        TZrInt64 nextMin;
        TZrInt64 nextMax;

        if (!ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicInferInt64Range(
                    cs,
                    term->node,
                    &termMin,
                    &termMax) ||
            (term->hasCoefficientRange &&
             !loop_assignment_sequence_symbolic_int64_range_mul(
                     termMin,
                     termMax,
                     term->coefficientMin,
                     term->coefficientMax,
                     &termMin,
                     &termMax)) ||
            !loop_assignment_sequence_symbolic_add_signed_range(
                    minValue,
                    maxValue,
                    termMin,
                    termMax,
                    term->sign,
                    &nextMin,
                    &nextMax)) {
            return ZR_FALSE;
        }
        minValue = nextMin;
        maxValue = nextMax;
    }

    *outMin = minValue;
    *outMax = maxValue;
    return ZR_TRUE;
}

static TZrBool loop_assignment_sequence_symbolic_string_equal(const TZrChar *left,
                                                              const TZrChar *right) {
    return left != ZR_NULL && right != ZR_NULL && strcmp(left, right) == 0;
}

static TZrBool loop_assignment_sequence_symbolic_signed_integer_literal_add(
        TZrInt64 currentSum,
        TZrInt64 literalValue,
        TZrInt32 sign,
        TZrInt64 *outSum) {
    TZrInt64 signedValue;

    if (outSum == ZR_NULL ||
        (sign != 1 && sign != -1)) {
        return ZR_FALSE;
    }

    if (sign < 0) {
        if (literalValue == ZR_TYPE_RANGE_INT64_MIN) {
            return ZR_FALSE;
        }
        signedValue = -literalValue;
    } else {
        signedValue = literalValue;
    }

    return loop_assignment_sequence_symbolic_int64_add(currentSum, signedValue, outSum);
}

static TZrBool loop_assignment_sequence_symbolic_delta_collect_terms(
        SZrCompilerState *cs,
        SZrString *sequenceName,
        SZrAstNode *node,
        TZrInt32 sign,
        SZrTypeInferenceLoopAssignmentSequenceSymbolicTerm *terms,
        TZrSize *rawTermCount,
        TZrSize *termCount,
        TZrInt64 *integerLiteralSum);

static TZrBool loop_assignment_sequence_symbolic_delta_literal_coefficient(
        SZrAstNode *node,
        TZrInt64 *outValue) {
    TZrInt64 value;
    TZrInt64 leftValue;
    TZrInt64 rightValue;

    if (node == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (node->type == ZR_AST_INTEGER_LITERAL) {
        *outValue = node->data.integerLiteral.value;
        return ZR_TRUE;
    }

    if (node->type == ZR_AST_UNARY_EXPRESSION &&
        node->data.unaryExpression.op.op != ZR_NULL &&
        loop_assignment_sequence_symbolic_delta_literal_coefficient(
                node->data.unaryExpression.argument,
                &value)) {
        if (loop_assignment_sequence_symbolic_string_equal(node->data.unaryExpression.op.op, "+")) {
            *outValue = value;
            return ZR_TRUE;
        }
        if (loop_assignment_sequence_symbolic_string_equal(node->data.unaryExpression.op.op, "-") &&
            value != ZR_TYPE_RANGE_INT64_MIN) {
            *outValue = -value;
            return ZR_TRUE;
        }
    }

    if (node->type == ZR_AST_BINARY_EXPRESSION &&
        node->data.binaryExpression.op.op != ZR_NULL &&
        loop_assignment_sequence_symbolic_delta_literal_coefficient(
                node->data.binaryExpression.left,
                &leftValue) &&
        loop_assignment_sequence_symbolic_delta_literal_coefficient(
                node->data.binaryExpression.right,
                &rightValue)) {
        if (loop_assignment_sequence_symbolic_string_equal(
                    node->data.binaryExpression.op.op,
                    "+")) {
            return loop_assignment_sequence_symbolic_int64_add(
                    leftValue,
                    rightValue,
                    outValue);
        }
        if (loop_assignment_sequence_symbolic_string_equal(
                    node->data.binaryExpression.op.op,
                    "-") &&
            rightValue != ZR_TYPE_RANGE_INT64_MIN) {
            return loop_assignment_sequence_symbolic_int64_add(
                    leftValue,
                    -rightValue,
                    outValue);
        }
        if (loop_assignment_sequence_symbolic_string_equal(
                    node->data.binaryExpression.op.op,
                    "*")) {
            return loop_assignment_sequence_symbolic_int64_mul(
                    leftValue,
                    rightValue,
                    outValue);
        }
        if (loop_assignment_sequence_symbolic_string_equal(
                    node->data.binaryExpression.op.op,
                    "/")) {
            return loop_assignment_sequence_symbolic_int64_div(
                    leftValue,
                    rightValue,
                    outValue);
        }
        if (loop_assignment_sequence_symbolic_string_equal(
                    node->data.binaryExpression.op.op,
                    "%")) {
            return loop_assignment_sequence_symbolic_int64_mod(
                    leftValue,
                    rightValue,
                    outValue);
        }
        if (loop_assignment_sequence_symbolic_string_equal(
                    node->data.binaryExpression.op.op,
                    "&")) {
            return loop_assignment_sequence_symbolic_int64_bitwise_and(
                    leftValue,
                    rightValue,
                    outValue);
        }
        if (loop_assignment_sequence_symbolic_string_equal(
                    node->data.binaryExpression.op.op,
                    "|")) {
            return loop_assignment_sequence_symbolic_int64_bitwise_or(
                    leftValue,
                    rightValue,
                    outValue);
        }
        if (loop_assignment_sequence_symbolic_string_equal(
                    node->data.binaryExpression.op.op,
                    "^")) {
            return loop_assignment_sequence_symbolic_int64_bitwise_xor(
                    leftValue,
                    rightValue,
                    outValue);
        }
        if (loop_assignment_sequence_symbolic_string_equal(
                    node->data.binaryExpression.op.op,
                    "<<")) {
            return loop_assignment_sequence_symbolic_int64_shift_left(
                    leftValue,
                    rightValue,
                    outValue);
        }
        if (loop_assignment_sequence_symbolic_string_equal(
                    node->data.binaryExpression.op.op,
                    ">>")) {
            return loop_assignment_sequence_symbolic_int64_shift_right(
                    leftValue,
                    rightValue,
                    outValue);
        }
    }

    return ZR_FALSE;
}

static TZrBool loop_assignment_sequence_symbolic_delta_singleton_coefficient(
        SZrCompilerState *cs,
        SZrString *sequenceName,
        SZrAstNode *node,
        TZrInt64 *outValue) {
    TZrInt64 minValue;
    TZrInt64 maxValue;

    if (node == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if (loop_assignment_sequence_symbolic_delta_literal_coefficient(node, outValue)) {
        return ZR_TRUE;
    }
    if (cs == ZR_NULL ||
        sequenceName == ZR_NULL ||
        ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(node, sequenceName) ||
        !ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicInferInt64Range(
                cs,
                node,
                &minValue,
                &maxValue) ||
        minValue != maxValue) {
        return ZR_FALSE;
    }

    *outValue = minValue;
    return ZR_TRUE;
}

static TZrBool loop_assignment_sequence_symbolic_delta_range_coefficient(
        SZrCompilerState *cs,
        SZrString *sequenceName,
        SZrAstNode *node,
        TZrInt64 *outMin,
        TZrInt64 *outMax) {
    TZrInt64 minValue;
    TZrInt64 maxValue;

    if (cs == ZR_NULL ||
        node == ZR_NULL ||
        sequenceName == ZR_NULL ||
        outMin == ZR_NULL ||
        outMax == ZR_NULL ||
        ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(node, sequenceName) ||
        !ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicInferInt64Range(
                cs,
                node,
                &minValue,
                &maxValue) ||
        maxValue < minValue) {
        return ZR_FALSE;
    }

    *outMin = minValue;
    *outMax = maxValue;
    return ZR_TRUE;
}

typedef struct SZrTypeInferenceLoopAssignmentSequenceCoefficientExpansion {
    TZrInt64 exactTermCount;
    TZrInt32 exactTermSign;
    TZrInt64 residualMax;
    TZrInt32 residualSign;
    TZrInt64 selectionScore;
    TZrBool crossesZero;
} SZrTypeInferenceLoopAssignmentSequenceCoefficientExpansion;

static TZrBool loop_assignment_sequence_symbolic_delta_coefficient_expansion(
        TZrInt64 coefficientMin,
        TZrInt64 coefficientMax,
        SZrTypeInferenceLoopAssignmentSequenceCoefficientExpansion *outExpansion) {
    TZrInt64 magnitudeMin;
    TZrInt64 magnitudeMax;
    TZrInt64 residualMax;

    if (outExpansion == ZR_NULL ||
        coefficientMax < coefficientMin) {
        return ZR_FALSE;
    }

    memset(outExpansion, 0, sizeof(*outExpansion));
    if (coefficientMin >= 0) {
        outExpansion->exactTermCount = coefficientMin;
        outExpansion->exactTermSign = 1;
        outExpansion->residualMax = coefficientMax - coefficientMin;
        outExpansion->residualSign = 1;
        outExpansion->selectionScore = coefficientMin;
        return ZR_TRUE;
    }

    if (coefficientMin == ZR_TYPE_RANGE_INT64_MIN) {
        return ZR_FALSE;
    }

    if (coefficientMax <= 0) {
        magnitudeMin = -coefficientMax;
        magnitudeMax = -coefficientMin;
        outExpansion->exactTermCount = magnitudeMin;
        outExpansion->exactTermSign = -1;
        outExpansion->residualMax = magnitudeMax - magnitudeMin;
        outExpansion->residualSign = -1;
        outExpansion->selectionScore = magnitudeMin;
        return ZR_TRUE;
    }

    magnitudeMin = -coefficientMin;
    if (!loop_assignment_sequence_symbolic_int64_add(
                coefficientMax,
                magnitudeMin,
                &residualMax)) {
        return ZR_FALSE;
    }
    outExpansion->exactTermCount = magnitudeMin;
    outExpansion->exactTermSign = -1;
    outExpansion->residualMax = residualMax;
    outExpansion->residualSign = 1;
    outExpansion->selectionScore = magnitudeMin;
    outExpansion->crossesZero = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool loop_assignment_sequence_symbolic_delta_append_term(
        SZrAstNode *node,
        SZrAstNode *coefficientNode,
        TZrInt32 sign,
        TZrBool hasCoefficientRange,
        TZrInt64 coefficientMin,
        TZrInt64 coefficientMax,
        SZrTypeInferenceLoopAssignmentSequenceSymbolicTerm *terms,
        TZrSize *rawTermCount,
        TZrSize *termCount) {
    if (node == ZR_NULL ||
        terms == ZR_NULL ||
        rawTermCount == ZR_NULL ||
        termCount == ZR_NULL ||
        (sign != 1 && sign != -1) ||
        coefficientMin < 0 ||
        coefficientMax < coefficientMin ||
        *rawTermCount >= ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_SEQUENCE_SYMBOLIC_TERM_LIMIT ||
        *termCount >= ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_SEQUENCE_SYMBOLIC_TERM_LIMIT) {
        return ZR_FALSE;
    }

    *rawTermCount = *rawTermCount + 1;
    terms[*termCount].node = node;
    terms[*termCount].coefficientNode = coefficientNode;
    terms[*termCount].sign = sign;
    terms[*termCount].hasCoefficientRange = hasCoefficientRange;
    terms[*termCount].coefficientMin = coefficientMin;
    terms[*termCount].coefficientMax = coefficientMax;
    *termCount = *termCount + 1;
    return ZR_TRUE;
}

static TZrBool loop_assignment_sequence_symbolic_delta_try_collect_range_coefficient_product(
        SZrCompilerState *cs,
        SZrString *sequenceName,
        SZrAstNode *node,
        TZrInt32 sign,
        SZrTypeInferenceLoopAssignmentSequenceSymbolicTerm *terms,
        TZrSize *rawTermCount,
        TZrSize *termCount,
        TZrBool *outCollected) {
    SZrAstNode *termNode = ZR_NULL;
    SZrAstNode *coefficientNode = ZR_NULL;
    TZrInt64 coefficientMin = 0;
    TZrInt64 coefficientMax = 0;
    TZrInt64 termMin = 0;
    TZrInt64 termMax = 0;
    TZrInt64 leftCoefficientMin = 0;
    TZrInt64 leftCoefficientMax = 0;
    TZrInt64 rightCoefficientMin = 0;
    TZrInt64 rightCoefficientMax = 0;
    TZrInt64 repeatIndex;
    TZrInt32 exactTermSign;
    TZrInt32 residualTermSign;
    TZrSize requiredTerms = 0;
    SZrTypeInferenceLoopAssignmentSequenceCoefficientExpansion coefficientExpansion = {0};
    SZrTypeInferenceLoopAssignmentSequenceCoefficientExpansion leftExpansion = {0};
    SZrTypeInferenceLoopAssignmentSequenceCoefficientExpansion rightExpansion = {0};
    TZrBool hasLeftCoefficient;
    TZrBool hasRightCoefficient;

    if (outCollected == ZR_NULL) {
        return ZR_FALSE;
    }
    *outCollected = ZR_FALSE;

    if (node == ZR_NULL ||
        terms == ZR_NULL ||
        rawTermCount == ZR_NULL ||
        termCount == ZR_NULL ||
        node->type != ZR_AST_BINARY_EXPRESSION ||
        !loop_assignment_sequence_symbolic_string_equal(
                node->data.binaryExpression.op.op,
                "*")) {
        return ZR_TRUE;
    }

    hasRightCoefficient = loop_assignment_sequence_symbolic_delta_range_coefficient(
            cs,
            sequenceName,
            node->data.binaryExpression.right,
            &rightCoefficientMin,
            &rightCoefficientMax) &&
            loop_assignment_sequence_symbolic_delta_coefficient_expansion(
                    rightCoefficientMin,
                    rightCoefficientMax,
                    &rightExpansion);
    hasLeftCoefficient = loop_assignment_sequence_symbolic_delta_range_coefficient(
            cs,
            sequenceName,
            node->data.binaryExpression.left,
            &leftCoefficientMin,
            &leftCoefficientMax) &&
            loop_assignment_sequence_symbolic_delta_coefficient_expansion(
                    leftCoefficientMin,
                    leftCoefficientMax,
                    &leftExpansion);

    if (hasRightCoefficient &&
        (!hasLeftCoefficient ||
         rightExpansion.selectionScore > leftExpansion.selectionScore ||
         (rightExpansion.selectionScore == leftExpansion.selectionScore &&
          (rightExpansion.crossesZero || !leftExpansion.crossesZero)))) {
        termNode = node->data.binaryExpression.left;
        coefficientNode = node->data.binaryExpression.right;
        coefficientMin = rightCoefficientMin;
        coefficientMax = rightCoefficientMax;
        coefficientExpansion = rightExpansion;
    } else if (hasLeftCoefficient) {
        termNode = node->data.binaryExpression.right;
        coefficientNode = node->data.binaryExpression.left;
        coefficientMin = leftCoefficientMin;
        coefficientMax = leftCoefficientMax;
        coefficientExpansion = leftExpansion;
    } else {
        return ZR_TRUE;
    }

    if (coefficientExpansion.crossesZero &&
        (!ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsIdentifier(termNode) ||
         !ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsSupportedCrossingCoefficient(
                 cs,
                 coefficientNode) ||
         !ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicInferInt64Range(
                 cs,
                 termNode,
                 &termMin,
                 &termMax) ||
         termMin < 0 ||
         ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicNodeIsBinaryMultiply(termNode))) {
        return ZR_TRUE;
    }
    if (termNode == ZR_NULL || coefficientMin == coefficientMax) {
        return ZR_TRUE;
    }
    if (coefficientExpansion.exactTermCount >
        (TZrInt64)ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_SEQUENCE_SYMBOLIC_TERM_LIMIT) {
        return ZR_TRUE;
    }
    requiredTerms = (TZrSize)coefficientExpansion.exactTermCount +
                    (coefficientExpansion.residualMax > 0 ? 1u : 0u);
    if (requiredTerms == 0 ||
        *rawTermCount + requiredTerms >
                ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_SEQUENCE_SYMBOLIC_TERM_LIMIT ||
        *termCount + requiredTerms >
                ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_SEQUENCE_SYMBOLIC_TERM_LIMIT) {
        return ZR_TRUE;
    }

    exactTermSign = sign;
    if (coefficientExpansion.exactTermSign < 0) {
        exactTermSign = -exactTermSign;
    }
    residualTermSign = sign;
    if (coefficientExpansion.residualSign < 0) {
        residualTermSign = -residualTermSign;
    }

    for (repeatIndex = 0; repeatIndex < coefficientExpansion.exactTermCount; repeatIndex++) {
        if (!loop_assignment_sequence_symbolic_delta_append_term(
                    termNode,
                    ZR_NULL,
                    exactTermSign,
                    ZR_FALSE,
                    1,
                    1,
                    terms,
                    rawTermCount,
                    termCount)) {
            return ZR_FALSE;
        }
    }
    if (coefficientExpansion.residualMax > 0 &&
        !loop_assignment_sequence_symbolic_delta_append_term(
                termNode,
                coefficientNode,
                residualTermSign,
                ZR_TRUE,
                0,
                coefficientExpansion.residualMax,
                terms,
                rawTermCount,
                termCount)) {
        return ZR_FALSE;
    }

    *outCollected = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool loop_assignment_sequence_symbolic_delta_try_collect_coefficient_product(
        SZrCompilerState *cs,
        SZrString *sequenceName,
        SZrAstNode *node,
        TZrInt32 sign,
        SZrTypeInferenceLoopAssignmentSequenceSymbolicTerm *terms,
        TZrSize *rawTermCount,
        TZrSize *termCount,
        TZrInt64 *integerLiteralSum,
        TZrBool *outCollected) {
    SZrAstNode *termNode = ZR_NULL;
    TZrInt64 repeatCount = 0;
    TZrInt64 repeatIndex;

    if (outCollected == ZR_NULL) {
        return ZR_FALSE;
    }
    *outCollected = ZR_FALSE;

    if (node == ZR_NULL ||
        node->type != ZR_AST_BINARY_EXPRESSION ||
        !loop_assignment_sequence_symbolic_string_equal(
                node->data.binaryExpression.op.op,
                "*")) {
        return ZR_TRUE;
    }

    if (loop_assignment_sequence_symbolic_delta_singleton_coefficient(
                cs,
                sequenceName,
                node->data.binaryExpression.left,
                &repeatCount)) {
        termNode = node->data.binaryExpression.right;
    } else if (loop_assignment_sequence_symbolic_delta_singleton_coefficient(
                       cs,
                       sequenceName,
                       node->data.binaryExpression.right,
                       &repeatCount)) {
        termNode = node->data.binaryExpression.left;
    } else {
        return loop_assignment_sequence_symbolic_delta_try_collect_range_coefficient_product(
                cs,
                sequenceName,
                node,
                sign,
                terms,
                rawTermCount,
                termCount,
                outCollected);
    }

    if (termNode == ZR_NULL) {
        return ZR_FALSE;
    }
    if (repeatCount == 0) {
        *outCollected = ZR_TRUE;
        return ZR_TRUE;
    }
    if (repeatCount < 0) {
        if (repeatCount == ZR_TYPE_RANGE_INT64_MIN) {
            return ZR_TRUE;
        }
        repeatCount = -repeatCount;
        sign = -sign;
    }
    if (repeatCount > (TZrInt64)ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_SEQUENCE_SYMBOLIC_TERM_LIMIT) {
        return ZR_TRUE;
    }

    for (repeatIndex = 0; repeatIndex < repeatCount; repeatIndex++) {
        if (!loop_assignment_sequence_symbolic_delta_collect_terms(
                    cs,
                    sequenceName,
                    termNode,
                    sign,
                    terms,
                    rawTermCount,
                    termCount,
                    integerLiteralSum)) {
            return ZR_FALSE;
        }
    }

    *outCollected = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool loop_assignment_sequence_symbolic_delta_collect_terms(
        SZrCompilerState *cs,
        SZrString *sequenceName,
        SZrAstNode *node,
        TZrInt32 sign,
        SZrTypeInferenceLoopAssignmentSequenceSymbolicTerm *terms,
        TZrSize *rawTermCount,
        TZrSize *termCount,
        TZrInt64 *integerLiteralSum) {
    const TZrChar *op;

    if (node == ZR_NULL ||
        terms == ZR_NULL ||
        rawTermCount == ZR_NULL ||
        termCount == ZR_NULL ||
        integerLiteralSum == ZR_NULL ||
        (sign != 1 && sign != -1)) {
        return ZR_FALSE;
    }

    if (node->type == ZR_AST_BINARY_EXPRESSION) {
        op = node->data.binaryExpression.op.op;
        if (loop_assignment_sequence_symbolic_string_equal(op, "+")) {
            return loop_assignment_sequence_symbolic_delta_collect_terms(
                           cs,
                           sequenceName,
                           node->data.binaryExpression.left,
                           sign,
                           terms,
                           rawTermCount,
                           termCount,
                           integerLiteralSum) &&
                   loop_assignment_sequence_symbolic_delta_collect_terms(
                           cs,
                           sequenceName,
                           node->data.binaryExpression.right,
                           sign,
                           terms,
                           rawTermCount,
                           termCount,
                           integerLiteralSum) &&
                   ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicDeltaCancelCollectedTerms(
                           terms,
                           rawTermCount,
                           termCount,
                           sequenceName);
        }
        if (loop_assignment_sequence_symbolic_string_equal(op, "-")) {
            return loop_assignment_sequence_symbolic_delta_collect_terms(
                           cs,
                           sequenceName,
                           node->data.binaryExpression.left,
                           sign,
                           terms,
                           rawTermCount,
                           termCount,
                           integerLiteralSum) &&
                   loop_assignment_sequence_symbolic_delta_collect_terms(
                           cs,
                           sequenceName,
                           node->data.binaryExpression.right,
                           -sign,
                           terms,
                           rawTermCount,
                           termCount,
                           integerLiteralSum) &&
                   ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicDeltaCancelCollectedTerms(
                           terms,
                           rawTermCount,
                           termCount,
                           sequenceName);
        }
        if (loop_assignment_sequence_symbolic_string_equal(op, "*")) {
            TZrBool collected = ZR_FALSE;

            if (!loop_assignment_sequence_symbolic_delta_try_collect_coefficient_product(
                        cs,
                        sequenceName,
                        node,
                        sign,
                        terms,
                        rawTermCount,
                        termCount,
                        integerLiteralSum,
                        &collected)) {
                return ZR_FALSE;
            }
            if (collected) {
                return ZR_TRUE;
            }
        }
    }

    if (node->type == ZR_AST_INTEGER_LITERAL) {
        TZrInt64 nextSum = 0;

        if (*rawTermCount >= ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_SEQUENCE_SYMBOLIC_TERM_LIMIT) {
            return ZR_FALSE;
        }
        *rawTermCount = *rawTermCount + 1;
        if (!loop_assignment_sequence_symbolic_signed_integer_literal_add(
                    *integerLiteralSum,
                    node->data.integerLiteral.value,
                    sign,
                    &nextSum)) {
            return ZR_FALSE;
        }
        *integerLiteralSum = nextSum;
        return ZR_TRUE;
    }

    return loop_assignment_sequence_symbolic_delta_append_term(
            node,
            ZR_NULL,
            sign,
            ZR_FALSE,
            1,
            1,
            terms,
            rawTermCount,
            termCount);
}

static TZrBool loop_assignment_sequence_symbolic_delta_append(
        SZrCompilerState *cs,
        SZrTypeInferenceLoopAssignmentSequenceDeltaTracker *tracker,
        SZrAstNode *deltaExpression,
        TZrInt32 deltaSign,
        SZrString *sequenceName) {
    TZrSize rawTermCount;

    if (tracker == ZR_NULL ||
        deltaExpression == ZR_NULL ||
        sequenceName == ZR_NULL ||
        (deltaSign != 1 && deltaSign != -1)) {
        return ZR_FALSE;
    }

    rawTermCount = tracker->symbolicResidualTermCount;
    tracker->hasSymbolicConstantResidualDelta = ZR_FALSE;
    if (!loop_assignment_sequence_symbolic_delta_collect_terms(
                cs,
                sequenceName,
                deltaExpression,
                deltaSign,
                tracker->symbolicResidualTerms,
                &rawTermCount,
                &tracker->symbolicResidualTermCount,
                &tracker->symbolicResidualIntegerLiteralSum) ||
        !ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicDeltaCancelTerms(
                tracker->symbolicResidualTerms,
                &tracker->symbolicResidualTermCount,
                sequenceName)) {
        return ZR_FALSE;
    }

    if (tracker->symbolicResidualTermCount == 0) {
        tracker->hasSymbolicConstantResidualDelta = ZR_TRUE;
        tracker->symbolicConstantResidualDelta =
                tracker->symbolicResidualIntegerLiteralSum;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicDeltaUpdate(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentStep *step,
        SZrString *sequenceName,
        SZrTypeInferenceLoopAssignmentSequenceDeltaTracker *tracker) {
    SZrAstNode *deltaExpression;
    TZrInt32 deltaSign = 0;

    if (sequenceName == ZR_NULL || tracker == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!tracker->canTrackSymbolicDelta) {
        return ZR_TRUE;
    }
    if (step == ZR_NULL ||
        step->right == ZR_NULL ||
        step->resolveSelfDependentDeltaOnReplay) {
        tracker->canTrackSymbolicDelta = ZR_FALSE;
        return ZR_TRUE;
    }

    deltaExpression = ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaExpression(
            step->right,
            sequenceName);
    if (deltaExpression == ZR_NULL ||
        !ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaSign(
                step->right,
                sequenceName,
                &deltaSign)) {
        tracker->canTrackSymbolicDelta = ZR_FALSE;
        return ZR_TRUE;
    }

    if (tracker->symbolicDeltaExpression == ZR_NULL) {
        tracker->symbolicDeltaExpression = deltaExpression;
    }
    if (!loop_assignment_sequence_symbolic_delta_append(
                cs,
                tracker,
                deltaExpression,
                deltaSign,
                sequenceName)) {
        tracker->canTrackSymbolicDelta = ZR_FALSE;
        return ZR_TRUE;
    }
    tracker->symbolicDeltaBalance += (TZrInt64)deltaSign;
    return ZR_TRUE;
}
