#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "tests/harness/runtime_support.h"
#include "ssa_runtime_objects_faults.h"
#include "ssa_runtime_objects_concurrency.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser/exec_ir_state_maps.h"

static SZrState *state;
static SZrExecIrFunction function;
static SZrExecIrObjectMaterializationRequest request;
static SZrExecIrMaterializedObjects target;
static SZrExecIrRuntimeTypeBinding binding;
static SZrExecIrRuntimeFieldBinding fields[4];
static SZrTypeValue values[3];
static SZrObject *objects[2];
static SZrExecIrDiagnostic diagnostic;
static TZrUInt32 callbackCount;

static TZrInt64 unexpected_callback(SZrState *callbackState) {
    ZR_UNUSED_PARAMETER(callbackState);
    ++callbackCount;
    return 0;
}

void setUp(void) {
    SZrExecIrInstruction instruction = {0};
    SZrObjectPrototype *prototype;
    TZrUInt32 index;
    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    state->global->garbageCollector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    memset(&request, 0, sizeof(request));
    memset(&target, 0, sizeof(target));
    memset(&binding, 0, sizeof(binding));
    memset(objects, 0, sizeof(objects));
    memset(values, 0, sizeof(values));
    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = 99u;
    function.signatureHash = 123u;
    function.contract.generation = 7u;
    for (index = 0u; index < 3u; ++index) {
        TEST_ASSERT_EQUAL_UINT32(index + 1u, ZrCore_ExecIr_FunctionAddExternalValue(
                &function, 1u, index == 0u ? ZR_EXEC_IR_OWNERSHIP_GC : ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                ZR_EXEC_IR_NULLABILITY_NULLABLE));
    }
    instruction.opcode = ZR_EXEC_IR_OPCODE_NOP;
    instruction.flags = ZR_EXEC_IR_FLAG_MAY_GC;
    instruction.sourceId = 42u;
    instruction.deoptId = 91u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(&function, &instruction, ZR_NULL));
    function.deoptStates = calloc(1u, sizeof(*function.deoptStates));
    function.deoptAggregates = calloc(2u, sizeof(*function.deoptAggregates));
    function.deoptAggregateFields = calloc(7u, sizeof(*function.deoptAggregateFields));
    TEST_ASSERT_NOT_NULL(function.deoptStates);
    TEST_ASSERT_NOT_NULL(function.deoptAggregates);
    TEST_ASSERT_NOT_NULL(function.deoptAggregateFields);
    function.deoptStateCount = function.deoptStateCapacity = 1u;
    function.deoptAggregateCount = function.deoptAggregateCapacity = 2u;
    function.deoptAggregateFieldCount = function.deoptAggregateFieldCapacity = 7u;
    function.deoptStates[0].id = 91u;
    function.deoptStates[0].sourceId = 42u;
    function.deoptStates[0].resumeId = 701u;
    function.deoptStates[0].aggregates.count = 2u;
    for (index = 0u; index < 2u; ++index) {
        function.deoptAggregates[index].identityId = 11u + index;
        function.deoptAggregates[index].typeToken = 101u;
        function.deoptAggregates[index].layoutId = 201u;
    }
    function.deoptAggregates[0].fields.count = 4u;
    function.deoptAggregates[1].fields.start = 4u;
    function.deoptAggregates[1].fields.count = 3u;
    function.deoptAggregateFields[0].kind = ZR_EXEC_IR_DEOPT_FIELD_AGGREGATE;
    function.deoptAggregateFields[0].aggregateId = 12u;
    function.deoptAggregateFields[1] = function.deoptAggregateFields[0];
    function.deoptAggregateFields[1].fieldIndex = 1u;
    function.deoptAggregateFields[2].fieldIndex = 2u;
    function.deoptAggregateFields[3].fieldIndex = 3u;
    function.deoptAggregateFields[3].kind = ZR_EXEC_IR_DEOPT_FIELD_VALUE;
    function.deoptAggregateFields[3].valueId = 3u;
    function.deoptAggregateFields[4].kind = ZR_EXEC_IR_DEOPT_FIELD_AGGREGATE;
    function.deoptAggregateFields[4].aggregateId = 11u;
    function.deoptAggregateFields[5].fieldIndex = 1u;
    function.deoptAggregateFields[5].kind = ZR_EXEC_IR_DEOPT_FIELD_VALUE;
    function.deoptAggregateFields[5].valueId = 1u;
    function.deoptAggregateFields[6].fieldIndex = 2u;
    function.deoptAggregateFields[6].kind = ZR_EXEC_IR_DEOPT_FIELD_VALUE;
    function.deoptAggregateFields[6].valueId = 2u;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));

    prototype = (SZrObjectPrototype *)ZrCore_StructPrototype_New(
            state, ZrCore_String_Create(state, "Restored", 8u));
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype)));
    for (index = 0u; index < 4u; ++index) {
        SZrMemberDescriptor descriptor = {0};
        char name[3] = {'f', (char)('0' + index), 0};
        descriptor.name = ZrCore_String_Create(state, name, 2u);
        descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
        descriptor.isWritable = ZR_FALSE; /* Restoration also handles frozen fields. */
        TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototype, &descriptor));
        fields[index].fieldIndex = index;
        fields[index].memberDescriptorIndex = index;
    }
    binding.typeToken = 101u;
    binding.layoutId = 201u;
    binding.prototype = prototype;
    binding.layoutGeneration = prototype->layoutGeneration;
    binding.fields = fields;
    binding.fieldCount = 4u;
    ZrCore_GarbageCollector_UnignoreObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    ZrCore_Value_InitAsRawObject(state, &values[0], ZR_CAST_RAW_OBJECT_AS_SUPER(ZrCore_Object_New(state, ZR_NULL)));
    ZrCore_Value_InitAsInt(state, &values[1], 73);
    ZrCore_Value_ResetAsNull(&values[2]);
    target.objects = objects;
    target.capacity = 2u;
    request.state = state;
    request.checkpoint.function = &function;
    request.checkpoint.map = function.stateMap;
    request.checkpoint.functionToken = 99u;
    request.checkpoint.generation = 7u;
    request.checkpoint.signatureHash = 123u;
    request.checkpoint.sourceId = 42u;
    request.checkpoint.resumeId = 701u;
    request.checkpoint.phase = ZR_EXEC_IR_STATE_AFTER_EFFECT;
    request.values = values;
    request.valueCount = 3u;
    request.types = &binding;
    request.typeCount = 1u;
    request.target = &target;
    ssa_runtime_objects_faults(0u, ZR_FALSE, ZR_FALSE, 0u);
}

