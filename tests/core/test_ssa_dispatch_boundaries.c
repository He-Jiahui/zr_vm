#if !defined(ZR_PLATFORM_WIN) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "unity.h"

#include "harness/runtime_support.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/execution_budget.h"
#include "zr_vm_core/execution_context.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/profile.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
typedef HANDLE ZrDispatchTestThread;
typedef volatile LONG ZrDispatchTestAtomic;
#else
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
typedef pthread_t ZrDispatchTestThread;
typedef _Atomic int ZrDispatchTestAtomic;
#endif

#define ZR_DISPATCH_TEST_WAIT_LIMIT_MILLISECONDS 3000u

typedef struct ZrDispatchPauseWorkerContext {
    SZrState state;
    ZrDispatchTestAtomic ready;
    ZrDispatchTestAtomic stop;
    SZrExecutionCancelToken *cancelToken;
    TZrBool collector;
    TZrBool mutatorEntered;
    TZrBool timedOut;
    TZrBool sawExpectedRunningPair;
    TZrBool sawDispatcherParked;
    TZrBool observerPollReturned;
    TZrUInt32 observedParkedMutatorCount;
    TZrUInt32 observedRunningMutatorCount;
    TZrUInt64 observedSafepointEpoch;
    TZrBool fullCollectionReturned;
} ZrDispatchPauseWorkerContext;

static int dispatch_test_atomic_load(ZrDispatchTestAtomic *value) {
#if defined(ZR_PLATFORM_WIN)
    return (int)InterlockedCompareExchange(value, 0, 0);
#else
    return atomic_load_explicit(value, memory_order_acquire);
#endif
}

static void dispatch_test_atomic_store(ZrDispatchTestAtomic *value, int next) {
#if defined(ZR_PLATFORM_WIN)
    InterlockedExchange(value, (LONG)next);
#else
    atomic_store_explicit(value, next, memory_order_release);
#endif
}

static void dispatch_test_atomic_init(ZrDispatchTestAtomic *value) {
#if defined(ZR_PLATFORM_WIN)
    InterlockedExchange(value, 0);
#else
    atomic_init(value, 0);
#endif
}

static void dispatch_test_sleep_one_millisecond(void) {
#if defined(ZR_PLATFORM_WIN)
    Sleep(1u);
#else
    struct timespec duration;
    duration.tv_sec = 0;
    duration.tv_nsec = 1000000L;
    nanosleep(&duration, ZR_NULL);
#endif
}

static void dispatch_pause_worker_run(ZrDispatchPauseWorkerContext *context) {
    TZrUInt32 attempt;

    if (context->collector) {
        dispatch_test_atomic_store(&context->ready, 1);
        for (attempt = 0u;
             attempt < ZR_DISPATCH_TEST_WAIT_LIMIT_MILLISECONDS;
             ++attempt) {
            SZrGcDomainMutatorSnapshot snapshot;

            if (dispatch_test_atomic_load(&context->stop)) {
                return;
            }
            ZrCore_GcDomain_GetMutatorSnapshot(&context->state, &snapshot);
            if (!snapshot.pauseRequested && snapshot.runningMutatorCount == 2u) {
                context->sawExpectedRunningPair = ZR_TRUE;
                ZrCore_GarbageCollector_GcFull(&context->state, ZR_TRUE);
                context->fullCollectionReturned = ZR_TRUE;
                ZrCore_ExecutionCancelToken_Cancel(context->cancelToken);
                return;
            }
            dispatch_test_sleep_one_millisecond();
        }
        context->timedOut = ZR_TRUE;
        ZrCore_ExecutionCancelToken_Cancel(context->cancelToken);
        return;
    }

    context->mutatorEntered = ZrCore_GcDomain_MutatorEnter(&context->state);
    dispatch_test_atomic_store(&context->ready, 1);
    if (!context->mutatorEntered) {
        return;
    }
    for (attempt = 0u;
         attempt < ZR_DISPATCH_TEST_WAIT_LIMIT_MILLISECONDS;
         ++attempt) {
        SZrGcDomainMutatorSnapshot snapshot;

        if (dispatch_test_atomic_load(&context->stop)) {
            break;
        }
        ZrCore_GcDomain_GetMutatorSnapshot(&context->state, &snapshot);
        if (snapshot.pauseRequested &&
            snapshot.parkedMutatorCount == 1u &&
            snapshot.runningMutatorCount == 1u) {
            context->sawDispatcherParked = ZR_TRUE;
            context->observedParkedMutatorCount = snapshot.parkedMutatorCount;
            context->observedRunningMutatorCount = snapshot.runningMutatorCount;
            context->observedSafepointEpoch = snapshot.safepointEpoch;
            context->observerPollReturned =
                    ZrCore_GcDomain_MutatorPoll(&context->state);
            break;
        }
        dispatch_test_sleep_one_millisecond();
    }
    if (!context->sawDispatcherParked &&
        !dispatch_test_atomic_load(&context->stop)) {
        context->timedOut = ZR_TRUE;
    }
    ZrCore_GcDomain_MutatorLeave(&context->state);
}

