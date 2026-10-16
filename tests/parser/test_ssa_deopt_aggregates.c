#include "unity.h"
#include "ssa_deopt_aggregate_fault_allocator.h"

#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_parser/exec_ir_state_maps.h"
#include "zr_vm_parser/exec_ir_pass_manager.h"
#include "exec_ir/exec_ir_pass_internal.h"
#include "exec_ir/exec_ir_mutation_internal.h"

static SZrExecIrFunction function;
static SZrExecIrDiagnostic diagnostic;
static SZrExecIrMaterializedState target;

void setUp(void) {
    SZrExecIrInstruction instruction = {0};
    ssa_deopt_aggregate_fail_allocation(0u);
    ZrCore_ExecIr_FunctionInit(&function);
    ZrCore_ExecIr_MaterializedStateInit(&target);
    function.id = 1u;
    function.functionToken = 99u;
    function.signatureHash = 123u;
    function.contract.generation = 7u;
    TEST_ASSERT_EQUAL_UINT32(1u, ZrCore_ExecIr_FunctionAddExternalValue(
            &function, 1u, ZR_EXEC_IR_OWNERSHIP_GC,
            ZR_EXEC_IR_NULLABILITY_NULLABLE));
    instruction.opcode = ZR_EXEC_IR_OPCODE_NOP;
    instruction.flags = ZR_EXEC_IR_FLAG_MAY_GC;
    instruction.sourceId = 42u;
    instruction.deoptId = 91u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(
            &function, &instruction, NULL));
    function.deoptStates = calloc(1u, sizeof(*function.deoptStates));
    function.deoptAggregates = calloc(3u, sizeof(*function.deoptAggregates));
    function.deoptAggregateFields = calloc(6u, sizeof(*function.deoptAggregateFields));
    TEST_ASSERT_NOT_NULL(function.deoptStates);
    TEST_ASSERT_NOT_NULL(function.deoptAggregates);
    TEST_ASSERT_NOT_NULL(function.deoptAggregateFields);
    function.deoptStateCount = function.deoptStateCapacity = 1u;
    function.deoptAggregateCount = 2u;
    function.deoptAggregateCapacity = 3u;
    function.deoptAggregateFieldCount = 5u;
    function.deoptAggregateFieldCapacity = 6u;
    function.deoptStates[0].id = 91u;
    function.deoptStates[0].sourceId = 42u;
    function.deoptStates[0].resumeId = 701u;
    function.deoptStates[0].aggregates.count = 2u;
    /* A has two aliases to B and an uninitialized field. B refers to A and
     * to the only runtime SSA value, which no ordinary instruction uses. */
    function.deoptAggregates[0].identityId = 11u;
    function.deoptAggregates[0].typeToken = 101u;
    function.deoptAggregates[0].layoutId = 201u;
    function.deoptAggregates[0].fields.count = 3u;
    function.deoptAggregates[1].identityId = 22u;
    function.deoptAggregates[1].typeToken = 102u;
    function.deoptAggregates[1].layoutId = 202u;
    function.deoptAggregates[1].fields.start = 3u;
    function.deoptAggregates[1].fields.count = 2u;
    function.deoptAggregateFields[0].kind = ZR_EXEC_IR_DEOPT_FIELD_AGGREGATE;
    function.deoptAggregateFields[0].aggregateId = 22u;
    function.deoptAggregateFields[1] = function.deoptAggregateFields[0];
    function.deoptAggregateFields[1].fieldIndex = 1u;
    function.deoptAggregateFields[2].fieldIndex = 2u;
    function.deoptAggregateFields[3].kind = ZR_EXEC_IR_DEOPT_FIELD_AGGREGATE;
    function.deoptAggregateFields[3].aggregateId = 11u;
    function.deoptAggregateFields[4].fieldIndex = 1u;
    function.deoptAggregateFields[4].kind = ZR_EXEC_IR_DEOPT_FIELD_VALUE;
    function.deoptAggregateFields[4].valueId = 1u;
}

void tearDown(void) {
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
}

static TZrBool materialize_from_map(const SZrExecIrStateMap *map) {
    SZrExecIrResumeRequest request = {0};
    request.function = &function;
    request.map = map;
    request.functionToken = function.functionToken;
    request.generation = function.contract.generation;
    request.signatureHash = function.signatureHash;
    request.sourceId = 42u;
    request.resumeId = 701u;
    request.phase = ZR_EXEC_IR_STATE_AFTER_EFFECT;
    request.target = &target;
    return ZrCore_ExecIr_MaterializeState(&request, &diagnostic);
}

static TZrBool materialize(void) {
    return materialize_from_map(function.stateMap);
}

static void build(void) {
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
}

static void assert_location(void) {
    TEST_ASSERT_EQUAL_UINT32(99u, diagnostic.functionToken);
    TEST_ASSERT_EQUAL_UINT32(42u, diagnostic.sourceId);
    TEST_ASSERT_EQUAL_UINT32(1u, diagnostic.instructionId);
}

