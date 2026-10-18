#include <string.h>

#include "unity.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/execution_budget.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/stack.h"
#ifdef ZR_BUDGET_SOURCE_TESTS
#include "zr_vm_core/string.h"
#include "zr_vm_parser.h"
#endif

void setUp(void) {}
void tearDown(void) {}

enum { NOP_COUNT = 600, INSTRUCTION_COUNT = NOP_COUNT + 2 };

typedef struct Observation {
    TZrUInt64 instructions;
    SZrExecutionCancelToken *cancelAt17;
    SZrFunction *rootFunction;
    TZrUInt64 firstNestedInstruction;
    TZrBool countMismatch;
} Observation;

static TZrDebugSignal observe(SZrState *state, SZrFunction *function,
        const TZrInstruction *pc, TZrUInt32 offset, TZrUInt32 line, TZrPtr data) {
    Observation *observation = data;
    (void)pc; (void)offset; (void)line;
    ++observation->instructions;
    if (state->executionBudget != ZR_NULL &&
        state->executionBudget->executedInstructions != observation->instructions)
        observation->countMismatch = ZR_TRUE;
    if (observation->rootFunction != ZR_NULL && function != observation->rootFunction &&
        observation->firstNestedInstruction == 0u)
        observation->firstNestedInstruction = observation->instructions;
    if (observation->instructions == 17u && observation->cancelAt17 != ZR_NULL) {
        ZrCore_ExecutionCancelToken_Cancel(observation->cancelAt17);
    }
    return ZR_DEBUG_SIGNAL_NONE;
}

static SZrFunction *new_function(SZrState *state) {
    SZrFunction *function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrCore_Function_AcquireHostRoot(state, function));
    function->instructionsList = ZrCore_Memory_RawMallocWithType(state->global,
            INSTRUCTION_COUNT * sizeof(TZrInstruction), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->constantValueList = ZrCore_Memory_RawMallocWithType(state->global,
            sizeof(SZrTypeValue), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    memset(function->instructionsList, 0, INSTRUCTION_COUNT * sizeof(TZrInstruction));
    function->instructionsLength = INSTRUCTION_COUNT;
    function->constantValueLength = 1u;
    function->stackSize = 1u;
    for (unsigned pc = 0u; pc < NOP_COUNT; ++pc) {
        function->instructionsList[pc].instruction.operationCode = ZR_INSTRUCTION_ENUM(NOP);
    }
    function->instructionsList[NOP_COUNT].instruction.operationCode = ZR_INSTRUCTION_ENUM(GET_CONSTANT);
    function->instructionsList[NOP_COUNT + 1u].instruction.operationCode = ZR_INSTRUCTION_ENUM(FUNCTION_RETURN);
    function->instructionsList[NOP_COUNT + 1u].instruction.operandExtra = 1u;
    ZrCore_Value_InitAsInt(state, function->constantValueList, 42);
    return function;
}

static void test_instruction_limits_are_exact_across_resource_quanta(void) {
    static const TZrUInt64 limits[] = {0, 1, 255, 256, 257, 511, 512, 601, 602, 603};
    for (unsigned observed = 0; observed < 2u; ++observed) {
        for (unsigned index = 0; index < ZR_ARRAY_COUNT(limits); ++index) {
            SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
            SZrExecutionBudget budget = {0};
            Observation observation = {0};
            SZrTypeValue result;
            TZrBool success;
            TZrUInt64 admitted = limits[index] < INSTRUCTION_COUNT ? limits[index] : INSTRUCTION_COUNT;
            TEST_ASSERT_NOT_NULL(state);
            SZrFunction *function = new_function(state);
            budget.hasInstructionLimit = ZR_TRUE;
            budget.maxInstructions = limits[index];
            state->executionBudget = &budget;
            if (observed) ZrCore_Debug_SetTraceObserver(state, observe, &observation);
            success = ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &result);
            state->executionBudget = ZR_NULL;
            ZrCore_Debug_SetTraceObserver(state, ZR_NULL, ZR_NULL);
            ZrTests_Runtime_State_Destroy(state);
            TEST_ASSERT_EQUAL_UINT64(admitted, budget.executedInstructions);
            if (observed) TEST_ASSERT_EQUAL_UINT64(admitted, observation.instructions);
            TEST_ASSERT_EQUAL_INT(limits[index] >= INSTRUCTION_COUNT, success);
            TEST_ASSERT_EQUAL_INT(success ? ZR_EXECUTION_TERMINATION_NONE :
                    ZR_EXECUTION_TERMINATION_INSTRUCTION_LIMIT, budget.termination);
            if (success) TEST_ASSERT_EQUAL_INT64(42, result.value.nativeObject.nativeInt64);
        }
    }
}

