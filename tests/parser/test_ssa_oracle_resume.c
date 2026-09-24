#include "ssa_state_map_fixture.h"
#include "ssa_oracle_resume_fault_allocator.h"
#include "zr_vm_core/exec_ir_interpreter.h"

#include <string.h>

typedef struct SResumeEffects {
    TZrUInt32 calls;
    TZrUInt32 stores;
    TZrBool rejectCall;
    TZrBool throwInvoke;
    TZrInt64 stored;
} SResumeEffects;

static SZrExecIrOracleValue scalar(TZrInt64 number) {
    SZrExecIrOracleValue result = {0};
    result.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    result.as.signedInteger = number;
    return result;
}

static TZrBool getter(void *userData,
                      const SZrExecIrInstruction *instruction,
                      const SZrExecIrOracleValue *operands,
                      TZrUInt32 operandCount,
                      SZrExecIrOracleValue *result) {
    SResumeEffects *effects = (SResumeEffects *)userData;
    if (instruction->opcode != ZR_EXEC_IR_OPCODE_CALL || operandCount != 1u ||
        operands[0].kind != ZR_EXEC_IR_ORACLE_VALUE_SIGNED ||
        operands[0].as.signedInteger != 7 || result == NULL) {
        return ZR_FALSE;
    }
    ++effects->calls;
    if (effects->rejectCall) return ZR_FALSE;
    *result = scalar(42);
    return ZR_TRUE;
}

static TZrBool memory(void *userData,
                      const SZrExecIrInstruction *instruction,
                      EZrExecIrOracleMemoryOperation operation,
                      const SZrExecIrOracleValue *operands,
                      TZrUInt32 operandCount,
                      SZrExecIrOracleValue *result) {
    SResumeEffects *effects = (SResumeEffects *)userData;
    if (instruction->opcode != ZR_EXEC_IR_OPCODE_STORE ||
        operation != ZR_EXEC_IR_ORACLE_MEMORY_STORE || operandCount != 2u ||
        result != NULL || operands[0].kind != ZR_EXEC_IR_ORACLE_VALUE_SIGNED ||
        operands[0].as.signedInteger != 7 ||
        operands[1].kind != ZR_EXEC_IR_ORACLE_VALUE_SIGNED) {
        return ZR_FALSE;
    }
    ++effects->stores;
    effects->stored = operands[1].as.signedInteger;
    return ZR_TRUE;
}

static void effect_metadata(TZrExecIrInstructionId id, TZrUInt32 token,
                            TZrBool reads, TZrBool writes) {
    SZrExecIrInstruction *instruction = &function.instructions[id - 1u];
    TZrExecIrMemoryTokenId next = token + 1u;
    instruction->effectIn = token;
    instruction->effectOut = next;
    if (reads) {
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendMemoryTokens(
                &function, &token, 1u, &instruction->memoryIn));
    }
    if (writes) {
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendMemoryTokens(
                &function, &next, 1u, &instruction->memoryOut));
    }
}

static TZrExecIrInstructionId terminate(TZrExecIrBlockId blockId,
                                        EZrExecIrOpcode opcode,
                                        TZrUInt16 flags,
                                        TZrExecIrValueId operand) {
    TZrExecIrInstructionId id = emit(blockId, opcode, flags, operand, 0u);
    function.instructions[id - 1u].successorRange =
            function.blocks[blockId - 1u].successorRange;
    function.blocks[blockId - 1u].terminatorInstructionId = id;
    return id;
}

static SZrExecIrOracleCheckpoint stop_at(TZrExecIrInstructionId id,
                                        EZrExecIrStateMapPhase phase) {
    const SZrExecIrStateMapEntry *entry = checkpoint(id, phase);
    SZrExecIrOracleCheckpoint stop;
    stop.sourceId = entry->sourceId;
    stop.resumeId = entry->resumeId;
    stop.phase = entry->phase;
    return stop;
}

static void assert_value(const SZrExecIrOracleValue *expected,
                          const SZrExecIrOracleValue *actual) {
    TEST_ASSERT_EQUAL(expected->kind, actual->kind);
    switch (expected->kind) {
        case ZR_EXEC_IR_ORACLE_VALUE_SIGNED:
            TEST_ASSERT_EQUAL_INT64(expected->as.signedInteger,
                                    actual->as.signedInteger);
            break;
        case ZR_EXEC_IR_ORACLE_VALUE_BOOL:
            TEST_ASSERT_EQUAL(expected->as.boolean, actual->as.boolean);
            break;
        default:
            TEST_FAIL_MESSAGE("unexpected fixture value kind");
    }
}

static void assert_execution(const SZrExecIrOracleExecutionResult *expected,
                              const SZrExecIrOracleExecutionResult *actual) {
    TZrUInt32 eventIndex, operand;
    TEST_ASSERT_TRUE(actual->returned);
    TEST_ASSERT_FALSE(actual->paused);
    TEST_ASSERT_EQUAL(expected->terminatedByThrow, actual->terminatedByThrow);
    TEST_ASSERT_EQUAL(expected->suspended, actual->suspended);
    assert_value(&expected->returnValue, &actual->returnValue);
    TEST_ASSERT_EQUAL_UINT32(expected->executedInstructionCount,
                             actual->executedInstructionCount);
    TEST_ASSERT_EQUAL_UINT32(expected->eventCount, actual->eventCount);
    for (eventIndex = 0u; eventIndex < expected->eventCount; ++eventIndex) {
        const SZrExecIrOracleEvent *left = &expected->events[eventIndex];
        const SZrExecIrOracleEvent *right = &actual->events[eventIndex];
        TEST_ASSERT_EQUAL(left->kind, right->kind);
        TEST_ASSERT_EQUAL_UINT32(left->instructionId, right->instructionId);
        TEST_ASSERT_EQUAL_UINT32(left->sourceId, right->sourceId);
        TEST_ASSERT_EQUAL_UINT32(left->operandCount, right->operandCount);
        for (operand = 0u; operand < left->operandCount; ++operand) {
            assert_value(&left->operands[operand], &right->operands[operand]);
        }
    }
}