static void assert_invalid(EZrExecutionDiagnosticCode code) {
    TEST_ASSERT_FALSE(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    TEST_ASSERT_EQUAL(code, diagnostic.code);
    assert_location();
}

static void test_fields_feed_checkpoint_liveness_and_roots(void) {
    const SZrExecIrStateMapEntry *entry;
    build();
    entry = ZrCore_ExecIr_StateMapFind(function.stateMap, 42u, 701u,
                                     ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT32(1u, entry->liveValues.count);
    TEST_ASSERT_EQUAL_UINT32(1u, entry->rootValues.count);
    TEST_ASSERT_EQUAL_UINT32(1u, function.stateMap->valuePool[entry->liveValues.start]);
    TEST_ASSERT_EQUAL_UINT32(1u, function.stateMap->rootPool[entry->rootValues.start]);
    TEST_ASSERT_TRUE(materialize());
    TEST_ASSERT_EQUAL_UINT32(1u, target.valueCount);
    TEST_ASSERT_EQUAL_UINT32(1u, target.rootCount);
}

static void test_materialized_graph_preserves_alias_cycle_and_uninitialized_field(void) {
    build();
    TEST_ASSERT_TRUE(materialize());
    TEST_ASSERT_EQUAL_UINT32(2u, target.aggregateCount);
    TEST_ASSERT_EQUAL_UINT32(5u, target.aggregateFieldCount);
    TEST_ASSERT_TRUE(target.aggregates != function.deoptAggregates);
    TEST_ASSERT_TRUE(target.aggregateFields != function.deoptAggregateFields);
    TEST_ASSERT_EQUAL_UINT32(11u, target.aggregates[0].identityId);
    TEST_ASSERT_EQUAL_UINT32(22u, target.aggregates[1].identityId);
    TEST_ASSERT_EQUAL_UINT32(22u, target.aggregateFields[0].aggregateId);
    TEST_ASSERT_EQUAL_UINT32(22u, target.aggregateFields[1].aggregateId);
    TEST_ASSERT_EQUAL_UINT32(11u, target.aggregateFields[3].aggregateId);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DEOPT_FIELD_UNINITIALIZED, target.aggregateFields[2].kind);
    TEST_ASSERT_EQUAL_UINT32(1u, target.aggregateFields[4].valueId);
    function.deoptAggregateFields[4].valueId = 0u;
    TEST_ASSERT_EQUAL_UINT32(1u, target.aggregateFields[4].valueId);
}

static void test_ranges_reject_capacity_and_wrap_before_analysis(void) {
    function.deoptStates[0].aggregates.count = 3u;
    assert_invalid(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    function.deoptStates[0].aggregates.count = 2u;
    function.deoptAggregates[1].fields.start = UINT32_MAX;
    assert_invalid(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
}

static void test_fields_reject_invalid_value_and_enum(void) {
    function.deoptAggregateFields[4].valueId = 2u;
    assert_invalid(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
    function.deoptAggregateFields[4].valueId = 1u;
    function.deoptAggregateFields[4].kind = (EZrExecIrDeoptFieldKind)-1;
    assert_invalid(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID);
}

static void test_graph_rejects_missing_identity(void) {
    function.deoptAggregateFields[1].aggregateId = 33u;
    assert_invalid(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID);
}

static void test_uninitialized_field_cannot_hide_a_value(void) {
    function.deoptAggregateFields[2].valueId = 1u;
    assert_invalid(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID);
}

static void test_clone_owns_recipe_arrays(void) {
    SZrExecIrFunction clone;
    ZrCore_ExecIr_FunctionInit(&clone);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_CloneFunction(&function, &clone, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(2u, clone.deoptAggregateCount);
    TEST_ASSERT_EQUAL_UINT32(5u, clone.deoptAggregateFieldCount);
    TEST_ASSERT_TRUE(clone.deoptAggregates != function.deoptAggregates);
    TEST_ASSERT_TRUE(clone.deoptAggregateFields != function.deoptAggregateFields);
    TEST_ASSERT_EQUAL_UINT32(22u, clone.deoptAggregateFields[0].aggregateId);
    ZrCore_ExecIr_FreeFunction(&clone);
    TEST_ASSERT_EQUAL_UINT32(22u, function.deoptAggregateFields[0].aggregateId);
}

static void test_borrowed_aggregate_field_rejects_suspend_boundary(void) {
    function.values[0].ownership = ZR_EXEC_IR_OWNERSHIP_BORROWED;
    function.instructions[0].flags |= ZR_EXEC_IR_FLAG_MAY_SUSPEND;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_BORROWED_ACROSS_SUSPEND, diagnostic.code);
    assert_location();
}

static void test_unavailable_aggregate_field_is_not_silently_omitted(void) {
    TEST_ASSERT_EQUAL_UINT32(2u, ZrCore_ExecIr_FunctionAddValue(
            &function, 1u, ZR_EXEC_IR_OWNERSHIP_GC,
            ZR_EXEC_IR_NULLABILITY_NULLABLE));
    function.deoptAggregateFields[4].valueId = 2u;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    assert_location();
}

static void test_failed_rebuild_and_consumer_preserve_published_state(void) {
    SZrExecIrStateMap *published;
    SZrExecIrMaterializedState old;
    build();
    TEST_ASSERT_TRUE(materialize());
    TEST_ASSERT_EQUAL_UINT32(2u, target.aggregateCount);
    old = target;
    published = function.stateMap;
    function.deoptAggregateFields[4].valueId = 9u;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, diagnostic.code);
    assert_location();
    TEST_ASSERT_EQUAL_PTR(published, function.stateMap);
    TEST_ASSERT_FALSE(materialize());
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, diagnostic.code);
    assert_location();
    TEST_ASSERT_EQUAL_MEMORY(&old, &target, sizeof(target));
}

static void test_duplicate_identity_and_field_are_rejected(void) {
    function.deoptAggregates[1].identityId = 11u;
    assert_invalid(ZR_EXEC_IR_DIAGNOSTIC_DUPLICATE_DEFINITION);
    function.deoptAggregates[1].identityId = 22u;
    function.deoptAggregateFields[1].fieldIndex = 0u;
    assert_invalid(ZR_EXEC_IR_DIAGNOSTIC_DUPLICATE_DEFINITION);
}

static void test_selected_graph_cannot_reference_another_states_object(void) {
    function.deoptStates[0].aggregates.count = 1u;
    assert_invalid(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID);
}

static void test_selected_graph_is_compact_and_field_ranges_are_rebased(void) {
    function.deoptAggregateCount = 3u;
    function.deoptAggregates[2] = function.deoptAggregates[1];
    function.deoptAggregates[2].identityId = 33u;
    function.deoptAggregates[2].fields.start = 4u;
    function.deoptAggregates[2].fields.count = 1u;
    function.deoptAggregateFields[4].fieldIndex = 87u;
    function.deoptStates[0].aggregates.start = 2u;
    function.deoptStates[0].aggregates.count = 1u;
    build();
    TEST_ASSERT_TRUE(materialize());
    TEST_ASSERT_EQUAL_UINT32(1u, target.aggregateCount);
    TEST_ASSERT_EQUAL_UINT32(1u, target.aggregateFieldCount);
    TEST_ASSERT_EQUAL_UINT32(33u, target.aggregates[0].identityId);
    TEST_ASSERT_EQUAL_UINT32(0u, target.aggregates[0].fields.start);
    TEST_ASSERT_EQUAL_UINT32(1u, target.aggregates[0].fields.count);
    TEST_ASSERT_EQUAL_UINT32(87u, target.aggregateFields[0].fieldIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, target.aggregateFields[0].valueId);
}

static void test_unreferenced_state_does_not_keep_fields_live(void) {
    function.instructions[0].deoptId = 0u;
    build();
    TEST_ASSERT_EQUAL_UINT32(0u, function.stateMap->entries[0].liveValues.count);
    TEST_ASSERT_EQUAL_UINT32(0u, function.stateMap->entries[0].rootValues.count);
    function.deoptStates[0].aggregates.count = 3u;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(42u, diagnostic.sourceId);
    TEST_ASSERT_EQUAL_UINT32(0u, diagnostic.instructionId);
}

static void test_positive_unknown_kind_and_conflicting_ids_are_rejected(void) {
    function.deoptAggregateFields[4].kind = ZR_EXEC_IR_DEOPT_FIELD_KIND_COUNT;
    assert_invalid(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID);
    function.deoptAggregateFields[4].kind = ZR_EXEC_IR_DEOPT_FIELD_VALUE;
    function.deoptAggregateFields[4].aggregateId = 11u;
    assert_invalid(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID);
    function.deoptAggregateFields[4].aggregateId = 0u;
    function.deoptAggregateFields[0].valueId = 1u;
    assert_invalid(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID);
}

static void test_consumer_rejects_omitted_field_live_value(void) {
    SZrExecIrMaterializedState old;
    TZrUInt32 index;
    build();
    TEST_ASSERT_TRUE(materialize());
    old = target;
    for (index = 0u; index < function.stateMap->entryCount; ++index) {
        function.stateMap->entries[index].liveValues.count = 0u;
        function.stateMap->entries[index].rootValues.count = 0u;
        function.stateMap->entries[index].ownerStates.count = 0u;
    }
    TEST_ASSERT_FALSE(materialize());
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    assert_location();
    TEST_ASSERT_EQUAL_MEMORY(&old, &target, sizeof(target));
}

static void test_target_cannot_alias_function_recipe_storage(void) {
    SZrExecIrDeoptAggregate *saved;
    TZrUInt32 savedCapacity;
    TZrBool ok;
    build();
    TEST_ASSERT_TRUE(materialize());
    saved = target.aggregates;
    savedCapacity = target.aggregateCapacity;
    target.aggregates = function.deoptAggregates;
    target.aggregateCapacity = function.deoptAggregateCapacity;
    ok = materialize();
    target.aggregates = saved;
    target.aggregateCapacity = savedCapacity;
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED, diagnostic.code);
}

static void test_empty_graph_replaces_previous_prepared_graph(void) {
    build();
    TEST_ASSERT_TRUE(materialize());
    TEST_ASSERT_EQUAL_UINT32(2u, target.aggregateCount);
    function.deoptStates[0].aggregates.start = function.deoptAggregateCount;
    function.deoptStates[0].aggregates.count = 0u;
    build();
    TEST_ASSERT_TRUE(materialize());
    TEST_ASSERT_EQUAL_UINT32(0u, target.aggregateCount);
    TEST_ASSERT_EQUAL_UINT32(0u, target.aggregateFieldCount);
    TEST_ASSERT_NULL(target.aggregates);
    TEST_ASSERT_NULL(target.aggregateFields);
}

/* Detach rejected borrowed storage so teardown remains safe on both the red
 * and green paths. A buggy successful commit has already freed that storage. */
static TZrBool try_borrowed_target(const SZrExecIrStateMap *map, void *storage) {
    TZrBool ok;
    target.values = storage;
    target.valueCount = target.valueCapacity = 1u;
    ok = materialize_from_map(map);
    if (!ok) {
        target.values = ZR_NULL;
        target.valueCount = target.valueCapacity = 0u;
    }
    return ok;
}

static void test_target_cannot_own_function_frame_slots(void) {
    TZrBool ok;
    build();
    function.frameLayout = calloc(1u, sizeof(*function.frameLayout));
    TEST_ASSERT_NOT_NULL(function.frameLayout);
    function.frameLayout->slots = calloc(1u, sizeof(*function.frameLayout->slots));
    TEST_ASSERT_NOT_NULL(function.frameLayout->slots);
    function.frameLayout->slotCount = function.frameLayout->slotCapacity = 1u;
    ok = try_borrowed_target(function.stateMap, function.frameLayout->slots);
    if (ok) function.frameLayout->slots = ZR_NULL;
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED, diagnostic.code);
    TEST_ASSERT_NOT_NULL(function.frameLayout->slots);
}

static void test_target_cannot_own_function_gc_slots(void) {
    TZrBool ok;
    build();
    function.gcMap = calloc(1u, sizeof(*function.gcMap));
    TEST_ASSERT_NOT_NULL(function.gcMap);
    function.gcMap->slotIndexPool = calloc(1u, sizeof(*function.gcMap->slotIndexPool));
    TEST_ASSERT_NOT_NULL(function.gcMap->slotIndexPool);
    function.gcMap->slotIndexCount = function.gcMap->slotIndexCapacity = 1u;
    ok = try_borrowed_target(function.stateMap, function.gcMap->slotIndexPool);
    if (ok) function.gcMap->slotIndexPool = ZR_NULL;
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED, diagnostic.code);
    TEST_ASSERT_NOT_NULL(function.gcMap->slotIndexPool);
}

static void test_target_cannot_own_function_map_when_using_external_map(void) {
    SZrExecIrStateMap source;
    TZrBool ok;
    build();
    ZrCore_ExecIr_StateMapInit(&source);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_StateMapClone(function.stateMap, &source));
    ok = try_borrowed_target(&source, function.stateMap->valuePool);
    if (ok) function.stateMap->valuePool = ZR_NULL;
    ZrCore_ExecIr_StateMapFree(&source);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED, diagnostic.code);
    TEST_ASSERT_NOT_NULL(function.stateMap->valuePool);
}

