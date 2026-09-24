#include "unity.h"

#include <stdlib.h>

#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_parser/exec_ir_state_maps.h"

static SZrExecIrFunction function;
static SZrExecIrDiagnostic diagnostic;

void setUp(void) {
    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = 99u;
    function.signatureHash = 123u;
    function.contract.generation = 7u;
}

void tearDown(void) {
    ZrCore_ExecIr_FreeFunction(&function);
}

static TZrExecIrValueId value(EZrExecIrOwnership ownership, TZrBool external) {
    TZrExecIrValueId id = external
            ? ZrCore_ExecIr_FunctionAddExternalValue(
                    &function, 1u, ownership, ZR_EXEC_IR_NULLABILITY_NULLABLE)
            : ZrCore_ExecIr_FunctionAddValue(
                    &function, 1u, ownership, ZR_EXEC_IR_NULLABILITY_NULLABLE);
    TEST_ASSERT_NOT_EQUAL(0u, id);
    return id;
}

static void blocks(TZrUInt32 count) {
    TZrUInt32 index;
    for (index = 0u; index < count; ++index) {
        TEST_ASSERT_EQUAL_UINT32(index + 1u, ZrCore_ExecIr_FunctionAddBlock(
                &function, index == 0u ? ZR_EXEC_IR_BLOCK_FLAG_ENTRY : 0u));
    }
}

static void edges(TZrExecIrBlockId block, TZrExecIrBlockId first,
                  TZrExecIrBlockId second) {
    TZrExecIrBlockId targets[2] = {first, second};
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendSuccessors(
            &function, targets, second == 0u ? 1u : 2u,
            &function.blocks[block - 1u].successorRange));
}

static void predecessors(void) {
    TZrUInt32 target, source, edge;
    for (target = 0u; target < function.blockCount; ++target) {
        SZrExecIrRange range = {0};
        range.start = function.predecessorCount;
        for (source = 0u; source < function.blockCount; ++source) {
            const SZrExecIrBlock *block = &function.blocks[source];
            for (edge = block->successorRange.start;
                 edge < block->successorRange.start + block->successorRange.count;
                 ++edge) {
                if (function.successors[edge] == target + 1u) {
                    TZrExecIrBlockId id = source + 1u;
                    SZrExecIrRange appended;
                    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendPredecessors(
                            &function, &id, 1u, &appended));
                    ++range.count;
                }
            }
        }
        function.blocks[target].predecessorRange = range;
    }
}

static TZrExecIrInstructionId emit(TZrExecIrBlockId blockId, EZrExecIrOpcode opcode,
                                   TZrUInt16 flags, TZrExecIrValueId operand,
                                   TZrExecIrValueId result) {
    SZrExecIrInstruction instruction = {0};
    TZrExecIrInstructionId id;
    instruction.opcode = opcode;
    instruction.flags = flags;
    instruction.sourceId = 100u + function.instructionCount + 1u;
    if (operand != 0u) {
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendOperands(
                &function, &operand, 1u, &instruction.operandRange));
    }
    if (result != 0u) {
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendResults(
                &function, &result, 1u, &instruction.resultRange));
    }
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(
            &function, &instruction, &id));
    if (blockId != 0u) {
        SZrExecIrBlock *block = &function.blocks[blockId - 1u];
        if (block->instructionRange.count == 0u) {
            block->instructionRange.start = id - 1u;
        }
        ++block->instructionRange.count;
    }
    return id;
}

static const SZrExecIrStateMapEntry *checkpoint(TZrExecIrInstructionId instruction,
                                                EZrExecIrStateMapPhase phase) {
    TZrUInt32 index;
    const SZrExecIrStateMapEntry *found = NULL;
    TEST_ASSERT_NOT_NULL(function.stateMap);
    for (index = 0u; index < function.stateMap->entryCount; ++index) {
        const SZrExecIrStateMapEntry *entry = &function.stateMap->entries[index];
        if (entry->instructionId == instruction && entry->phase == phase) {
            found = entry;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(found, "missing checkpoint");
    return found;
}

static void assert_only_root(TZrExecIrInstructionId instruction,
                             EZrExecIrStateMapPhase phase, TZrExecIrValueId root) {
    const SZrExecIrStateMapEntry *entry = checkpoint(instruction, phase);
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request = {0};
    TEST_ASSERT_EQUAL_UINT32(root == 0u ? 0u : 1u, entry->rootValues.count);
    if (root != 0u) {
        TEST_ASSERT_EQUAL_UINT32(root,
                function.stateMap->rootPool[entry->rootValues.start]);
    }
    ZrCore_ExecIr_MaterializedStateInit(&target);
    request.function = &function;
    request.map = function.stateMap;
    request.functionToken = function.functionToken;
    request.generation = function.contract.generation;
    request.signatureHash = function.signatureHash;
    request.sourceId = entry->sourceId;
    request.resumeId = entry->resumeId;
    request.phase = phase;
    request.target = &target;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(entry->rootValues.count, target.rootCount);
    if (root != 0u) {
        TEST_ASSERT_EQUAL_UINT32(root, target.roots[0]);
    }
    ZrCore_ExecIr_MaterializedStateFree(&target);
}

static void build(void) {
    predecessors();
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
}

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