static void build_getter(void) {
    TZrExecIrValueId receiver = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_TRUE);
    TZrExecIrValueId result = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_FALSE);
    blocks(1u);
    effect_metadata(emit(1u, ZR_EXEC_IR_OPCODE_CALL,
            ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE,
            receiver, result), 1u, ZR_TRUE, ZR_TRUE);
    terminate(1u, ZR_EXEC_IR_OPCODE_RETURN, 0u, result);
    build();
    assert_only_root(1u, ZR_EXEC_IR_STATE_BEFORE_EFFECT, receiver);
}

static SZrExecIrOracleInput oracle_input(SZrExecIrOracleValue *initial,
                                          TZrUInt32 count,
                                          SResumeEffects *effects) {
    SZrExecIrOracleInput input = {0};
    input.function = &function;
    input.initialValues = initial;
    input.initialValueCount = count;
    input.call = getter;
    input.userData = effects;
    input.memory = memory;
    input.memoryUserData = effects;
    return input;
}

static void compare_getter_resume(EZrExecIrStateMapPhase phase) {
    SZrExecIrOracleExecutionResult uninterrupted, resumed;
    SZrExecIrOracleValue initial[1] = {scalar(7)};
    SResumeEffects referenceEffects = {0}, resumedEffects = {0};
    SZrExecIrOracleInput input;
    SZrExecIrOracleCheckpoint stop;
    SZrExecIrOracleValue *oldValues;
    SZrExecIrOracleEvent *oldEvents;
    build_getter();
    input = oracle_input(initial, 1u, &referenceEffects);
    ZrCore_ExecIr_OracleResultInit(&uninterrupted);
    ZrCore_ExecIr_OracleResultInit(&resumed);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &uninterrupted, &diagnostic));
    stop = stop_at(1u, phase);
    input.stopAt = &stop;
    input.userData = &resumedEffects;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_TRUE(resumed.paused);
    TEST_ASSERT_FALSE(resumed.returned);
    TEST_ASSERT_EQUAL_UINT32(phase == ZR_EXEC_IR_STATE_BEFORE_EFFECT ? 0u : 1u,
                             resumedEffects.calls);
    if (phase == ZR_EXEC_IR_STATE_AFTER_EFFECT) {
        /* Dead receiver state must not be restored from either source. */
        resumed.values[0] = scalar(999);
        initial[0] = scalar(999);
    }
    /* Keeping the before selector must skip this occurrence exactly once. */
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(1u, resumedEffects.calls);
    assert_execution(&uninterrupted, &resumed);
    if (phase == ZR_EXEC_IR_STATE_AFTER_EFFECT) {
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED, resumed.values[0].kind);
    }
    oldValues = resumed.values;
    oldEvents = resumed.events;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(1u, resumedEffects.calls);
    TEST_ASSERT_EQUAL_PTR(oldValues, resumed.values);
    TEST_ASSERT_EQUAL_PTR(oldEvents, resumed.events);
    assert_execution(&uninterrupted, &resumed);
    ZrCore_ExecIr_OracleResultFree(&resumed);
    ZrCore_ExecIr_OracleResultFree(&uninterrupted);
}

static void test_before_getter_resumes_once_with_same_selector(void) {
    compare_getter_resume(ZR_EXEC_IR_STATE_BEFORE_EFFECT);
}

static void test_after_getter_restores_only_live_result_without_replay(void) {
    compare_getter_resume(ZR_EXEC_IR_STATE_AFTER_EFFECT);
}

