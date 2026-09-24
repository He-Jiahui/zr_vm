#include "ssa_state_map_fixture.h"
#include "ssa_oracle_resume_fault_allocator.h"
#include "ssa_owner_fault_allocator.h"
#include "zr_vm_core/exec_ir_interpreter.h"

#include <string.h>

typedef struct SCleanupFixture {
    TZrExecIrValueId owner;
    TZrExecIrInstructionId poll;
    TZrExecIrInstructionId drop;
    TZrExecIrInstructionId call;
} SCleanupFixture;

typedef struct SCleanupEffects {
    TZrUInt32 calls;
    TZrUInt32 iterations;
    TZrBool throwing;
} SCleanupEffects;

static SZrExecIrOracleValue scalar(TZrInt64 number) {
    SZrExecIrOracleValue result = {0};
    result.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    result.as.signedInteger = number;
    return result;
}

static SZrExecIrOracleValue boolean(TZrBool flag) {
    SZrExecIrOracleValue result = {0};
    result.kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
    result.as.boolean = flag;
    return result;
}

static void effects(TZrExecIrInstructionId id, TZrUInt32 token,
                    TZrBool memory) {
    SZrExecIrInstruction *instruction = &function.instructions[id - 1u];
    TZrUInt32 next = token + 1u;
    instruction->effectIn = token;
    instruction->effectOut = next;
    if (memory) {
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendMemoryTokens(
                &function, &token, 1u, &instruction->memoryIn));
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendMemoryTokens(
                &function, &next, 1u, &instruction->memoryOut));
    }
}

static TZrExecIrInstructionId terminal(TZrExecIrBlockId block,
                                       EZrExecIrOpcode opcode,
                                       TZrExecIrValueId operand) {
    TZrExecIrInstructionId id = emit(block, opcode, 0u, operand, 0u);
    function.instructions[id - 1u].successorRange =
            function.blocks[block - 1u].successorRange;
    function.blocks[block - 1u].terminatorInstructionId = id;
    return id;
}

static void elaborate(void) {
    predecessors();
    TEST_ASSERT_TRUE(ZrParser_ExecIr_ElaborateCleanupDrops(&function, &diagnostic));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
}

/* The owner payload exists only on the true arm. Its cleanup obligation is
 * valid on both arms; an ordinary read of that payload would be invalid. */
static SCleanupFixture diamond(TZrBool moveOnTrue) {
    SCleanupFixture fixture = {0};
    TZrExecIrValueId condition = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_TRUE);
    TZrExecIrValueId answer = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_TRUE);
    fixture.owner = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, moveOnTrue);
    blocks(4u);
    function.blocks[3].flags |= ZR_EXEC_IR_BLOCK_FLAG_CLEANUP;
    edges(1u, 2u, 3u);
    edges(2u, 4u, 0u);
    edges(3u, 4u, 0u);
    terminal(1u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, condition);
    if (moveOnTrue) {
        TZrExecIrValueId destination = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_FALSE);
        emit(2u, ZR_EXEC_IR_OPCODE_MOVE, 0u, fixture.owner, destination);
        effects(emit(2u, ZR_EXEC_IR_OPCODE_DROP, 0u, destination, 0u), 1u, ZR_TRUE);
    } else {
        emit(2u, ZR_EXEC_IR_OPCODE_COPY, 0u, answer, fixture.owner);
    }
    terminal(2u, ZR_EXEC_IR_OPCODE_BRANCH, 0u);
    terminal(3u, ZR_EXEC_IR_OPCODE_BRANCH, 0u);
    fixture.poll = emit(4u, ZR_EXEC_IR_OPCODE_NOP,
                        ZR_EXEC_IR_FLAG_DEBUG_POLL, 0u, 0u);
    fixture.drop = emit(4u, ZR_EXEC_IR_OPCODE_DROP, 0u, fixture.owner, 0u);
    effects(fixture.drop, 3u, ZR_TRUE);
    terminal(4u, ZR_EXEC_IR_OPCODE_RETURN, answer);
    elaborate();
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED,
                      function.instructions[fixture.drop - 1u].opcode);
    return fixture;
}

static SZrExecIrOracleCheckpoint stop_at(TZrExecIrInstructionId id,
                                        EZrExecIrStateMapPhase phase) {
    const SZrExecIrStateMapEntry *entry = checkpoint(id, phase);
    SZrExecIrOracleCheckpoint point = {0};
    point.sourceId = entry->sourceId;
    point.resumeId = entry->resumeId;
    point.phase = phase;
    return point;
}

static TZrUInt32 drop_count(const SZrExecIrOracleExecutionResult *result) {
    TZrUInt32 count = 0u, index;
    for (index = 0u; index < result->eventCount; ++index) {
        count += result->events[index].kind == ZR_EXEC_IR_ORACLE_EVENT_DROP;
    }
    return count;
}