#if defined(ZR_PLATFORM_WIN)
static DWORD WINAPI dispatch_pause_worker_entry(LPVOID argument) {
    dispatch_pause_worker_run((ZrDispatchPauseWorkerContext *)argument);
    return 0;
}
#else
static void *dispatch_pause_worker_entry(void *argument) {
    dispatch_pause_worker_run((ZrDispatchPauseWorkerContext *)argument);
    return ZR_NULL;
}
#endif

static TZrBool dispatch_pause_worker_state_init(
        ZrDispatchPauseWorkerContext *context,
        SZrState *ownerState,
        SZrExecutionCancelToken *cancelToken,
        TZrBool collector) {
    context->cancelToken = cancelToken;
    context->collector = collector;
    ZrCore_RawObject_Construct(
            &context->state.super, ZR_RAW_OBJECT_TYPE_THREAD);
    context->state.global = ownerState->global;
    return ZrCore_GcDomain_MutatorAttach(ownerState, &context->state);
}

static TZrBool dispatch_pause_worker_start(
        ZrDispatchTestThread *thread,
        ZrDispatchPauseWorkerContext *context) {
#if defined(ZR_PLATFORM_WIN)
    *thread = CreateThread(
            ZR_NULL, 0u, dispatch_pause_worker_entry, context, 0u, ZR_NULL);
    return (TZrBool)(*thread != ZR_NULL);
#else
    return (TZrBool)(pthread_create(
                             thread,
                             ZR_NULL,
                             dispatch_pause_worker_entry,
                             context) == 0);
#endif
}

static TZrBool dispatch_pause_worker_wait_ready(
        ZrDispatchPauseWorkerContext *context) {
    TZrUInt32 attempt;

    for (attempt = 0u;
         attempt < ZR_DISPATCH_TEST_WAIT_LIMIT_MILLISECONDS;
         ++attempt) {
        if (dispatch_test_atomic_load(&context->ready)) {
            return ZR_TRUE;
        }
        dispatch_test_sleep_one_millisecond();
    }
    return (TZrBool)dispatch_test_atomic_load(&context->ready);
}

static TZrBool dispatch_pause_worker_join(ZrDispatchTestThread thread) {
#if defined(ZR_PLATFORM_WIN)
    DWORD waitResult = WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return (TZrBool)(waitResult == WAIT_OBJECT_0);
#else
    return (TZrBool)(pthread_join(thread, ZR_NULL) == 0);
#endif
}

void setUp(void) {}
void tearDown(void) {}

typedef struct ZrDispatchThrowTraceCapture {
    SZrFunction *expectedFunction;
    TZrUInt32 expectedInstructionOffset;
    TZrUInt32 observedCallbackCount;
    TZrUInt32 observedInstructionOffset;
    TZrUInt32 observedSourceLine;
    TZrBool observedThrow;
    TZrBool callInfoPointedAtObservedInstruction;
    TZrBool previousProgramCounterMatched;
} ZrDispatchThrowTraceCapture;

