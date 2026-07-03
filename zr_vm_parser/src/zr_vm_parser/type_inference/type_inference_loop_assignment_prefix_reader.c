#include "type_inference_loop_assignment_prefix_reader.h"
#include "type_inference_loop_assignment_prefix_offset.h"
#include "type_inference_loop_assignment_prefix_target_guard.h"
#include "type_inference_loop_assignment_prefix_zero_inclusive_negative.h"
#include "type_inference_loop_assignment_self_dependency.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_parser/compiler.h"

#include <string.h>

static TZrBool type_inference_loop_assignment_prefix_reader_operator_is(
        const TZrChar *actual,
        const TZrChar *expected) {
    return actual != ZR_NULL && expected != ZR_NULL && strcmp(actual, expected) == 0;
}

static SZrString *type_inference_loop_assignment_prefix_reader_supported_target_name(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize readerIndex,
        const SZrAstNode *expression) {
    const SZrBinaryExpression *binary;
    const SZrUnaryExpression *unary;

    if (expression == ZR_NULL) {
        return ZR_NULL;
    }

    if (expression->type == ZR_AST_IDENTIFIER_LITERAL &&
        expression->data.identifier.name != ZR_NULL) {
        SZrString *targetName = expression->data.identifier.name;

        return ZrParser_TypeInferenceLoopAssignment_PrefixReaderPlanContains(plan, targetName) &&
               ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetIsSupported(
                       plan,
                       readerIndex,
                       targetName,
                       ZR_NULL,
                       ZR_NULL)
                        ? targetName
                        : ZR_NULL;
    }
    if (expression->type == ZR_AST_UNARY_EXPRESSION) {
        unary = &expression->data.unaryExpression;
        if (type_inference_loop_assignment_prefix_reader_operator_is(unary->op.op, "+")) {
            return type_inference_loop_assignment_prefix_reader_supported_target_name(
                    cs,
                    plan,
                    readerIndex,
                    unary->argument);
        }
        return ZR_NULL;
    }
    if (expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_NULL;
    }

    binary = &expression->data.binaryExpression;
    if (type_inference_loop_assignment_prefix_reader_operator_is(binary->op.op, "+")) {
        if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                cs,
                binary->right)) {
            return type_inference_loop_assignment_prefix_reader_supported_target_name(
                        cs,
                        plan,
                        readerIndex,
                        binary->left);
        }
        if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                cs,
                binary->left)) {
            return type_inference_loop_assignment_prefix_reader_supported_target_name(
                        cs,
                        plan,
                        readerIndex,
                        binary->right);
        }
        return ZR_NULL;
    }
    if (type_inference_loop_assignment_prefix_reader_operator_is(binary->op.op, "-")) {
        if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                cs,
                binary->right)) {
            return type_inference_loop_assignment_prefix_reader_supported_target_name(
                       cs,
                       plan,
                       readerIndex,
                       binary->left);
        }
        return ZR_NULL;
    }
    return ZR_NULL;
}

