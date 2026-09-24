#include "ssa_state_map_fixture.h"
#include "ssa_owner_fault_allocator.h"

static void make_drop(TZrExecIrInstructionId id, TZrExecIrValueId owner) {
    SZrExecIrInstruction *instruction = &function.instructions[id - 1u];
    TZrExecIrMemoryTokenId tokens[2] = {1u, 2u};
    instruction->opcode = ZR_EXEC_IR_OPCODE_DROP;
    instruction->flags = 0u;
    instruction->effectIn = 1u;
    instruction->effectOut = 2u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendOperands(
            &function, &owner, 1u, &instruction->operandRange));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendMemoryTokens(
            &function, &tokens[0], 1u, &instruction->memoryIn));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendMemoryTokens(
            &function, &tokens[1], 1u, &instruction->memoryOut));
}

static void assert_initialized(TZrExecIrInstructionId id, TZrExecIrValueId owner) {
    const SZrExecIrStateMapEntry *entry = checkpoint(id, ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    TZrUInt32 index;
    assert_only_root(id, ZR_EXEC_IR_STATE_BEFORE_EFFECT, owner);
    assert_only_root(id, ZR_EXEC_IR_STATE_AFTER_EFFECT, owner);
    for (index = 0u; index < entry->liveValues.count; ++index) {
        if (function.stateMap->valuePool[entry->liveValues.start + index] == owner) {
            TEST_ASSERT_EQUAL_UINT32(ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED,
                    function.stateMap->ownerStatePool[entry->ownerStates.start + index]);
        }
    }
}

static TZrBool resume(const SZrExecIrStateMapEntry *entry,
                      SZrExecIrMaterializedState *target) {
    SZrExecIrResumeRequest request = {0};
    request.function = &function;
    request.map = function.stateMap;
    request.functionToken = function.functionToken;
    request.generation = function.contract.generation;
    request.sourceId = entry->sourceId;
    request.resumeId = entry->resumeId;
    request.phase = entry->phase;
    request.target = target;
    return ZrCore_ExecIr_MaterializeState(&request, &diagnostic);
}

static void sibling_transition(TZrBool drop) {
    TZrExecIrValueId condition = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_TRUE);
    TZrExecIrValueId owner = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_TRUE);
    TZrExecIrValueId moved = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_FALSE);
    blocks(3u);
    edges(1u, 2u, 3u);
    emit(1u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 0u, condition, 0u);
    if (drop) {
        make_drop(emit(2u, ZR_EXEC_IR_OPCODE_NOP, 0u, 0u, 0u), owner);
    } else {
        emit(2u, ZR_EXEC_IR_OPCODE_MOVE, 0u, owner, moved);
    }
    emit(2u, ZR_EXEC_IR_OPCODE_RETURN, 0u, drop ? condition : moved, 0u);
    emit(3u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(3u, ZR_EXEC_IR_OPCODE_RETURN, 0u, owner, 0u);
    build();
    assert_initialized(4u, owner);
}

static void test_sibling_drop_does_not_consume_live_owner(void) {
    sibling_transition(ZR_TRUE);
}

static void test_sibling_move_does_not_consume_live_owner(void) {
    sibling_transition(ZR_FALSE);
}

static void test_consumer_rejects_ambiguous_join_owner_without_replacing_target(void) {
    TZrExecIrValueId condition = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_TRUE);
    TZrExecIrValueId owner = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_TRUE);
    SZrExecIrMaterializedState target;
    SZrExecIrStateMapEntry selected;
    TZrExecIrValueId *oldValues;
    blocks(4u);
    edges(1u, 2u, 3u);
    edges(2u, 4u, 0u);
    edges(3u, 4u, 0u);
    emit(1u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 0u, condition, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_NOP, 0u, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    emit(3u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    emit(4u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(4u, ZR_EXEC_IR_OPCODE_RETURN, 0u, owner, 0u);
    build();
    selected = *checkpoint(5u, ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    function.stateMap->entries[0] = selected;
    function.stateMap->entryCount = 1u;
    ZrCore_ExecIr_MaterializedStateInit(&target);
    TEST_ASSERT_TRUE(resume(&selected, &target));
    oldValues = target.values;
    make_drop(2u, owner);
    /* Forge the state selected by the old scan's last stored transition. */
    function.stateMap->ownerStatePool[selected.ownerStates.start] =
            ZR_EXEC_IR_STATE_MAP_OWNER_DROPPED;
    TEST_ASSERT_FALSE(resume(&selected, &target));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(5u, diagnostic.instructionId);
    TEST_ASSERT_EQUAL_UINT32(105u, diagnostic.sourceId);
    TEST_ASSERT_EQUAL_PTR(oldValues, target.values);
    TEST_ASSERT_EQUAL_UINT32(owner, target.roots[0]);
    ZrCore_ExecIr_MaterializedStateFree(&target);
}

static void test_loop_carried_move_rejects_live_checkpoint_and_preserves_map(void) {
    TZrExecIrValueId condition = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_TRUE);
    TZrExecIrValueId owner = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_TRUE);
    TZrExecIrValueId moved = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_FALSE);
    SZrExecIrStateMap *published;
    blocks(3u);
    edges(1u, 2u, 0u);
    edges(2u, 2u, 3u);
    emit(1u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_COPY, 0u, owner, moved);
    emit(2u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 0u, condition, 0u);
    emit(3u, ZR_EXEC_IR_OPCODE_RETURN, 0u, moved, 0u);
    build();
    published = function.stateMap;
    function.instructions[2].opcode = ZR_EXEC_IR_OPCODE_MOVE;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(2u, diagnostic.instructionId);
    TEST_ASSERT_EQUAL_UINT32(102u, diagnostic.sourceId);
    TEST_ASSERT_EQUAL_PTR(published, function.stateMap);
}

