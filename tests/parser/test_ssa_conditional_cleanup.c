#include "ssa_state_map_fixture.h"
#include "ssa_oracle_resume_fault_allocator.h"
#include "ssa_owner_fault_allocator.h"
#include "zr_vm_core/exec_ir_interpreter.h"

#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif

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

static SZrExecIrOracleValue oracle_boolean(TZrBool flag) {
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
        TZrExecIrMemoryTokenId before = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
                ZR_EXEC_IR_MEMORY_OWNERSHIP, token);
        TZrExecIrMemoryTokenId after = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
                ZR_EXEC_IR_MEMORY_OWNERSHIP, next);
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendMemoryTokens(
                &function, &before, 1u, &instruction->memoryIn));
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendMemoryTokens(
                &function, &after, 1u, &instruction->memoryOut));
    }
}

static void call_effects(TZrExecIrInstructionId id, TZrUInt32 token,
                         TZrUInt32 memoryToken) {
    SZrExecIrInstruction *instruction = &function.instructions[id - 1u];
    TZrUInt32 next = token + 1u;
    TZrExecIrMemoryTokenId before[2] = {
            ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, memoryToken),
            ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, memoryToken)};
    TZrExecIrMemoryTokenId after[2] = {
            ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, memoryToken + 1u),
            ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, memoryToken + 1u)};
    instruction->effectIn = token;
    instruction->effectOut = next;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendMemoryTokens(
            &function, before, 2u, &instruction->memoryIn));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendMemoryTokens(
            &function, after, 2u, &instruction->memoryOut));
}

static void ownership_effects(TZrExecIrInstructionId id,
                              TZrUInt32 effectToken,
                              TZrUInt32 memoryToken) {
    SZrExecIrInstruction *instruction = &function.instructions[id - 1u];
    TZrExecIrMemoryTokenId before = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_OWNERSHIP, memoryToken);
    TZrExecIrMemoryTokenId after = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_OWNERSHIP, memoryToken + 1u);
    instruction->effectIn = effectToken;
    instruction->effectOut = effectToken + 1u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendMemoryTokens(
            &function, &before, 1u, &instruction->memoryIn));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendMemoryTokens(
            &function, &after, 1u, &instruction->memoryOut));
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
    TZrBool elaborated;
    char failure[192];
    predecessors();
    elaborated = ZrParser_ExecIr_ElaborateCleanupDrops(&function, &diagnostic);
    (void)snprintf(failure, sizeof(failure),
            "cleanup fixture code=%u block=%u instruction=%u expected=%u actual=%u",
            (unsigned)diagnostic.code, (unsigned)diagnostic.blockId,
            (unsigned)diagnostic.instructionId,
            (unsigned)diagnostic.expectedVersion, (unsigned)diagnostic.actualVersion);
    TEST_ASSERT_TRUE_MESSAGE(elaborated, failure);
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
    if (moveOnTrue) {
        SZrExecIrPhiIncoming effectIncoming[2] = {{2u, 2u}, {3u, 0u}};
        SZrExecIrPhiIncoming memoryIncoming[2] = {
                {2u, ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_OWNERSHIP, 2u)},
                {3u, 0u}};
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionSetEffectPhi(
                &function, 4u, 3u, effectIncoming, 2u));
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionSetMemoryPhi(
                &function, 4u, ZR_EXEC_IR_MEMORY_OWNERSHIP,
                ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_OWNERSHIP, 3u),
                memoryIncoming, 2u));
    }
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
    SZrExecIrOracleValue initial[3] = {oracle_boolean(takeTrue), scalar(91), scalar(73)};
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
    SZrExecIrOracleValue initial[2] = {oracle_boolean(ZR_FALSE), scalar(91)};
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
    SZrExecIrOracleValue initial[3] = {oracle_boolean(ZR_TRUE), scalar(91), scalar(73)};
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
    call_effects(call, 1u, 1u);
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
        *result = oracle_boolean(state->iterations < 257u);
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
    call_effects(emit(2u, ZR_EXEC_IR_OPCODE_CALL,
            ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE, 0u, owner), 1u, 1u);
    drop = emit(2u, ZR_EXEC_IR_OPCODE_DROP, 0u, owner, 0u);
    ownership_effects(drop, 2u, 1u);
    call_effects(emit(2u, ZR_EXEC_IR_OPCODE_CALL,
            ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE, 0u, again), 3u, 2u);
    terminal(2u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, again);
    terminal(3u, ZR_EXEC_IR_OPCODE_RETURN, answer);
    {
        SZrExecIrPhiIncoming effectIncoming[2] = {{1u, 0u}, {2u, 4u}};
        SZrExecIrPhiIncoming memoryIncoming[2] = {
                {1u, 0u},
                {2u, ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_OWNERSHIP, 2u)}};
        SZrExecIrPhiIncoming heapIncoming[2] = {
                {1u, 0u},
                {2u, ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 3u)}};
        SZrExecIrPhiIncoming ffiIncoming[2] = {
                {1u, 0u},
                {2u, ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 3u)}};
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionSetEffectPhi(
                &function, 2u, 1u, effectIncoming, 2u));
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionSetMemoryPhi(
                &function, 2u, ZR_EXEC_IR_MEMORY_OWNERSHIP,
                ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_OWNERSHIP, 1u),
                memoryIncoming, 2u));
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionSetMemoryPhi(
                &function, 2u, ZR_EXEC_IR_MEMORY_MANAGED_HEAP,
                ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u),
                heapIncoming, 2u));
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionSetMemoryPhi(
                &function, 2u, ZR_EXEC_IR_MEMORY_NATIVE_FFI,
                ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 1u),
                ffiIncoming, 2u));
    }
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
    SZrExecIrOracleValue initial[2] = {oracle_boolean(ZR_FALSE), scalar(91)};
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
    /* diamond() already lowered and mapped guarded DROP. The source for this
     * test is pre-lowering IR; discard metadata for the previous instruction. */
    ZrCore_ExecIr_StateMapFree(function.stateMap);
    free(function.stateMap);
    function.stateMap = ZR_NULL;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(&function,
            ZR_EXEC_IR_VERIFY_STRUCTURE | ZR_EXEC_IR_VERIFY_EFFECT, &diagnostic));
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
    SZrExecIrOracleValue initial[2] = {oracle_boolean(ZR_FALSE), scalar(91)};
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
    SZrExecIrOracleValue initial[2] = {oracle_boolean(ZR_TRUE), scalar(91)};
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
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    setvbuf(stdout, NULL, _IONBF, 0);
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