static TZrDebugSignal dispatch_test_capture_throw_instruction(
        SZrState *state,
        SZrFunction *function,
        const TZrInstruction *programCounter,
        TZrUInt32 instructionOffset,
        TZrUInt32 sourceLine,
        TZrPtr userData) {
    ZrDispatchThrowTraceCapture *capture = (ZrDispatchThrowTraceCapture *)userData;

    if (capture != ZR_NULL && function == capture->expectedFunction) {
        ++capture->observedCallbackCount;
        if (instructionOffset == capture->expectedInstructionOffset) {
            SZrCallInfo *callInfo = state != ZR_NULL ? state->callInfoList : ZR_NULL;

            capture->observedThrow = (TZrBool)(
                    programCounter == function->instructionsList + instructionOffset &&
                    ZR_INSTRUCTION_OPCODE(function->instructionsList[instructionOffset]) ==
                            ZR_INSTRUCTION_ENUM(THROW));
            capture->observedInstructionOffset = instructionOffset;
            capture->observedSourceLine = sourceLine;
            capture->callInfoPointedAtObservedInstruction = (TZrBool)(
                    callInfo != ZR_NULL &&
                    callInfo->context.context.programCounter == programCounter);
            capture->previousProgramCounterMatched = (TZrBool)(
                    state != ZR_NULL && state->previousProgramCounter == instructionOffset);
        }
    }
    return ZR_DEBUG_SIGNAL_NONE;
}

static void dispatch_test_execute_call_info(SZrState *state, TZrPtr arguments) {
    ZrCore_Execute(state, *(SZrCallInfo **)arguments);
}

static void test_observer_only_debug_reports_throw_instruction_pc_and_line(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    TZrInstruction *instructions = ZR_NULL;
    SZrFunctionExecutionLocationInfo *sourceLocations = ZR_NULL;
    SZrTypeValue callableValue;
    TZrStackValuePointer functionBase = ZR_NULL;
    SZrTypeValue *functionBaseValue = ZR_NULL;
    SZrCallInfo *callInfo = ZR_NULL;
    ZrDispatchThrowTraceCapture capture;
    EZrThreadStatus status = ZR_THREAD_STATUS_FINE;
    TZrBool callInfoSavedThrowPc = ZR_FALSE;
    TZrMemoryOffset savedPreviousProgramCounter = (TZrMemoryOffset)-1;

    ZrCore_Memory_RawSet(&capture, 0, sizeof(capture));
    if (state != ZR_NULL) {
        function = ZrCore_Function_New(state);
    }
    if (function != ZR_NULL) {
        instructions = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(TZrInstruction) * 2u,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        sourceLocations = (SZrFunctionExecutionLocationInfo *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionExecutionLocationInfo) * 2u,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        function->instructionsList = instructions;
        function->instructionsLength = 2u;
        function->executionLocationInfoList = sourceLocations;
        function->executionLocationInfoLength = 2u;
    }
    if (function != ZR_NULL && instructions != ZR_NULL && sourceLocations != ZR_NULL) {
        ZrCore_Memory_RawSet(instructions, 0, sizeof(TZrInstruction) * 2u);
        ZrCore_Memory_RawSet(
                sourceLocations,
                0,
                sizeof(SZrFunctionExecutionLocationInfo) * 2u);
        instructions[0].instruction.operationCode =
                (TZrUInt16)ZR_INSTRUCTION_ENUM(NOP);
        instructions[1].instruction.operationCode =
                (TZrUInt16)ZR_INSTRUCTION_ENUM(THROW);
        function->constantValueList = ZR_NULL;
        function->constantValueLength = 0u;
        function->stackSize = 1u;
        function->parameterCount = 0u;
        function->hasVariableArguments = ZR_FALSE;
        function->closureValueLength = 0u;
        sourceLocations[0].currentInstructionOffset = 0u;
        sourceLocations[0].lineInSource = 11u;
        sourceLocations[1].currentInstructionOffset = 1u;
        sourceLocations[1].lineInSource = 47u;

        ZrCore_Value_ResetAsNull(&callableValue);
        ZrCore_Value_InitAsRawObject(
                state, &callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
        callableValue.type = ZR_VALUE_TYPE_FUNCTION;
        callableValue.isGarbageCollectable = ZR_TRUE;
        callableValue.isNative = ZR_FALSE;
        functionBase = ZrCore_Function_CheckStackAndGc(
                state,
                (TZrSize)(1u + function->stackSize),
                state->stackTop.valuePointer);
    }
    if (functionBase != ZR_NULL) {
        functionBaseValue = ZrCore_Stack_GetValue(functionBase);
    }
    if (functionBaseValue != ZR_NULL) {
        ZrCore_Value_Copy(state, functionBaseValue, &callableValue);
        ZrCore_Value_InitAsInt(
                state, ZrCore_Stack_GetValue(functionBase + 1), 123);
        state->stackTop.valuePointer = functionBase + 1 + function->stackSize;
        callInfo = ZrCore_CallInfo_Extend(state);
    }
    if (callInfo != ZR_NULL) {
        ZrCore_CallInfo_EntryNativeInit(
                state, callInfo, state->stackBase, state->stackTop, state->callInfoList);
        callInfo->functionBase.valuePointer = functionBase;
        callInfo->functionTop.valuePointer = functionBase + 1 + function->stackSize;
        callInfo->context.context.programCounter = function->instructionsList;
        callInfo->callStatus = ZR_CALL_STATUS_CREATE_FRAME;
        callInfo->expectedReturnCount = 1u;
        state->callInfoList = callInfo;
        state->threadStatus = ZR_THREAD_STATUS_FINE;
        capture.expectedFunction = function;
        capture.expectedInstructionOffset = 1u;

        /* An observer alone must disable the no-debug fast path. */
        ZrCore_Debug_SetTraceObserver(
                state, dispatch_test_capture_throw_instruction, &capture);
        status = ZrCore_Exception_TryRun(
                state, dispatch_test_execute_call_info, &callInfo);
        callInfoSavedThrowPc = (TZrBool)(
                callInfo->context.context.programCounter == function->instructionsList + 1u);
        savedPreviousProgramCounter = state->previousProgramCounter;
        ZrCore_Debug_SetTraceObserver(state, ZR_NULL, ZR_NULL);
    }

    if (state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(state);
    }

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(instructions);
    TEST_ASSERT_NOT_NULL(callInfo);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR, status);
    TEST_ASSERT_EQUAL_UINT32(2u, capture.observedCallbackCount);
    TEST_ASSERT_TRUE(capture.observedThrow);
    TEST_ASSERT_EQUAL_UINT32(1u, capture.observedInstructionOffset);
    TEST_ASSERT_EQUAL_UINT32(47u, capture.observedSourceLine);
    TEST_ASSERT_TRUE(capture.callInfoPointedAtObservedInstruction);
    TEST_ASSERT_TRUE(capture.previousProgramCounterMatched);
    TEST_ASSERT_TRUE(callInfoSavedThrowPc);
    TEST_ASSERT_EQUAL_INT64(1, savedPreviousProgramCounter);
}

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