void tearDown(void) {
    if (target.objects != objects) free(target.objects);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrTests_Runtime_State_Destroy(state);
    state = ZR_NULL;
}

static SZrHashKeyValuePair *field(TZrUInt32 objectIndex, TZrUInt32 descriptorIndex) {
    SZrTypeValue key;
    ZrCore_Value_InitAsRawObject(state, &key,
            ZR_CAST_RAW_OBJECT_AS_SUPER(binding.prototype->memberDescriptors[descriptorIndex].name));
    return ZrCore_HashSet_Find(state, &objects[objectIndex]->nodeMap, &key);
}

static void assert_graph(void) {
    TEST_ASSERT_EQUAL_UINT32(2u, target.count);
    TEST_ASSERT_NOT_EQUAL(objects[0], objects[1]);
    TEST_ASSERT_EQUAL_PTR(binding.prototype, objects[0]->prototype);
    TEST_ASSERT_EQUAL(ZR_OBJECT_INTERNAL_TYPE_STRUCT, objects[0]->internalType);
    TEST_ASSERT_NOT_NULL(field(0u, 0u));
    TEST_ASSERT_NOT_NULL(field(0u, 1u));
    TEST_ASSERT_NOT_NULL(field(1u, 0u));
    TEST_ASSERT_EQUAL_PTR(objects[1], field(0u, 0u)->value.value.object);
    TEST_ASSERT_EQUAL_PTR(objects[1], field(0u, 1u)->value.value.object);
    TEST_ASSERT_EQUAL_PTR(objects[0], field(1u, 0u)->value.value.object);
    TEST_ASSERT_NULL(field(0u, 2u));
    TEST_ASSERT_NOT_NULL(field(0u, 3u));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(field(0u, 3u)->value.type));
    TEST_ASSERT_EQUAL_PTR(values[0].value.object, field(1u, 1u)->value.value.object);
    TEST_ASSERT_EQUAL_INT64(73, field(1u, 2u)->value.value.nativeObject.nativeInt64);
}

static void assert_failure_location(EZrExecutionDiagnosticCode code) {
    TEST_ASSERT_EQUAL(code, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(99u, diagnostic.functionToken);
    TEST_ASSERT_EQUAL_UINT32(42u, diagnostic.sourceId);
    TEST_ASSERT_EQUAL_UINT32(1u, diagnostic.instructionId);
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Gc_AotRootFrameDepth(state));
}

static void test_real_struct_graph_preserves_aliases_cycle_and_initialization(void) {
    TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeObjects(&request, &diagnostic));
    assert_graph();
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Gc_AotRootFrameDepth(state));
}

static void test_gc_during_every_object_allocation_preserves_sources_and_graph(void) {
    TZrUInt64 collections = state->global->garbageCollector->statsSnapshot.fullCollectionCount;
    ssa_runtime_objects_faults(0u, ZR_FALSE, ZR_TRUE, 0u);
    TEST_ASSERT_TRUE(ssa_runtime_objects_materialize(&request, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(2u, ssa_runtime_objects_allocation_count());
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(collections + 2u,
            state->global->garbageCollector->statsSnapshot.fullCollectionCount);
    assert_graph();
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Gc_AotRootFrameDepth(state));
}

static void test_nth_object_failure_preserves_prior_target_and_balances_roots(void) {
    TZrUInt32 index;
    SZrObject *original[2];
    TZrSize ignored = state->global->garbageCollector->ignoredObjectCount;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeObjects(&request, &diagnostic));
    memcpy(original, objects, sizeof(original));
    for (index = 1u; index <= 2u; ++index) {
        ssa_runtime_objects_faults(index, ZR_FALSE, ZR_FALSE, 0u);
        TEST_ASSERT_FALSE(ssa_runtime_objects_materialize(&request, &diagnostic));
        assert_failure_location(ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY);
        TEST_ASSERT_EQUAL_MEMORY(original, objects, sizeof(original));
        TEST_ASSERT_EQUAL_UINT64(ignored, state->global->garbageCollector->ignoredObjectCount);
        assert_graph();
    }
}

