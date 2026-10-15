#include "zr_vm_parser/exec_ir_array_lowering.h"
#include <assert.h>
#include <stdint.h>

static void assert_view_equal(
        const SZrContiguousView *actual,
        const SZrContiguousView *expected) {
    assert(actual->ownerRoot == expected->ownerRoot);
    assert(actual->byteOffset == expected->byteOffset);
    assert(actual->length == expected->length);
    assert(actual->stride == expected->stride);
    assert(actual->elementSize == expected->elementSize);
    assert(actual->elementLayoutHash == expected->elementLayoutHash);
    assert(actual->storageGeneration == expected->storageGeneration);
    assert(actual->lifetimeRegion == expected->lifetimeRegion);
    assert(actual->flags == expected->flags);
}

static void test_empty_tail_slice_remains_valid(void) {
    SZrContiguousViewRequest request = {
            (TZrPtr)(uintptr_t)1u, SIZE_MAX - 4u, 1u, 4u, 4u,
            0x55u, 3u, 7u, 0u};
    SZrContiguousView view, slice;
    SZrViewDiagnostic diagnostic;

    assert(ZrCore_View_Create(&request, &view, &diagnostic));
    assert(ZrCore_View_Slice(&view, 1u, 0u, &slice, &diagnostic));
    assert(slice.length == 0u && slice.byteOffset == SIZE_MAX);
    assert(ZrCore_View_Validate(&slice, 3u, &diagnostic));
}

static void test_tail_slice_arithmetic_overflow_is_reported(void) {
    SZrContiguousViewRequest request = {
            (TZrPtr)(uintptr_t)1u, 0u, 2u, SIZE_MAX - 1u, 1u,
            0x55u, 3u, 7u, 0u};
    SZrContiguousView view, slice;
    SZrViewDiagnostic diagnostic;

    assert(ZrCore_View_Create(&request, &view, &diagnostic));
    slice = view;
    assert(!ZrCore_View_Slice(&view, 2u, 0u, &slice, &diagnostic));
    assert(diagnostic.code == ZR_VIEW_DIAGNOSTIC_OVERFLOW);
    assert(slice.byteOffset == view.byteOffset && slice.length == view.length);
}

static void test_tail_slice_offset_add_overflow_is_reported(void) {
    SZrContiguousViewRequest request = {
            (TZrPtr)(uintptr_t)1u, SIZE_MAX - 1u, 1u, 2u, 1u,
            0x55u, 3u, 7u, 0u};
    SZrContiguousView view, slice;
    SZrViewDiagnostic diagnostic;

    assert(ZrCore_View_Create(&request, &view, &diagnostic));
    slice = view;
    assert(!ZrCore_View_Slice(&view, 1u, 0u, &slice, &diagnostic));
    assert(diagnostic.code == ZR_VIEW_DIAGNOSTIC_OVERFLOW);
    assert(slice.byteOffset == view.byteOffset && slice.length == view.length);
}

static void test_index_past_end_reports_bounds(void) {
    SZrContiguousViewRequest request = {
            (TZrPtr)(uintptr_t)1u, 8u, 4u, 4u, 4u,
            0x55u, 3u, 7u, 0u};
    SZrContiguousView view;
    SZrViewDiagnostic diagnostic;
    TZrSize offset = 19u;

    assert(ZrCore_View_Create(&request, &view, &diagnostic));
    assert(!ZrParser_ExecIr_LowerArrayIndex(&view, 4, &offset, &diagnostic));
    assert(diagnostic.code == ZR_VIEW_DIAGNOSTIC_BOUNDS);
    assert(offset == 19u);
}

static void test_index_requires_output_pointer(void) {
    SZrContiguousViewRequest request = {
            (TZrPtr)(uintptr_t)1u, 8u, 4u, 4u, 4u,
            0x55u, 3u, 7u, 0u};
    SZrContiguousView view;
    SZrViewDiagnostic diagnostic;

    assert(ZrCore_View_Create(&request, &view, &diagnostic));
    assert(!ZrParser_ExecIr_LowerArrayIndex(&view, 0, ZR_NULL, &diagnostic));
    assert(diagnostic.code == ZR_VIEW_DIAGNOSTIC_INVALID);
}

static void test_slice_requires_output_pointer(void) {
    SZrContiguousViewRequest request = {
            (TZrPtr)(uintptr_t)1u, 8u, 4u, 4u, 4u,
            0x55u, 3u, 7u, 0u};
    SZrContiguousView view;
    SZrViewDiagnostic diagnostic;

    assert(ZrCore_View_Create(&request, &view, &diagnostic));
    assert(!ZrCore_View_Slice(&view, 0u, 1u, ZR_NULL, &diagnostic));
    assert(diagnostic.code == ZR_VIEW_DIAGNOSTIC_INVALID);
}

