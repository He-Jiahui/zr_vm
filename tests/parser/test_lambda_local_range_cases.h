#ifndef ZR_VM_TEST_LAMBDA_LOCAL_RANGE_CASES_H
#define ZR_VM_TEST_LAMBDA_LOCAL_RANGE_CASES_H

#include <stdint.h>

static TZrBool lambda_local_range_matches(
        const SZrFunctionTypedLocalBinding *local, SZrFileRange range) {
    if (range.start.line < 0 || range.start.column < 0 ||
        range.end.line < 0 || range.end.column < 0) {
        return ZR_FALSE;
    }
    return local->declarationStartLine == (TZrUInt32)range.start.line &&
           local->declarationStartColumn == (TZrUInt32)range.start.column &&
           local->declarationEndLine == (TZrUInt32)range.end.line &&
           local->declarationEndColumn == (TZrUInt32)range.end.column;
}

static void assert_lambda_local_range_coordinate_boundaries(void) {
    SZrFunctionTypedLocalBinding local = {0};
    SZrFileRange range = {0};
    TZrUInt32 acceptedNegativeCoordinates = 0U;
    TZrInt32 *sourceCoordinates[] = {
            &range.start.line, &range.start.column,
            &range.end.line, &range.end.column};
    TZrUInt32 *metadataCoordinates[] = {
            &local.declarationStartLine, &local.declarationStartColumn,
            &local.declarationEndLine, &local.declarationEndColumn};

    TEST_ASSERT_TRUE(lambda_local_range_matches(&local, range));
    for (TZrSize index = 0U; index < 4U; index++) {
        *sourceCoordinates[index] = INT32_MAX;
        *metadataCoordinates[index] = (TZrUInt32)INT32_MAX;
    }
    TEST_ASSERT_TRUE(lambda_local_range_matches(&local, range));
    for (TZrSize index = 0U; index < 4U; index++) {
        *sourceCoordinates[index] = 1;
        *metadataCoordinates[index] = 1U;
    }

    /* Evaluate every coordinate before Unity can stop this test on failure. */
    for (TZrSize index = 0U; index < 4U; index++) {
        *sourceCoordinates[index] = -1;
        *metadataCoordinates[index] = UINT32_MAX;
        if (lambda_local_range_matches(&local, range)) {
            acceptedNegativeCoordinates |= (TZrUInt32)1U << index;
        }
        *sourceCoordinates[index] = 1;
        *metadataCoordinates[index] = 1U;
    }
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(
            0U, acceptedNegativeCoordinates,
            "negative declaration coordinates must not match unsigned metadata");
}

#endif
