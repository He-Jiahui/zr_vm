/* 验收断言在 NDEBUG 的 Release 构建里也必须执行；做法同 tests/core/test_ssa_platform_matrix.c。 */
#ifdef NDEBUG
#undef NDEBUG
#endif
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

/* 分别固定正向部分重叠的拒绝和逆序输入的读取顺序；负步长与重叠叠加的布局见下一用例。 */
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

/* 负步长的元素位于 data 之前，别名区间取逻辑元素最低到最高地址加元素宽度：
 * 既不能漏掉伸向 data 之前的重叠，也不能把 data 之后未触及的字节算成重叠。 */
static void test_negative_stride_alias_range(void) {
    TZrByte storage[8] = {0,1,2,3,4,5,6,7};
    const TZrByte untouched[8] = {0,1,2,3,4,5,6,7};
    const TZrByte mapped[8] = {1,2,3,4,4,3,2,1};
    TZrByte other[4] = {9,9,9,9};
    SZrBatchBuffer forward = bytes(storage, 4u, 1);          /* storage[0..3] */
    SZrBatchBuffer tail = bytes(storage + 4, 4u, 1);         /* storage[4..7] */
    SZrBatchBuffer reverse = bytes(storage + 3, 4u, -1);     /* storage[3..0] */
    SZrBatchBuffer backward = bytes(storage + 4, 4u, -1);    /* storage[4..1] */
    SZrBatchBuffer backwardOut = bytes(storage + 5, 4u, -1); /* storage[5..2] */
    SZrBatchBuffer separate = bytes(other, sizeof(other), 1);
    SZrBatchDiagnostic diagnostic;
    TZrBool alias = ZR_FALSE;

    /* 逆序视图与正向视图共享 storage[1..3]：Map 的两个方向和 Zip 都必须在写入前拒绝。
     * 把逆序区间算成 data 之后的 byteLength 会放行，add_one 随即写出 5,4,3,5。 */
    assert(ZrCore_BatchBuffers_MayAlias(&backward, 4u, &forward, 4u, &alias, &diagnostic) && alias);
    assert(!ZrLibrary_Batch_Map(&backward, &forward, 4u, add_one, ZR_NULL, &diagnostic));
    assert(diagnostic.code == ZR_BATCH_DIAGNOSTIC_ALIAS_UNSAFE);
    assert(!ZrLibrary_Batch_Map(&forward, &backwardOut, 4u, add_one, ZR_NULL, &diagnostic));
    assert(diagnostic.code == ZR_BATCH_DIAGNOSTIC_ALIAS_UNSAFE);
    assert(!ZrLibrary_Batch_Zip(&backward, &separate, &forward, 4u, add_bytes, ZR_NULL,
                                &diagnostic));
    assert(diagnostic.code == ZR_BATCH_DIAGNOSTIC_ALIAS_UNSAFE);
    assert(memcmp(storage, untouched, sizeof(storage)) == 0);

    /* 相邻不相交：逆序视图只占 storage[0..3]，与 storage[4..7] 的输出不别名，可正常写入。 */
    assert(ZrCore_BatchBuffers_MayAlias(&forward, 4u, &tail, 4u, &alias, &diagnostic) && !alias);
    assert(ZrCore_BatchBuffers_MayAlias(&reverse, 4u, &tail, 4u, &alias, &diagnostic) && !alias);
    assert(ZrLibrary_Batch_Map(&reverse, &tail, 4u, add_one, ZR_NULL, &diagnostic));
    /* 完全同布局的逆序视图仍允许原地 Map。 */
    assert(ZrLibrary_Batch_Map(&reverse, &reverse, 4u, add_one, ZR_NULL, &diagnostic));
    assert(memcmp(storage, mapped, sizeof(storage)) == 0);
}

/* 以下两个用例依赖 64 位 ptrdiff_t/size_t；32 位界限是 batch_contract.h 仍标注的另一处缺陷。 */
#if PTRDIFF_MAX == INT64_MAX && SIZE_MAX > UINT32_MAX
/* 末端偏移可以恰为 INT64_MIN。它可表示，所以只能按 byteLength 判边界；幅度 2^63 既不能靠
 * 对最小值取负得到，也不能先把下标 2^63 转成有符号数再相乘。 */
