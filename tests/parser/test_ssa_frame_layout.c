#include "zr_vm_parser/exec_ir_frame_layout.h"

#include <assert.h>
#include <limits.h>

int main(void) {
    SZrExecIrPackedValue values[3] = {
        {1u, 11u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 8u, 8u, 0u, 2u, 0u},
        {2u, 12u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 8u, 8u, 2u, 4u, 0u},
        {3u, 13u, ZR_EXEC_IR_PACKED_SLOT_REF, 8u, 8u, 0u, 4u,
         ZR_EXEC_IR_PACKED_SLOT_ADDRESS_ESCAPED}
    };
    SZrExecIrPackedFrameRequest request = {7u, values, 3u, 1u, 4u, 4u, 0u};
    SZrExecIrPackedFrameLayout layout;
    SZrExecIrDiagnostic diagnostic;

    ZrParser_ExecIr_PackedFrameLayoutInit(&layout);
    assert(ZrParser_ExecIr_LayoutPackedFrame(&request, &layout, &diagnostic));
    assert(layout.frame.logicalSlotCount == 3u);
    assert(layout.frame.storageSlotCount == 2u);
    assert(layout.logicalToPhysical[0] == layout.logicalToPhysical[1]);
    assert(layout.logicalToPhysical[2] != layout.logicalToPhysical[0]);
    assert(layout.frame.layoutHash != 0u);

    request.returnBufferSize = 3u;
    assert(ZrParser_ExecIr_LayoutPackedFrame(&request, &layout, &diagnostic));
    assert(layout.frame.frameByteSize % layout.frame.frameByteAlign == 0u);
    request.returnBufferSize = 4u;

    /* A and B do not overlap, while C overlaps B.  C must not be put back
     * into A's slot merely because A was the slot's first occupant. */
    {
        SZrExecIrPackedValue chain[3] = {
            {11u, 1u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 4u, 4u, 0u, 2u, 0u},
            {12u, 1u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 4u, 4u, 2u, 5u, 0u},
            {13u, 1u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 4u, 4u, 3u, 6u, 0u}
        };
        SZrExecIrPackedFrameRequest chainRequest =
                {8u, chain, 3u, 0u, 0u, 4u, 0u};
        assert(ZrParser_ExecIr_LayoutPackedFrame(&chainRequest, &layout,
                                                  &diagnostic));
        assert(layout.frame.storageSlotCount == 2u);
        assert(layout.logicalToPhysical[0] == layout.logicalToPhysical[1]);
        assert(layout.logicalToPhysical[2] != layout.logicalToPhysical[1]);
    }

    values[0].liveEnd = UINT32_MAX;
    values[1].liveStart = 0u;
    assert(ZrParser_ExecIr_LayoutPackedFrame(&request, &layout, &diagnostic));
    values[0].byteAlign = 3u;
    assert(!ZrParser_ExecIr_LayoutPackedFrame(&request, &layout, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    values[0].byteAlign = 8u;
    request.frameByteLimit = 1u;
    assert(!ZrParser_ExecIr_LayoutPackedFrame(&request, &layout, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
    ZrParser_ExecIr_PackedFrameLayoutFree(&layout);
    return 0;
}
