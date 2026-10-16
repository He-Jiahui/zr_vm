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
#include "exec_ir/exec_ir_mutation_internal.h"

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

static void test_concurrent_attachment_throw_does_not_release_unwound_lock_again(void) {
    SZrGcDomainMutatorSnapshot snapshot;
    SZrGarbageCollector *collector = state->global->garbageCollector;
    TZrUInt32 invalidEnds;
    TZrBool held = ZR_FALSE;
    TZrBool materialized;
    /* Keep status normalization allocation-free while testing lock retirement. */
    ZrCore_Value_InitAsInt(state, &state->currentException, 912);
    state->hasCurrentException = ZR_TRUE;
    state->currentExceptionStatus = ZR_THREAD_STATUS_EXCEPTION_ERROR;
    ZrCore_GarbageCollector_SetWorkerCount(state->global, 1u);
    ZrCore_GarbageCollector_ScheduleCollection(
            state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR);
    ZrCore_GarbageCollector_GcStep(state);
    TEST_ASSERT_TRUE(collector->concurrentMajorActive);
    ssa_runtime_objects_set_barrier_hook(throw_during_attachment, &held);
    materialized = ssa_runtime_objects_materialize(&request, &diagnostic);
    invalidEnds = ssa_runtime_objects_invalid_mutation_end_count();
    ZrCore_GcDomain_GetMutatorSnapshot(state, &snapshot);
    for (TZrUInt32 step = 0u; collector->concurrentMajorActive && step < 4096u; ++step)
        ZrCore_GarbageCollector_GcStep(state);
    TEST_ASSERT_FALSE(collector->concurrentMajorActive);
    TEST_ASSERT_FALSE(materialized);
    TEST_ASSERT_TRUE(held);
    TEST_ASSERT_EQUAL_UINT32(0u, invalidEnds);
    TEST_ASSERT_FALSE(snapshot.pauseRequested);
    TEST_ASSERT_EQUAL_UINT32(0u, snapshot.runningMutatorCount);
    TEST_ASSERT_EQUAL_UINT32(0u, target.count);
    TEST_ASSERT_NULL(objects[0]);
    TEST_ASSERT_NULL(objects[1]);
    TEST_ASSERT_TRUE(state->hasCurrentException);
    TEST_ASSERT_EQUAL_INT64(912, state->currentException.value.nativeObject.nativeInt64);
    assert_failure_location(ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY);
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
    RUN_TEST(test_concurrent_attachment_throw_does_not_release_unwound_lock_again);
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

/**
 * @var SZrState *state
 *
 * ## Summary
 * 保存本可执行程序单 case 的真实 runtime state。
 *
 * ## Constraints and lifecycle
 * setUp 创建并选 generational GC，tearDown 销毁后置 NULL；其余 fixture 指针借用此 state 的 GC 对象。记录地址与 GC 对象生命周期分开，不能把静态指针本身视作 GC 根。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:16；tests/core/test_ssa_runtime_objects.c:37；tests/core/test_ssa_runtime_objects.c:39；tests/core/test_ssa_runtime_objects.c:145；tests/core/test_ssa_runtime_objects.c:146。
 */

/**
 * @var SZrExecIrFunction function
 *
 * ## Summary
 * 持有 checkpoint 所用 ExecIR 池和自有 stateMap。
 *
 * ## Constraints and lifecycle
 * FunctionInit 后填充值/deopt recipes 并 BuildStateMaps；FreeFunction 负责其 native 池/map。重建 map 会使旧借用失效，request.map 必须更新；不代表可运行任意完整 CFG。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:17；tests/core/test_ssa_runtime_objects.c:45；tests/core/test_ssa_runtime_objects.c:97；tests/core/test_ssa_runtime_objects.c:144；tests/core/test_ssa_runtime_objects.c:458；tests/core/test_ssa_runtime_objects.c:547；tests/core/test_ssa_runtime_objects.c:773。
 */

/**
 * @var SZrExecIrObjectMaterializationRequest request
 *
 * ## Summary
 * 聚合对当前 case 的函数、map、values、types、owner witness 与 target 的借用。
 *
 * ## Constraints and lifecycle
 * 记录本身在 setUp 清零后构造，嵌套输入与输出有不同可变性；部分 case 暂借局部栈 owner/value 数组，只能用于同步物化调用期间。它不拥有借用数组或延长 GC 对象生命周期。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:18；tests/core/test_ssa_runtime_objects.c:40；tests/core/test_ssa_runtime_objects.c:125；tests/core/test_ssa_runtime_objects.c:134；tests/core/test_ssa_runtime_objects.c:136；tests/core/test_ssa_runtime_objects.c:138；tests/core/test_ssa_runtime_objects.c:775；tests/core/test_ssa_runtime_objects.c:781；tests/core/test_ssa_runtime_objects.c:801。
 */

/**
 * @var SZrExecIrMaterializedObjects target
 *
 * ## Summary
 * 记录调用方提供的发布槽及已发布逻辑数量。
 *
 * ## Constraints and lifecycle
 * 常态 objects=静态双槽数组、capacity2；大图 case 临时改为 calloc 数组；alias case 必须在断言前恢复。只有非静态当前 objects 才由 tearDown free，GC 对象不会逐个 C free；count0不保证槽字节清零。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:19；tests/core/test_ssa_runtime_objects.c:41；tests/core/test_ssa_runtime_objects.c:123；tests/core/test_ssa_runtime_objects.c:124；tests/core/test_ssa_runtime_objects.c:143；tests/core/test_ssa_runtime_objects.c:519；tests/core/test_ssa_runtime_objects.c:563；tests/core/test_ssa_runtime_objects.c:564；tests/core/test_ssa_runtime_objects.c:656；tests/core/test_ssa_runtime_objects.c:658；tests/core/test_ssa_runtime_objects.c:803；tests/core/test_ssa_runtime_objects.c:806。
 */

/**
 * @var SZrExecIrRuntimeTypeBinding binding
 *
 * ## Summary
 * 将固定 typeToken/layoutId 映射到当前 GC 原型与字段绑定。
 *
 * ## Constraints and lifecycle
 * fields 借用静态数组，prototype 借用 state 管理的 GC 对象；layoutGeneration 要与当时原型一致。部分负例故意改变 token/generation 或原型，不能将这些时点视作通用合法绑定。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:20；tests/core/test_ssa_runtime_objects.c:42；tests/core/test_ssa_runtime_objects.c:113；tests/core/test_ssa_runtime_objects.c:115；tests/core/test_ssa_runtime_objects.c:116；tests/core/test_ssa_runtime_objects.c:117；tests/core/test_ssa_runtime_objects.c:118；tests/core/test_ssa_runtime_objects.c:332；tests/core/test_ssa_runtime_objects.c:498；tests/core/test_ssa_runtime_objects.c:717；tests/core/test_ssa_runtime_objects.c:718。
 */

/**
 * @var SZrExecIrRuntimeFieldBinding fields[4]
 *
 * ## Summary
 * 保存四个 recipe fieldIndex 到 prototype descriptor 索引的静态映射。
 *
 * ## Constraints and lifecycle
 * setUp 按索引0..3覆盖每项；binding 不拥有此数组。重复目的索引负例仅改变第1项，下一 case 重新初始化；这里的4是固定 fixture规模，不是公开最大字段数。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:21；tests/core/test_ssa_runtime_objects.c:110；tests/core/test_ssa_runtime_objects.c:111；tests/core/test_ssa_runtime_objects.c:117；tests/core/test_ssa_runtime_objects.c:449。
 */

/**
 * @var SZrTypeValue values[3]
 *
 * ## Summary
 * 保存固定三个运行时源单元，供 VALUE 配方与 GC 根注册读取或更新。
 *
 * ## Constraints and lifecycle
 * 第0单元GC对象、第1整数73、第2NULL；数组为静态存储但 payload 对象由 state 管理，移动/收集时的有效性取决于已注册根。物化可以更新根所指 payload，失败不承诺其所有字节冻结。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:22；tests/core/test_ssa_runtime_objects.c:44；tests/core/test_ssa_runtime_objects.c:120；tests/core/test_ssa_runtime_objects.c:121；tests/core/test_ssa_runtime_objects.c:122；tests/core/test_ssa_runtime_objects.c:134；tests/core/test_ssa_runtime_objects.c:467；tests/core/test_ssa_runtime_objects.c:487；tests/core/test_ssa_runtime_objects.c:599；tests/core/test_ssa_runtime_objects.c:603；tests/core/test_ssa_runtime_objects.c:785。
 */

/**
 * @var SZrObject *objects[2]
 *
 * ## Summary
 * 提供常态双对象图的调用方发布槽。
 *
 * ## Constraints and lifecycle
 * 槽内指针的对象归 runtime GC；静态槽不由 tearDown free。成功发布/FullGC 可改写槽，旧裸借用不能跨无根的收集；target 借用槽不自动把其变成永久根。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:23；tests/core/test_ssa_runtime_objects.c:43；tests/core/test_ssa_runtime_objects.c:123；tests/core/test_ssa_runtime_objects.c:143；tests/core/test_ssa_runtime_objects.c:483；tests/core/test_ssa_runtime_objects.c:485；tests/core/test_ssa_runtime_objects.c:806。
 */

/**
 * @var SZrExecIrDiagnostic diagnostic
 *
 * ## Summary
 * 保存 builder/物化写出的独立诊断结果供有限断言读取。
 *
 * ## Constraints and lifecycle
 * 不与 request、target、IR/map、values、types 或 owner witness 重叠；不假定每 case 开始先清空，也不把未读取的字段计为断言覆盖。对象 materializer 的失败路径按位置补诊断，成功不得靠旧诊断推导失败。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:24；tests/core/test_ssa_runtime_objects.c:97；tests/core/test_ssa_runtime_objects.c:175；tests/core/test_ssa_runtime_objects.c:176；tests/core/test_ssa_runtime_objects.c:177；tests/core/test_ssa_runtime_objects.c:178；tests/core/test_ssa_runtime_objects.c:183；tests/core/test_ssa_runtime_objects.c:808。
 */

/**
 * @var TZrUInt32 callbackCount
 *
 * ## Summary
 * 观测安装到构造器和字段访问器的同一个 native callable 是否被调用。
 *
 * ## Constraints and lifecycle
 * unexpected_callback 每次加1；只有安装 callback 的 case 显式归零，并在恢复图后检查仍0。不是所有 runtime callback 的全局监视器，且不是原子计数，fixture须串行使用。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:25；tests/core/test_ssa_runtime_objects.c:29；tests/core/test_ssa_runtime_objects.c:625；tests/core/test_ssa_runtime_objects.c:628。
 */

/**
 * @fn TZrInt64 unexpected_callback(SZrState *callbackState)
 *
 * ## Summary
 * 为构造器与字段访问入口提供可观测的同一 native callable；物化测试期望它不被调用。
 *
 * ## Parameters
 * @param[in] callbackState 符合 native 回调签名的状态参数；本函数仅用 UNUSED 标记，不解引用。
 *
 * ## Returns
 * @return 固定整数0；唯一观测副作用是 callbackCount 增1。
 *
 * ## Constraints and lifecycle
 * 函数只用于本TU的串行 callback 计数，不分配、释放或转移 VM 资源。它的地址被安装到 closure，再用于 constructor、initializer、getter、setter；没有本TU直接调用，恢复测试实际只检查计数仍为0，不证明所有可能回调入口。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:27；tests/core/test_ssa_runtime_objects.c:29；tests/core/test_ssa_runtime_objects.c:616；tests/core/test_ssa_runtime_objects.c:619；tests/core/test_ssa_runtime_objects.c:620；tests/core/test_ssa_runtime_objects.c:621；tests/core/test_ssa_runtime_objects.c:622；tests/core/test_ssa_runtime_objects.c:628；zr_vm_core/src/zr_vm_core/function/function.c:5004。
 */

/**
 * @fn void setUp(void)
 *
 * ## Summary
 * 在每个 Objects case 前建立独立真实 VM、checkpoint/deopt 配方和双节点运行时绑定。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值。全部 fixture 构造断言满足时正常完成；断言失败由 Unity longjmp 退出当前 case 的受保护阶段，runner 随后尝试 tearDown，不继续该 case 正文。
 *
 * ## Constraints and lifecycle
 * 先创建 state 并设 generational GC，初始化自有 function、3个external value及NOP/MAY_GC deopt checkpoint，构造两aggregate与7个field配方并 BuildStateMaps，再创建固定 STRUCT prototype/4个descriptor、3个运行时源和双输出槽。fixed token99/generation7/signature123/source42/resume701 只服务该 fixture；不是通用可执行CFG构造器。request/target/binding是记录，静态数组及GC对象分别借用；function的native池/map由FreeFunction，GC对象由state销毁。prototype在添加descriptor期间Ignore、正常路径Unignore。只在末尾复位故障替身；不显式清空diagnostic或callbackCount。构造中任一断言早退可跳过后续初始化/Unignore，不能保证所有局部资源按正常尾部释放。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:33；tests/core/test_ssa_runtime_objects.c:37；tests/core/test_ssa_runtime_objects.c:45；tests/core/test_ssa_runtime_objects.c:60；tests/core/test_ssa_runtime_objects.c:97；tests/core/test_ssa_runtime_objects.c:102；tests/core/test_ssa_runtime_objects.c:119；tests/core/test_ssa_runtime_objects.c:120；tests/core/test_ssa_runtime_objects.c:123；tests/core/test_ssa_runtime_objects.c:139；tests/third_party/zr_unity/Unity/src/unity.c:2298；tests/third_party/zr_unity/Unity/src/unity.c:2303。
 */

/**
 * @fn void tearDown(void)
 *
 * ## Summary
 * 释放当前 case 的自有 function 和 VM，并按当前 target 指针回收额外的 native 输出槽。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；按顺序完成当前全局 fixture 的释放后返回 Unity runner，没有失败码或资源返回。
 *
 * ## Constraints and lifecycle
 * target.objects若不是静态objects数组则交给C free，因此alias负例必须在断言之前恢复指针，大图正常尾部也必须避免留下已free地址。FreeFunction回收其IR池/map；State_Destroy回收state所属runtime资源，之后只将state置NULL。它不逐个free GC对象、不清空所有静态记录、不替局部case执行剩余AOT根Pop、NativeLeave、MutatorLeave或暂停End；Unity longjmp也不会执行被跳过的case尾部。依赖setUp已完成的有效初始化；不承诺任意半初始化损坏记录都可安全清理。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:142；tests/core/test_ssa_runtime_objects.c:143；tests/core/test_ssa_runtime_objects.c:144；tests/core/test_ssa_runtime_objects.c:145；tests/core/test_ssa_runtime_objects.c:146；tests/core/test_ssa_runtime_objects.c:489；tests/core/test_ssa_runtime_objects.c:563；tests/core/test_ssa_runtime_objects.c:658；tests/core/test_ssa_runtime_objects.c:806；tests/third_party/zr_unity/Unity/src/unity.c:2303。
 */

/**
 * @fn SZrHashKeyValuePair *field(TZrUInt32 objectIndex, TZrUInt32 descriptorIndex)
 *
 * ## Summary
 * 按固定 fixture 的对象槽和 descriptor 名称查找已发布字段 pair。
 *
 * ## Parameters
 * @param[in] objectIndex 静态 objects 数组的有效索引；本helper不检查边界或对象非空。
 * @param[in] descriptorIndex 当前 binding.prototype 的有效 descriptor 索引；其 name 必须是有效字符串。
 *
 * ## Returns
 * @return 真实 HashSet_Find 的借用 pair 指针；字段不存在可为空，不分配或转移pair所有权。
 *
 * ## Constraints and lifecycle
 * 使用全局objects而不是target.objects，故仅用于双对象fixture/相应class变体。调用时state、prototype、descriptor与对象nodeMap均须仍有效且读取期间稳定；返回pair仅在所属map未释放/重建或引起存储失效的修改前可用，不能作为跨无根GC或对象释放的独立保活。函数自身不做Unity断言；调用方在解引用返回pair前必须保证其非空，当前调用点并非逐个先作非空断言。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:149；tests/core/test_ssa_runtime_objects.c:151；tests/core/test_ssa_runtime_objects.c:153；tests/core/test_ssa_runtime_objects.c:161；tests/core/test_ssa_runtime_objects.c:164；tests/core/test_ssa_runtime_objects.c:487；tests/core/test_ssa_runtime_objects.c:722。
 */

/**
 * @fn void assert_graph(void)
 *
 * ## Summary
 * 集中断言固定双对象图中有限的身份、初始化状态和源字段值。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；所列运行期断言全部满足则正常返回，否则 Unity longjmp 退出当前 case，runner 继续其清理/后续 case 流程。
 *
 * ## Constraints and lifecycle
 * 检查count2、两对象不同、第0对象prototype等于binding且tag为STRUCT、三条重复/回边、未初始化字段缺席、NULL字段以及源GC对象/整数73。没有逐一断言第1对象prototype/tag、所有descriptor或整个堆；仅适用于常态objects[2]和该固定recipe。field返回值借用于同步断言，不跨GC保留。本helper不释放任何case局部资源，断言早退可跳过调用方尾部清理。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:156；tests/core/test_ssa_runtime_objects.c:157；tests/core/test_ssa_runtime_objects.c:159；tests/core/test_ssa_runtime_objects.c:160；tests/core/test_ssa_runtime_objects.c:161；tests/core/test_ssa_runtime_objects.c:164；tests/core/test_ssa_runtime_objects.c:167；tests/core/test_ssa_runtime_objects.c:170；tests/core/test_ssa_runtime_objects.c:171；tests/third_party/zr_unity/Unity/src/unity_internals.h:891。
 */

/**
 * @fn void assert_failure_location(EZrExecutionDiagnosticCode code)
 *
 * ## Summary
 * 集中断言 Objects 失败诊断的固定位置及临时 AOT 根深度回到0。
 *
 * ## Parameters
 * @param[in] code 本调用场景期望的诊断枚举值。
 *
 * ## Returns
 * 无C返回值；指定字段与root depth断言满足时返回；否则 Unity 中止当前 case 的受保护正文，不直接终止整个进程。
 *
 * ## Constraints and lifecycle
 * 只读全局diagnostic.code/functionToken/sourceId/instructionId及当前state根深度；固定期望99/42/1。它不核对resumeId、blockId、全部VM状态、所有锁或GC根内容，也不代表底层每个拒绝分支已被执行。本helper不分配或回收资源；调用方必须已完成会填诊断的同步调用。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:174；tests/core/test_ssa_runtime_objects.c:175；tests/core/test_ssa_runtime_objects.c:176；tests/core/test_ssa_runtime_objects.c:177；tests/core/test_ssa_runtime_objects.c:178；tests/core/test_ssa_runtime_objects.c:179；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:101；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:730。
 */

/**
 * @fn void test_real_struct_graph_preserves_aliases_cycle_and_initialization(void)
 *
 * ## Summary
 * 建立双对象 STRUCT 图的最小成功断言。实际运行期断言：普通库入口返回TRUE；assert_graph检查两对象不同、第0对象prototype/tag、重复/回边、未初始化字段缺席、NULL/GC对象/整数源字段；直接检查root depth0。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 使用普通库入口；两个 aggregate 的重复边和回边属于允许的配方图，不是 prototype 继承链。assert_graph 只检查列出的节点/字段，不证明全部原型层次或任意图规模。 GC 对象由测试 state 管理，objects 是调用方提供的两个发布槽。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:182；tests/core/test_ssa_runtime_objects.c:815；tests/core/test_ssa_runtime_objects.c:183；tests/core/test_ssa_runtime_objects.c:185；tests/core/test_ssa_runtime_objects.c:184；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:550；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:606；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:725。
 */

/**
 * @fn void test_gc_during_every_object_allocation_preserves_sources_and_graph(void)
 *
 * ## Summary
 * 在每次故障替身 Object_New 前执行 FullGC，检查源与结果图。实际运行期断言：fault copy每次对象申请前真实FullGC；断言2次对象申请、fullCollectionCount至少+2、有限图及root深度。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * collectEnabled 也作用于字段 pair-pool 预留；断言只要求 collection count 至少增加二次，不固定实际回收总次数或证明对象发生移动。 源值/原型和新对象在物化内部临时根保护期间可能被更新；成功后仍需调用方保活。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:188；tests/core/test_ssa_runtime_objects.c:816；tests/core/test_ssa_runtime_objects.c:191；tests/core/test_ssa_runtime_objects.c:192；tests/core/test_ssa_runtime_objects.c:193；tests/core/test_ssa_runtime_objects.c:196；tests/core/test_ssa_runtime_objects.c:195；tests/core/ssa_runtime_objects_faults.c:69；tests/core/ssa_runtime_objects_faults.c:81；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:488。
 */

/**
 * @fn void test_nth_object_failure_preserves_prior_target_and_balances_roots(void)
 *
 * ## Summary
 * 逐一使双对象准备的第1或第2次 Object_New 返回空并保留旧输出。实际运行期断言：预先成功旧图；对象ordinal1/2返回NULL，断言OUT_OF_MEMORY位置、旧指针字节、ignored计数、图和root深度。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 循环仅覆盖两个对象分配入口；不遍历 linked allocator、逻辑物化或全部 native 分配。比较旧对象指针槽字节、ignored 数量和有限图字段，未比较整个运行时堆。 旧输出先由普通入口创建；候选 GC 对象由实际 runtime 清理/回收，测试不逐个 C free。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:199；tests/core/test_ssa_runtime_objects.c:817；tests/core/test_ssa_runtime_objects.c:203；tests/core/test_ssa_runtime_objects.c:207；tests/core/test_ssa_runtime_objects.c:209；tests/core/test_ssa_runtime_objects.c:210；tests/core/test_ssa_runtime_objects.c:208；tests/core/test_ssa_runtime_objects.c:211；tests/core/ssa_runtime_objects_faults.c:67；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:642；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:730。
 */

/**
 * @fn void test_throwing_oom_restores_ambient_exception_and_allows_retry(void)
 *
 * ## Summary
 * 使第二个对象创建抛出 OOM，检查环境异常恢复及一次无注入重试。实际运行期断言：对象ordinal2 Throw OOM；保留ambient整数异常/状态和空target，关闭fault后能重试成功。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 只观察整数异常912、指定 exception status/flag、threadStatus、count、诊断和重试图；不枚举所有异常种类或嵌套 TryRun 组合。 重试前显式复位故障配置；环境异常对象并非本例新增资源。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:215；tests/core/test_ssa_runtime_objects.c:818；tests/core/test_ssa_runtime_objects.c:220；tests/core/test_ssa_runtime_objects.c:222；tests/core/test_ssa_runtime_objects.c:223；tests/core/test_ssa_runtime_objects.c:224；tests/core/test_ssa_runtime_objects.c:225；tests/core/test_ssa_runtime_objects.c:226；tests/core/test_ssa_runtime_objects.c:228；tests/core/test_ssa_runtime_objects.c:221；tests/core/test_ssa_runtime_objects.c:229；tests/core/ssa_runtime_objects_faults.c:71；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:707；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:714。
 */

/**
 * @fn void test_detached_field_reservation_failure_leaves_target_unchanged(void)
 *
 * ## Summary
 * 逐一拒绝两个 detached 字段 pair-pool 预留并检查尚未发布的空输出。实际运行期断言：pair reservation ordinal1/2失败；断言OOM位置、target.count0和两slot仍NULL。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 仅两个 test_reserve 序号；objects[0/1] 仍空及 count0，不能外推所有哈希桶或真实 allocator OOM。 失败只退役候选 map/根/临时数组；fixture 持有输出槽。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:232；tests/core/test_ssa_runtime_objects.c:819；tests/core/test_ssa_runtime_objects.c:236；tests/core/test_ssa_runtime_objects.c:238；tests/core/test_ssa_runtime_objects.c:239；tests/core/test_ssa_runtime_objects.c:240；tests/core/test_ssa_runtime_objects.c:237；tests/core/ssa_runtime_objects_faults.c:80；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:564。
 */

/**
 * @fn void test_remembered_reservation_failure_preserves_existing_graph(void)
 *
 * ## Summary
 * 拒绝 remembered registry 预留并验证旧图保留。实际运行期断言：remembered reservation失败；断言OOM位置、旧输出指针及有限图保持。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * failRemembered 是持续开关至下一次 faults 复位；仅此调用边界被强制返回 FALSE，未证明 remembered 集合所有分配路径均可注入。 先成功发布旧图，之后比较发布槽和有限字段；候选资源由物化统一清理。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:244；tests/core/test_ssa_runtime_objects.c:820；tests/core/test_ssa_runtime_objects.c:246；tests/core/test_ssa_runtime_objects.c:249；tests/core/test_ssa_runtime_objects.c:251；tests/core/test_ssa_runtime_objects.c:250；tests/core/test_ssa_runtime_objects.c:252；tests/core/ssa_runtime_objects_faults.c:86；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:582。
 */

/**
 * @fn void test_gc_preserves_ambient_object_exception_on_oom(void)
 *
 * ## Summary
 * 在 FullGC 和抛出 OOM 后保留作为环境异常的源 GC 对象。实际运行期断言：ambient异常为真实GC对象且对象ordinal2 Throw+GC；断言状态/异常指针同更新后的values[0]、空target。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 异常指针与可能已更新的 values[0] 比较；不假定 GC 移动，也未证明所有线程/全局异常状态逐字节不变。 正常尾部把 threadStatus 恢复 FINE；若断言 longjmp，该尾部不会执行，tearDown 仍仅处理全局 fixture。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:255；tests/core/test_ssa_runtime_objects.c:821；tests/core/test_ssa_runtime_objects.c:261；tests/core/test_ssa_runtime_objects.c:263；tests/core/test_ssa_runtime_objects.c:264；tests/core/test_ssa_runtime_objects.c:265；tests/core/test_ssa_runtime_objects.c:266；tests/core/test_ssa_runtime_objects.c:267；tests/core/test_ssa_runtime_objects.c:262；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:521；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:714。
 */

/**
 * @fn void test_caught_oom_preserves_nested_mutator_and_native_scopes(void)
 *
 * ## Summary
 * 检查 OOM 捕获后两层 mutator 与两层 GC_AWARE native 作用域仍按序可退出。实际运行期断言：嵌套mutator/native GC_AWARE各2层，Throw+GC后按次leave检查running计数；不穷尽任意嵌套层数。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 只断言指定退出序列中的 runningMutatorCount 为1/1/1/0，未读取或逐字段比较全部作用域深度/epoch。 MutatorLeave/NativeLeave 位于断言后的正常路径；longjmp 可跳过剩余 leave，不能称任意断言失败均平衡局部作用域。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:271；tests/core/test_ssa_runtime_objects.c:822；tests/core/test_ssa_runtime_objects.c:273；tests/core/test_ssa_runtime_objects.c:274；tests/core/test_ssa_runtime_objects.c:275；tests/core/test_ssa_runtime_objects.c:276；tests/core/test_ssa_runtime_objects.c:278；tests/core/test_ssa_runtime_objects.c:281；tests/core/test_ssa_runtime_objects.c:285；tests/core/test_ssa_runtime_objects.c:288；tests/core/test_ssa_runtime_objects.c:291；tests/core/test_ssa_runtime_objects.c:279；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:59；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:79。
 */

/**
 * @fn void test_caught_oom_preserves_native_only_scopes(void)
 *
 * ## Summary
 * 检查仅有两层 GC_AWARE native 作用域时的 OOM 后续退出。实际运行期断言：仅两层native GC_AWARE作用域，Throw+GC后逐次leave观察running计数。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 只观察两次 NativeLeave 后 runningMutatorCount 为1、0；不覆盖所有 native mode 或深度溢出。 本例进入的 native 作用域在正常尾部退出；Unity 早退可跳过后续 leave。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:294；tests/core/test_ssa_runtime_objects.c:823；tests/core/test_ssa_runtime_objects.c:296；tests/core/test_ssa_runtime_objects.c:297；tests/core/test_ssa_runtime_objects.c:299；tests/core/test_ssa_runtime_objects.c:302；tests/core/test_ssa_runtime_objects.c:305；tests/core/test_ssa_runtime_objects.c:308；tests/core/test_ssa_runtime_objects.c:300；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:64；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:94。
 */

/**
 * @fn void test_unsafe_native_modes_reject_without_heap_or_scope_changes(void)
 *
 * ## Summary
 * 检查两种不安全 native mode 在准备 GC 对象前被拒绝。实际运行期断言：BLOCKING_DETACHED和NO_SAFEPOINT_CRITICAL两模式早拒绝；比较对应计数、0对象申请、离开后running0。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * BLOCKING_DETACHED 与 NO_SAFEPOINT_CRITICAL 各运行一次；首拒绝位于 save_scopes 的 mode/status 检查。对象替身计数0不等于证明整次调用绝无任何 allocator 行为。 每轮正常路径 NativeLeave；断言早退不保证本地作用域尾部执行。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:311；tests/core/test_ssa_runtime_objects.c:824；tests/core/test_ssa_runtime_objects.c:317；tests/core/test_ssa_runtime_objects.c:319；tests/core/test_ssa_runtime_objects.c:321；tests/core/test_ssa_runtime_objects.c:323；tests/core/test_ssa_runtime_objects.c:324；tests/core/test_ssa_runtime_objects.c:327；tests/core/test_ssa_runtime_objects.c:320；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:64；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:679。
 */

/**
 * @fn void test_stale_layout_fails_before_allocating_objects(void)
 *
 * ## Summary
 * 检查过期 layoutGeneration 在 GC 对象创建前失败。实际运行期断言：只改binding.layoutGeneration；断言MATERIALIZATION_FAILED位置和0对象申请，指向types_valid代际比较。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 只递增 binding.layoutGeneration；graph/types 验证失败前已可能完成逻辑 MaterializeState，0对象次数不等于0 native 分配。 借用当前 prototype，不修改 prototype 实际布局；tearDown 结束其 state 生命周期。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:331；tests/core/test_ssa_runtime_objects.c:825；tests/core/test_ssa_runtime_objects.c:333；tests/core/test_ssa_runtime_objects.c:334；tests/core/test_ssa_runtime_objects.c:335；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:317；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:684。
 */

/**
 * @fn void observe_attachment_pause(SZrState *callbackState, void *context)
 *
 * ## Summary
 * 作为一次性 barrier hook，把附着期间的 pauseRequested 快照写回调用方。
 *
 * ## Parameters
 * @param[in] callbackState 实际执行附着的有效state，供GetMutatorSnapshot读取。
 * @param[out] context 指向可写TZrBool的借用地址；必须存活到同步hook返回。
 *
 * ## Returns
 * 无C返回值；写入快照中的pauseRequested后正常返回，不分配或释放资源。
 *
 * ## Constraints and lifecycle
 * 直接转换context并写入，无NULL/类型/边界检查。setter只借用该地址，fault test_barrier在调用前已清空hook；直接调用者throw_during_attachment随后抛错。本函数记录一个时点的布尔值，不证明锁深度或整个时间段没有竞争收集。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:338；tests/core/test_ssa_runtime_objects.c:340；tests/core/test_ssa_runtime_objects.c:341；tests/core/test_ssa_runtime_objects.c:347；tests/core/test_ssa_runtime_objects.c:371；tests/core/ssa_runtime_objects_faults.c:95；tests/core/ssa_runtime_objects_faults.c:96；tests/core/ssa_runtime_objects_faults.c:97。
 */

/**
 * @fn void test_attachment_holds_and_releases_collection_pause(void)
 *
 * ## Summary
 * 用首个原型附着 barrier 回调观察 collection pause 的持有和释放。实际运行期断言：one-shot barrier hook记录pauseRequested；成功后pausefalse并核对有限图。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 一次 hook 记录 pauseRequested，再检查返回后为 FALSE；不能等同精确锁递归深度或所有调度交错的证明。 held 栈单元仅在同步物化调用期间被借用；hook 在调用前已清空，不把该地址留给后续 case。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:344；tests/core/test_ssa_runtime_objects.c:826；tests/core/test_ssa_runtime_objects.c:348；tests/core/test_ssa_runtime_objects.c:349；tests/core/test_ssa_runtime_objects.c:351；tests/core/test_ssa_runtime_objects.c:352；tests/core/ssa_runtime_objects_faults.c:94；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:570；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:653。
 */

/**
 * @fn void test_failed_attachment_pause_preserves_old_graph(void)
 *
 * ## Summary
 * 强制附着暂停获取失败后检查旧图和域快照。实际运行期断言：pause hook返回false；断言失败分类、旧指针/图、pausefalse及running0。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 强制 test_pause 返回 FALSE；检查诊断、旧槽、pauseRequested 和 runningMutatorCount，未枚举真实超时或域暂停诊断字段。 失败发生时候选对象/native work 可以已经分配；旧图仍由 state 和 fixture 持有。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:355；tests/core/test_ssa_runtime_objects.c:827；tests/core/test_ssa_runtime_objects.c:358；tests/core/test_ssa_runtime_objects.c:361；tests/core/test_ssa_runtime_objects.c:363；tests/core/test_ssa_runtime_objects.c:365；tests/core/test_ssa_runtime_objects.c:366；tests/core/test_ssa_runtime_objects.c:362；tests/core/test_ssa_runtime_objects.c:367；tests/core/ssa_runtime_objects_faults.c:90；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:570。
 */

/**
 * @fn void throw_during_attachment(SZrState *callbackState, void *context)
 *
 * ## Summary
 * 在附着 barrier 先记录暂停快照，再向物化TryRun抛出MEMORY_ERROR。
 *
 * ## Parameters
 * @param[in,out] callbackState 用于读取域快照并改变异常/恢复上下文的有效state。
 * @param[out] context 可写TZrBool的借用地址，要求与observe_attachment_pause相同。
 *
 * ## Returns
 * 无C返回值；观察后调用不返回的Exception_Throw，控制转移到外层恢复点，不产生普通失败码。
 *
 * ## Constraints and lifecycle
 * 仅由两例配置到一次性barrier hook，需有效的外层异常恢复点及同步有效的栈context。Throw会中断真实barrier后续语句，故障wrapper已先清空hook；恢复由真实exception/runtime路径和物化统一cleanup承担，不由本callback释放fixture。只测试该位置的MEMORY_ERROR。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:370；tests/core/test_ssa_runtime_objects.c:371；tests/core/test_ssa_runtime_objects.c:372；tests/core/test_ssa_runtime_objects.c:381；tests/core/test_ssa_runtime_objects.c:407；tests/core/ssa_runtime_objects_faults.c:97；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:707；zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:80。
 */

/**
 * @fn void test_attachment_exception_releases_pause_and_preserves_old_graph(void)
 *
 * ## Summary
 * 在附着 barrier 内抛出 MEMORY_ERROR 后检查 pause、旧图和运行计数。实际运行期断言：one-shot barrier hook真实Throw OOM；pause曾持有，失败后释放pause/running、旧图保持。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * hook 先观察 pause 再抛错；只覆盖这一回调位置及 MEMORY_ERROR，非所有用户异常/所有 barrier。 held 是同步回调借用；物化在 TryRun 后退役根和暂停，测试只观察所列终态。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:375；tests/core/test_ssa_runtime_objects.c:828；tests/core/test_ssa_runtime_objects.c:379；tests/core/test_ssa_runtime_objects.c:382；tests/core/test_ssa_runtime_objects.c:383；tests/core/test_ssa_runtime_objects.c:385；tests/core/test_ssa_runtime_objects.c:387；tests/core/test_ssa_runtime_objects.c:388；tests/core/test_ssa_runtime_objects.c:384；tests/core/test_ssa_runtime_objects.c:389；tests/core/test_ssa_runtime_objects.c:370；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:708；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:647。
 */

/**
 * @fn void test_concurrent_attachment_throw_does_not_release_unwound_lock_again(void)
 *
 * ## Summary
 * 在并发 major 标记和附着抛错组合中检测重复释放已退役 mutation token 的请求。实际运行期断言：真实concurrentMajorActive加one-shot Throw；最多4096步drain，断言无invalid MutationEnd、pause/running0、空target、ambient912。非任意交错证明。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * fault wrapper 观察 locked=TRUE 且当前 mutationDepth=0 时记录并抑制真实 unlock；invalidEnds0 证明该检测器没有命中，不是直接执行错误平台 unlock 的测试。major 排空最多4096步，不枚举所有并发调度。 先设置整数环境异常避免状态归一化额外分配；worker、域和 GC 状态由真实 runtime 管理；断言早退不保证完成本地 GC 排空。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:392；tests/core/test_ssa_runtime_objects.c:829；tests/core/test_ssa_runtime_objects.c:406；tests/core/test_ssa_runtime_objects.c:413；tests/core/test_ssa_runtime_objects.c:414；tests/core/test_ssa_runtime_objects.c:415；tests/core/test_ssa_runtime_objects.c:416；tests/core/test_ssa_runtime_objects.c:417；tests/core/test_ssa_runtime_objects.c:418；tests/core/test_ssa_runtime_objects.c:419；tests/core/test_ssa_runtime_objects.c:420；tests/core/test_ssa_runtime_objects.c:421；tests/core/test_ssa_runtime_objects.c:422；tests/core/test_ssa_runtime_objects.c:423；tests/core/test_ssa_runtime_objects.c:424；tests/core/ssa_runtime_objects_faults.c:101；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:712；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:636。
 */

/**
 * @fn void test_existing_pause_survives_nested_materialization(void)
 *
 * ## Summary
 * 检查既有 StopTheWorld 暂停在嵌套物化返回后仍有效。实际运行期断言：外层预持有pause；物化成功后外层pause仍在，显式End后为false。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 断言外层 pauseRequested 在内层结束后仍真，再由本例 End 后变假；不验证任意多层暂停计数。 外层暂停的 End 位于正常路径；Unity early abort 可跳过 End。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:427；tests/core/test_ssa_runtime_objects.c:830；tests/core/test_ssa_runtime_objects.c:429；tests/core/test_ssa_runtime_objects.c:430；tests/core/test_ssa_runtime_objects.c:432；tests/core/test_ssa_runtime_objects.c:435；tests/core/test_ssa_runtime_objects.c:436；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:570；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:653。
 */

/**
 * @fn void test_competing_collector_during_field_attachment_preserves_graph(void)
 *
 * ## Summary
 * 调用真实竞争 collector 支持，检查物化后的图和一次并发收集交接。实际运行期断言：经concurrency helper启动真实collector；caller预注册输入根并维持RUNNING到发布输出根；有限有界同步后图/root深度/collection计数检查。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * helper 要求首 barrier 被观察、收集完成且发生于 materializationReturned 之后，并 join worker；本例还只比较 fullCollectionCount 增加、rootdepth0 和有限图字段。不保证任意交错。 并发 helper 在 caller 根仍有效时保护输入/旧输出/新输出；超时无法安全 join 时调用 abort，区别于 Unity 断言 longjmp。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:439；tests/core/test_ssa_runtime_objects.c:831；tests/core/test_ssa_runtime_objects.c:441；tests/core/test_ssa_runtime_objects.c:442；tests/core/test_ssa_runtime_objects.c:444；tests/core/test_ssa_runtime_objects.c:445；tests/core/ssa_runtime_objects_concurrency.c:270；tests/core/ssa_runtime_objects_concurrency.c:277；tests/core/ssa_runtime_objects_concurrency.c:292；tests/core/ssa_runtime_objects_concurrency.c:293。
 */

/**
 * @fn void test_duplicate_field_destination_fails_before_allocating_objects(void)
 *
 * ## Summary
 * 检查两个配方字段映射到同一 descriptor 的拒绝。实际运行期断言：把fields[1].memberDescriptorIndex改成fields[0]；断言类型绑定拒绝和0对象申请；没有穷尽所有重复fieldIndex/名称分支。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 修改 fields[1].memberDescriptorIndex 与第0项重复；首拒绝是 types_valid 的目标 descriptor 唯一性检查，不是所有配方 identity 冲突。 只借用/修改 fixture 的字段绑定表；0对象替身计数不排除前置逻辑分析。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:448；tests/core/test_ssa_runtime_objects.c:832；tests/core/test_ssa_runtime_objects.c:450；tests/core/test_ssa_runtime_objects.c:451；tests/core/test_ssa_runtime_objects.c:452；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:347。
 */

/**
 * @fn void test_empty_graph_clears_published_count_without_allocating(void)
 *
 * ## Summary
 * 检查空 aggregate 集发布 count0 且不调用 GC 对象创建替身。实际运行期断言：先产出旧图，再把deopt aggregate count改0重建map；成功将target.count清零且0对象申请。不能称逻辑snapshot零native分配。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 先有旧图，修改 aggregate 数量并重建 stateMap 后调用；只断言 count0 与对象分配次数0，不声称整个调用零 malloc/calloc，也未检查旧槽字节被清零。 旧 GC 对象并非由 count 清零立即 C free；function 的旧 map 借用在 BuildStateMaps 成功后失效，request.map 已更新。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:455；tests/core/test_ssa_runtime_objects.c:833；tests/core/test_ssa_runtime_objects.c:456；tests/core/test_ssa_runtime_objects.c:458；tests/core/test_ssa_runtime_objects.c:460；tests/core/test_ssa_runtime_objects.c:461；tests/core/test_ssa_runtime_objects.c:462；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:684；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:687。
 */

/**
 * @fn void test_gc_on_failure_keeps_old_graph_and_source_values(void)
 *
 * ## Summary
 * 检查失败重建不把新整数源99覆盖到旧图73。实际运行期断言：旧图后改变源int73→99，再对象ordinal2 Throw+GC；旧图int仍73而源保持99，非整个runtime所有状态不变。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * assert_graph 仍期待旧字段73，并单独检查 values[1] 为99；只证明这两个观测，不把失败解释为所有源/堆字节不变。 OOM 可伴随 FullGC 和源指针更新；旧输出逻辑内容由现有根保护。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:465；tests/core/test_ssa_runtime_objects.c:834；tests/core/test_ssa_runtime_objects.c:466；tests/core/test_ssa_runtime_objects.c:469；tests/core/test_ssa_runtime_objects.c:472；tests/core/test_ssa_runtime_objects.c:470；tests/core/test_ssa_runtime_objects.c:471；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:725；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:730。
 */

/**
 * @fn void test_published_graph_survives_gc_from_caller_roots(void)
 *
 * ## Summary
 * 由调用方根帧保护已发布图并经历一次 FullGC。实际运行期断言：成功发布后caller显式注册两输出根并FullGC；重获prototype/源地址后assert_graph；未证明一定发生移动。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 两槽 root map 与实际输出数组绑定，GC 后重新取 prototype 和源对象指针再 assert_graph；不宣称根帧之外的未注册裸指针都有效或发生了移动。 frame/map/slots 在栈上，正常尾部 Pop；若中间断言失败 longjmp，局部 Pop 不保证执行，tearDown 不专门弹此帧。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:475；tests/core/test_ssa_runtime_objects.c:835；tests/core/test_ssa_runtime_objects.c:479；tests/core/test_ssa_runtime_objects.c:483；tests/core/test_ssa_runtime_objects.c:489；tests/core/test_ssa_runtime_objects.c:488；tests/core/test_ssa_runtime_objects.c:485。
 */

/**
 * @fn void test_non_field_and_missing_binding_are_rejected_before_heap_work(void)
 *
 * ## Summary
 * 分别拒绝非 FIELD descriptor 和找不到的 runtime type binding。实际运行期断言：先PROPERTY而非FIELD拒绝，再恢复FIELD并改typeToken造成无匹配binding；两次零对象申请并MATERIALIZATION_FAILED。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 两轮独立首拒绝分别在 types_valid 的 descriptor kind 检查、graph 的缺失 binding 检查；只观察各自诊断及0对象次数。 第二轮前恢复 descriptor.kind 再改变 binding.typeToken；不会把前一畸形条件混入后一归因。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:492；tests/core/test_ssa_runtime_objects.c:836；tests/core/test_ssa_runtime_objects.c:494；tests/core/test_ssa_runtime_objects.c:496；tests/core/test_ssa_runtime_objects.c:499；tests/core/test_ssa_runtime_objects.c:501；tests/core/test_ssa_runtime_objects.c:495；tests/core/test_ssa_runtime_objects.c:500；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:338；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:407。
 */

/**
 * @fn void test_insufficient_target_capacity_preserves_existing_graph(void)
 *
 * ## Summary
 * 检查既有 count2 与缩小 capacity1 的 request 早拒绝保留旧图。实际运行期断言：先成功使target.count2，再capacity1；在request_valid C224因count>capacity早退，未覆盖graph C387的aggregateCount容量分支；未断言diag分类。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 首拒绝为 request_valid 的 count>capacity 检查，早于 graph 的 aggregateCount>target.capacity；本例没有断言 diagnostic，不能记为 graph容量分支覆盖。 调用后先恢复 capacity2 再运行 assert_graph；目标地址及旧图仍由 fixture 管理。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:504；tests/core/test_ssa_runtime_objects.c:837；tests/core/test_ssa_runtime_objects.c:505；tests/core/test_ssa_runtime_objects.c:507；tests/core/test_ssa_runtime_objects.c:508；tests/core/test_ssa_runtime_objects.c:511；tests/core/test_ssa_runtime_objects.c:510；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:224；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:387。
 */

/**
 * @fn void test_large_cycle_and_repeated_preparation(void)
 *
 * ## Summary
 * 构造257节点配方环并连续三次准备以检查每条下一节点边。实际运行期断言：257节点环、每node4字段，连续3轮恢复；断言target.count和每node下一环边、root深度0；非任意规模或性能证明。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 每轮只验证 count257、字段0存在且指向下一个节点、rootdepth0；不检查每个标量字段、所有节点原型或不同轮次对象地址独立性。配方环允许，未测试 prototype 继承环拒绝。 大输出槽和 deopt recipe 数组由本例/全局 function 持有；正常尾部 free大输出并复原 target，Unity 早退可跳过尾部，tearDown 只按当前 target 处理。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:514；tests/core/test_ssa_runtime_objects.c:838；tests/core/test_ssa_runtime_objects.c:518；tests/core/test_ssa_runtime_objects.c:525；tests/core/test_ssa_runtime_objects.c:526；tests/core/test_ssa_runtime_objects.c:547；tests/core/test_ssa_runtime_objects.c:550；tests/core/test_ssa_runtime_objects.c:551；tests/core/test_ssa_runtime_objects.c:558；tests/core/test_ssa_runtime_objects.c:559；tests/core/test_ssa_runtime_objects.c:561；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:550；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:618。
 */

/**
 * @fn void test_each_native_preparation_allocation_failure_preserves_target(void)
 *
 * ## Summary
 * 按序故障化文本 include 实例的 native calloc 并检查失败旧图保留。实际运行期断言：ignored源确保snapshot非空；native calloc ordinal1..64至首个未注入成功，已注入则旧输出字节和ignored注册保持；只扫当前C6个工作数组，不扫MaterializeState/analysis allocator。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 循环上限64且遇首次未注入的成功即停止；只控制 included Objects C 的直接 calloc（五个必需 work 池和可选 ignored 快照），linked logical/Owner/Liveness/Hash/GC 内部 allocator 不在此宏范围；不能由循环上限或静态源码遍历外推精确注入次数。 旧图与被 ignored 的源先建立；每轮 reset 后指定 ordinal，失败保持原槽/ignored membership；普通库入口用于先建立旧图。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:569；tests/core/test_ssa_runtime_objects.c:839；tests/core/test_ssa_runtime_objects.c:573；tests/core/test_ssa_runtime_objects.c:574；tests/core/test_ssa_runtime_objects.c:582；tests/core/test_ssa_runtime_objects.c:586；tests/core/test_ssa_runtime_objects.c:588；tests/core/test_ssa_runtime_objects.c:589；tests/core/test_ssa_runtime_objects.c:591；tests/core/test_ssa_runtime_objects.c:592；tests/core/test_ssa_runtime_objects.c:587；tests/core/test_ssa_runtime_objects.c:593；tests/core/ssa_runtime_objects_faults.c:63；tests/core/ssa_runtime_objects_faults.c:130；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:444；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:451。
 */

/**
 * @fn void test_owner_transfer_is_rejected_without_changing_the_source(void)
 *
 * ## Summary
 * 拒绝运行时 source value 的 UNIQUE ownershipKind 并保留其字节。实际运行期断言：改源单元ownershipKind为UNIQUE而ExecIR ownership仍原值；断言源字节不变、失败和零对象申请；不证明ExecIR UNIQUE/SHARED/BORROWED所有分支。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 本例仅把 values[0].ownershipKind 改为 UNIQUE；ExecIR value 仍为 GC。拒绝于运行时 plain value 验证，不能计为所有 ExecIR UNIQUE/SHARED/BORROWED 分支测试。 原值按字节保存，检查后正常路径还原 ownershipKind；中途 assert 早退可跳过还原，tearDown 仍只销毁 state/function。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:596；tests/core/test_ssa_runtime_objects.c:840；tests/core/test_ssa_runtime_objects.c:602；tests/core/test_ssa_runtime_objects.c:604；tests/core/test_ssa_runtime_objects.c:606；tests/core/test_ssa_runtime_objects.c:605；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:278；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:419。
 */

/**
 * @fn void test_restoration_does_not_invoke_constructor_or_field_accessors(void)
 *
 * ## Summary
 * 安装计数 native callable 到构造器和字段访问钩子并检查恢复不触发它们。实际运行期断言：constructor/initializer/getter/setter共用计数回调；恢复成功且callbackCount0；不代表所有可能用户回调路径均有测试。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 仅观察这一个 callable 在 constructor/initializer/getter/setter 四个入口的计数仍0；不证明所有潜在语言 callback 都被穷尽。 closure 被标记 permanent，原型修改前 Ignore/之后 Unignore；callbackState 在 unexpected_callback 中未读取。runtime state 管理这些 GC 对象。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:609；tests/core/test_ssa_runtime_objects.c:841；tests/core/test_ssa_runtime_objects.c:612；tests/core/test_ssa_runtime_objects.c:615；tests/core/test_ssa_runtime_objects.c:626；tests/core/test_ssa_runtime_objects.c:628；tests/core/test_ssa_runtime_objects.c:627；tests/core/test_ssa_runtime_objects.c:616；tests/core/test_ssa_runtime_objects.c:619；tests/core/test_ssa_runtime_objects.c:620；tests/core/test_ssa_runtime_objects.c:622；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:622。
 */

/**
 * @fn void test_resource_prototype_rejects_before_object_allocation(void)
 *
 * ## Summary
 * 拒绝 RESOURCE 类型原型作为重建目标。实际运行期断言：绑定原型加RESOURCE modifier；断言拒绝和零对象申请。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 只设置当前 prototype 的 RESOURCE modifier 并观察0对象创建和物化失败诊断；不测试其全部祖先、各类 modifier 或继承环。 修改由本例 fixture state 拥有的原型；不创建 owner handle。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:631；tests/core/test_ssa_runtime_objects.c:842；tests/core/test_ssa_runtime_objects.c:633；tests/core/test_ssa_runtime_objects.c:635；tests/core/test_ssa_runtime_objects.c:634；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:296。
 */

/**
 * @fn void test_resource_source_without_owner_handle_is_rejected(void)
 *
 * ## Summary
 * 拒绝缺少 owner handle 的 resource source 对象。实际运行期断言：构造resource对象源但ownershipKindNONE/lifecycleNONE；断言拒绝和零对象申请，覆盖plain_value的resource身份限制。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 源对象由 RESOURCE class prototype 建立，先断言 ownershipKind=NONE/lifecycle=NONE，再因 plain runtime source 验证拒绝；不证明转移/释放 resource owner 的执行。 新原型与源对象由 state 管理；未对该 resource 执行用户析构或 owner transfer。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:638；tests/core/test_ssa_runtime_objects.c:843；tests/core/test_ssa_runtime_objects.c:641；tests/core/test_ssa_runtime_objects.c:645；tests/core/test_ssa_runtime_objects.c:646；tests/core/test_ssa_runtime_objects.c:647；tests/core/test_ssa_runtime_objects.c:649；tests/core/test_ssa_runtime_objects.c:648；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:282；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:419。
 */

/**
 * @fn void test_output_storage_cannot_overlap_source_value_payloads(void)
 *
 * ## Summary
 * 检查输出槽不得覆盖源 SZrTypeValue payload 存储。实际运行期断言：target.objects指到values[0].payload；恢复target指针后断言false、整个values字节不变、0对象申请/root深度。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 输出临时指向 values[0].value.object，首拒绝是 request 的 value-array alias 检查；比较全部3个 value字节、0对象次数和rootdepth，不外推任意地址合法性认证。 必须在任何断言前恢复 target.objects=objects，避免 tearDown 把源静态数组误当 malloc 输出释放。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:652；tests/core/test_ssa_runtime_objects.c:844；tests/core/test_ssa_runtime_objects.c:659；tests/core/test_ssa_runtime_objects.c:660；tests/core/test_ssa_runtime_objects.c:661；tests/core/test_ssa_runtime_objects.c:662；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:235。
 */

/**
 * @fn void test_existing_ignore_registrations_survive_success_and_failure(void)
 *
 * ## Summary
 * 检查既有 prototype/name/source ignored 注册在成功与 OOM+GC 后仍存在。实际运行期断言：预ignore原型/一个名称/源对象；成功与Throw+GC失败后检查ignored计数及三者成员资格、旧图。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 只比较 ignored 总数和三个成员存在性，不比较顺序、全部忽略对象、所有标志或底层数组字节。 本例 Ignore 的 GC 对象随 state 销毁；物化应恢复其先前 membership，测试不自行 C free。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:665；tests/core/test_ssa_runtime_objects.c:845；tests/core/test_ssa_runtime_objects.c:669；tests/core/test_ssa_runtime_objects.c:670；tests/core/test_ssa_runtime_objects.c:671；tests/core/test_ssa_runtime_objects.c:673；tests/core/test_ssa_runtime_objects.c:674；tests/core/test_ssa_runtime_objects.c:675；tests/core/test_ssa_runtime_objects.c:676；tests/core/test_ssa_runtime_objects.c:677；tests/core/test_ssa_runtime_objects.c:679；tests/core/test_ssa_runtime_objects.c:681；tests/core/test_ssa_runtime_objects.c:682；tests/core/test_ssa_runtime_objects.c:684；tests/core/test_ssa_runtime_objects.c:686；tests/core/test_ssa_runtime_objects.c:680；tests/core/test_ssa_runtime_objects.c:687；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:451；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:528。
 */

/**
 * @fn void test_output_storage_cannot_overlap_ir_or_prototype_metadata(void)
 *
 * ## Summary
 * 检查四种 IR/map/prototype 元数据存储不能充当发布槽。实际运行期断言：依次输出指向deoptAggregateFields、stateMap.valuePool、prototype、memberDescriptors四处；比较每处前sizeof(objects)字节和0对象申请/root深度；未穷尽16池/所有pointee。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 四轮仅覆盖 deoptAggregateFields、stateMap.valuePool、prototype记录、memberDescriptors；前两类由 request/IR disjoint 检查拒绝，后两类由 graph/types 的 prototype 区域检查拒绝。仅比较 sizeof(objects) 前缀，不证明所有元数据数组都被测试。 每轮在断言前恢复 target.objects，防止 tearDown 错误释放借用元数据；不真正经该指针写出对象。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:690；tests/core/test_ssa_runtime_objects.c:846；tests/core/test_ssa_runtime_objects.c:701；tests/core/test_ssa_runtime_objects.c:702；tests/core/test_ssa_runtime_objects.c:703；tests/core/test_ssa_runtime_objects.c:704；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:242；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:323；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:324。
 */

/**
 * @fn void test_reference_class_graph_preserves_identity(void)
 *
 * ## Summary
 * 将结构原型换为 CLASS 后检查引用图的别名与回边。实际运行期断言：把绑定换CLASS原型并复制4descriptor；断言OBJECT tag和重复边/回边身份，仅有限字段。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 只断言 count2、第0节点OBJECT tag及三条指定边；不复用 assert_graph 的 STRUCT tag，也不检查所有字段值/所有节点原型。 4个 descriptor 从原型复制到新 class，binding 更新 prototype/layoutGeneration；GC对象仍由 state 管理。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:708；tests/core/test_ssa_runtime_objects.c:847；tests/core/test_ssa_runtime_objects.c:712；tests/core/test_ssa_runtime_objects.c:714；tests/core/test_ssa_runtime_objects.c:719；tests/core/test_ssa_runtime_objects.c:720；tests/core/test_ssa_runtime_objects.c:721；tests/core/test_ssa_runtime_objects.c:722；tests/core/test_ssa_runtime_objects.c:723；tests/core/test_ssa_runtime_objects.c:724；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:593；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:618。
 */

/**
 * @fn void test_inactive_cleanup_payload_is_neither_read_nor_rooted(void)
 *
 * ## Summary
 * 检查已消费 owner 的非配方槽可含毒化字节而不参与 live 校验与根注册。实际运行期断言：第4个owner经DROP被具体witness标DROPPED且payload填0xff；realGC恢复成功，有限图/owner未改/2次对象申请；该poisoned值没有被recipe VALUE引用。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 第4值为已 DROP 的 UNIQUE owner，witness 为DROPPED，extended[3]全0xff；它未被任何 VALUE 配方引用。仅证明 inactive live/root 遍历先看 owner，再读取 payload/tag，不能宣称配方 VALUE 分支永不读取该槽或任意 invalid payload 都可接受。 局部 owners/extended 在同步调用期间有效；正常路径把可能经 GC 更新的前三值复制回全局后 assert_graph；Unity 早退可能跳过该复制。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:727；tests/core/test_ssa_runtime_objects.c:848；tests/core/test_ssa_runtime_objects.c:738；tests/core/test_ssa_runtime_objects.c:739；tests/core/test_ssa_runtime_objects.c:746；tests/core/test_ssa_runtime_objects.c:748；tests/core/test_ssa_runtime_objects.c:750；tests/core/test_ssa_runtime_objects.c:752；tests/core/test_ssa_runtime_objects.c:753；tests/core/test_ssa_runtime_objects.c:760；tests/core/test_ssa_runtime_objects.c:762；tests/core/test_ssa_runtime_objects.c:764；tests/core/test_ssa_runtime_objects.c:768；tests/core/test_ssa_runtime_objects.c:770；tests/core/test_ssa_runtime_objects.c:773；tests/core/test_ssa_runtime_objects.c:784；tests/core/test_ssa_runtime_objects.c:787；tests/core/test_ssa_runtime_objects.c:788；tests/core/test_ssa_runtime_objects.c:786；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:393；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:415；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:494。
 */

/**
 * @fn void test_output_storage_cannot_overwrite_concrete_owner_witness(void)
 *
 * ## Summary
 * 检查输出槽不得覆盖 concrete owner witness。实际运行期断言：target.objects覆盖3项owner witness；公开入口INVALID_ARGUMENT，count0及整个witness字节未变；在request_valid早退，不覆盖resolver witness执行。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；场景断言满足则正常返回。断言失败由Unity的longjmp退出当前case受保护正文，runner随后尝试tearDown并继续后续case；最终失败数由main的UNITY_END返回。
 *
 * ## Constraints and lifecycle
 * 公开入口的 request_valid ownerStates alias 检查先拒绝；断言 INVALID_ARGUMENT、count0及四个uint32字节保留，不记为 resolver 成功消费该 witness 的覆盖。 局部 union 提供 owner槽和对象指针对齐；拒绝后先恢复 target.objects 再断言，避免 tearDown free 栈地址。 各case依赖setUp建立的独立fixture，正常退出由tearDown释放全局function/state；Unity早退不会自动执行局部case尾部，不据此承诺所有局部根/作用域/临时native资源均已平衡。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:791；tests/core/test_ssa_runtime_objects.c:849；tests/core/test_ssa_runtime_objects.c:807；tests/core/test_ssa_runtime_objects.c:808；tests/core/test_ssa_runtime_objects.c:809；tests/core/test_ssa_runtime_objects.c:810；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:232。
 */

/**
 * @anchor zr_test_runtime_objects_owner_witness_anonymous_union
 *
 * ## Summary
 * 为owner witness别名拒绝用例提供同时容纳四个uint32和两个对象指针的局部对齐存储。
 *
 * ## Fields and invariants
 * owners[4]保存具体状态及尾部比较字节；alignment[2]仅提供对象指针对齐/足够存储，未被读取。
 *
 * ## Constraints and lifecycle
 * 匿名union仅在test791的栈作用域内存在；owners是初始化/读取的成员，alignment成员只提供尺寸和对齐，不读取未激活成员，也不真实向其中写SZrObject指针。request只声明前三项owner witness，尾部第4项参与整体字节比较；target.capacity仍为2。将其地址转为target.objects仅用于request alias早拒绝，调用后在断言前恢复正常输出槽；没有对象所有权或C free责任。局部存储不会跨同步调用或case返回存活，不能据此证明resolver已接受witness。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:792；tests/core/test_ssa_runtime_objects.c:800；tests/core/test_ssa_runtime_objects.c:801；tests/core/test_ssa_runtime_objects.c:803；tests/core/test_ssa_runtime_objects.c:810；tests/core/test_ssa_runtime_objects.c:802；tests/core/test_ssa_runtime_objects.c:806；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:232。
 */

/**
 * @fn int main(void)
 *
 * ## Summary
 * 注册并顺序运行本TU定义的35个Objects运行期测试，返回Unity记录的失败数。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * @return UNITY_END的整数结果；当前UnityEnd返回累计失败数，0只表示实际运行中没有记录测试失败。源码审查本身不构成一次执行。
 *
 * ## Constraints and lifecycle
 * UNITY_BEGIN后35个RUN_TEST都指向本TU的静态case，没有文本include其它case定义；fault/concurrency头仅提供接口。RUN_TEST把函数指针交UnityDefaultTestRun，按setUp、case、tearDown受保护阶段派发；断言longjmp只退出当前阶段，后续case仍继续。并发helper无法安全join时显式abort可终止进程，区别于普通assert早退。main自身不替case清理局部root frame/作用域；所有fixture及故障计数须串行使用。
 *
 * ## References
 * tests/core/test_ssa_runtime_objects.c:813；tests/core/test_ssa_runtime_objects.c:5；tests/core/test_ssa_runtime_objects.c:6；tests/core/test_ssa_runtime_objects.c:814；tests/core/test_ssa_runtime_objects.c:815；tests/core/test_ssa_runtime_objects.c:849；tests/core/test_ssa_runtime_objects.c:850；tests/third_party/zr_unity/Unity/src/unity_internals.h:910；tests/third_party/zr_unity/Unity/src/unity.c:2298；tests/third_party/zr_unity/Unity/src/unity.c:2299；tests/third_party/zr_unity/Unity/src/unity.c:2303；tests/third_party/zr_unity/Unity/src/unity.c:2359；tests/cmake/ssa-tests.cmake:304；tests/cmake/ssa-tests.cmake:324。
 */