static void same_trace(const SZrExecIrOracleExecutionResult *expected,
                        const SZrExecIrOracleExecutionResult *actual) {
    TZrUInt32 index, operand;
    TEST_ASSERT_TRUE(actual->returned);
    TEST_ASSERT_FALSE(actual->paused);
    TEST_ASSERT_EQUAL(expected->returnValue.kind, actual->returnValue.kind);
    TEST_ASSERT_EQUAL_INT64(expected->returnValue.as.signedInteger,
                            actual->returnValue.as.signedInteger);
    TEST_ASSERT_EQUAL_UINT32(expected->executedInstructionCount,
                             actual->executedInstructionCount);
    TEST_ASSERT_EQUAL_UINT32(expected->eventCount, actual->eventCount);
    for (index = 0u; index < expected->eventCount; ++index) {
        const SZrExecIrOracleEvent *left = &expected->events[index];
        const SZrExecIrOracleEvent *right = &actual->events[index];
        TEST_ASSERT_EQUAL(left->kind, right->kind);
        TEST_ASSERT_EQUAL_UINT32(left->instructionId, right->instructionId);
        TEST_ASSERT_EQUAL_UINT32(left->sourceId, right->sourceId);
        TEST_ASSERT_EQUAL_UINT32(left->operandCount, right->operandCount);
        for (operand = 0u; operand < left->operandCount; ++operand) {
            TEST_ASSERT_EQUAL(left->operands[operand].kind, right->operands[operand].kind);
            TEST_ASSERT_EQUAL_MEMORY(&left->operands[operand].as,
                                     &right->operands[operand].as,
                                     sizeof(left->operands[operand].as));
        }
    }
}

static void compare_diamond(TZrBool takeTrue, TZrBool moved) {
    SCleanupFixture fixture = diamond(moved);
    SZrExecIrOracleValue initial[3] = {boolean(takeTrue), scalar(91), scalar(73)};
    SZrExecIrOracleExecutionResult uninterrupted, resumed;
    SZrExecIrOracleInput input = {0};
    TZrUInt32 which;
    input.function = &function;
    input.initialValues = initial;
    input.initialValueCount = moved ? 3u : 2u;
    ZrCore_ExecIr_OracleResultInit(&uninterrupted);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &uninterrupted, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(moved ? 1u : (takeTrue ? 1u : 0u), drop_count(&uninterrupted));
    for (which = 0u; which < 5u; ++which) {
        SZrExecIrOracleCheckpoint stop = stop_at(which < 2u ? fixture.poll : fixture.drop,
                which < 2u ? (EZrExecIrStateMapPhase)which
                           : (EZrExecIrStateMapPhase)(which - 2u));
        ZrCore_ExecIr_OracleResultInit(&resumed);
        input.stopAt = &stop;
        TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &resumed, &diagnostic));
        TEST_ASSERT_TRUE(resumed.paused);
        TEST_ASSERT_EQUAL_UINT32(function.valueCount, resumed.ownerStateCount);
        if (which < 3u) {
            TEST_ASSERT_EQUAL_UINT32(moved && takeTrue ? ZR_EXEC_IR_STATE_MAP_OWNER_MOVED
                    : (!moved && !takeTrue ? ZR_EXEC_IR_STATE_MAP_OWNER_UNINITIALIZED
                                           : ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED),
                    resumed.ownerStates[fixture.owner - 1u]);
        }
        input.stopAt = NULL;
        TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
        same_trace(&uninterrupted, &resumed);
        TEST_ASSERT_FALSE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
        ZrCore_ExecIr_OracleResultFree(&resumed);
    }
    ZrCore_ExecIr_OracleResultFree(&uninterrupted);
}

static void test_defined_arm_cleanup_survives_every_phase(void) { compare_diamond(ZR_TRUE, ZR_FALSE); }
static void test_uninitialized_arm_skips_cleanup_at_every_phase(void) { compare_diamond(ZR_FALSE, ZR_FALSE); }
static void test_moved_arm_does_not_drop_twice(void) { compare_diamond(ZR_TRUE, ZR_TRUE); }
static void test_unmoved_arm_drops_original_owner(void) { compare_diamond(ZR_FALSE, ZR_TRUE); }