static void test_each_recipe_allocation_failure_preserves_old_roots_and_graph(void) {
    SZrExecIrMaterializedState old;
    size_t ordinal;
    build();
    TEST_ASSERT_TRUE(materialize());
    old = target;
    for (ordinal = 1u; ordinal <= 2u; ++ordinal) {
        ssa_deopt_aggregate_fail_allocation(ordinal);
        TEST_ASSERT_FALSE(materialize());
        TEST_ASSERT_TRUE(ssa_deopt_aggregate_allocation_failed());
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, diagnostic.code);
        assert_location();
        TEST_ASSERT_EQUAL_MEMORY(&old, &target, sizeof(target));
        TEST_ASSERT_EQUAL_UINT32(1u, target.roots[0]);
        TEST_ASSERT_EQUAL_UINT32(22u, target.aggregateFields[0].aggregateId);
    }
    ssa_deopt_aggregate_fail_allocation(3u);
    TEST_ASSERT_TRUE(materialize());
    TEST_ASSERT_FALSE(ssa_deopt_aggregate_allocation_failed());
    TEST_ASSERT_EQUAL_UINT32(2u, target.aggregateCount);
    TEST_ASSERT_EQUAL_UINT32(1u, target.roots[0]);
}

static void test_dce_keeps_definition_used_only_by_aggregate_recipe(void) {
    SZrExecIrInstruction copy = {0}, checkpointInstruction = function.instructions[0];
    SZrExecIrPassContext context = {0};
    TZrExecIrValueId input = 1u, result;
    TZrBool changed;
    result = ZrCore_ExecIr_FunctionAddValue(&function, 1u,
            ZR_EXEC_IR_OWNERSHIP_GC, ZR_EXEC_IR_NULLABILITY_NULLABLE);
    TEST_ASSERT_EQUAL_UINT32(2u, result);
    copy.opcode = ZR_EXEC_IR_OPCODE_COPY;
    copy.sourceId = 41u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendOperands(
            &function, &input, 1u, &copy.operandRange));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendResults(
            &function, &result, 1u, &copy.resultRange));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(&function, &copy, NULL));
    function.instructions[0] = copy;
    function.instructions[1] = checkpointInstruction;
    function.values[result - 1u].definition = 1u;
    function.deoptAggregateFields[4].valueId = result;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_RunDcePass(&function, &context, &changed, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_OPCODE_COPY, function.instructions[0].opcode);
    TEST_ASSERT_EQUAL_UINT32(1u, function.values[result - 1u].definition);
    build();
    TEST_ASSERT_TRUE(materialize());
    TEST_ASSERT_EQUAL_UINT32(result, target.aggregateFields[4].valueId);
    TEST_ASSERT_EQUAL_UINT32(result, target.values[0]);
}

static void test_function_hash_tracks_field_binding_layout_and_selected_graph(void) {
    TZrUInt64 original;
    TEST_ASSERT_EQUAL_UINT32(2u, ZrCore_ExecIr_FunctionAddExternalValue(
            &function, 1u, ZR_EXEC_IR_OWNERSHIP_GC,
            ZR_EXEC_IR_NULLABILITY_NULLABLE));
    original = ZrParser_ExecIr_FunctionHash(&function);
    TEST_ASSERT_NOT_EQUAL(0u, original);
    function.deoptAggregateFields[4].valueId = 2u;
    TEST_ASSERT_NOT_EQUAL(original, ZrParser_ExecIr_FunctionHash(&function));
    function.deoptAggregateFields[4].valueId = 1u;
    function.deoptAggregateFields[4].fieldIndex = 7u;
    TEST_ASSERT_NOT_EQUAL(original, ZrParser_ExecIr_FunctionHash(&function));
    function.deoptAggregateFields[4].fieldIndex = 1u;
    function.deoptAggregates[0].layoutId = 300u;
    TEST_ASSERT_NOT_EQUAL(original, ZrParser_ExecIr_FunctionHash(&function));
    function.deoptAggregates[0].layoutId = 201u;
    function.deoptAggregates[0].typeToken = 300u;
    TEST_ASSERT_NOT_EQUAL(original, ZrParser_ExecIr_FunctionHash(&function));
    function.deoptAggregates[0].typeToken = 101u;
    function.deoptStates[0].aggregates.count = 0u;
    TEST_ASSERT_NOT_EQUAL(original, ZrParser_ExecIr_FunctionHash(&function));
    function.deoptStates[0].aggregates.count = 2u;
    TEST_ASSERT_EQUAL_UINT64(original, ZrParser_ExecIr_FunctionHash(&function));
}

static void test_recipe_requires_identity_type_and_layout(void) {
    function.deoptAggregates[0].identityId = 0u;
    assert_invalid(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID);
    function.deoptAggregates[0].identityId = 11u;
    function.deoptAggregates[0].typeToken = 0u;
    assert_invalid(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID);
    function.deoptAggregates[0].typeToken = 101u;
    function.deoptAggregates[0].layoutId = 0u;
    assert_invalid(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID);
}

static void test_null_recipe_pool_is_rejected_before_dereference(void) {
    SZrExecIrDeoptAggregateField *saved = function.deoptAggregateFields;
    TZrBool ok;
    function.deoptAggregateFields = NULL;
    ok = ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic);
    function.deoptAggregateFields = saved;
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
}

static void test_large_cycle_repeated_preparation_preserves_one_root(void) {
    const TZrUInt32 count = 257u;
    TZrUInt32 index, repeat;
    free(function.deoptAggregates);
    free(function.deoptAggregateFields);
    function.deoptAggregates = calloc(count, sizeof(*function.deoptAggregates));
    function.deoptAggregateFields = calloc(count + 1u, sizeof(*function.deoptAggregateFields));
    TEST_ASSERT_NOT_NULL(function.deoptAggregates);
    TEST_ASSERT_NOT_NULL(function.deoptAggregateFields);
    function.deoptAggregateCount = function.deoptAggregateCapacity = count;
    function.deoptAggregateFieldCount = function.deoptAggregateFieldCapacity = count + 1u;
    function.deoptStates[0].aggregates.count = count;
    for (index = 0u; index < count; ++index) {
        function.deoptAggregates[index].identityId = index + 1u;
        function.deoptAggregates[index].typeToken = 101u;
        function.deoptAggregates[index].layoutId = 201u;
        function.deoptAggregates[index].fields.start = index;
        function.deoptAggregates[index].fields.count = 1u;
        function.deoptAggregateFields[index].kind = ZR_EXEC_IR_DEOPT_FIELD_AGGREGATE;
        function.deoptAggregateFields[index].aggregateId = (index + 1u) % count + 1u;
    }
    function.deoptAggregates[count - 1u].fields.count = 2u;
    function.deoptAggregateFields[count].fieldIndex = 1u;
    function.deoptAggregateFields[count].kind = ZR_EXEC_IR_DEOPT_FIELD_VALUE;
    function.deoptAggregateFields[count].valueId = 1u;
    build();
    for (repeat = 0u; repeat < 4u; ++repeat) {
        TEST_ASSERT_TRUE(materialize());
        TEST_ASSERT_EQUAL_UINT32(count, target.aggregateCount);
        TEST_ASSERT_EQUAL_UINT32(count + 1u, target.aggregateFieldCount);
        TEST_ASSERT_EQUAL_UINT32(1u, target.aggregateFields[count - 1u].aggregateId);
        TEST_ASSERT_EQUAL_UINT32(1u, target.rootCount);
        TEST_ASSERT_EQUAL_UINT32(1u, target.roots[0]);
    }
}

static void test_checkpoint_result_cannot_supply_its_before_effect_recipe(void) {
    SZrExecIrStateMap *published;
    TZrExecIrValueId input = 1u, result;
    build();
    published = function.stateMap;
    result = ZrCore_ExecIr_FunctionAddValue(&function, 1u,
            ZR_EXEC_IR_OWNERSHIP_GC, ZR_EXEC_IR_NULLABILITY_NULLABLE);
    function.instructions[0].opcode = ZR_EXEC_IR_OPCODE_COPY;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendOperands(
            &function, &input, 1u, &function.instructions[0].operandRange));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendResults(
            &function, &result, 1u, &function.instructions[0].resultRange));
    function.values[result - 1u].definition = 1u;
    function.deoptAggregateFields[4].valueId = result;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    TEST_ASSERT_FALSE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    assert_location();
    TEST_ASSERT_EQUAL_PTR(published, function.stateMap);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fields_feed_checkpoint_liveness_and_roots);
    RUN_TEST(test_materialized_graph_preserves_alias_cycle_and_uninitialized_field);
    RUN_TEST(test_ranges_reject_capacity_and_wrap_before_analysis);
    RUN_TEST(test_fields_reject_invalid_value_and_enum);
    RUN_TEST(test_graph_rejects_missing_identity);
    RUN_TEST(test_uninitialized_field_cannot_hide_a_value);
    RUN_TEST(test_clone_owns_recipe_arrays);
    RUN_TEST(test_borrowed_aggregate_field_rejects_suspend_boundary);
    RUN_TEST(test_unavailable_aggregate_field_is_not_silently_omitted);
    RUN_TEST(test_failed_rebuild_and_consumer_preserve_published_state);
    RUN_TEST(test_duplicate_identity_and_field_are_rejected);
    RUN_TEST(test_selected_graph_cannot_reference_another_states_object);
    RUN_TEST(test_selected_graph_is_compact_and_field_ranges_are_rebased);
    RUN_TEST(test_unreferenced_state_does_not_keep_fields_live);
    RUN_TEST(test_positive_unknown_kind_and_conflicting_ids_are_rejected);
    RUN_TEST(test_consumer_rejects_omitted_field_live_value);
    RUN_TEST(test_target_cannot_alias_function_recipe_storage);
    RUN_TEST(test_target_cannot_own_function_frame_slots);
    RUN_TEST(test_target_cannot_own_function_gc_slots);
    RUN_TEST(test_target_cannot_own_function_map_when_using_external_map);
    RUN_TEST(test_empty_graph_replaces_previous_prepared_graph);
    RUN_TEST(test_each_recipe_allocation_failure_preserves_old_roots_and_graph);
    RUN_TEST(test_dce_keeps_definition_used_only_by_aggregate_recipe);
    RUN_TEST(test_function_hash_tracks_field_binding_layout_and_selected_graph);
    RUN_TEST(test_recipe_requires_identity_type_and_layout);
    RUN_TEST(test_null_recipe_pool_is_rejected_before_dereference);
    RUN_TEST(test_large_cycle_repeated_preparation_preserves_one_root);
    RUN_TEST(test_checkpoint_result_cannot_supply_its_before_effect_recipe);
    return UNITY_END();
}