static void test_throwing_oom_restores_ambient_exception_and_allows_retry(void) {
    ssa_runtime_objects_faults(2u, ZR_TRUE, ZR_FALSE, 0u);
    ZrCore_Value_InitAsInt(state, &state->currentException, 912);
    state->hasCurrentException = ZR_TRUE;
    state->currentExceptionStatus = ZR_THREAD_STATUS_EXCEPTION_ERROR;
    TEST_ASSERT_FALSE(ssa_runtime_objects_materialize(&request, &diagnostic));
    assert_failure_location(ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY);
    TEST_ASSERT_EQUAL(ZR_THREAD_STATUS_FINE, state->threadStatus);
    TEST_ASSERT_TRUE(state->hasCurrentException);
    TEST_ASSERT_EQUAL(ZR_THREAD_STATUS_EXCEPTION_ERROR, state->currentExceptionStatus);
    TEST_ASSERT_EQUAL_INT64(912, state->currentException.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(0u, target.count);
    ssa_runtime_objects_faults(0u, ZR_FALSE, ZR_FALSE, 0u);
    TEST_ASSERT_TRUE(ssa_runtime_objects_materialize(&request, &diagnostic));
    assert_graph();
}

static void test_detached_field_reservation_failure_leaves_target_unchanged(void) {
    TZrUInt32 index;
    for (index = 1u; index <= 2u; ++index) {
        ssa_runtime_objects_faults(0u, ZR_FALSE, ZR_FALSE, index);
        TEST_ASSERT_FALSE(ssa_runtime_objects_materialize(&request, &diagnostic));
        assert_failure_location(ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY);
        TEST_ASSERT_EQUAL_UINT32(0u, target.count);
        TEST_ASSERT_NULL(objects[0]);
        TEST_ASSERT_NULL(objects[1]);
    }
}

static void test_remembered_reservation_failure_preserves_existing_graph(void) {
    SZrObject *original[2];
    TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeObjects(&request, &diagnostic));
    memcpy(original, objects, sizeof(original));
    ssa_runtime_objects_fail_remembered_reservation();
    TEST_ASSERT_FALSE(ssa_runtime_objects_materialize(&request, &diagnostic));
    assert_failure_location(ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY);
    TEST_ASSERT_EQUAL_MEMORY(original, objects, sizeof(original));
    assert_graph();
}

static void test_gc_preserves_ambient_object_exception_on_oom(void) {
    state->currentException = values[0];
    state->hasCurrentException = ZR_TRUE;
    state->currentExceptionStatus = ZR_THREAD_STATUS_EXCEPTION_ERROR;
    state->threadStatus = ZR_THREAD_STATUS_EXCEPTION_ERROR;
    ssa_runtime_objects_faults(2u, ZR_TRUE, ZR_TRUE, 0u);
    TEST_ASSERT_FALSE(ssa_runtime_objects_materialize(&request, &diagnostic));
    assert_failure_location(ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY);
    TEST_ASSERT_EQUAL(ZR_THREAD_STATUS_EXCEPTION_ERROR, state->threadStatus);
    TEST_ASSERT_TRUE(state->hasCurrentException);
    TEST_ASSERT_EQUAL(ZR_THREAD_STATUS_EXCEPTION_ERROR, state->currentExceptionStatus);
    TEST_ASSERT_EQUAL_PTR(values[0].value.object, state->currentException.value.object);
    TEST_ASSERT_EQUAL_UINT32(0u, target.count);
    state->threadStatus = ZR_THREAD_STATUS_FINE;
}

static void test_caught_oom_preserves_nested_mutator_and_native_scopes(void) {
    SZrGcDomainMutatorSnapshot snapshot;
    TEST_ASSERT_TRUE(ZrCore_GcDomain_MutatorEnter(state));
    TEST_ASSERT_TRUE(ZrCore_GcDomain_MutatorEnter(state));
    TEST_ASSERT_TRUE(ZrCore_GcDomain_NativeEnter(state, ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE));
    TEST_ASSERT_TRUE(ZrCore_GcDomain_NativeEnter(state, ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE));
    ssa_runtime_objects_faults(2u, ZR_TRUE, ZR_TRUE, 0u);
    TEST_ASSERT_FALSE(ssa_runtime_objects_materialize(&request, &diagnostic));
    assert_failure_location(ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY);
    ZrCore_GcDomain_GetMutatorSnapshot(state, &snapshot);
    TEST_ASSERT_EQUAL_UINT32(1u, snapshot.runningMutatorCount);
    ZrCore_GcDomain_MutatorLeave(state);
    ZrCore_GcDomain_MutatorLeave(state);
    ZrCore_GcDomain_GetMutatorSnapshot(state, &snapshot);
    TEST_ASSERT_EQUAL_UINT32(1u, snapshot.runningMutatorCount);
    ZrCore_GcDomain_NativeLeave(state);
    ZrCore_GcDomain_GetMutatorSnapshot(state, &snapshot);
    TEST_ASSERT_EQUAL_UINT32(1u, snapshot.runningMutatorCount);
    ZrCore_GcDomain_NativeLeave(state);
    ZrCore_GcDomain_GetMutatorSnapshot(state, &snapshot);
    TEST_ASSERT_EQUAL_UINT32(0u, snapshot.runningMutatorCount);
}

static void test_caught_oom_preserves_native_only_scopes(void) {
    SZrGcDomainMutatorSnapshot snapshot;
    TEST_ASSERT_TRUE(ZrCore_GcDomain_NativeEnter(state, ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE));
    TEST_ASSERT_TRUE(ZrCore_GcDomain_NativeEnter(state, ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE));
    ssa_runtime_objects_faults(2u, ZR_TRUE, ZR_TRUE, 0u);
    TEST_ASSERT_FALSE(ssa_runtime_objects_materialize(&request, &diagnostic));
    assert_failure_location(ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY);
    ZrCore_GcDomain_GetMutatorSnapshot(state, &snapshot);
    TEST_ASSERT_EQUAL_UINT32(1u, snapshot.runningMutatorCount);
    ZrCore_GcDomain_NativeLeave(state);
    ZrCore_GcDomain_GetMutatorSnapshot(state, &snapshot);
    TEST_ASSERT_EQUAL_UINT32(1u, snapshot.runningMutatorCount);
    ZrCore_GcDomain_NativeLeave(state);
    ZrCore_GcDomain_GetMutatorSnapshot(state, &snapshot);
    TEST_ASSERT_EQUAL_UINT32(0u, snapshot.runningMutatorCount);
}