static void test_store_and_cleanup_resume_preserve_effect_trace(void) {
    TZrExecIrValueId receiver = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_TRUE);
    TZrExecIrValueId owner = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_TRUE);
    TZrExecIrValueId result = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_FALSE);
    TZrExecIrValueId storeOperands[2] = {receiver, result};
    SZrExecIrOracleValue initial[2] = {scalar(7), scalar(91)};
    SZrExecIrOracleExecutionResult uninterrupted, resumed;
    SZrExecIrOracleCheckpoint storeStop, cleanupStop;
    SResumeEffects referenceEffects = {0}, resumedEffects = {0};
    SZrExecIrOracleInput input;
    blocks(1u);
    effect_metadata(emit(1u, ZR_EXEC_IR_OPCODE_CALL,
            ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE,
            receiver, result), 1u, ZR_TRUE, ZR_TRUE);
    effect_metadata(emit(1u, ZR_EXEC_IR_OPCODE_STORE,
            ZR_EXEC_IR_FLAG_MAY_THROW, 0u, 0u), 2u, ZR_FALSE, ZR_TRUE);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendOperands(&function,
            storeOperands, 2u, &function.instructions[1].operandRange));
    effect_metadata(emit(1u, ZR_EXEC_IR_OPCODE_DROP, 0u, owner, 0u),
                    3u, ZR_TRUE, ZR_TRUE);
    terminate(1u, ZR_EXEC_IR_OPCODE_RETURN, 0u, result);
    build();
    input = oracle_input(initial, 2u, &referenceEffects);
    ZrCore_ExecIr_OracleResultInit(&uninterrupted);
    ZrCore_ExecIr_OracleResultInit(&resumed);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &uninterrupted, &diagnostic));
    storeStop = stop_at(2u, ZR_EXEC_IR_STATE_AFTER_EFFECT);
    cleanupStop = stop_at(3u, ZR_EXEC_IR_STATE_CLEANUP_COMPLETE);
    input.stopAt = &storeStop;
    input.userData = input.memoryUserData = &resumedEffects;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_TRUE(resumed.paused);
    TEST_ASSERT_EQUAL_UINT32(1u, resumedEffects.calls);
    TEST_ASSERT_EQUAL_UINT32(1u, resumedEffects.stores);
    TEST_ASSERT_EQUAL_UINT32(2u, resumed.eventCount);
    input.stopAt = &cleanupStop;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_TRUE(resumed.paused);
    TEST_ASSERT_EQUAL_UINT32(3u, resumed.eventCount);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_ORACLE_EVENT_DROP, resumed.events[2].kind);
    input.stopAt = NULL;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(referenceEffects.calls, resumedEffects.calls);
    TEST_ASSERT_EQUAL_UINT32(referenceEffects.stores, resumedEffects.stores);
    TEST_ASSERT_EQUAL_INT64(referenceEffects.stored, resumedEffects.stored);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED,
                      resumed.values[owner - 1u].kind);
    assert_execution(&uninterrupted, &resumed);
    ZrCore_ExecIr_OracleResultFree(&resumed);
    ZrCore_ExecIr_OracleResultFree(&uninterrupted);
}

static void compare_rejected_preparation(TZrBool changeGeneration) {
    SZrExecIrOracleExecutionResult resumed, saved;
    SZrExecIrOracleValue initial[1] = {scalar(7)};
    SResumeEffects effects = {0};
    SZrExecIrOracleInput input;
    SZrExecIrOracleCheckpoint stop;
    build_getter();
    stop = stop_at(1u, ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    input = oracle_input(initial, 1u, &effects);
    input.stopAt = &stop;
    ZrCore_ExecIr_OracleResultInit(&resumed);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_TRUE(resumed.paused);
    if (changeGeneration) ++function.contract.generation;
    else resumed.values[0].kind = ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED;
    saved = resumed;
    input.stopAt = NULL;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_EQUAL(changeGeneration ? ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION
                                       : ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                      diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(stop.sourceId, diagnostic.sourceId);
    TEST_ASSERT_EQUAL_UINT32(1u, diagnostic.instructionId);
    TEST_ASSERT_EQUAL_PTR(saved.values, resumed.values);
    TEST_ASSERT_EQUAL_PTR(saved.events, resumed.events);
    TEST_ASSERT_EQUAL_UINT32(saved.eventCount, resumed.eventCount);
    TEST_ASSERT_EQUAL_UINT32(saved.executedInstructionCount, resumed.executedInstructionCount);
    TEST_ASSERT_TRUE(resumed.paused);
    TEST_ASSERT_EQUAL_UINT32(0u, effects.calls);
    if (changeGeneration) --function.contract.generation;
    else resumed.values[0] = scalar(7);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_TRUE(resumed.returned);
    TEST_ASSERT_EQUAL_INT64(42, resumed.returnValue.as.signedInteger);
    TEST_ASSERT_EQUAL_UINT32(1u, effects.calls);
    ZrCore_ExecIr_OracleResultFree(&resumed);
}

static void test_stale_generation_rejects_without_consuming_checkpoint(void) {
    compare_rejected_preparation(ZR_TRUE);
}

static void test_missing_live_value_rejects_without_consuming_checkpoint(void) {
    compare_rejected_preparation(ZR_FALSE);
}

static void test_failed_callback_consumes_checkpoint_before_effect_can_replay(void) {
    SZrExecIrOracleExecutionResult resumed;
    SZrExecIrOracleValue initial[1] = {scalar(7)};
    SResumeEffects effects = {0};
    SZrExecIrOracleInput input;
    SZrExecIrOracleCheckpoint stop;
    build_getter();
    stop = stop_at(1u, ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    input = oracle_input(initial, 1u, &effects);
    input.stopAt = &stop;
    ZrCore_ExecIr_OracleResultInit(&resumed);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_TRUE(resumed.paused);
    effects.rejectCall = ZR_TRUE;
    input.stopAt = NULL;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(1u, effects.calls);
    TEST_ASSERT_FALSE(resumed.paused);
    TEST_ASSERT_FALSE(resumed.returned);
    effects.rejectCall = ZR_FALSE;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(1u, effects.calls);
    ZrCore_ExecIr_OracleResultFree(&resumed);
}

static void build_parallel_phi(void) {
    TZrExecIrValueId left = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_TRUE);
    TZrExecIrValueId right = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_TRUE);
    TZrExecIrValueId condition = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_TRUE);
    TZrExecIrValueId receiver = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_TRUE);
    TZrExecIrValueId merged = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_FALSE);
    TZrExecIrValueId called = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_FALSE);
    SZrExecIrPhi phi = {0};
    SZrExecIrPhiIncoming incoming[2] = {{1u, left}, {1u, right}};
    blocks(2u);
    edges(1u, 2u, 2u);
    terminate(1u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
               ZR_EXEC_IR_FLAG_DEBUG_POLL, condition);
    emit(2u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_DEBUG_POLL, 0u, 0u);
    effect_metadata(emit(2u, ZR_EXEC_IR_OPCODE_CALL,
            ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE,
            receiver, called), 1u, ZR_TRUE, ZR_TRUE);
    terminate(2u, ZR_EXEC_IR_OPCODE_RETURN, 0u, merged);
    phi.result = merged;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendPhiIncoming(
            &function, incoming, 2u, &phi.incomings));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendPhis(
            &function, &phi, 1u, &function.blocks[1].phis));
    build();
}