/**
 * @struct SCleanupFixture
 *
 * ## Summary
 * 保存 diamond 测试图中供后续 oracle 或 materialization 场景引用的 ExecIR 标识；它只携带编号，不拥有函数池或运行期状态。
 *
 * ## Fields
 * owner、poll、drop 是值或指令的一基标识，只有在构造它们的全局 function 仍是当前 fixture 时才可用于索引。call 字段仅随聚合对象零初始化，本翻译单元没有显式赋值或读取，不能据此推断有 invoke 覆盖。
 *
 * ## Invariants and lifecycle
 * diamond 返回该值副本；重置或释放全局 function 后，其中的 ID 不再保证指向同一语义对象。结构体本身不分配资源。
 *
 * ## References
 * diamond 构造该 fixture；compare_diamond、request_at 及直接测试用其 ID 建立检查点和断言。
 */

/**
 * @struct SCleanupEffects
 *
 * ## Summary
 * 为同步 oracle 回调记录调用次数、循环次数及是否抛出异常的测试状态。
 *
 * ## Fields
 * calls 记录回调次数，iterations 记录特定循环调用次数，throwing 选择 invoke 的返回路径。
 *
 * ## Invariants and lifecycle
 * 回调通过 userData 借用此结构；本文件将结构放在发起同步 Run/Resume 的栈帧中，必须保持到该次调用返回。它不拥有 ExecIR 值或回调资源。
 *
 * ## References
 * invoke、loop_call 读取或更新该状态；compare_exception_cleanup 与 test_loop_reinitializes_owner_across_repeated_cleanup_resume 管理其生命周期。
 */

/**
 * @fn void effects(TZrExecIrInstructionId id, TZrUInt32 token, TZrBool memory)
 *
 * ## Summary
 * 为测试构造的当前指令设置 effect 输入/输出 token；按需追加 ownership memory token，以形成可供 verifier 检查的指令元数据。
 *
 * ## Parameters
 * @param[in] id 当前全局 function 中的有效一基指令编号。
 * @param[in] token 指令的 effect 输入 token；函数把 token 加一作为输出。
 * @param[in] memory 为真时追加 ownership 域的前后 memory token，为假时不改 memory 范围。
 *
 * ## Returns
 * 无返回值；元数据写入全局 function，追加失败由 Unity 断言报告并结束当前 case。
 *
 * ## Constraints and lifecycle
 * 仅用于测试图构造；调用方须先建立 function 和 instruction，并保证 token 加一可表示。指令和 token 池仍由全局 function 持有。
 *
 * ## References
 * diamond、compare_exception_cleanup、test_ordinary_double_drop_still_fails 及两个 paused-owner 测试在构造图时使用。
 */

/**
 * @fn void call_effects(TZrExecIrInstructionId id, TZrUInt32 token, TZrUInt32 memoryToken)
 *
 * ## Summary
 * 为测试中的调用指令设置 effect 边，并同时追加 managed-heap 与 native-FFI 两个 memory 域的输入、输出 token。
 *
 * ## Parameters
 * @param[in] id 已发射调用指令在全局 function 中的一基编号。
 * @param[in] token effect 输入 token；输出取其后继值。
 * @param[in] memoryToken 两个 memory 域共享的输入序号；对应输出使用后继序号。
 *
 * ## Returns
 * 无返回值；更新 function 内的调用元数据，token 池追加失败由 Unity 断言报告。
 *
 * ## Constraints and lifecycle
 * 仅在测试构图阶段调用；id 必须指向已发射指令，两个序号加一不得溢出。memory token 池由 function 持有。
 *
 * ## References
 * compare_exception_cleanup 和 test_loop_reinitializes_owner_across_repeated_cleanup_resume 为 invoke/call 节点建立效果边。
 */