/**
 * @var static SZrExecIrFunction function
 *
 * ## Summary
 * 串行case共享的函数owner与恢复配方fixture。
 *
 * ## Constraints and lifecycle
 * setUp Init并拥有values/instructions/deoptStates/aggregate/field等动态池及后续stateMap；各case可在自身前提下修改。tearDown FreeFunction统一释放当前合法owner，浅拷贝published/old不复制所有权。池指针借用随重分配/替换/Free失效；静态记录地址本身仍在。禁止并发使用。
 * 失败/资源边界：Unity早退仅能由tearDown回收当前登记于全局function的owner，不能接管局部clone/map或凭空修复alias。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:13。
 * tests/parser/test_ssa_deopt_aggregates.c:20。
 * tests/parser/test_ssa_deopt_aggregates.c:35。
 * tests/parser/test_ssa_deopt_aggregates.c:75。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:175。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:177。
 */

/**
 * @var static SZrExecIrDiagnostic diagnostic
 *
 * ## Summary
 * 记录本fixture最近一次builder/verifier/materializer结果的共享诊断槽。
 *
 * ## Constraints and lifecycle
 * 静态初始为零，setUp未显式清零；各被调用入口按其合同清零/填充，不能假定成功helper都会清旧值。与function/target和池是独立记录，无动态owner，不延长被定位对象生命周期。由assert_invalid/assert_location及各case只读消费，串行。
 * 失败/资源边界：失败内容由实际首个拒绝入口决定，不按case名称推断下游分支或诊断值。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:14。
 * tests/parser/test_ssa_deopt_aggregates.c:89。
 * tests/parser/test_ssa_deopt_aggregates.c:97。
 * tests/parser/test_ssa_deopt_aggregates.c:101。
 * tests/parser/test_ssa_deopt_aggregates.c:109。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:30。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:674。
 */

/**
 * @var static SZrExecIrMaterializedState target
 *
 * ## Summary
 * 拥有逻辑materialized状态数组的共享输出记录。
 *
 * ## Constraints and lifecycle
 * setUp Init，成功MaterializeState释放旧池并发布新池；tearDown MaterializedStateFree释放当前独立owner并清零，记录地址仍有效。values/roots为逻辑ValueId数组，ownerStates/aggregate recipes各有自身编码，不是运行时值或GC保活对象。old=target只是观察快照，不可另Free；source借用测试须按其明确恢复/脱离路径操作。
 * 失败/资源边界：普通物化失败保留旧target；临时alias测试故意构造非法旧owner，不能据此承诺所有异常路径清理。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:15。
 * tests/parser/test_ssa_deopt_aggregates.c:21。
 * tests/parser/test_ssa_deopt_aggregates.c:74。
 * tests/parser/test_ssa_deopt_aggregates.c:88。
 * tests/parser/test_ssa_deopt_aggregates.c:209。
 * tests/parser/test_ssa_deopt_aggregates.c:331。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:813。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:818。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:819。
 */

/**
 * @fn void setUp(void)
 *
 * ## Summary
 * 在每个Unity case前建立同一函数身份、checkpoint和两对象别名/循环配方。
 *
 * ## Returns
 * 无C返回值；构造及断言完成则交给case；断言失败由Unity退出本case并尝试tearDown。
 *
 * ## Constraints and lifecycle
 * 串行重置recipe malloc故障计数为关闭；Init全局function/target，设id1/token99/signature123/generation7、external GC value1、MAY_GC NOP(source42/deopt91)。分配1state/3aggregate容量/6field容量，逻辑用2对象/5fields；state.resume701，A→B两别名、A未初始化field、B→A及B.VALUE1。全局记录拥有各数组，target随后仅持逻辑ID/recipe，不生成运行时对象；diagnostic没有在本hook显式清零。不得与其它线程同时改全局/fault状态。
 * 失败/资源边界：calloc/Append等断言可提前longjmp；已写全局owner指针由tearDown释放，不承诺局部自动对象生命周期超出调用。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:17。
 * tests/parser/test_ssa_deopt_aggregates.c:19。
 * tests/parser/test_ssa_deopt_aggregates.c:20。
 * tests/parser/test_ssa_deopt_aggregates.c:21。
 * tests/parser/test_ssa_deopt_aggregates.c:26。
 * tests/parser/test_ssa_deopt_aggregates.c:35。
 * tests/parser/test_ssa_deopt_aggregates.c:38。
 * tests/parser/test_ssa_deopt_aggregates.c:41。
 * tests/parser/test_ssa_deopt_aggregates.c:46。
 * tests/parser/test_ssa_deopt_aggregates.c:52。
 * tests/parser/test_ssa_deopt_aggregates.c:61。
 * tests/third_party/zr_unity/Unity/src/unity.c:2298。
 */

/**
 * @fn void tearDown(void)
 *
 * ## Summary
 * 在每个case正常或Unity保护的失败出口释放全局逻辑target和函数拥有资源。
 *
 * ## Returns
 * 无C返回值；先MaterializedStateFree(target)，再FreeFunction(function)。
 *
 * ## Constraints and lifecycle
 * 仅接管全局target/function的合法独立所有权，不接管局部clone、source map、old快照或任意未恢复借用。Free前原池借用须停止；记录清空仍有效。用例暂借source池必须按其正常路径恢复/脱离。串行，不重置fault计数，下一setUp负责重置。
 * 失败/资源边界：Unity会在独立保护段调用；此前case长跳转不会自动执行被跳过的局部Free/restore。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:73。
 * tests/parser/test_ssa_deopt_aggregates.c:74。
 * tests/parser/test_ssa_deopt_aggregates.c:75。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:175。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:177。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:178。
 * tests/third_party/zr_unity/Unity/src/unity.c:2303。
 */

/**
 * @fn static TZrBool materialize_from_map(const SZrExecIrStateMap *map)
 *
 * ## Summary
 * 把本fixture的固定checkpoint和当前函数身份包装成一次逻辑MaterializeState请求。
 *
 * ## Parameters
 * @param[in] map 借用的候选map，可NULL，由下层回退function.stateMap；输入存储在调用期间稳定可读。
 *
 * ## Returns
 * @return 直接返回MaterializeState布尔结果；失败详情写全局diagnostic。
 *
 * ## Constraints and lifecycle
 * 使用当前functionToken/generation/signature，固定source42/resume701/AFTER_EFFECT和全局target；request仅栈内借用，不持有map。非NULLtarget须已Init/可独立free，且与输入records/pools分离。成功替换旧target、使旧池借用失效；失败下层保留旧target。values/roots为逻辑ID，不是运行时payload/GC保活。
 * 失败/资源边界：函数本身无断言/分配；下层失败可更新diagnostic并返回FALSE，不应消费target为新结果。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:78。
 * tests/parser/test_ssa_deopt_aggregates.c:80。
 * tests/parser/test_ssa_deopt_aggregates.c:82。
 * tests/parser/test_ssa_deopt_aggregates.c:85。
 * tests/parser/test_ssa_deopt_aggregates.c:88。
 * tests/parser/test_ssa_deopt_aggregates.c:89。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:674。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:720。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:818。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:819。
 */

/**
 * @fn static TZrBool materialize(void)
 *
 * ## Summary
 * 用当前function.stateMap调用本fixture的固定AFTER_EFFECT逻辑物化请求。
 *
 * ## Returns
 * @return 原样返回materialize_from_map结果。
 *
 * ## Constraints and lifecycle
 * 借用当前全局function/map/target；与带map helper相同的稳定性、独立owner和诊断非别名前提。无额外分配/检查，不扩展恢复或执行能力。
 * 失败/资源边界：返回FALSE时下层diagnostic记录失败，旧target由下层事务保留。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:92。
 * tests/parser/test_ssa_deopt_aggregates.c:93。
 * tests/parser/test_ssa_deopt_aggregates.c:78。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:812。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:813。
 */