static void test_basic_view_boundaries(void) {
    SZrContiguousViewRequest request = {
            (TZrPtr)(uintptr_t)1u, 8u, 4u, 4u, 4u,
            0x55u, 3u, 7u, 0u};
    SZrContiguousView view, slice;
    SZrContiguousView originalView;
    SZrViewDiagnostic diagnostic;
    TZrSize offset;

    assert(ZrCore_View_Create(&request, &view, &diagnostic));
    assert(ZrParser_ExecIr_LowerArrayIndex(&view, 2, &offset, &diagnostic));
    assert(offset == 16u);
    assert(!ZrParser_ExecIr_LowerArrayIndex(&view, -1, &offset, &diagnostic));
    assert(diagnostic.code == ZR_VIEW_DIAGNOSTIC_BOUNDS);
    assert(ZrCore_View_Slice(&view, 1u, 2u, &slice, &diagnostic));
    assert(slice.byteOffset == 12u && slice.length == 2u);
    assert(!ZrCore_View_Slice(&view, 4u, 1u, &slice, &diagnostic));
    assert(diagnostic.code == ZR_VIEW_DIAGNOSTIC_BOUNDS);
    assert(slice.byteOffset == 12u && slice.length == 2u);
    assert(ZrCore_View_Slice(&view, 4u, 0u, &slice, &diagnostic));
    assert(slice.byteOffset == 24u && slice.length == 0u);
    assert(ZrCore_View_Validate(&slice, 3u, &diagnostic));
    assert(!ZrParser_ExecIr_LowerArrayIndex(&slice, 0, &offset, &diagnostic));
    assert(diagnostic.code == ZR_VIEW_DIAGNOSTIC_BOUNDS);
    originalView = view;

    /* 父 byteOffset 与末元素跨度相加溢出，不能发布新描述符。 */
    request.stride = SIZE_MAX;
    request.length = 2u;
    assert(!ZrCore_View_Create(&request, &view, &diagnostic));
    assert(diagnostic.code == ZR_VIEW_DIAGNOSTIC_OVERFLOW);
    assert_view_equal(&view, &originalView);

    /* 两倍 stride 的乘积已超过 SIZE_MAX，元素末端检查尚未进行。 */
    request.byteOffset = 0u;
    request.length = 3u;
    request.stride = SIZE_MAX / 2u + 1u;
    request.elementSize = 1u;
    assert(!ZrCore_View_Create(&request, &view, &diagnostic));
    assert(diagnostic.code == ZR_VIEW_DIAGNOSTIC_OVERFLOW);
    assert_view_equal(&view, &originalView);

    request.stride = 0u;
    assert(!ZrCore_View_Create(&request, &view, &diagnostic));
    assert(diagnostic.code == ZR_VIEW_DIAGNOSTIC_INVALID);
    assert_view_equal(&view, &originalView);
}

int main(void) {
    test_basic_view_boundaries();
    test_slice_requires_output_pointer();
    test_index_requires_output_pointer();
    test_index_past_end_reports_bounds();
    test_tail_slice_offset_add_overflow_is_reported();
    test_tail_slice_arithmetic_overflow_is_reported();
    test_empty_tail_slice_remains_valid();
    return 0;
}

/**
 * @fn static void test_empty_tail_slice_remains_valid(void)
 * ## Summary
 * 确认有效父视图在最后一个元素之后可建立零长度尾切片，并且切片偏移达到
 * SIZE_MAX 时仍能通过描述符校验。
 *
 * ## Constraints and lifecycle
 * ownerRoot 使用非空整数标记值，当前 Create/Slice/Validate 只检查视图元数据，
 * 不解引用它；本例不创建真实 backing storage，也不证明 owner 保活、GC root、pin
 * 或借用生命周期。验证依赖断言启用的 Debug 构建；NDEBUG 会移除 assert 中的 API 调用。
 *
 * ## Returns
 * 无返回值；断言启用且均通过时正常返回，任一 assert 失败都会终止测试进程。本函数不返回资源或错误码。
 *
 * ## References
 * ZrCore_View_Create、ZrCore_View_Slice、ZrCore_View_Validate；计划
 * docs/plans/ssa/05-data-layout/02-arrays-slices.md。
 */
