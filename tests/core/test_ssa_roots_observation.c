#include "zr_vm_core/execution_frame_layout.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)
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
    CHECK(root != ZR_NULL);
    CHECK(slotAddress != ZR_NULL);
    CHECK(state != ZR_NULL);
    if (root->kind == ZR_EXECUTION_FRAME_ROOT_MANAGED) {
        TZrPtr replacement = state->movedBase;
        TZrPtr observed = ZR_NULL;
        memcpy(&observed, slotAddress, sizeof(observed));
        CHECK(observed == state->oldBase);
        memcpy(slotAddress, &replacement, sizeof(replacement));
        ++state->managedCount;
    } else if (root->kind == ZR_EXECUTION_FRAME_ROOT_DERIVED) {
        TZrPtr base = ZR_NULL;
        CHECK(baseAddress != ZR_NULL);
        memcpy(&base, baseAddress, sizeof(base));
        CHECK(base == state->movedBase);
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
    TZrByte oldObject[8] = {0};
    TZrByte movedObject[8] = {0};
    TZrPtr oldBase = (TZrPtr)&oldObject;
    TZrPtr movedBase = (TZrPtr)&movedObject;
    TZrPtr derived = (TZrPtr)(frame + 8u);

    CHECK(ZrCore_ExecutionFrameLayout_Finalize(&layout, &diagnostic));
    ZrCore_ExecutionFrameRootMap_Init(&map);
    CHECK(ZrCore_ExecutionFrameRootMap_Build(
            &layout, specs, 3u, &map, &diagnostic));
    CHECK(ZrCore_ExecutionFrameRootMap_Validate(
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
    CHECK(ZrCore_Execution_VisitFrameRoots(
            ZR_NULL, &visitor, &diagnostic));
    CHECK(state.managedCount == 1u);
    CHECK(state.derivedCount == 1u);
    CHECK(state.inlineCount == 1u);
    memcpy(&oldBase, frame, sizeof(oldBase));
    CHECK(oldBase == movedBase);
    memcpy(&derived, frame + 8u, sizeof(derived));
    CHECK(derived == (TZrPtr)((TZrByte *)movedBase + 4u));
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

    CHECK(ZrCore_ExecutionFrameLayout_Finalize(&layout, &diagnostic));
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
    CHECK(ZrCore_Execution_ObserveFrame(&request, &diagnostic));
    CHECK(writeback[0] == scalarValues[0]);
    CHECK((writeback[1] & UINT64_C(0xffffffff)) == scalarValues[1]);
    CHECK(invalidatedCount == 3u);
    CHECK(invalidated[0] == 0u && invalidated[1] == 1u && invalidated[2] == 2u);

    memcpy(snapshot, frame, sizeof(snapshot));
    request.frameByteSize = layout.frameByteSize - 1u;
    invalidatedCount = 77u;
    CHECK(!ZrCore_Execution_ObserveFrame(&request, &diagnostic));
    CHECK(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_FRAME_BOUNDS);
    CHECK(invalidatedCount == 77u);
    CHECK(memcmp(snapshot, frame, sizeof(snapshot)) == 0);
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

    CHECK(ZrCore_ExecutionFrameLayout_Finalize(&layout, &diagnostic));
    ZrCore_ExecutionFrameRootMap_Init(&map);
    CHECK(!ZrCore_ExecutionFrameRootMap_Build(
            &layout, &badDerived, 1u, &map, &diagnostic));
    CHECK(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_ROOT_INVALID);
    ZrCore_ExecutionFrameRootMap_Free(&map);
}

typedef struct SInlineFieldVisit {
    TZrPtr expectedAddress;
    TZrPtr replacement;
    TZrUInt32 count;
} SInlineFieldVisit;

static TZrBool visit_inline_field(SZrExecutionFrameRoot *root,
                                  TZrPtr slotAddress,
                                  TZrPtr baseAddress,
                                  TZrPtr userData) {
    SInlineFieldVisit *visit = (SInlineFieldVisit *)userData;
    CHECK(root->kind == ZR_EXECUTION_FRAME_ROOT_INLINE_FIELD);
    CHECK(slotAddress == visit->expectedAddress);
    CHECK(baseAddress == ZR_NULL);
    memcpy(slotAddress, &visit->replacement, sizeof(visit->replacement));
    ++visit->count;
    return ZR_TRUE;
}

static void test_inline_field_at_frame_end(void) {
    const TZrUInt32 pointerSize = (TZrUInt32)sizeof(TZrPtr);
    SZrExecutionFrameSlot slot = {
        1u, 0u, 0u, 2u * (TZrUInt32)sizeof(TZrPtr),
        (TZrUInt32)sizeof(TZrPtr), 0u, 1u, 1u,
        ZR_EXECUTION_FRAME_SLOT_INLINE_SPAN,
        ZR_EXECUTION_FRAME_SLOT_FLAG_INITIALIZED
    };
    SZrExecutionFrameRootSpec spec = {
        1u, ZR_EXECUTION_FRAME_ROOT_INLINE_FIELD,
        ZR_EXECUTION_FRAME_ROOT_STORAGE_VALUE_BYTES,
        (TZrUInt32)sizeof(TZrPtr), 0u, 0, ZR_TRUE
    };
    SZrExecutionFrameLayout layout;
    SZrExecutionFrameRootMap map;
    SZrExecutionFrameDiagnostic diagnostic;
    TZrByte frame[2u * sizeof(TZrPtr)] = {0};
    TZrByte object = 0u;
    SInlineFieldVisit visit = {frame + pointerSize, &object, 0u};
    SZrFrameRootVisitor visitor = {
        &map, frame, (TZrUInt32)sizeof(frame), visit_inline_field, &visit
    };
    TZrPtr observed = ZR_NULL;
    TZrByte snapshot[sizeof(frame)];
    union {
        TZrPtr alignment;
        TZrByte bytes[sizeof(frame) + 1u];
    } unalignedFrame = {0};

    ZrCore_ExecutionFrameLayout_Init(&layout);
    layout.logicalSlotCount = 1u;
    layout.storageSlotCount = 1u;
    layout.returnBufferOffset = (TZrUInt32)sizeof(frame);
    layout.frameByteSize = (TZrUInt32)sizeof(frame);
    layout.frameByteAlign = pointerSize;
    layout.slots = &slot;
    layout.slotCount = 1u;
    CHECK(ZrCore_ExecutionFrameLayout_Finalize(&layout, &diagnostic));
    ZrCore_ExecutionFrameRootMap_Init(&map);
    CHECK(ZrCore_ExecutionFrameRootMap_Build(
            &layout, &spec, 1u, &map, &diagnostic));
    CHECK(ZrCore_ExecutionFrameRootMap_Validate(&layout, &map, &diagnostic));
    CHECK(ZrCore_Execution_VisitFrameRoots(ZR_NULL, &visitor, &diagnostic));
    CHECK(visit.count == 1u);
    memcpy(&observed, frame + pointerSize, sizeof(observed));
    CHECK(observed == &object);
    memcpy(&observed, frame, sizeof(observed));
    CHECK(observed == ZR_NULL);

    /* The containing span and the pointer field must both fit. */
    memcpy(snapshot, frame, sizeof(frame));
    --visitor.frameByteSize;
    CHECK(!ZrCore_Execution_VisitFrameRoots(ZR_NULL, &visitor, &diagnostic));
    CHECK(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_FRAME_BOUNDS);
    CHECK(visit.count == 1u && memcmp(frame, snapshot, sizeof(frame)) == 0);
    ++visitor.frameByteSize;
    ++spec.fieldByteOffset;
    CHECK(!ZrCore_ExecutionFrameRootMap_Build(
            &layout, &spec, 1u, &map, &diagnostic));
    CHECK(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_FRAME_BOUNDS);
    CHECK(map.rootCount == 1u && map.roots[0].fieldByteOffset == pointerSize);
    --spec.fieldByteOffset;

    map.roots[0].initialized = ZR_FALSE;
    CHECK(ZrCore_Execution_VisitFrameRoots(ZR_NULL, &visitor, &diagnostic));
    CHECK(visit.count == 1u);
    map.roots[0].initialized = ZR_TRUE;
    /* A callback may retain a stationary root, or rewrite it to null. */
    CHECK(ZrCore_Execution_VisitFrameRoots(ZR_NULL, &visitor, &diagnostic));
    CHECK(visit.count == 2u);
    visit.replacement = ZR_NULL;
    CHECK(ZrCore_Execution_VisitFrameRoots(ZR_NULL, &visitor, &diagnostic));
    memcpy(&observed, frame + pointerSize, sizeof(observed));
    CHECK(visit.count == 3u && observed == ZR_NULL);

    /* Byte-address callbacks support unaligned host backing storage. */
    visitor.frameBase = unalignedFrame.bytes + 1u;
    visit.expectedAddress = visitor.frameBase + pointerSize;
    visit.replacement = &object;
    CHECK(ZrCore_Execution_VisitFrameRoots(ZR_NULL, &visitor, &diagnostic));
    memcpy(&observed, visit.expectedAddress, sizeof(observed));
    CHECK(visit.count == 4u && observed == &object);

    ++slot.byteOffset;
    CHECK(!ZrCore_ExecutionFrameLayout_Validate(&layout, &diagnostic));
    CHECK(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_ALIGNMENT);
    --slot.byteOffset;
    CHECK(ZrCore_ExecutionFrameRootMap_Build(
            &layout, ZR_NULL, 0u, &map, &diagnostic));
    visitor.frameByteSize = 0u;
    CHECK(ZrCore_Execution_VisitFrameRoots(ZR_NULL, &visitor, &diagnostic));
    CHECK(visit.count == 4u);
    CHECK(!ZrCore_Execution_VisitFrameRoots(ZR_NULL, ZR_NULL, &diagnostic));
    CHECK(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_ARGUMENT);
    visitor.frameBase = ZR_NULL;
    CHECK(!ZrCore_Execution_VisitFrameRoots(ZR_NULL, &visitor, &diagnostic));
    CHECK(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_ARGUMENT);
    visitor.frameBase = frame;
    visitor.rootMap = ZR_NULL;
    CHECK(!ZrCore_Execution_VisitFrameRoots(ZR_NULL, &visitor, &diagnostic));
    CHECK(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_ARGUMENT);
    visitor.rootMap = &map;
    visitor.visit = ZR_NULL;
    CHECK(!ZrCore_Execution_VisitFrameRoots(ZR_NULL, &visitor, &diagnostic));
    CHECK(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_ARGUMENT);
    ZrCore_ExecutionFrameRootMap_Free(&map);
}

static TZrBool count_only_root(SZrExecutionFrameRoot *root,
                              TZrPtr slotAddress,
                              TZrPtr baseAddress,
                              TZrPtr userData) {
    (void)root;
    (void)slotAddress;
    (void)baseAddress;
    ++*(TZrUInt32 *)userData;
    return ZR_TRUE;
}

static void test_root_field_address_overflow(void) {
    SZrExecutionFrameRootMap map;
    SZrExecutionFrameRoot root = {0};
    SZrExecutionFrameDiagnostic diagnostic;
    TZrByte frame[2u * sizeof(TZrPtr)] = {0};
    TZrUInt32 count = 0u;
    SZrFrameRootVisitor visitor = {
        &map, frame, (TZrUInt32)sizeof(frame), count_only_root, &count
    };

    /* A detached map can validate its containing span without a layout.
     * The visitor must still check overflow of the final field address. */
    ZrCore_ExecutionFrameRootMap_Init(&map);
    root.frameByteOffset = 1u;
    root.byteSize = (TZrUInt32)sizeof(TZrPtr);
    root.kind = ZR_EXECUTION_FRAME_ROOT_MANAGED;
    root.storage = ZR_EXECUTION_FRAME_ROOT_STORAGE_POINTER;
    root.fieldByteOffset = UINT32_MAX;
    root.initialized = ZR_TRUE;
    map.roots = &root;
    map.rootCount = 1u;
    map.rootCapacity = 1u;
    CHECK(ZrCore_ExecutionFrameRootMap_Validate(ZR_NULL, &map, &diagnostic));
    CHECK(!ZrCore_Execution_VisitFrameRoots(ZR_NULL, &visitor, &diagnostic));
    CHECK(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_FRAME_BOUNDS);
    CHECK(diagnostic.index == 0u && diagnostic.actual == 0u);
    CHECK(count == 0u);
    /* The detached fixture borrows a stack root, so it does not call Free. */
}

typedef struct SNonInlineVisit {
    TZrPtr expectedAddress;
    TZrPtr expectedBase;
    TZrUInt32 count;
} SNonInlineVisit;

static TZrBool visit_non_inline_root(SZrExecutionFrameRoot *root,
                                    TZrPtr slotAddress,
                                    TZrPtr baseAddress,
                                    TZrPtr userData) {
    SNonInlineVisit *visit = (SNonInlineVisit *)userData;
    CHECK(root->kind != ZR_EXECUTION_FRAME_ROOT_INLINE_FIELD);
    CHECK(slotAddress == visit->expectedAddress);
    CHECK(baseAddress == visit->expectedBase);
    ++visit->count;
    return ZR_TRUE;
}

static void test_non_inline_root_preserves_access_span(void) {
    const TZrUInt32 pointerSize = (TZrUInt32)sizeof(TZrPtr);
    SZrExecutionFrameSlot slots[2] = {
        {1u, 0u, 0u, 2u * (TZrUInt32)sizeof(TZrPtr),
         (TZrUInt32)sizeof(TZrPtr), 0u, 1u, 1u,
         ZR_EXECUTION_FRAME_SLOT_REFERENCE, 0u},
        {2u, 1u, 2u * (TZrUInt32)sizeof(TZrPtr), (TZrUInt32)sizeof(TZrPtr),
         (TZrUInt32)sizeof(TZrPtr), 0u, 1u, 1u,
         ZR_EXECUTION_FRAME_SLOT_REFERENCE, 0u}
    };
    SZrExecutionFrameLayout layout;
    SZrExecutionFrameRootMap map;
    SZrExecutionFrameRootSpec spec = {
        1u, ZR_EXECUTION_FRAME_ROOT_MANAGED,
        ZR_EXECUTION_FRAME_ROOT_STORAGE_POINTER, 0u, 2u, 0, ZR_TRUE
    };
    const EZrExecutionFrameRootKind kinds[2] = {
        ZR_EXECUTION_FRAME_ROOT_MANAGED, ZR_EXECUTION_FRAME_ROOT_DERIVED
    };
    SZrExecutionFrameDiagnostic diagnostic;
    TZrByte frame[4u * sizeof(TZrPtr)] = {0};
    TZrByte snapshot[sizeof(frame)];
    SNonInlineVisit visit = {frame + 3u * pointerSize, ZR_NULL, 0u};
    SZrFrameRootVisitor visitor = {
        &map, frame, (TZrUInt32)sizeof(frame), visit_non_inline_root, &visit
    };

    ZrCore_ExecutionFrameLayout_Init(&layout);
    layout.logicalSlotCount = 2u;
    layout.storageSlotCount = 2u;
    layout.returnBufferOffset = (TZrUInt32)sizeof(frame);
    layout.frameByteSize = (TZrUInt32)sizeof(frame);
    layout.frameByteAlign = pointerSize;
    layout.slots = slots;
    layout.slotCount = 2u;
    CHECK(ZrCore_ExecutionFrameLayout_Finalize(&layout, &diagnostic));
    ZrCore_ExecutionFrameRootMap_Init(&map);
    memcpy(snapshot, frame, sizeof(frame));
    for (TZrUInt32 index = 0u; index < 2u; ++index) {
        spec.kind = kinds[index];
        spec.fieldByteOffset = 3u * pointerSize;
        visit.count = 0u;
        visit.expectedAddress = frame + spec.fieldByteOffset;
        visit.expectedBase = index == 0u ? ZR_NULL : frame + 2u * pointerSize;
        CHECK(ZrCore_ExecutionFrameRootMap_Build(
                &layout, &spec, 1u, &map, &diagnostic));
        CHECK(ZrCore_ExecutionFrameRootMap_Validate(ZR_NULL, &map, &diagnostic));
        /* One pointer fits, but the non-inline access span does not. */
        CHECK(!ZrCore_Execution_VisitFrameRoots(ZR_NULL, &visitor, &diagnostic));
        CHECK(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_FRAME_BOUNDS);
        CHECK(diagnostic.actual == spec.fieldByteOffset);
        CHECK(visit.count == 0u && memcmp(frame, snapshot, sizeof(frame)) == 0);

        /* Existing nonzero offsets remain valid when the full span fits. */
        for (TZrUInt32 offset = 1u; offset <= 2u; ++offset) {
            spec.fieldByteOffset = offset * pointerSize;
            visit.expectedAddress = frame + spec.fieldByteOffset;
            CHECK(ZrCore_ExecutionFrameRootMap_Build(
                    &layout, &spec, 1u, &map, &diagnostic));
            CHECK(ZrCore_Execution_VisitFrameRoots(ZR_NULL, &visitor, &diagnostic));
            CHECK(visit.count == offset);
        }
    }
    ZrCore_ExecutionFrameRootMap_Free(&map);
}

static void test_observation_reused_scalar_conflict_preserves_outputs(void) {
    SZrExecutionFrameSlot slots[3] = {
        {30u, 0u, 0u, 8u, 8u, 0u, 1u, 3u,
         ZR_EXECUTION_FRAME_SLOT_SCALAR, 0u},
        {31u, 1u, 8u, 8u, 8u, 0u, 2u, 3u,
         ZR_EXECUTION_FRAME_SLOT_SCALAR, 0u},
        {32u, 0u, 0u, 8u, 8u, 1u, 2u, 3u,
         ZR_EXECUTION_FRAME_SLOT_SCALAR, 0u}
    };
    SZrExecutionFrameLayout layout = make_layout(slots);
    SZrExecutionFrameDiagnostic diagnostic;
    TZrByte frame[40], snapshot[40];
    TZrUInt64 values[3] = {111u, 333u, 222u};
    TZrUInt64 writeback[3] = {91u, 92u, 93u};
    TZrUInt64 savedWriteback[3];
    TZrUInt32 invalidated[3] = {81u, 82u, 83u};
    TZrUInt32 savedInvalidated[3];
    TZrUInt32 count = 77u;
    SZrFrameObservationRequest request = {
        &layout, frame, sizeof(frame), values, 3u, writeback, 3u,
        invalidated, 3u, &count,
        ZR_EXECUTION_FRAME_OBSERVE_WRITE_SCALARS |
        ZR_EXECUTION_FRAME_OBSERVE_INVALIDATE
    };

    CHECK(ZrCore_ExecutionFrameLayout_Finalize(&layout, &diagnostic));
    memset(frame, 0xa5, sizeof(frame));
    memcpy(snapshot, frame, sizeof(frame));
    memcpy(savedWriteback, writeback, sizeof(writeback));
    memcpy(savedInvalidated, invalidated, sizeof(invalidated));
    for (TZrUInt32 repeat = 0u; repeat < 2u; ++repeat) {
        CHECK(!ZrCore_Execution_ObserveFrame(&request, &diagnostic));
        CHECK(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_SLOT_OVERLAP);
        CHECK(diagnostic.index == 2u && diagnostic.relatedIndex == 0u);
        CHECK(memcmp(frame, snapshot, sizeof(frame)) == 0);
        CHECK(memcmp(writeback, savedWriteback, sizeof(writeback)) == 0);
        CHECK(memcmp(invalidated, savedInvalidated, sizeof(invalidated)) == 0);
        CHECK(count == 77u);
    }
}

static void test_observation_compatible_scalar_aliases(void) {
    SZrExecutionFrameSlot slots[3] = {
        {40u, 0u, 0u, 4u, 4u, 0u, 1u, 3u,
         ZR_EXECUTION_FRAME_SLOT_SCALAR, 0u},
        {41u, 0u, 0u, 4u, 4u, 1u, 2u, 3u,
         ZR_EXECUTION_FRAME_SLOT_SCALAR, 0u},
        {42u, 1u, 8u, 8u, 8u, 0u, 2u, 3u,
         ZR_EXECUTION_FRAME_SLOT_SCALAR, 0u}
    };
    SZrExecutionFrameLayout layout = make_layout(slots);
    SZrExecutionFrameDiagnostic diagnostic;
    TZrByte frame[40] = {0};
    TZrByte snapshot[40];
    TZrUInt64 values[3] = {111u, 111u, 222u};
    TZrUInt64 writeback[3] = {0u, 0u, 0u};
    TZrUInt32 invalidated[3] = {99u, 99u, 99u};
    TZrUInt32 count = 99u;
    SZrFrameObservationRequest request = {
        &layout, frame, sizeof(frame), values, 3u, writeback, 3u,
        invalidated, 3u, &count,
        ZR_EXECUTION_FRAME_OBSERVE_WRITE_SCALARS |
        ZR_EXECUTION_FRAME_OBSERVE_INVALIDATE
    };

    CHECK(ZrCore_ExecutionFrameLayout_Finalize(&layout, &diagnostic));
    for (TZrUInt32 repeat = 0u; repeat < 2u; ++repeat) {
        CHECK(ZrCore_Execution_ObserveFrame(&request, &diagnostic));
        CHECK(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_NONE);
        CHECK(writeback[0] == writeback[1]);
        CHECK(memcmp(&writeback[0], &values[0], 4u) == 0);
        CHECK(writeback[2] == 222u);
        CHECK(count == 2u && invalidated[0] == 0u && invalidated[1] == 1u);
        CHECK(invalidated[2] == 99u);
    }
    /* Change only bytes outside the actual four-byte payload. This also
     * exercises unused high bits on the supported little-endian targets. */
    ((TZrByte *)&values[1])[sizeof(TZrUInt64) - 1u] ^= 0x80u;
    CHECK(values[0] != values[1]);
    CHECK(ZrCore_Execution_ObserveFrame(&request, &diagnostic));
    CHECK(writeback[0] == writeback[1] && writeback[2] == 222u);

    memcpy(snapshot, frame, sizeof(frame));
    values[1] = 333u;
    request.flags = ZR_EXECUTION_FRAME_OBSERVE_INVALIDATE;
    CHECK(ZrCore_Execution_ObserveFrame(&request, &diagnostic));
    CHECK(memcmp(frame, snapshot, sizeof(frame)) == 0);
    CHECK(writeback[0] == writeback[1] && count == 2u);

    /* The validator also accepts different physical IDs with byte aliases.
     * Their actual overlapping payload, rather than identity, is decisive. */
    slots[1].physicalSlot = 2u;
    CHECK(ZrCore_ExecutionFrameLayout_Hash(&layout) != layout.layoutHash);
    layout.layoutHash = 0u;
    CHECK(ZrCore_ExecutionFrameLayout_Finalize(&layout, &diagnostic));
    request.flags |= ZR_EXECUTION_FRAME_OBSERVE_WRITE_SCALARS;
    CHECK(!ZrCore_Execution_ObserveFrame(&request, &diagnostic));
    CHECK(diagnostic.code == ZR_EXECUTION_FRAME_DIAGNOSTIC_SLOT_OVERLAP);
    CHECK(diagnostic.index == 1u && diagnostic.relatedIndex == 0u);
    CHECK(memcmp(frame, snapshot, sizeof(frame)) == 0);
    values[1] = values[0];
    CHECK(ZrCore_Execution_ObserveFrame(&request, &diagnostic));
    CHECK(writeback[0] == writeback[1] && count == 3u);
    CHECK(invalidated[0] == 0u && invalidated[1] == 2u && invalidated[2] == 1u);
}

int main(void) {
    test_observation_reused_scalar_conflict_preserves_outputs();
    test_observation_compatible_scalar_aliases();
    test_precise_roots_and_derived_relocation();
    test_observation_materializes_and_invalidates_atomically();
    test_root_and_observation_validation_failures();
    test_inline_field_at_frame_end();
    test_root_field_address_overflow();
    test_non_inline_root_preserves_access_span();
    return 0;
}