/**
 * @fn static void build(void)
 *
 * ## Summary
 * 为当前全局function构建状态图，并把失败报告为当前Unity case断言。
 *
 * ## Returns
 * 无C返回值；Build返回TRUE才正常返回，否则触发Unity断言。
 *
 * ## Constraints and lifecycle
 * Build成功仅替换map，不转移function其它池/全局owner；旧map池借用失效。调用前function须满足实际builder前提，diagnostic独立；测试对返回TRUE的断言不代替完整目标运行证据。Unity断言失败通过longjmp跳过余下函数/case，runner记录失败并在保护段调用tearDown，然后继续其它case；void函数没有C失败返回值，UNITY_END最终返回失败计数。
 * 失败/资源边界：断言失败跳过本helper/case后续语句；global teardown仍是runner保护调用，局部owner不由此helper接管。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:96。
 * tests/parser/test_ssa_deopt_aggregates.c:97。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:374。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:376。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:379。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:384。
 */

/**
 * @fn static void assert_location(void)
 *
 * ## Summary
 * 检查配方失败诊断的固定函数token、source和instruction位置。
 *
 * ## Returns
 * 无C返回值；三个断言满足正常返回，否则按Unity退出当前case。
 *
 * ## Constraints and lifecycle
 * 只读全局diagnostic；预期token99/source42/instruction1来自本fixture。未检查blockId、expected/actual或diagnostic.code；caller需分别核错误码。Unity断言失败通过longjmp跳过余下函数/case，runner记录失败并在保护段调用tearDown，然后继续其它case；void函数没有C失败返回值，UNITY_END最终返回失败计数。
 * 失败/资源边界：没有诊断修复/资源释放或新返回码。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:100。
 * tests/parser/test_ssa_deopt_aggregates.c:101。
 * tests/parser/test_ssa_deopt_aggregates.c:102。
 * tests/parser/test_ssa_deopt_aggregates.c:103。
 */

/**
 * @fn static void assert_invalid(EZrExecutionDiagnosticCode code)
 *
 * ## Summary
 * 用VerifyFunction ALL验证当前畸形fixture被拒绝，并核错误码与固定位置。
 *
 * ## Parameters
 * @param[in] code 本次失败预期的诊断码。
 *
 * ## Returns
 * 无C返回值；Verify FALSE、code相等且固定位置满足才正常返回。
 *
 * ## Constraints and lifecycle
 * 只对当前全局function执行现有校验；错误可能由前置验证提前拒绝，不能仅从case名称授下游分支信用。diagnostic须独立于输入。Unity断言失败通过longjmp跳过余下函数/case，runner记录失败并在保护段调用tearDown，然后继续其它case；void函数没有C失败返回值，UNITY_END最终返回失败计数。
 * 失败/资源边界：不恢复被修改字段；函数拥有的当前存储仍由tearDown释放。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:106。
 * tests/parser/test_ssa_deopt_aggregates.c:107。
 * tests/parser/test_ssa_deopt_aggregates.c:109。
 * tests/parser/test_ssa_deopt_aggregates.c:110。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:403。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:909。
 */

/**
 * @fn static void test_fields_feed_checkpoint_liveness_and_roots(void)
 *
 * ## Summary
 * 检查仅由recipe引用的SSA值进入checkpoint的live/root清单，并可生成逻辑target。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。没有比较全部phase或实际GC对象/执行结果。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：BEFORE entry非空、live/root各1且首ID均1；AFTER物化target.valueCount/rootCount各1。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:113。
 * tests/parser/test_ssa_deopt_aggregates.c:118。
 * tests/parser/test_ssa_deopt_aggregates.c:119。
 * tests/parser/test_ssa_deopt_aggregates.c:120。
 * tests/parser/test_ssa_deopt_aggregates.c:121。
 * tests/parser/test_ssa_deopt_aggregates.c:122。
 * tests/parser/test_ssa_deopt_aggregates.c:123。
 * tests/parser/test_ssa_deopt_aggregates.c:124。
 * tests/parser/test_ssa_deopt_aggregates.c:125。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:175。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:37。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:67。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:122。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:252。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:361。
 */

/**
 * @fn static void test_materialized_graph_preserves_alias_cycle_and_uninitialized_field(void)
 *
 * ## Summary
 * 检查逻辑recipe副本保留两对象别名、循环边与UNINITIALIZED标记。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。未比较全部字段/字节或源释放后读取；不是运行时对象GC根测试。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：target两个aggregate/五fields；两池地址与function不同；核两个identity、两条A→B、B→A、UNINITIALIZED及VALUE；修改原field后副本首值仍1。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:128。
 * tests/parser/test_ssa_deopt_aggregates.c:130。
 * tests/parser/test_ssa_deopt_aggregates.c:131。
 * tests/parser/test_ssa_deopt_aggregates.c:132。
 * tests/parser/test_ssa_deopt_aggregates.c:133。
 * tests/parser/test_ssa_deopt_aggregates.c:134。
 * tests/parser/test_ssa_deopt_aggregates.c:135。
 * tests/parser/test_ssa_deopt_aggregates.c:136。
 * tests/parser/test_ssa_deopt_aggregates.c:137。
 * tests/parser/test_ssa_deopt_aggregates.c:138。
 * tests/parser/test_ssa_deopt_aggregates.c:139。
 * tests/parser/test_ssa_deopt_aggregates.c:140。
 * tests/parser/test_ssa_deopt_aggregates.c:141。
 * tests/parser/test_ssa_deopt_aggregates.c:143。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:153。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:397。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:400。
 */

/**
 * @fn static void test_ranges_reject_capacity_and_wrap_before_analysis(void)
 *
 * ## Summary
 * 验证state逻辑选区越界和field.start极大值在analysis前被拒绝。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。不证明所有整数/尺寸溢出路径。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：aggregate count3超过logical2但在capacity3内，随后fields.start=UINT32_MAX；均断言INVALID_RANGE及固定位置。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:146。
 * tests/parser/test_ssa_deopt_aggregates.c:147。
 * tests/parser/test_ssa_deopt_aggregates.c:148。
 * tests/parser/test_ssa_deopt_aggregates.c:150。
 * tests/parser/test_ssa_deopt_aggregates.c:151。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:7。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:196。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:207。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:403。
 */

/**
 * @fn static void test_fields_reject_invalid_value_and_enum(void)
 *
 * ## Summary
 * 拒绝recipe VALUE越界和负向未知field kind。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。无其它pool/enum全域信用。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：valueId2>valueCount1得到INVALID_VALUE；恢复ID后kind=-1得到STATE_MAP_INVALID，均核位置。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:154。
 * tests/parser/test_ssa_deopt_aggregates.c:155。
 * tests/parser/test_ssa_deopt_aggregates.c:156。
 * tests/parser/test_ssa_deopt_aggregates.c:158。
 * tests/parser/test_ssa_deopt_aggregates.c:159。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:107。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:121。
 */

/**
 * @fn static void test_graph_rejects_missing_identity(void)
 *
 * ## Summary
 * 拒绝指向同一state选区内不存在identity的边。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。不覆盖多state或prototype继承链环。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：A第二条边改aggregateId33，断言STATE_MAP_INVALID及固定位置。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:162。
 * tests/parser/test_ssa_deopt_aggregates.c:163。
 * tests/parser/test_ssa_deopt_aggregates.c:164。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:159。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:164。
 */

/**
 * @fn static void test_uninitialized_field_cannot_hide_a_value(void)
 *
 * ## Summary
 * 拒绝UNINITIALIZED field隐藏非零VALUE引用。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。未测试运行时未初始化对象。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：UNINITIALIZED的valueId改1，断言STATE_MAP_INVALID及位置。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:167。
 * tests/parser/test_ssa_deopt_aggregates.c:168。
 * tests/parser/test_ssa_deopt_aggregates.c:169。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:100。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:102。
 */

/**
 * @fn static void test_clone_owns_recipe_arrays(void)
 *
 * ## Summary
 * 检查函数clone独立复制recipe数组，释放clone后源recipe仍可读。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。局部clone仅正常路径Free，Unity早退可跳过；未比较state池独立性/源先释放后clone可读性。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：clone aggregate/field count为2/5，两池地址均不同；clone.field0.aggregateId22；FreeClone后source.field0仍22。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:172。
 * tests/parser/test_ssa_deopt_aggregates.c:175。
 * tests/parser/test_ssa_deopt_aggregates.c:176。
 * tests/parser/test_ssa_deopt_aggregates.c:177。
 * tests/parser/test_ssa_deopt_aggregates.c:178。
 * tests/parser/test_ssa_deopt_aggregates.c:179。
 * tests/parser/test_ssa_deopt_aggregates.c:180。
 * tests/parser/test_ssa_deopt_aggregates.c:181。
 * tests/parser/test_ssa_deopt_aggregates.c:182。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:318。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:423。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:424。
 */