static SZrExecIrResumeRequest request_at(SCleanupFixture fixture,
                                         SZrExecIrMaterializedState *target,
                                         TZrUInt32 *owners) {
    const SZrExecIrStateMapEntry *entry = checkpoint(fixture.poll, ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    SZrExecIrResumeRequest request = {0};
    request.function = &function;
    request.map = function.stateMap;
    request.functionToken = function.functionToken;
    request.signatureHash = function.signatureHash;
    request.generation = function.contract.generation;
    request.sourceId = entry->sourceId;
    request.resumeId = entry->resumeId;
    request.phase = entry->phase;
    request.target = target;
    request.ownerStates = owners;
    request.ownerStateCount = owners != NULL ? function.valueCount : 0u;
    return request;
}

static void test_runtime_witness_filters_roots_and_preserves_obligation(void) {
    SCleanupFixture fixture = diamond(ZR_FALSE);
    SZrExecIrMaterializedState target;
    TZrUInt32 owners[3] = {ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN,
        ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN, ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED};
    SZrExecIrResumeRequest request;
    TZrUInt32 index, matches = 0u;
    ZrCore_ExecIr_MaterializedStateInit(&target);
    request = request_at(fixture, &target, owners);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(1u, target.rootCount);
    TEST_ASSERT_EQUAL_UINT32(fixture.owner, target.roots[0]);
    owners[2] = ZR_EXEC_IR_STATE_MAP_OWNER_UNINITIALIZED;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(0u, target.rootCount);
    for (index = 0u; index < target.valueCount; ++index) {
        if (target.values[index] == fixture.owner) {
            ++matches;
            TEST_ASSERT_EQUAL(ZR_EXEC_IR_STATE_MAP_OWNER_UNINITIALIZED, target.ownerStates[index]);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1u, matches);
    ZrCore_ExecIr_MaterializedStateFree(&target);
}

static void test_missing_or_impossible_witness_preserves_target(void) {
    SCleanupFixture fixture = diamond(ZR_FALSE);
    SZrExecIrMaterializedState target, saved;
    TZrUInt32 owners[3] = {0u, 0u, ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED};
    SZrExecIrResumeRequest request;
    ZrCore_ExecIr_MaterializedStateInit(&target);
    request = request_at(fixture, &target, owners);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    saved = target;
    owners[2] = ZR_EXEC_IR_STATE_MAP_OWNER_DROPPED;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_EQUAL_MEMORY(&saved, &target, sizeof(saved));
    request.ownerStates = NULL;
    request.ownerStateCount = 0u;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL_MEMORY(&saved, &target, sizeof(saved));
    request.target = NULL;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    ZrCore_ExecIr_MaterializedStateFree(&target);
}

static void test_forged_paused_owner_does_not_consume_cursor(void) {
    SCleanupFixture fixture = diamond(ZR_FALSE);
    SZrExecIrOracleValue initial[2] = {boolean(ZR_FALSE), scalar(91)};
    SZrExecIrOracleExecutionResult result, saved;
    SZrExecIrOracleInput input = {0};
    SZrExecIrOracleCheckpoint stop = stop_at(fixture.poll, ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    input.function = &function;
    input.initialValues = initial;
    input.initialValueCount = 2u;
    input.stopAt = &stop;
    ZrCore_ExecIr_OracleResultInit(&result);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &result, &diagnostic));
    result.ownerStates[fixture.owner - 1u] = ZR_EXEC_IR_STATE_MAP_OWNER_DROPPED;
    saved = result;
    input.stopAt = NULL;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_ResumeOracleEx(&input, &result, &diagnostic));
    TEST_ASSERT_EQUAL_MEMORY(&saved, &result, sizeof(saved));
    TEST_ASSERT_EQUAL_UINT32(0u, drop_count(&result));
    result.ownerStates[fixture.owner - 1u] = ZR_EXEC_IR_STATE_MAP_OWNER_UNINITIALIZED;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &result, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(0u, drop_count(&result));
    ZrCore_ExecIr_OracleResultFree(&result);
}

static void test_ordinary_use_is_not_relaxed_by_conditional_cleanup(void) {
    SCleanupFixture fixture = diamond(ZR_FALSE);
    SZrExecIrStateMap *published = function.stateMap;
    SZrExecIrInstruction *ret = &function.instructions[function.instructionCount - 1u];
    function.operands[ret->operandRange.start] = fixture.owner;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    TEST_ASSERT_EQUAL_PTR(published, function.stateMap);
}

static void test_resume_allocation_failures_preserve_owner_and_cursor(void) {
    SCleanupFixture fixture = diamond(ZR_TRUE);
    SZrExecIrOracleValue initial[3] = {boolean(ZR_TRUE), scalar(91), scalar(73)};
    SZrExecIrOracleExecutionResult result, saved;
    SZrExecIrOracleInput input = {0};
    SZrExecIrOracleCheckpoint stop = stop_at(fixture.poll, ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    size_t ordinal;
    input.function = &function;
    input.initialValues = initial;
    input.initialValueCount = 3u;
    input.stopAt = &stop;
    ZrCore_ExecIr_OracleResultInit(&result);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &result, &diagnostic));
    saved = result;
    input.stopAt = NULL;
    for (ordinal = 1u; ordinal < 32u; ++ordinal) {
        TZrBool ok;
        int injected;
        ssa_oracle_resume_fail_allocation(ordinal);
        ok = ZrCore_ExecIr_ResumeOracleEx(&input, &result, &diagnostic);
        injected = ssa_oracle_resume_allocation_failed();
        ssa_oracle_resume_fail_allocation(0u);
        if (ok) {
            TEST_ASSERT_FALSE(injected);
            break;
        }
        TEST_ASSERT_TRUE(injected);
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, diagnostic.code);
        TEST_ASSERT_EQUAL_MEMORY(&saved, &result, sizeof(saved));
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_STATE_MAP_OWNER_MOVED, result.ownerStates[fixture.owner - 1u]);
        TEST_ASSERT_EQUAL_UINT32(1u, drop_count(&result));
    }
    TEST_ASSERT_LESS_THAN_UINT32(32u, ordinal);
    TEST_ASSERT_GREATER_THAN_UINT32(2u, ordinal);
    TEST_ASSERT_TRUE(result.returned);
    TEST_ASSERT_EQUAL_UINT32(1u, drop_count(&result));
    ZrCore_ExecIr_OracleResultFree(&result);
}

