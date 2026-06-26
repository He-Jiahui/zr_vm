#include "type_inference_loop_assignment_delta_product.h"

#include <string.h>

#include "zr_vm_common/zr_type_conf.h"

enum {
    ZR_LOOP_ASSIGNMENT_DELTA_PRODUCT_TERM_LIMIT = 8
};

static TZrBool loop_assignment_delta_product_string_equal(const TZrChar *left,
                                                          const TZrChar *right) {
    return left != ZR_NULL && right != ZR_NULL && strcmp(left, right) == 0;
}

static TZrBool loop_assignment_delta_product_is_binary_multiply(SZrAstNode *node) {
    return node != ZR_NULL &&
           node->type == ZR_AST_BINARY_EXPRESSION &&
           loop_assignment_delta_product_string_equal(node->data.binaryExpression.op.op, "*");
}

static TZrBool loop_assignment_delta_product_is_binary_plus(SZrAstNode *node) {
    return node != ZR_NULL &&
           node->type == ZR_AST_BINARY_EXPRESSION &&
           loop_assignment_delta_product_string_equal(node->data.binaryExpression.op.op, "+");
}

static TZrBool loop_assignment_delta_product_is_binary_minus(SZrAstNode *node) {
    return node != ZR_NULL &&
           node->type == ZR_AST_BINARY_EXPRESSION &&
           loop_assignment_delta_product_string_equal(node->data.binaryExpression.op.op, "-");
}

static TZrBool loop_assignment_delta_product_is_binary_divide(SZrAstNode *node) {
    return node != ZR_NULL &&
           node->type == ZR_AST_BINARY_EXPRESSION &&
           loop_assignment_delta_product_string_equal(node->data.binaryExpression.op.op, "/");
}

static TZrBool loop_assignment_delta_product_is_binary_modulo(SZrAstNode *node) {
    return node != ZR_NULL &&
           node->type == ZR_AST_BINARY_EXPRESSION &&
           loop_assignment_delta_product_string_equal(node->data.binaryExpression.op.op, "%");
}

static TZrBool loop_assignment_delta_product_is_unary_negative(SZrAstNode *node) {
    return node != ZR_NULL &&
           node->type == ZR_AST_UNARY_EXPRESSION &&
           loop_assignment_delta_product_string_equal(node->data.unaryExpression.op.op, "-");
}

static TZrBool loop_assignment_delta_product_is_unary_positive(SZrAstNode *node) {
    return node != ZR_NULL &&
           node->type == ZR_AST_UNARY_EXPRESSION &&
           loop_assignment_delta_product_string_equal(node->data.unaryExpression.op.op, "+");
}

static TZrBool loop_assignment_delta_product_collect_terms(SZrAstNode *node,
                                                           SZrAstNode **terms,
                                                           TZrSize *termCount) {
    if (node == ZR_NULL || terms == ZR_NULL || termCount == ZR_NULL) {
        return ZR_FALSE;
    }

    if (loop_assignment_delta_product_is_binary_multiply(node)) {
        return loop_assignment_delta_product_collect_terms(
                       node->data.binaryExpression.left,
                       terms,
                       termCount) &&
               loop_assignment_delta_product_collect_terms(
                       node->data.binaryExpression.right,
                       terms,
                       termCount);
    }

    if (*termCount >= ZR_LOOP_ASSIGNMENT_DELTA_PRODUCT_TERM_LIMIT) {
        return ZR_FALSE;
    }

    terms[*termCount] = node;
    *termCount = *termCount + 1;
    return ZR_TRUE;
}