static void test_loop_definition_reinitializes_previously_dropped_owner(void) {
    TZrExecIrValueId owner = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_FALSE);
    blocks(2u);
    edges(1u, 2u, 0u);
    edges(2u, 2u, 0u);
    emit(1u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_CONSTANT, 0u, 0u, owner);
    emit(2u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    make_drop(emit(2u, ZR_EXEC_IR_OPCODE_NOP, 0u, 0u, 0u), owner);
    emit(2u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    build();
    assert_initialized(3u, owner);
    assert_only_root(4u, ZR_EXEC_IR_STATE_AFTER_EFFECT, 0u);
}

static void test_move_checkpoint_switches_from_source_to_result(void) {
    TZrExecIrValueId owner = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_TRUE);
    TZrExecIrValueId moved = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_FALSE);
    emit(0u, ZR_EXEC_IR_OPCODE_MOVE, ZR_EXEC_IR_FLAG_DEBUG_POLL, owner, moved);
    emit(0u, ZR_EXEC_IR_OPCODE_RETURN, 0u, moved, 0u);
    build();
    assert_only_root(1u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, owner);
    assert_only_root(1u, ZR_EXEC_IR_STATE_AFTER_EFFECT, moved);
}

static void test_copy_result_cannot_restore_an_already_moved_input(void) {
    TZrExecIrValueId owner = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_TRUE);
    TZrExecIrValueId moved = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_FALSE);
    TZrExecIrValueId copied = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_FALSE);
    emit(0u, ZR_EXEC_IR_OPCODE_MOVE, 0u, owner, moved);
    emit(0u, ZR_EXEC_IR_OPCODE_COPY, 0u, owner, copied);
    emit(0u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(0u, ZR_EXEC_IR_OPCODE_RETURN, 0u, copied, 0u);
    TEST_ASSERT_FALSE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(3u, diagnostic.instructionId);
    TEST_ASSERT_EQUAL_UINT32(103u, diagnostic.sourceId);
    TEST_ASSERT_NULL(function.stateMap);
}

static void test_dead_block_move_does_not_poison_reachable_owner(void) {
    TZrExecIrValueId owner = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_TRUE);
    TZrExecIrValueId moved = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_FALSE);
    blocks(3u);
    edges(1u, 2u, 0u);
    emit(1u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    emit(3u, ZR_EXEC_IR_OPCODE_MOVE, 0u, owner, moved);
    emit(3u, ZR_EXEC_IR_OPCODE_RETURN, 0u, moved, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_RETURN, 0u, owner, 0u);
    build();
    assert_initialized(4u, owner);
}

static void test_loop_phi_reinitializes_its_moved_result(void) {
    TZrExecIrValueId condition = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_TRUE);
    TZrExecIrValueId initial = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_TRUE);
    TZrExecIrValueId moved = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_FALSE);
    TZrExecIrValueId merged = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_FALSE);
    SZrExecIrPhi phi = {0};
    SZrExecIrPhiIncoming incoming[2] = {{1u, initial}, {2u, moved}};
    blocks(3u);
    edges(1u, 2u, 0u);
    edges(2u, 2u, 3u);
    emit(1u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_MOVE, 0u, merged, moved);
    emit(2u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 0u, condition, 0u);
    emit(3u, ZR_EXEC_IR_OPCODE_RETURN, 0u, moved, 0u);
    phi.result = merged;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendPhiIncoming(
            &function, incoming, 2u, &phi.incomings));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendPhis(
            &function, &phi, 1u, &function.blocks[1].phis));
    build();
    assert_initialized(2u, merged);
}

