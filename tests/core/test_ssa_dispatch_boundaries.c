#include "unity.h"

#include "zr_vm_core/execution_context.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/profile.h"

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

static void test_reload_rebuilds_context_from_replaced_frame_roots(void) {
    SZrExecutionContext context;
    SZrState state;
    SZrCallInfo initialCallInfo;
    SZrCallInfo resumedCallInfo;
    SZrFunction initialFunction;
    SZrFunction resumedFunction;
    SZrGlobalState initialGlobal;
    SZrGlobalState resumedGlobal;
    SZrProfileRuntime initialProfile;
    SZrProfileRuntime resumedProfile;
    SZrTypeValueOnStack initialStack[4];
    SZrTypeValueOnStack resumedStack[5];
    TZrInstruction initialInstructions[2];
    TZrInstruction resumedInstructions[3];

    ZrCore_Memory_RawSet(&state, 0, sizeof(state));
    ZrCore_Memory_RawSet(&initialCallInfo, 0, sizeof(initialCallInfo));
    ZrCore_Memory_RawSet(&resumedCallInfo, 0, sizeof(resumedCallInfo));
    ZrCore_Memory_RawSet(&initialFunction, 0, sizeof(initialFunction));
    ZrCore_Memory_RawSet(&resumedFunction, 0, sizeof(resumedFunction));
    ZrCore_Memory_RawSet(&initialGlobal, 0, sizeof(initialGlobal));
    ZrCore_Memory_RawSet(&resumedGlobal, 0, sizeof(resumedGlobal));
    ZrCore_Memory_RawSet(&initialProfile, 0, sizeof(initialProfile));
    ZrCore_Memory_RawSet(&resumedProfile, 0, sizeof(resumedProfile));

    initialFunction.instructionsList = initialInstructions;
    initialFunction.instructionsLength = 2u;
    initialCallInfo.metadataFunction = &initialFunction;
    initialCallInfo.functionBase.valuePointer = &initialStack[0];
    initialCallInfo.functionTop.valuePointer = &initialStack[3];
    initialCallInfo.context.context.programCounter = &initialInstructions[0];
    initialGlobal.profileRuntime = &initialProfile;

    state.callInfoList = &initialCallInfo;
    state.global = &initialGlobal;
    state.stackBase.valuePointer = &initialStack[0];
    state.stackTail.valuePointer = &initialStack[4];
    state.stackTop.valuePointer = &initialStack[2];

    resumedFunction.instructionsList = resumedInstructions;
    resumedFunction.instructionsLength = 3u;
    resumedCallInfo.metadataFunction = &resumedFunction;
    resumedCallInfo.functionBase.valuePointer = &resumedStack[0];
    resumedCallInfo.functionTop.valuePointer = &resumedStack[4];
    resumedCallInfo.context.context.programCounter = &resumedInstructions[2];
    resumedGlobal.profileRuntime = &resumedProfile;

    ZrCore_ExecutionContext_Init(&context);
    TEST_ASSERT_EQUAL(ZR_EXECUTION_BOUNDARY_OK,
                      ZrCore_Execution_PublishBoundary(&context,
                                                       &state,
                                                       &initialCallInfo,
                                                       &initialInstructions[1]));
    TEST_ASSERT_EQUAL_PTR(&initialCallInfo, context.callInfo);
    TEST_ASSERT_EQUAL_PTR(&initialInstructions[1], context.programCounter);
    TEST_ASSERT_EQUAL_PTR(&initialStack[1], context.frameBase);
    TEST_ASSERT_EQUAL_PTR(&initialInstructions[0], context.instructionsBegin);

    /* Simulate a boundary returning with the state roots pointing at a moved frame. */
    state.callInfoList = &resumedCallInfo;
    state.global = &resumedGlobal;
    state.stackBase.valuePointer = &resumedStack[0];
    state.stackTail.valuePointer = &resumedStack[5];
    state.stackTop.valuePointer = &resumedStack[3];
    /* Poison auxiliary caches to ensure reload consults the current state roots. */
    context.domain = (struct SZrGcDomain *)(void *)&initialCallInfo;
    context.profileRuntime = &initialProfile;

    TEST_ASSERT_EQUAL(ZR_EXECUTION_BOUNDARY_OK,
                      ZrCore_Execution_ReloadBoundary(&context, &state));

    TEST_ASSERT_EQUAL_PTR(&state, context.state);
    TEST_ASSERT_EQUAL_PTR(state.callInfoList, context.callInfo);
    TEST_ASSERT_EQUAL_PTR(&resumedFunction, context.function);
    TEST_ASSERT_EQUAL_PTR(resumedCallInfo.context.context.programCounter, context.programCounter);
    TEST_ASSERT_EQUAL_PTR(resumedFunction.instructionsList, context.instructionsBegin);
    TEST_ASSERT_EQUAL_PTR(&resumedInstructions[3], context.instructionsEnd);
    TEST_ASSERT_EQUAL_PTR(&resumedStack[1], context.frameBase);
    TEST_ASSERT_EQUAL_PTR(resumedCallInfo.functionTop.valuePointer, context.frameTop);
    TEST_ASSERT_EQUAL_PTR(state.stackTop.valuePointer, context.stackTop);
    TEST_ASSERT_EQUAL_PTR(state.gcDomain, context.domain);
    TEST_ASSERT_EQUAL_PTR(resumedGlobal.profileRuntime, context.profileRuntime);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_publish_rejects_null_and_invalid_pc);
    RUN_TEST(test_reload_rejects_missing_frame);
    RUN_TEST(test_publish_and_reload_rebuilds_frame_state);
    RUN_TEST(test_reload_rebuilds_context_from_replaced_frame_roots);
    return UNITY_END();
}
