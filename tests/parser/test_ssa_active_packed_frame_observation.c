#include "exec_ir/exec_ir_frame_roots.h"
#include "unity.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Genuine builder owners, physical-byte mirrors, no VM or opaque owner probes. */
enum { FIXTURES = 2, LOGICAL = 4, FRAME_BYTES = 256 };
typedef struct SFixture {
    SZrExecIrPackedFrameLayout layout;
    SZrExecIrPackedFrameLayout owner;
    unsigned owned, built, logicalCount, slotCapacity;
} SFixture;
typedef struct SData {
    TZrUInt64 frame[FRAME_BYTES / sizeof(TZrUInt64)];
    TZrUInt64 mirrors[LOGICAL], writeback[LOGICAL];
    TZrUInt32 invalidated[LOGICAL];
    TZrExecIrValueId active[LOGICAL];
    SZrExecIrActiveFrameObservationResult result;
} SData;
typedef struct SView {
    SZrExecIrPackedFrameLayout layout;
    TZrExecIrValueId ids[LOGICAL];
    TZrUInt32 mapping[LOGICAL];
    EZrExecIrPackedSlotClass classes[LOGICAL];
    SZrExecIrFrameSlot slots[LOGICAL];
} SView;
typedef struct SLayoutImage {
    SZrExecIrPackedFrameLayout header;
    TZrExecIrValueId ids[LOGICAL];
    TZrUInt32 mapping[LOGICAL];
    EZrExecIrPackedSlotClass classes[LOGICAL];
    SZrExecIrFrameSlot slots[LOGICAL];
} SLayoutImage;
typedef struct SWitness {
    unsigned support, feature, drained, builders, calls, accepted;
    unsigned observed, invalidations, layoutPreserved, outputChecked;
    unsigned acquired, freed, priorFailed, priorIgnored, flagsPreserved;
    int lastResult;
} SWitness;
static SFixture g_fixtures[FIXTURES];
static SData g_data, g_priorData;
static SView g_view;
static SWitness g_witness, g_prior;
static SZrExecIrActiveFrameObservation g_request;
static unsigned g_case, g_support, g_feature, g_closed;
static unsigned g_totalAcquired, g_totalFreed;
static void support(int yes) { if (!yes) g_support = 0u; }
static void feature(int yes) { if (!yes) g_feature = 0u; }