static void compare_phi_resume(TZrBool afterBranch) {
    SZrExecIrOracleExecutionResult uninterrupted, resumed;
    SZrExecIrOracleValue initial[4] = {
        scalar(11), scalar(22), scalar(0), scalar(7)
    };
    SResumeEffects referenceEffects = {0}, resumedEffects = {0};
    SZrExecIrOracleInput input;
    SZrExecIrOracleCheckpoint stop;
    TZrUInt32 index;
    build_parallel_phi();
    input = oracle_input(initial, 4u, &referenceEffects);
    ZrCore_ExecIr_OracleResultInit(&uninterrupted);
    ZrCore_ExecIr_OracleResultInit(&resumed);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &uninterrupted, &diagnostic));
    TEST_ASSERT_EQUAL_INT64(22, uninterrupted.returnValue.as.signedInteger);
    stop = stop_at(afterBranch ? 1u : 2u,
                   afterBranch ? ZR_EXEC_IR_STATE_AFTER_EFFECT
                               : ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    input.stopAt = &stop;
    input.userData = &resumedEffects;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_TRUE(resumed.paused);
    TEST_ASSERT_EQUAL_UINT32(0u, resumedEffects.calls);
    if (afterBranch) {
        /* The false edge is already chosen; this dead condition is not
         * allowed to select the first occurrence of the parallel edge. */
        resumed.values[2] = scalar(1);
    } else {
        /* Only the merged value is live once the block's phi was applied.
         * Re-entering the block would read these deliberately dead inputs. */
        TEST_ASSERT_EQUAL_INT64(22, resumed.values[4].as.signedInteger);
        resumed.values[0].kind = ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED;
        resumed.values[1].kind = ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED;
    }
    for (index = 0u; index < 4u; ++index) initial[index] = scalar(-1);
    input.stopAt = NULL;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(1u, resumedEffects.calls);
    assert_execution(&uninterrupted, &resumed);
    ZrCore_ExecIr_OracleResultFree(&resumed);
    ZrCore_ExecIr_OracleResultFree(&uninterrupted);
}

static void test_after_branch_preserves_selected_parallel_edge(void) {
    compare_phi_resume(ZR_TRUE);
}

static void test_in_block_resume_uses_materialized_phi_without_reentering_block(void) {
    compare_phi_resume(ZR_FALSE);
}

static void test_invalid_next_checkpoint_preserves_paused_frame(void) {
    SZrExecIrOracleExecutionResult resumed;
    SZrExecIrOracleValue initial[1] = {scalar(7)};
    SResumeEffects effects = {0};
    SZrExecIrOracleInput input;
    SZrExecIrOracleCheckpoint stop, invalid;
    SZrExecIrOracleValue *oldValues;
    build_getter();
    stop = stop_at(1u, ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    input = oracle_input(initial, 1u, &effects);
    input.stopAt = &stop;
    ZrCore_ExecIr_OracleResultInit(&resumed);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_TRUE(resumed.paused);
    oldValues = resumed.values;
    invalid = stop;
    invalid.resumeId = UINT32_MAX;
    input.stopAt = &invalid;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_RESUME_NOT_FOUND, diagnostic.code);
    TEST_ASSERT_EQUAL_PTR(oldValues, resumed.values);
    TEST_ASSERT_TRUE(resumed.paused);
    TEST_ASSERT_EQUAL_UINT32(0u, effects.calls);
    input.stopAt = NULL;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_TRUE(resumed.returned);
    TEST_ASSERT_EQUAL_UINT32(1u, effects.calls);
    ZrCore_ExecIr_OracleResultFree(&resumed);
}

