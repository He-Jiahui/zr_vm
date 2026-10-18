#include <stdio.h>
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
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_finalizers_run_with_their_object_graph_intact);
    RUN_TEST(test_allocation_debt_is_repaid_at_interpreter_boundaries);
    RUN_TEST(test_user_disable_defers_automatic_collection);
    RUN_TEST(test_generational_work_repays_debt_without_new_allocations);
    RUN_TEST(test_dropped_interpreter_strings_are_collectable);
    RUN_TEST(test_intern_identity_survives_only_while_referenced);
    RUN_TEST(test_function_metadata_context_is_a_traced_and_forwarded_edge);
    return UNITY_END();
}