/**
 * @fn void ownership_effects(TZrExecIrInstructionId id, TZrUInt32 effectToken, TZrUInt32 memoryToken)
 *
 * ## Summary
 * 给循环测试中的指令补充 effect 边和 ownership 域 memory 输入/输出 token。
 *
 * ## Parameters
 * @param[in] id 当前 function 中有效的一基指令编号。
 * @param[in] effectToken effect 输入 token，输出使用后继值。
 * @param[in] memoryToken ownership memory 输入序号，输出使用后继序号。
 *
 * ## Returns
 * 无返回值；元数据写入 function，token 追加失败由 Unity 断言报告。
 *
 * ## Constraints and lifecycle
 * 只服务测试图构造；调用方先发射指令并保证两个输入 token 均可安全加一。追加的 token 与 instruction 一同由 function 持有。
 *
 * ## References
 * test_loop_reinitializes_owner_across_repeated_cleanup_resume 为循环 DROP 建立 ownership effect。
 */

/**
 * @fn TZrExecIrInstructionId terminal(TZrExecIrBlockId block, EZrExecIrOpcode opcode, TZrExecIrValueId operand)
 *
 * ## Summary
 * 在测试块中发射终结指令，并把块当前 successor range 与 terminator 标识同步到该指令。
 *
 * ## Parameters
 * @param[in] block 已建立的有效一基 block 编号。
 * @param[in] opcode 测试图期望的终结 opcode。
 * @param[in] operand 按 emit 约定传入的值编号；无操作数终结可传零。
 *
 * ## Returns
 * @return 返回新指令的一基 ID；该 ID 只在当前 function 指令池生命周期内有效。
 *
 * ## Constraints and lifecycle
 * 只用于测试图构造。调用者须先建立 block 及其 successor range，且发射结果须与该块的 CFG 终结语义匹配；索引和池归全局 function 所有。
 *
 * ## References
 * diamond、compare_exception_cleanup、test_ordinary_double_drop_still_fails 与循环及 alias 测试使用它结束基本块。
 */

/**
 * @fn void elaborate(void)
 *
 * ## Summary
 * 完成当前测试图的前驱关系、cleanup DROP lowering、全量验证和 state-map 构建，并把失败诊断格式化为 Unity 断言上下文。
 *
 * ## Returns
 * 无返回值；所有阶段通过断言表示成功，断言失败会终止当前测试 case。
 *
 * ## Constraints and lifecycle
 * 需要由调用方先完整构造可展开的全局 function。函数修改并替换该全局对象；其正常返回后测试可读取新 generation 和 state map。测试断言提前退出时，之后的局部资源释放语句不会执行。
 *
 * ## References
 * compare_exception_cleanup 与 test_loop_reinitializes_owner_across_repeated_cleanup_resume 调用该构造阶段；内部顺序见 predecessors、ZrParser_ExecIr_ElaborateCleanupDrops、ZrCore_ExecIr_VerifyFunction 和 ZrParser_ExecIr_BuildStateMaps。
 */

/**
 * @fn SCleanupFixture diamond(TZrBool moveOnTrue)
 *
 * ## Summary
 * 建立 true/false 分支汇入 cleanup block 的小型 owner 图，供条件 DROP 的 lowering、暂停恢复和 materialization 测试复用。
 *
 * ## Parameters
 * @param[in] moveOnTrue 为真时 true arm 将 owner 移到另一个值并先 drop；为假时 true arm 以 COPY 定义 owner。该参数选择图的指令形状，不选择 oracle 后续实际走哪条分支。
 *
 * ## Returns
 * @return 返回当前图的 owner、poll 与 cleanup-drop 标识副本；这些编号不拥有底层池且只在本次全局 function 生命周期中有效。
 *
 * ## Constraints and lifecycle
 * 重置全局 fixture 后须重新调用。owner payload 的定义与 cleanup join 的条件义务由 CFG 共同决定；call 成员仅随 fixture 零初始化，本 helper 没有显式设置它，此翻译单元也没有读取它。
 *
 * ## References
 * compare_diamond 以 moveOnTrue 构造矩阵；多项 materialization、verify 和 clone 测试直接调用本 helper。
 */

/**
 * @fn SZrExecIrOracleCheckpoint stop_at(TZrExecIrInstructionId id, EZrExecIrStateMapPhase phase)
 *
 * ## Summary
 * 从当前全局 function 的 state map 复制指定指令/阶段的 source 与 resume 标识，构造 oracle 可借用的暂停点值。
 *
 * ## Parameters
 * @param[in] id 已存在 map entry 对应的一基指令编号。
 * @param[in] phase 该指令的有效状态 map phase。
 *
 * ## Returns
 * @return 返回 checkpoint 值副本，不转移 map entry 的所有权；返回值可在源 map 被释放前后独立保存其标量字段。
 *
 * ## Constraints and lifecycle
 * 调用要求当前 function 已建好匹配的 state map；checkpoint 的指令与 phase 必须可由 checkpoint 查询到，否则 fixture helper 的断言失败。
 *
 * ## References
 * compare_diamond、resume、invoke、loop 及 paused-owner 测试用该值设置 OracleInput.stopAt。
 */

