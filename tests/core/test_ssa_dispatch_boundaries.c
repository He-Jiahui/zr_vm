#include "unity.h"

#include "harness/runtime_support.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/execution_context.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/profile.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"

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

static void test_dispatch_publishes_the_saved_pc_at_its_256_instruction_poll(void) {
    const TZrSize instructionCount = 257u;
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrCallInfo *callInfo;
    SZrTypeValue callableValue;
    SZrTypeValue *functionBaseValue;
    TZrStackValuePointer functionBase;
    TZrInstruction *instructions;
    TZrSize index;
    TZrMemoryOffset resumeOffset;
    TZrUInt32 previousProgramCounter;
    EZrThreadStatus threadStatus;
    TZrBool callInfoIsActive;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    instructions = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * instructionCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(instructions);
    for (index = 0u; index < instructionCount; ++index) {
        ZrCore_Memory_RawSet(&instructions[index], 0, sizeof(instructions[index]));
        instructions[index].instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(NOP);
    }
    function->instructionsList = instructions;
    function->instructionsLength = instructionCount;
    function->constantValueList = ZR_NULL;
    function->constantValueLength = 0u;
    function->stackSize = 0u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;

    ZrCore_Value_ResetAsNull(&callableValue);
    ZrCore_Value_InitAsRawObject(state,
                                 &callableValue,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    callableValue.type = ZR_VALUE_TYPE_FUNCTION;
    callableValue.isGarbageCollectable = ZR_TRUE;
    callableValue.isNative = ZR_FALSE;

    functionBase = ZrCore_Function_CheckStackAndGc(
            state,
            (TZrSize)(1u + function->stackSize),
            state->stackTop.valuePointer);
    TEST_ASSERT_NOT_NULL(functionBase);
    functionBaseValue = ZrCore_Stack_GetValue(functionBase);
    TEST_ASSERT_NOT_NULL(functionBaseValue);
    ZrCore_Value_Copy(state, functionBaseValue, &callableValue);
    state->stackTop.valuePointer = functionBase + 1 + function->stackSize;

    callInfo = ZrCore_CallInfo_Extend(state);
    TEST_ASSERT_NOT_NULL(callInfo);
    ZrCore_CallInfo_EntryNativeInit(
            state, callInfo, state->stackBase, state->stackTop, state->callInfoList);
    callInfo->functionBase.valuePointer = functionBase;
    callInfo->functionTop.valuePointer = functionBase + 1 + function->stackSize;
    callInfo->context.context.programCounter = function->instructionsList;
    callInfo->callStatus = ZR_CALL_STATUS_CREATE_FRAME;
    callInfo->expectedReturnCount = 1u;
    state->callInfoList = callInfo;
    state->threadStatus = ZR_THREAD_STATUS_FINE;

    ZrCore_Execute(state, callInfo);

    threadStatus = state->threadStatus;
    callInfoIsActive = (TZrBool)(callInfo == state->callInfoList);
    resumeOffset = callInfo->context.context.programCounter - function->instructionsList;
    previousProgramCounter = state->previousProgramCounter;
    ZrTests_Runtime_State_Destroy(state);

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, threadStatus);
    TEST_ASSERT_TRUE(callInfoIsActive);
    TEST_ASSERT_TRUE(resumeOffset > 0);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)resumeOffset, previousProgramCounter);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_publish_rejects_null_and_invalid_pc);
    RUN_TEST(test_reload_rejects_missing_frame);
    RUN_TEST(test_publish_and_reload_rebuilds_frame_state);
    RUN_TEST(test_reload_rebuilds_context_from_replaced_frame_roots);
    RUN_TEST(test_dispatch_publishes_the_saved_pc_at_its_256_instruction_poll);
    return UNITY_END();
}