static TZrBool invoke(void *data, const SZrExecIrInstruction *instruction,
                       const SZrExecIrOracleValue *operands, TZrUInt32 count,
                       SZrExecIrOracleValue *result, TZrBool *threw) {
    SCleanupEffects *state = (SCleanupEffects *)data;
    (void)instruction;
    if (count != 1u || operands[0].kind != ZR_EXEC_IR_ORACLE_VALUE_SIGNED) return ZR_FALSE;
    ++state->calls;
    *threw = state->throwing;
    if (!*threw) *result = scalar(52);
    return ZR_TRUE;
}

static void compare_exception_cleanup(TZrBool throwing) {
    TZrExecIrValueId receiver = value(ZR_EXEC_IR_OWNERSHIP_GC, ZR_TRUE);
    TZrExecIrValueId answer = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_TRUE);
    TZrExecIrValueId owner = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_FALSE);
    SZrExecIrOracleValue initial[2] = {scalar(7), scalar(91)};
    SZrExecIrOracleExecutionResult uninterrupted, resumed;
    SZrExecIrOracleInput input = {0};
    SCleanupEffects reference = {0}, actual = {0};
    TZrExecIrInstructionId call, poll, drop;
    TZrUInt32 which;
    blocks(4u);
    function.blocks[2].flags |= ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION;
    function.blocks[3].flags |= ZR_EXEC_IR_BLOCK_FLAG_CLEANUP;
    edges(1u, 2u, 3u);
    edges(2u, 4u, 0u);
    edges(3u, 4u, 0u);
    call = emit(1u, ZR_EXEC_IR_OPCODE_INVOKE,
                ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE, receiver, owner);
    effects(call, 1u, ZR_FALSE);
    function.instructions[call - 1u].successorRange = function.blocks[0].successorRange;
    function.blocks[0].terminatorInstructionId = call;
    terminal(2u, ZR_EXEC_IR_OPCODE_BRANCH, 0u);
    terminal(3u, ZR_EXEC_IR_OPCODE_BRANCH, 0u);
    poll = emit(4u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_DEBUG_POLL, 0u, 0u);
    drop = emit(4u, ZR_EXEC_IR_OPCODE_DROP, 0u, owner, 0u);
    effects(drop, 2u, ZR_TRUE);
    terminal(4u, ZR_EXEC_IR_OPCODE_RETURN, answer);
    elaborate();
    reference.throwing = actual.throwing = throwing;
    input.function = &function;
    input.initialValues = initial;
    input.initialValueCount = 2u;
    input.invoke = invoke;
    input.invokeUserData = &reference;
    ZrCore_ExecIr_OracleResultInit(&uninterrupted);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &uninterrupted, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(throwing ? 0u : 1u, drop_count(&uninterrupted));
    for (which = 0u; which < 2u; ++which) {
        SZrExecIrOracleCheckpoint stop = stop_at(which == 0u ? call : poll,
                which == 0u ? ZR_EXEC_IR_STATE_AFTER_EFFECT : ZR_EXEC_IR_STATE_BEFORE_EFFECT);
        actual.calls = 0u;
        actual.throwing = throwing;
        input.invokeUserData = &actual;
        input.stopAt = &stop;
        ZrCore_ExecIr_OracleResultInit(&resumed);
        TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &resumed, &diagnostic));
        TEST_ASSERT_TRUE(resumed.paused);
        TEST_ASSERT_EQUAL_UINT32(1u, actual.calls);
        actual.throwing = !throwing;
        input.stopAt = NULL;
        TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
        TEST_ASSERT_EQUAL_UINT32(1u, actual.calls);
        same_trace(&uninterrupted, &resumed);
        ZrCore_ExecIr_OracleResultFree(&resumed);
    }
    ZrCore_ExecIr_OracleResultFree(&uninterrupted);
}

static void test_successful_invoke_initializes_cleanup_once(void) { compare_exception_cleanup(ZR_FALSE); }
static void test_throwing_invoke_keeps_cleanup_uninitialized(void) { compare_exception_cleanup(ZR_TRUE); }