static void test_unsafe_native_modes_reject_without_heap_or_scope_changes(void) {
    const EZrGcNativeSafepointMode modes[] = {ZR_GC_NATIVE_SAFEPOINT_MODE_BLOCKING_DETACHED,
                                            ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL};
    size_t index;
    for (index = 0u; index < sizeof(modes) / sizeof(modes[0]); ++index) {
        SZrGcDomainMutatorSnapshot before, after;
        TEST_ASSERT_TRUE(ZrCore_GcDomain_NativeEnter(state, modes[index]));
        ZrCore_GcDomain_GetMutatorSnapshot(state, &before);
        TEST_ASSERT_FALSE(ssa_runtime_objects_materialize(&request, &diagnostic));
        assert_failure_location(ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED);
        TEST_ASSERT_EQUAL_UINT32(0u, ssa_runtime_objects_allocation_count());
        ZrCore_GcDomain_GetMutatorSnapshot(state, &after);
        TEST_ASSERT_EQUAL_UINT32(before.blockingDetachedMutatorCount, after.blockingDetachedMutatorCount);
        TEST_ASSERT_EQUAL_UINT32(before.noSafepointCriticalMutatorCount, after.noSafepointCriticalMutatorCount);
        ZrCore_GcDomain_NativeLeave(state);
        ZrCore_GcDomain_GetMutatorSnapshot(state, &after);
        TEST_ASSERT_EQUAL_UINT32(0u, after.runningMutatorCount);
    }
}

static void test_stale_layout_fails_before_allocating_objects(void) {
    ++binding.layoutGeneration;
    TEST_ASSERT_FALSE(ssa_runtime_objects_materialize(&request, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(0u, ssa_runtime_objects_allocation_count());
    assert_failure_location(ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED);
}

static void observe_attachment_pause(SZrState *callbackState, void *context) {
    SZrGcDomainMutatorSnapshot snapshot;
    ZrCore_GcDomain_GetMutatorSnapshot(callbackState, &snapshot);
    *(TZrBool *)context = snapshot.pauseRequested;
}

static void test_attachment_holds_and_releases_collection_pause(void) {
    TZrBool held = ZR_FALSE;
    SZrGcDomainMutatorSnapshot snapshot;
    ssa_runtime_objects_set_barrier_hook(observe_attachment_pause, &held);
    TEST_ASSERT_TRUE(ssa_runtime_objects_materialize(&request, &diagnostic));
    TEST_ASSERT_TRUE(held);
    ZrCore_GcDomain_GetMutatorSnapshot(state, &snapshot);
    TEST_ASSERT_FALSE(snapshot.pauseRequested);
    assert_graph();
}

static void test_failed_attachment_pause_preserves_old_graph(void) {
    SZrObject *original[2];
    SZrGcDomainMutatorSnapshot snapshot;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeObjects(&request, &diagnostic));
    memcpy(original, objects, sizeof(objects));
    ssa_runtime_objects_fail_pause();
    TEST_ASSERT_FALSE(ssa_runtime_objects_materialize(&request, &diagnostic));
    assert_failure_location(ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED);
    TEST_ASSERT_EQUAL_MEMORY(original, objects, sizeof(objects));
    ZrCore_GcDomain_GetMutatorSnapshot(state, &snapshot);
    TEST_ASSERT_FALSE(snapshot.pauseRequested);
    TEST_ASSERT_EQUAL_UINT32(0u, snapshot.runningMutatorCount);
    assert_graph();
}

static void throw_during_attachment(SZrState *callbackState, void *context) {
    observe_attachment_pause(callbackState, context);
    ZrCore_Exception_Throw(callbackState, ZR_THREAD_STATUS_MEMORY_ERROR);
}

static void test_attachment_exception_releases_pause_and_preserves_old_graph(void) {
    SZrObject *original[2];
    SZrGcDomainMutatorSnapshot snapshot;
    TZrBool held = ZR_FALSE;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeObjects(&request, &diagnostic));
    memcpy(original, objects, sizeof(objects));
    ssa_runtime_objects_set_barrier_hook(throw_during_attachment, &held);
    TEST_ASSERT_FALSE(ssa_runtime_objects_materialize(&request, &diagnostic));
    TEST_ASSERT_TRUE(held);
    assert_failure_location(ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY);
    TEST_ASSERT_EQUAL_MEMORY(original, objects, sizeof(objects));
    ZrCore_GcDomain_GetMutatorSnapshot(state, &snapshot);
    TEST_ASSERT_FALSE(snapshot.pauseRequested);
    TEST_ASSERT_EQUAL_UINT32(0u, snapshot.runningMutatorCount);
    assert_graph();
}