/**
 * @fn void same_trace(const SZrExecIrOracleExecutionResult *expected, const SZrExecIrOracleExecutionResult *actual)
 *
 * ## Summary
 * 比较不中断运行与暂停后恢复运行的可观察结果，检查返回状态、返回值、已执行指令数和逐项 oracle event。
 *
 * ## Parameters
 * @param[in] expected 已完成的参考结果。
 * @param[in] actual 已完成的待比较结果；必须已返回且未暂停。
 *
 * ## Returns
 * 无返回值；差异由 Unity 断言标记为当前 case 失败。
 *
 * ## Constraints and lifecycle
 * 两个结果及其 event/operand 数组须在整个比较期间有效；比较读取 event kind、instruction/source ID、operand 数及 operand union 存储，不获取或释放结果所有权。断言提前失败会跳过调用者后续的 Free。
 *
 * ## References
 * compare_diamond、compare_exception_cleanup 和 test_loop_reinitializes_owner_across_repeated_cleanup_resume 比较 oracle 执行轨迹。
 */

/**
 * @fn void compare_diamond(TZrBool takeTrue, TZrBool moved)
 *
 * ## Summary
 * 对 diamond 图比较一次不中断执行与五个暂停 phase 后的恢复执行，并核对 owner 状态、drop 次数、完成结果及重复 resume 的拒绝行为。
 *
 * ## Parameters
 * @param[in] takeTrue oracle 初始条件值，选择实际执行的 CFG arm。
 * @param[in] moved 传给 diamond 的图形选项；同时决定测试输入值数量及期望的 move/drop 情形。
 *
 * ## Returns
 * 无返回值；构图、运行及结果比较均由 Unity 断言报告。
 *
 * ## Constraints and lifecycle
 * 需要同步运行 Oracle 且全局 function/state map 已由 helper 正确构造。局部结果在正常断言路径显式 Free；断言早退时该释放不会执行。该 helper 测的是测试解释器路径，不是独立生产 pipeline 调用。
 *
 * ## References
 * 四个 diamond 测试函数分别传入 true/false arm 与 move 选项；执行点由 stop_at 和 state map 提供。
 */

/**
 * @fn SZrExecIrResumeRequest request_at(SCleanupFixture fixture, SZrExecIrMaterializedState *target, TZrUInt32 *owners)
 *
 * ## Summary
 * 用当前 function 与 poll 的 BEFORE_EFFECT map entry 填充一份 materialization resume request。
 *
 * ## Parameters
 * @param[in] fixture 当前 function 的标识副本，提供 poll ID 和 owner ID。
 * @param[in] target 可选的调用方物化状态输出对象地址；本 helper 仅保存该地址，供后续 ZrCore_ExecIr_MaterializeState 调用作为可写输出借用，不获取对象所有权。
 * @param[in] owners 可选的 owner-state 借用数组；非空时 request.ownerStateCount 取当前 function.valueCount，为空时 count 为零。
 *
 * ## Returns
 * @return 返回 request 值副本；其中 function、map、target、ownerStates 都是借用指针，须由发起调用者维持有效。
 *
 * ## Constraints and lifecycle
 * 当前全局 function、state map 和 fixture 必须互相匹配；调用者须保证目标及 owner 数组覆盖声明长度，并让所借存储持续到同步 MaterializeState 调用返回。
 *
 * ## References
 * test_runtime_witness_filters_roots_and_preserves_obligation 与 test_missing_or_impossible_witness_preserves_target 建立请求。
 */

/**
 * @fn TZrBool invoke(void *data, const SZrExecIrInstruction *instruction, const SZrExecIrOracleValue *operands, TZrUInt32 count, SZrExecIrOracleValue *result, TZrBool *threw)
 *
 * ## Summary
 * 为异常清理测试实现同步 oracle invoke callback，接受一个 signed 参数、记录调用次数，并按测试状态返回值或抛出标记。
 *
 * ## Parameters
 * @param[in,out] data 指向调用方持有的 SCleanupEffects；在回调期间借用。
 * @param[in] instruction oracle 当前 invoke 指令，本 helper 不读取其字段。
 * @param[in] operands 回调参数数组；仅当 count 为一且首项为 signed 时接受。
 * @param[in] count 参数个数。
 * @param[out] result 未抛出时写入 signed 值 52；抛出时不写入。
 * @param[out] threw 写入 SCleanupEffects.throwing 的选择结果。
 *
 * ## Returns
 * @return 返回 ZR_TRUE 表示已处理唯一 signed 参数，返回 ZR_FALSE 表示参数形状不匹配。
 *
 * ## Constraints and lifecycle
 * operands、result、threw 与 userData 均由同步 oracle 调用方提供并借用；本函数不保留指针或分配资源。测试在调用期间保持 data 指向的栈对象有效。
 *
 * ## References
 * compare_exception_cleanup 将本函数注册为 OracleInput.invoke 并分别比较正常返回与抛出路径。
 */

/**
 * @fn void compare_exception_cleanup(TZrBool throwing)
 *
 * ## Summary
 * 构造含 invoke 正常/异常 successor 与 cleanup block 的图，比较同步执行和在 invoke/poll 处暂停后恢复的事件轨迹，检查调用只发生一次及异常路径不错误 drop。
 *
 * ## Parameters
 * @param[in] throwing 控制 callback 在首次调用时返回或标记抛出；恢复前反转该字段以检测是否重复调用。
 *
 * ## Returns
 * 无返回值；任何 CFG、callback 次数、drop 次数或轨迹差异由 Unity 断言报告。
 *
 * ## Constraints and lifecycle
 * reference/actual callback 状态位于当前同步测试栈帧；oracle userData 在各次调用期间借用。正常断言路径释放两个 execution result；Unity 断言早退可能跳过后续 Free。
 *
 * ## References
 * test_successful_invoke_initializes_cleanup_once 与 test_throwing_invoke_keeps_cleanup_uninitialized 分别测试不抛出和抛出。
 */

