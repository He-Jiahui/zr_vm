#include "zr_vm_parser/exec_ir_frame_roots.h"
#include <assert.h>
#include <string.h>

static TZrBool visit(SZrExecIrFrameRoot *root, TZrPtr slot, TZrPtr base, TZrPtr data) {
    TZrUInt32 *count = (TZrUInt32 *)data;
    assert(root != ZR_NULL); if (root->initialized) assert(slot != ZR_NULL);
    if (root->kind == ZR_EXEC_IR_FRAME_ROOT_DERIVED) assert(base != ZR_NULL);
    ++*count; return ZR_TRUE;
}

int main(void) {
    SZrExecIrPackedValue values[2] = {
        {1u, 1u, ZR_EXEC_IR_PACKED_SLOT_REF, 8u, 8u, 0u, 3u, 0u},
        {2u, 2u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 8u, 8u, 0u, 3u, 0u}
    };
    SZrExecIrPackedFrameRequest request = {1u, values, 2u, 0u, 0u, 8u, 0u};
    SZrExecIrPackedFrameLayout layout; SZrExecIrFrameRootMap map;
    SZrExecIrFrameRootSpec specs[2] = {{1u, ZR_EXEC_IR_FRAME_ROOT_MANAGED, 0u, 0u, 0, ZR_TRUE},
                                       {2u, ZR_EXEC_IR_FRAME_ROOT_DERIVED, 0u, 1u, 4, ZR_TRUE}};
    TZrByte frame[32] = {0}; TZrUInt32 count = 0u; SZrExecIrDiagnostic d;
    ZrParser_ExecIr_PackedFrameLayoutInit(&layout); ZrParser_ExecIr_FrameRootMapInit(&map);
    assert(ZrParser_ExecIr_LayoutPackedFrame(&request, &layout, &d));
    assert(ZrParser_ExecIr_BuildFrameRootMap(&layout, specs, 2u, &map, &d));
    assert(ZrParser_ExecIr_VisitFrameRoots(&map, frame, visit, &count, &d) && count == 2u);
    specs[1].baseValueId = 99u;
    assert(!ZrParser_ExecIr_BuildFrameRootMap(&layout, specs, 2u, &map, &d));
    assert(d.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
    ZrParser_ExecIr_FrameRootMapFree(&map); ZrParser_ExecIr_PackedFrameLayoutFree(&layout); return 0;
}