static TZrBool loop_call(void *data, const SZrExecIrInstruction *instruction,
                         const SZrExecIrOracleValue *operands, TZrUInt32 count,
                         SZrExecIrOracleValue *result) {
    SCleanupEffects *state = (SCleanupEffects *)data;
    (void)operands;
    if (count != 0u) return ZR_FALSE;
    ++state->calls;
    if (instruction->sourceId == 102u) {
        ++state->iterations;
        *result = scalar(1000 + state->iterations);
    } else {
        *result = boolean(state->iterations < 257u);
    }
    return ZR_TRUE;
}

static void test_loop_reinitializes_owner_across_repeated_cleanup_resume(void) {
    TZrExecIrValueId answer = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_TRUE);
    TZrExecIrValueId owner = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_FALSE);
    TZrExecIrValueId again = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_FALSE);
    SZrExecIrOracleValue initial[1] = {scalar(91)};
    SZrExecIrOracleExecutionResult uninterrupted, resumed;
    SZrExecIrOracleInput input = {0};
    SZrExecIrOracleCheckpoint stop;
    SCleanupEffects reference = {0}, actual = {0};
    TZrExecIrInstructionId drop;
    TZrUInt32 pauses = 0u;
    blocks(3u);
    function.blocks[1].flags |= ZR_EXEC_IR_BLOCK_FLAG_CLEANUP;
    edges(1u, 2u, 0u);
    edges(2u, 2u, 3u);
    terminal(1u, ZR_EXEC_IR_OPCODE_BRANCH, 0u);
    effects(emit(2u, ZR_EXEC_IR_OPCODE_CALL,
            ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE, 0u, owner), 1u, ZR_TRUE);
    drop = emit(2u, ZR_EXEC_IR_OPCODE_DROP, 0u, owner, 0u);
    effects(drop, 2u, ZR_TRUE);
    effects(emit(2u, ZR_EXEC_IR_OPCODE_CALL,
            ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE, 0u, again), 3u, ZR_TRUE);
    terminal(2u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, again);
    terminal(3u, ZR_EXEC_IR_OPCODE_RETURN, answer);
    elaborate();
    assert_only_root(drop, ZR_EXEC_IR_STATE_CLEANUP_COMPLETE, 0u);
    input.function = &function;
    input.initialValues = initial;
    input.initialValueCount = 1u;
    input.call = loop_call;
    input.userData = &reference;
    input.maxSteps = 4096u;
    ZrCore_ExecIr_OracleResultInit(&uninterrupted);
    ZrCore_ExecIr_OracleResultInit(&resumed);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &uninterrupted, &diagnostic));
    stop = stop_at(drop, ZR_EXEC_IR_STATE_CLEANUP_COMPLETE);
    input.stopAt = &stop;
    input.userData = &actual;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &resumed, &diagnostic));
    while (resumed.paused && pauses < 258u) {
        ++pauses;
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_STATE_MAP_OWNER_DROPPED, resumed.ownerStates[owner - 1u]);
        TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &resumed, &diagnostic));
    }
    TEST_ASSERT_EQUAL_UINT32(257u, pauses);
    TEST_ASSERT_EQUAL_UINT32(257u, actual.iterations);
    TEST_ASSERT_EQUAL_UINT32(reference.calls, actual.calls);
    TEST_ASSERT_EQUAL_UINT32(257u, drop_count(&resumed));
    same_trace(&uninterrupted, &resumed);
    ZrCore_ExecIr_OracleResultFree(&resumed);
    ZrCore_ExecIr_OracleResultFree(&uninterrupted);
}

static void test_guarded_drop_requires_cleanup_block(void) {
    SCleanupFixture fixture = diamond(ZR_FALSE);
    function.blocks[3].flags &= ~ZR_EXEC_IR_BLOCK_FLAG_CLEANUP;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(fixture.drop, diagnostic.instructionId);
    TEST_ASSERT_EQUAL_UINT32(100u + fixture.drop, diagnostic.sourceId);
}

static void test_guarded_drop_rejects_nonowner_operand(void) {
    SCleanupFixture fixture = diamond(ZR_FALSE);
    function.values[fixture.owner - 1u].ownership = ZR_EXEC_IR_OWNERSHIP_GC;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(fixture.drop, diagnostic.instructionId);
}

static void test_guarded_drop_rejects_missing_value_id(void) {
    SCleanupFixture fixture = diamond(ZR_FALSE);
    function.operands[function.instructions[fixture.drop - 1u].operandRange.start] = function.valueCount + 1u;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    /* This malformed pool is rejected before per-instruction verification. */
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(function.functionToken, diagnostic.functionToken);
}