/**
 * @fn TZrBool loop_call(void *data, const SZrExecIrInstruction *instruction, const SZrExecIrOracleValue *operands, TZrUInt32 count, SZrExecIrOracleValue *result)
 *
 * ## Summary
 * 为循环 cleanup resume 测试生成受控的迭代返回值，并累计同步 callback 调用和循环次数。
 *
 * ## Parameters
 * @param[in,out] data 指向调用方的 SCleanupEffects 状态，在同步回调期间借用。
 * @param[in] instruction 当前测试图中的调用指令；sourceId 102 选择递增的 signed 结果，其余调用产生是否继续循环的 bool。
 * @param[in] operands 本测试 callback 不读取其内容。
 * @param[in] count 必须为零，否则返回未处理。
 * @param[out] result count 为零时写入 signed iteration 值或循环条件 bool。
 *
 * ## Returns
 * @return count 为零时返回 ZR_TRUE 并写入结果；有参数时返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * data、instruction 和 result 仅在同步 callback 期间借用；迭代计数及 sourceId 规则属于本测试图协议，不是生产调用 ABI。
 *
 * ## References
 * test_loop_reinitializes_owner_across_repeated_cleanup_resume 把本函数注册到 OracleInput.call。
 */

/**
 * @fn void test_defined_arm_cleanup_survives_every_phase(void)
 *
 * ## Summary
 * 运行 diamond 的定义 arm，前三个 checkpoint 检查 owner 状态，五个 checkpoint 均比较恢复轨迹；未移动的 owner 最终仅 drop 一次。
 *
 * ## Returns
 * 断言全部通过则正常返回；任一 Unity 断言失败会跳出当前测试用例并进入 tearDown，随后 runner 继续执行，不返回业务状态。
 *
 * ## Constraints and lifecycle
 * 依赖 Debug 断言及 diamond 测试 builder；局部 oracle 结果只在断言能继续到显式 Free 时释放。
 *
 * ## References
 * compare_diamond(ZR_TRUE, ZR_FALSE)；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:831。
 */

/**
 * @fn void test_uninitialized_arm_skips_cleanup_at_every_phase(void)
 *
 * ## Summary
 * 让 diamond 运行到没有 owner payload 的 arm，检查五个暂停 phase 在恢复后与整段运行轨迹相同且不执行 cleanup drop。
 *
 * ## Returns
 * 断言全部通过则正常返回；任一 Unity 断言失败会跳出当前测试用例并进入 tearDown，随后 runner 继续执行。
 *
 * ## Constraints and lifecycle
 * 断言依赖 Debug 构建；局部 execution result 在正常路径显式释放，断言早退不保证执行这些释放。
 *
 * ## References
 * compare_diamond(ZR_FALSE, ZR_FALSE)；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:832。
 */

/**
 * @fn void test_moved_arm_does_not_drop_twice(void)
 *
 * ## Summary
 * 选择 moveOnTrue 图并运行 true arm，原 owner 经 MOVE 后处于 MOVED 状态，目的值已单独 DROP；验证汇合 cleanup block 不再 drop 原 owner。
 *
 * ## Returns
 * 断言通过则正常返回；失败由 Unity longjmp 跳出当前用例，再执行 tearDown 与后续用例。
 *
 * ## Constraints and lifecycle
 * 只证明本测试 ExecIR/oracle 图中的 drop 事件与暂停恢复行为；结果资源仅在断言通过后续路径释放。
 *
 * ## References
 * compare_diamond(ZR_TRUE, ZR_TRUE)；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:833。
 */

/**
 * @fn void test_unmoved_arm_drops_original_owner(void)
 *
 * ## Summary
 * 对含 true-arm move 分支的图选择另一 arm，确认原 owner 沿未移动路径由汇合 cleanup block drop 一次。
 *
 * ## Returns
 * 断言通过则正常返回；失败由 Unity longjmp 跳出当前用例，再执行 tearDown 与后续用例。
 *
 * ## Constraints and lifecycle
 * 本例检查测试图的事件和 pause/resume 语义，不证明真实外部资源析构或生产 pipeline 调用。
 *
 * ## References
 * compare_diamond(ZR_FALSE, ZR_TRUE)；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:834。
 */

/**
 * @fn void test_runtime_witness_filters_roots_and_preserves_obligation(void)
 *
 * ## Summary
 * 使用运行期 owner witness 两次 materialize 同一 poll：INITIALIZED 状态把 owner 作为 root，UNINITIALIZED 状态不把它作为 root，但仍保留 owner-state 记录。
 *
 * ## Returns
 * 断言通过则正常返回；失败由 Unity longjmp 跳出当前用例，再执行 tearDown 与后续用例。
 *
 * ## Constraints and lifecycle
 * request 借用局部 owner 数组和 target；target 在正常路径显式释放。断言早退可能跳过 MaterializedStateFree。
 *
 * ## References
 * request_at 建立 BEFORE_EFFECT 请求；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:835。
 */

