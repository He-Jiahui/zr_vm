#include <string.h>

#include "unity.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/execution_budget.h"
#include "zr_vm_core/memory.h"

void setUp(void) {}
void tearDown(void) {}

enum { NOP_COUNT = 600, INSTRUCTION_COUNT = NOP_COUNT + 2 };

typedef struct Observation {
    TZrUInt64 instructions;
    SZrExecutionCancelToken *cancelAt17;
} Observation;

static TZrDebugSignal observe(SZrState *state, SZrFunction *function,
        const TZrInstruction *pc, TZrUInt32 offset, TZrUInt32 line, TZrPtr data) {
    Observation *observation = data;
    (void)state; (void)function; (void)pc; (void)offset; (void)line;
    if (++observation->instructions == 17u && observation->cancelAt17 != ZR_NULL) {
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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_instruction_limits_are_exact_across_resource_quanta);
    RUN_TEST(test_cancellation_is_observed_within_one_quantum);
    RUN_TEST(test_expired_deadline_admits_no_instructions);
    return UNITY_END();
}