/**
 * @fn static void test_borrowed_aggregate_field_rejects_suspend_boundary(void)
 *
 * ## Summary
 * 检查recipe借用值跨SUSPEND边界被Builder拒绝。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。不证明所有Borrowed或resume场景。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：value1改BORROWED、instruction加MAY_SUSPEND；Build FALSE，精确BORROWED_ACROSS_SUSPEND及位置。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:185。
 * tests/parser/test_ssa_deopt_aggregates.c:186。
 * tests/parser/test_ssa_deopt_aggregates.c:187。
 * tests/parser/test_ssa_deopt_aggregates.c:188。
 * tests/parser/test_ssa_deopt_aggregates.c:189。
 * tests/parser/test_ssa_deopt_aggregates.c:190。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:67。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:265。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:269。
 */

/**
 * @fn static void test_unavailable_aggregate_field_is_not_silently_omitted(void)
 *
 * ## Summary
 * 检查recipe引用只有声明而无可用定义的新值时Builder不静默省略。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。不把所有SSA非法输入/owner分配故障归于此例。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：添加GC value2且recipe改引用2；Build FALSE、STATE_MAP_INVALID及位置。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:193。
 * tests/parser/test_ssa_deopt_aggregates.c:194。
 * tests/parser/test_ssa_deopt_aggregates.c:197。
 * tests/parser/test_ssa_deopt_aggregates.c:198。
 * tests/parser/test_ssa_deopt_aggregates.c:199。
 * tests/parser/test_ssa_deopt_aggregates.c:200。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:321。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:290。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:258。
 */

/**
 * @fn static void test_failed_rebuild_and_consumer_preserve_published_state(void)
 *
 * ## Summary
 * 检查recipe非法时重建和consumer失败保留已发布map及旧逻辑target记录。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。target记录相等含指针，不穷尽全部池payload；old不可另Free。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：先成功构造并浅拷贝old；valueId改9后Build/Materialize均FALSE、INVALID_VALUE及位置；map指针相同、target整记录字节相等。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:203。
 * tests/parser/test_ssa_deopt_aggregates.c:207。
 * tests/parser/test_ssa_deopt_aggregates.c:208。
 * tests/parser/test_ssa_deopt_aggregates.c:209。
 * tests/parser/test_ssa_deopt_aggregates.c:212。
 * tests/parser/test_ssa_deopt_aggregates.c:213。
 * tests/parser/test_ssa_deopt_aggregates.c:215。
 * tests/parser/test_ssa_deopt_aggregates.c:216。
 * tests/parser/test_ssa_deopt_aggregates.c:217。
 * tests/parser/test_ssa_deopt_aggregates.c:219。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:374。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:376。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:107。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:573。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685。
 */

/**
 * @fn static void test_duplicate_identity_and_field_are_rejected(void)
 *
 * ## Summary
 * 分别拒绝重复aggregate identity和同对象重复fieldIndex。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。不等价全局跨state identity唯一。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：先B.identityId改11得DUPLICATE；恢复22后A第二fieldIndex改0再得DUPLICATE，均核位置。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:222。
 * tests/parser/test_ssa_deopt_aggregates.c:223。
 * tests/parser/test_ssa_deopt_aggregates.c:224。
 * tests/parser/test_ssa_deopt_aggregates.c:225。
 * tests/parser/test_ssa_deopt_aggregates.c:226。
 * tests/parser/test_ssa_deopt_aggregates.c:227。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:139。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:142。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:222。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:225。
 */

/**
 * @fn static void test_selected_graph_cannot_reference_another_states_object(void)
 *
 * ## Summary
 * 拒绝selected state选区之外的AGGREGATE目标。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。不凭case名称声称跨两个state实测。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：仅把唯一state.aggregateCount从2缩为1，保留全局B；A→B不在selected范围，STATE_MAP_INVALID及位置。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:230。
 * tests/parser/test_ssa_deopt_aggregates.c:231。
 * tests/parser/test_ssa_deopt_aggregates.c:232。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:159。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:164。
 */

/**
 * @fn static void test_selected_graph_is_compact_and_field_ranges_are_rebased(void)
 *
 * ## Summary
 * 检查selected graph复制为紧凑数组并重基fields.start。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。没有验证任意稀疏图或所有原始fields内容。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：选择新增identity33单对象/fieldIndex87/value1；target count1/1、identity33、start0/count1、fieldIndex87/value1。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:235。
 * tests/parser/test_ssa_deopt_aggregates.c:236。
 * tests/parser/test_ssa_deopt_aggregates.c:242。
 * tests/parser/test_ssa_deopt_aggregates.c:244。
 * tests/parser/test_ssa_deopt_aggregates.c:245。
 * tests/parser/test_ssa_deopt_aggregates.c:246。
 * tests/parser/test_ssa_deopt_aggregates.c:247。
 * tests/parser/test_ssa_deopt_aggregates.c:248。
 * tests/parser/test_ssa_deopt_aggregates.c:249。
 * tests/parser/test_ssa_deopt_aggregates.c:250。
 * tests/parser/test_ssa_deopt_aggregates.c:251。
 * tests/parser/test_ssa_deopt_aggregates.c:252。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:394。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:398。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:406。
 */

/**
 * @fn static void test_unreferenced_state_does_not_keep_fields_live(void)
 *
 * ## Summary
 * 区分未被checkpoint引用的recipe与仍须全局预检的侧表。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。不证明全局ValueReferenced会忽略未引用state，实际上全局query扫描全部fields。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：instruction.deoptId=0后Build，entries0 live/root计数0；再把state aggregateCount3，Verify FALSE/INVALID_RANGE，source42但instruction0。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:255。
 * tests/parser/test_ssa_deopt_aggregates.c:256。
 * tests/parser/test_ssa_deopt_aggregates.c:257。
 * tests/parser/test_ssa_deopt_aggregates.c:258。
 * tests/parser/test_ssa_deopt_aggregates.c:259。
 * tests/parser/test_ssa_deopt_aggregates.c:260。
 * tests/parser/test_ssa_deopt_aggregates.c:261。
 * tests/parser/test_ssa_deopt_aggregates.c:263。
 * tests/parser/test_ssa_deopt_aggregates.c:264。
 * tests/parser/test_ssa_deopt_aggregates.c:265。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:41。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:196。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:42。
 */

/**
 * @fn static void test_positive_unknown_kind_and_conflicting_ids_are_rejected(void)
 *
 * ## Summary
 * 拒绝正向越界kind及VALUE/AGGREGATE互斥ID冲突。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。不把enum名称当穷尽payload验证。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：kind=KIND_COUNT；VALUE加aggregateId11；恢复后AGGREGATE加valueId1；各STATE_MAP_INVALID及位置。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:268。
 * tests/parser/test_ssa_deopt_aggregates.c:269。
 * tests/parser/test_ssa_deopt_aggregates.c:270。
 * tests/parser/test_ssa_deopt_aggregates.c:272。
 * tests/parser/test_ssa_deopt_aggregates.c:273。
 * tests/parser/test_ssa_deopt_aggregates.c:275。
 * tests/parser/test_ssa_deopt_aggregates.c:276。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:104。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:116。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:121。
 */

/**
 * @fn static void test_consumer_rejects_omitted_field_live_value(void)
 *
 * ## Summary
 * 检查删除map内recipe live/root/owner列后consumer拒绝且旧target记录不变。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。不为aggregate-live缺值分支单独授覆盖；old非owner不可Free。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：先物化并浅拷贝old，清所有entry三类count；Materialize FALSE/STATE_MAP_INVALID及位置，target整记录相等。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:279。
 * tests/parser/test_ssa_deopt_aggregates.c:283。
 * tests/parser/test_ssa_deopt_aggregates.c:284。
 * tests/parser/test_ssa_deopt_aggregates.c:285。
 * tests/parser/test_ssa_deopt_aggregates.c:286。
 * tests/parser/test_ssa_deopt_aggregates.c:287。
 * tests/parser/test_ssa_deopt_aggregates.c:288。
 * tests/parser/test_ssa_deopt_aggregates.c:290。
 * tests/parser/test_ssa_deopt_aggregates.c:291。
 * tests/parser/test_ssa_deopt_aggregates.c:292。
 * tests/parser/test_ssa_deopt_aggregates.c:293。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:503。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:504。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:52。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:62。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:351。
 */

/**
 * @fn static void test_target_cannot_alias_function_recipe_storage(void)
 *
 * ## Summary
 * 拒绝把function recipe池伪装为旧target拥有存储。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。仅预期FALSE路径保证原recipe不被释放；不能承诺假设错误成功时fixture仍无损；未比较所有target字段。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：先成功物化，暂借function.deoptAggregates地址/capacity，调用后先恢复原target指针/cap，再断言FALSE/MATERIALIZATION_FAILED。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:296。
 * tests/parser/test_ssa_deopt_aggregates.c:301。
 * tests/parser/test_ssa_deopt_aggregates.c:302。
 * tests/parser/test_ssa_deopt_aggregates.c:304。
 * tests/parser/test_ssa_deopt_aggregates.c:306。
 * tests/parser/test_ssa_deopt_aggregates.c:307。
 * tests/parser/test_ssa_deopt_aggregates.c:308。
 * tests/parser/test_ssa_deopt_aggregates.c:309。
 * tests/parser/test_ssa_deopt_aggregates.c:310。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:720。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c:308。
 */

