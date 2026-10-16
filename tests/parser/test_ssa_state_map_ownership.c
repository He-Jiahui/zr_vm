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
    SZrExecIrPhiIncoming effects[2] = {{1u, 0u}, {2u, 2u}};
    blocks(2u);
    edges(1u, 2u, 0u);
    edges(2u, 2u, 0u);
    emit(1u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_CONSTANT, 0u, 0u, owner);
    emit(2u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    make_drop(emit(2u, ZR_EXEC_IR_OPCODE_NOP, 0u, 0u, 0u), owner);
    emit(2u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionSetEffectPhi(
            &function, 2u, 1u, effects, 2u));
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

/**
 * @fn static void make_drop(TZrExecIrInstructionId id, TZrExecIrValueId owner)
 *
 * ## Summary
 * 把全局 function 中已有的一条指令改为普通 DROP，清 flags、设置 effectIn=1/effectOut=2，并追加一个 owner operand 和两个独立 memory token 范围。
 *
 * ## Parameters
 * @param[in] id 已有指令的1起始ID，调用方保证 instruction 存在；本 helper 不检查边界。
 * @param[in] owner 已有的有效 ValueId，用作普通 DROP 的唯一 operand；ownership/CFG/effect 合法性由后续 build/Verify 检查。
 *
 * ## Returns
 * 无 C 返回值。三个追加调用的 TEST_ASSERT_TRUE 全部通过则正常返回；失败由 Unity 退出当前 case，不用返回码传播。
 *
 * ## Constraints and lifecycle
 * id 和 owner 均由固定测试 fixture 提供。helper 直接取得 instructions[id-1]，不验证ID、owner类别、当前CFG或固定token与原图是否一致；这些须由构造和后续 Verify/build 保证。
 * append 只复制局部 token/ID 数据到 function 所有的池，不持有栈上 tokens 指针。它们不重分配 instruction 池，取得的指令指针在这些追加间继续有效。
 * opcode/effect 字段在追加前已经修改；任一后续断言失败可留下部分已追加元数据，不回滚。共享 tearDown 释放全局 function，但此处不替其他局部对象安排清理。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_ownership.c:5
 * - tests/parser/test_ssa_state_map_ownership.c:7
 * - tests/parser/test_ssa_state_map_ownership.c:9
 * - tests/parser/test_ssa_state_map_ownership.c:11
 * - tests/parser/test_ssa_state_map_ownership.c:13
 * - tests/parser/test_ssa_state_map_ownership.c:15
 * - tests/parser/test_ssa_state_map_ownership.c:54
 * - tests/parser/test_ssa_state_map_ownership.c:96
 * - tests/parser/test_ssa_state_map_ownership.c:141
 * - tests/parser/ssa_state_map_fixture.h:25
 * - tests/parser/ssa_state_map_fixture.h:158
 * - tests/parser/ssa_state_map_fixture.h:162
 * - tests/parser/ssa_state_map_fixture.h:170
 */

/**
 * @fn static void assert_initialized(TZrExecIrInstructionId id, TZrExecIrValueId owner)
 *
 * ## Summary
 * 检查当前 checkpoint 在 BEFORE/AFTER 两个 phase 均只有 owner 这一 root；再遍历 BEFORE entry 的 liveValues，对其中等于 owner 的槽断言存储 owner 状态为 INITIALIZED。
 *
 * ## Parameters
 * @param[in] id 当前已构建 map 中 checkpoint 的1起始指令ID。
 * @param[in] owner 预期保活的唯一 owner ID，须在当前 function/map 解释上下文内有效。
 *
 * ## Returns
 * 无 C 返回值；所有实际断言通过则正常返回，失败交由 Unity 退出当前 case并进入共享 teardown/后续 runner 流程。
 *
 * ## Constraints and lifecycle
 * 先取得 BEFORE entry 的借用指针，再调用 assert_only_root 两次；这些调用只为局部 target 做物化，不替换 map。调用期间 function/map 不得被重建或释放。
 * 两次 root 检查包含实际 MaterializeState 以及 root count/ID 断言。ownerState 的显式枚举断言只来自 BEFORE entry；不把它表述为 AFTER ownerState 已逐项检查。
 * 此处对 liveValues 逐槽匹配，没有另设“至少匹配一次”的独立断言；整体 map 验证的跨池约束由被调用生产路径提供。
 * 局部 entry 为 map 借用，不释放。若 assert_only_root 在成功物化后断言失败，其本地 target Free 可被 longjmp 跳过；共享 tearDown 只负责全局 function。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_ownership.c:20
 * - tests/parser/test_ssa_state_map_ownership.c:22
 * - tests/parser/test_ssa_state_map_ownership.c:23
 * - tests/parser/test_ssa_state_map_ownership.c:24
 * - tests/parser/test_ssa_state_map_ownership.c:26
 * - tests/parser/test_ssa_state_map_ownership.c:62
 * - tests/parser/test_ssa_state_map_ownership.c:146
 * - tests/parser/test_ssa_state_map_ownership.c:186
 * - tests/parser/test_ssa_state_map_ownership.c:210
 * - tests/parser/test_ssa_state_map_ownership.c:339
 * - tests/parser/ssa_state_map_fixture.h:114
 * - tests/parser/ssa_state_map_fixture.h:126
 * - tests/parser/ssa_state_map_fixture.h:130
 * - tests/parser/ssa_state_map_fixture.h:135
 * - tests/parser/ssa_state_map_fixture.h:150
 * - tests/parser/ssa_state_map_fixture.h:151
 * - tests/parser/ssa_state_map_fixture.h:155
 */

/**
 * @fn static TZrBool resume(const SZrExecIrStateMapEntry *entry, SZrExecIrMaterializedState *target)
 *
 * ## Summary
 * 从全局 function 及其当前 stateMap 复制身份字段，并从 entry 复制 sourceId、resumeId、phase，将 target 转发给 MaterializeState。
 *
 * ## Parameters
 * @param[in] entry 当前function/map或其测试栈副本中的有效entry记录；调用内稳定可读，提供source/resume/phase。
 * @param[in,out] target 可为NULL以仅验证；非空时须已Init或持有合法旧输出，满足 MaterializeState 的独立存储/所有权前提。
 *
 * ## Returns
 * @return 返回 MaterializeState 的 TZrBool 原值，diagnostic 写入全局测试诊断；本 helper 无自己的断言或独立错误映射。
 *
 * ## Constraints and lifecycle
 * entry 必须非NULL且可读。request 零初始化后只写函数、map、token、generation、source/resume/phase与target；signatureHash 仍为0，ownerStates/objects 等其他可选输入为空，不能据此授予非零签名请求或owner witness路径覆盖。
 * 输入仅在同步调用内借用，不保存 request/entry 指针或获得 function/map/target 所有权。实际alias与target所有权、失败保持/成功替换遵循生产 MaterializeState 合同，本 helper不重新验证。
 * 如target非NULL，正常测试路径由调用方显式 Free；FALSE 不由helper直接回收旧target。成功后仅旧 target 的池借用按生产提交语义失效，记录本身仍由caller拥有。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_ownership.c:34
 * - tests/parser/test_ssa_state_map_ownership.c:35
 * - tests/parser/test_ssa_state_map_ownership.c:37
 * - tests/parser/test_ssa_state_map_ownership.c:39
 * - tests/parser/test_ssa_state_map_ownership.c:42
 * - tests/parser/test_ssa_state_map_ownership.c:43
 * - tests/parser/test_ssa_state_map_ownership.c:94
 * - tests/parser/test_ssa_state_map_ownership.c:100
 * - tests/parser/test_ssa_state_map_ownership.c:226
 * - tests/parser/test_ssa_state_map_ownership.c:258
 * - tests/parser/test_ssa_state_map_ownership.c:281
 * - tests/parser/test_ssa_state_map_ownership.c:353
 * - tests/parser/test_ssa_state_map_ownership.c:359
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:666
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:819
 */

/**
 * @fn static void sibling_transition(TZrBool drop)
 *
 * ## Summary
 * 在三块分支图中让一条 successor 消费外部 UNIQUE owner，另一条 successor 在 MAY_GC checkpoint 后仍返回原 owner，检查分支间 owner 状态不会错误串联。
 *
 * ## Parameters
 * @param[in] drop 非零在一条sibling分支构造DROP，为0则构造MOVE；两种路径都由各自case调用。
 *
 * ## Returns
 * 无 C 返回值；完成 fixture build 和 assert_initialized 的断言后正常返回，失败由 Unity 结束当前case。
 *
 * ## Constraints and lifecycle
 * 共享 setUp 提供空 function。helper追加condition/owner/moved、三块与边，再按drop选择普通DROP或MOVE。drop路径也预先声明moved，但不把它当本分支可用值。
 * build 执行前驱构建、完整Verify和BuildStateMaps；随后检查指令4两phase root与before owner状态，证据来自固定图，未执行runtime DROP/MOVE payload。
 * 所有追加池归全局 function，正常或断言失败后的共享 tearDown 释放；本helper没有独立堆对象，但其调用的物化assert helper可能在失败断言时跳过本地target清理。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_ownership.c:47
 * - tests/parser/test_ssa_state_map_ownership.c:50
 * - tests/parser/test_ssa_state_map_ownership.c:51
 * - tests/parser/test_ssa_state_map_ownership.c:53
 * - tests/parser/test_ssa_state_map_ownership.c:54
 * - tests/parser/test_ssa_state_map_ownership.c:56
 * - tests/parser/test_ssa_state_map_ownership.c:59
 * - tests/parser/test_ssa_state_map_ownership.c:60
 * - tests/parser/test_ssa_state_map_ownership.c:61
 * - tests/parser/test_ssa_state_map_ownership.c:62
 * - tests/parser/test_ssa_state_map_ownership.c:66
 * - tests/parser/test_ssa_state_map_ownership.c:70
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/parser/ssa_state_map_fixture.h:158
 */

/**
 * @fn static void test_sibling_drop_does_not_consume_live_owner(void)
 *
 * ## Summary
 * 在一条 sibling 路径 DROP owner；另一条 sibling 的 checkpoint 经 assert_initialized 检查 before/after root 都为 owner，并检查 before entry 中该 live owner 的存储状态为 INITIALIZED。
 *
 * ## Returns
 * 无 C 返回值；本case的场景断言通过则正常返回。TEST_ASSERT失败由Unity跳出当前case，runner再执行tearDown并继续后续case，失败计数由最终UNITY_END返回；不是终止整个测试进程。
 *
 * ## Constraints and lifecycle
 * 不逐项断言 after 的 ownerState 枚举；不执行实际 DROP payload。
 * 此case在共享setUp建立的固定function上构造/修改输入。图、值、map等归全局function；共享tearDown释放这些池，不负责释放未挂到function的本地MaterializedState。
 * 该case自身没有独立MaterializedState的最终释放语句；调用的共享断言helper可能临时物化并在正常路径Free，其失败longjmp边界仍适用。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_ownership.c:65
 * - tests/parser/test_ssa_state_map_ownership.c:54
 * - tests/parser/test_ssa_state_map_ownership.c:59
 * - tests/parser/test_ssa_state_map_ownership.c:60
 * - tests/parser/test_ssa_state_map_ownership.c:61
 * - tests/parser/test_ssa_state_map_ownership.c:62
 * - tests/parser/test_ssa_state_map_ownership.c:66
 * - tests/parser/test_ssa_state_map_ownership.c:22
 * - tests/parser/test_ssa_state_map_ownership.c:23
 * - tests/parser/test_ssa_state_map_ownership.c:26
 * - tests/parser/ssa_state_map_fixture.h:135
 * - tests/parser/ssa_state_map_fixture.h:137
 * - tests/parser/ssa_state_map_fixture.h:150
 * - tests/parser/ssa_state_map_fixture.h:151
 * - tests/parser/ssa_state_map_fixture.h:153
 * - tests/parser/ssa_state_map_fixture.h:155
 * - tests/parser/test_ssa_state_map_ownership.c:380
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 */

/**
 * @fn static void test_sibling_move_does_not_consume_live_owner(void)
 *
 * ## Summary
 * 在一条 sibling 路径 MOVE owner；另一条 sibling 的相同 checkpoint 断言两 phase 的 root 为原 owner，before 存储状态为 INITIALIZED。
 *
 * ## Returns
 * 无 C 返回值；本case的场景断言通过则正常返回。TEST_ASSERT失败由Unity跳出当前case，runner再执行tearDown并继续后续case，失败计数由最终UNITY_END返回；不是终止整个测试进程。
 *
 * ## Constraints and lifecycle
 * 不证明任意分支组合或 runtime MOVE 语义。
 * 此case在共享setUp建立的固定function上构造/修改输入。图、值、map等归全局function；共享tearDown释放这些池，不负责释放未挂到function的本地MaterializedState。
 * 该case自身没有独立MaterializedState的最终释放语句；调用的共享断言helper可能临时物化并在正常路径Free，其失败longjmp边界仍适用。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_ownership.c:69
 * - tests/parser/test_ssa_state_map_ownership.c:56
 * - tests/parser/test_ssa_state_map_ownership.c:59
 * - tests/parser/test_ssa_state_map_ownership.c:60
 * - tests/parser/test_ssa_state_map_ownership.c:61
 * - tests/parser/test_ssa_state_map_ownership.c:62
 * - tests/parser/test_ssa_state_map_ownership.c:70
 * - tests/parser/test_ssa_state_map_ownership.c:22
 * - tests/parser/test_ssa_state_map_ownership.c:23
 * - tests/parser/test_ssa_state_map_ownership.c:26
 * - tests/parser/ssa_state_map_fixture.h:135
 * - tests/parser/ssa_state_map_fixture.h:137
 * - tests/parser/ssa_state_map_fixture.h:150
 * - tests/parser/ssa_state_map_fixture.h:151
 * - tests/parser/ssa_state_map_fixture.h:153
 * - tests/parser/ssa_state_map_fixture.h:155
 * - tests/parser/test_ssa_state_map_ownership.c:381
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 */

/**
 * @fn static void test_consumer_rejects_ambiguous_join_owner_without_replacing_target(void)
 *
 * ## Summary
 * 先成功 resume 并保存 target.values；后在一个分支插 DROP，伪造旧 map 的 owner 状态为 DROPPED；再次 resume 为 FALSE、STATE_MAP_INVALID，instructionId=5、sourceId=105，旧 values 指针和 roots[0]=owner 保留。
 *
 * ## Returns
 * 无 C 返回值；本case的场景断言通过则正常返回。TEST_ASSERT失败由Unity跳出当前case，runner再执行tearDown并继续后续case，失败计数由最终UNITY_END返回；不是终止整个测试进程。
 *
 * ## Constraints and lifecycle
 * 不逐一比较 target 五个池的所有字段/元素，也未检查完整旧输出字节。
 * 此case在共享setUp建立的固定function上构造/修改输入。图、值、map等归全局function；共享tearDown释放这些池，不负责释放未挂到function的本地MaterializedState。
 * 正常路径在tests/parser/test_ssa_state_map_ownership.c:106显式Free本地target；此前任一Unity断言longjmp都可能跳过该语句，不能承诺失败断言下回收所有局部资源。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_ownership.c:73
 * - tests/parser/test_ssa_state_map_ownership.c:89
 * - tests/parser/test_ssa_state_map_ownership.c:90
 * - tests/parser/test_ssa_state_map_ownership.c:94
 * - tests/parser/test_ssa_state_map_ownership.c:96
 * - tests/parser/test_ssa_state_map_ownership.c:98
 * - tests/parser/test_ssa_state_map_ownership.c:100
 * - tests/parser/test_ssa_state_map_ownership.c:101
 * - tests/parser/test_ssa_state_map_ownership.c:102
 * - tests/parser/test_ssa_state_map_ownership.c:103
 * - tests/parser/test_ssa_state_map_ownership.c:104
 * - tests/parser/test_ssa_state_map_ownership.c:105
 * - tests/parser/test_ssa_state_map_ownership.c:106
 * - tests/parser/test_ssa_state_map_ownership.c:382
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 */

/**
 * @fn static void test_loop_carried_move_rejects_live_checkpoint_and_preserves_map(void)
 *
 * ## Summary
 * 已成功建图后将 loop 中 COPY 改为 MOVE；BuildStateMaps 返回 FALSE、STATE_MAP_INVALID，instructionId=2、sourceId=102，原 stateMap 精确指针保留。
 *
 * ## Returns
 * 无 C 返回值；本case的场景断言通过则正常返回。TEST_ASSERT失败由Unity跳出当前case，runner再执行tearDown并继续后续case，失败计数由最终UNITY_END返回；不是终止整个测试进程。
 *
 * ## Constraints and lifecycle
 * 只断言 map 指针不替换，未逐字比较旧 map 全池。
 * 此case在共享setUp建立的固定function上构造或修改输入；由全局function拥有的图、值、map及已挂接deopt池在tearDown中释放。
 * 本case没有独立MaterializedState，也不调用临时物化断言helper；新增池归全局function，由tearDown释放。Unity失败不会自动回收其它尚未发布的局部资源。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_ownership.c:109
 * - tests/parser/test_ssa_state_map_ownership.c:122
 * - tests/parser/test_ssa_state_map_ownership.c:124
 * - tests/parser/test_ssa_state_map_ownership.c:125
 * - tests/parser/test_ssa_state_map_ownership.c:126
 * - tests/parser/test_ssa_state_map_ownership.c:127
 * - tests/parser/test_ssa_state_map_ownership.c:128
 * - tests/parser/test_ssa_state_map_ownership.c:129
 * - tests/parser/test_ssa_state_map_ownership.c:383
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 */

/**
 * @fn static void test_loop_definition_reinitializes_previously_dropped_owner(void)
 *
 * ## Summary
 * loop 内 CONSTANT 重新定义 owner；assert_initialized 检查 checkpoint 两 phase root 和 before 存储状态；DROP 后 AFTER checkpoint 的 root 数为0。
 *
 * ## Returns
 * 无 C 返回值；本case的场景断言通过则正常返回。TEST_ASSERT失败由Unity跳出当前case，runner再执行tearDown并继续后续case，失败计数由最终UNITY_END返回；不是终止整个测试进程。
 *
 * ## Constraints and lifecycle
 * 该固定 CFG 不证明所有循环终止/PHI 情形，也未运行 payload 分配释放。
 * 此case在共享setUp建立的固定function上构造/修改输入。图、值、map等归全局function；共享tearDown释放这些池，不负责释放未挂到function的本地MaterializedState。
 * 该case自身没有独立MaterializedState的最终释放语句；调用的共享断言helper可能临时物化并在正常路径Free，其失败longjmp边界仍适用。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_ownership.c:132
 * - tests/parser/test_ssa_state_map_ownership.c:139
 * - tests/parser/test_ssa_state_map_ownership.c:141
 * - tests/parser/test_ssa_state_map_ownership.c:143
 * - tests/parser/test_ssa_state_map_ownership.c:145
 * - tests/parser/test_ssa_state_map_ownership.c:146
 * - tests/parser/test_ssa_state_map_ownership.c:147
 * - tests/parser/test_ssa_state_map_ownership.c:22
 * - tests/parser/test_ssa_state_map_ownership.c:23
 * - tests/parser/test_ssa_state_map_ownership.c:26
 * - tests/parser/ssa_state_map_fixture.h:135
 * - tests/parser/ssa_state_map_fixture.h:137
 * - tests/parser/ssa_state_map_fixture.h:150
 * - tests/parser/ssa_state_map_fixture.h:151
 * - tests/parser/ssa_state_map_fixture.h:153
 * - tests/parser/ssa_state_map_fixture.h:155
 * - tests/parser/test_ssa_state_map_ownership.c:384
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 */

/**
 * @fn static void test_move_checkpoint_switches_from_source_to_result(void)
 *
 * ## Summary
 * 单条带 DEBUG_POLL 的 MOVE，在 BEFORE checkpoint 只含源 owner root，在 AFTER checkpoint 只含 moved root；共享 helper 也实际 Materialize 并核对 root 数/首 ID。
 *
 * ## Returns
 * 无 C 返回值；本case的场景断言通过则正常返回。TEST_ASSERT失败由Unity跳出当前case，runner再执行tearDown并继续后续case，失败计数由最终UNITY_END返回；不是终止整个测试进程。
 *
 * ## Constraints and lifecycle
 * 此用例直接断言 root ID，未直接调用 raw OwnerStateAt 比较两个状态枚举。
 * 此case在共享setUp建立的固定function上构造/修改输入。图、值、map等归全局function；共享tearDown释放这些池，不负责释放未挂到function的本地MaterializedState。
 * 该case自身没有独立MaterializedState的最终释放语句；调用的共享断言helper可能临时物化并在正常路径Free，其失败longjmp边界仍适用。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_ownership.c:150
 * - tests/parser/test_ssa_state_map_ownership.c:153
 * - tests/parser/test_ssa_state_map_ownership.c:155
 * - tests/parser/test_ssa_state_map_ownership.c:156
 * - tests/parser/test_ssa_state_map_ownership.c:157
 * - tests/parser/ssa_state_map_fixture.h:135
 * - tests/parser/ssa_state_map_fixture.h:137
 * - tests/parser/ssa_state_map_fixture.h:150
 * - tests/parser/ssa_state_map_fixture.h:151
 * - tests/parser/ssa_state_map_fixture.h:153
 * - tests/parser/ssa_state_map_fixture.h:155
 * - tests/parser/test_ssa_state_map_ownership.c:385
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 */

/**
 * @fn static void test_copy_result_cannot_restore_an_already_moved_input(void)
 *
 * ## Summary
 * MOVE 后又 COPY 已消费源，后续 live checkpoint 的 BuildStateMaps 返回 FALSE、STATE_MAP_INVALID，instructionId=3、sourceId=103，stateMap 保持 NULL。
 *
 * ## Returns
 * 无 C 返回值；本case的场景断言通过则正常返回。TEST_ASSERT失败由Unity跳出当前case，runner再执行tearDown并继续后续case，失败计数由最终UNITY_END返回；不是终止整个测试进程。
 *
 * ## Constraints and lifecycle
 * 这是 builder 消费 owner 分析结果后的拒绝，不表示 OwnerAnalysisBuild 自身对每个 consumed use 都返回 FALSE。
 * 此case在共享setUp建立的固定function上构造或修改输入；由全局function拥有的图、值、map及已挂接deopt池在tearDown中释放。
 * 本case没有独立MaterializedState，也不调用临时物化断言helper；新增池归全局function，由tearDown释放。Unity失败不会自动回收其它尚未发布的局部资源。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_ownership.c:160
 * - tests/parser/test_ssa_state_map_ownership.c:164
 * - tests/parser/test_ssa_state_map_ownership.c:165
 * - tests/parser/test_ssa_state_map_ownership.c:166
 * - tests/parser/test_ssa_state_map_ownership.c:168
 * - tests/parser/test_ssa_state_map_ownership.c:169
 * - tests/parser/test_ssa_state_map_ownership.c:170
 * - tests/parser/test_ssa_state_map_ownership.c:171
 * - tests/parser/test_ssa_state_map_ownership.c:172
 * - tests/parser/test_ssa_state_map_ownership.c:386
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 */

/**
 * @fn static void test_dead_block_move_does_not_poison_reachable_owner(void)
 *
 * ## Summary
 * 一个未从 entry 连通的 block 中 MOVE owner，不影响 reachable checkpoint 的两 phase root 与 before INITIALIZED 存储状态。
 *
 * ## Returns
 * 无 C 返回值；本case的场景断言通过则正常返回。TEST_ASSERT失败由Unity跳出当前case，runner再执行tearDown并继续后续case，失败计数由最终UNITY_END返回；不是终止整个测试进程。
 *
 * ## Constraints and lifecycle
 * 不直接检查 dead block 的 reachable 数组或 raw query 为0。
 * 此case在共享setUp建立的固定function上构造/修改输入。图、值、map等归全局function；共享tearDown释放这些池，不负责释放未挂到function的本地MaterializedState。
 * 该case自身没有独立MaterializedState的最终释放语句；调用的共享断言helper可能临时物化并在正常路径Free，其失败longjmp边界仍适用。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_ownership.c:175
 * - tests/parser/test_ssa_state_map_ownership.c:179
 * - tests/parser/test_ssa_state_map_ownership.c:181
 * - tests/parser/test_ssa_state_map_ownership.c:183
 * - tests/parser/test_ssa_state_map_ownership.c:185
 * - tests/parser/test_ssa_state_map_ownership.c:186
 * - tests/parser/test_ssa_state_map_ownership.c:22
 * - tests/parser/test_ssa_state_map_ownership.c:23
 * - tests/parser/test_ssa_state_map_ownership.c:26
 * - tests/parser/ssa_state_map_fixture.h:135
 * - tests/parser/ssa_state_map_fixture.h:137
 * - tests/parser/ssa_state_map_fixture.h:150
 * - tests/parser/ssa_state_map_fixture.h:151
 * - tests/parser/ssa_state_map_fixture.h:153
 * - tests/parser/ssa_state_map_fixture.h:155
 * - tests/parser/test_ssa_state_map_ownership.c:387
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 */

/**
 * @fn static void test_loop_phi_reinitializes_its_moved_result(void)
 *
 * ## Summary
 * loop PHI 从 entry initial 与 backedge moved 值定义 merged，随后 MOVE merged；checkpoint 断言 merged 两 phase root 与 before INITIALIZED 状态。
 *
 * ## Returns
 * 无 C 返回值；本case的场景断言通过则正常返回。TEST_ASSERT失败由Unity跳出当前case，runner再执行tearDown并继续后续case，失败计数由最终UNITY_END返回；不是终止整个测试进程。
 *
 * ## Constraints and lifecycle
 * 仅该 PHI/loop fixture，未直接比较所有 incoming mask，也未测试任意 PHI source/destination 别名。
 * 此case在共享setUp建立的固定function上构造/修改输入。图、值、map等归全局function；共享tearDown释放这些池，不负责释放未挂到function的本地MaterializedState。
 * 该case自身没有独立MaterializedState的最终释放语句；调用的共享断言helper可能临时物化并在正常路径Free，其失败longjmp边界仍适用。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_ownership.c:189
 * - tests/parser/test_ssa_state_map_ownership.c:195
 * - tests/parser/test_ssa_state_map_ownership.c:201
 * - tests/parser/test_ssa_state_map_ownership.c:205
 * - tests/parser/test_ssa_state_map_ownership.c:207
 * - tests/parser/test_ssa_state_map_ownership.c:209
 * - tests/parser/test_ssa_state_map_ownership.c:210
 * - tests/parser/test_ssa_state_map_ownership.c:22
 * - tests/parser/test_ssa_state_map_ownership.c:23
 * - tests/parser/test_ssa_state_map_ownership.c:26
 * - tests/parser/ssa_state_map_fixture.h:135
 * - tests/parser/ssa_state_map_fixture.h:137
 * - tests/parser/ssa_state_map_fixture.h:150
 * - tests/parser/ssa_state_map_fixture.h:151
 * - tests/parser/ssa_state_map_fixture.h:153
 * - tests/parser/ssa_state_map_fixture.h:155
 * - tests/parser/test_ssa_state_map_ownership.c:388
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 */

/**
 * @fn static void test_phi_cannot_reinitialize_an_already_moved_input(void)
 *
 * ## Summary
 * 成功建图后将 PHI incoming 的原 COPY 改为 MOVE；resume 返回 FALSE、STATE_MAP_INVALID，instructionId=3、sourceId=103，空 target.values 仍 NULL；builder 同样拒绝、instructionId=3，原 map 指针保留。
 *
 * ## Returns
 * 无 C 返回值；本case的场景断言通过则正常返回。TEST_ASSERT失败由Unity跳出当前case，runner再执行tearDown并继续后续case，失败计数由最终UNITY_END返回；不是终止整个测试进程。
 *
 * ## Constraints and lifecycle
 * 不直接逐步观察 PHI 内部掩码；仅经消费者的结果和诊断验证该场景。
 * 此case在共享setUp建立的固定function上构造/修改输入。图、值、map等归全局function；共享tearDown释放这些池，不负责释放未挂到function的本地MaterializedState。
 * 正常路径在tests/parser/test_ssa_state_map_ownership.c:267显式Free本地target；此前任一Unity断言longjmp都可能跳过该语句，不能承诺失败断言下回收所有局部资源。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_ownership.c:233
 * - tests/parser/test_ssa_state_map_ownership.c:253
 * - tests/parser/test_ssa_state_map_ownership.c:256
 * - tests/parser/test_ssa_state_map_ownership.c:258
 * - tests/parser/test_ssa_state_map_ownership.c:259
 * - tests/parser/test_ssa_state_map_ownership.c:260
 * - tests/parser/test_ssa_state_map_ownership.c:261
 * - tests/parser/test_ssa_state_map_ownership.c:262
 * - tests/parser/test_ssa_state_map_ownership.c:263
 * - tests/parser/test_ssa_state_map_ownership.c:264
 * - tests/parser/test_ssa_state_map_ownership.c:265
 * - tests/parser/test_ssa_state_map_ownership.c:266
 * - tests/parser/test_ssa_state_map_ownership.c:267
 * - tests/parser/test_ssa_state_map_ownership.c:389
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 */

/**
 * @fn static void test_consumer_rejects_invalid_cfg_edge_before_analysis(void)
 *
 * ## Summary
 * 成功建图后把 successor[0] 改为 UINT32_MAX；resume 返回 FALSE、STATE_MAP_INVALID、blockId=1，空 target.values 保持 NULL。
 *
 * ## Returns
 * 无 C 返回值；本case的场景断言通过则正常返回。TEST_ASSERT失败由Unity跳出当前case，runner再执行tearDown并继续后续case，失败计数由最终UNITY_END返回；不是终止整个测试进程。
 *
 * ## Constraints and lifecycle
 * 实际断言不单独调用内部 validator；结合源码路径可定位输入检查，但没有独立原始 analysis 状态断言。
 * 此case在共享setUp建立的固定function上构造/修改输入。图、值、map等归全局function；共享tearDown释放这些池，不负责释放未挂到function的本地MaterializedState。
 * 正常路径在tests/parser/test_ssa_state_map_ownership.c:230显式Free本地target；此前任一Unity断言longjmp都可能跳过该语句，不能承诺失败断言下回收所有局部资源。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_ownership.c:213
 * - tests/parser/test_ssa_state_map_ownership.c:222
 * - tests/parser/test_ssa_state_map_ownership.c:224
 * - tests/parser/test_ssa_state_map_ownership.c:226
 * - tests/parser/test_ssa_state_map_ownership.c:227
 * - tests/parser/test_ssa_state_map_ownership.c:228
 * - tests/parser/test_ssa_state_map_ownership.c:229
 * - tests/parser/test_ssa_state_map_ownership.c:230
 * - tests/parser/test_ssa_state_map_ownership.c:390
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 */

/**
 * @fn static void test_consumer_rejects_invalid_result_before_analysis(void)
 *
 * ## Summary
 * 成功建图后使 results[0]=valueCount+1；resume 返回 FALSE、STATE_MAP_INVALID、instructionId=1、sourceId=101，空 target.values 保持 NULL。
 *
 * ## Returns
 * 无 C 返回值；本case的场景断言通过则正常返回。TEST_ASSERT失败由Unity跳出当前case，runner再执行tearDown并继续后续case，失败计数由最终UNITY_END返回；不是终止整个测试进程。
 *
 * ## Constraints and lifecycle
 * 未覆盖所有错误 result 范围、容量或不可读地址；不把拒绝当实际内存可读性证明。
 * 此case在共享setUp建立的固定function上构造/修改输入。图、值、map等归全局function；共享tearDown释放这些池，不负责释放未挂到function的本地MaterializedState。
 * 正常路径在tests/parser/test_ssa_state_map_ownership.c:286显式Free本地target；此前任一Unity断言longjmp都可能跳过该语句，不能承诺失败断言下回收所有局部资源。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_ownership.c:270
 * - tests/parser/test_ssa_state_map_ownership.c:277
 * - tests/parser/test_ssa_state_map_ownership.c:279
 * - tests/parser/test_ssa_state_map_ownership.c:281
 * - tests/parser/test_ssa_state_map_ownership.c:282
 * - tests/parser/test_ssa_state_map_ownership.c:283
 * - tests/parser/test_ssa_state_map_ownership.c:284
 * - tests/parser/test_ssa_state_map_ownership.c:285
 * - tests/parser/test_ssa_state_map_ownership.c:286
 * - tests/parser/test_ssa_state_map_ownership.c:391
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 */

/**
 * @fn static void test_builder_rejects_uninitialized_deopt_value(void)
 *
 * ## Summary
 * 声明 non-external UNIQUE owner 但没有定义，将其放入 deopt value 列表；BuildStateMaps 返回 FALSE、STATE_MAP_INVALID、instructionId=1、sourceId=101，stateMap 保持 NULL。
 *
 * ## Returns
 * 无 C 返回值；本case的场景断言通过则正常返回。TEST_ASSERT失败由Unity跳出当前case，runner再执行tearDown并继续后续case，失败计数由最终UNITY_END返回；不是终止整个测试进程。
 *
 * ## Constraints and lifecycle
 * owner 分析本身不校验 deopt 表；此用例经 builder/deopt/liveness 依赖后拒绝，不能授予本两源完整 deopt validator 信用。
 * 此case在共享setUp建立的固定function上构造或修改输入；由全局function拥有的图、值、map及已挂接deopt池在tearDown中释放。
 * 本case没有独立MaterializedState，也不调用临时物化断言helper；新增池归全局function，由tearDown释放。Unity失败不会自动回收其它尚未发布的局部资源。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_ownership.c:289
 * - tests/parser/test_ssa_state_map_ownership.c:290
 * - tests/parser/test_ssa_state_map_ownership.c:293
 * - tests/parser/test_ssa_state_map_ownership.c:294
 * - tests/parser/test_ssa_state_map_ownership.c:299
 * - tests/parser/test_ssa_state_map_ownership.c:300
 * - tests/parser/test_ssa_state_map_ownership.c:302
 * - tests/parser/test_ssa_state_map_ownership.c:303
 * - tests/parser/test_ssa_state_map_ownership.c:304
 * - tests/parser/test_ssa_state_map_ownership.c:305
 * - tests/parser/test_ssa_state_map_ownership.c:306
 * - tests/parser/test_ssa_state_map_ownership.c:307
 * - tests/parser/test_ssa_state_map_ownership.c:308
 * - tests/parser/test_ssa_state_map_ownership.c:392
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 */

/**
 * @fn static void test_builder_preserves_map_on_each_owner_analysis_allocation_failure(void)
 *
 * ## Summary
 * 逐 ordinal 开启本 TU allocator 注入，调用 BuildStateMaps，随后先关闭注入再断言 outstanding=0；失败时 failed=TRUE、OUT_OF_MEMORY、token99、原 map 指针及 rootPool[0]=owner 保留；首次成功时 failed=FALSE，退出 ordinal 严格在1和32之间，末尾再核 owner checkpoint。
 *
 * ## Returns
 * 无 C 返回值；本case的场景断言通过则正常返回。TEST_ASSERT失败由Unity跳出当前case，runner再执行tearDown并继续后续case，失败计数由最终UNITY_END返回；不是终止整个测试进程。
 *
 * ## Constraints and lifecycle
 * 未断言精确成功/失败 ordinal 数；不覆盖 Clone、liveness、map 池等其他 TU 分配；未完整比较所有旧 map 元素。
 * 此case在共享setUp建立的固定function上构造/修改输入。图、值、map等归全局function；共享tearDown释放这些池，不负责释放未挂到function的本地MaterializedState。
 * 该case自身没有独立MaterializedState的最终释放语句；调用的共享断言helper可能临时物化并在正常路径Free，其失败longjmp边界仍适用。
 * 故障只注入被单独编译的ssa_owner_fault_allocator.c文本包含的owner分析TU。3输出池+5scratch分配在范围内，Clone/liveness/map/prepared等其他TU分配不在范围内；循环未断言精确失败次数。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_ownership.c:311
 * - tests/parser/test_ssa_state_map_ownership.c:317
 * - tests/parser/test_ssa_state_map_ownership.c:319
 * - tests/parser/test_ssa_state_map_ownership.c:322
 * - tests/parser/test_ssa_state_map_ownership.c:323
 * - tests/parser/test_ssa_state_map_ownership.c:324
 * - tests/parser/test_ssa_state_map_ownership.c:325
 * - tests/parser/test_ssa_state_map_ownership.c:326
 * - tests/parser/test_ssa_state_map_ownership.c:328
 * - tests/parser/test_ssa_state_map_ownership.c:331
 * - tests/parser/test_ssa_state_map_ownership.c:332
 * - tests/parser/test_ssa_state_map_ownership.c:333
 * - tests/parser/test_ssa_state_map_ownership.c:334
 * - tests/parser/test_ssa_state_map_ownership.c:335
 * - tests/parser/test_ssa_state_map_ownership.c:337
 * - tests/parser/test_ssa_state_map_ownership.c:338
 * - tests/parser/test_ssa_state_map_ownership.c:339
 * - tests/parser/ssa_state_map_fixture.h:135
 * - tests/parser/ssa_state_map_fixture.h:137
 * - tests/parser/ssa_state_map_fixture.h:150
 * - tests/parser/ssa_state_map_fixture.h:151
 * - tests/parser/ssa_state_map_fixture.h:153
 * - tests/parser/ssa_state_map_fixture.h:155
 * - tests/parser/test_ssa_state_map_ownership.c:393
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 */

/**
 * @fn static void test_materializer_preserves_target_on_each_owner_analysis_allocation_failure(void)
 *
 * ## Summary
 * 先成功物化并保存 values；逐 ordinal 注入后 resume，先关闭注入，再核 outstanding=0；失败时 failed=TRUE、OUT_OF_MEMORY、token99、原 values 指针与 roots[0]=owner 保留；首次成功 failed=FALSE，退出 ordinal 在1和32之间；正常路径 Free target。
 *
 * ## Returns
 * 无 C 返回值；本case的场景断言通过则正常返回。TEST_ASSERT失败由Unity跳出当前case，runner再执行tearDown并继续后续case，失败计数由最终UNITY_END返回；不是终止整个测试进程。
 *
 * ## Constraints and lifecycle
 * 不是所有 Materialize prepared 池 OOM 测试，其他 TU 分配未替换；只明确比较 values 指针和首 root，没有全五池/全记录事务快照。
 * 此case在共享setUp建立的固定function上构造/修改输入。图、值、map等归全局function；共享tearDown释放这些池，不负责释放未挂到function的本地MaterializedState。
 * 正常路径在tests/parser/test_ssa_state_map_ownership.c:375显式Free本地target；此前任一Unity断言longjmp都可能跳过该语句，不能承诺失败断言下回收所有局部资源。
 * 故障只注入被单独编译的ssa_owner_fault_allocator.c文本包含的owner分析TU。3输出池+5scratch分配在范围内，Clone/liveness/map/prepared等其他TU分配不在范围内；循环未断言精确失败次数。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_ownership.c:342
 * - tests/parser/test_ssa_state_map_ownership.c:352
 * - tests/parser/test_ssa_state_map_ownership.c:353
 * - tests/parser/test_ssa_state_map_ownership.c:354
 * - tests/parser/test_ssa_state_map_ownership.c:355
 * - tests/parser/test_ssa_state_map_ownership.c:358
 * - tests/parser/test_ssa_state_map_ownership.c:359
 * - tests/parser/test_ssa_state_map_ownership.c:360
 * - tests/parser/test_ssa_state_map_ownership.c:361
 * - tests/parser/test_ssa_state_map_ownership.c:362
 * - tests/parser/test_ssa_state_map_ownership.c:364
 * - tests/parser/test_ssa_state_map_ownership.c:367
 * - tests/parser/test_ssa_state_map_ownership.c:368
 * - tests/parser/test_ssa_state_map_ownership.c:369
 * - tests/parser/test_ssa_state_map_ownership.c:370
 * - tests/parser/test_ssa_state_map_ownership.c:371
 * - tests/parser/test_ssa_state_map_ownership.c:373
 * - tests/parser/test_ssa_state_map_ownership.c:374
 * - tests/parser/test_ssa_state_map_ownership.c:375
 * - tests/parser/test_ssa_state_map_ownership.c:394
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 */

/**
 * @fn int main(void)
 *
 * ## Summary
 * 初始化Unity并依次注册本文件定义的15个ownership case，最后返回UNITY_END的结果。
 *
 * ## Returns
 * @return 返回UNITY_END产生的测试失败计数；全case无失败时为0，失败不由各void case的C返回值传递。
 *
 * ## Constraints and lifecycle
 * RUN_TEST把case作为UnityTestFunction传给UnityDefaultTestRun；实际case入口为Func()，每次调用前后分别调共享setUp/tearDown。失败断言通常以longjmp退出当前case，后续teardown与case继续。
 * setUp/tearDown实际定义在ssa_state_map_fixture.h，不在本文件重复计键。该tearDown释放全局function，不清理case未发布的本地target，也不自动重置fault allocator状态。
 * 测试目标另编译ssa_owner_fault_allocator.c，其预处理alias只改写被包含owner分析C的分配，不改本testC、fixture、liveness、Clone或prepared数组的linked分配。两个OOM case在生产调用返回后、首个本地断言前显式禁用注入。
 * 本main仅组织这些源码断言，不意味着本轮预审运行过可执行程序，也不授予未注册case或整生产模块额外覆盖。
 *
 * ## References
 * - tests/parser/test_ssa_state_map_ownership.c:378
 * - tests/parser/test_ssa_state_map_ownership.c:379
 * - tests/parser/test_ssa_state_map_ownership.c:380
 * - tests/parser/test_ssa_state_map_ownership.c:394
 * - tests/parser/test_ssa_state_map_ownership.c:395
 * - tests/parser/ssa_state_map_fixture.h:16
 * - tests/parser/ssa_state_map_fixture.h:24
 * - tests/cmake/ssa-tests.cmake:244
 * - tests/cmake/ssa-tests.cmake:269
 * - tests/third_party/zr_unity/Unity/src/unity.c:2283
 * - tests/third_party/zr_unity/Unity/src/unity.c:2298
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 */