static void test_ordinary_double_drop_still_fails(void) {
    TZrExecIrValueId owner = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_TRUE);
    TZrExecIrValueId answer = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_TRUE);
    SZrExecIrOracleValue initial[2] = {scalar(73), scalar(91)};
    SZrExecIrOracleExecutionResult result;
    SZrExecIrOracleInput input = {0};
    blocks(1u);
    effects(emit(1u, ZR_EXEC_IR_OPCODE_DROP, 0u, owner, 0u), 1u, ZR_TRUE);
    effects(emit(1u, ZR_EXEC_IR_OPCODE_DROP, 0u, owner, 0u), 2u, ZR_TRUE);
    terminal(1u, ZR_EXEC_IR_OPCODE_RETURN, answer);
    predecessors();
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    input.function = &function;
    input.initialValues = initial;
    input.initialValueCount = 2u;
    ZrCore_ExecIr_OracleResultInit(&result);
    TEST_ASSERT_FALSE(ZrCore_ExecIr_RunOracleEx(&input, &result, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(2u, diagnostic.instructionId);
    ZrCore_ExecIr_OracleResultFree(&result);
}

static void test_clone_preserves_independent_cleanup_metadata(void) {
    SCleanupFixture fixture = diamond(ZR_FALSE);
    SZrExecIrFunction clone;
    SZrExecIrOracleValue initial[2] = {boolean(ZR_FALSE), scalar(91)};
    SZrExecIrOracleExecutionResult result;
    SZrExecIrOracleInput input = {0};
    SZrExecIrOracleCheckpoint stop = stop_at(fixture.poll, ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    ZrCore_ExecIr_FunctionInit(&clone);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_CloneFunction(&function, &clone, &diagnostic));
    TEST_ASSERT_NOT_EQUAL(function.stateMap, clone.stateMap);
    TEST_ASSERT_NOT_EQUAL(function.stateMap->ownerStatePool, clone.stateMap->ownerStatePool);
    TEST_ASSERT_EQUAL_MEMORY(function.stateMap->ownerStatePool, clone.stateMap->ownerStatePool,
            function.stateMap->ownerStateCount * sizeof(*function.stateMap->ownerStatePool));
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_FunctionInit(&function);
    input.function = &clone;
    input.initialValues = initial;
    input.initialValueCount = 2u;
    input.stopAt = &stop;
    ZrCore_ExecIr_OracleResultInit(&result);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &result, &diagnostic));
    input.stopAt = NULL;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &result, &diagnostic));
    TEST_ASSERT_TRUE(result.returned);
    TEST_ASSERT_EQUAL_UINT32(0u, drop_count(&result));
    ZrCore_ExecIr_OracleResultFree(&result);
    ZrCore_ExecIr_FreeFunction(&clone);
}

static void test_elaboration_rejects_sealed_function_without_replacement(void) {
    SCleanupFixture fixture = diamond(ZR_FALSE);
    SZrExecIrInstruction *instructions = function.instructions;
    SZrExecIrStateMap *map = function.stateMap;
    TZrUInt32 generation = function.contract.generation;
    function.instructions[fixture.drop - 1u].opcode = ZR_EXEC_IR_OPCODE_DROP;
    function.sealed = ZR_TRUE;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_ElaborateCleanupDrops(&function, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_SEALED, diagnostic.code);
    TEST_ASSERT_EQUAL_PTR(instructions, function.instructions);
    TEST_ASSERT_EQUAL_PTR(map, function.stateMap);
    TEST_ASSERT_EQUAL_UINT32(generation, function.contract.generation);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_OPCODE_DROP, function.instructions[fixture.drop - 1u].opcode);
}

static void test_elaboration_owner_analysis_oom_is_transactional(void) {
    SCleanupFixture fixture = diamond(ZR_FALSE);
    SZrExecIrFunction saved;
    size_t ordinal;
    function.instructions[fixture.drop - 1u].opcode = ZR_EXEC_IR_OPCODE_DROP;
    saved = function;
    for (ordinal = 1u; ordinal < 64u; ++ordinal) {
        TZrBool ok;
        int injected;
        ssa_owner_fail_allocation(ordinal);
        ok = ZrParser_ExecIr_ElaborateCleanupDrops(&function, &diagnostic);
        injected = ssa_owner_allocation_failed();
        ssa_owner_fail_allocation(0u);
        TEST_ASSERT_EQUAL_UINT32(0u, ssa_owner_outstanding_allocations());
        if (ok) {
            TEST_ASSERT_FALSE(injected);
            break;
        }
        TEST_ASSERT_TRUE(injected);
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, diagnostic.code);
        TEST_ASSERT_EQUAL_MEMORY(&saved, &function, sizeof(saved));
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_OPCODE_DROP, function.instructions[fixture.drop - 1u].opcode);
    }
    TEST_ASSERT_GREATER_THAN_UINT32(1u, ordinal);
    TEST_ASSERT_LESS_THAN_UINT32(64u, ordinal);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED,
                      function.instructions[fixture.drop - 1u].opcode);
}

