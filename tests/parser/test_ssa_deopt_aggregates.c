#include "unity.h"
#include "ssa_deopt_aggregate_fault_allocator.h"

#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_parser/exec_ir_state_maps.h"
#include "zr_vm_parser/exec_ir_pass_manager.h"
#include "../../zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_internal.h"

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