static TZrBool loop_assignment_delta_product_checked_multiply_int64(TZrInt64 left,
                                                                    TZrInt64 right,
                                                                    TZrInt64 *outValue) {
    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if (left == 0 || right == 0) {
        *outValue = 0;
        return ZR_TRUE;
    }
    if (left == -1) {
        if (right == ZR_TYPE_RANGE_INT64_MIN) {
            return ZR_FALSE;
        }
        *outValue = -right;
        return ZR_TRUE;
    }
    if (right == -1) {
        if (left == ZR_TYPE_RANGE_INT64_MIN) {
            return ZR_FALSE;
        }
        *outValue = -left;
        return ZR_TRUE;
    }
    if ((left > 0 && right > 0 && left > ZR_TYPE_RANGE_INT64_MAX / right) ||
        (left > 0 && right < 0 && right < ZR_TYPE_RANGE_INT64_MIN / left) ||
        (left < 0 && right > 0 && left < ZR_TYPE_RANGE_INT64_MIN / right) ||
        (left < 0 && right < 0 && right < ZR_TYPE_RANGE_INT64_MAX / left)) {
        return ZR_FALSE;
    }

    *outValue = left * right;
    return ZR_TRUE;
}

static TZrBool loop_assignment_delta_product_checked_add_int64(TZrInt64 left,
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

static TZrBool loop_assignment_delta_product_checked_subtract_int64(TZrInt64 left,
                                                                    TZrInt64 right,
                                                                    TZrInt64 *outValue) {
    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if ((right > 0 && left < ZR_TYPE_RANGE_INT64_MIN + right) ||
        (right < 0 && left > ZR_TYPE_RANGE_INT64_MAX + right)) {
        return ZR_FALSE;
    }

    *outValue = left - right;
    return ZR_TRUE;
}

static TZrBool loop_assignment_delta_product_checked_divide_int64(TZrInt64 left,
                                                                  TZrInt64 right,
                                                                  TZrInt64 *outValue) {
    if (outValue == ZR_NULL ||
        right == 0 ||
        (left == ZR_TYPE_RANGE_INT64_MIN && right == -1)) {
        return ZR_FALSE;
    }

    *outValue = left / right;
    return ZR_TRUE;
}

static TZrBool loop_assignment_delta_product_checked_modulo_int64(TZrInt64 left,
                                                                  TZrInt64 right,
                                                                  TZrInt64 *outValue) {
    if (outValue == ZR_NULL ||
        right == 0 ||
        (left == ZR_TYPE_RANGE_INT64_MIN && right == -1)) {
        return ZR_FALSE;
    }

    *outValue = left % right;
    return ZR_TRUE;
}

static TZrBool loop_assignment_delta_product_fold_integer_literal_factor_at_depth(
        SZrAstNode *term,
        TZrSize depth,
        TZrInt64 *outValue) {
    TZrInt64 leftValue = 0;
    TZrInt64 rightValue = 0;

    if (term == ZR_NULL ||
        outValue == ZR_NULL ||
        depth >= ZR_LOOP_ASSIGNMENT_DELTA_PRODUCT_TERM_LIMIT) {
        return ZR_FALSE;
    }

    if (term->type == ZR_AST_INTEGER_LITERAL) {
        *outValue = term->data.integerLiteral.value;
        return ZR_TRUE;
    }

    if (loop_assignment_delta_product_is_unary_positive(term)) {
        return loop_assignment_delta_product_fold_integer_literal_factor_at_depth(
                term->data.unaryExpression.argument,
                depth + 1,
                outValue);
    }

    if (loop_assignment_delta_product_is_unary_negative(term)) {
        if (!loop_assignment_delta_product_fold_integer_literal_factor_at_depth(
                    term->data.unaryExpression.argument,
                    depth + 1,
                    &leftValue)) {
            return ZR_FALSE;
        }
        return loop_assignment_delta_product_checked_multiply_int64(
                leftValue,
                -1,
                outValue);
    }

    if (loop_assignment_delta_product_is_binary_plus(term)) {
        if (!loop_assignment_delta_product_fold_integer_literal_factor_at_depth(
                    term->data.binaryExpression.left,
                    depth + 1,
                    &leftValue) ||
            !loop_assignment_delta_product_fold_integer_literal_factor_at_depth(
                    term->data.binaryExpression.right,
                    depth + 1,
                    &rightValue)) {
            return ZR_FALSE;
        }
        return loop_assignment_delta_product_checked_add_int64(
                leftValue,
                rightValue,
                outValue);
    }

    if (loop_assignment_delta_product_is_binary_minus(term)) {
        if (!loop_assignment_delta_product_fold_integer_literal_factor_at_depth(
                    term->data.binaryExpression.left,
                    depth + 1,
                    &leftValue) ||
            !loop_assignment_delta_product_fold_integer_literal_factor_at_depth(
                    term->data.binaryExpression.right,
                    depth + 1,
                    &rightValue)) {
            return ZR_FALSE;
        }
        return loop_assignment_delta_product_checked_subtract_int64(
                leftValue,
                rightValue,
                outValue);
    }

    if (loop_assignment_delta_product_is_binary_divide(term)) {
        if (!loop_assignment_delta_product_fold_integer_literal_factor_at_depth(
                    term->data.binaryExpression.left,
                    depth + 1,
                    &leftValue) ||
            !loop_assignment_delta_product_fold_integer_literal_factor_at_depth(
                    term->data.binaryExpression.right,
                    depth + 1,
                    &rightValue)) {
            return ZR_FALSE;
        }
        return loop_assignment_delta_product_checked_divide_int64(
                leftValue,
                rightValue,
                outValue);
    }

    if (loop_assignment_delta_product_is_binary_modulo(term)) {
        if (!loop_assignment_delta_product_fold_integer_literal_factor_at_depth(
                    term->data.binaryExpression.left,
                    depth + 1,
                    &leftValue) ||
            !loop_assignment_delta_product_fold_integer_literal_factor_at_depth(
                    term->data.binaryExpression.right,
                    depth + 1,
                    &rightValue)) {
            return ZR_FALSE;
        }
        return loop_assignment_delta_product_checked_modulo_int64(
                leftValue,
                rightValue,
                outValue);
    }

    return ZR_FALSE;
}

static TZrBool loop_assignment_delta_product_fold_integer_literal_factor(
        SZrAstNode *term,
        TZrInt64 *outValue) {
    return loop_assignment_delta_product_fold_integer_literal_factor_at_depth(
            term,
            0,
            outValue);
}

static TZrBool loop_assignment_delta_product_normalize_unary_sign(
        SZrAstNode **term,
        TZrInt64 *integerLiteralProduct) {
    TZrSize depth = 0;

    if (term == ZR_NULL || *term == ZR_NULL || integerLiteralProduct == ZR_NULL) {
        return ZR_FALSE;
    }

    while (loop_assignment_delta_product_is_unary_positive(*term) ||
           loop_assignment_delta_product_is_unary_negative(*term)) {
        SZrAstNode *current = *term;

        if (depth >= ZR_LOOP_ASSIGNMENT_DELTA_PRODUCT_TERM_LIMIT) {
            return ZR_FALSE;
        }
        depth++;

        if (loop_assignment_delta_product_is_unary_negative(current)) {
            TZrInt64 nextProduct = 0;

            if (!loop_assignment_delta_product_checked_multiply_int64(
                        *integerLiteralProduct,
                        -1,
                        &nextProduct)) {
                return ZR_FALSE;
            }
            *integerLiteralProduct = nextProduct;
        }

        *term = current->data.unaryExpression.argument;
        if (*term == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool loop_assignment_delta_product_split_terms(
        SZrAstNode **sourceTerms,
        TZrSize sourceCount,
        SZrAstNode **expressionTerms,
        TZrSize *expressionTermCount,
        TZrInt64 *integerLiteralProduct) {
    TZrSize index;

    if (sourceTerms == ZR_NULL ||
        expressionTerms == ZR_NULL ||
        expressionTermCount == ZR_NULL ||
        integerLiteralProduct == ZR_NULL) {
        return ZR_FALSE;
    }

    *expressionTermCount = 0;
    *integerLiteralProduct = 1;
    for (index = 0; index < sourceCount; index++) {
        SZrAstNode *term = sourceTerms[index];
        TZrInt64 foldedIntegerLiteral = 0;

        if (term == ZR_NULL) {
            return ZR_FALSE;
        }
        if (!loop_assignment_delta_product_normalize_unary_sign(
                    &term,
                    integerLiteralProduct)) {
            return ZR_FALSE;
        }
        if (loop_assignment_delta_product_fold_integer_literal_factor(
                    term,
                    &foldedIntegerLiteral)) {
            TZrInt64 nextProduct = 0;

            if (!loop_assignment_delta_product_checked_multiply_int64(
                        *integerLiteralProduct,
                        foldedIntegerLiteral,
                        &nextProduct)) {
                return ZR_FALSE;
            }
            *integerLiteralProduct = nextProduct;
            continue;
        }

        if (*expressionTermCount >= ZR_LOOP_ASSIGNMENT_DELTA_PRODUCT_TERM_LIMIT) {
            return ZR_FALSE;
        }
        expressionTerms[*expressionTermCount] = term;
        *expressionTermCount = *expressionTermCount + 1;
    }

    return ZR_TRUE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_DeltaProductTermsEqual(
        SZrAstNode *left,
        SZrAstNode *right,
        SZrString *targetName,
        FZrLoopAssignmentDeltaProductTermEqual termEqual) {
    SZrAstNode *leftTerms[ZR_LOOP_ASSIGNMENT_DELTA_PRODUCT_TERM_LIMIT];
    SZrAstNode *rightTerms[ZR_LOOP_ASSIGNMENT_DELTA_PRODUCT_TERM_LIMIT];
    SZrAstNode *leftExpressionTerms[ZR_LOOP_ASSIGNMENT_DELTA_PRODUCT_TERM_LIMIT];
    SZrAstNode *rightExpressionTerms[ZR_LOOP_ASSIGNMENT_DELTA_PRODUCT_TERM_LIMIT];
    TZrBool matched[ZR_LOOP_ASSIGNMENT_DELTA_PRODUCT_TERM_LIMIT];
    TZrSize leftCount = 0;
    TZrSize rightCount = 0;
    TZrSize leftExpressionCount = 0;
    TZrSize rightExpressionCount = 0;
    TZrInt64 leftIntegerLiteralProduct = 1;
    TZrInt64 rightIntegerLiteralProduct = 1;
    TZrSize leftIndex;
    TZrSize rightIndex;

    if (targetName == ZR_NULL ||
        termEqual == ZR_NULL ||
        !loop_assignment_delta_product_collect_terms(left, leftTerms, &leftCount) ||
        !loop_assignment_delta_product_collect_terms(right, rightTerms, &rightCount) ||
        !loop_assignment_delta_product_split_terms(
                leftTerms,
                leftCount,
                leftExpressionTerms,
                &leftExpressionCount,
                &leftIntegerLiteralProduct) ||
        !loop_assignment_delta_product_split_terms(
                rightTerms,
                rightCount,
                rightExpressionTerms,
                &rightExpressionCount,
                &rightIntegerLiteralProduct) ||
        leftExpressionCount != rightExpressionCount ||
        leftIntegerLiteralProduct != rightIntegerLiteralProduct) {
        return ZR_FALSE;
    }

    for (rightIndex = 0; rightIndex < rightExpressionCount; rightIndex++) {
        matched[rightIndex] = ZR_FALSE;
    }

    for (leftIndex = 0; leftIndex < leftExpressionCount; leftIndex++) {
        TZrBool found = ZR_FALSE;

        for (rightIndex = 0; rightIndex < rightExpressionCount; rightIndex++) {
            if (!matched[rightIndex] &&
                termEqual(
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