static void test_forged_initialized_witness_requires_valid_payload(void) {
    SCleanupFixture fixture = diamond(ZR_FALSE);
    SZrExecIrOracleValue initial[2] = {boolean(ZR_FALSE), scalar(91)};
    SZrExecIrOracleExecutionResult result, saved;
    SZrExecIrOracleInput input = {0};
    SZrExecIrOracleCheckpoint stop = stop_at(fixture.poll, ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    input.function = &function;
    input.initialValues = initial;
    input.initialValueCount = 2u;
    input.stopAt = &stop;
    ZrCore_ExecIr_OracleResultInit(&result);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &result, &diagnostic));
    result.ownerStates[fixture.owner - 1u] = ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED;
    saved = result;
    input.stopAt = NULL;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_ResumeOracleEx(&input, &result, &diagnostic));
    TEST_ASSERT_EQUAL_MEMORY(&saved, &result, sizeof(saved));
    TEST_ASSERT_EQUAL_UINT32(fixture.poll, diagnostic.instructionId);
    result.ownerStates[fixture.owner - 1u] = ZR_EXEC_IR_STATE_MAP_OWNER_UNINITIALIZED;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &result, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(0u, drop_count(&result));
    ZrCore_ExecIr_OracleResultFree(&result);
}

static void test_shared_owner_uses_same_conditional_cleanup_contract(void) {
    SCleanupFixture fixture = diamond(ZR_FALSE);
    SZrExecIrOracleValue initial[2] = {boolean(ZR_TRUE), scalar(91)};
    SZrExecIrOracleExecutionResult result;
    SZrExecIrOracleInput input = {0};
    SZrExecIrOracleCheckpoint stop;
    function.values[fixture.owner - 1u].ownership = ZR_EXEC_IR_OWNERSHIP_SHARED;
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    stop = stop_at(fixture.poll, ZR_EXEC_IR_STATE_AFTER_EFFECT);
    input.function = &function;
    input.initialValues = initial;
    input.initialValueCount = 2u;
    input.stopAt = &stop;
    ZrCore_ExecIr_OracleResultInit(&result);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &result, &diagnostic));
    input.stopAt = NULL;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &result, &diagnostic));
    TEST_ASSERT_TRUE(result.returned);
    TEST_ASSERT_EQUAL_UINT32(1u, drop_count(&result));
    ZrCore_ExecIr_OracleResultFree(&result);
}

static void test_oracle_rejects_owner_that_was_never_defined(void) {
    TZrExecIrValueId answer = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_TRUE);
    TZrExecIrValueId owner = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_FALSE);
    SZrExecIrOracleValue initial[1] = {scalar(91)};
    SZrExecIrOracleExecutionResult result;
    SZrExecIrOracleInput input = {0};
    blocks(1u);
    function.blocks[0].flags |= ZR_EXEC_IR_BLOCK_FLAG_CLEANUP;
    effects(emit(1u, ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED, 0u, owner, 0u), 1u, ZR_TRUE);
    terminal(1u, ZR_EXEC_IR_OPCODE_RETURN, answer);
    predecessors();
    input.function = &function;
    input.initialValues = initial;
    input.initialValueCount = 1u;
    ZrCore_ExecIr_OracleResultInit(&result);
    TEST_ASSERT_FALSE(ZrCore_ExecIr_RunOracleEx(&input, &result, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(1u, diagnostic.instructionId);
    TEST_ASSERT_EQUAL_UINT32(101u, diagnostic.sourceId);
    ZrCore_ExecIr_OracleResultFree(&result);
}

static void test_paused_owner_buffer_alias_is_rejected_before_commit(void) {
    TZrExecIrValueId answer = value(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_FALSE);
    SZrExecIrOracleExecutionResult result, saved;
    SZrExecIrOracleInput input = {0};
    SZrExecIrOracleCheckpoint stop;
    TZrUInt32 *ownedStates;
    TZrUInt32 offset;
    blocks(1u);
    emit(1u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_DEBUG_POLL, 0u, 0u);
    emit(1u, ZR_EXEC_IR_OPCODE_CONSTANT, 0u, 0u, answer);
    function.instructions[1].layoutId = 91u;
    terminal(1u, ZR_EXEC_IR_OPCODE_RETURN, answer);
    build();
    TEST_ASSERT_EQUAL_UINT32(0u, checkpoint(1u, ZR_EXEC_IR_STATE_BEFORE_EFFECT)->liveValues.count);
    stop = stop_at(1u, ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    input.function = &function;
    input.stopAt = &stop;
    ZrCore_ExecIr_OracleResultInit(&result);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &result, &diagnostic));
    ownedStates = result.ownerStates;
    input.stopAt = NULL;
    for (offset = 0u; offset < 2u; ++offset) {
        TZrBool ok;
        result.ownerStates = (TZrUInt32 *)(void *)result.values + offset;
        saved = result;
        ok = ZrCore_ExecIr_ResumeOracleEx(&input, &result, &diagnostic);
        /* Restore the actual allocation before any test failure cleanup. */
        result.ownerStates = ownedStates;
        saved.ownerStates = ownedStates;
        TEST_ASSERT_FALSE(ok);
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
        TEST_ASSERT_EQUAL_MEMORY(&saved, &result, sizeof(saved));
    }
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &result, &diagnostic));
    TEST_ASSERT_EQUAL_INT64(91, result.returnValue.as.signedInteger);
    ZrCore_ExecIr_OracleResultFree(&result);
}

