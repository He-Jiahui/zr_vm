#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"

static SZrState *state;
void setUp(void) {
    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
}
void tearDown(void) { ZrTests_Runtime_State_Destroy(state); }

static TZrUInt64 collections(void) {
    SZrGarbageCollector *gc = state->global->garbageCollector;
    return gc->collectionCounts[0] + gc->collectionCounts[1] + gc->collectionCounts[2];
}
static TZrSize live_strings(void) {
    TZrSize count = 0u;
    for (SZrRawObject *object = state->global->garbageCollector->gcObjectList;
         object != ZR_NULL; object = object->next)
        if (object->type == ZR_RAW_OBJECT_TYPE_STRING) count++;
    return count;
}
static SZrFunction *compile(const char *source) {
    SZrFunction *function = ZrParser_Source_Compile(state, source, strlen(source),
            ZrCore_String_CreateFromNative(state, "interpreter_gc.zr"));
    TEST_ASSERT_NOT_NULL(function);
    return function;
}
static void expect_int(SZrFunction *function, TZrInt64 expected) {
    TZrInt64 value = 0;
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &value));
    TEST_ASSERT_EQUAL_INT64(expected, value);
}
static void test_allocation_debt_is_repaid_at_interpreter_boundaries(void) {
    SZrFunction *function = compile(
        "var i = 0; while (i < 20000) { var o = { value: i, slot: i }; i = i + 1; } return i;");
    TZrUInt64 before = collections();
    expect_int(function, 20000);
    TEST_ASSERT_TRUE(collections() > before);
    TEST_ASSERT_TRUE(state->global->allocationPeakBytes < 64u * 1024u * 1024u);
    ZrCore_Function_Free(state, function);
}
static void test_user_disable_defers_automatic_collection(void) {
    SZrFunction *function = compile(
        "var i = 0; while (i < 2000) { var o = { value: i }; i = i + 1; } return i;");
    state->global->garbageCollector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    state->global->garbageCollector->stopGcFlag = ZR_TRUE;
    TZrUInt64 before = collections();
    expect_int(function, 2000);
    TEST_ASSERT_EQUAL_UINT64(before, collections());
    state->global->garbageCollector->stopGcFlag = ZR_FALSE;
    expect_int(function, 2000);
    TEST_ASSERT_TRUE(collections() > before);
    ZrCore_Function_Free(state, function);
}
static void test_native_reentry_defers_automatic_collection(void) {
    SZrFunction *function = compile(
        "var i = 0; while (i < 2000) { var o = { value: i }; i = i + 1; } return i;");
    TZrUInt64 before = collections();
    /* ExecuteExpectInt64 adds one native call. Simulate an outer callback. */
    state->nestedNativeCalls = 1u;
    expect_int(function, 2000);
    state->nestedNativeCalls = 0u;
    TEST_ASSERT_EQUAL_UINT64(before, collections());
    expect_int(function, 2000);
    TEST_ASSERT_TRUE(collections() > before);
    ZrCore_Function_Free(state, function);
}
static void test_generational_work_repays_debt_without_new_allocations(void) {
    SZrGarbageCollector *collector = state->global->garbageCollector;
    collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    collector->gcDebtSize = 1;
    ZrCore_GarbageCollector_ScheduleCollection(state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR);
    ZrCore_GarbageCollector_GcStep(state);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_GetDebtSpace(collector) <= 0);
    TZrUInt64 after = collections();
    ZrCore_GarbageCollector_CheckGc(state);
    TEST_ASSERT_EQUAL_UINT64(after, collections());
    collector->gcDebtSize = 1;
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_GetDebtSpace(collector) <= 0);
}
static void test_automatic_major_reclaims_dropped_old_strings(void) {
    SZrGarbageCollector *collector = state->global->garbageCollector;
    collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    TZrSize baseline = live_strings();
    TZrUInt64 before = collector->collectionCounts[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR];
    for (unsigned i = 0u; i < 16000u; ++i) {
        char text[64];
        SZrGcNativeCallPin pin = {0};
        snprintf(text, sizeof(text), "dropped-old-string-%u", i);
        SZrString *string = ZrCore_String_CreateFromNative(state, text);
        /* Native pins promote targets to stable old storage even after unpin. */
        TEST_ASSERT_TRUE(ZrCore_Gc_NativeCallPinObject(state, &string->super, &pin));
        ZrCore_Gc_NativeCallUnpin(state->global, &pin);
    }
    ZrCore_GarbageCollector_CheckGc(state);
    TEST_ASSERT_TRUE(collector->concurrentMajorActive);
    for (unsigned step = 0u; step < 10000u && collector->concurrentMajorActive; ++step)
        ZrCore_GarbageCollector_CheckGc(state);
    TEST_ASSERT_FALSE(collector->concurrentMajorActive);
    TEST_ASSERT_TRUE(collector->collectionCounts[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR] > before);
    TEST_ASSERT_TRUE(live_strings() <= baseline + 1u);
    TEST_ASSERT_TRUE(collector->managedMemories < 1024u * 1024u);
}
static void test_dropped_interpreter_strings_are_collectable(void) {
    SZrFunction *function = compile(
        "var i = 0; while (i < 5000) { var s = \"a-\" + <string> i + \"-b\"; i = i + 1; } return i;");
    expect_int(function, 5000);
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    TEST_ASSERT_TRUE(live_strings() < 1500u);
    TEST_ASSERT_TRUE(state->global->allocationPeakBytes < 64u * 1024u * 1024u);
    ZrCore_Function_Free(state, function);
}
static void test_intern_identity_survives_only_while_referenced(void) {
    SZrString *kept = ZrCore_String_CreateFromNative(state, "intern-identity-live");
    SZrGcRootLease lease = {0};
    TEST_ASSERT_TRUE(ZrCore_Gc_RootLeaseAcquire(state, &kept->super, &lease));
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    TZrSize baseline = live_strings();
    for (int i = 0; i < 12000; i++) {
        char text[64];
        snprintf(text, sizeof(text), "weak-api-cache-%d", i);
        TEST_ASSERT_NOT_NULL(ZrCore_String_CreateTryHitCache(state, text));
    }
    ZrCore_GarbageCollector_ScheduleCollection(state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR);
    ZrCore_GarbageCollector_GcStep(state);
    TEST_ASSERT_EQUAL_PTR(kept, ZrCore_String_CreateFromNative(state, "intern-identity-live"));
    state->global->garbageCollector->gcSweepSliceBudget = 1u;
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    TEST_ASSERT_EQUAL_PTR(kept, ZrCore_String_CreateFromNative(state, "intern-identity-live"));
    TEST_ASSERT_TRUE(live_strings() <= baseline + 1u);
    ZrCore_Gc_RootLeaseRelease(state->global, &lease);
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    TEST_ASSERT_TRUE(live_strings() < baseline);
}
static void test_function_metadata_context_is_a_traced_and_forwarded_edge(void) {
    SZrFunction *function = ZrCore_Function_New(state);
    SZrFunction *context = ZrCore_Function_New(state);
    SZrGcRootLease lease = {0};
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(context);
    context->functionName = ZrCore_String_CreateFromNative(state, "metadata-context");
    function->prototypeContextFunction = context;
    TEST_ASSERT_TRUE(ZrCore_Gc_RootLeaseAcquire(state, &function->super, &lease));
    state->global->garbageCollector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    for (unsigned i = 0u; i < 2u; ++i) {
        ZrCore_GarbageCollector_ScheduleCollection(state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR);
        ZrCore_GarbageCollector_GcStep(state);
    }
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    TEST_ASSERT_EQUAL_INT(ZR_RAW_OBJECT_TYPE_FUNCTION, function->prototypeContextFunction->super.type);
    TEST_ASSERT_EQUAL_PTR(ZrCore_String_CreateFromNative(state, "metadata-context"),
                         function->prototypeContextFunction->functionName);
    ZrCore_Gc_RootLeaseRelease(state->global, &lease);
}
static SZrObject *new_initialized_object(void) {
    SZrObject *object = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);
    return object;
}
static TZrBool heap_contains(const SZrRawObject *wanted) {
    for (SZrRawObject *object = state->global->garbageCollector->gcObjectList;
         object != ZR_NULL; object = object->next)
        if (object == wanted) return ZR_TRUE;
    return ZR_FALSE;
}
static void run_incremental_cycle(void) {
    SZrGarbageCollector *collector = state->global->garbageCollector;
    unsigned steps = 0u;
    ZrCore_GarbageCollector_ScheduleCollection(state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR);
    do {
        TEST_ASSERT_TRUE(++steps < 10000u);
        ZrCore_GarbageCollector_GcStep(state);
    } while (collector->gcRunningStatus != ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED);
}
static void publish_child_graph(SZrObject *owner, SZrTypeValue *key,
                                 SZrObject **child, SZrObject **grandchild) {
    SZrTypeValue value;
    *child = new_initialized_object();
    *grandchild = new_initialized_object();
    ZrCore_Value_InitAsRawObject(state, &value, &(*grandchild)->super);
    ZrCore_Object_SetValue(state, *child, key, &value);
    ZrCore_Value_InitAsRawObject(state, &value, &(*child)->super);
    ZrCore_Object_SetValue(state, owner, key, &value);
}
static void test_idle_barrier_preserves_a_new_transitive_graph(void) {
    SZrObject *owner = new_initialized_object(), *child, *grandchild;
    SZrTypeValue key;
    ZrCore_Value_InitAsInt(state, &key, 0);
    ZrCore_RawObject_MarkAsPermanent(state, &owner->super);
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    publish_child_graph(owner, &key, &child, &grandchild);
    for (unsigned cycle = 0u; cycle < 2u; ++cycle) {
        run_incremental_cycle();
        TEST_ASSERT_TRUE(heap_contains((SZrRawObject *)child));
        TEST_ASSERT_TRUE(heap_contains((SZrRawObject *)grandchild));
    }
}
static void test_sweep_barrier_preserves_graph_allocated_behind_cursor(void) {
    SZrGarbageCollector *collector = state->global->garbageCollector;
    SZrObject *owner = new_initialized_object(), *child, *grandchild;
    SZrGcRootLease ownerLease = {0}, headLease = {0};
    SZrTypeValue key;
    ZrCore_Value_InitAsInt(state, &key, 0);
    TEST_ASSERT_TRUE(ZrCore_Gc_RootLeaseAcquire(state, &owner->super, &ownerLease));
    SZrObject *head = new_initialized_object();
    TEST_ASSERT_TRUE(ZrCore_Gc_RootLeaseAcquire(state, &head->super, &headLease));
    collector->gcPauseBudget = 1u;
    collector->gcSweepSliceBudget = 1u;
    ZrCore_GarbageCollector_ScheduleCollection(state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR);
    unsigned steps = 0u;
    while (collector->gcRunningStatus != ZR_GARBAGE_COLLECT_RUNNING_STATUS_SWEEP_OBJECTS) {
        TEST_ASSERT_TRUE(++steps < 10000u);
        ZrCore_GarbageCollector_GcStep(state);
    }
    ZrCore_GarbageCollector_GcStep(state);
    TEST_ASSERT_TRUE(collector->gcObjectListSweeper != &collector->gcObjectList);
    TEST_ASSERT_TRUE(ZrCore_RawObject_IsMarkReferenced(&owner->super));
    publish_child_graph(owner, &key, &child, &grandchild);
    TEST_ASSERT_EQUAL_INT(ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED,
                          child->super.garbageCollectMark.status);
    while (collector->gcRunningStatus != ZR_GARBAGE_COLLECT_RUNNING_STATUS_PAUSED) {
        TEST_ASSERT_TRUE(++steps < 10000u);
        ZrCore_GarbageCollector_GcStep(state);
    }
    for (unsigned cycle = 0u; cycle < 2u; ++cycle) {
        run_incremental_cycle();
        TEST_ASSERT_TRUE(heap_contains((SZrRawObject *)child));
        TEST_ASSERT_TRUE(heap_contains((SZrRawObject *)grandchild));
    }
    ZrCore_Gc_RootLeaseRelease(state->global, &headLease);
    ZrCore_Gc_RootLeaseRelease(state->global, &ownerLease);
}
static void test_incremental_linked_graph_survives_interpreter_churn(void) {
    SZrFunction *function = compile(
        "var queue = { first: null, last: null, count: 0 }; var i = 0;"
        "while (i < 4096) { var node = { value: i, next: null };"
        " if (queue.last != null) { queue.last.next = node; } else { queue.first = node; }"
        " queue.last = node; queue.count = queue.count + 1; i = i + 1; }"
        "var sum = 0; while (queue.count > 0) { var head = queue.first;"
        " queue.first = head.next; sum = sum + <int> head.value; head.next = null;"
        " queue.count = queue.count - 1; } return sum;");
    TZrUInt64 before = collections();
    expect_int(function, 8386560);
    TEST_ASSERT_TRUE(collections() > before);
    ZrCore_Function_Free(state, function);
}
static SZrString *finalizerKey;
static unsigned finalizerCalls;
static TZrBool finalizerSawValue;
static void inspect_finalizer_graph(SZrState *vm, SZrRawObject *raw) {
    SZrTypeValue key;
    ZrCore_Value_InitAsRawObject(vm, &key, &finalizerKey->super);
    const SZrTypeValue *value = ZrCore_Object_GetValue(vm, (SZrObject *)raw, &key);
    finalizerSawValue = value != ZR_NULL && value->type == ZR_VALUE_TYPE_INT64 &&
                       value->value.nativeObject.nativeInt64 == 42;
    ++finalizerCalls;
}
static void test_finalizers_run_with_their_object_graph_intact(void) {
    for (unsigned minor = 0u; minor < 2u; ++minor) {
        SZrObject *owner = ZrCore_Object_New(state, ZR_NULL);
        SZrTypeValue key, value;
        TEST_ASSERT_NOT_NULL(owner);
        ZrCore_Object_Init(state, owner);
        finalizerKey = ZrCore_String_CreateFromNative(state, "finalizer-graph-key");
        ZrCore_Value_InitAsRawObject(state, &key, &finalizerKey->super);
        ZrCore_Value_InitAsInt(state, &value, 42);
        ZrCore_Object_SetValue(state, owner, &key, &value);
        owner->super.scanMarkGcFunction = inspect_finalizer_graph;
        finalizerCalls = 0u;
        finalizerSawValue = ZR_FALSE;
        if (minor) {
            state->global->garbageCollector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
            ZrCore_GarbageCollector_ScheduleCollection(state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR);
            ZrCore_GarbageCollector_GcStep(state);
        } else {
            ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
        }
        TEST_ASSERT_EQUAL_UINT(1u, finalizerCalls);
        TEST_ASSERT_TRUE(finalizerSawValue);
        ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
        TEST_ASSERT_EQUAL_UINT(1u, finalizerCalls);
    }
}
static void test_automatic_minor_waits_for_nursery_occupancy(void) {
    SZrGarbageCollector *collector = state->global->garbageCollector;
    collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    TZrUInt64 before = collector->collectionCounts[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR];
    collector->gcDebtSize = 1;
    ZrCore_GarbageCollector_GcStep(state);
    TEST_ASSERT_EQUAL_UINT64(before, collector->collectionCounts[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR]);
    TEST_ASSERT_TRUE(collector->nextStepDebtBytes > 128u * 1024u);
    TEST_ASSERT_EQUAL(ZR_GARBAGE_COLLECT_COLLECTION_PHASE_IDLE, collector->collectionPhase);
    /* Explicit host requests bypass occupancy and remain accurately counted. */
    ZrCore_GarbageCollector_ScheduleCollection(state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR);
    ZrCore_GarbageCollector_CheckGc(state);
    TEST_ASSERT_EQUAL_UINT64(before + 1u, collector->collectionCounts[ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR]);
    TEST_ASSERT_NOT_NULL(collector->minorOldBoundary);
}
static void test_gc_coverage_summary_tracks_sparse_and_cross_slot_layouts(void) {
    SZrFunction function = {0};
    SZrFunctionFrameSlotLayout layouts[2] = {{0}};
    SZrTypeValueOnStack stack[6] = {0};
    function.stackSize = 6u;
    function.frameByteSize = sizeof(stack);
    function.frameSlotLayouts = layouts;
    function.frameSlotLayoutLength = 2u;
    layouts[0].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    layouts[0].byteOffset = sizeof(stack[0]) + 8u;
    layouts[0].byteSize = sizeof(SZrTypeValue);
    layouts[1].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    layouts[1].byteOffset = 3u * sizeof(stack[0]) + 32u;
    layouts[1].byteSize = sizeof(stack[0]) + 16u;
    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);
    TEST_ASSERT_TRUE(function.frameGcSummaryValid);
    TEST_ASSERT_NOT_NULL(function.frameGcSlotBitmap);
    for (unsigned i = 0u; i < 6u; ++i)
        TEST_ASSERT_EQUAL(i == 1u || i == 3u || i == 4u,
                ZrCore_Function_FrameStackSlotIntersectsFrameGcValue(&function, stack, stack + i));
    layouts[1].byteOffset = 4u * sizeof(stack[0]);
    ZrCore_Function_FinalizeDirectFrameValueSlots(&function);
    TEST_ASSERT_FALSE(ZrCore_Function_FrameStackSlotIntersectsFrameGcValue(&function, stack, stack + 3u));
    TEST_ASSERT_TRUE(ZrCore_Function_FrameStackSlotIntersectsFrameGcValue(&function, stack, stack + 5u));
    free(function.frameGcSlotBitmap);
}
static void test_deep_frame_scan_has_linear_coverage_work(void) {
    SZrFunction *function = compile(
        "fn rec(n: int): int { if (n == 0) { var i = 0;"
        "while (i < 600) { var o = { value: i }; i = i + 1; } return i; }"
        "return rec(n - 1) + 0; } return rec(80);");
#ifdef ZR_DEBUG
    TZrUInt64 scans = state->gcThreadScanCount;
    TZrUInt64 slots = state->gcThreadScanSlots;
    TZrUInt64 checks = state->gcThreadScanCoverageChecks;
#endif
    expect_int(function, 600);
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
#ifdef ZR_DEBUG
    TEST_ASSERT_TRUE(state->gcThreadScanCount > scans);
    TEST_ASSERT_TRUE(state->gcThreadScanCoverageChecks > checks);
    TEST_ASSERT_TRUE(state->gcThreadScanCoverageChecks - checks <= state->gcThreadScanSlots - slots);
#endif
    ZrCore_Function_Free(state, function);
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_automatic_minor_waits_for_nursery_occupancy);
    RUN_TEST(test_gc_coverage_summary_tracks_sparse_and_cross_slot_layouts);
    RUN_TEST(test_deep_frame_scan_has_linear_coverage_work);
    RUN_TEST(test_idle_barrier_preserves_a_new_transitive_graph);
    RUN_TEST(test_sweep_barrier_preserves_graph_allocated_behind_cursor);
    RUN_TEST(test_incremental_linked_graph_survives_interpreter_churn);
    RUN_TEST(test_finalizers_run_with_their_object_graph_intact);
    RUN_TEST(test_allocation_debt_is_repaid_at_interpreter_boundaries);
    RUN_TEST(test_user_disable_defers_automatic_collection);
    RUN_TEST(test_native_reentry_defers_automatic_collection);
    RUN_TEST(test_generational_work_repays_debt_without_new_allocations);
    RUN_TEST(test_automatic_major_reclaims_dropped_old_strings);
    RUN_TEST(test_dropped_interpreter_strings_are_collectable);
    RUN_TEST(test_intern_identity_survives_only_while_referenced);
    RUN_TEST(test_function_metadata_context_is_a_traced_and_forwarded_edge);
    return UNITY_END();
}
