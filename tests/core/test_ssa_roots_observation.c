#include "zr_vm_core/execution_frame_layout.h"

#include <assert.h>
#include <string.h>

typedef struct SVisitState {
    TZrPtr oldBase;
    TZrPtr movedBase;
    TZrUInt32 managedCount;
    TZrUInt32 derivedCount;
    TZrUInt32 inlineCount;
} SVisitState;

static SZrExecutionFrameLayout make_layout(SZrExecutionFrameSlot *slots) {
    SZrExecutionFrameLayout layout;
    ZrCore_ExecutionFrameLayout_Init(&layout);
    layout.logicalSlotCount = 3u;
    layout.storageSlotCount = 3u;
    layout.returnBufferOffset = 40u;
    layout.frameByteSize = 40u;
    layout.frameByteAlign = 8u;
    layout.slots = slots;
    layout.slotCount = 3u;
    return layout;
}

static TZrBool visit_root(SZrExecutionFrameRoot *root,
                          TZrPtr slotAddress,
                          TZrPtr baseAddress,
                          TZrPtr userData) {
    SVisitState *state = (SVisitState *)userData;
    assert(root != ZR_NULL);
    assert(slotAddress != ZR_NULL);
    assert(state != ZR_NULL);
    if (root->kind == ZR_EXECUTION_FRAME_ROOT_MANAGED) {
        TZrPtr replacement = state->movedBase;
        TZrPtr observed = ZR_NULL;
        memcpy(&observed, slotAddress, sizeof(observed));
        assert(observed == state->oldBase);
        memcpy(slotAddress, &replacement, sizeof(replacement));
        ++state->managedCount;
    } else if (root->kind == ZR_EXECUTION_FRAME_ROOT_DERIVED) {
        TZrPtr base = ZR_NULL;
        assert(baseAddress != ZR_NULL);
        memcpy(&base, baseAddress, sizeof(base));
        assert(base == state->movedBase);
        ++state->derivedCount;
    } else {
        ++state->inlineCount;
    }
    return ZR_TRUE;
}

static void test_precise_roots_and_derived_relocation(void) {
    SZrExecutionFrameSlot slots[3] = {
        {10u, 0u, 0u, (TZrUInt32)sizeof(TZrPtr),
         (TZrUInt32)sizeof(TZrPtr), 0u, 3u, 1u,
         ZR_EXECUTION_FRAME_SLOT_REFERENCE,
         ZR_EXECUTION_FRAME_SLOT_FLAG_INITIALIZED},
        {11u, 1u, 8u, (TZrUInt32)sizeof(TZrPtr),
         (TZrUInt32)sizeof(TZrPtr), 0u, 3u, 1u,
         ZR_EXECUTION_FRAME_SLOT_REFERENCE,
         ZR_EXECUTION_FRAME_SLOT_FLAG_INITIALIZED},
        {12u, 2u, 16u, 16u, 8u, 0u, 3u, 2u,
         ZR_EXECUTION_FRAME_SLOT_INLINE_SPAN,
         ZR_EXECUTION_FRAME_SLOT_FLAG_INITIALIZED}
    };
    SZrExecutionFrameLayout layout = make_layout(slots);
    SZrExecutionFrameRootSpec specs[3] = {
        {11u, ZR_EXECUTION_FRAME_ROOT_DERIVED,
         ZR_EXECUTION_FRAME_ROOT_STORAGE_POINTER, 0u, 10u, 4, ZR_TRUE},
        {10u, ZR_EXECUTION_FRAME_ROOT_MANAGED,
         ZR_EXECUTION_FRAME_ROOT_STORAGE_POINTER, 0u, 0u, 0, ZR_TRUE},
        {12u, ZR_EXECUTION_FRAME_ROOT_INLINE_FIELD,
         ZR_EXECUTION_FRAME_ROOT_STORAGE_VALUE_BYTES, 0u, 0u, 0, ZR_TRUE}
    };
    SZrExecutionFrameRootMap map;
    SZrFrameRootVisitor visitor;
    SZrExecutionFrameDiagnostic diagnostic;
    SVisitState state;
    TZrByte frame[40] = {0};
    TZrByte oldObject = 0u;
    TZrByte movedObject = 0u;
    TZrPtr oldBase = (TZrPtr)&oldObject;
    TZrPtr movedBase = (TZrPtr)&movedObject;
    TZrPtr derived = (TZrPtr)(frame + 8u);

    assert(ZrCore_ExecutionFrameLayout_Finalize(&layout, &diagnostic));
    ZrCore_ExecutionFrameRootMap_Init(&map);
    assert(ZrCore_ExecutionFrameRootMap_Build(
            &layout, specs, 3u, &map, &diagnostic));
    assert(ZrCore_ExecutionFrameRootMap_Validate(
            &layout, &map, &diagnostic));
    memcpy(frame, &oldBase, sizeof(oldBase));
    memcpy(frame + 8u, &oldBase, sizeof(oldBase));
    memcpy(frame + 16u, &derived, sizeof(derived));

    memset(&state, 0, sizeof(state));
    state.oldBase = oldBase;
    state.movedBase = movedBase;
    visitor.rootMap = &map;
    visitor.frameBase = frame;
    visitor.frameByteSize = (TZrUInt32)sizeof(frame);
    visitor.visit = visit_root;
    visitor.userData = (TZrPtr)&state;
    assert(ZrCore_Execution_VisitFrameRoots(
            ZR_NULL, &visitor, &diagnostic));
    assert(state.managedCount == 1u);
    assert(state.derivedCount == 1u);
    assert(state.inlineCount == 1u);
    memcpy(&oldBase, frame, sizeof(oldBase));
    assert(oldBase == movedBase);
    memcpy(&derived, frame + 8u, sizeof(derived));
    assert(derived == (TZrPtr)((TZrByte *)movedBase + 4u));
    ZrCore_ExecutionFrameRootMap_Free(&map);
}