static void test_paused_owner_buffer_cannot_own_function_map_storage(void) {
    TZrExecIrValueId owner = value(ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_TRUE);
    SZrExecIrOracleValue initial = scalar(73);
    SZrExecIrOracleExecutionResult result, saved;
    SZrExecIrOracleInput input = {0};
    SZrExecIrOracleCheckpoint stop;
    TZrUInt32 *ownedStates;
    TZrUInt32 offset;
    blocks(1u);
    function.blocks[0].flags |= ZR_EXEC_IR_BLOCK_FLAG_CLEANUP;
    emit(1u, ZR_EXEC_IR_OPCODE_NOP, ZR_EXEC_IR_FLAG_DEBUG_POLL, 0u, 0u);
    effects(emit(1u, ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED, 0u, owner, 0u), 1u, ZR_TRUE);
    terminal(1u, ZR_EXEC_IR_OPCODE_RETURN, 0u);
    build();
    TEST_ASSERT_EQUAL_UINT32(0u, checkpoint(2u, ZR_EXEC_IR_STATE_AFTER_EFFECT)->liveValues.count);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2u, function.stateMap->ownerStateCount);
    stop = stop_at(2u, ZR_EXEC_IR_STATE_AFTER_EFFECT);
    input.function = &function;
    input.initialValues = &initial;
    input.initialValueCount = 1u;
    input.stopAt = &stop;
    ZrCore_ExecIr_OracleResultInit(&result);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_RunOracleEx(&input, &result, &diagnostic));
    ownedStates = result.ownerStates;
    input.stopAt = NULL;
    for (offset = 0u; offset < 2u; ++offset) {
        TZrBool ok;
        TEST_ASSERT_EQUAL_UINT32(ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED,
                                function.stateMap->ownerStatePool[offset]);
        result.ownerStates = function.stateMap->ownerStatePool + offset;
        saved = result;
        ok = ZrCore_ExecIr_ResumeOracleEx(&input, &result, &diagnostic);
        if (ok) {
            /* A broken commit freed the source pool. Detach it so the base-
             * alias regression reports an assertion rather than double-free. */
            function.stateMap->ownerStatePool = NULL;
            function.stateMap->ownerStateCount = 0u;
            function.stateMap->ownerStateCapacity = 0u;
            free(ownedStates);
            ZrCore_ExecIr_OracleResultFree(&result);
            TEST_FAIL_MESSAGE("resume accepted function-owned owner-state storage");
        }
        result.ownerStates = ownedStates;
        saved.ownerStates = ownedStates;
        TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
        TEST_ASSERT_EQUAL_MEMORY(&saved, &result, sizeof(saved));
    }
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ResumeOracleEx(&input, &result, &diagnostic));
    TEST_ASSERT_TRUE(result.returned);
    TEST_ASSERT_EQUAL_UINT32(1u, drop_count(&result));
    ZrCore_ExecIr_OracleResultFree(&result);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_defined_arm_cleanup_survives_every_phase);
    RUN_TEST(test_uninitialized_arm_skips_cleanup_at_every_phase);
    RUN_TEST(test_moved_arm_does_not_drop_twice);
    RUN_TEST(test_unmoved_arm_drops_original_owner);
    RUN_TEST(test_runtime_witness_filters_roots_and_preserves_obligation);
    RUN_TEST(test_missing_or_impossible_witness_preserves_target);
    RUN_TEST(test_forged_paused_owner_does_not_consume_cursor);
    RUN_TEST(test_ordinary_use_is_not_relaxed_by_conditional_cleanup);
    RUN_TEST(test_resume_allocation_failures_preserve_owner_and_cursor);
    RUN_TEST(test_successful_invoke_initializes_cleanup_once);
    RUN_TEST(test_throwing_invoke_keeps_cleanup_uninitialized);
    RUN_TEST(test_loop_reinitializes_owner_across_repeated_cleanup_resume);
    RUN_TEST(test_guarded_drop_requires_cleanup_block);
    RUN_TEST(test_guarded_drop_rejects_nonowner_operand);
    RUN_TEST(test_guarded_drop_rejects_missing_value_id);
    RUN_TEST(test_ordinary_double_drop_still_fails);
    RUN_TEST(test_clone_preserves_independent_cleanup_metadata);
    RUN_TEST(test_elaboration_rejects_sealed_function_without_replacement);
    RUN_TEST(test_elaboration_owner_analysis_oom_is_transactional);
    RUN_TEST(test_forged_initialized_witness_requires_valid_payload);
    RUN_TEST(test_shared_owner_uses_same_conditional_cleanup_contract);
    RUN_TEST(test_oracle_rejects_owner_that_was_never_defined);
    RUN_TEST(test_paused_owner_buffer_alias_is_rejected_before_commit);
    RUN_TEST(test_paused_owner_buffer_cannot_own_function_map_storage);
    return UNITY_END();
}