static void layout_image(SLayoutImage images[FIXTURES]) {
    memset(images, 0, FIXTURES * sizeof(*images));
    for (unsigned i = 0u; i < FIXTURES; ++i) {
        SFixture *f = &g_fixtures[i];
        if (!f->owned) continue;
        memcpy(&images[i].header, &f->layout, sizeof(f->layout));
        /* Extents and roots come from the successful owner, never mutated counts. */
        if (f->logicalCount) {
            memcpy(images[i].ids, f->owner.logicalValueIds,
                   f->logicalCount * sizeof(*f->owner.logicalValueIds));
            memcpy(images[i].mapping, f->owner.logicalToPhysical,
                   f->logicalCount * sizeof(*f->owner.logicalToPhysical));
            memcpy(images[i].classes, f->owner.slotClasses,
                   f->logicalCount * sizeof(*f->owner.slotClasses));
        }
        if (f->slotCapacity)
            memcpy(images[i].slots, f->owner.frame.slots,
                   f->slotCapacity * sizeof(*f->owner.frame.slots));
    }
}
static void clone_view(const SFixture *f) {
    memset(&g_view, 0, sizeof(g_view));
    memcpy(&g_view.layout, &f->owner, sizeof(g_view.layout));
    memcpy(g_view.ids, f->owner.logicalValueIds, f->logicalCount * sizeof(*g_view.ids));
    memcpy(g_view.mapping, f->owner.logicalToPhysical, f->logicalCount * sizeof(*g_view.mapping));
    memcpy(g_view.classes, f->owner.slotClasses, f->logicalCount * sizeof(*g_view.classes));
    memcpy(g_view.slots, f->owner.frame.slots, f->slotCapacity * sizeof(*g_view.slots));
    g_view.layout.logicalValueIds = g_view.ids;
    g_view.layout.logicalToPhysical = g_view.mapping;
    g_view.layout.slotClasses = g_view.classes; g_view.layout.frame.slots = g_view.slots;
}
static int layout_unchanged(const SLayoutImage before[FIXTURES]) {
    SLayoutImage after[FIXTURES]; layout_image(after);
    return memcmp(before, after, sizeof(after)) == 0;
}
static int diagnostic_is(const SZrExecIrDiagnostic *d,
                         EZrExecutionDiagnosticCode code, TZrUInt32 valueId) {
    return d->code == code && d->functionToken == 0u && d->blockId == 0u &&
        d->instructionId == 0u && d->sourceId == 0u && d->expectedVersion == 0u &&
        d->actualVersion == valueId && d->expectedHash == 0u && d->actualHash == 0u;
}
static SFixture *build(unsigned index, const SZrExecIrPackedValue *values,
                       unsigned count, unsigned returnBytes) {
    SFixture *f = &g_fixtures[index];
    SZrExecIrDiagnostic d;
    SZrExecIrPackedFrameRequest request;
    support(index < FIXTURES && !f->owned && count <= LOGICAL);
    if (!g_support) return f;
    memset(&request, 0, sizeof(request));
    request.functionToken = 71u; request.values = values;
    request.valueCount = count; request.returnBufferSize = returnBytes;
    request.returnBufferAlign = 8u;
    ZrParser_ExecIr_PackedFrameLayoutInit(&f->layout);
    /* Register the initialized actual owner before invoking the genuine builder. */
    f->owned = 1u;
    int result = ZrParser_ExecIr_LayoutPackedFrame(&request, &f->layout, &d);
    if (result == ZR_TRUE) {
        /* Save actual allocation roots immediately, before checking support facts. */
        memcpy(&f->owner, &f->layout, sizeof(f->owner));
        f->logicalCount = count; f->slotCapacity = f->layout.frame.slotCapacity;
        f->built = 1u;
        ++g_witness.builders; ++g_witness.acquired; ++g_totalAcquired;
    }
    support(result == ZR_TRUE && d.code == ZR_EXECUTION_DIAGNOSTIC_NONE);
    if (!g_support) return f;
    support(f->layout.frame.logicalSlotCount == count &&
        f->layout.frame.parameterPrefixCount == 0u &&
        f->layout.frame.parameterCount == 0u && f->layout.frame.localCount == count &&
        f->layout.frame.slotCount == f->layout.frame.storageSlotCount &&
        f->layout.frame.slotCapacity == count && f->layout.frame.frameByteSize <= FRAME_BYTES &&
        f->layout.frame.layoutHash != 0u);
    support(count == 0u || (f->layout.logicalValueIds && f->layout.logicalToPhysical &&
        f->layout.slotClasses && f->layout.frame.slots));
    if (!g_support) return f;
    for (unsigned i = 0u; i < count; ++i) {
        support(f->layout.logicalValueIds[i] == values[i].valueId &&
            f->layout.slotClasses[i] == values[i].slotClass &&
            f->layout.logicalToPhysical[i] < f->layout.frame.slotCount);
    }
    return f;
}
static SFixture *standard(unsigned index) {
    const SZrExecIrPackedValue values[] = {
        {10u, 81u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 8u, 8u, 0u, 2u, 0u},
        {20u, 81u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 8u, 8u, 2u, 5u, 0u},
        {30u, 81u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 8u, 8u, 0u, 5u, 0u}
    };
    SFixture *f = build(index, values, 3u, 8u);
    if (!g_support) return f;
    support(f->layout.frame.storageSlotCount == 2u &&
        f->layout.logicalToPhysical[0] == 0u && f->layout.logicalToPhysical[1] == 0u &&
        f->layout.logicalToPhysical[2] == 1u && f->layout.frame.slots[0].byteOffset == 0u &&
        f->layout.frame.slots[1].byteOffset == 8u && f->layout.frame.returnBufferOffset == 16u &&
        f->layout.frame.frameByteSize == 24u && f->layout.frame.frameByteAlign == 8u);
    for (unsigned i = 0u; i < 2u; ++i) {
        support(f->layout.frame.slots[i].byteSize == 8u &&
            f->layout.frame.slots[i].byteAlign == 8u &&
            f->layout.frame.slots[i].kind == ZR_EXEC_IR_FRAME_SLOT_VALUE &&
            f->layout.frame.slots[i].byteOffset + 8u <= f->layout.frame.frameByteSize);
    }
    return f;
}
static void data_reset(void) {
    memset(&g_data, 0xA7, sizeof(g_data));
    g_data.mirrors[0] = 41u; g_data.mirrors[1] = 93u;
    g_data.mirrors[2] = 157u; g_data.mirrors[3] = 211u;
    for (unsigned i = 0u; i < LOGICAL; ++i) {
        g_data.writeback[i] = UINT64_C(0x1020304050607080) + i;
        g_data.invalidated[i] = UINT32_C(0xA0B0C000) + i;
        g_data.active[i] = 0u;
    }
    g_data.result.observedValueCount = 301u;
    g_data.result.invalidatedCount = 302u;
}
static void request_for(SFixture *f, TZrExecIrValueId a, TZrExecIrValueId b,
                        unsigned count) {
    memset(&g_request, 0, sizeof(g_request));
    g_data.active[0] = a; g_data.active[1] = b;
    g_request.layout = &f->layout;
    g_request.frameBase = (TZrByte *)g_data.frame;
    g_request.frameCapacityBytes = sizeof(g_data.frame);
    g_request.activeValueIds = g_data.active; g_request.activeValueCount = count;
    g_request.scalarValues = g_data.mirrors; g_request.scalarValueCount = f->logicalCount;
    g_request.writebackValues = g_data.writeback; g_request.writebackCapacity = LOGICAL;
    g_request.invalidatedPhysicalSlots = g_data.invalidated;
    g_request.invalidatedCapacity = LOGICAL;
}
static void observe(int wantSuccess, EZrExecutionDiagnosticCode code,
                    TZrUInt32 badId, unsigned first, unsigned second, unsigned count) {
    if (!g_support) return;
    SData before, expected;
    SZrExecIrActiveFrameObservation requestBefore;
    SZrExecIrDiagnostic d;
    SLayoutImage beforeLayout[FIXTURES], afterLayout[FIXTURES];
    SView viewBefore;
    unsigned failed = (unsigned)Unity.CurrentTestFailed;
    unsigned ignored = (unsigned)Unity.CurrentTestIgnored;
    memcpy(&before, &g_data, sizeof(before)); memcpy(&expected, &before, sizeof(expected));
    memcpy(&requestBefore, &g_request, sizeof(requestBefore));
    layout_image(beforeLayout); memcpy(&viewBefore, &g_view, sizeof(viewBefore));
    if (wantSuccess) {
        /* Independent explicit logical indices supplied by each case, not ID resolution. */
        const unsigned selected[] = {first, second};
        for (unsigned i = 0u; i < count; ++i) {
            unsigned logical = selected[i];
            const SZrExecIrPackedFrameLayout *layout = g_request.layout;
            unsigned physical = layout->logicalToPhysical[logical];
            const SZrExecIrFrameSlot *slot = &layout->frame.slots[physical];
            support(logical < LOGICAL && slot->byteSize <= sizeof(TZrUInt64));
            if (!g_support) return;
            if (g_request.scalarValues)
                memcpy((unsigned char *)expected.frame + slot->byteOffset,
                       &before.mirrors[logical], slot->byteSize);
            expected.writeback[logical] = 0u;
            memcpy(&expected.writeback[logical],
                   (unsigned char *)expected.frame + slot->byteOffset, slot->byteSize);
            expected.invalidated[i] = physical;
        }
        expected.result.observedValueCount = count;
        expected.result.invalidatedCount = count;
    }
    memset(&d, 0xB6, sizeof(d));
    int result = ZrParser_ExecIr_ObserveActiveFrame(&g_request, &g_data.result, &d);
    ++g_witness.calls; g_witness.lastResult = result;
    support(result == ZR_FALSE || result == ZR_TRUE);
    layout_image(afterLayout);
    unsigned unchangedLayout = (unsigned)(memcmp(beforeLayout, afterLayout, sizeof(beforeLayout)) == 0 &&
        memcmp(&viewBefore, &g_view, sizeof(g_view)) == 0);
    support(unchangedLayout && memcmp(&g_request, &requestBefore, sizeof(g_request)) == 0 &&
        memcmp(g_data.mirrors, before.mirrors, sizeof(g_data.mirrors)) == 0 &&
        memcmp(g_data.active, before.active, sizeof(g_data.active)) == 0 &&
        failed == (unsigned)Unity.CurrentTestFailed && ignored == (unsigned)Unity.CurrentTestIgnored);
    g_witness.layoutPreserved &= unchangedLayout;
    /* Restore only borrowed headers after recording any violation; cleanup uses real roots. */
    for (unsigned i = 0u; i < FIXTURES; ++i)
        memcpy(&g_fixtures[i].layout, &beforeLayout[i].header, sizeof(beforeLayout[i].header));
    if (result == ZR_FALSE) support(memcmp(&g_data, &before, sizeof(g_data)) == 0);
    if (result == ZR_TRUE) {
        ++g_witness.accepted;
        support(g_data.result.observedValueCount <= LOGICAL &&
            g_data.result.invalidatedCount <= LOGICAL);
        if (g_data.result.observedValueCount <= LOGICAL)
            g_witness.observed += g_data.result.observedValueCount;
        if (g_data.result.invalidatedCount <= LOGICAL)
            g_witness.invalidations += g_data.result.invalidatedCount;
    }
    feature(result == (wantSuccess ? ZR_TRUE : ZR_FALSE));
    feature(diagnostic_is(&d, code, badId));
    feature(memcmp(&g_data, &expected, sizeof(g_data)) == 0);
    g_witness.outputChecked = 1u;
}
static void success(unsigned first, unsigned second, unsigned count) {
    observe(1, ZR_EXECUTION_DIAGNOSTIC_NONE, 0u, first, second, count);
}
static void refusal(EZrExecutionDiagnosticCode code, TZrUInt32 badId) {
    observe(0, code, badId, 0u, 0u, 0u);
}
static void missing_argument(unsigned which) {
    SData before;
    SLayoutImage image[FIXTURES];
    SZrExecIrActiveFrameObservation requestBefore;
    SZrExecIrDiagnostic d;
    memcpy(&before, &g_data, sizeof(before)); layout_image(image);
    memcpy(&requestBefore, &g_request, sizeof(requestBefore));
    unsigned failed = (unsigned)Unity.CurrentTestFailed, ignored = (unsigned)Unity.CurrentTestIgnored;
    memset(&d, 0xB6, sizeof(d));
    int result = ZrParser_ExecIr_ObserveActiveFrame(which ? &g_request : NULL,
                                                  which ? NULL : &g_data.result, &d);
    ++g_witness.calls;
    support(result == ZR_FALSE || result == ZR_TRUE);
    support(layout_unchanged(image) && memcmp(&g_request, &requestBefore, sizeof(g_request)) == 0 &&
        failed == (unsigned)Unity.CurrentTestFailed && ignored == (unsigned)Unity.CurrentTestIgnored);
    if (result == ZR_FALSE) support(memcmp(&before, &g_data, sizeof(before)) == 0);
    if (result == ZR_TRUE) ++g_witness.accepted;
    feature(result == ZR_FALSE && memcmp(&before, &g_data, sizeof(before)) == 0 &&
        diagnostic_is(&d, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u));
    g_witness.outputChecked = 1u;
}
static void release_fixture(SFixture *f) {
    if (!f->owned) return;
    /* Owner roots were registered before checks; never free a caller-mutated header. */
    memcpy(&f->layout, &f->owner, sizeof(f->layout));
    ZrParser_ExecIr_PackedFrameLayoutFree(&f->layout); f->owned = 0u;
    if (f->built) { ++g_witness.freed; ++g_totalFreed; }
    support(f->layout.frame.slots == NULL && f->layout.logicalValueIds == NULL &&
        f->layout.logicalToPhysical == NULL && f->layout.slotClasses == NULL &&
        f->layout.frame.slotCount == 0u && f->layout.frame.logicalSlotCount == 0u);
    f->built = 0u; memset(&f->owner, 0, sizeof(f->owner));
}
static void drain(void) {
    if (g_closed) return;
    unsigned failed = (unsigned)Unity.CurrentTestFailed;
    unsigned ignored = (unsigned)Unity.CurrentTestIgnored;
    for (unsigned i = 0u; i < FIXTURES; ++i) {
        release_fixture(&g_fixtures[i]);
    }
    g_witness.drained = (unsigned)(g_witness.acquired == g_witness.freed);
    g_witness.priorFailed = failed; g_witness.priorIgnored = ignored;
    g_witness.flagsPreserved = (unsigned)(failed == (unsigned)Unity.CurrentTestFailed &&
        ignored == (unsigned)Unity.CurrentTestIgnored);
    support(g_witness.drained && g_witness.flagsPreserved);
    g_witness.support = g_support; g_witness.feature = g_feature; g_closed = 1u;
}
static void finish(void) {
    drain();
    if (!g_support) TEST_FAIL_MESSAGE("ACTIVE_PACKED_FRAME_SUPPORT_FAILURE");
    if (!g_feature) TEST_FAIL_MESSAGE("ACTIVE_PACKED_FRAME_MISSING_FEATURE");
}
void setUp(void) {
    ++g_case; g_support = 1u; g_feature = 1u; g_closed = 0u;
    memset(&g_witness, 0, sizeof(g_witness)); g_witness.layoutPreserved = 1u;
    for (unsigned i = 0u; i < FIXTURES; ++i) support(!g_fixtures[i].owned);
    data_reset(); memset(&g_request, 0, sizeof(g_request));
}
void tearDown(void) {
    if (g_case == 15u && !g_closed) {
        support(Unity.CurrentTestFailed == 1u && Unity.CurrentTestIgnored == 0u &&
            g_fixtures[0].owned && g_witness.builders == 1u);
        if (g_support) success(0u, 0u, 1u);
        memcpy(&g_priorData, &g_data, sizeof(g_priorData));
        drain(); memcpy(&g_prior, &g_witness, sizeof(g_prior));
    } else drain();
    /* No assertion, flag reset or recovery in the cleanup callback. */
}