static void test_suspend_after_phase_enters_successor_without_repeating_suspend(void) {
    TZrExecIrValueId receiver = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_TRUE);
    TZrExecIrValueId suspended = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_FALSE);
    TZrExecIrValueId called = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_FALSE);
    SZrExecIrOracleValue initial[1] = {scalar(7)};
    SZrExecIrOracleExecutionResult baseline, resumed;
    SResumeEffects effects = {0};
    SZrExecIrOracleInput input;
    SZrExecIrOracleCheckpoint stop;
    blocks(2u);
    edges(1u, 2u, 0u);
    effect_metadata(emit(1u, ZR_EXEC_IR_OPCODE_SUSPEND,
            ZR_EXEC_IR_FLAG_MAY_SUSPEND, receiver, suspended),
            1u, ZR_TRUE, ZR_TRUE);
    function.instructions[0].successorRange = function.blocks[0].successorRange;
    function.blocks[0].terminatorInstructionId = 1u;
    effect_metadata(emit(2u, ZR_EXEC_IR_OPCODE_CALL,
            ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE,
            receiver, called), 2u, ZR_TRUE, ZR_TRUE);
    terminate(2u, ZR_EXEC_IR_OPCODE_RETURN, 0u, suspended);
    build();
    input = oracle_input(initial, 1u, &effects);
    ZrCore_ExecIr_OracleResultInit(&baseline);
    ZrCore_ExecIr_OracleResultInit(&resumed);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &baseline, &diagnostic));
    TEST_ASSERT_TRUE(baseline.suspended);
    TEST_ASSERT_FALSE(baseline.returned);
    TEST_ASSERT_EQUAL_UINT32(1u, baseline.eventCount);
    stop = stop_at(1u, ZR_EXEC_IR_STATE_AFTER_EFFECT);
    input.stopAt = &stop;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_TRUE(resumed.paused);
    TEST_ASSERT_TRUE(resumed.suspended);
    TEST_ASSERT_EQUAL_UINT32(0u, effects.calls);
    TEST_ASSERT_EQUAL_UINT32(1u, resumed.eventCount);
    assert_value(&baseline.returnValue, &resumed.returnValue);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_TRUE(resumed.returned);
    TEST_ASSERT_FALSE(resumed.paused);
    TEST_ASSERT_FALSE(resumed.suspended);
    TEST_ASSERT_EQUAL_INT64(7, resumed.returnValue.as.signedInteger);
    TEST_ASSERT_EQUAL_UINT32(1u, effects.calls);
    TEST_ASSERT_EQUAL_UINT32(3u, resumed.executedInstructionCount);
    TEST_ASSERT_EQUAL_UINT32(2u, resumed.eventCount);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_ORACLE_EVENT_SUSPEND, resumed.events[0].kind);
    TEST_ASSERT_EQUAL_UINT32(1u, resumed.events[0].instructionId);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_ORACLE_EVENT_CALL, resumed.events[1].kind);
    TEST_ASSERT_EQUAL_UINT32(2u, resumed.events[1].instructionId);
    TEST_ASSERT_FALSE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(1u, effects.calls);
    TEST_ASSERT_EQUAL_UINT32(2u, resumed.eventCount);
    ZrCore_ExecIr_OracleResultFree(&resumed);
    ZrCore_ExecIr_OracleResultFree(&baseline);
}

enum { RESUME_LOOP_ITERATIONS = 70 };

static TZrBool loop_condition(void *userData,
                              const SZrExecIrInstruction *instruction,
                              const SZrExecIrOracleValue *operands,
                              TZrUInt32 operandCount,
                              SZrExecIrOracleValue *result) {
    SResumeEffects *effects = (SResumeEffects *)userData;
    if (!getter(userData, instruction, operands, operandCount, result)) return ZR_FALSE;
    result->kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
    result->as.boolean = (TZrBool)(effects->calls < RESUME_LOOP_ITERATIONS);
    return ZR_TRUE;
}

static void test_repeated_loop_checkpoint_advances_and_accumulates_effects(void) {
    TZrExecIrValueId receiver = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_TRUE);
    TZrExecIrValueId condition = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_FALSE);
    SZrExecIrOracleValue initial[1] = {scalar(7)};
    SZrExecIrOracleExecutionResult uninterrupted, resumed;
    SResumeEffects referenceEffects = {0}, resumedEffects = {0};
    SZrExecIrOracleInput input;
    SZrExecIrOracleCheckpoint stop;
    TZrUInt32 iteration;
    blocks(3u);
    edges(1u, 2u, 0u);
    edges(2u, 2u, 3u);
    terminate(1u, ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u);
    effect_metadata(emit(2u, ZR_EXEC_IR_OPCODE_CALL,
            ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE,
            receiver, condition), 1u, ZR_TRUE, ZR_TRUE);
    terminate(2u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 0u, condition);
    terminate(3u, ZR_EXEC_IR_OPCODE_RETURN, 0u, receiver);
    build();
    input = oracle_input(initial, 1u, &referenceEffects);
    input.call = loop_condition;
    ZrCore_ExecIr_OracleResultInit(&uninterrupted);
    ZrCore_ExecIr_OracleResultInit(&resumed);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &uninterrupted, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(RESUME_LOOP_ITERATIONS, referenceEffects.calls);
    stop = stop_at(2u, ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    input.stopAt = &stop;
    input.userData = &resumedEffects;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_TRUE(resumed.paused);
    TEST_ASSERT_EQUAL_UINT32(0u, resumedEffects.calls);
    for (iteration = 1u; iteration <= RESUME_LOOP_ITERATIONS; ++iteration) {
        TZrUInt32 previousInstructions = resumed.executedInstructionCount;
        TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
        TEST_ASSERT_EQUAL_UINT32(iteration, resumedEffects.calls);
        TEST_ASSERT_EQUAL_UINT32(iteration, resumed.eventCount);
        TEST_ASSERT_TRUE(resumed.executedInstructionCount > previousInstructions);
        TEST_ASSERT_EQUAL((TZrBool)(iteration < RESUME_LOOP_ITERATIONS), resumed.paused);
    }
    assert_execution(&uninterrupted, &resumed);
    TEST_ASSERT_EQUAL_UINT32(2u * RESUME_LOOP_ITERATIONS + 2u,
                             resumed.executedInstructionCount);
    ZrCore_ExecIr_OracleResultFree(&resumed);
    ZrCore_ExecIr_OracleResultFree(&uninterrupted);
}