/**
 * @fn void test_missing_or_impossible_witness_preserves_target(void)
 *
 * ## Summary
 * 检查 DROPPED witness 与缺少 witness 时 materialize 失败且不改写既有 target；随后把 target 指针置为 NULL，仍无 witness 时无输出检查返回成功，不构造材料化状态，也不要求补充该 conditional owner 的具体 witness。
 *
 * ## Returns
 * 断言通过则正常返回；失败由 Unity longjmp 跳出当前用例，再执行 tearDown 与后续用例。
 *
 * ## Constraints and lifecycle
 * 对 target 的失败保证仅由该测试比较整个结构体字节确认；target 的分配资源在正常路径释放，Unity 早退时后续释放可能不执行。
 *
 * ## References
 * request_at 构造被测请求；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:836。
 */

/**
 * @fn void test_forged_paused_owner_does_not_consume_cursor(void)
 *
 * ## Summary
 * 暂停后把 owner witness 伪造为 DROPPED，确认 resume 拒绝且结果不变、未 drop；恢复成 UNINITIALIZED 后再确认成功恢复且仍无 drop。
 *
 * ## Returns
 * 断言通过则正常返回；失败由 Unity longjmp 跳出当前用例，再执行 tearDown 与后续用例。
 *
 * ## Constraints and lifecycle
 * 只覆盖给定 paused-result 的 witness/cursor 事务；结果由测试拥有并在正常路径释放，断言早退可能跳过释放。
 *
 * ## References
 * stop_at 提供 poll checkpoint；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:837。
 */

/**
 * @fn void test_ordinary_use_is_not_relaxed_by_conditional_cleanup(void)
 *
 * ## Summary
 * 把 merge 返回值改为读取条件定义的 owner，确认普通 use 仍不因 cleanup 的条件 drop 语义而放宽，BuildStateMaps 失败时旧 map 指针保持不变。
 *
 * ## Returns
 * 断言通过则正常返回；失败由 Unity longjmp 跳出当前用例，再执行 tearDown 与后续用例。
 *
 * ## Constraints and lifecycle
 * diamond 初始化时已执行 cleanup lowering；修改当前 function 的 operand 后不再调用 lowering，本断言只检查 BuildStateMaps 的拒绝结果及 map 身份。
 *
 * ## References
 * diamond 提供 owner/drop 图；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:838。
 */

/**
 * @fn void test_resume_allocation_failures_preserve_owner_and_cursor(void)
 *
 * ## Summary
 * 对 Oracle resume 的分配 ordinal 注入失败，检查失败时结果字节、MOVED 状态和已产生 drop 事件保持不变，随后允许一次成功恢复。
 *
 * ## Returns
 * 断言通过则正常返回；失败由 Unity longjmp 跳出当前用例，再执行 tearDown 与后续用例。
 *
 * ## Constraints and lifecycle
 * 故障注入范围是 oracle resume fault wrapper，不覆盖 cleanup lower 的 clone、owner analysis 或 state-map 分配。结果仅在正常断言路径释放；早退可能跳过释放。
 *
 * ## References
 * ssa_oracle_resume_fail_allocation 与 ssa_oracle_resume_allocation_failed 提供注入控制；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:839。
 */

/**
 * @fn void test_successful_invoke_initializes_cleanup_once(void)
 *
 * ## Summary
 * 在 invoke 正常返回时比较不间断运行和两处暂停后恢复，确认 callback 不被重放且已定义 owner 由 cleanup drop 一次。
 *
 * ## Returns
 * 断言通过则正常返回；失败由 Unity longjmp 跳出当前用例，再执行 tearDown 与后续用例。
 *
 * ## Constraints and lifecycle
 * callback userData 只借用同步调用栈上的状态；execution result 在正常路径释放，断言早退不保证执行后续释放。
 *
 * ## References
 * compare_exception_cleanup(ZR_FALSE)；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:840。
 */

/**
 * @fn void test_throwing_invoke_keeps_cleanup_uninitialized(void)
 *
 * ## Summary
 * 在 invoke 抛出路径比较暂停/恢复与整段运行，确认异常 arm 汇入 cleanup 时不对未初始化 owner 产生 drop，且 callback 不被重放。
 *
 * ## Returns
 * 断言通过则正常返回；失败由 Unity longjmp 跳出当前用例，再执行 tearDown 与后续用例。
 *
 * ## Constraints and lifecycle
 * callback userData 在同步 oracle 调用期间借用；局部 execution result 仅在正常断言路径释放。
 *
 * ## References
 * compare_exception_cleanup(ZR_TRUE)；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:841。
 */

/**
 * @fn void test_loop_reinitializes_owner_across_repeated_cleanup_resume(void)
 *
 * ## Summary
 * 执行一个迭代 257 次的 cleanup loop，在每次 cleanup-complete phase 暂停并恢复，检查 owner 可在循环中重新初始化、累计 drop 与 uninterrupted trace 相同。
 *
 * ## Returns
 * 断言通过则正常返回；失败由 Unity longjmp 跳出当前用例，再执行 tearDown 与后续用例。
 *
 * ## Constraints and lifecycle
 * loop_call 的计数和返回值遵从该测试图的 sourceId 约定；结果、callback state 均由同步测试栈持有，结果只在正常断言路径释放。
 *
 * ## References
 * loop_call 注册到 OracleInput.call；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:842。
 */