static void test_dispatch_resumes_after_full_gc_parks_running_mutator(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDispatchPauseWorkerContext observer;
    ZrDispatchPauseWorkerContext collector;
    ZrDispatchTestThread observerThread;
    ZrDispatchTestThread collectorThread;
    SZrExecutionCancelToken *cancelToken = ZR_NULL;
    SZrExecutionBudget budget;
    SZrGarbageCollectorStatsSnapshot statsBefore;
    SZrGarbageCollectorStatsSnapshot statsAfter;
    SZrFunction *function = ZR_NULL;
    TZrInstruction *instructions = ZR_NULL;
    SZrTypeValue callableValue;
    SZrTypeValue *functionBaseValue = ZR_NULL;
    TZrStackValuePointer functionBase = ZR_NULL;
    SZrCallInfo *callInfo = ZR_NULL;
    TZrBool observerAttached = ZR_FALSE;
    TZrBool collectorAttached = ZR_FALSE;
    TZrBool observerStarted = ZR_FALSE;
    TZrBool collectorStarted = ZR_FALSE;
    TZrBool observerReady = ZR_FALSE;
    TZrBool collectorReady = ZR_FALSE;
    TZrBool executionReturned = ZR_FALSE;
    TZrBool observerJoined = ZR_FALSE;
    TZrBool collectorJoined = ZR_FALSE;
    TZrBool statsCaptured = ZR_FALSE;
    TZrBool budgetWasBound = ZR_FALSE;
    TZrBool fullCollectionCompleted = ZR_FALSE;
    TZrBool stateCreated;
    TZrBool cancelTokenCreated = ZR_FALSE;
    EZrThreadStatus finalThreadStatus = ZR_THREAD_STATUS_FINE;
    EZrExecutionTermination finalBudgetTermination = ZR_EXECUTION_TERMINATION_NONE;
    TZrUInt32 finalPreviousProgramCounter = (TZrUInt32)-1;

    ZrCore_Memory_RawSet(&observer, 0, sizeof(observer));
    ZrCore_Memory_RawSet(&collector, 0, sizeof(collector));
    dispatch_test_atomic_init(&observer.ready);
    dispatch_test_atomic_init(&observer.stop);
    dispatch_test_atomic_init(&collector.ready);
    dispatch_test_atomic_init(&collector.stop);
    ZrCore_Memory_RawSet(&budget, 0, sizeof(budget));
    ZrCore_Memory_RawSet(&statsBefore, 0, sizeof(statsBefore));
    ZrCore_Memory_RawSet(&statsAfter, 0, sizeof(statsAfter));
    stateCreated = (TZrBool)(state != ZR_NULL);
    if (state != ZR_NULL) {
        cancelToken = ZrCore_ExecutionCancelToken_New();
        cancelTokenCreated = (TZrBool)(cancelToken != ZR_NULL);
    }
    if (state != ZR_NULL && cancelToken != ZR_NULL) {
        function = ZrCore_Function_New(state);
    }
    if (function != ZR_NULL) {
        instructions = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(TZrInstruction) * 2u,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function != ZR_NULL && instructions != ZR_NULL) {
        ZrCore_Memory_RawSet(instructions, 0, sizeof(TZrInstruction) * 2u);
        instructions[0].instruction.operationCode =
                (TZrUInt16)ZR_INSTRUCTION_ENUM(NOP);
        instructions[1].instruction.operationCode =
                (TZrUInt16)ZR_INSTRUCTION_ENUM(JUMP);
        instructions[1].instruction.operand.operand2[0] = -1;
        function->instructionsList = instructions;
        function->instructionsLength = 2u;
        function->constantValueList = ZR_NULL;
        function->constantValueLength = 0u;
        function->stackSize = 0u;
        function->parameterCount = 0u;
        function->hasVariableArguments = ZR_FALSE;
        function->closureValueLength = 0u;

        ZrCore_Value_ResetAsNull(&callableValue);
        ZrCore_Value_InitAsRawObject(
                state,
                &callableValue,
                ZR_CAST_RAW_OBJECT_AS_SUPER(function));
        callableValue.type = ZR_VALUE_TYPE_FUNCTION;
        callableValue.isGarbageCollectable = ZR_TRUE;
        callableValue.isNative = ZR_FALSE;
        functionBase = ZrCore_Function_CheckStackAndGc(
                state,
                (TZrSize)(1u + function->stackSize),
                state->stackTop.valuePointer);
    }
    if (functionBase != ZR_NULL) {
        functionBaseValue = ZrCore_Stack_GetValue(functionBase);
    }
    if (functionBaseValue != ZR_NULL) {
        ZrCore_Value_Copy(state, functionBaseValue, &callableValue);
        state->stackTop.valuePointer = functionBase + 1 + function->stackSize;
        callInfo = ZrCore_CallInfo_Extend(state);
    }
    if (callInfo != ZR_NULL) {
        ZrCore_CallInfo_EntryNativeInit(
                state, callInfo, state->stackBase, state->stackTop, state->callInfoList);
        callInfo->functionBase.valuePointer = functionBase;
        callInfo->functionTop.valuePointer = functionBase + 1 + function->stackSize;
        callInfo->context.context.programCounter = function->instructionsList;
        callInfo->callStatus = ZR_CALL_STATUS_CREATE_FRAME;
        callInfo->expectedReturnCount = 1u;
        state->callInfoList = callInfo;
        state->threadStatus = ZR_THREAD_STATUS_FINE;
        state->previousProgramCounter = (TZrUInt32)-1;

        budget.cancelToken = cancelToken;
        state->executionBudget = &budget;
        budgetWasBound = ZR_TRUE;
        ZrCore_GarbageCollector_GetStatsSnapshot(
                state->global, &statsBefore);
        statsCaptured = ZR_TRUE;

        observerAttached = dispatch_pause_worker_state_init(
                &observer, state, cancelToken, ZR_FALSE);
        collectorAttached = dispatch_pause_worker_state_init(
                &collector, state, cancelToken, ZR_TRUE);
        if (observerAttached && collectorAttached) {
            observerStarted = dispatch_pause_worker_start(
                    &observerThread, &observer);
            if (observerStarted) {
                collectorStarted = dispatch_pause_worker_start(
                        &collectorThread, &collector);
            }
            if (observerStarted && collectorStarted) {
                observerReady = dispatch_pause_worker_wait_ready(&observer);
                collectorReady = dispatch_pause_worker_wait_ready(&collector);
                if (observerReady && collectorReady) {
                    ZrCore_Execute(state, callInfo);
                    executionReturned = ZR_TRUE;
                }
            }
        }
    }

    /* 任一启动/握手超时都先撤销执行并唤醒 waiters，再回收线程借用的 state。 */
    dispatch_test_atomic_store(&observer.stop, 1);
    dispatch_test_atomic_store(&collector.stop, 1);
    ZrCore_ExecutionCancelToken_Cancel(cancelToken);
    if (state != ZR_NULL) {
        ZrCore_GcDomain_WakeMutators(state);
    }
    if (observerStarted) {
        observerJoined = dispatch_pause_worker_join(observerThread);
    }
    if (collectorStarted) {
        collectorJoined = dispatch_pause_worker_join(collectorThread);
    }
    if (observerAttached) {
        ZrCore_GcDomain_MutatorDetach(&observer.state);
    }
    if (collectorAttached) {
        ZrCore_GcDomain_MutatorDetach(&collector.state);
    }
    if (state != ZR_NULL) {
        if (budgetWasBound) {
            state->executionBudget = ZR_NULL;
            finalThreadStatus = state->threadStatus;
            finalPreviousProgramCounter = state->previousProgramCounter;
            finalBudgetTermination = budget.termination;
        }
        if (statsCaptured) {
            ZrCore_GarbageCollector_GetStatsSnapshot(
                    state->global, &statsAfter);
            fullCollectionCompleted =
                    (TZrBool)(statsAfter.fullCollectionCount >
                                      statsBefore.fullCollectionCount &&
                              statsAfter.lastCollectionKind ==
                                      ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL);
        }
        ZrTests_Runtime_State_Destroy(state);
    }
    ZrCore_ExecutionCancelToken_Free(cancelToken);

    /* 断言延后至所有成功启动的线程已 join、state 已 detach/destroy。 */
    TEST_ASSERT_TRUE(stateCreated);
    TEST_ASSERT_TRUE(cancelTokenCreated);
    TEST_ASSERT_TRUE(observerAttached);
    TEST_ASSERT_TRUE(collectorAttached);
    TEST_ASSERT_TRUE(observerStarted);
    TEST_ASSERT_TRUE(collectorStarted);
    TEST_ASSERT_TRUE(observerReady);
    TEST_ASSERT_TRUE(collectorReady);
    TEST_ASSERT_TRUE(executionReturned);
    TEST_ASSERT_TRUE(observerJoined);
    TEST_ASSERT_TRUE(collectorJoined);
    TEST_ASSERT_TRUE(collector.sawExpectedRunningPair);
    TEST_ASSERT_TRUE(collector.fullCollectionReturned);
    TEST_ASSERT_TRUE(fullCollectionCompleted);
    TEST_ASSERT_TRUE(observer.mutatorEntered);
    TEST_ASSERT_TRUE(observer.sawDispatcherParked);
    TEST_ASSERT_EQUAL_UINT32(1u, observer.observedParkedMutatorCount);
    TEST_ASSERT_EQUAL_UINT32(1u, observer.observedRunningMutatorCount);
    TEST_ASSERT_TRUE(observer.observerPollReturned);
    TEST_ASSERT_TRUE(observer.observedSafepointEpoch > 0u);
    TEST_ASSERT_FALSE(observer.timedOut);
    TEST_ASSERT_FALSE(collector.timedOut);
    TEST_ASSERT_EQUAL_INT(
            ZR_THREAD_STATUS_EXECUTION_TERMINATED, finalThreadStatus);
    TEST_ASSERT_EQUAL_INT(
            ZR_EXECUTION_TERMINATION_CANCELLED, finalBudgetTermination);
    TEST_ASSERT_TRUE(finalPreviousProgramCounter <= 1u);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_observer_only_debug_reports_throw_instruction_pc_and_line);
    RUN_TEST(test_publish_rejects_null_and_invalid_pc);
    RUN_TEST(test_reload_rejects_missing_frame);
    RUN_TEST(test_publish_and_reload_rebuilds_frame_state);
    RUN_TEST(test_reload_rebuilds_context_from_replaced_frame_roots);
    RUN_TEST(test_dispatch_publishes_the_saved_pc_at_its_256_instruction_poll);
    RUN_TEST(test_dispatch_resumes_after_full_gc_parks_running_mutator);
    return UNITY_END();
}
