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
    count = 0u;
    map.rootCapacity = map.rootCount - 1u;
    assert(!ZrParser_ExecIr_VisitFrameRoots(&map, frame, visit, &count, &d));
    assert(d.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE && count == 0u);
    map.rootCapacity = map.rootCount;
    {
        TZrUInt64 writeback[2] = {11u, 22u};
        TZrUInt32 invalidated[1] = {UINT32_MAX};
        SZrExecIrFrameObservation observation = {
            &layout, frame, ZR_NULL, 0u, writeback, 2u,
            invalidated, 1u, 77u
        };
        assert(layout.frame.storageSlotCount == 2u);
        assert(!ZrParser_ExecIr_ObserveFrame(&observation, &d));
        assert(d.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
        assert(writeback[0] == 11u && writeback[1] == 22u);
        assert(invalidated[0] == UINT32_MAX && observation.invalidatedCount == 77u);
    }
    specs[1].baseValueId = 99u;
    assert(!ZrParser_ExecIr_BuildFrameRootMap(&layout, specs, 2u, &map, &d));
    assert(d.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
    specs[1] = specs[0];
    assert(!ZrParser_ExecIr_BuildFrameRootMap(&layout, specs, 2u, &map, &d));
    assert(d.code == ZR_EXEC_IR_DIAGNOSTIC_DUPLICATE_DEFINITION);
    specs[0].kind = ZR_EXEC_IR_FRAME_ROOT_INLINE_FIELD;
    specs[0].fieldByteOffset = 8u;
    assert(!ZrParser_ExecIr_BuildFrameRootMap(&layout, specs, 1u, &map, &d));
    assert(d.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    specs[0].kind = ZR_EXEC_IR_FRAME_ROOT_MANAGED;
    specs[0].fieldByteOffset = 0u;
    {
        SZrExecIrPackedValue reusedValues[2] = {
            {10u, 1u, ZR_EXEC_IR_PACKED_SLOT_REF, 8u, 8u, 0u, 1u, 0u},
            {20u, 1u, ZR_EXEC_IR_PACKED_SLOT_REF, 8u, 8u, 1u, 2u, 0u}
        };
        SZrExecIrFrameRootSpec reusedSpec =
                {20u, ZR_EXEC_IR_FRAME_ROOT_MANAGED, 0u, 0u, 0, ZR_TRUE};
        request.values = reusedValues;
        request.valueCount = 2u;
        assert(ZrParser_ExecIr_LayoutPackedFrame(&request, &layout, &d));
        assert(layout.logicalToPhysical[0] == layout.logicalToPhysical[1]);
        assert(ZrParser_ExecIr_BuildFrameRootMap(&layout, &reusedSpec, 1u,
                                                &map, &d));
        assert(map.roots[0].physicalSlot == layout.logicalToPhysical[1]);
        {
            TZrUInt64 writeback[2] = {0u, 0u};
            TZrUInt32 invalidated[2] = {UINT32_MAX, UINT32_MAX};
            SZrExecIrFrameObservation observation = {
                &layout, frame, ZR_NULL, 0u, writeback, 2u,
                invalidated, 2u, 0u
            };
            assert(ZrParser_ExecIr_ObserveFrame(&observation, &d));
            assert(observation.invalidatedCount == 1u);
            assert(invalidated[0] == layout.logicalToPhysical[0]);
            assert(invalidated[1] == UINT32_MAX);
        }
    }
    ZrParser_ExecIr_FrameRootMapFree(&map); ZrParser_ExecIr_PackedFrameLayoutFree(&layout); return 0;
}
