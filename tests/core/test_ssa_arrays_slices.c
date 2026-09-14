#include "zr_vm_parser/exec_ir_array_lowering.h"
#include <assert.h>
#include <stdint.h>

int main(void) {
    SZrContiguousViewRequest request = {(TZrPtr)(uintptr_t)1u, 8u, 4u, 4u, 4u, 0x55u, 3u, 7u, 0u};
    SZrContiguousView view, slice; SZrViewDiagnostic diagnostic; TZrSize offset;
    assert(ZrCore_View_Create(&request, &view, &diagnostic));
    assert(ZrParser_ExecIr_LowerArrayIndex(&view, 2, &offset, &diagnostic) && offset == 16u);
    assert(!ZrParser_ExecIr_LowerArrayIndex(&view, -1, &offset, &diagnostic));
    assert(diagnostic.code == ZR_VIEW_DIAGNOSTIC_BOUNDS);
    assert(ZrCore_View_Slice(&view, 1u, 2u, &slice, &diagnostic) && slice.byteOffset == 12u && slice.length == 2u);
    assert(!ZrCore_View_Slice(&view, 4u, 1u, &slice, &diagnostic));
    request.stride = SIZE_MAX; request.length = 2u;
    assert(!ZrCore_View_Create(&request, &view, &diagnostic));
    assert(diagnostic.code == ZR_VIEW_DIAGNOSTIC_INVALID);
    return 0;
}