static TZrBool type_inference_loop_assignment_prefix_reader_expression_reads_supported_target(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        TZrSize readerIndex,
        const SZrAstNode *expression) {
    const SZrBinaryExpression *binary;
    const SZrUnaryExpression *unary;
    SZrString *targetName;
    TZrInt64 offsetMin;
    TZrInt64 offsetMax;

    if (type_inference_loop_assignment_prefix_reader_supported_target_name(
            cs,
            plan,
            readerIndex,
            expression) != ZR_NULL) {
        return ZR_TRUE;
    }
    if (expression == ZR_NULL) {
        return ZR_FALSE;
    }
    if (expression->type == ZR_AST_UNARY_EXPRESSION) {
        unary = &expression->data.unaryExpression;
        return type_inference_loop_assignment_prefix_reader_operator_is(unary->op.op, "+") &&
               type_inference_loop_assignment_prefix_reader_expression_reads_supported_target(
                       cs,
                       plan,
                       readerIndex,
                       unary->argument);
    }
    if (expression->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    binary = &expression->data.binaryExpression;
    if (type_inference_loop_assignment_prefix_reader_operator_is(binary->op.op, "+")) {
        if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                cs,
                binary->right)) {
            return type_inference_loop_assignment_prefix_reader_expression_reads_supported_target(
                    cs,
                    plan,
                    readerIndex,
                    binary->left);
        }
        if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
                cs,
                binary->left)) {
            return type_inference_loop_assignment_prefix_reader_expression_reads_supported_target(
                    cs,
                    plan,
                    readerIndex,
                    binary->right);
        }
        if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                cs,
                binary->right,
                ZR_NULL,
                &offsetMax)) {
            targetName = type_inference_loop_assignment_prefix_reader_supported_target_name(
                    cs,
                    plan,
                    readerIndex,
                    binary->left);
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetAddOffsetIsSupported(
                    cs,
                    plan,
                    readerIndex,
                    targetName,
                    offsetMax)) {
                return ZR_TRUE;
            }
        }
        if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
                cs,
                binary->left,
                ZR_NULL,
                &offsetMax)) {
            targetName = type_inference_loop_assignment_prefix_reader_supported_target_name(
                    cs,
                    plan,
                    readerIndex,
                    binary->right);
            if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetAddOffsetIsSupported(
                    cs,
                    plan,
                    readerIndex,
                    targetName,
                    offsetMax)) {
                return ZR_TRUE;
            }
        }
        if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedNegativeOffset(
                cs,
                binary->right,
                &offsetMin,
                &offsetMax)) {
            targetName = type_inference_loop_assignment_prefix_reader_supported_target_name(
                    cs,
                    plan,
                    readerIndex,
                    binary->left);
            return ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetAddNegativeOffsetIsSupported(
                    cs,
                    plan,
                    readerIndex,
                    targetName,
                    offsetMin,
                    offsetMax);
        }
        if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedNegativeOffset(
                cs,
                binary->left,
                &offsetMin,
                &offsetMax)) {
            targetName = type_inference_loop_assignment_prefix_reader_supported_target_name(
                    cs,
                    plan,
                    readerIndex,
                    binary->right);
            return ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetAddNegativeOffsetIsSupported(
                    cs,
                    plan,
                    readerIndex,
                    targetName,
                    offsetMin,
                    offsetMax);
        }
        if (ZrParser_TypeInferenceLoopAssignment_PrefixZeroInclusiveNegativeExpressionIsBinding(
                cs,
                binary->right,
                &offsetMin,
                &offsetMax)) {
            targetName = type_inference_loop_assignment_prefix_reader_supported_target_name(
                    cs,
                    plan,
                    readerIndex,
                    binary->left);
            return ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetAddZeroInclusiveNegativeOffsetIsSupported(
                    cs,
                    plan,
                    readerIndex,
                    targetName,
                    offsetMin,
                    offsetMax);
        }
        if (ZrParser_TypeInferenceLoopAssignment_PrefixZeroInclusiveNegativeExpressionIsBinding(
                cs,
                binary->left,
                &offsetMin,
                &offsetMax)) {
            targetName = type_inference_loop_assignment_prefix_reader_supported_target_name(
                    cs,
                    plan,
                    readerIndex,
                    binary->right);
            return ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetAddZeroInclusiveNegativeOffsetIsSupported(
                    cs,
                    plan,
                    readerIndex,
                    targetName,
                    offsetMin,
                    offsetMax);
        }
        return ZR_FALSE;
    }
    if (!type_inference_loop_assignment_prefix_reader_operator_is(binary->op.op, "-")) {
        return ZR_FALSE;
    }
    if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsExactZeroValue(
            cs,
            binary->right)) {
        return type_inference_loop_assignment_prefix_reader_expression_reads_supported_target(
                cs,
                plan,
                readerIndex,
                binary->left);
    }
    if (ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedOffset(
            cs,
            binary->right,
            ZR_NULL,
            &offsetMax)) {
        targetName = type_inference_loop_assignment_prefix_reader_supported_target_name(
                cs,
                plan,
                readerIndex,
                binary->left);
        return ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetSubtractOffsetIsSupported(
                cs,
                plan,
                readerIndex,
                targetName,
                offsetMax);
    }
    if (ZrParser_TypeInferenceLoopAssignment_PrefixZeroInclusiveNegativeExpressionIsBinding(
            cs,
            binary->right,
            &offsetMin,
            &offsetMax) &&
        offsetMax == 0 &&
        offsetMin != ZR_TYPE_RANGE_INT64_MIN) {
        targetName = type_inference_loop_assignment_prefix_reader_supported_target_name(
                cs,
                plan,
                readerIndex,
                binary->left);
        return ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetAddOffsetIsSupported(
                cs,
                plan,
                readerIndex,
                targetName,
                -offsetMin);
    }
    if (!ZrParser_TypeInferenceLoopAssignment_PrefixReaderExpressionIsSupportedBoundedNegativeOffset(
            cs,
            binary->right,
            &offsetMin,
            &offsetMax) ||
        offsetMin == ZR_TYPE_RANGE_INT64_MIN) {
        return ZR_FALSE;
    }

    targetName = type_inference_loop_assignment_prefix_reader_supported_target_name(
            cs,
            plan,
            readerIndex,
            binary->left);
    return ZrParser_TypeInferenceLoopAssignment_PrefixReaderTargetAddOffsetIsSupported(
            cs,
            plan,
            readerIndex,
            targetName,
            -offsetMin);
}

TZrBool ZrParser_TypeInferenceLoopAssignment_PrefixReaderRhsIsSupported(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        const SZrTypeInferenceLoopAssignmentStep *step,
        TZrSize stepIndex) {
    if (cs == ZR_NULL ||
        plan == ZR_NULL ||
        step == ZR_NULL ||
        step->right == ZR_NULL) {
        return ZR_FALSE;
    }

    return type_inference_loop_assignment_prefix_reader_expression_reads_supported_target(
            cs,
            plan,
            stepIndex,
            step->right);
}