static void test_empty_live_checkpoint_resumes_future_definition(void) {
    TZrExecIrValueId result = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_FALSE);
    SZrExecIrOracleExecutionResult uninterrupted, resumed;
    SResumeEffects effects = {0};
    SZrExecIrOracleInput input;
    SZrExecIrOracleCheckpoint stop;
    blocks(1u);
    emit(1u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_DEBUG_POLL, 0u, 0u);
    emit(1u, ZR_EXEC_IR_OPCODE_CONSTANT, 0u, 0u, result);
    function.instructions[1].layoutId = 42u;
    terminate(1u, ZR_EXEC_IR_OPCODE_RETURN, 0u, result);
    build();
    TEST_ASSERT_EQUAL_UINT32(0u,
            checkpoint(1u, ZR_EXEC_IR_STATE_BEFORE_EFFECT)->liveValues.count);
    input = oracle_input(NULL, 0u, &effects);
    ZrCore_ExecIr_OracleResultInit(&uninterrupted);
    ZrCore_ExecIr_OracleResultInit(&resumed);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &uninterrupted, &diagnostic));
    stop = stop_at(1u, ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    input.stopAt = &stop;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_TRUE(resumed.paused);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED, resumed.values[0].kind);
    input.stopAt = NULL;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    assert_execution(&uninterrupted, &resumed);
    TEST_ASSERT_EQUAL_INT64(42, resumed.returnValue.as.signedInteger);
    TEST_ASSERT_EQUAL_UINT32(0u, effects.calls);
    ZrCore_ExecIr_OracleResultFree(&resumed);
    ZrCore_ExecIr_OracleResultFree(&uninterrupted);
}

static void test_malformed_successor_ordinal_preserves_paused_frame(void) {
    SZrExecIrOracleExecutionResult resumed, saved;
    SZrExecIrOracleValue initial[4] = {
        scalar(11), scalar(22), scalar(0), scalar(7)
    };
    SResumeEffects effects = {0};
    SZrExecIrOracleInput input;
    SZrExecIrOracleCheckpoint stop;
    TZrUInt32 originalOrdinal;
    build_parallel_phi();
    input = oracle_input(initial, 4u, &effects);
    stop = stop_at(1u, ZR_EXEC_IR_STATE_AFTER_EFFECT);
    input.stopAt = &stop;
    ZrCore_ExecIr_OracleResultInit(&resumed);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_TRUE(resumed.paused);
    originalOrdinal = resumed.continuation.successorOrdinal;
    resumed.continuation.successorOrdinal = UINT32_MAX;
    memcpy(&saved, &resumed, sizeof(saved));
    input.stopAt = NULL;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(1u, diagnostic.instructionId);
    TEST_ASSERT_EQUAL_UINT32(stop.sourceId, diagnostic.sourceId);
    TEST_ASSERT_EQUAL_MEMORY(&saved, &resumed, sizeof(saved));
    TEST_ASSERT_EQUAL_UINT32(0u, effects.calls);
    resumed.continuation.successorOrdinal = originalOrdinal;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_TRUE(resumed.returned);
    TEST_ASSERT_EQUAL_INT64(22, resumed.returnValue.as.signedInteger);
    TEST_ASSERT_EQUAL_UINT32(1u, effects.calls);
    ZrCore_ExecIr_OracleResultFree(&resumed);
}

static void test_each_resume_preparation_allocation_preserves_checkpoint_on_failure(void) {
    SZrExecIrOracleExecutionResult uninterrupted, resumed, saved;
    SZrExecIrOracleValue initial[1] = {scalar(7)};
    SResumeEffects referenceEffects = {0}, resumedEffects = {0};
    SZrExecIrOracleInput input;
    SZrExecIrOracleCheckpoint stop;
    size_t ordinal;
    TZrUInt32 failures = 0u;
    TZrBool completed = ZR_FALSE;
    build_getter();
    input = oracle_input(initial, 1u, &referenceEffects);
    ZrCore_ExecIr_OracleResultInit(&uninterrupted);
    ZrCore_ExecIr_OracleResultInit(&resumed);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &uninterrupted, &diagnostic));
    stop = stop_at(1u, ZR_EXEC_IR_STATE_AFTER_EFFECT);
    input.stopAt = &stop;
    input.userData = &resumedEffects;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_TRUE(resumed.paused);
    TEST_ASSERT_EQUAL_UINT32(1u, resumed.eventCount);
    memcpy(&saved, &resumed, sizeof(saved));
    input.stopAt = NULL;
    for (ordinal = 1u; ordinal <= 8u; ++ordinal) {
        int failed;
        ssa_oracle_resume_fail_allocation(ordinal);
        completed = ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic);
        failed = ssa_oracle_resume_allocation_failed();
        ssa_oracle_resume_fail_allocation(0u);
        TEST_ASSERT_EQUAL_UINT32(1u, resumedEffects.calls);
        if (completed) {
            TEST_ASSERT_FALSE(failed);
            break;
        }
        ++failures;
        TEST_ASSERT_TRUE(failed);
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, diagnostic.code);
        TEST_ASSERT_EQUAL_UINT32(1u, diagnostic.instructionId);
        TEST_ASSERT_EQUAL_UINT32(stop.sourceId, diagnostic.sourceId);
        TEST_ASSERT_EQUAL_MEMORY(&saved, &resumed, sizeof(saved));
        TEST_ASSERT_EQUAL_INT64(42, resumed.values[1].as.signedInteger);
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN, resumed.ownerStates[1]);
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_ORACLE_EVENT_CALL, resumed.events[0].kind);
    }
    TEST_ASSERT_TRUE(completed);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, failures);
    assert_execution(&uninterrupted, &resumed);
    ZrCore_ExecIr_OracleResultFree(&resumed);
    ZrCore_ExecIr_OracleResultFree(&uninterrupted);
}