/**
 * @fn static void test_empty_graph_replaces_previous_prepared_graph(void)
 *
 * ## Summary
 * 检查空selected graph成功替换旧prepared recipe为全空数组。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。不测自然OOM或空function。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：原target.aggregateCount2；state start设logical end/count0重建物化；两个recipe counts0、指针NULL。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:313。
 * tests/parser/test_ssa_deopt_aggregates.c:315。
 * tests/parser/test_ssa_deopt_aggregates.c:316。
 * tests/parser/test_ssa_deopt_aggregates.c:317。
 * tests/parser/test_ssa_deopt_aggregates.c:318。
 * tests/parser/test_ssa_deopt_aggregates.c:319。
 * tests/parser/test_ssa_deopt_aggregates.c:320。
 * tests/parser/test_ssa_deopt_aggregates.c:321。
 * tests/parser/test_ssa_deopt_aggregates.c:322。
 * tests/parser/test_ssa_deopt_aggregates.c:323。
 * tests/parser/test_ssa_deopt_aggregates.c:324。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:378。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:386。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:406。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:818。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:819。
 */

/**
 * @fn static TZrBool try_borrowed_target(const SZrExecIrStateMap *map, void *storage)
 *
 * ## Summary
 * 把一个source池临时伪装为target.values，测试物化的输入/输出隔离拒绝，并在FALSE时脱离借用。
 *
 * ## Parameters
 * @param[in] map 借用的验证输入map，调用期间稳定。
 * @param[in] storage 非空借用source池；仅临时作为1元素values地址，不向target转移合法所有权。
 *
 * ## Returns
 * @return 原样返回materialize_from_map结果；FALSE时把target.values及其count/cap清空，TRUE时不恢复。
 *
 * ## Constraints and lifecycle
 * 调用前target.values须为空且没有待释放的旧values owner，否则直接覆盖会丢失原所有权。map/function须正常可读，故意破坏的仅旧target/source隔离前提。FALSE之后source仍由原owner拥有；若被测实现错误地成功，借用可能已被其Free，caller不再解引用该storage并先把source原指针置NULL。该异常绿路径防重复Free的补救不是生产API接受alias的保证。串行，无本地分配。
 * 失败/资源边界：FALSE时主动脱离借用便于teardown；其它target字段未整体重置，不声称修复任意异常/错误实现。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:329。
 * tests/parser/test_ssa_deopt_aggregates.c:331。
 * tests/parser/test_ssa_deopt_aggregates.c:333。
 * tests/parser/test_ssa_deopt_aggregates.c:334。
 * tests/parser/test_ssa_deopt_aggregates.c:335。
 * tests/parser/test_ssa_deopt_aggregates.c:349。
 * tests/parser/test_ssa_deopt_aggregates.c:350。
 * tests/parser/test_ssa_deopt_aggregates.c:364。
 * tests/parser/test_ssa_deopt_aggregates.c:365。
 * tests/parser/test_ssa_deopt_aggregates.c:377。
 * tests/parser/test_ssa_deopt_aggregates.c:378。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:720。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:818。
 */

/**
 * @fn static void test_target_cannot_own_function_frame_slots(void)
 *
 * ## Summary
 * 拒绝旧target.values占有function.frameLayout.slots借用存储。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。helper在FALSE脱离借用；若异常成功，caller先将source slots置NULL以避免重复free；只核所列地址。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：构造1槽，try_borrowed_target后FALSE/MATERIALIZATION_FAILED且source slots仍非NULL。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:341。
 * tests/parser/test_ssa_deopt_aggregates.c:344。
 * tests/parser/test_ssa_deopt_aggregates.c:346。
 * tests/parser/test_ssa_deopt_aggregates.c:349。
 * tests/parser/test_ssa_deopt_aggregates.c:350。
 * tests/parser/test_ssa_deopt_aggregates.c:351。
 * tests/parser/test_ssa_deopt_aggregates.c:352。
 * tests/parser/test_ssa_deopt_aggregates.c:353。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c:271。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c:308。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:720。
 */

/**
 * @fn static void test_target_cannot_own_function_gc_slots(void)
 *
 * ## Summary
 * 拒绝旧target.values占有function.gcMap.slotIndexPool借用存储。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。FALSE脱离target借用，异常成功时source置NULL；不证明运行时GC槽语义。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：构造1项gc槽，FALSE/MATERIALIZATION_FAILED且slotIndexPool仍非NULL。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:356。
 * tests/parser/test_ssa_deopt_aggregates.c:359。
 * tests/parser/test_ssa_deopt_aggregates.c:361。
 * tests/parser/test_ssa_deopt_aggregates.c:364。
 * tests/parser/test_ssa_deopt_aggregates.c:365。
 * tests/parser/test_ssa_deopt_aggregates.c:366。
 * tests/parser/test_ssa_deopt_aggregates.c:367。
 * tests/parser/test_ssa_deopt_aggregates.c:368。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c:278。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c:308。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:720。
 */

/**
 * @fn static void test_target_cannot_own_function_map_when_using_external_map(void)
 *
 * ## Summary
 * 使用external clone map时仍拒绝target占有function原map的valuePool。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。source仅正常路径释放；Clone断言longjmp可能跳过局部Free，global tearDown不接管source。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：局部source克隆map，借function.stateMap.valuePool；先Free局部source，再FALSE/MATERIALIZATION_FAILED且原valuePool非NULL。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:371。
 * tests/parser/test_ssa_deopt_aggregates.c:375。
 * tests/parser/test_ssa_deopt_aggregates.c:376。
 * tests/parser/test_ssa_deopt_aggregates.c:377。
 * tests/parser/test_ssa_deopt_aggregates.c:378。
 * tests/parser/test_ssa_deopt_aggregates.c:379。
 * tests/parser/test_ssa_deopt_aggregates.c:380。
 * tests/parser/test_ssa_deopt_aggregates.c:381。
 * tests/parser/test_ssa_deopt_aggregates.c:382。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c:284。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c:288。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c:308。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:720。
 */

/**
 * @fn static void test_each_recipe_allocation_failure_preserves_old_roots_and_graph(void)
 *
 * ## Summary
 * 逐次注入两处recipe malloc失败，核旧逻辑roots/graph保留并能继续成功。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。不覆盖普通三池/Owner/Liveness/Clone分配、自然OOM；old非owner，不另Free。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：ordinal1/2各FALSE、hit TRUE、OUT_OF_MEMORY、固定位置、target整记录等old及root0=1/field0.aggregateId22；ordinal3成功且无hit、aggregateCount2/root0=1。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:385。
 * tests/parser/test_ssa_deopt_aggregates.c:389。
 * tests/parser/test_ssa_deopt_aggregates.c:392。
 * tests/parser/test_ssa_deopt_aggregates.c:393。
 * tests/parser/test_ssa_deopt_aggregates.c:394。
 * tests/parser/test_ssa_deopt_aggregates.c:395。
 * tests/parser/test_ssa_deopt_aggregates.c:397。
 * tests/parser/test_ssa_deopt_aggregates.c:398。
 * tests/parser/test_ssa_deopt_aggregates.c:399。
 * tests/parser/test_ssa_deopt_aggregates.c:401。
 * tests/parser/test_ssa_deopt_aggregates.c:402。
 * tests/parser/test_ssa_deopt_aggregates.c:403。
 * tests/parser/test_ssa_deopt_aggregates.c:404。
 * tests/parser/test_ssa_deopt_aggregates.c:405。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:387。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:391。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:813。
 * tests/parser/ssa_deopt_aggregate_fault_allocator.c:20。
 * tests/parser/ssa_deopt_aggregate_fault_allocator.c:32。
 * tests/parser/ssa_deopt_aggregate_fault_allocator.c:33。
 */

/**
 * @fn static void test_dce_keeps_definition_used_only_by_aggregate_recipe(void)
 *
 * ## Summary
 * 检查仅为aggregate recipe保留的COPY定义不会被DCE删除。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。未断言changed值、实际执行/Oracle等价或其它优化pass；局部context不拥有资源。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：构造input1→result2 COPY再checkpoint；VerifyTRUE、DCE TRUE后COPY仍在且definition1；重建物化后recipe.valueId和target.values0均result。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:408。
 * tests/parser/test_ssa_deopt_aggregates.c:415。
 * tests/parser/test_ssa_deopt_aggregates.c:418。
 * tests/parser/test_ssa_deopt_aggregates.c:420。
 * tests/parser/test_ssa_deopt_aggregates.c:422。
 * tests/parser/test_ssa_deopt_aggregates.c:427。
 * tests/parser/test_ssa_deopt_aggregates.c:429。
 * tests/parser/test_ssa_deopt_aggregates.c:430。
 * tests/parser/test_ssa_deopt_aggregates.c:431。
 * tests/parser/test_ssa_deopt_aggregates.c:432。
 * tests/parser/test_ssa_deopt_aggregates.c:433。
 * tests/parser/test_ssa_deopt_aggregates.c:434。
 * tests/parser/test_ssa_deopt_aggregates.c:435。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c:237。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c:260。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c:359。
 */