static void test_minimum_offset_boundaries(void) {
    TZrByte storage[8] = {0,1,2,3,4,5,6,7};
    TZrByte output[1] = {0};
    const TZrUInt64 minMagnitude = (TZrUInt64)INT64_MAX + 1u;
    const TZrSize hugeCount = (TZrSize)minMagnitude + 1u;
    SZrBatchBuffer view = bytes(storage + 4, 4u, INT64_MIN);
    SZrBatchBuffer out = bytes(output, sizeof(output), 1);
    SZrBatchDiagnostic diagnostic;
    TZrMemoryOffset offset = 1;

    /* 单个元素不使用 stride，INT64_MIN 步长照常通过并可参与 Map。 */
    assert(ZrCore_BatchBuffer_Validate(&view, 1u, &diagnostic));
    assert(ZrCore_BatchBuffer_Offset(&view, 1u, 0u, &offset, &diagnostic) && offset == 0);
    assert(ZrLibrary_Batch_Map(&view, &out, 1u, add_one, ZR_NULL, &diagnostic));
    assert(output[0] == 5u);
    /* 两个元素的末端偏移是 INT64_MIN：短缓冲报边界；三个元素才是乘法溢出。 */
    assert(!ZrCore_BatchBuffer_Validate(&view, 2u, &diagnostic));
    assert(diagnostic.code == ZR_BATCH_DIAGNOSTIC_BOUNDS);
    assert(diagnostic.expected == 4u && diagnostic.actual == minMagnitude + 1u);
    assert(!ZrCore_BatchBuffer_Validate(&view, 3u, &diagnostic));
    assert(diagnostic.code == ZR_BATCH_DIAGNOSTIC_OVERFLOW);

    /* stride=-2^62、count=3 同样落在 INT64_MIN；足够长的 byteLength 能通过并给出各偏移。 */
    view.stride = -((TZrMemoryOffset)1 << 62);
    assert(!ZrCore_BatchBuffer_Validate(&view, 3u, &diagnostic));
    assert(diagnostic.code == ZR_BATCH_DIAGNOSTIC_BOUNDS && diagnostic.actual == minMagnitude + 1u);
    assert(!ZrCore_BatchBuffer_Validate(&view, 4u, &diagnostic));
    assert(diagnostic.code == ZR_BATCH_DIAGNOSTIC_OVERFLOW);
    view.byteLength = SIZE_MAX;
    assert(ZrCore_BatchBuffer_Validate(&view, 3u, &diagnostic));
    assert(ZrCore_BatchBuffer_Offset(&view, 3u, 1u, &offset, &diagnostic) && offset == view.stride);
    assert(ZrCore_BatchBuffer_Offset(&view, 3u, 2u, &offset, &diagnostic) && offset == INT64_MIN);

    /* stride=-1、count=2^63+1：末元素下标 2^63 放不进有符号数，偏移 INT64_MIN 仍可表示。 */
    view.stride = -1;
    view.byteLength = 4u;
    assert(!ZrCore_BatchBuffer_Validate(&view, hugeCount, &diagnostic));
    assert(diagnostic.code == ZR_BATCH_DIAGNOSTIC_BOUNDS && diagnostic.actual == minMagnitude + 1u);
    assert(!ZrCore_BatchBuffer_Validate(&view, hugeCount + 1u, &diagnostic));
    assert(diagnostic.code == ZR_BATCH_DIAGNOSTIC_OVERFLOW);
    view.byteLength = SIZE_MAX;
    assert(ZrCore_BatchBuffer_Validate(&view, hugeCount, &diagnostic));
    assert(ZrCore_BatchBuffer_Offset(&view, hugeCount, hugeCount - 2u, &offset, &diagnostic) &&
           offset == -INT64_MAX);
    assert(ZrCore_BatchBuffer_Offset(&view, hugeCount, hugeCount - 1u, &offset, &diagnostic) &&
           offset == INT64_MIN);
}

/* MayAlias 只做地址运算、不解引用，这里用伪造地址固定区间端点：末端偏移为 INT64_MIN 的
 * 逆序视图向下伸 2^63 字节；伸过地址零或越过地址上限时报告溢出而不是回绕。 */
static void test_alias_address_arithmetic(void) {
    const TZrUInt64 minMagnitude = (TZrUInt64)INT64_MAX + 1u;
    const TZrSize hugeCount = (TZrSize)minMagnitude + 1u;
    SZrBatchBuffer wide = bytes((TZrByte *)(uintptr_t)(minMagnitude + 16u), SIZE_MAX, -1);
    SZrBatchBuffer below = bytes((TZrByte *)(uintptr_t)8u, 8u, 1);
    SZrBatchBuffer lowest = bytes((TZrByte *)(uintptr_t)16u, 8u, 1);
    SZrBatchBuffer top = bytes((TZrByte *)(uintptr_t)(UINTPTR_MAX - 1u), 4u, 1);
    SZrBatchDiagnostic diagnostic;
    TZrBool alias = ZR_TRUE;

    /* wide 占 [16, 2^63+17)：[8,16) 紧贴其下而不别名；从 16 起的视图与它的最低元素重叠。 */
    assert(ZrCore_BatchBuffers_MayAlias(&wide, hugeCount, &below, 8u, &alias, &diagnostic) && !alias);
    assert(ZrCore_BatchBuffers_MayAlias(&wide, hugeCount, &lowest, 8u, &alias, &diagnostic) && alias);
    /* 同样的跨度从地址 16 出发会伸到零以下。 */
    wide.data = (TZrByte *)(uintptr_t)16u;
    assert(!ZrCore_BatchBuffers_MayAlias(&wide, hugeCount, &below, 8u, &alias, &diagnostic));
    assert(diagnostic.code == ZR_BATCH_DIAGNOSTIC_OVERFLOW && diagnostic.actual == minMagnitude);
    /* 正向视图的末元素越过地址上限。 */
    assert(!ZrCore_BatchBuffers_MayAlias(&top, 4u, &below, 8u, &alias, &diagnostic));
    assert(diagnostic.code == ZR_BATCH_DIAGNOSTIC_OVERFLOW);
}
#endif

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

/* 文件开头取消了 NDEBUG，Release 构建同样执行各用例 assert 内的 Map、Zip 与验证调用。 */
int main(void) {
    test_lengths_and_tail();
    test_callback_failure_preserves_order();
    test_alias_and_negative_stride();
    test_negative_stride_alias_range();
#if PTRDIFF_MAX == INT64_MAX && SIZE_MAX > UINT32_MAX
    test_minimum_offset_boundaries();
    test_alias_address_arithmetic();
#endif
    test_zip_matrix_and_shape_overflow();
    return 0;
}