static TZrBool invoke(void *userData,
                      const SZrExecIrInstruction *instruction,
                      const SZrExecIrOracleValue *operands,
                      TZrUInt32 operandCount,
                      SZrExecIrOracleValue *result, TZrBool *threw) {
    SResumeEffects *effects = (SResumeEffects *)userData;
    if (instruction->opcode != ZR_EXEC_IR_OPCODE_INVOKE || operandCount != 1u ||
        operands[0].kind != ZR_EXEC_IR_ORACLE_VALUE_SIGNED ||
        operands[0].as.signedInteger != 7) return ZR_FALSE;
    ++effects->calls;
    *threw = effects->throwInvoke;
    if (!*threw) *result = scalar(42);
    return ZR_TRUE;
}

static void compare_invoke_resume(TZrBool throwing) {
    TZrExecIrValueId receiver = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_TRUE);
    TZrExecIrValueId fallback = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_TRUE);
    TZrExecIrValueId result = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_FALSE);
    SZrExecIrOracleValue initial[3] = {scalar(7), scalar(99), scalar(-777)};
    SZrExecIrOracleExecutionResult uninterrupted, resumed;
    SResumeEffects referenceEffects = {0}, resumedEffects = {0};
    SZrExecIrOracleInput input;
    SZrExecIrOracleCheckpoint stop;
    blocks(3u);
    function.blocks[2].flags |= ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION;
    edges(1u, 2u, 3u);
    effect_metadata(emit(1u, ZR_EXEC_IR_OPCODE_INVOKE,
            ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE,
            receiver, result), 1u, ZR_FALSE, ZR_FALSE);
    function.instructions[0].successorRange = function.blocks[0].successorRange;
    function.blocks[0].terminatorInstructionId = 1u;
    terminate(2u, ZR_EXEC_IR_OPCODE_RETURN, 0u, result);
    terminate(3u, ZR_EXEC_IR_OPCODE_RETURN, 0u, fallback);
    build();
    referenceEffects.throwInvoke = resumedEffects.throwInvoke = throwing;
    input = oracle_input(initial, 3u, &referenceEffects);
    input.invoke = invoke;
    input.invokeUserData = &referenceEffects;
    ZrCore_ExecIr_OracleResultInit(&uninterrupted);
    ZrCore_ExecIr_OracleResultInit(&resumed);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &uninterrupted, &diagnostic));
    TEST_ASSERT_EQUAL_INT64(throwing ? 99 : 42,
                            uninterrupted.returnValue.as.signedInteger);
    stop = stop_at(1u, ZR_EXEC_IR_STATE_AFTER_EFFECT);
    input.stopAt = &stop;
    input.invokeUserData = &resumedEffects;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_TRUE(resumed.paused);
    TEST_ASSERT_EQUAL_UINT32(1u, resumedEffects.calls);
    TEST_ASSERT_EQUAL_UINT32(1u, resumed.eventCount);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_ORACLE_EVENT_CALL, resumed.events[0].kind);
    if (throwing) {
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED,
                          resumed.values[result - 1u].kind);
        resumed.values[result - 1u] = scalar(-777);
    }
    /* The provider would choose the other edge if execution were replayed. */
    resumedEffects.throwInvoke = !throwing;
    input.stopAt = NULL;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(1u, resumedEffects.calls);
    assert_execution(&uninterrupted, &resumed);
    if (throwing) {
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED,
                          resumed.values[result - 1u].kind);
    }
    ZrCore_ExecIr_OracleResultFree(&resumed);
    ZrCore_ExecIr_OracleResultFree(&uninterrupted);
}

static void test_after_invoke_resumes_normal_result_without_replaying_call(void) {
    compare_invoke_resume(ZR_FALSE);
}

static void test_after_throwing_invoke_resumes_exception_edge_without_normal_result(void) {
    compare_invoke_resume(ZR_TRUE);
}

