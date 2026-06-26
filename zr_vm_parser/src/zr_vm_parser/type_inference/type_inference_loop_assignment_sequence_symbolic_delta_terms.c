#include "type_inference_loop_assignment_sequence_symbolic_delta_terms.h"

#include "type_inference_loop_assignment_self_dependency.h"

static TZrBool loop_assignment_sequence_symbolic_delta_terms_can_cancel(
        const SZrTypeInferenceLoopAssignmentSequenceSymbolicTerm *left,
        const SZrTypeInferenceLoopAssignmentSequenceSymbolicTerm *right,
        SZrString *sequenceName) {
    if (left == ZR_NULL ||
        right == ZR_NULL ||
        sequenceName == ZR_NULL ||
        left->node == ZR_NULL ||
        right->node == ZR_NULL ||
        left->sign != -right->sign ||
        !ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaExpressionsEqual(
                left->node,
                right->node,
                sequenceName)) {
        return ZR_FALSE;
    }

    if (!left->hasCoefficientRange && !right->hasCoefficientRange) {
        return ZR_TRUE;
    }

    return left->hasCoefficientRange &&
           right->hasCoefficientRange &&
           left->coefficientMin == right->coefficientMin &&
           left->coefficientMax == right->coefficientMax &&
           left->coefficientNode != ZR_NULL &&
           right->coefficientNode != ZR_NULL &&
           ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaExpressionsEqual(
                   left->coefficientNode,
                   right->coefficientNode,
                   sequenceName);
}

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicDeltaCancelTerms(
        SZrTypeInferenceLoopAssignmentSequenceSymbolicTerm *terms,
        TZrSize *termCount,
        SZrString *sequenceName) {
    TZrSize leftIndex;
    TZrSize rightIndex;
    TZrSize writeIndex;

    if (terms == ZR_NULL || termCount == ZR_NULL || sequenceName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (leftIndex = 0; leftIndex < *termCount; leftIndex++) {
        if (terms[leftIndex].node == ZR_NULL) {
            continue;
        }
        for (rightIndex = leftIndex + 1;
             rightIndex < *termCount;
             rightIndex++) {
            if (loop_assignment_sequence_symbolic_delta_terms_can_cancel(
                    &terms[leftIndex],
                    &terms[rightIndex],
                    sequenceName)) {
                terms[leftIndex].node = ZR_NULL;
                terms[rightIndex].node = ZR_NULL;
                break;
            }
        }
    }

    for (leftIndex = 0, writeIndex = 0;
         leftIndex < *termCount;
         leftIndex++) {
        if (terms[leftIndex].node != ZR_NULL) {
            terms[writeIndex] = terms[leftIndex];
            writeIndex = writeIndex + 1;
        }
    }
    *termCount = writeIndex;
    return ZR_TRUE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicDeltaCancelCollectedTerms(
        SZrTypeInferenceLoopAssignmentSequenceSymbolicTerm *terms,
        TZrSize *rawTermCount,
        TZrSize *termCount,
        SZrString *sequenceName) {
    TZrSize termCountBefore;
    TZrSize removedTerms;

    if (rawTermCount == ZR_NULL || termCount == ZR_NULL) {
        return ZR_FALSE;
    }

    termCountBefore = *termCount;
    if (!ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicDeltaCancelTerms(
                terms,
                termCount,
                sequenceName)) {
        return ZR_FALSE;
    }

    removedTerms = termCountBefore - *termCount;
    if (*rawTermCount >= removedTerms) {
        *rawTermCount = *rawTermCount - removedTerms;
    } else {
        *rawTermCount = 0;
    }
    return ZR_TRUE;
}