static void test_cancellation_is_observed_within_one_quantum(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrExecutionBudget budget = {0};
    Observation observation = {0};
    SZrTypeValue result;
    TEST_ASSERT_NOT_NULL(state);
    SZrFunction *function = new_function(state);
    observation.cancelAt17 = ZrCore_ExecutionCancelToken_New();
    TEST_ASSERT_NOT_NULL(observation.cancelAt17);
    budget.cancelToken = observation.cancelAt17;
    state->executionBudget = &budget;
    ZrCore_Debug_SetTraceObserver(state, observe, &observation);
    TZrBool success = ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &result);
    state->executionBudget = ZR_NULL;
    ZrCore_Debug_SetTraceObserver(state, ZR_NULL, ZR_NULL);
    ZrCore_ExecutionCancelToken_Free(observation.cancelAt17);
    ZrTests_Runtime_State_Destroy(state);
    TEST_ASSERT_FALSE(success);
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_TERMINATION_CANCELLED, budget.termination);
    TEST_ASSERT_EQUAL_UINT64(observation.instructions, budget.executedInstructions);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(17u, budget.executedInstructions);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(17u + 256u, budget.executedInstructions);
}

static void test_expired_deadline_admits_no_instructions(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrExecutionBudget budget = {0};
    SZrTypeValue result;
    TEST_ASSERT_NOT_NULL(state);
    SZrFunction *function = new_function(state);
    budget.hasDeadline = ZR_TRUE;
    budget.deadlineMicros = 0u;
    state->executionBudget = &budget;
    TZrBool success = ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &result);
    state->executionBudget = ZR_NULL;
    ZrTests_Runtime_State_Destroy(state);
    TEST_ASSERT_FALSE(success);
    TEST_ASSERT_EQUAL_UINT64(0u, budget.executedInstructions);
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_TERMINATION_DEADLINE, budget.termination);
}

/* The callback reenters the real VM without a nested test harness TryRun.
 * A terminated nested VM returns to this native frame before the caller's
 * mandatory resource poll unwinds the outer VM. */
static SZrFunction *nestedFunction;
static TZrBool nestedCompleted;

static TZrInt64 reenter_vm(SZrState *state) {
    SZrFunctionStackAnchor anchor;
    TZrStackValuePointer base = state->stackTop.valuePointer;
    base = ZrCore_Function_CheckStackAndAnchor(state, nestedFunction->stackSize + 1u,
                                              base, base, &anchor);
    ZrCore_Closure_PushToStack(state, nestedFunction, ZR_NULL, base, state->stackTop.valuePointer);
    ++state->stackTop.valuePointer;
    TZrStackValuePointer result = ZrCore_Function_CallAndRestoreAnchor(state, &anchor, 1u);
    nestedCompleted = (TZrBool)(state->threadStatus == ZR_THREAD_STATUS_FINE && result != ZR_NULL &&
            result->value.type == ZR_VALUE_TYPE_INT64 && result->value.value.nativeObject.nativeInt64 == 42);
    return 0;
}

