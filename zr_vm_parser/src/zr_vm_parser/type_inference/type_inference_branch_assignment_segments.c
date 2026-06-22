//
// Created by Auto on 2026/06/23.
//

#include "type_inference_branch_assignment_segments.h"

#include "zr_vm_parser/compiler.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/array.h"

static TZrBool branch_assignment_segments_append_type(SZrState *state,
                                                      SZrArray *segments,
                                                      const SZrInferredType *type) {
    TZrSize index;

    if (state == ZR_NULL ||
        segments == ZR_NULL ||
        type == ZR_NULL ||
        !type->hasRangeConstraint) {
        return ZR_FALSE;
    }

    if (type->rangeSegmentCount == 0) {
        SZrNumericRangeSegment segment;

        segment.minValue = type->minValue;
        segment.maxValue = type->maxValue;
        ZrCore_Array_Push(state, segments, &segment);
        return ZR_TRUE;
    }

    for (index = 0; index < type->rangeSegmentCount; index++) {
        const SZrNumericRangeSegment *segment = ZrParser_InferredType_RangeSegmentAt(type, index);
        if (segment == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Array_Push(state, segments, (TZrPtr)segment);
    }
    return ZR_TRUE;
}

static void branch_assignment_segments_sort(SZrArray *segments) {
    TZrSize index;

    if (segments == ZR_NULL || !segments->isValid) {
        return;
    }

    for (index = 1; index < segments->length; index++) {
        SZrNumericRangeSegment current =
                *(SZrNumericRangeSegment *)ZrCore_Array_Get(segments, index);
        TZrSize insert = index;

        while (insert > 0) {
            const SZrNumericRangeSegment *previous =
                    (const SZrNumericRangeSegment *)ZrCore_Array_Get(segments, insert - 1);
            SZrNumericRangeSegment previousValue;

            if (previous->minValue < current.minValue ||
                (previous->minValue == current.minValue &&
                 previous->maxValue <= current.maxValue)) {
                break;
            }
            previousValue = *previous;
            ZrCore_Array_Set(segments, insert, &previousValue);
            insert--;
        }
        ZrCore_Array_Set(segments, insert, &current);
    }
}

static TZrBool branch_assignment_segments_connected(const SZrNumericRangeSegment *left,
                                                    const SZrNumericRangeSegment *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }
    if (right->minValue <= left->maxValue) {
        return ZR_TRUE;
    }
    return left->maxValue != ZR_TYPE_RANGE_INT64_MAX &&
           right->minValue == left->maxValue + 1;
}

TZrBool ZrParser_TypeInferenceBranchAssignment_ApplyJoinedSegments(SZrCompilerState *cs,
                                                                   const SZrInferredType *thenType,
                                                                   const SZrInferredType *elseType,
                                                                   SZrInferredType *outType) {
    SZrArray segments;
    SZrArray mergedSegments;
    TZrBool segmentsInitialized = ZR_FALSE;
    TZrBool mergedInitialized = ZR_FALSE;
    TZrBool success = ZR_FALSE;
    TZrSize index;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        thenType == ZR_NULL ||
        elseType == ZR_NULL ||
        outType == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(cs->state, &segments, sizeof(SZrNumericRangeSegment), 4);
    segmentsInitialized = ZR_TRUE;
    ZrCore_Array_Init(cs->state, &mergedSegments, sizeof(SZrNumericRangeSegment), 4);
    mergedInitialized = ZR_TRUE;

    if (!branch_assignment_segments_append_type(cs->state, &segments, thenType) ||
        !branch_assignment_segments_append_type(cs->state, &segments, elseType)) {
        goto cleanup;
    }

    branch_assignment_segments_sort(&segments);
    for (index = 0; index < segments.length; index++) {
        const SZrNumericRangeSegment *segment =
                (const SZrNumericRangeSegment *)ZrCore_Array_Get(&segments, index);
        if (segment == ZR_NULL) {
            goto cleanup;
        }
        if (mergedSegments.length == 0) {
            ZrCore_Array_Push(cs->state, &mergedSegments, (TZrPtr)segment);
        } else {
            SZrNumericRangeSegment *last =
                    (SZrNumericRangeSegment *)ZrCore_Array_Get(&mergedSegments, mergedSegments.length - 1);
            if (branch_assignment_segments_connected(last, segment)) {
                if (segment->maxValue > last->maxValue) {
                    last->maxValue = segment->maxValue;
                }
            } else {
                ZrCore_Array_Push(cs->state, &mergedSegments, (TZrPtr)segment);
            }
        }
    }

    ZrParser_InferredType_ResetRangeSegments(outType);
    if (mergedSegments.length > 1) {
        for (index = 0; index < mergedSegments.length; index++) {
            const SZrNumericRangeSegment *segment =
                    (const SZrNumericRangeSegment *)ZrCore_Array_Get(&mergedSegments, index);
            if (segment == ZR_NULL ||
                !ZrParser_InferredType_AppendRangeSegment(cs->state,
                                                          outType,
                                                          segment->minValue,
                                                          segment->maxValue)) {
                goto cleanup;
            }
        }
    }
    success = ZR_TRUE;

cleanup:
    if (mergedInitialized) {
        ZrCore_Array_Free(cs->state, &mergedSegments);
    }
    if (segmentsInitialized) {
        ZrCore_Array_Free(cs->state, &segments);
    }
    return success;
}