static void test_existing_pause_survives_nested_materialization(void) {
    SZrGcDomainMutatorSnapshot snapshot;
    TEST_ASSERT_TRUE(ZrCore_GcDomain_StopTheWorldBegin(state, 1000u, ZR_NULL));
    TEST_ASSERT_TRUE(ssa_runtime_objects_materialize(&request, &diagnostic));
    ZrCore_GcDomain_GetMutatorSnapshot(state, &snapshot);
    TEST_ASSERT_TRUE(snapshot.pauseRequested);
    ZrCore_GcDomain_StopTheWorldEnd(state);
    ZrCore_GcDomain_GetMutatorSnapshot(state, &snapshot);
    TEST_ASSERT_FALSE(snapshot.pauseRequested);
    assert_graph();
}

static void test_competing_collector_during_field_attachment_preserves_graph(void) {
    TZrUInt64 collections = state->global->garbageCollector->statsSnapshot.fullCollectionCount;
    TEST_ASSERT_TRUE(ssa_runtime_objects_competing_collector(&request, &diagnostic));
    TEST_ASSERT_GREATER_THAN_UINT64(collections,
            state->global->garbageCollector->statsSnapshot.fullCollectionCount);
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Gc_AotRootFrameDepth(state));
    assert_graph();
}

static void test_duplicate_field_destination_fails_before_allocating_objects(void) {
    fields[1].memberDescriptorIndex = fields[0].memberDescriptorIndex;
    TEST_ASSERT_FALSE(ssa_runtime_objects_materialize(&request, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(0u, ssa_runtime_objects_allocation_count());
    assert_failure_location(ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED);
}

static void test_empty_graph_clears_published_count_without_allocating(void) {
    TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeObjects(&request, &diagnostic));
    function.deoptStates[0].aggregates.count = 0u;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    request.checkpoint.map = function.stateMap;
    TEST_ASSERT_TRUE(ssa_runtime_objects_materialize(&request, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(0u, target.count);
    TEST_ASSERT_EQUAL_UINT32(0u, ssa_runtime_objects_allocation_count());
}

static void test_gc_on_failure_keeps_old_graph_and_source_values(void) {
    TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeObjects(&request, &diagnostic));
    ZrCore_Value_InitAsInt(state, &values[1], 99);
    ssa_runtime_objects_faults(2u, ZR_TRUE, ZR_TRUE, 0u);
    TEST_ASSERT_FALSE(ssa_runtime_objects_materialize(&request, &diagnostic));
    assert_failure_location(ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY);
    assert_graph(); /* The old object's scalar remains 73, not the new 99. */
    TEST_ASSERT_EQUAL_INT64(99, values[1].value.nativeObject.nativeInt64);
}

static void test_published_graph_survives_gc_from_caller_roots(void) {
    SZrAotGcRootSlot slots[2] = {{0}};
    SZrAotGcRootMap map = {2u, slots};
    SZrAotGcRootFrame frame;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeObjects(&request, &diagnostic));
    slots[0].locationKind = ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS;
    slots[1].locationKind = ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS;
    slots[1].frameByteOffset = sizeof(objects[0]);
    TEST_ASSERT_TRUE(ZrCore_Gc_AotRootFramePush(state, &frame,
                    (TZrStackValuePointer)(void *)objects, &map));
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    binding.prototype = objects[0]->prototype;
    values[0].value.object = field(1u, 1u)->value.value.object;
    assert_graph();
    TEST_ASSERT_TRUE(ZrCore_Gc_AotRootFramePop(state, &frame));
}

static void test_non_field_and_missing_binding_are_rejected_before_heap_work(void) {
    binding.prototype->memberDescriptors[0].kind = ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY;
    TEST_ASSERT_FALSE(ssa_runtime_objects_materialize(&request, &diagnostic));
    assert_failure_location(ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED);
    TEST_ASSERT_EQUAL_UINT32(0u, ssa_runtime_objects_allocation_count());
    binding.prototype->memberDescriptors[0].kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
    binding.typeToken = 102u;
    TEST_ASSERT_FALSE(ssa_runtime_objects_materialize(&request, &diagnostic));
    assert_failure_location(ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED);
    TEST_ASSERT_EQUAL_UINT32(0u, ssa_runtime_objects_allocation_count());
}

static void test_insufficient_target_capacity_preserves_existing_graph(void) {
    TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeObjects(&request, &diagnostic));
    target.capacity = 1u;
    TEST_ASSERT_FALSE(ssa_runtime_objects_materialize(&request, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(0u, ssa_runtime_objects_allocation_count());
    target.capacity = 2u;
    assert_graph();
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Gc_AotRootFrameDepth(state));
}

static void test_large_cycle_and_repeated_preparation(void) {
    const TZrUInt32 count = 257u;
    TZrUInt32 index, iteration;
    SZrObject **largeObjects = calloc(count, sizeof(*largeObjects));
    TEST_ASSERT_NOT_NULL(largeObjects);
    target.objects = largeObjects;
    target.capacity = count;
    free(function.deoptAggregates);
    free(function.deoptAggregateFields);
    function.deoptAggregates = calloc(count, sizeof(*function.deoptAggregates));
    function.deoptAggregateFields = calloc(count * 4u, sizeof(*function.deoptAggregateFields));
    TEST_ASSERT_NOT_NULL(function.deoptAggregates);
    TEST_ASSERT_NOT_NULL(function.deoptAggregateFields);
    function.deoptAggregateCount = function.deoptAggregateCapacity = count;
    function.deoptAggregateFieldCount = function.deoptAggregateFieldCapacity = count * 4u;
    function.deoptStates[0].aggregates.count = count;
    for (index = 0u; index < count; ++index) {
        SZrExecIrDeoptAggregate *aggregate = &function.deoptAggregates[index];
        SZrExecIrDeoptAggregateField *recipe = &function.deoptAggregateFields[index * 4u];
        TZrUInt32 value;
        aggregate->identityId = index + 1u;
        aggregate->typeToken = 101u;
        aggregate->layoutId = 201u;
        aggregate->fields.start = index * 4u;
        aggregate->fields.count = 4u;
        recipe[0].kind = ZR_EXEC_IR_DEOPT_FIELD_AGGREGATE;
        recipe[0].aggregateId = (index + 1u) % count + 1u;
        for (value = 1u; value <= 3u; ++value) {
            recipe[value].kind = ZR_EXEC_IR_DEOPT_FIELD_VALUE;
            recipe[value].fieldIndex = value;
            recipe[value].valueId = value;
        }
    }
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    request.checkpoint.map = function.stateMap;
    for (iteration = 0u; iteration < 3u; ++iteration) {
        TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeObjects(&request, &diagnostic));
        TEST_ASSERT_EQUAL_UINT32(count, target.count);
        for (index = 0u; index < count; ++index) {
            SZrTypeValue key;
            SZrHashKeyValuePair *pair;
            ZrCore_Value_InitAsRawObject(state, &key,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(binding.prototype->memberDescriptors[0].name));
            pair = ZrCore_HashSet_Find(state, &largeObjects[index]->nodeMap, &key);
            TEST_ASSERT_NOT_NULL(pair);
            TEST_ASSERT_EQUAL_PTR(largeObjects[(index + 1u) % count], pair->value.value.object);
        }
        TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Gc_AotRootFrameDepth(state));
    }
    free(largeObjects);
    target.objects = objects;
    target.count = 0u;
    target.capacity = 2u;
}