static void test_observation_materializes_and_invalidates_atomically(void) {
    SZrExecutionFrameSlot slots[3] = {
        {20u, 0u, 0u, 8u, 8u, 0u, 1u, 3u,
         ZR_EXECUTION_FRAME_SLOT_SCALAR, 0u},
        {21u, 1u, 8u, 4u, 4u, 0u, 1u, 4u,
         ZR_EXECUTION_FRAME_SLOT_SCALAR, 0u},
        {22u, 2u, 16u, 8u, 8u, 0u, 1u, 5u,
         ZR_EXECUTION_FRAME_SLOT_REFERENCE,
         ZR_EXECUTION_FRAME_SLOT_FLAG_INITIALIZED}
    };
    SZrExecutionFrameLayout layout = make_layout(slots);
    SZrFrameObservationRequest request;
    SZrExecutionFrameDiagnostic diagnostic;
    TZrUInt64 scalarValues[3] = {UINT64_C(0x1122334455667788), 0xabcdefu, 0u};
    TZrUInt64 writeback[3] = {0u, 0u, 0u};
    TZrUInt32 invalidated[3] = {99u, 99u, 99u};
    TZrUInt32 invalidatedCount = 99u;
    TZrByte frame[40] = {0};
    TZrByte snapshot[40];

    assert(ZrCore_ExecutionFrameLayout_Finalize(&layout, &diagnostic));
    memset(&request, 0, sizeof(request));
    request.layout = &layout;
    request.frameBase = frame;
    request.frameByteSize = (TZrUInt32)sizeof(frame);
    request.scalarValues = scalarValues;
    request.scalarValueCount = 3u;
    request.writebackValues = writeback;
    request.writebackCapacity = 3u;
    request.invalidatedPhysicalSlots = invalidated;
    request.invalidatedCapacity = 3u;
    request.invalidatedCount = &invalidatedCount;
    request.flags = ZR_EXECUTION_FRAME_OBSERVE_WRITE_SCALARS |
                    ZR_EXECUTION_FRAME_OBSERVE_INVALIDATE;
    assert(ZrCore_Execution_ObserveFrame(&request, &diagnostic));
    assert(writeback[0] == scalarValues[0]);
    assert((writeback[1] & UINT64_C(0xffffffff)) == scalarValues[1]);
    assert(invalidatedCount == 3u);
    assert(invalidated[0] == 0u && invalidated[1] == 1u && invalidated[2] == 2u);

    memcpy(snapshot, frame, sizeof(snapshot));
    request.frameByteSize = layout.frameByteSize - 1u;
    invalidatedCount = 77u;
    assert(!ZrCore_Execution_ObserveFrame(&request, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_FRAME_BOUNDS);
    assert(invalidatedCount == 77u);
    assert(memcmp(snapshot, frame, sizeof(snapshot)) == 0);
}

static void test_root_and_observation_validation_failures(void) {
    SZrExecutionFrameSlot slots[3] = {
        {10u, 0u, 0u, 8u, 8u, 0u, 1u, 1u,
         ZR_EXECUTION_FRAME_SLOT_REFERENCE, 0u},
        {11u, 1u, 8u, 8u, 8u, 0u, 1u, 1u,
         ZR_EXECUTION_FRAME_SLOT_REFERENCE, 0u},
        {12u, 2u, 16u, 16u, 8u, 0u, 1u, 1u,
         ZR_EXECUTION_FRAME_SLOT_INLINE_SPAN, 0u}
    };
    SZrExecutionFrameLayout layout = make_layout(slots);
    SZrExecutionFrameRootMap map;
    SZrExecutionFrameRootSpec badDerived = {
        11u, ZR_EXECUTION_FRAME_ROOT_DERIVED,
        ZR_EXECUTION_FRAME_ROOT_STORAGE_VALUE_BYTES, 0u, 10u, 0, ZR_TRUE};
    SZrExecutionFrameDiagnostic diagnostic;

    assert(ZrCore_ExecutionFrameLayout_Finalize(&layout, &diagnostic));
    ZrCore_ExecutionFrameRootMap_Init(&map);
    assert(!ZrCore_ExecutionFrameRootMap_Build(
            &layout, &badDerived, 1u, &map, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_ROOT_INVALID);
    ZrCore_ExecutionFrameRootMap_Free(&map);
}

int main(void) {
    test_precise_roots_and_derived_relocation();
    test_observation_materializes_and_invalidates_atomically();
    test_root_and_observation_validation_failures();
    return 0;
}
