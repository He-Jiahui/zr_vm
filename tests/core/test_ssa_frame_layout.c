#include "zr_vm_core/execution_frame_layout.h"

#include <assert.h>
#include <string.h>

static SZrExecutionFrameLayout make_layout(
        SZrExecutionFrameSlot *slots,
        TZrUInt32 slotCount,
        TZrUInt32 storageSlotCount,
        TZrUInt32 frameByteSize) {
    SZrExecutionFrameLayout layout;
    ZrCore_ExecutionFrameLayout_Init(&layout);
    layout.logicalSlotCount = slotCount;
    layout.storageSlotCount = storageSlotCount;
    layout.parameterPrefixCount = 0u;
    layout.returnBufferOffset = frameByteSize;
    layout.frameByteSize = frameByteSize;
    layout.frameByteAlign = 8u;
    layout.slots = slots;
    layout.slotCount = slotCount;
    return layout;
}

static void test_packed_layout_finalizes_and_finds_slots(void) {
    SZrExecutionFrameSlot slots[3] = {
        {10u, 0u, 0u, 8u, 8u, 0u, 4u, 1u,
         ZR_EXECUTION_FRAME_SLOT_SCALAR, 0u},
        {11u, 1u, 8u, 8u, 8u, 0u, 4u, 2u,
         ZR_EXECUTION_FRAME_SLOT_REFERENCE,
         ZR_EXECUTION_FRAME_SLOT_FLAG_INITIALIZED},
        {12u, 0u, 0u, 8u, 8u, 4u, 8u, 1u,
         ZR_EXECUTION_FRAME_SLOT_SCALAR, 0u}
    };
    SZrExecutionFrameLayout layout = make_layout(slots, 3u, 2u, 32u);
    SZrExecutionFrameDiagnostic diagnostic;

    assert(ZrCore_ExecutionFrameLayout_Finalize(&layout, &diagnostic));
    assert(layout.layoutHash != 0u);
    assert(ZrCore_ExecutionFrameLayout_FindLogical(&layout, 11u) == &slots[1]);
    assert(ZrCore_ExecutionFrameLayout_FindLogical(&layout, 99u) == NULL);
}

static void test_layout_rejects_overlapping_reuse_and_escape(void) {
    SZrExecutionFrameSlot slots[2] = {
        {1u, 0u, 0u, 8u, 8u, 0u, 5u, 1u,
         ZR_EXECUTION_FRAME_SLOT_SCALAR, 0u},
        {2u, 0u, 0u, 8u, 8u, 4u, 8u, 1u,
         ZR_EXECUTION_FRAME_SLOT_SCALAR, 0u}
    };
    SZrExecutionFrameLayout layout = make_layout(slots, 2u, 1u, 16u);
    SZrExecutionFrameDiagnostic diagnostic;

    assert(!ZrCore_ExecutionFrameLayout_Validate(&layout, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_SLOT_OVERLAP);

    slots[1].liveStart = 5u;
    slots[1].liveEnd = 8u;
    slots[1].flags = ZR_EXECUTION_FRAME_SLOT_FLAG_ADDRESS_ESCAPED;
    assert(!ZrCore_ExecutionFrameLayout_Validate(&layout, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_SLOT);
}

static void test_layout_rejects_bad_alignment_and_offset_overflow(void) {
    SZrExecutionFrameSlot slot =
        {1u, 0u, 0u, 8u, 3u, 0u, 1u, 1u,
         ZR_EXECUTION_FRAME_SLOT_SCALAR, 0u};
    SZrExecutionFrameLayout layout = make_layout(&slot, 1u, 1u, 16u);
    SZrExecutionFrameDiagnostic diagnostic;

    assert(!ZrCore_ExecutionFrameLayout_Validate(&layout, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_ALIGNMENT);

    slot.byteAlign = 8u;
    slot.byteOffset = UINT32_MAX - 7u;
    assert(!ZrCore_ExecutionFrameLayout_Validate(&layout, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_SLOT);
}

static void test_layout_rejects_unaligned_frame_size(void) {
    SZrExecutionFrameLayout layout = make_layout(NULL, 0u, 0u, 18u);
    SZrExecutionFrameDiagnostic diagnostic;

    assert(!ZrCore_ExecutionFrameLayout_Validate(&layout, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_ALIGNMENT);
    assert(diagnostic.expected == layout.frameByteAlign);
    assert(diagnostic.actual == layout.frameByteSize);
}

static void test_layout_rejects_excess_storage_slots(void) {
    SZrExecutionFrameLayout layout = make_layout(NULL, 0u, 1u, 8u);
    SZrExecutionFrameDiagnostic diagnostic;

    assert(!ZrCore_ExecutionFrameLayout_Validate(&layout, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_SLOT);
    assert(diagnostic.expected == layout.logicalSlotCount);
    assert(diagnostic.actual == layout.storageSlotCount);
}

static void test_layout_hash_is_independent_of_producer_order(void) {
    SZrExecutionFrameSlot first[2] = {
        {7u, 0u, 0u, 8u, 8u, 0u, 1u, 1u,
         ZR_EXECUTION_FRAME_SLOT_SCALAR, 0u},
        {3u, 1u, 8u, 8u, 8u, 0u, 1u, 2u,
         ZR_EXECUTION_FRAME_SLOT_REFERENCE, 0u}
    };
    SZrExecutionFrameSlot second[2] = {first[1], first[0]};
    SZrExecutionFrameLayout left = make_layout(first, 2u, 2u, 24u);
    SZrExecutionFrameLayout right = make_layout(second, 2u, 2u, 24u);

    assert(ZrCore_ExecutionFrameLayout_Finalize(&left, NULL));
    assert(ZrCore_ExecutionFrameLayout_Finalize(&right, NULL));
    assert(left.layoutHash == right.layoutHash);
}

int main(void) {
    test_packed_layout_finalizes_and_finds_slots();
    test_layout_rejects_overlapping_reuse_and_escape();
    test_layout_rejects_bad_alignment_and_offset_overflow();
    test_layout_rejects_unaligned_frame_size();
    test_layout_rejects_excess_storage_slots();
    test_layout_hash_is_independent_of_producer_order();
    return 0;
}