static void test_each_native_preparation_allocation_failure_preserves_target(void) {
    size_t ordinal;
    TZrBool reachedSuccess = ZR_FALSE;
    SZrObject *original[2];
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(state, values[0].value.object));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeObjects(&request, &diagnostic));
    memcpy(original, objects, sizeof(original));
    for (ordinal = 1u; ordinal <= 64u; ++ordinal) {
        TZrBool ok;
        ssa_runtime_objects_faults(0u, ZR_FALSE, ZR_FALSE, 0u);
        ssa_runtime_objects_fail_native_allocation(ordinal);
        ok = ssa_runtime_objects_materialize(&request, &diagnostic);
        if (!ssa_runtime_objects_native_allocation_failed()) {
            TEST_ASSERT_TRUE(ok);
            reachedSuccess = ZR_TRUE;
            break;
        }
        TEST_ASSERT_FALSE(ok);
        assert_failure_location(ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY);
        TEST_ASSERT_EQUAL_MEMORY(original, objects, sizeof(original));
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, values[0].value.object));
    }
    TEST_ASSERT_TRUE(reachedSuccess);
    TEST_ASSERT_GREATER_THAN_UINT32(1u, ordinal);
    assert_graph();
}

static void test_owner_transfer_is_rejected_without_changing_the_source(void) {
    SZrTypeValue original;
    TZrBool ok;
    values[0].ownershipKind = ZR_OWNERSHIP_VALUE_KIND_UNIQUE;
    original = values[0];
    ok = ssa_runtime_objects_materialize(&request, &diagnostic);
    TEST_ASSERT_EQUAL_MEMORY(&original, &values[0], sizeof(original));
    values[0].ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
    TEST_ASSERT_FALSE(ok);
    assert_failure_location(ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED);
    TEST_ASSERT_EQUAL_UINT32(0u, ssa_runtime_objects_allocation_count());
}

static void test_restoration_does_not_invoke_constructor_or_field_accessors(void) {
    SZrClosureNative *closure;
    SZrFunction *callable;
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(state,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(binding.prototype)));
    closure = ZrCore_ClosureNative_New(state, 0u);
    TEST_ASSERT_NOT_NULL(closure);
    closure->nativeFunction = unexpected_callback;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    callable = ZR_CAST(SZrFunction *, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    ZrCore_ObjectPrototype_AddMeta(state, binding.prototype, ZR_META_CONSTRUCTOR, callable);
    binding.prototype->memberDescriptors[0].initializerFunction = callable;
    binding.prototype->memberDescriptors[0].getterFunction = callable;
    binding.prototype->memberDescriptors[0].setterFunction = callable;
    binding.layoutGeneration = binding.prototype->layoutGeneration;
    ZrCore_GarbageCollector_UnignoreObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(binding.prototype));
    callbackCount = 0u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeObjects(&request, &diagnostic));
    assert_graph();
    TEST_ASSERT_EQUAL_UINT32(0u, callbackCount);
}

static void test_resource_prototype_rejects_before_object_allocation(void) {
    binding.prototype->modifierFlags |= ZR_TYPE_MODIFIER_FLAG_RESOURCE;
    TEST_ASSERT_FALSE(ssa_runtime_objects_materialize(&request, &diagnostic));
    assert_failure_location(ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED);
    TEST_ASSERT_EQUAL_UINT32(0u, ssa_runtime_objects_allocation_count());
}