static void test_consumer_rejects_invalid_cfg_edge_before_analysis(void) {
    TZrExecIrValueId owner = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_TRUE);
    SZrExecIrMaterializedState target;
    const SZrExecIrStateMapEntry *entry;
    blocks(2u);
    edges(1u, 2u, 0u);
    emit(1u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_RETURN, 0u, owner, 0u);
    build();
    entry = checkpoint(2u, ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    function.successors[0] = UINT32_MAX;
    ZrCore_ExecIr_MaterializedStateInit(&target);
    TEST_ASSERT_FALSE(resume(entry, &target));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(1u, diagnostic.blockId);
    TEST_ASSERT_NULL(target.values);
    ZrCore_ExecIr_MaterializedStateFree(&target);
}

static void test_phi_cannot_reinitialize_an_already_moved_input(void) {
    TZrExecIrValueId owner = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_TRUE);
    TZrExecIrValueId copied = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_FALSE);
    TZrExecIrValueId merged = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_FALSE);
    SZrExecIrPhi phi = {0};
    SZrExecIrPhiIncoming incoming = {1u, owner};
    SZrExecIrMaterializedState target;
    const SZrExecIrStateMapEntry *entry;
    SZrExecIrStateMap *published;
    blocks(2u);
    edges(1u, 2u, 0u);
    emit(1u, ZR_EXEC_IR_OPCODE_COPY, 0u, owner, copied);
    emit(1u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_RETURN, 0u, merged, 0u);
    phi.result = merged;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendPhiIncoming(
            &function, &incoming, 1u, &phi.incomings));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendPhis(
            &function, &phi, 1u, &function.blocks[1].phis));
    build();
    published = function.stateMap;
    entry = checkpoint(3u, ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    function.instructions[0].opcode = ZR_EXEC_IR_OPCODE_MOVE;
    ZrCore_ExecIr_MaterializedStateInit(&target);
    TEST_ASSERT_FALSE(resume(entry, &target));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(3u, diagnostic.instructionId);
    TEST_ASSERT_EQUAL_UINT32(103u, diagnostic.sourceId);
    TEST_ASSERT_NULL(target.values);
    TEST_ASSERT_FALSE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(3u, diagnostic.instructionId);
    TEST_ASSERT_EQUAL_PTR(published, function.stateMap);
    ZrCore_ExecIr_MaterializedStateFree(&target);
}

static void test_consumer_rejects_invalid_result_before_analysis(void) {
    TZrExecIrValueId owner = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_TRUE);
    TZrExecIrValueId moved = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_FALSE);
    SZrExecIrMaterializedState target;
    const SZrExecIrStateMapEntry *entry;
    emit(0u, ZR_EXEC_IR_OPCODE_MOVE, ZR_EXEC_IR_FLAG_DEBUG_POLL, owner, moved);
    emit(0u, ZR_EXEC_IR_OPCODE_RETURN, 0u, moved, 0u);
    build();
    entry = checkpoint(1u, ZR_EXEC_IR_STATE_AFTER_EFFECT);
    function.results[0] = function.valueCount + 1u;
    ZrCore_ExecIr_MaterializedStateInit(&target);
    TEST_ASSERT_FALSE(resume(entry, &target));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(1u, diagnostic.instructionId);
    TEST_ASSERT_EQUAL_UINT32(101u, diagnostic.sourceId);
    TEST_ASSERT_NULL(target.values);
    ZrCore_ExecIr_MaterializedStateFree(&target);
}

static void test_builder_rejects_uninitialized_deopt_value(void) {
    TZrExecIrValueId owner = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_FALSE);
    emit(0u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_DEBUG_POLL, 0u, 0u);
    function.instructions[0].deoptId = 91u;
    function.deoptStates = (SZrExecIrDeoptState *)calloc(1u, sizeof(*function.deoptStates));
    function.deoptValues = (TZrExecIrValueId *)malloc(sizeof(*function.deoptValues));
    TEST_ASSERT_NOT_NULL(function.deoptStates);
    TEST_ASSERT_NOT_NULL(function.deoptValues);
    function.deoptStateCount = function.deoptStateCapacity = 1u;
    function.deoptValueCount = function.deoptValueCapacity = 1u;
    function.deoptValues[0] = owner;
    function.deoptStates[0].id = 91u;
    function.deoptStates[0].sourceId = 101u;
    function.deoptStates[0].resumeId = 701u;
    function.deoptStates[0].valueRange.count = 1u;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(1u, diagnostic.instructionId);
    TEST_ASSERT_EQUAL_UINT32(101u, diagnostic.sourceId);
    TEST_ASSERT_NULL(function.stateMap);
}

