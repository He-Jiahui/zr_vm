//
// Created by Auto on 2026/06/22.
//

#include "zr_vm_parser/type_system.h"

#include "zr_vm_core/array.h"

#include <string.h>

void ZrParser_NumericRangeSegments_Reset(TZrSize *segmentCount,
                                         SZrNumericRangeSegment *inlineSegments,
                                         SZrArray *extraSegments) {
    if (segmentCount != ZR_NULL) {
        *segmentCount = 0;
    }
    if (inlineSegments != ZR_NULL) {
        memset(inlineSegments,
               0,
               sizeof(SZrNumericRangeSegment) * ZR_PARSER_NUMERIC_RANGE_SEGMENT_CAPACITY);
    }
    if (extraSegments != ZR_NULL && extraSegments->isValid) {
        ZrCore_Array_Empty(extraSegments);
    }
}

void ZrParser_NumericRangeSegments_Free(SZrState *state,
                                        TZrSize *segmentCount,
                                        SZrNumericRangeSegment *inlineSegments,
                                        SZrArray *extraSegments) {
    if (segmentCount != ZR_NULL) {
        *segmentCount = 0;
    }
    if (inlineSegments != ZR_NULL) {
        memset(inlineSegments,
               0,
               sizeof(SZrNumericRangeSegment) * ZR_PARSER_NUMERIC_RANGE_SEGMENT_CAPACITY);
    }
    if (state != ZR_NULL && extraSegments != ZR_NULL && extraSegments->isValid) {
        ZrCore_Array_Free(state, extraSegments);
    } else if (extraSegments != ZR_NULL && !extraSegments->isValid) {
        ZrCore_Array_Construct(extraSegments);
    }
}

TZrBool ZrParser_NumericRangeSegments_Append(SZrState *state,
                                             TZrSize *segmentCount,
                                             SZrNumericRangeSegment *inlineSegments,
                                             SZrArray *extraSegments,
                                             const SZrNumericRangeSegment *segment) {
    TZrSize index;

    if (state == ZR_NULL ||
        segmentCount == ZR_NULL ||
        inlineSegments == ZR_NULL ||
        extraSegments == ZR_NULL ||
        segment == ZR_NULL ||
        segment->minValue > segment->maxValue) {
        return ZR_FALSE;
    }

    index = *segmentCount;
    if (index < ZR_PARSER_NUMERIC_RANGE_SEGMENT_CAPACITY) {
        inlineSegments[index] = *segment;
    } else {
        if (!extraSegments->isValid) {
            ZrCore_Array_Init(state,
                              extraSegments,
                              sizeof(SZrNumericRangeSegment),
                              ZR_PARSER_NUMERIC_RANGE_SEGMENT_CAPACITY);
        }
        ZrCore_Array_Push(state, extraSegments, (TZrPtr)segment);
    }
    *segmentCount = index + 1;
    return ZR_TRUE;
}

const SZrNumericRangeSegment *ZrParser_NumericRangeSegments_At(
        TZrSize segmentCount,
        const SZrNumericRangeSegment *inlineSegments,
        const SZrArray *extraSegments,
        TZrSize index) {
    TZrSize extraIndex;

    if (inlineSegments == ZR_NULL || index >= segmentCount) {
        return ZR_NULL;
    }
    if (index < ZR_PARSER_NUMERIC_RANGE_SEGMENT_CAPACITY) {
        return &inlineSegments[index];
    }

    extraIndex = index - ZR_PARSER_NUMERIC_RANGE_SEGMENT_CAPACITY;
    if (extraSegments == ZR_NULL ||
        !extraSegments->isValid ||
        extraIndex >= extraSegments->length) {
        return ZR_NULL;
    }
    return (const SZrNumericRangeSegment *)ZrCore_Array_Get((SZrArray *)extraSegments, extraIndex);
}

TZrBool ZrParser_NumericRangeSegments_Copy(SZrState *state,
                                           TZrSize *dstSegmentCount,
                                           SZrNumericRangeSegment *dstInlineSegments,
                                           SZrArray *dstExtraSegments,
                                           TZrSize srcSegmentCount,
                                           const SZrNumericRangeSegment *srcInlineSegments,
                                           const SZrArray *srcExtraSegments) {
    TZrSize index;

    if (state == ZR_NULL ||
        dstSegmentCount == ZR_NULL ||
        dstInlineSegments == ZR_NULL ||
        dstExtraSegments == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_NumericRangeSegments_Reset(dstSegmentCount, dstInlineSegments, dstExtraSegments);
    for (index = 0; index < srcSegmentCount; index++) {
        const SZrNumericRangeSegment *segment =
            ZrParser_NumericRangeSegments_At(srcSegmentCount,
                                             srcInlineSegments,
                                             srcExtraSegments,
                                             index);
        if (segment == ZR_NULL ||
            !ZrParser_NumericRangeSegments_Append(state,
                                                  dstSegmentCount,
                                                  dstInlineSegments,
                                                  dstExtraSegments,
                                                  segment)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

void ZrParser_InferredType_ResetRangeSegments(SZrInferredType *type) {
    if (type == ZR_NULL) {
        return;
    }

    ZrParser_NumericRangeSegments_Reset(&type->rangeSegmentCount,
                                        type->rangeSegments,
                                        &type->rangeExtraSegments);
}

TZrBool ZrParser_InferredType_AppendRangeSegment(SZrState *state,
                                                 SZrInferredType *type,
                                                 TZrInt64 minValue,
                                                 TZrInt64 maxValue) {
    SZrNumericRangeSegment segment;

    if (type == ZR_NULL) {
        return ZR_FALSE;
    }

    segment.minValue = minValue;
    segment.maxValue = maxValue;
    return ZrParser_NumericRangeSegments_Append(state,
                                                &type->rangeSegmentCount,
                                                type->rangeSegments,
                                                &type->rangeExtraSegments,
                                                &segment);
}

TZrBool ZrParser_InferredType_CopyRangeSegments(SZrState *state,
                                                SZrInferredType *dest,
                                                const SZrInferredType *src) {
    if (dest == ZR_NULL || src == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrParser_NumericRangeSegments_Copy(state,
                                              &dest->rangeSegmentCount,
                                              dest->rangeSegments,
                                              &dest->rangeExtraSegments,
                                              src->rangeSegmentCount,
                                              src->rangeSegments,
                                              &src->rangeExtraSegments);
}

const SZrNumericRangeSegment *ZrParser_InferredType_RangeSegmentAt(
        const SZrInferredType *type,
        TZrSize index) {
    if (type == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_NumericRangeSegments_At(type->rangeSegmentCount,
                                            type->rangeSegments,
                                            &type->rangeExtraSegments,
                                            index);
}
