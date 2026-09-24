#include "ssa_state_map_fixture.h"

static void loop(EZrExecIrOwnership ownership, TZrUInt16 flags) {
    TZrExecIrValueId condition = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_TRUE);
    TZrExecIrValueId reference = value(ownership, ZR_TRUE);
    TZrExecIrValueId copy = value(ownership, ZR_FALSE);
    blocks(3u);
    edges(1u, 2u, 0u);
    edges(2u, 2u, 3u);
    emit(1u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_COPY, 0u, reference, copy);
    emit(2u, ZR_EXEC_IR_OPCODE_NOP, flags, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 0u, condition, 0u);
    emit(3u, ZR_EXEC_IR_OPCODE_RETURN, 0u, condition, 0u);
}

static void test_backedge_keeps_reference_live_after_last_linear_use(void) {
    loop(ZR_EXEC_IR_OWNERSHIP_GC, ZR_EXEC_IR_FLAG_MAY_GC);
    build();
    assert_only_root(3u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, 2u);
    assert_only_root(3u, ZR_EXEC_IR_STATE_AFTER_EFFECT, 2u);
}

static void test_backedge_borrow_is_rejected_at_suspend(void) {
    SZrExecIrStateMap *published;
    loop(ZR_EXEC_IR_OWNERSHIP_BORROWED, ZR_EXEC_IR_FLAG_MAY_GC);
    build();
    published = function.stateMap;
    function.instructions[2].flags = ZR_EXEC_IR_FLAG_MAY_SUSPEND;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_BORROWED_ACROSS_SUSPEND, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(99u, diagnostic.functionToken);
    TEST_ASSERT_EQUAL_UINT32(3u, diagnostic.instructionId);
    TEST_ASSERT_EQUAL_UINT32(103u, diagnostic.sourceId);
    TEST_ASSERT_EQUAL_UINT32(2u, diagnostic.blockId);
    TEST_ASSERT_EQUAL_PTR(published, function.stateMap);
}

static void test_sibling_branch_value_does_not_cross_suspend(void) {
    TZrExecIrValueId condition = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_TRUE);
    TZrExecIrValueId borrowed = value(ZR_EXEC_IR_OWNERSHIP_BORROWED, ZR_TRUE);
    blocks(3u);
    edges(1u, 2u, 3u);
    emit(1u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 0u, condition, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_SUSPEND, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_RETURN, 0u, condition, 0u);
    emit(3u, ZR_EXEC_IR_OPCODE_RETURN, 0u, borrowed, 0u);
    build();
    TEST_ASSERT_EQUAL_UINT32(1u,
            checkpoint(2u, ZR_EXEC_IR_STATE_BEFORE_EFFECT)->liveValues.count);
    assert_only_root(2u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, 0u);
}

static void test_phi_inputs_are_live_only_on_their_incoming_edges(void) {
    TZrExecIrValueId condition = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_TRUE);
    TZrExecIrValueId left = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_TRUE);
    TZrExecIrValueId right = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_TRUE);
    TZrExecIrValueId merged = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_FALSE);
    SZrExecIrPhi phi = {0};
    SZrExecIrPhiIncoming incoming[2] = {{2u, left}, {3u, right}};
    blocks(4u);
    edges(1u, 2u, 3u);
    edges(2u, 4u, 0u);
    edges(3u, 4u, 0u);
    emit(1u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 0u, condition, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    emit(3u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(3u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    emit(4u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(4u, ZR_EXEC_IR_OPCODE_RETURN, 0u, merged, 0u);
    phi.result = merged;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendPhiIncoming(
            &function, incoming, 2u, &phi.incomings));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendPhis(
            &function, &phi, 1u, &function.blocks[3].phis));
    build();
    assert_only_root(2u, ZR_EXEC_IR_STATE_AFTER_EFFECT, left);
    assert_only_root(4u, ZR_EXEC_IR_STATE_AFTER_EFFECT, right);
    assert_only_root(6u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, merged);
}

static void test_definition_order_follows_cfg_instead_of_instruction_ids(void) {
    TZrExecIrValueId reference = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_FALSE);
    blocks(3u);
    edges(1u, 3u, 0u);
    edges(3u, 2u, 0u);
    emit(1u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_RETURN, 0u, reference, 0u);
    emit(3u, ZR_EXEC_IR_OPCODE_CONSTANT, 0u, 0u, reference);
    emit(3u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    build();
    assert_only_root(2u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, reference);
}

static void test_exception_successor_keeps_its_own_roots(void) {
    TZrExecIrValueId reference = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_TRUE);
    TZrExecIrValueId result = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_FALSE);
    blocks(3u);
    function.blocks[2].flags |= ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION;
    edges(1u, 2u, 3u);
    emit(1u, ZR_EXEC_IR_OPCODE_INVOKE,
         ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE, 0u, result);
    function.instructions[0].effectIn = 1u;
    function.instructions[0].effectOut = 2u;
    emit(2u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_RETURN, 0u, result, 0u);
    emit(3u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(3u, ZR_EXEC_IR_OPCODE_RETURN, 0u, reference, 0u);
    build();
    assert_only_root(1u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, reference);
    assert_only_root(2u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, result);
    assert_only_root(4u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, reference);
}

