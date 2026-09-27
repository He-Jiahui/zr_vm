#include "zr_vm_core/batch_contract.h"
#include "zr_vm_library/batch_protocol.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

/* Map 正常路径的确定性回调，供尾部、别名和逆序布局共用。 */
static TZrBool add_one(const TZrByte *input, TZrByte *output,
                       TZrSize size, TZrPtr userData) {
    TZrSize i;
    (void)userData;
    for (i = 0u; i < size; ++i) output[i] = (TZrByte)(input[i] + 1u);
    return ZR_TRUE;
}

/* 二输入回调让矩阵测试观察 Zip 和形状映射的组合结果。 */
static TZrBool add_bytes(const TZrByte *left, const TZrByte *right,
                         TZrByte *output, TZrSize size, TZrPtr userData) {
    TZrSize i;
    (void)userData;
    for (i = 0u; i < size; ++i) output[i] = (TZrByte)(left[i] + right[i]);
    return ZR_TRUE;
}

/* 第三次调用主动失败，用来固定诊断索引与已写前缀的语义。 */
static TZrBool fail_at_two(const TZrByte *input, TZrByte *output,
                           TZrSize size, TZrPtr userData) {
    TZrSize *calls = (TZrSize *)userData;
    (void)size;
    (*calls)++;
    if (*calls == 3u) return ZR_FALSE;
    *output = (TZrByte)(*input + 1u);
    return ZR_TRUE;
}

/* 描述借用的单字节槽位；调用方负责保证底层栈数组仍有效。 */
static SZrBatchBuffer bytes(TZrByte *data, TZrSize length, TZrMemoryOffset stride) {
    SZrBatchBuffer result;
    result.data = data;
    result.byteLength = length;
    result.stride = stride;
    result.elementSize = 1u;
    return result;
}

/* 各长度下只允许写入指定元素，尾部哨兵保持不变。 */
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

/* 回调失败应停在第三个元素，报告索引 2 并保留已写前缀。 */
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

/* TODO: 当前仅分别测正向重叠与负步长；需补两者叠加的布局。
 * batch_contract.c 的 MayAlias 用 data 起点向后推范围，负步长可漏报重叠。 */
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

/* 二维形状须先排除元素数溢出，再由 Zip 写入矩阵结果。 */
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

/* BUG: NDEBUG 删除各用例 assert 中的 Map、Zip 与验证调用，CTest 可空跑成功。 */
int main(void) {
    test_lengths_and_tail();
    test_callback_failure_preserves_order();
    test_alias_and_negative_stride();
    test_zip_matrix_and_shape_overflow();
    return 0;
}