static SZrFunction *new_reentrant_function(SZrState *state) {
    nestedFunction = new_function(state);
    SZrFunction *outer = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(outer);
    TEST_ASSERT_TRUE(ZrCore_Function_AcquireHostRoot(state, outer));
    outer->instructionsLength = 4u;
    outer->stackSize = 2u;
    outer->constantValueLength = 2u;
    outer->instructionsList = ZrCore_Memory_RawMallocWithType(state->global,
            4u * sizeof(TZrInstruction), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    outer->constantValueList = ZrCore_Memory_RawMallocWithType(state->global,
            2u * sizeof(SZrTypeValue), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(outer->instructionsList);
    TEST_ASSERT_NOT_NULL(outer->constantValueList);
    memset(outer->instructionsList, 0, 4u * sizeof(TZrInstruction));
    for (unsigned i = 0u; i < 2u; ++i) ZrCore_Value_ResetAsNull(outer->constantValueList + i);
    SZrClosureNative *native = ZrCore_ClosureNative_New(state, 0u);
    TEST_ASSERT_NOT_NULL(native);
    native->nativeFunction = reenter_vm;
    ZrCore_Value_InitAsRawObject(state, outer->constantValueList, ZR_CAST_RAW_OBJECT_AS_SUPER(native));
    ZrCore_Value_InitAsInt(state, outer->constantValueList + 1u, 42);
    outer->instructionsList[0].instruction.operationCode = ZR_INSTRUCTION_ENUM(GET_CONSTANT);
    outer->instructionsList[1].instruction.operationCode = ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL);
    outer->instructionsList[2].instruction.operationCode = ZR_INSTRUCTION_ENUM(GET_CONSTANT);
    outer->instructionsList[2].instruction.operand.operand1[0] = 1u;
    outer->instructionsList[3].instruction.operationCode = ZR_INSTRUCTION_ENUM(FUNCTION_RETURN);
    outer->instructionsList[3].instruction.operandExtra = 1u;
    return outer;
}

static void check_control_flow_limits(SZrState *state, SZrFunction *function, TZrBool hasNestedVm) {
    SZrExecutionBudget baseline = {0};
    Observation complete = {0};
    SZrTypeValue result;
    complete.rootFunction = function;
    state->executionBudget = &baseline;
    ZrCore_Debug_SetTraceObserver(state, observe, &complete);
    TZrBool success = ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &result);
    state->executionBudget = ZR_NULL;
    ZrCore_Debug_SetTraceObserver(state, ZR_NULL, ZR_NULL);
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT64(42, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT64(complete.instructions, baseline.executedInstructions);
    TEST_ASSERT_FALSE(complete.countMismatch);
    if (nestedFunction != ZR_NULL) TEST_ASSERT_TRUE(nestedCompleted);
    if (hasNestedVm) TEST_ASSERT_GREATER_THAN_UINT64(0u, complete.firstNestedInstruction);
    const TZrUInt64 limits[] = {0u, 1u, 2u, 3u, 17u, 255u, 256u, 257u, 511u, 512u,
            complete.firstNestedInstruction + 1u, complete.instructions - 1u,
            complete.instructions, complete.instructions + 1u};
    for (unsigned traced = 0u; traced < 2u; ++traced) {
        for (unsigned i = 0u; i < ZR_ARRAY_COUNT(limits); ++i) {
            SZrExecutionBudget budget = {0};
            Observation observation = {0};
            ZrCore_State_ResetThread(state, ZR_THREAD_STATUS_FINE);
            budget.hasInstructionLimit = ZR_TRUE;
            budget.maxInstructions = limits[i];
            nestedCompleted = ZR_FALSE;
            state->executionBudget = &budget;
            if (traced) ZrCore_Debug_SetTraceObserver(state, observe, &observation);
            success = ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &result);
            state->executionBudget = ZR_NULL;
            ZrCore_Debug_SetTraceObserver(state, ZR_NULL, ZR_NULL);
            TZrUInt64 admitted = limits[i] < complete.instructions ? limits[i] : complete.instructions;
            TEST_ASSERT_EQUAL_UINT64(admitted, budget.executedInstructions);
            TEST_ASSERT_EQUAL_INT(limits[i] >= complete.instructions, success);
            TEST_ASSERT_EQUAL_INT(success ? ZR_EXECUTION_TERMINATION_NONE :
                    ZR_EXECUTION_TERMINATION_INSTRUCTION_LIMIT, budget.termination);
            if (traced) {
                TEST_ASSERT_EQUAL_UINT64(admitted, observation.instructions);
                TEST_ASSERT_FALSE(observation.countMismatch);
            }
            if (success) {
                TEST_ASSERT_EQUAL_INT64(42, result.value.nativeObject.nativeInt64);
                if (nestedFunction != ZR_NULL) TEST_ASSERT_TRUE(nestedCompleted);
            }
        }
    }
    ZrTests_Runtime_State_Destroy(state);
}

static void test_native_reentry_shares_exact_instruction_count(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    SZrFunction *outer = new_reentrant_function(state);
    check_control_flow_limits(state, outer, ZR_TRUE);
    nestedFunction = ZR_NULL;
}

#ifdef ZR_BUDGET_SOURCE_TESTS
static void check_source_control_flow(const char *source, TZrBool hasNestedVm) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    SZrFunction *function = ZrParser_Source_Compile(state, source, strlen(source),
            ZrCore_String_CreateFromNative(state, "budget_control_flow.zr"));
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrCore_Function_AcquireHostRoot(state, function));
    check_control_flow_limits(state, function, hasNestedVm);
}

static void test_vm_call_return_and_limit_inside_callee_are_exact(void) {
    check_source_control_flow("fn inc(n: int): int { return n + 1; } return inc(40) + inc(0);", ZR_TRUE);
}

static void test_caught_throw_keeps_exact_instruction_and_trace_counts(void) {
    check_source_control_flow("var result = 0; try { throw 7; } catch (error) { result = 42; } return result;", ZR_FALSE);
}
#endif

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_instruction_limits_are_exact_across_resource_quanta);
    RUN_TEST(test_cancellation_is_observed_within_one_quantum);
    RUN_TEST(test_expired_deadline_admits_no_instructions);
    RUN_TEST(test_native_reentry_shares_exact_instruction_count);
#ifdef ZR_BUDGET_SOURCE_TESTS
    RUN_TEST(test_vm_call_return_and_limit_inside_callee_are_exact);
    RUN_TEST(test_caught_throw_keeps_exact_instruction_and_trace_counts);
#endif
    return UNITY_END();
}
