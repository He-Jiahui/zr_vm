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

    /* The last-element endpoint overflows when elementSize is added. */
    request.stride = SIZE_MAX;
    request.length = 2u;
    assert(!ZrCore_View_Create(&request, &view, &diagnostic));
    assert(diagnostic.code == ZR_VIEW_DIAGNOSTIC_OVERFLOW);
    assert_view_equal(&view, &originalView);

    /* Two strides exceed SIZE_MAX before the element extent is added. */
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