/**
 * @fn static void test_tail_slice_arithmetic_overflow_is_reported(void)
 * ## Summary
 * 先创建末端恰可表示的父视图，再从 start == length 请求空切片，使 start * stride
 * 的乘法溢出；断言 Core 报 OVERFLOW，并检查失败后 slice 的 byteOffset 和 length
 * 仍等于父视图对应字段。
 *
 * ## Constraints and lifecycle
 * 失败保留只检查上述两个字段，不代表测试逐字段比较完整描述符。ownerRoot 是不被
 * 解引用的非空标记值，测试不覆盖真实内存、owner 生命周期或 pin。断言启用的 Debug
 * 构建是有效执行条件；NDEBUG 会移除 assert 中的 API 调用。
 *
 * ## Returns
 * 无返回值；断言启用且均通过时正常返回，任一 assert 失败都会终止测试进程。本函数不返回资源或错误码。
 *
 * ## References
 * ZrCore_View_Slice；tests/acceptance/ssa-arrays-slices-view-boundaries.md。
 */
/**
 * @fn static void test_tail_slice_offset_add_overflow_is_reported(void)
 * ## Summary
 * 使用末端可表示的单元素父视图，在尾部空切片偏移计算中触发 byteOffset + delta
 * 溢出；断言 Core 报 OVERFLOW，并检查失败后 slice 的 byteOffset 和 length 未改变。
 *
 * ## Constraints and lifecycle
 * 失败后只比较 byteOffset 与 length，不证明其余 descriptor 字段未改。ownerRoot 是
 * 非空标记值且不被这些 metadata helper 解引用；没有真实存储或 pin 生命周期。测试
 * 依赖启用 assert 的 Debug 构建，NDEBUG 会移除 assert 中的 API 调用。
 *
 * ## Returns
 * 无返回值；断言启用且均通过时正常返回，任一 assert 失败都会终止测试进程。本函数不返回资源或错误码。
 *
 * ## References
 * ZrCore_View_Slice；tests/acceptance/ssa-arrays-slices-view-boundaries.md。
 */
/**
 * @fn static void test_index_past_end_reports_bounds(void)
 * ## Summary
 * 经 Parser facade 传入 index == view->length，确认 Core 的边界诊断被转发为
 * ZR_VIEW_DIAGNOSTIC_BOUNDS，并确认 sentinel offset 19 保持不变。
 *
 * ## Constraints and lifecycle
 * facade 不解引用本例中的非空 ownerRoot 标记；这只证明 metadata 路径，不证明真实
 * 数组访问。断言启用的 Debug 构建是有效执行条件；NDEBUG 会移除 assert 中的 API 调用。
 *
 * ## Returns
 * 无返回值；断言启用且均通过时正常返回，任一 assert 失败都会终止测试进程。本函数不返回资源或错误码。
 *
 * ## References
 * ZrParser_ExecIr_LowerArrayIndex；ZrCore_View_IndexOffset；本文件
 * test_basic_view_boundaries。
 */
/**
 * @fn static void test_index_requires_output_pointer(void)
 * ## Summary
 * 经 Parser facade 传入空 offset 输出指针，确认无有效结果槽时返回失败并由 Core
 * 给出 ZR_VIEW_DIAGNOSTIC_INVALID。
 *
 * ## Constraints and lifecycle
 * 本例的 ownerRoot 只是非空标记值，未涉及真实存储或元素取址；也未测试空 diagnostic
 * 指针。断言启用的 Debug 构建是有效执行条件；NDEBUG 会移除 assert 中的 API 调用。
 *
 * ## Returns
 * 无返回值；断言启用且均通过时正常返回，任一 assert 失败都会终止测试进程。本函数不返回资源或错误码。
 *
 * ## References
 * ZrParser_ExecIr_LowerArrayIndex；ZrCore_View_IndexOffset。
 */
/**
 * @fn static void test_basic_view_boundaries(void)
 * ## Summary
 * 覆盖 facade 的有效索引（2 映射到偏移 16）、负索引及空视图索引的 BOUNDS 诊断；
 * 同时覆盖 Core 的普通/越界/空尾切片、Create 的末元素偏移加法与 stride 乘法溢出，以及
 * 零 stride 的 INVALID 诊断。Create 失败时用 assert_view_equal 比较完整旧视图。
 *
 * ## Constraints and lifecycle
 * wrapper 覆盖仅是连续视图元数据计算，不运行生产 ExecIR/AOT 发射或真实数组读写。
 * request.ownerRoot 是不被解引用的非空整数标记，不提供 owner rooting、pin 或借用
 * 生命周期证据。断言启用的 Debug 构建是有效执行条件；NDEBUG 会移除 assert 中的
 * API 调用。
 *
 * ## Returns
 * 无返回值；断言启用且均通过时正常返回，任一 assert 失败都会终止测试进程。本函数不返回资源或错误码。
 *
 * ## References
 * ZrParser_ExecIr_LowerArrayIndex；ZrCore_View_Create、ZrCore_View_Slice、
 * ZrCore_View_Validate；helper assert_view_equal。
 */