static void test_deopt_values_die_after_their_checkpoint(void) {
    TZrExecIrValueId reference = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_TRUE);
    TZrUInt32 index;
    for (index = 0u; index < 3u; ++index) {
        emit(0u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_DEBUG_POLL, 0u, 0u);
    }
    function.deoptStates = (SZrExecIrDeoptState *)calloc(1u, sizeof(*function.deoptStates));
    function.deoptValues = (TZrExecIrValueId *)malloc(sizeof(*function.deoptValues));
    TEST_ASSERT_NOT_NULL(function.deoptStates);
    TEST_ASSERT_NOT_NULL(function.deoptValues);
    function.deoptStateCount = function.deoptStateCapacity = 1u;
    function.deoptValueCount = function.deoptValueCapacity = 1u;
    function.deoptValues[0] = reference;
    function.instructions[1].deoptId = 91u;
    function.deoptStates[0].id = 91u;
    function.deoptStates[0].sourceId = 102u;
    function.deoptStates[0].resumeId = 701u;
    function.deoptStates[0].valueRange.count = 1u;
    build();
    assert_only_root(1u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, reference);
    assert_only_root(2u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, reference);
    assert_only_root(2u, ZR_EXEC_IR_STATE_AFTER_EFFECT, reference);
    assert_only_root(3u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, 0u);
}

static void test_reverse_layout_converges_and_preserves_high_value_id(void) {
    TZrUInt32 index;
    TZrExecIrValueId reference = 0u;
    for (index = 0u; index < 65u; ++index) {
        reference = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_TRUE);
    }
    blocks(64u);
    edges(1u, 64u, 0u);
    emit(1u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
    emit(1u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    emit(2u, ZR_EXEC_IR_OPCODE_RETURN, 0u, reference, 0u);
    for (index = 3u; index <= 64u; ++index) {
        edges(index, index - 1u, 0u);
        emit(index, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_MAY_GC, 0u, 0u);
        emit(index, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 0u);
    }
    build();
    assert_only_root(function.instructionCount - 1u,
                     ZR_EXEC_IR_STATE_AFTER_EFFECT, reference);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    assert_only_root(1u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, reference);
}

static void test_empty_function_has_empty_state_map(void) {
    build();
    TEST_ASSERT_NOT_NULL(function.stateMap);
    TEST_ASSERT_EQUAL_UINT32(0u, function.stateMap->entryCount);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_backedge_keeps_reference_live_after_last_linear_use);
    RUN_TEST(test_backedge_borrow_is_rejected_at_suspend);
    RUN_TEST(test_sibling_branch_value_does_not_cross_suspend);
    RUN_TEST(test_phi_inputs_are_live_only_on_their_incoming_edges);
    RUN_TEST(test_definition_order_follows_cfg_instead_of_instruction_ids);
    RUN_TEST(test_exception_successor_keeps_its_own_roots);
    RUN_TEST(test_deopt_values_die_after_their_checkpoint);
    RUN_TEST(test_reverse_layout_converges_and_preserves_high_value_id);
    RUN_TEST(test_empty_function_has_empty_state_map);
    return UNITY_END();
}