/**
 * @fn void test_guarded_drop_requires_cleanup_block(void)
 *
 * ## Summary
 * 移除包含 DROP_IF_INITIALIZED 的 block cleanup 标记，确认 Core verifier 拒绝并定位该指令的 instructionId/sourceId。
 *
 * ## Returns
 * 断言通过则正常返回；失败由 Unity longjmp 跳出当前用例，再执行 tearDown 与后续用例。
 *
 * ## Constraints and lifecycle
 * diamond 初始化时已执行 parser cleanup lowering；移除 cleanup 标记后只调用 Core verifier 检查拒绝，不再调用 lowering，也不执行 Oracle。
 *
 * ## References
 * ZrCore_ExecIr_VerifyFunction 执行拒绝检查；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:843。
 */

/**
 * @fn void test_guarded_drop_rejects_nonowner_operand(void)
 *
 * ## Summary
 * 把 guarded DROP 的 operand ownership 类别改为 GC，确认 verifier 拒绝该非 owner 操作数。
 *
 * ## Returns
 * 断言通过则正常返回；失败由 Unity longjmp 跳出当前用例，再执行 tearDown 与后续用例。
 *
 * ## Constraints and lifecycle
 * 使用未 sealed 的 diamond fixture 且只检查 verifier 返回值和 instruction ID，不测试执行行为。
 *
 * ## References
 * ZrCore_ExecIr_VerifyFunction 检查修改后的图；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:844。
 */

/**
 * @fn void test_guarded_drop_rejects_missing_value_id(void)
 *
 * ## Summary
 * 将 guarded DROP 的 operand ID 改为超出 value pool 的编号，确认 verifier 在逐指令语义检查前以 INVALID_VALUE 拒绝并保留 functionToken。
 *
 * ## Returns
 * 断言通过则正常返回；失败由 Unity longjmp 跳出当前用例，再执行 tearDown 与后续用例。
 *
 * ## Constraints and lifecycle
 * 测试故意构造格式错误的 operand pool；不据此声称 guarded DROP 的全部 verifier 错误均落在同一诊断阶段。
 *
 * ## References
 * 原位注释说明 pool 级早拒绝；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:845。
 */

/**
 * @fn void test_ordinary_double_drop_still_fails(void)
 *
 * ## Summary
 * 在普通 block 连续 drop 同一唯一 owner 两次，确认 verifier 允许该静态图后，oracle 在第二条 DROP 处以 INVALID_VALUE 失败。
 *
 * ## Returns
 * 断言通过则正常返回；失败由 Unity longjmp 跳出当前用例，再执行 tearDown 与后续用例。
 *
 * ## Constraints and lifecycle
 * 该运行期语义使用本测试的初始值与 effect token；result 在正常路径释放，断言早退可能跳过释放。
 *
 * ## References
 * terminal 与 effects 建立测试指令；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:846。
 */

/**
 * @fn void test_clone_preserves_independent_cleanup_metadata(void)
 *
 * ## Summary
 * 克隆已建 state map 的 cleanup function，确认 map 与 ownerStatePool 地址独立但内容相等，并在释放原函数后仍能通过 clone 暂停、恢复。
 *
 * ## Returns
 * 断言通过则正常返回；失败由 Unity longjmp 跳出当前用例，再执行 tearDown 与后续用例。
 *
 * ## Constraints and lifecycle
 * clone 由测试初始化并显式释放；原 function 在中途释放后重置。局部 oracle result 在正常路径释放，断言早退不保证执行后续资源回收。
 *
 * ## References
 * ZrCore_ExecIr_CloneFunction 与 stop_at 构成测试路径；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:847。
 */

/**
 * @fn void test_elaboration_rejects_sealed_function_without_replacement(void)
 *
 * ## Summary
 * 在 function sealed 且 DROP 已恢复为普通 opcode 时调用 parser cleanup lower，检查拒绝后 instruction pool、map、generation 和 opcode 均保留原值。
 *
 * ## Returns
 * 断言通过则正常返回；失败由 Unity longjmp 跳出当前用例，再执行 tearDown 与后续用例。
 *
 * ## Constraints and lifecycle
 * 此用例验证 sealed 早期拒绝及对象身份；原 map 指针只在该 function 未被替换时有效。
 *
 * ## References
 * ZrParser_ExecIr_ElaborateCleanupDrops 在调用处被直接执行；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:848。
 */