static void test_resource_source_without_owner_handle_is_rejected(void) {
    SZrObjectPrototype *resourcePrototype = ZrCore_ObjectPrototype_New(
            state, binding.prototype->name, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(resourcePrototype);
    resourcePrototype->modifierFlags |= ZR_TYPE_MODIFIER_FLAG_RESOURCE;
    ZrCore_Value_InitAsRawObject(state, &values[0], ZR_CAST_RAW_OBJECT_AS_SUPER(
                    ZrCore_Object_New(state, resourcePrototype)));
    TEST_ASSERT_EQUAL(ZR_OWNERSHIP_VALUE_KIND_NONE, values[0].ownershipKind);
    TEST_ASSERT_EQUAL(ZR_RESOURCE_LIFECYCLE_NONE, values[0].value.object->resourceLifecycleState);
    TEST_ASSERT_FALSE(ssa_runtime_objects_materialize(&request, &diagnostic));
    assert_failure_location(ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED);
    TEST_ASSERT_EQUAL_UINT32(0u, ssa_runtime_objects_allocation_count());
}

static void test_output_storage_cannot_overlap_source_value_payloads(void) {
    SZrTypeValue original[3];
    TZrBool ok;
    memcpy(original, values, sizeof(values));
    target.objects = (SZrObject **)(void *)&values[0].value.object;
    ok = ssa_runtime_objects_materialize(&request, &diagnostic);
    target.objects = objects;
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_MEMORY(original, values, sizeof(values));
    TEST_ASSERT_EQUAL_UINT32(0u, ssa_runtime_objects_allocation_count());
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Gc_AotRootFrameDepth(state));
}

static void test_existing_ignore_registrations_survive_success_and_failure(void) {
    TZrSize ignored;
    SZrRawObject *prototype = ZR_CAST_RAW_OBJECT_AS_SUPER(binding.prototype);
    SZrRawObject *name = ZR_CAST_RAW_OBJECT_AS_SUPER(binding.prototype->memberDescriptors[0].name);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(state, prototype));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(state, name));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(state, values[0].value.object));
    ignored = state->global->garbageCollector->ignoredObjectCount;
    TEST_ASSERT_TRUE(ssa_runtime_objects_materialize(&request, &diagnostic));
    TEST_ASSERT_EQUAL_UINT64(ignored, state->global->garbageCollector->ignoredObjectCount);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, prototype));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, name));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, values[0].value.object));
    ssa_runtime_objects_faults(2u, ZR_TRUE, ZR_TRUE, 0u);
    TEST_ASSERT_FALSE(ssa_runtime_objects_materialize(&request, &diagnostic));
    assert_failure_location(ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY);
    TEST_ASSERT_EQUAL_UINT64(ignored, state->global->garbageCollector->ignoredObjectCount);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(binding.prototype)));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(binding.prototype->memberDescriptors[0].name)));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(state->global, values[0].value.object));
    assert_graph();
}

static void test_output_storage_cannot_overlap_ir_or_prototype_metadata(void) {
    void *destinations[] = {function.deoptAggregateFields, function.stateMap->valuePool,
                           binding.prototype, binding.prototype->memberDescriptors};
    size_t index;
    for (index = 0u; index < sizeof(destinations) / sizeof(destinations[0]); ++index) {
        TZrBool ok;
        unsigned char original[sizeof(objects)];
        memcpy(original, destinations[index], sizeof(original));
        target.objects = destinations[index];
        ok = ssa_runtime_objects_materialize(&request, &diagnostic);
        target.objects = objects;
        TEST_ASSERT_FALSE(ok);
        TEST_ASSERT_EQUAL_MEMORY(original, destinations[index], sizeof(original));
        TEST_ASSERT_EQUAL_UINT32(0u, ssa_runtime_objects_allocation_count());
        TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Gc_AotRootFrameDepth(state));
    }
}

static void test_reference_class_graph_preserves_identity(void) {
    TZrUInt32 index;
    SZrObjectPrototype *classPrototype = ZrCore_ObjectPrototype_New(
            state, binding.prototype->name, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(classPrototype);
    for (index = 0u; index < 4u; ++index) {
        TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, classPrototype,
                        &binding.prototype->memberDescriptors[index]));
    }
    binding.prototype = classPrototype;
    binding.layoutGeneration = classPrototype->layoutGeneration;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeObjects(&request, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(2u, target.count);
    TEST_ASSERT_EQUAL(ZR_OBJECT_INTERNAL_TYPE_OBJECT, objects[0]->internalType);
    TEST_ASSERT_EQUAL_PTR(objects[1], field(0u, 0u)->value.value.object);
    TEST_ASSERT_EQUAL_PTR(objects[1], field(0u, 1u)->value.value.object);
    TEST_ASSERT_EQUAL_PTR(objects[0], field(1u, 0u)->value.value.object);
}

