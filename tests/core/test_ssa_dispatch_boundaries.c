#include "unity.h"

#include "zr_vm_core/execution_context.h"

void setUp(void) {}
void tearDown(void) {}

static void test_publish_rejects_null_and_invalid_pc(void) {
    SZrExecutionContext context;
    ZrCore_ExecutionContext_Init(&context);

    TEST_ASSERT_EQUAL(ZR_EXECUTION_BOUNDARY_INVALID_ARGUMENT,
                      ZrCore_Execution_PublishBoundary(&context, ZR_NULL, ZR_NULL, ZR_NULL));
    TEST_ASSERT_EQUAL_STRING("invalid-program-counter",
                             ZrCore_Execution_BoundaryStatusName(
                                     ZR_EXECUTION_BOUNDARY_INVALID_PROGRAM_COUNTER));
}

static void test_reload_rejects_missing_frame(void) {
    SZrExecutionContext context;
    SZrState state;

    ZrCore_ExecutionContext_Init(&context);
    ZrCore_Memory_RawSet(&state, 0, sizeof(state));
    TEST_ASSERT_EQUAL(ZR_EXECUTION_BOUNDARY_INVALID_FRAME,
                      ZrCore_Execution_ReloadBoundary(&context, &state));
}

static void test_publish_and_reload_rebuilds_frame_state(void) {
    SZrExecutionContext context;
    SZrState state;
    SZrCallInfo callInfo;
    SZrFunction function;
    SZrTypeValueOnStack stack[4];
    TZrInstruction instructions[2];

    ZrCore_Memory_RawSet(&state, 0, sizeof(state));
    ZrCore_Memory_RawSet(&callInfo, 0, sizeof(callInfo));
    ZrCore_Memory_RawSet(&function, 0, sizeof(function));
    function.instructionsList = instructions;
    function.instructionsLength = 2u;
    callInfo.metadataFunction = &function;
    callInfo.functionBase.valuePointer = &stack[0];
    callInfo.functionTop.valuePointer = &stack[3];
    callInfo.context.context.programCounter = &instructions[0];
    state.callInfoList = &callInfo;
    state.stackBase.valuePointer = &stack[0];
    state.stackTail.valuePointer = &stack[4];
    state.stackTop.valuePointer = &stack[2];

    ZrCore_ExecutionContext_Init(&context);
    TEST_ASSERT_EQUAL(ZR_EXECUTION_BOUNDARY_OK,
                      ZrCore_Execution_PublishBoundary(&context, &state, &callInfo, &instructions[1]));
    TEST_ASSERT_EQUAL_PTR(&instructions[1], callInfo.context.context.programCounter);
    TEST_ASSERT_EQUAL_PTR(&stack[1], context.frameBase);
    TEST_ASSERT_EQUAL_PTR(&stack[2], context.stackTop);
    TEST_ASSERT_EQUAL(1, state.previousProgramCounter);

    context.programCounter = ZR_NULL;
    TEST_ASSERT_EQUAL(ZR_EXECUTION_BOUNDARY_OK,
                      ZrCore_Execution_ReloadBoundary(&context, &state));
    TEST_ASSERT_EQUAL_PTR(&instructions[1], context.programCounter);
    TEST_ASSERT_EQUAL_PTR(&stack[1], context.frameBase);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_publish_rejects_null_and_invalid_pc);
    RUN_TEST(test_reload_rejects_missing_frame);
    RUN_TEST(test_publish_and_reload_rebuilds_frame_state);
    return UNITY_END();
}
