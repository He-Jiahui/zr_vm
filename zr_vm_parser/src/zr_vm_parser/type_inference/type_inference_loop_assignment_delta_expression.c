#include "type_inference_loop_assignment_self_dependency.h"

#include <string.h>

#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"

#include "type_inference_loop_assignment_delta_product.h"

enum {
    ZR_LOOP_ASSIGNMENT_DELTA_EXPRESSION_TERM_LIMIT = 8
};

typedef struct SZrLoopAssignmentDeltaExpressionSignedTerm {
    SZrAstNode *node;
    TZrInt32 sign;
} SZrLoopAssignmentDeltaExpressionSignedTerm;

static TZrBool loop_assignment_delta_expression_string_equal(const TZrChar *left,
                                                             const TZrChar *right) {
    return left != ZR_NULL && right != ZR_NULL && strcmp(left, right) == 0;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_DeltaExpressionIsSupported(
        SZrAstNode *node,
        SZrString *targetName) {
    SZrAstNodeArray *members;

    if (node == ZR_NULL || targetName == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_BOOLEAN_LITERAL:
            return ZR_TRUE;
        case ZR_AST_IDENTIFIER_LITERAL:
            return node->data.identifier.name != ZR_NULL &&
                   !ZrCore_String_Equal(node->data.identifier.name, targetName);
        case ZR_AST_BINARY_EXPRESSION:
            return ZrParser_TypeInferenceLoopAssignment_DeltaExpressionIsSupported(
                           node->data.binaryExpression.left,
                           targetName) &&
                   ZrParser_TypeInferenceLoopAssignment_DeltaExpressionIsSupported(
                           node->data.binaryExpression.right,
                           targetName);
        case ZR_AST_LOGICAL_EXPRESSION:
            return ZrParser_TypeInferenceLoopAssignment_DeltaExpressionIsSupported(
                           node->data.logicalExpression.left,
                           targetName) &&
                   ZrParser_TypeInferenceLoopAssignment_DeltaExpressionIsSupported(
                           node->data.logicalExpression.right,
                           targetName);
        case ZR_AST_UNARY_EXPRESSION:
            return ZrParser_TypeInferenceLoopAssignment_DeltaExpressionIsSupported(
                    node->data.unaryExpression.argument,
                    targetName);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return ZrParser_TypeInferenceLoopAssignment_DeltaExpressionIsSupported(
                    node->data.typeCastExpression.expression,
                    targetName);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return ZrParser_TypeInferenceLoopAssignment_DeltaExpressionIsSupported(
                           node->data.conditionalExpression.test,
                           targetName) &&
                   ZrParser_TypeInferenceLoopAssignment_DeltaExpressionIsSupported(
                           node->data.conditionalExpression.consequent,
                           targetName) &&
                   ZrParser_TypeInferenceLoopAssignment_DeltaExpressionIsSupported(
                           node->data.conditionalExpression.alternate,
                           targetName);
        case ZR_AST_PRIMARY_EXPRESSION:
            members = node->data.primaryExpression.members;
            if (members != ZR_NULL && members->count > 0) {
                return ZR_FALSE;
            }
            return ZrParser_TypeInferenceLoopAssignment_DeltaExpressionIsSupported(
                    node->data.primaryExpression.property,
                    targetName);
        default:
            return ZR_FALSE;
    }
}

static TZrBool loop_assignment_delta_expression_equal(SZrAstNode *left,
                                                      SZrAstNode *right,
                                                      SZrString *targetName);

static TZrBool loop_assignment_delta_expression_is_binary_plus(SZrAstNode *node) {
    return node != ZR_NULL &&
           node->type == ZR_AST_BINARY_EXPRESSION &&
           loop_assignment_delta_expression_string_equal(node->data.binaryExpression.op.op, "+");
}

static TZrBool loop_assignment_delta_expression_is_binary_additive(SZrAstNode *node) {
    return node != ZR_NULL &&
           node->type == ZR_AST_BINARY_EXPRESSION &&
           (loop_assignment_delta_expression_string_equal(node->data.binaryExpression.op.op, "+") ||
            loop_assignment_delta_expression_string_equal(node->data.binaryExpression.op.op, "-"));
}

static TZrBool loop_assignment_delta_expression_collect_plus_terms(SZrAstNode *node,
                                                                   SZrAstNode **terms,
                                                                   TZrSize *termCount) {
    if (node == ZR_NULL || terms == ZR_NULL || termCount == ZR_NULL) {
        return ZR_FALSE;
    }

    if (loop_assignment_delta_expression_is_binary_plus(node)) {
        return loop_assignment_delta_expression_collect_plus_terms(
                       node->data.binaryExpression.left,
                       terms,
                       termCount) &&
               loop_assignment_delta_expression_collect_plus_terms(
                       node->data.binaryExpression.right,
                       terms,
                       termCount);
    }

    if (*termCount >= ZR_LOOP_ASSIGNMENT_DELTA_EXPRESSION_TERM_LIMIT) {
        return ZR_FALSE;
    }

    terms[*termCount] = node;
    *termCount = *termCount + 1;
    return ZR_TRUE;
}

static TZrBool loop_assignment_delta_expression_checked_add_int64(TZrInt64 left,
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

static TZrBool loop_assignment_delta_expression_split_plus_terms(SZrAstNode **sourceTerms,
                                                                 TZrSize sourceCount,
                                                                 SZrAstNode **expressionTerms,
                                                                 TZrSize *expressionTermCount,
                                                                 TZrInt64 *integerLiteralSum) {
    TZrSize index;

    if (sourceTerms == ZR_NULL ||
        expressionTerms == ZR_NULL ||
        expressionTermCount == ZR_NULL ||
        integerLiteralSum == ZR_NULL) {
        return ZR_FALSE;
    }

    *expressionTermCount = 0;
    *integerLiteralSum = 0;
    for (index = 0; index < sourceCount; index++) {
        SZrAstNode *term = sourceTerms[index];

        if (term == ZR_NULL) {
            return ZR_FALSE;
        }
        if (term->type == ZR_AST_INTEGER_LITERAL) {
            TZrInt64 nextSum = 0;

            if (!loop_assignment_delta_expression_checked_add_int64(
                        *integerLiteralSum,
                        term->data.integerLiteral.value,
                        &nextSum)) {
                return ZR_FALSE;
            }
            *integerLiteralSum = nextSum;
            continue;
        }

        if (*expressionTermCount >= ZR_LOOP_ASSIGNMENT_DELTA_EXPRESSION_TERM_LIMIT) {
            return ZR_FALSE;
        }
        expressionTerms[*expressionTermCount] = term;
        *expressionTermCount = *expressionTermCount + 1;
    }

    return ZR_TRUE;
}

static TZrBool loop_assignment_delta_expression_checked_add_signed_integer_literal(
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

    return loop_assignment_delta_expression_checked_add_int64(currentSum, signedValue, outSum);
}

static TZrBool loop_assignment_delta_expression_collect_signed_additive_terms(
        SZrAstNode *node,
        TZrInt32 sign,
        SZrLoopAssignmentDeltaExpressionSignedTerm *expressionTerms,
        TZrSize *rawTermCount,
        TZrSize *expressionTermCount,
        TZrInt64 *integerLiteralSum) {
    const TZrChar *op;

    if (node == ZR_NULL ||
        expressionTerms == ZR_NULL ||
        rawTermCount == ZR_NULL ||
        expressionTermCount == ZR_NULL ||
        integerLiteralSum == ZR_NULL ||
        (sign != 1 && sign != -1)) {
        return ZR_FALSE;
    }

    if (node->type == ZR_AST_BINARY_EXPRESSION) {
        op = node->data.binaryExpression.op.op;
        if (loop_assignment_delta_expression_string_equal(op, "+")) {
            return loop_assignment_delta_expression_collect_signed_additive_terms(
                           node->data.binaryExpression.left,
                           sign,
                           expressionTerms,
                           rawTermCount,
                           expressionTermCount,
                           integerLiteralSum) &&
                   loop_assignment_delta_expression_collect_signed_additive_terms(
                           node->data.binaryExpression.right,
                           sign,
                           expressionTerms,
                           rawTermCount,
                           expressionTermCount,
                           integerLiteralSum);
        }
        if (loop_assignment_delta_expression_string_equal(op, "-")) {
            return loop_assignment_delta_expression_collect_signed_additive_terms(
                           node->data.binaryExpression.left,
                           sign,
                           expressionTerms,
                           rawTermCount,
                           expressionTermCount,
                           integerLiteralSum) &&
                   loop_assignment_delta_expression_collect_signed_additive_terms(
                           node->data.binaryExpression.right,
                           -sign,
                           expressionTerms,
                           rawTermCount,
                           expressionTermCount,
                           integerLiteralSum);
        }
    }

    if (*rawTermCount >= ZR_LOOP_ASSIGNMENT_DELTA_EXPRESSION_TERM_LIMIT) {
        return ZR_FALSE;
    }
    *rawTermCount = *rawTermCount + 1;

    if (node->type == ZR_AST_INTEGER_LITERAL) {
        TZrInt64 nextSum = 0;

        if (!loop_assignment_delta_expression_checked_add_signed_integer_literal(
                    *integerLiteralSum,
                    node->data.integerLiteral.value,
                    sign,
                    &nextSum)) {
            return ZR_FALSE;
        }
        *integerLiteralSum = nextSum;
        return ZR_TRUE;
    }

    if (*expressionTermCount >= ZR_LOOP_ASSIGNMENT_DELTA_EXPRESSION_TERM_LIMIT) {
        return ZR_FALSE;
    }
    expressionTerms[*expressionTermCount].node = node;
    expressionTerms[*expressionTermCount].sign = sign;
    *expressionTermCount = *expressionTermCount + 1;
    return ZR_TRUE;
}

static TZrBool loop_assignment_delta_expression_cancel_opposite_signed_terms(
        SZrLoopAssignmentDeltaExpressionSignedTerm *terms,
        TZrSize *termCount,
        SZrString *targetName,
        TZrBool *outCancelledAny) {
    TZrSize leftIndex;
    TZrSize rightIndex;
    TZrSize writeIndex;

    if (terms == ZR_NULL ||
        termCount == ZR_NULL ||
        targetName == ZR_NULL ||
        outCancelledAny == ZR_NULL) {
        return ZR_FALSE;
    }

    *outCancelledAny = ZR_FALSE;
    for (leftIndex = 0; leftIndex < *termCount; leftIndex++) {
        if (terms[leftIndex].node == ZR_NULL) {
            continue;
        }
        for (rightIndex = leftIndex + 1; rightIndex < *termCount; rightIndex++) {
            if (terms[rightIndex].node != ZR_NULL &&
                terms[leftIndex].sign == -terms[rightIndex].sign &&
                loop_assignment_delta_expression_equal(
                        terms[leftIndex].node,
                        terms[rightIndex].node,
                        targetName)) {
                terms[leftIndex].node = ZR_NULL;
                terms[rightIndex].node = ZR_NULL;
                *outCancelledAny = ZR_TRUE;
                break;
            }
        }
    }

    for (leftIndex = 0, writeIndex = 0; leftIndex < *termCount; leftIndex++) {
        if (terms[leftIndex].node != ZR_NULL) {
            terms[writeIndex] = terms[leftIndex];
            writeIndex = writeIndex + 1;
        }
    }
    *termCount = writeIndex;
    return ZR_TRUE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_TrySignedAdditiveConstantResidual(
        SZrAstNode *left,
        TZrInt32 leftSign,
        SZrAstNode *right,
        TZrInt32 rightSign,
        SZrString *targetName,
        TZrInt64 *outResidual) {
    SZrLoopAssignmentDeltaExpressionSignedTerm
            terms[ZR_LOOP_ASSIGNMENT_DELTA_EXPRESSION_TERM_LIMIT];
    TZrSize rawTermCount = 0;
    TZrSize termCount = 0;
    TZrInt64 integerLiteralSum = 0;
    TZrBool cancelledAny = ZR_FALSE;

    if (outResidual != ZR_NULL) {
        *outResidual = 0;
    }
    if (left == ZR_NULL ||
        right == ZR_NULL ||
        targetName == ZR_NULL ||
        outResidual == ZR_NULL ||
        (leftSign != 1 && leftSign != -1) ||
        (rightSign != 1 && rightSign != -1) ||
        !ZrParser_TypeInferenceLoopAssignment_DeltaExpressionIsSupported(left, targetName) ||
        !ZrParser_TypeInferenceLoopAssignment_DeltaExpressionIsSupported(right, targetName) ||
        !loop_assignment_delta_expression_collect_signed_additive_terms(
                left,
                leftSign,
                terms,
                &rawTermCount,
                &termCount,
                &integerLiteralSum) ||
        !loop_assignment_delta_expression_collect_signed_additive_terms(
                right,
                rightSign,
                terms,
                &rawTermCount,
                &termCount,
                &integerLiteralSum) ||
        !loop_assignment_delta_expression_cancel_opposite_signed_terms(
                terms,
                &termCount,
                targetName,
                &cancelledAny) ||
        termCount != 0) {
        return ZR_FALSE;
    }

    *outResidual = integerLiteralSum;
    return ZR_TRUE;
}

static TZrBool loop_assignment_delta_expression_signed_additive_terms_equal(
        SZrAstNode *left,
        SZrAstNode *right,
        SZrString *targetName) {
    SZrLoopAssignmentDeltaExpressionSignedTerm
            leftTerms[ZR_LOOP_ASSIGNMENT_DELTA_EXPRESSION_TERM_LIMIT];
    SZrLoopAssignmentDeltaExpressionSignedTerm
            rightTerms[ZR_LOOP_ASSIGNMENT_DELTA_EXPRESSION_TERM_LIMIT];
    TZrBool matched[ZR_LOOP_ASSIGNMENT_DELTA_EXPRESSION_TERM_LIMIT];
    TZrSize leftRawCount = 0;
    TZrSize rightRawCount = 0;
    TZrSize leftCount = 0;
    TZrSize rightCount = 0;
    TZrInt64 leftIntegerLiteralSum = 0;
    TZrInt64 rightIntegerLiteralSum = 0;
    TZrSize leftIndex;
    TZrSize rightIndex;
    TZrBool cancelledAny;

    if (!loop_assignment_delta_expression_collect_signed_additive_terms(
                left,
                1,
                leftTerms,
                &leftRawCount,
                &leftCount,
                &leftIntegerLiteralSum) ||
        !loop_assignment_delta_expression_collect_signed_additive_terms(
                right,
                1,
                rightTerms,
                &rightRawCount,
                &rightCount,
                &rightIntegerLiteralSum) ||
        leftIntegerLiteralSum != rightIntegerLiteralSum) {
        return ZR_FALSE;
    }

    if (!loop_assignment_delta_expression_cancel_opposite_signed_terms(
                leftTerms,
                &leftCount,
                targetName,
                &cancelledAny) ||
        !loop_assignment_delta_expression_cancel_opposite_signed_terms(
                rightTerms,
                &rightCount,
                targetName,
                &cancelledAny) ||
        leftCount != rightCount) {
        return ZR_FALSE;
    }

    for (rightIndex = 0; rightIndex < rightCount; rightIndex++) {
        matched[rightIndex] = ZR_FALSE;
    }

    for (leftIndex = 0; leftIndex < leftCount; leftIndex++) {
        TZrBool found = ZR_FALSE;

        for (rightIndex = 0; rightIndex < rightCount; rightIndex++) {
            if (!matched[rightIndex] &&
                leftTerms[leftIndex].sign == rightTerms[rightIndex].sign &&
                loop_assignment_delta_expression_equal(
                        leftTerms[leftIndex].node,
                        rightTerms[rightIndex].node,
                        targetName)) {
                matched[rightIndex] = ZR_TRUE;
                found = ZR_TRUE;
                break;
            }
        }

        if (!found) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool loop_assignment_delta_expression_infer_int64_range(
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

static TZrBool loop_assignment_delta_expression_add_signed_range(
        TZrInt64 currentMin,
        TZrInt64 currentMax,
        TZrInt64 termMin,
        TZrInt64 termMax,
        TZrInt32 sign,
        TZrInt64 *outMin,
        TZrInt64 *outMax) {
    TZrInt64 signedTermMin;
    TZrInt64 signedTermMax;

    if (outMin == ZR_NULL ||
        outMax == ZR_NULL ||
        (sign != 1 && sign != -1)) {
        return ZR_FALSE;
    }

    if (sign > 0) {
        signedTermMin = termMin;
        signedTermMax = termMax;
    } else {
        if (termMin == ZR_TYPE_RANGE_INT64_MIN) {
            return ZR_FALSE;
        }
        signedTermMin = -termMax;
        signedTermMax = -termMin;
    }

    return loop_assignment_delta_expression_checked_add_int64(
                   currentMin,
                   signedTermMin,
                   outMin) &&
           loop_assignment_delta_expression_checked_add_int64(
                   currentMax,
                   signedTermMax,
                   outMax);
}

TZrBool ZrParser_TypeInferenceLoopAssignment_TryCancelledSignedAdditiveDeltaRange(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrString *targetName,
        TZrInt64 *outMin,
        TZrInt64 *outMax) {
    SZrLoopAssignmentDeltaExpressionSignedTerm
            terms[ZR_LOOP_ASSIGNMENT_DELTA_EXPRESSION_TERM_LIMIT];
    TZrSize rawTermCount = 0;
    TZrSize termCount = 0;
    TZrInt64 integerLiteralSum = 0;
    TZrInt64 minValue;
    TZrInt64 maxValue;
    TZrSize index;
    TZrBool cancelledAny = ZR_FALSE;

    if (cs == ZR_NULL ||
        node == ZR_NULL ||
        targetName == ZR_NULL ||
        outMin == ZR_NULL ||
        outMax == ZR_NULL ||
        !ZrParser_TypeInferenceLoopAssignment_DeltaExpressionIsSupported(node, targetName) ||
        !loop_assignment_delta_expression_collect_signed_additive_terms(
                node,
                1,
                terms,
                &rawTermCount,
                &termCount,
                &integerLiteralSum) ||
        !loop_assignment_delta_expression_cancel_opposite_signed_terms(
                terms,
                &termCount,
                targetName,
                &cancelledAny) ||
        !cancelledAny) {
        return ZR_FALSE;
    }

    minValue = integerLiteralSum;
    maxValue = integerLiteralSum;
    for (index = 0; index < termCount; index++) {
        TZrInt64 termMin;
        TZrInt64 termMax;
        TZrInt64 nextMin;
        TZrInt64 nextMax;

        if (!loop_assignment_delta_expression_infer_int64_range(
                    cs,
                    terms[index].node,
                    &termMin,
                    &termMax) ||
            !loop_assignment_delta_expression_add_signed_range(
                    minValue,
                    maxValue,
                    termMin,
                    termMax,
                    terms[index].sign,
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

static TZrBool loop_assignment_delta_expression_plus_terms_equal(SZrAstNode *left,
                                                                SZrAstNode *right,
                                                                SZrString *targetName) {
    SZrAstNode *leftTerms[ZR_LOOP_ASSIGNMENT_DELTA_EXPRESSION_TERM_LIMIT];
    SZrAstNode *rightTerms[ZR_LOOP_ASSIGNMENT_DELTA_EXPRESSION_TERM_LIMIT];
    SZrAstNode *leftExpressionTerms[ZR_LOOP_ASSIGNMENT_DELTA_EXPRESSION_TERM_LIMIT];
    SZrAstNode *rightExpressionTerms[ZR_LOOP_ASSIGNMENT_DELTA_EXPRESSION_TERM_LIMIT];
    TZrBool matched[ZR_LOOP_ASSIGNMENT_DELTA_EXPRESSION_TERM_LIMIT];
    TZrSize leftCount = 0;
    TZrSize rightCount = 0;
    TZrSize leftExpressionCount = 0;
    TZrSize rightExpressionCount = 0;
    TZrInt64 leftIntegerLiteralSum = 0;
    TZrInt64 rightIntegerLiteralSum = 0;
    TZrSize leftIndex;
    TZrSize rightIndex;

    if (!loop_assignment_delta_expression_collect_plus_terms(left, leftTerms, &leftCount) ||
        !loop_assignment_delta_expression_collect_plus_terms(right, rightTerms, &rightCount) ||
        !loop_assignment_delta_expression_split_plus_terms(
                leftTerms,
                leftCount,
                leftExpressionTerms,
                &leftExpressionCount,
                &leftIntegerLiteralSum) ||
        !loop_assignment_delta_expression_split_plus_terms(
                rightTerms,
                rightCount,
                rightExpressionTerms,
                &rightExpressionCount,
                &rightIntegerLiteralSum) ||
        leftExpressionCount != rightExpressionCount ||
        leftIntegerLiteralSum != rightIntegerLiteralSum) {
        return ZR_FALSE;
    }

    for (rightIndex = 0; rightIndex < rightExpressionCount; rightIndex++) {
        matched[rightIndex] = ZR_FALSE;
    }

    for (leftIndex = 0; leftIndex < leftExpressionCount; leftIndex++) {
        TZrBool found = ZR_FALSE;

        for (rightIndex = 0; rightIndex < rightExpressionCount; rightIndex++) {
            if (!matched[rightIndex] &&
                loop_assignment_delta_expression_equal(
                        leftExpressionTerms[leftIndex],
                        rightExpressionTerms[rightIndex],
                        targetName)) {
                matched[rightIndex] = ZR_TRUE;
                found = ZR_TRUE;
                break;
            }
        }

        if (!found) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool loop_assignment_delta_expression_equal(SZrAstNode *left,
                                                      SZrAstNode *right,
                                                      SZrString *targetName) {
    SZrAstNodeArray *leftMembers;
    SZrAstNodeArray *rightMembers;

    if (left == ZR_NULL ||
        right == ZR_NULL ||
        targetName == ZR_NULL ||
        !ZrParser_TypeInferenceLoopAssignment_DeltaExpressionIsSupported(left, targetName) ||
        !ZrParser_TypeInferenceLoopAssignment_DeltaExpressionIsSupported(right, targetName)) {
        return ZR_FALSE;
    }

    if (left->type != right->type) {
        if ((loop_assignment_delta_expression_is_binary_plus(left) ||
             loop_assignment_delta_expression_is_binary_plus(right)) &&
            loop_assignment_delta_expression_plus_terms_equal(left, right, targetName)) {
            return ZR_TRUE;
        }
        return (loop_assignment_delta_expression_is_binary_additive(left) ||
                loop_assignment_delta_expression_is_binary_additive(right)) &&
               loop_assignment_delta_expression_signed_additive_terms_equal(
                       left,
                       right,
                       targetName);
    }

    switch (left->type) {
        case ZR_AST_INTEGER_LITERAL:
            return left->data.integerLiteral.value == right->data.integerLiteral.value;
        case ZR_AST_FLOAT_LITERAL:
            return left->data.floatLiteral.value == right->data.floatLiteral.value &&
                   left->data.floatLiteral.isSingle == right->data.floatLiteral.isSingle;
        case ZR_AST_BOOLEAN_LITERAL:
            return left->data.booleanLiteral.value == right->data.booleanLiteral.value;
        case ZR_AST_IDENTIFIER_LITERAL:
            return left->data.identifier.name != ZR_NULL &&
                   right->data.identifier.name != ZR_NULL &&
                   ZrCore_String_Equal(left->data.identifier.name, right->data.identifier.name);
        case ZR_AST_BINARY_EXPRESSION:
            if (!loop_assignment_delta_expression_string_equal(
                        left->data.binaryExpression.op.op,
                        right->data.binaryExpression.op.op)) {
                return loop_assignment_delta_expression_is_binary_additive(left) &&
                       loop_assignment_delta_expression_is_binary_additive(right) &&
                       loop_assignment_delta_expression_signed_additive_terms_equal(
                               left,
                               right,
                               targetName);
            }
            if (loop_assignment_delta_expression_equal(
                        left->data.binaryExpression.left,
                        right->data.binaryExpression.left,
                        targetName) &&
                loop_assignment_delta_expression_equal(
                        left->data.binaryExpression.right,
                        right->data.binaryExpression.right,
                        targetName)) {
                return ZR_TRUE;
            }
            if (loop_assignment_delta_expression_string_equal(
                        left->data.binaryExpression.op.op,
                        "*") &&
                ZrParser_TypeInferenceLoopAssignment_DeltaProductTermsEqual(
                        left,
                        right,
                        targetName,
                        loop_assignment_delta_expression_equal)) {
                return ZR_TRUE;
            }
            return loop_assignment_delta_expression_string_equal(
                           left->data.binaryExpression.op.op,
                           "+") &&
                   (loop_assignment_delta_expression_plus_terms_equal(left, right, targetName) ||
                    loop_assignment_delta_expression_signed_additive_terms_equal(
                            left,
                            right,
                            targetName));
        case ZR_AST_LOGICAL_EXPRESSION:
            return loop_assignment_delta_expression_string_equal(
                           left->data.logicalExpression.op,
                           right->data.logicalExpression.op) &&
                   loop_assignment_delta_expression_equal(
                           left->data.logicalExpression.left,
                           right->data.logicalExpression.left,
                           targetName) &&
                   loop_assignment_delta_expression_equal(
                           left->data.logicalExpression.right,
                           right->data.logicalExpression.right,
                           targetName);
        case ZR_AST_UNARY_EXPRESSION:
            return loop_assignment_delta_expression_string_equal(
                           left->data.unaryExpression.op.op,
                           right->data.unaryExpression.op.op) &&
                   loop_assignment_delta_expression_equal(
                           left->data.unaryExpression.argument,
                           right->data.unaryExpression.argument,
                           targetName);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return left->data.typeCastExpression.targetType ==
                           right->data.typeCastExpression.targetType &&
                   loop_assignment_delta_expression_equal(
                           left->data.typeCastExpression.expression,
                           right->data.typeCastExpression.expression,
                           targetName);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return loop_assignment_delta_expression_equal(
                           left->data.conditionalExpression.test,
                           right->data.conditionalExpression.test,
                           targetName) &&
                   loop_assignment_delta_expression_equal(
                           left->data.conditionalExpression.consequent,
                           right->data.conditionalExpression.consequent,
                           targetName) &&
                   loop_assignment_delta_expression_equal(
                           left->data.conditionalExpression.alternate,
                           right->data.conditionalExpression.alternate,
                           targetName);
        case ZR_AST_PRIMARY_EXPRESSION:
            leftMembers = left->data.primaryExpression.members;
            rightMembers = right->data.primaryExpression.members;
            if ((leftMembers != ZR_NULL && leftMembers->count > 0) ||
                (rightMembers != ZR_NULL && rightMembers->count > 0)) {
                return ZR_FALSE;
            }
            return loop_assignment_delta_expression_equal(
                    left->data.primaryExpression.property,
                    right->data.primaryExpression.property,
                    targetName);
        default:
            return ZR_FALSE;
    }
}

TZrBool ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaExpressionsEqual(
        SZrAstNode *left,
        SZrAstNode *right,
        SZrString *targetName) {
    return loop_assignment_delta_expression_equal(left, right, targetName);
}