static void test_inactive_cleanup_payload_is_neither_read_nor_rooted(void) {
    SZrExecIrInstruction checkpointInstruction = function.instructions[0];
    SZrExecIrInstruction instruction = {0};
    SZrTypeValue extended[4];
    TZrUInt32 owners[4] = {ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED,
            ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN, ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN,
            ZR_EXEC_IR_STATE_MAP_OWNER_DROPPED};
    TZrExecIrValueId owner, answer = 2u;
    TZrExecIrMemoryTokenId token = 1u, next = 2u;
    owner = ZrCore_ExecIr_FunctionAddExternalValue(&function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_EXEC_IR_NULLABILITY_NULLABLE);
    TEST_ASSERT_EQUAL_UINT32(4u, owner);
    TEST_ASSERT_EQUAL_UINT32(1u, ZrCore_ExecIr_FunctionAddBlock(&function,
            ZR_EXEC_IR_BLOCK_FLAG_ENTRY | ZR_EXEC_IR_BLOCK_FLAG_CLEANUP));
    function.instructionCount = 0u;
    instruction.opcode = ZR_EXEC_IR_OPCODE_DROP;
    instruction.sourceId = 41u;
    instruction.effectIn = token;
    instruction.effectOut = next;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendOperands(&function, &owner, 1u,
            &instruction.operandRange));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendMemoryTokens(&function, &token, 1u,
            &instruction.memoryIn));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendMemoryTokens(&function, &next, 1u,
            &instruction.memoryOut));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(&function, &instruction, NULL));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(&function, &checkpointInstruction, NULL));
    instruction.opcode = ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED;
    instruction.sourceId = 43u;
    token = 2u;
    next = 3u;
    instruction.effectIn = token;
    instruction.effectOut = next;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendMemoryTokens(&function, &token, 1u,
            &instruction.memoryIn));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendMemoryTokens(&function, &next, 1u,
            &instruction.memoryOut));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(&function, &instruction, NULL));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instruction.sourceId = 44u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendOperands(&function, &answer, 1u,
            &instruction.operandRange));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(&function, &instruction, NULL));
    function.blocks[0].instructionRange.count = 4u;
    function.blocks[0].terminatorInstructionId = 4u;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    request.checkpoint.map = function.stateMap;
    request.checkpoint.ownerStates = owners;
    request.checkpoint.ownerStateCount = 4u;
    memcpy(extended, values, sizeof(values));
    /* A consumed slot may contain arbitrary bytes. Both payload validation
     * and GC root registration must first consult the concrete owner state. */
    memset(&extended[3], 0xff, sizeof(extended[3]));
    request.values = extended;
    request.valueCount = 4u;
    ssa_runtime_objects_faults(0u, ZR_FALSE, ZR_TRUE, 0u);
    TEST_ASSERT_TRUE(ssa_runtime_objects_materialize(&request, &diagnostic));
    memcpy(values, extended, sizeof(values));
    assert_graph();
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_STATE_MAP_OWNER_DROPPED, owners[3]);
    TEST_ASSERT_EQUAL_UINT32(2u, ssa_runtime_objects_allocation_count());
}

static void test_output_storage_cannot_overwrite_concrete_owner_witness(void) {
    union {
        TZrUInt32 owners[4];
        SZrObject *alignment[2];
    } witness = {{ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED,
                  ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN,
                  ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN, 0u}};
    TZrUInt32 original[4];
    TZrBool ok;
    memcpy(original, witness.owners, sizeof(original));
    request.checkpoint.ownerStates = witness.owners;
    request.checkpoint.ownerStateCount = 3u;
    target.objects = (SZrObject **)(void *)witness.owners;
    ok = ZrCore_ExecIr_MaterializeObjects(&request, &diagnostic);
    /* The test owns the witness on the stack, never the normal heap target. */
    target.objects = objects;
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL(ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(0u, target.count);
    TEST_ASSERT_EQUAL_MEMORY(original, witness.owners, sizeof(original));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_real_struct_graph_preserves_aliases_cycle_and_initialization);
    RUN_TEST(test_gc_during_every_object_allocation_preserves_sources_and_graph);
    RUN_TEST(test_nth_object_failure_preserves_prior_target_and_balances_roots);
    RUN_TEST(test_throwing_oom_restores_ambient_exception_and_allows_retry);
    RUN_TEST(test_detached_field_reservation_failure_leaves_target_unchanged);
    RUN_TEST(test_remembered_reservation_failure_preserves_existing_graph);
    RUN_TEST(test_gc_preserves_ambient_object_exception_on_oom);
    RUN_TEST(test_caught_oom_preserves_nested_mutator_and_native_scopes);
    RUN_TEST(test_caught_oom_preserves_native_only_scopes);
    RUN_TEST(test_unsafe_native_modes_reject_without_heap_or_scope_changes);
    RUN_TEST(test_stale_layout_fails_before_allocating_objects);
    RUN_TEST(test_attachment_holds_and_releases_collection_pause);
    RUN_TEST(test_failed_attachment_pause_preserves_old_graph);
    RUN_TEST(test_attachment_exception_releases_pause_and_preserves_old_graph);
    RUN_TEST(test_existing_pause_survives_nested_materialization);
    RUN_TEST(test_competing_collector_during_field_attachment_preserves_graph);
    RUN_TEST(test_duplicate_field_destination_fails_before_allocating_objects);
    RUN_TEST(test_empty_graph_clears_published_count_without_allocating);
    RUN_TEST(test_gc_on_failure_keeps_old_graph_and_source_values);
    RUN_TEST(test_published_graph_survives_gc_from_caller_roots);
    RUN_TEST(test_non_field_and_missing_binding_are_rejected_before_heap_work);
    RUN_TEST(test_insufficient_target_capacity_preserves_existing_graph);
    RUN_TEST(test_large_cycle_and_repeated_preparation);
    RUN_TEST(test_each_native_preparation_allocation_failure_preserves_target);
    RUN_TEST(test_owner_transfer_is_rejected_without_changing_the_source);
    RUN_TEST(test_restoration_does_not_invoke_constructor_or_field_accessors);
    RUN_TEST(test_resource_prototype_rejects_before_object_allocation);
    RUN_TEST(test_resource_source_without_owner_handle_is_rejected);
    RUN_TEST(test_output_storage_cannot_overlap_source_value_payloads);
    RUN_TEST(test_existing_ignore_registrations_survive_success_and_failure);
    RUN_TEST(test_output_storage_cannot_overlap_ir_or_prototype_metadata);
    RUN_TEST(test_reference_class_graph_preserves_identity);
    RUN_TEST(test_inactive_cleanup_payload_is_neither_read_nor_rooted);
    RUN_TEST(test_output_storage_cannot_overwrite_concrete_owner_witness);
    return UNITY_END();
}