static void test_active_reused_scalar_selection(void) {
    SFixture *f = standard(0u);
    if (g_support) {
        request_for(f, 10u, 0u, 1u); success(0u, 0u, 1u);
        request_for(f, 20u, 0u, 1u); success(1u, 0u, 1u);
    }
    finish();
}
static void test_active_selection_order(void) {
    SFixture *f = standard(0u);
    if (g_support) { request_for(f, 30u, 20u, 2u); success(2u, 1u, 2u); }
    finish();
}
static void test_active_readonly_selection(void) {
    SFixture *f = standard(0u);
    if (g_support) {
        TZrUInt64 payload = UINT64_C(0x3456789012345678);
        memcpy((unsigned char *)g_data.frame + f->layout.frame.slots[
            f->layout.logicalToPhysical[0]].byteOffset, &payload, sizeof(payload));
        request_for(f, 10u, 0u, 1u);
        g_request.scalarValues = NULL; g_request.scalarValueCount = 0u;
        success(0u, 0u, 1u);
        request_for(f, 20u, 0u, 1u);
        g_request.scalarValues = NULL; g_request.scalarValueCount = 0u;
        success(1u, 0u, 1u);
    }
    finish();
}
static void test_active_empty_selection(void) {
    SFixture *f = standard(0u);
    if (g_support) {
        request_for(f, 0u, 0u, 0u); g_request.activeValueIds = NULL;
        success(0u, 0u, 0u);
        g_request.activeValueIds = g_data.active; success(0u, 0u, 0u);
    }
    f = build(1u, NULL, 0u, 0u);
    if (g_support) {
        request_for(f, 0u, 0u, 0u); g_request.activeValueIds = NULL;
        g_request.frameBase = NULL; g_request.frameCapacityBytes = 0u;
        g_request.scalarValues = NULL; g_request.scalarValueCount = 0u;
        g_request.writebackValues = NULL; g_request.writebackCapacity = 0u;
        g_request.invalidatedPhysicalSlots = NULL; g_request.invalidatedCapacity = 0u;
        success(0u, 0u, 0u);
    }
    finish();
}
static void test_active_scalar_widths(void) {
    const unsigned widths[] = {1u, 2u, 4u, 8u};
    for (unsigned i = 0u; i < 4u && g_support; ++i) {
        SZrExecIrPackedValue v = {10u, 81u, ZR_EXEC_IR_PACKED_SLOT_SCALAR,
            widths[i], widths[i], 0u, 2u, 0u};
        SFixture *f = build(0u, &v, 1u, 8u);
        if (g_support) {
            support(f->layout.frame.storageSlotCount == 1u &&
                f->layout.frame.slots[0].byteSize == widths[i]);
            data_reset(); g_data.mirrors[0] = UINT64_C(0xF1E2D3C4B5A69788);
            request_for(f, 10u, 0u, 1u); success(0u, 0u, 1u);
        }
        /* Close only this actual lifetime; report state spans the four owners. */
        release_fixture(f);
    }
    finish();
}
static void test_active_mixed_width_reuse(void) {
    const SZrExecIrPackedValue values[] = {
        {10u, 81u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 8u, 8u, 0u, 2u, 0u},
        {20u, 82u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 2u, 2u, 2u, 5u, 0u}
    };
    SFixture *f = build(0u, values, 2u, 8u);
    if (g_support) {
        support(f->layout.frame.storageSlotCount == 1u &&
            f->layout.logicalToPhysical[0] == f->layout.logicalToPhysical[1] &&
            f->layout.frame.slots[0].byteSize == 8u && f->layout.frame.slots[0].typeToken == 81u);
        g_data.mirrors[1] = UINT64_C(0x8877665544332211);
        request_for(f, 20u, 0u, 1u); success(1u, 0u, 1u);
    }
    finish();
}
static void test_active_value_id_refusal(void) {
    SFixture *f = standard(0u);
    if (g_support) {
        const TZrUInt32 unknown[] = {0u, 99u, UINT32_MAX};
        for (unsigned i = 0u; i < 3u; ++i) {
            request_for(f, unknown[i], 0u, 1u);
            refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, unknown[i]);
        }
        request_for(f, 10u, 10u, 2u);
        refusal(ZR_EXEC_IR_DIAGNOSTIC_DUPLICATE_DEFINITION, 10u);
        request_for(f, 30u, 0u, 1u);
        clone_view(f); g_request.layout = &g_view.layout;
        g_view.ids[1] = 10u;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_DUPLICATE_DEFINITION, 10u);
    }
    finish();
}
static void test_active_conflict_refusal(void) {
    SFixture *f = standard(0u);
    if (g_support) {
        request_for(f, 10u, 20u, 2u);
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, 20u);
        g_data.mirrors[1] = g_data.mirrors[0];
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, 20u);
    }
    finish();
}
static void test_active_capacity_refusal(void) {
    SFixture *f = standard(0u);
    if (g_support) {
        request_for(f, 20u, 0u, 1u);
        g_request.scalarValueCount = 1u; refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u);
        g_request.scalarValueCount = 3u;
        g_request.frameCapacityBytes = f->layout.frame.frameByteSize - 1u;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u);
        g_request.frameCapacityBytes = sizeof(g_data.frame);
        g_request.writebackCapacity = 1u; refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 20u);
        g_request.writebackCapacity = LOGICAL;
        g_request.invalidatedCapacity = 0u; refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u);
        g_request.invalidatedCapacity = LOGICAL;
        g_request.scalarValues = NULL; refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u);
        g_request.scalarValueCount = 0u; g_request.writebackValues = NULL;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u);
        g_request.writebackValues = g_data.writeback; g_request.invalidatedPhysicalSlots = NULL;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u);
        g_request.invalidatedPhysicalSlots = g_data.invalidated; g_request.activeValueIds = NULL;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u);
        g_request.activeValueIds = g_data.active; g_request.layout = NULL;
        refusal(ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
        request_for(f, 20u, 0u, 1u); missing_argument(0u); missing_argument(1u);
    }
    finish();
}
static void test_active_layout_geometry_refusal(void) {
    SFixture *f = standard(0u);
    if (g_support) {
        request_for(f, 10u, 0u, 1u);
        clone_view(f); g_request.layout = &g_view.layout;
        SZrExecIrFrameLayout saved = g_view.layout.frame;
        g_view.mapping[2] = saved.slotCount;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 30u);
        g_view.mapping[2] = 1u;
        g_view.layout.frame.slotCapacity = saved.slotCount - 1u;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u); g_view.layout.frame = saved;
        g_view.layout.frame.storageSlotCount = saved.slotCount - 1u;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u); g_view.layout.frame = saved;
        g_view.layout.frame.parameterPrefixCount = 4u;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u); g_view.layout.frame = saved;
        g_view.layout.frame.localCount = 0u;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u); g_view.layout.frame = saved;
        SZrExecIrFrameSlot slot = saved.slots[1];
        saved.slots[1].byteOffset = saved.slots[0].byteOffset;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 30u); saved.slots[1] = slot;
        saved.slots[1].byteOffset = slot.byteOffset + 1u;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 30u); saved.slots[1] = slot;
        saved.slots[1].byteOffset = saved.frameByteSize;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 30u); saved.slots[1] = slot;
        saved.slots[1].byteSize = 0u;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 30u); saved.slots[1] = slot;
        saved.slots[1].byteAlign = 3u;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 30u); saved.slots[1] = slot;
        saved.slots[1].kind = ZR_EXEC_IR_FRAME_SLOT_REFERENCE;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 30u); saved.slots[1] = slot;
        g_view.classes[2] = (EZrExecIrPackedSlotClass)99;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 30u);
        g_view.classes[2] = ZR_EXEC_IR_PACKED_SLOT_SCALAR;
        g_view.mapping[2] = 0u;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u);
        g_view.mapping[2] = 1u;
        g_view.layout.frame.frameByteAlign = 3u;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u); g_view.layout.frame = saved;
        g_view.layout.frame.returnBufferOffset = saved.frameByteSize + 1u;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u); g_view.layout.frame = saved;
        g_view.layout.frame.parameterCount = 1u;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u); g_view.layout.frame = saved;
        g_view.layout.logicalValueIds = NULL;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u); g_view.layout.logicalValueIds = g_view.ids;
        g_view.layout.logicalToPhysical = NULL;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u); g_view.layout.logicalToPhysical = g_view.mapping;
        g_view.layout.slotClasses = NULL;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u); g_view.layout.slotClasses = g_view.classes;
        g_view.layout.frame.slots = NULL;
        refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0u); g_view.layout.frame = saved;
    }
    finish();
}
static void test_active_unsupported_and_mixed_layout(void) {
    const EZrExecIrPackedSlotClass classes[] = {ZR_EXEC_IR_PACKED_SLOT_REF,
        ZR_EXEC_IR_PACKED_SLOT_BOXED, ZR_EXEC_IR_PACKED_SLOT_INLINE_SPAN};
    for (unsigned i = 0u; i < 3u && g_support; ++i) {
        SZrExecIrPackedValue v = {10u, 81u, classes[i], 8u, 8u, 0u, 2u, 0u};
        SFixture *f = build(0u, &v, 1u, 8u);
        if (g_support) { request_for(f, 10u, 0u, 1u); refusal(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 10u); }
        release_fixture(f);
    }
    const SZrExecIrPackedValue wide = {10u, 81u, ZR_EXEC_IR_PACKED_SLOT_SCALAR,
        16u, 8u, 0u, 2u, 0u};
    SFixture *f = build(0u, &wide, 1u, 8u);
    if (g_support) { request_for(f, 10u, 0u, 1u); refusal(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 10u); }
    const SZrExecIrPackedValue mixed[] = {
        {10u, 81u, ZR_EXEC_IR_PACKED_SLOT_SCALAR, 8u, 8u, 0u, 2u, 0u},
        {20u, 82u, ZR_EXEC_IR_PACKED_SLOT_REF, 8u, 8u, 0u, 2u, 0u},
        {30u, 83u, ZR_EXEC_IR_PACKED_SLOT_BOXED, 8u, 8u, 0u, 2u, 0u},
        {40u, 84u, ZR_EXEC_IR_PACKED_SLOT_INLINE_SPAN, 8u, 8u, 0u, 2u, 0u}
    };
    f = build(1u, mixed, 4u, 8u);
    if (g_support) {
        support(f->layout.frame.storageSlotCount == 4u &&
            f->layout.logicalToPhysical[0] != f->layout.logicalToPhysical[1]);
        request_for(f, 10u, 0u, 1u); success(0u, 0u, 1u);
    }
    finish();
}
static void test_active_alias_refusal(void) {
    SFixture *f = standard(0u);
    if (g_support) {
        request_for(f, 10u, 0u, 1u);
        g_request.frameBase = (TZrByte *)g_data.writeback;
        g_request.frameCapacityBytes = sizeof(g_data.writeback);
        refusal(ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
        request_for(f, 10u, 0u, 1u);
        g_request.frameBase = (TZrByte *)g_data.mirrors;
        g_request.frameCapacityBytes = sizeof(g_data.mirrors);
        refusal(ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
        request_for(f, 10u, 0u, 1u);
        g_request.invalidatedPhysicalSlots = f->layout.logicalValueIds;
        g_request.invalidatedCapacity = f->logicalCount;
        refusal(ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
        request_for(f, 10u, 0u, 1u);
        g_request.writebackValues = g_data.mirrors;
        refusal(ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
        request_for(f, 10u, 0u, 1u);
        g_request.invalidatedPhysicalSlots = g_data.active;
        refusal(ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
        request_for(f, 10u, 0u, 1u);
        /* A genuine single UInt32 output object also serves as a result field. */
        g_request.invalidatedPhysicalSlots = &g_data.result.observedValueCount;
        g_request.invalidatedCapacity = 1u;
        refusal(ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
    }
    finish();
}
static void test_active_repeated_layout_lifetime(void) {
    SFixture *f = standard(0u);
    for (unsigned i = 0u; i < 4096u && g_support; ++i) {
        request_for(f, (i & 1u) ? 20u : 10u, 0u, 1u);
        success(i & 1u, 0u, 1u);
    }
    release_fixture(f);
    const SZrExecIrPackedValue v = {70u, 91u, ZR_EXEC_IR_PACKED_SLOT_SCALAR,
        8u, 8u, 0u, 2u, 0u};
    f = build(0u, &v, 1u, 8u);
    if (g_support) {
        request_for(f, 10u, 0u, 1u); refusal(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, 10u);
        request_for(f, 70u, 0u, 1u); success(0u, 0u, 1u);
    }
    finish();
}
static void test_active_legacy_contract(void) {
    SFixture *f = standard(0u);
    if (g_support) {
        SZrExecIrFrameObservation old;
        SZrExecIrDiagnostic d;
        memset(&old, 0, sizeof(old)); old.layout = &f->layout;
        old.frameBase = (TZrByte *)g_data.frame; old.scalarValues = g_data.mirrors;
        old.scalarValueCount = 3u; old.writebackValues = g_data.writeback;
        old.writebackCapacity = LOGICAL; old.invalidatedPhysicalSlots = g_data.invalidated;
        old.invalidatedCapacity = LOGICAL; old.invalidatedCount = 29u;
        SData before; memcpy(&before, &g_data, sizeof(before));
        SLayoutImage image[FIXTURES]; layout_image(image);
        int result = ZrParser_ExecIr_ObserveFrame(&old, &d);
        support(result == ZR_FALSE && d.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
            old.invalidatedCount == 29u && memcmp(&before, &g_data, sizeof(before)) == 0 &&
            layout_unchanged(image));
        g_data.mirrors[1] = g_data.mirrors[0];
        result = ZrParser_ExecIr_ObserveFrame(&old, &d);
        support(result == ZR_TRUE && old.invalidatedCount == 2u &&
            g_data.writeback[0] == 41u && g_data.writeback[1] == 41u &&
            g_data.writeback[2] == 157u && g_data.invalidated[0] == 0u &&
            g_data.invalidated[1] == 1u && layout_unchanged(image));
        old.scalarValues = NULL; old.scalarValueCount = 0u;
        memcpy(&before, &g_data, sizeof(before));
        result = ZrParser_ExecIr_ObserveFrame(&old, &d);
        support(result == ZR_TRUE && old.invalidatedCount == 2u &&
            memcmp(before.frame, g_data.frame, sizeof(before.frame)) == 0 &&
            g_data.writeback[0] == 41u && g_data.writeback[1] == 41u &&
            g_data.writeback[2] == 157u && layout_unchanged(image));
        g_witness.outputChecked = 1u;
    }
    finish();
}
static void test_active_prior_unity_failure(void) {
    SFixture *f = standard(0u);
    if (!g_support) finish();
    request_for(f, 10u, 0u, 1u);
    TEST_FAIL_MESSAGE("ACTIVE_PACKED_FRAME_PRIOR_FAILURE");
}
static void test_active_prior_failure_witness(void) {
    support(g_prior.support == 1u && g_prior.drained == 1u && g_prior.builders == 1u &&
        g_prior.calls == 1u && g_prior.acquired == 1u && g_prior.freed == 1u &&
        g_prior.layoutPreserved == 1u && g_prior.outputChecked == 1u &&
        g_prior.priorFailed == 1u && g_prior.priorIgnored == 0u && g_prior.flagsPreserved == 1u);
    support(Unity.CurrentTestFailed == 0u && Unity.CurrentTestIgnored == 0u);
    for (unsigned i = 0u; i < FIXTURES; ++i) support(!g_fixtures[i].owned);
    support(g_prior.lastResult == ZR_FALSE || g_prior.lastResult == ZR_TRUE);
    support(g_prior.accepted == (unsigned)(g_prior.lastResult == ZR_TRUE));
    support(g_priorData.mirrors[0] == 41u && g_priorData.mirrors[1] == 93u &&
        g_priorData.mirrors[2] == 157u && g_priorData.mirrors[3] == 211u &&
        g_priorData.active[0] == 10u && g_priorData.active[1] == 0u &&
        g_priorData.active[2] == 0u && g_priorData.active[3] == 0u);
    if (g_prior.lastResult == ZR_FALSE) {
        support(g_prior.observed == 0u && g_prior.invalidations == 0u &&
            g_priorData.result.observedValueCount == 301u && g_priorData.result.invalidatedCount == 302u);
        for (unsigned i = 0u; i < LOGICAL; ++i) {
            support(g_priorData.writeback[i] == UINT64_C(0x1020304050607080) + i &&
                g_priorData.invalidated[i] == UINT32_C(0xA0B0C000) + i);
        }
        const unsigned char *unchanged = (const unsigned char *)g_priorData.frame;
        for (size_t i = 0u; i < sizeof(g_priorData.frame); ++i) support(unchanged[i] == 0xA7u);
    }
    feature(g_prior.feature == 1u && g_prior.accepted == 1u && g_prior.observed == 1u &&
        g_prior.invalidations == 1u && g_priorData.result.observedValueCount == 1u &&
        g_priorData.result.invalidatedCount == 1u && g_priorData.writeback[0] == 41u &&
        g_priorData.writeback[1] == UINT64_C(0x1020304050607081) &&
        g_priorData.writeback[2] == UINT64_C(0x1020304050607082) &&
        g_priorData.writeback[3] == UINT64_C(0x1020304050607083) &&
        g_priorData.invalidated[0] == 0u && g_priorData.invalidated[1] == UINT32_C(0xA0B0C001) &&
        g_priorData.invalidated[2] == UINT32_C(0xA0B0C002) &&
        g_priorData.invalidated[3] == UINT32_C(0xA0B0C003));
    TZrUInt64 actual; memcpy(&actual, g_priorData.frame, sizeof(actual)); feature(actual == 41u);
    const unsigned char *bytes = (const unsigned char *)g_priorData.frame;
    for (size_t i = sizeof(TZrUInt64); i < sizeof(g_priorData.frame); ++i) feature(bytes[i] == 0xA7u);
    g_witness.outputChecked = 1u; finish();
}
static void emit_case(void) {
    printf("ACTIVE_PACKED_FRAME_CASE case=%u support=%u feature=%u drained=%u builders=%u calls=%u accepted=%u observed=%u invalidations=%u layout_preserved=%u output_checked=%u acquired=%u freed=%u prior_failed=%u prior_ignored=%u flags_preserved=%u\n",
        g_case, g_witness.support, g_witness.feature, g_witness.drained, g_witness.builders,
        g_witness.calls, g_witness.accepted, g_witness.observed, g_witness.invalidations,
        g_witness.layoutPreserved, g_witness.outputChecked, g_witness.acquired, g_witness.freed,
        g_witness.priorFailed, g_witness.priorIgnored, g_witness.flagsPreserved);
}
static void final_drain(void) {
    unsigned failed = (unsigned)Unity.CurrentTestFailed, ignored = (unsigned)Unity.CurrentTestIgnored;
    unsigned live = 0u;
    drain();
    for (unsigned i = 0u; i < FIXTURES; ++i) live += g_fixtures[i].owned;
    unsigned flags = (unsigned)(failed == (unsigned)Unity.CurrentTestFailed &&
        ignored == (unsigned)Unity.CurrentTestIgnored);
    printf("ACTIVE_PACKED_FRAME_FINAL drain=%u live=%u acquired=%u freed=%u prior_failed=%u prior_ignored=%u flags_preserved=%u\n",
        (unsigned)(live == 0u && g_totalAcquired == g_totalFreed), live,
        g_totalAcquired, g_totalFreed, failed, ignored, flags);
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_active_reused_scalar_selection); emit_case();
    RUN_TEST(test_active_selection_order); emit_case();
    RUN_TEST(test_active_readonly_selection); emit_case();
    RUN_TEST(test_active_empty_selection); emit_case();
    RUN_TEST(test_active_scalar_widths); emit_case();
    RUN_TEST(test_active_mixed_width_reuse); emit_case();
    RUN_TEST(test_active_value_id_refusal); emit_case();
    RUN_TEST(test_active_conflict_refusal); emit_case();
    RUN_TEST(test_active_capacity_refusal); emit_case();
    RUN_TEST(test_active_layout_geometry_refusal); emit_case();
    RUN_TEST(test_active_unsupported_and_mixed_layout); emit_case();
    RUN_TEST(test_active_alias_refusal); emit_case();
    RUN_TEST(test_active_repeated_layout_lifetime); emit_case();
    RUN_TEST(test_active_legacy_contract); emit_case();
    RUN_TEST(test_active_prior_unity_failure); emit_case();
    RUN_TEST(test_active_prior_failure_witness); emit_case();
    final_drain(); return UNITY_END();
}