/**
 * @fn static void test_function_hash_tracks_field_binding_layout_and_selected_graph(void)
 *
 * ## Summary
 * 检查Parser函数hash跟随recipe binding、字段索引、layout/type和selected选区变化并可恢复。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。只核有限改动，不证明碰撞自由、所有字段/非法hash0或缓存失效全部路径。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：基准非零；改valueId、fieldIndex、layoutId、typeToken、state.aggregateCount0各不等original；全部恢复后相等。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:438。
 * tests/parser/test_ssa_deopt_aggregates.c:443。
 * tests/parser/test_ssa_deopt_aggregates.c:444。
 * tests/parser/test_ssa_deopt_aggregates.c:446。
 * tests/parser/test_ssa_deopt_aggregates.c:449。
 * tests/parser/test_ssa_deopt_aggregates.c:452。
 * tests/parser/test_ssa_deopt_aggregates.c:455。
 * tests/parser/test_ssa_deopt_aggregates.c:458。
 * tests/parser/test_ssa_deopt_aggregates.c:460。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:305。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c:83。
 */

/**
 * @fn static void test_recipe_requires_identity_type_and_layout(void)
 *
 * ## Summary
 * 拒绝零identity/type/layout的recipe声明。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。仅零值缺失，不认证外部实际类型/layout存在。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：依次identityId0、恢复后typeToken0、恢复后layoutId0；各STATE_MAP_INVALID及位置。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:463。
 * tests/parser/test_ssa_deopt_aggregates.c:464。
 * tests/parser/test_ssa_deopt_aggregates.c:465。
 * tests/parser/test_ssa_deopt_aggregates.c:467。
 * tests/parser/test_ssa_deopt_aggregates.c:468。
 * tests/parser/test_ssa_deopt_aggregates.c:470。
 * tests/parser/test_ssa_deopt_aggregates.c:471。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:214。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:215。
 */

/**
 * @fn static void test_null_recipe_pool_is_rejected_before_dereference(void)
 *
 * ## Summary
 * 检查非零容量的NULL field池在遍历前拒绝。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。先恢复再assert保障预期/反例assert路径可由global tearDown释放；不认证任意非NULL无效地址。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：暂存field指针后设NULL，Verify结果保存并先恢复指针；再核FALSE/STATE_MAP_INVALID。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:474。
 * tests/parser/test_ssa_deopt_aggregates.c:475。
 * tests/parser/test_ssa_deopt_aggregates.c:477。
 * tests/parser/test_ssa_deopt_aggregates.c:478。
 * tests/parser/test_ssa_deopt_aggregates.c:479。
 * tests/parser/test_ssa_deopt_aggregates.c:480。
 * tests/parser/test_ssa_deopt_aggregates.c:481。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:187。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:190。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:403。
 */

/**
 * @fn static void test_large_cycle_repeated_preparation_preserves_one_root(void)
 *
 * ## Summary
 * 检查257对象环配方重复准备的有限结果及唯一逻辑root。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。只比较所列尾边/计数/root，不执行运行时对象或穷尽所有环边，也不是复杂度/栈深证明。
 * 失败/资源边界：测试断言失败不提供失败返回码或恢复保证；此句仅指void case，无关整个可执行程序最终状态。
 *
 * 实际断言：建立257aggregate/258fields的环，尾对象另含VALUE1；四次materialize各核count257/258、尾环边指向1、rootCount1/root0=1。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:484。
 * tests/parser/test_ssa_deopt_aggregates.c:485。
 * tests/parser/test_ssa_deopt_aggregates.c:489。
 * tests/parser/test_ssa_deopt_aggregates.c:490。
 * tests/parser/test_ssa_deopt_aggregates.c:491。
 * tests/parser/test_ssa_deopt_aggregates.c:492。
 * tests/parser/test_ssa_deopt_aggregates.c:509。
 * tests/parser/test_ssa_deopt_aggregates.c:511。
 * tests/parser/test_ssa_deopt_aggregates.c:512。
 * tests/parser/test_ssa_deopt_aggregates.c:513。
 * tests/parser/test_ssa_deopt_aggregates.c:514。
 * tests/parser/test_ssa_deopt_aggregates.c:515。
 * tests/parser/test_ssa_deopt_aggregates.c:516。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:153。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:394。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:818。
 */

/**
 * @fn static void test_checkpoint_result_cannot_supply_its_before_effect_recipe(void)
 *
 * ## Summary
 * 记录共享before-effect recipe拒绝场景的测试意图，并标明现有前置Verify成功断言已失败。
 *
 * ## Returns
 * 无C返回值；断言满足则正常返回。Unity断言失败longjmp退出当前case，runner记录失败并在保护段调用tearDown，随后继续后续case；UNITY_END最终返回失败计数。
 *
 * ## Constraints and lifecycle
 * 由本文件setUp构造固定token99/generation7/source42/resume701的两对象逻辑配方；各case串行运行。全局target和function由tearDown释放，局部浅拷贝old/published仅观察、不取得所有权。Unity longjmp跳过case余下语句，不能保证局部owner或临时借用恢复语句执行。本提案不宣称该case成功或后续Build拒绝已执行；静态疑点单独列metadata，Dox不写未经运行核实的错误结论。
 * 实际断言：源码先build/published，再新增COPY result并改recipe，随后要求VerifyFunction TRUE，再预期Build FALSE/STATE_MAP_INVALID/位置及published指针保留。Root现有BEFORE实际在535的Verify成功断言失败，536–539未到达。
 * BUG: tests/parser/test_ssa_deopt_aggregates.c:535 在tests/parser/test_ssa_deopt_aggregates.c:523发布旧stateMap后，经tests/parser/test_ssa_deopt_aggregates.c:525新增结果并在tests/parser/test_ssa_deopt_aggregates.c:527改为COPY、tests/parser/test_ssa_deopt_aggregates.c:533改recipe，现有VerifyFunction ALL成功断言返回FALSE；tests/parser/test_ssa_deopt_aggregates.c:536及后续Build/诊断/状态保留断言未到达。Root未改源码BEFORE已复现27PASS/1FAIL，证据E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/exec-ir-deopt-aggregate-native-before-r1-summary.json，SHA256 5534553b99fe8f236a8cbc2708adebb5f5b3abc014862e230dcd299fa4eebe22；静态入口zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:909和zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:910。不将该测试前置场景问题外推为生产API缺陷，未改断言或实现。
 *
 * ## References
 * tests/parser/test_ssa_deopt_aggregates.c:520。
 * tests/parser/test_ssa_deopt_aggregates.c:523。
 * tests/parser/test_ssa_deopt_aggregates.c:524。
 * tests/parser/test_ssa_deopt_aggregates.c:525。
 * tests/parser/test_ssa_deopt_aggregates.c:527。
 * tests/parser/test_ssa_deopt_aggregates.c:532。
 * tests/parser/test_ssa_deopt_aggregates.c:533。
 * tests/parser/test_ssa_deopt_aggregates.c:534。
 * tests/parser/test_ssa_deopt_aggregates.c:536。
 * tests/parser/test_ssa_deopt_aggregates.c:537。
 * tests/parser/test_ssa_deopt_aggregates.c:538。
 * tests/parser/test_ssa_deopt_aggregates.c:539。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:909。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:910。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:503。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:62。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:122。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:132。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:374。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:376。
 * tests/parser/test_ssa_deopt_aggregates.c:535。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:912。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:556。
 */

/**
 * @fn int main(void)
 *
 * ## Summary
 * 按固定顺序把28个本文件case注册给Unity并返回汇总状态。
 *
 * ## Returns
 * @return 返回UNITY_END()结果，反映runner记录的失败计数。
 *
 * ## Constraints and lifecycle
 * UNITY_BEGIN初始化runner；每个RUN_TEST把void函数指针交UnityDefaultTestRun，实际调用本TU的setUp/case/tearDown。没有外部case header/include注册；allocator头仅声明控制接口，faultC另编译为替身TU。28case串行，共享三条静态state和fault计数。不会在此执行VM/Oracle/运行时对象物化。
 * 失败/资源边界：单case断言longjmp后runner继续其它case，非断言崩溃等不在此提供恢复保证。没有在本提案构建/运行此入口。
 *
 * ## References
 * tests/third_party/zr_unity/Unity/src/unity_internals.h:910。
 * tests/parser/test_ssa_deopt_aggregates.c:542。
 * tests/parser/test_ssa_deopt_aggregates.c:543。
 * tests/parser/test_ssa_deopt_aggregates.c:544。
 * tests/parser/test_ssa_deopt_aggregates.c:571。
 * tests/parser/test_ssa_deopt_aggregates.c:572。
 * tests/third_party/zr_unity/Unity/src/unity_internals.h:934。
 * tests/third_party/zr_unity/Unity/src/unity.c:2298。
 * tests/third_party/zr_unity/Unity/src/unity.c:2299。
 * tests/third_party/zr_unity/Unity/src/unity.c:2303。
 * tests/cmake/ssa-tests.cmake:329。
 * tests/cmake/ssa-tests.cmake:362。
 */