static void test_builder_preserves_map_on_each_owner_analysis_allocation_failure(void) {
    TZrExecIrValueId owner = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_TRUE);
    SZrExecIrStateMap *published;
    size_t ordinal;
    emit(0u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(0u, ZR_EXEC_IR_OPCODE_RETURN, 0u, owner, 0u);
    build();
    published = function.stateMap;
    for (ordinal = 1u; ordinal < 32u; ++ordinal) {
        TZrBool result;
        int failed;
        ssa_owner_fail_allocation(ordinal);
        result = ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic);
        failed = ssa_owner_allocation_failed();
        ssa_owner_fail_allocation(0u);
        TEST_ASSERT_EQUAL_UINT32(0u, ssa_owner_outstanding_allocations());
        if (result) {
            TEST_ASSERT_FALSE(failed);
            break;
        }
        TEST_ASSERT_TRUE(failed);
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, diagnostic.code);
        TEST_ASSERT_EQUAL_UINT32(99u, diagnostic.functionToken);
        TEST_ASSERT_EQUAL_PTR(published, function.stateMap);
        TEST_ASSERT_EQUAL_UINT32(owner, function.stateMap->rootPool[0]);
    }
    TEST_ASSERT_GREATER_THAN_UINT32(1u, ordinal);
    TEST_ASSERT_LESS_THAN_UINT32(32u, ordinal);
    assert_initialized(1u, owner);
}

static void test_materializer_preserves_target_on_each_owner_analysis_allocation_failure(void) {
    TZrExecIrValueId owner = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_TRUE);
    SZrExecIrMaterializedState target;
    const SZrExecIrStateMapEntry *entry;
    TZrExecIrValueId *published;
    size_t ordinal;
    emit(0u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(0u, ZR_EXEC_IR_OPCODE_RETURN, 0u, owner, 0u);
    build();
    entry = checkpoint(1u, ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    ZrCore_ExecIr_MaterializedStateInit(&target);
    TEST_ASSERT_TRUE(resume(entry, &target));
    published = target.values;
    for (ordinal = 1u; ordinal < 32u; ++ordinal) {
        TZrBool result;
        int failed;
        ssa_owner_fail_allocation(ordinal);
        result = resume(entry, &target);
        failed = ssa_owner_allocation_failed();
        ssa_owner_fail_allocation(0u);
        TEST_ASSERT_EQUAL_UINT32(0u, ssa_owner_outstanding_allocations());
        if (result) {
            TEST_ASSERT_FALSE(failed);
            break;
        }
        TEST_ASSERT_TRUE(failed);
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, diagnostic.code);
        TEST_ASSERT_EQUAL_UINT32(99u, diagnostic.functionToken);
        TEST_ASSERT_EQUAL_PTR(published, target.values);
        TEST_ASSERT_EQUAL_UINT32(owner, target.roots[0]);
    }
    TEST_ASSERT_GREATER_THAN_UINT32(1u, ordinal);
    TEST_ASSERT_LESS_THAN_UINT32(32u, ordinal);
    ZrCore_ExecIr_MaterializedStateFree(&target);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sibling_drop_does_not_consume_live_owner);
    RUN_TEST(test_sibling_move_does_not_consume_live_owner);
    RUN_TEST(test_consumer_rejects_ambiguous_join_owner_without_replacing_target);
    RUN_TEST(test_loop_carried_move_rejects_live_checkpoint_and_preserves_map);
    RUN_TEST(test_loop_definition_reinitializes_previously_dropped_owner);
    RUN_TEST(test_move_checkpoint_switches_from_source_to_result);
    RUN_TEST(test_copy_result_cannot_restore_an_already_moved_input);
    RUN_TEST(test_dead_block_move_does_not_poison_reachable_owner);
    RUN_TEST(test_loop_phi_reinitializes_its_moved_result);
    RUN_TEST(test_phi_cannot_reinitialize_an_already_moved_input);
    RUN_TEST(test_consumer_rejects_invalid_cfg_edge_before_analysis);
    RUN_TEST(test_consumer_rejects_invalid_result_before_analysis);
    RUN_TEST(test_builder_rejects_uninitialized_deopt_value);
    RUN_TEST(test_builder_preserves_map_on_each_owner_analysis_allocation_failure);
    RUN_TEST(test_materializer_preserves_target_on_each_owner_analysis_allocation_failure);
    return UNITY_END();
}