/**
 * @fn void test_elaboration_owner_analysis_oom_is_transactional(void)
 *
 * ## Summary
 * 逐 ordinal 注入 owner-analysis 分配失败，验证 cleanup lowering 的失败事务性及最终成功。
 *
 * ## Returns
 * 断言全部通过才正常返回；Unity 记录失败并 longjmp 跳出当前用例，随后执行 tearDown。
 *
 * ## Constraints and lifecycle
 * diamond(ZR_FALSE) 已完成 cleanup lowering 并生成 guarded-DROP map。测试还原普通 DROP 后，
 * 释放旧 map 的内部池与外壳并置 NULL，再以 STRUCTURE|EFFECT 验证待 lowering 的输入。
 * saved 在该前置处理后取得。故障 wrapper 仅拦截 owner-state allocation；本用例不代表
 * clone、候选 OwnerAnalysis 或 map 分配的完整 OOM 覆盖。每次失败仍检查 OOM diagnostic、
 * 原 function/普通 DROP 不变以及零 outstanding allocation，最终必须得到 guarded DROP。
 *
 * ## Regression evidence
 * 历史 R2 基线保留了与普通 DROP 不一致的旧 map，输入可在预校验中被拒绝而未触发注入；
 * 当时 cleanup 为 23/24，injected 断言失败。历史日志仅记录断言，不记录 entry 或 ordinal。
 * 本修复移除失效元数据，保留原事务性断言；最终通过情况以固定输入的集成收据为准。
 *
 * ## References
 * 预校验见 ZrCore_ExecIr_VerifyFunction 与 ValidateStateMap；状态映射的两级释放遵循
 * ZrCore_ExecIr_FreeFunction。历史基线 parser-conditional-cleanup-preedit-native-r2-summary.json
 * SHA-256 fd0b2a1e766535900c34ce813e811056680176d3c88f591f2dca895c27cb3c72。
 * 本任务红绿证据位于 E:/cargo-targets/zr_vm/reports/review-bugfix-20261016/。
 */

/**
 * @fn void test_forged_initialized_witness_requires_valid_payload(void)
 *
 * ## Summary
 * 构造 diamond(ZR_FALSE) 图并用 false 初始条件选择未定义臂，暂停后伪造 INITIALIZED witness，确认缺少 payload 时 resume 拒绝且结果不变；改回 UNINITIALIZED 后恢复成功且 drop 数为零。
 *
 * ## Returns
 * 断言通过则正常返回；失败由 Unity longjmp 跳出当前用例，再执行 tearDown 与后续用例。
 *
 * ## Constraints and lifecycle
 * 该用例只覆盖 false/uninitialized arm 上“无 payload 的伪造 initialized”拒绝路径；不覆盖 true arm 有效 payload 的接受，也不检查特定 diagnostic.code。result 在正常路径释放，断言早退可能跳过释放。
 *
 * ## References
 * pause 点由 stop_at 提供；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:850。
 */

/**
 * @fn void test_shared_owner_uses_same_conditional_cleanup_contract(void)
 *
 * ## Summary
 * 将 diamond owner 的 ownership 类别改为 SHARED、重建 map，并通过 poll 暂停/恢复确认 cleanup 仍产生一次 drop。
 *
 * ## Returns
 * 断言通过则正常返回；失败由 Unity longjmp 跳出当前用例，再执行 tearDown 与后续用例。
 *
 * ## Constraints and lifecycle
 * 本例显式重建 state map 后运行；result 在正常路径释放，断言早退不保证执行释放。
 *
 * ## References
 * ZrParser_ExecIr_BuildStateMaps 构建测试图 map；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:851。
 */

/**
 * @fn void test_oracle_rejects_owner_that_was_never_defined(void)
 *
 * ## Summary
 * 对 cleanup block 中未曾定义的 UNIQUE owner 运行 guarded DROP，确认 oracle 以 INVALID_VALUE 拒绝并报告 instructionId 与 sourceId。
 *
 * ## Returns
 * 断言通过则正常返回；失败由 Unity longjmp 跳出当前用例，再执行 tearDown 与后续用例。
 *
 * ## Constraints and lifecycle
 * 本用例直接构图并 Build predecessor，不调用 parser cleanup lowering；oracle result 在正常路径释放。
 *
 * ## References
 * effects、terminal 和 predecessors 建立待执行图；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:852。
 */

/**
 * @fn void test_paused_owner_buffer_alias_is_rejected_before_commit(void)
 *
 * ## Summary
 * 把暂停结果的 ownerStates 指针分别指向 result.values 的起始和内部位置，确认 Resume 在提交前拒绝别名、保留结果字节，并在恢复正确指针后成功继续。
 *
 * ## Returns
 * 断言通过则正常返回；失败由 Unity longjmp 跳出当前用例，再执行 tearDown 与后续用例。
 *
 * ## Constraints and lifecycle
 * 该函数在每次 assertion 前恢复真实 ownerStates allocation，以免测试清理误释放别名地址；只覆盖本用例构造的两处别名和 result 事务性。
 *
 * ## References
 * ResumeOracleEx 的检查在循环内调用；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:853。
 */

/**
 * @fn void test_paused_owner_buffer_cannot_own_function_map_storage(void)
 *
 * ## Summary
 * 把暂停结果的 ownerStates 指针分别指向 function map ownerStatePool 的前两个位置，确认 Resume 拒绝让调用者结果接管 function 所有的池存储，随后使用真实结果缓冲恢复。
 *
 * ## Returns
 * 断言通过则正常返回；失败由 Unity longjmp 跳出当前用例，再执行 tearDown 与后续用例。
 *
 * ## Constraints and lifecycle
 * 若被测代码意外接受别名，测试先从 map 摘除该池并释放真实 result buffer，再显式失败以避免双重释放；正常失败路径恢复 ownerStates 后继续验证一次成功 resume。
 *
 * ## References
 * state map pool 与 Oracle result 在循环内交叉别名；Unity main 注册于 tests/parser/test_ssa_conditional_cleanup.c:854。
 */
