#include "zr_vm_core/batch_contract.h"
#include "zr_vm_library/batch_protocol.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static TZrBool add_one(const TZrByte *input, TZrByte *output,
                       TZrSize size, TZrPtr userData) {
    TZrSize i;
    (void)userData;
    for (i = 0u; i < size; ++i) output[i] = (TZrByte)(input[i] + 1u);
    return ZR_TRUE;
}

static TZrBool add_bytes(const TZrByte *left, const TZrByte *right,
                         TZrByte *output, TZrSize size, TZrPtr userData) {
    TZrSize i;
    (void)userData;
    for (i = 0u; i < size; ++i) output[i] = (TZrByte)(left[i] + right[i]);
    return ZR_TRUE;
}

static TZrBool fail_at_two(const TZrByte *input, TZrByte *output,
                           TZrSize size, TZrPtr userData) {
    TZrSize *calls = (TZrSize *)userData;
    (void)size;
    (*calls)++;
    if (*calls == 3u) return ZR_FALSE;
    *output = (TZrByte)(*input + 1u);
    return ZR_TRUE;
}

static SZrBatchBuffer bytes(TZrByte *data, TZrSize length, TZrMemoryOffset stride) {
    SZrBatchBuffer result;
    result.data = data;
    result.byteLength = length;
    result.stride = stride;
    result.elementSize = 1u;
    return result;
}

static void test_lengths_and_tail(void) {
    TZrByte input[9] = {0,1,2,3,4,5,6,7,8};
    TZrByte output[9] = {0};
    SZrBatchBuffer in = bytes(input, sizeof(input), 1);
    SZrBatchBuffer out = bytes(output, sizeof(output), 1);
    SZrBatchDiagnostic diagnostic;
    TZrSize lengths[] = {0u, 1u, 3u, 4u, 5u, 9u};
    TZrSize i;
    for (i = 0u; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        memset(output, 0, sizeof(output));
        ZrCore_BatchDiagnostic_Clear(&diagnostic);
        assert(ZrLibrary_Batch_Map(&in, &out, lengths[i], add_one, ZR_NULL,
                                   &diagnostic));
        assert(diagnostic.code == ZR_BATCH_DIAGNOSTIC_NONE);
        if (lengths[i] != 0u) assert(output[lengths[i] - 1u] == input[lengths[i] - 1u] + 1u);
        if (lengths[i] < sizeof(output)) assert(output[lengths[i]] == 0u);
    }
}

static void test_callback_failure_preserves_order(void) {
    TZrByte input[4] = {1,2,3,4};
    TZrByte output[4] = {0};
    SZrBatchBuffer in = bytes(input, sizeof(input), 1);
    SZrBatchBuffer out = bytes(output, sizeof(output), 1);
    SZrBatchDiagnostic diagnostic;
    TZrSize calls = 0u;
    assert(!ZrLibrary_Batch_Map(&in, &out, 4u, fail_at_two, &calls, &diagnostic));
    assert(calls == 3u);
    assert(diagnostic.code == ZR_BATCH_DIAGNOSTIC_CALLBACK_ERROR);
    assert(diagnostic.index == 2u);
    assert(output[0] == 2u && output[1] == 3u && output[2] == 0u);
}

static void test_alias_and_negative_stride(void) {
    TZrByte storage[8] = {0,1,2,3,4,5,6,7};
    TZrByte output[4] = {0};
    SZrBatchBuffer overlap = bytes(storage, sizeof(storage), 1);
    SZrBatchBuffer shifted = bytes(storage + 1, sizeof(storage) - 1u, 1);
    SZrBatchBuffer reverse = bytes(storage + 3, 4u, -1);
    SZrBatchBuffer out = bytes(output, sizeof(output), 1);
    SZrBatchDiagnostic diagnostic;
    assert(!ZrLibrary_Batch_Map(&overlap, &shifted, 4u, add_one, ZR_NULL, &diagnostic));
    assert(diagnostic.code == ZR_BATCH_DIAGNOSTIC_ALIAS_UNSAFE);
    assert(ZrLibrary_Batch_Map(&reverse, &out, 4u, add_one, ZR_NULL, &diagnostic));
    assert(output[0] == 4u && output[3] == 1u);
}

static void test_zip_matrix_and_shape_overflow(void) {
    TZrByte left[6] = {1,2,3,4,5,6};
    TZrByte right[6] = {6,5,4,3,2,1};
    TZrByte output[6] = {0};
    SZrBatchBuffer l = bytes(left, sizeof(left), 1);
    SZrBatchBuffer r = bytes(right, sizeof(right), 1);
    SZrBatchBuffer o = bytes(output, sizeof(output), 1);
    SZrBatchDiagnostic diagnostic;
    SZrBatchShape shape = {2u, 3u};
    TZrSize count = 0u;
    assert(ZrCore_BatchShape_ElementCount(shape, &count, &diagnostic) && count == 6u);
    assert(ZrLibrary_Batch_MatrixMap2D(shape, &l, &r, &o, add_bytes, ZR_NULL, &diagnostic));
    assert(output[0] == 7u && output[5] == 7u);
    shape.rows = SIZE_MAX;
    shape.columns = 2u;
    assert(!ZrCore_BatchShape_ElementCount(shape, &count, &diagnostic));
    assert(diagnostic.code == ZR_BATCH_DIAGNOSTIC_OVERFLOW);
}

int main(void) {
    test_lengths_and_tail();
    test_callback_failure_preserves_order();
    test_alias_and_negative_stride();
    test_zip_matrix_and_shape_overflow();
    return 0;
}