static void compare_zero_identity(TZrBool generation) {
    SZrExecIrOracleExecutionResult resumed, saved;
    SZrExecIrOracleValue initial[1] = {scalar(7)};
    SResumeEffects effects = {0};
    SZrExecIrOracleInput input;
    SZrExecIrOracleCheckpoint stop;
    if (generation) function.contract.generation = 0u;
    else function.signatureHash = 0u;
    build_getter();
    stop = stop_at(1u, ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    input = oracle_input(initial, 1u, &effects);
    input.stopAt = &stop;
    ZrCore_ExecIr_OracleResultInit(&resumed);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_TRUE(resumed.paused);
    memcpy(&saved, &resumed, sizeof(saved));
    if (generation) function.contract.generation = 1u;
    else function.signatureHash = 1u;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    input.stopAt = NULL;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_EQUAL(generation ? ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION
                                 : ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                      diagnostic.code);
    TEST_ASSERT_EQUAL_MEMORY(&saved, &resumed, sizeof(saved));
    TEST_ASSERT_EQUAL_UINT32(0u, effects.calls);
    if (generation) function.contract.generation = 0u;
    else function.signatureHash = 0u;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_TRUE(resumed.returned);
    TEST_ASSERT_EQUAL_UINT32(1u, effects.calls);
    ZrCore_ExecIr_OracleResultFree(&resumed);
}

static void test_zero_generation_is_exact_captured_identity(void) {
    compare_zero_identity(ZR_TRUE);
}

static void test_zero_signature_is_exact_captured_identity(void) {
    compare_zero_identity(ZR_FALSE);
}

static void compare_resumed_suspend_payload(TZrBool suspendAgain) {
    TZrExecIrValueId payload = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_TRUE);
    TZrExecIrValueId firstResult = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_FALSE);
    TZrExecIrValueId secondResult = suspendAgain
            ? value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_FALSE) : 0u;
    SZrExecIrOracleValue initial[1] = {scalar(7)};
    SZrExecIrOracleExecutionResult resumed;
    SZrExecIrOracleCheckpoint firstStop, secondStop;
    SResumeEffects effects = {0};
    SZrExecIrOracleInput input;
    blocks(suspendAgain ? 3u : 2u);
    edges(1u, 2u, 0u);
    effect_metadata(emit(1u, ZR_EXEC_IR_OPCODE_SUSPEND,
            ZR_EXEC_IR_FLAG_MAY_SUSPEND, payload, firstResult),
            1u, ZR_TRUE, ZR_TRUE);
    function.instructions[0].successorRange = function.blocks[0].successorRange;
    function.blocks[0].terminatorInstructionId = 1u;
    if (suspendAgain) {
        edges(2u, 3u, 0u);
        effect_metadata(emit(2u, ZR_EXEC_IR_OPCODE_SUSPEND,
                ZR_EXEC_IR_FLAG_MAY_SUSPEND, 0u, secondResult),
                2u, ZR_TRUE, ZR_TRUE);
        function.instructions[1].successorRange = function.blocks[1].successorRange;
        function.blocks[1].terminatorInstructionId = 2u;
    }
    terminate(suspendAgain ? 3u : 2u, ZR_EXEC_IR_OPCODE_RETURN, 0u, 0u);
    build();
    firstStop = stop_at(1u, ZR_EXEC_IR_STATE_AFTER_EFFECT);
    input = oracle_input(initial, 1u, &effects);
    input.stopAt = &firstStop;
    ZrCore_ExecIr_OracleResultInit(&resumed);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_TRUE(resumed.paused);
    TEST_ASSERT_TRUE(resumed.suspended);
    TEST_ASSERT_EQUAL_INT64(7, resumed.returnValue.as.signedInteger);
    TEST_ASSERT_EQUAL_UINT32(1u, resumed.eventCount);
    if (suspendAgain) {
        secondStop = stop_at(2u, ZR_EXEC_IR_STATE_AFTER_EFFECT);
        input.stopAt = &secondStop;
    } else {
        input.stopAt = NULL;
    }
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED, resumed.returnValue.kind);
    if (suspendAgain) {
        TEST_ASSERT_TRUE(resumed.paused);
        TEST_ASSERT_TRUE(resumed.suspended);
        TEST_ASSERT_FALSE(resumed.returned);
        TEST_ASSERT_EQUAL_UINT32(2u, resumed.eventCount);
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_ORACLE_EVENT_SUSPEND, resumed.events[1].kind);
        TEST_ASSERT_EQUAL_UINT32(0u, resumed.events[1].operandCount);
        input.stopAt = NULL;
        TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    }
    TEST_ASSERT_TRUE(resumed.returned);
    TEST_ASSERT_FALSE(resumed.paused);
    TEST_ASSERT_FALSE(resumed.suspended);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED, resumed.returnValue.kind);
    TEST_ASSERT_EQUAL_UINT32(suspendAgain ? 2u : 1u, resumed.eventCount);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_ORACLE_EVENT_SUSPEND, resumed.events[0].kind);
    TEST_ASSERT_EQUAL_UINT32(1u, resumed.events[0].operandCount);
    TEST_ASSERT_EQUAL_UINT32(0u, effects.calls);
    ZrCore_ExecIr_OracleResultFree(&resumed);
}

static void test_resumed_void_return_does_not_inherit_suspend_payload(void) {
    compare_resumed_suspend_payload(ZR_FALSE);
}

static void test_resumed_payloadless_suspend_does_not_inherit_prior_payload(void) {
    compare_resumed_suspend_payload(ZR_TRUE);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_before_getter_resumes_once_with_same_selector);
    RUN_TEST(test_after_getter_restores_only_live_result_without_replay);
    RUN_TEST(test_store_and_cleanup_resume_preserve_effect_trace);
    RUN_TEST(test_stale_generation_rejects_without_consuming_checkpoint);
    RUN_TEST(test_missing_live_value_rejects_without_consuming_checkpoint);
    RUN_TEST(test_failed_callback_consumes_checkpoint_before_effect_can_replay);
    RUN_TEST(test_after_branch_preserves_selected_parallel_edge);
    RUN_TEST(test_in_block_resume_uses_materialized_phi_without_reentering_block);
    RUN_TEST(test_invalid_next_checkpoint_preserves_paused_frame);
    RUN_TEST(test_suspend_after_phase_enters_successor_without_repeating_suspend);
    RUN_TEST(test_repeated_loop_checkpoint_advances_and_accumulates_effects);
    RUN_TEST(test_empty_live_checkpoint_resumes_future_definition);
    RUN_TEST(test_malformed_successor_ordinal_preserves_paused_frame);
    RUN_TEST(test_each_resume_preparation_allocation_preserves_checkpoint_on_failure);
    RUN_TEST(test_after_invoke_resumes_normal_result_without_replaying_call);
    RUN_TEST(test_after_throwing_invoke_resumes_exception_edge_without_normal_result);
    RUN_TEST(test_zero_generation_is_exact_captured_identity);
    RUN_TEST(test_zero_signature_is_exact_captured_identity);
    RUN_TEST(test_resumed_void_return_does_not_inherit_suspend_payload);
    RUN_TEST(test_resumed_payloadless_suspend_does_not_inherit_prior_payload);
    return UNITY_END();
}
