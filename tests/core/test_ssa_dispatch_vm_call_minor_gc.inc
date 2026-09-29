/* VM-to-VM call fixture paused by a same-domain minor collector. */
#include <stdint.h>
#include <stdio.h>

#define ZR_DISPATCH_VM_CALL_MINOR_GC_INSTRUCTION_COUNT 4194304u
#define ZR_DISPATCH_VM_CALL_MINOR_GC_PAUSE_ATTEMPTS 1000u

#define ZR_DISPATCH_VM_CALL_MINOR_GC_TIMEOUT_NONE 0u
#define ZR_DISPATCH_VM_CALL_MINOR_GC_TIMEOUT_WAITING_FOR_MUTATOR 1u
#define ZR_DISPATCH_VM_CALL_MINOR_GC_TIMEOUT_STOP_THE_WORLD 2u
#define ZR_DISPATCH_VM_CALL_MINOR_GC_TIMEOUT_CALL_CHAIN 3u

static void dispatch_vm_call_minor_gc_yield(void) {
#if defined(ZR_PLATFORM_WIN)
    Sleep(1u);
#else
    struct timespec duration;
    duration.tv_sec = 0;
    duration.tv_nsec = 100000L;
    nanosleep(&duration, ZR_NULL);
#endif
}

typedef struct ZrDispatchVmCallMinorGcWorkerContext {
    SZrState collectorState;
    SZrState *ownerState;
    SZrFunction *callerFunction;
    SZrFunction *calleeFunction;
    TZrMemoryOffset callerFrameOffset;
    uintptr_t initialStackBase;
    uintptr_t initialCallerFrameBase;
    TZrSize initialStackCapacity;
    ZrDispatchTestAtomic ready;
    ZrDispatchTestAtomic stop;
    TZrBool sawRunningMutator;
    TZrBool timedOut;
    TZrUInt32 timeoutStage;
    TZrUInt32 runningMutatorPollCount;
    TZrUInt32 stopTheWorldBeginAttemptCount;
    TZrUInt32 stopTheWorldBeginTimeoutCount;
    TZrUInt32 callChainMissCount;
    TZrBool stopTheWorldFailureTimedOut;
    TZrBool stopTheWorldFailureBlockedOwner;
    TZrUInt64 stopTheWorldFailureBlockingMutatorId;
    TZrUInt64 stopTheWorldFailureSafepointEpoch;
    TZrUInt32 failureRegisteredMutatorCount;
    TZrUInt32 failureRunningMutatorCount;
    TZrUInt32 failureParkedMutatorCount;
    TZrBool failurePauseRequested;
    TZrBool pauseAcquired;
    TZrBool pauseStayedActiveAcrossCollection;
    TZrBool callChainWasVmToVm;
    TZrBool callerPcWasNextInstruction;
    TZrBool callerFrameMatchedSavedOffset;
    TZrBool stackBaseMovedDuringCall;
    TZrBool callerFrameMovedDuringGrowth;
    TZrBool stackGrewBeforeCollection;
    TZrBool rootWasObjectBeforeCollection;
    TZrBool rootWasObjectAfterCollection;
    TZrBool calleeArgumentSurvived;
    TZrUInt32 calleeResumeOffset;
    TZrUInt32 rootRegionBeforeCollection;
    TZrUInt32 rootStorageBeforeCollection;
    TZrUInt32 rootRegionAfterCollection;
    TZrUInt32 rootStorageAfterCollection;
    uintptr_t rootObjectAfterCollection;
    TZrUInt64 minorCollectionCountBefore;
    TZrUInt64 minorCollectionCountAfter;
    EZrGarbageCollectCollectionKind collectionKindAfterCollection;
    TZrBool collectionStepReturned;
} ZrDispatchVmCallMinorGcWorkerContext;

static void dispatch_vm_call_minor_gc_worker_run(
        ZrDispatchVmCallMinorGcWorkerContext *context) {
    SZrGarbageCollectorStatsSnapshot statsBefore;
    SZrGarbageCollectorStatsSnapshot statsAfter;
    SZrGcDomainMutatorSnapshot snapshot;
    SZrCallInfo *calleeCallInfo;
    SZrCallInfo *callerCallInfo;
    SZrFunction *calleeFunction;
    SZrFunction *callerFunction;
    const TZrInstruction *calleeResumePc;
    SZrTypeValue *calleeArgument;
    SZrTypeValue *callerArgument;
    TZrUInt32 attempt;
    TZrUInt32 pauseAttempt;
    TZrBool outerPauseAcquired;
    TZrBool callChainCaptured;

    dispatch_test_atomic_store(&context->ready, 1);
    for (attempt = 0u;
         attempt < ZR_DISPATCH_TEST_WAIT_LIMIT_MILLISECONDS;
         ++attempt) {
        if (dispatch_test_atomic_load(&context->stop)) {
            return;
        }
        context->runningMutatorPollCount = attempt + 1u;
        ZrCore_GcDomain_GetMutatorSnapshot(&context->collectorState, &snapshot);
        if (snapshot.runningMutatorCount > 0u) {
            context->sawRunningMutator = ZR_TRUE;
            break;
        }
        dispatch_test_sleep_one_millisecond();
    }
    if (!context->sawRunningMutator) {
        context->timedOut = ZR_TRUE;
        context->timeoutStage =
                ZR_DISPATCH_VM_CALL_MINOR_GC_TIMEOUT_WAITING_FOR_MUTATOR;
        return;
    }

    callChainCaptured = ZR_FALSE;
    for (pauseAttempt = 0u;
         pauseAttempt < ZR_DISPATCH_VM_CALL_MINOR_GC_PAUSE_ATTEMPTS;
         ++pauseAttempt) {
        SZrGcDomainPauseDiagnostic pauseDiagnostic;

        context->stopTheWorldBeginAttemptCount = pauseAttempt + 1u;
        outerPauseAcquired = ZrCore_GcDomain_StopTheWorldBegin(
                &context->collectorState,
                ZR_DISPATCH_TEST_WAIT_LIMIT_MILLISECONDS,
                &pauseDiagnostic);
        if (!outerPauseAcquired) {
            ZrCore_GcDomain_GetMutatorSnapshot(
                    &context->collectorState, &snapshot);
            context->timedOut = ZR_TRUE;
            context->stopTheWorldBeginTimeoutCount += 1u;
            context->timeoutStage =
                    ZR_DISPATCH_VM_CALL_MINOR_GC_TIMEOUT_STOP_THE_WORLD;
            context->stopTheWorldFailureTimedOut = pauseDiagnostic.timedOut;
            context->stopTheWorldFailureBlockedOwner = (TZrBool)(
                    pauseDiagnostic.blockingState == context->ownerState);
            context->stopTheWorldFailureBlockingMutatorId =
                    pauseDiagnostic.blockingMutatorId;
            context->stopTheWorldFailureSafepointEpoch =
                    pauseDiagnostic.safepointEpoch;
            context->failureRegisteredMutatorCount =
                    snapshot.registeredMutatorCount;
            context->failureRunningMutatorCount = snapshot.runningMutatorCount;
            context->failureParkedMutatorCount = snapshot.parkedMutatorCount;
            context->failurePauseRequested = snapshot.pauseRequested;
            return;
        }
        context->pauseAcquired = ZR_TRUE;

        /* Inspect the active chain only under STW. An early pause can land
           before FUNCTION_CALL installs the VM callee, so release and retry. */
        calleeCallInfo = context->ownerState->callInfoList;
        callerCallInfo = calleeCallInfo != ZR_NULL ? calleeCallInfo->previous : ZR_NULL;
        calleeFunction = calleeCallInfo != ZR_NULL
                                 ? ZrCore_Closure_GetMetadataFunctionFromCallInfo(
                                           context->ownerState, calleeCallInfo)
                                 : ZR_NULL;
        callerFunction = callerCallInfo != ZR_NULL
                                 ? ZrCore_Closure_GetMetadataFunctionFromCallInfo(
                                           context->ownerState, callerCallInfo)
                                 : ZR_NULL;
        context->callChainWasVmToVm = (TZrBool)(
                calleeCallInfo != ZR_NULL && callerCallInfo != ZR_NULL &&
                calleeFunction == context->calleeFunction &&
                callerFunction == context->callerFunction);
        if (context->callChainWasVmToVm) {
            callChainCaptured = ZR_TRUE;
            break;
        }
        context->callChainMissCount += 1u;
        ZrCore_GcDomain_StopTheWorldEnd(&context->collectorState);
        dispatch_vm_call_minor_gc_yield();
    }
    if (!callChainCaptured) {
        context->timedOut = ZR_TRUE;
        context->timeoutStage =
                ZR_DISPATCH_VM_CALL_MINOR_GC_TIMEOUT_CALL_CHAIN;
        return;
    }

    /* While the VM mutator is parked, its published call chain must show the
       callee poll PC and the caller's instruction following FUNCTION_CALL. */
    {
        calleeResumePc = calleeCallInfo->context.context.programCounter;
        context->calleeResumeOffset =
                (TZrUInt32)(calleeResumePc - context->calleeFunction->instructionsList);
        context->callerPcWasNextInstruction = (TZrBool)(
                callerCallInfo->context.context.programCounter ==
                context->callerFunction->instructionsList + 1u);
        context->callerFrameMatchedSavedOffset = (TZrBool)(
                callerCallInfo->functionBase.valuePointer ==
                ZrCore_Stack_LoadOffsetToPointer(
                        context->ownerState, context->callerFrameOffset));
        context->stackBaseMovedDuringCall = (TZrBool)(
                (uintptr_t)(void *)context->ownerState->stackBase.valuePointer !=
                context->initialStackBase);
        context->callerFrameMovedDuringGrowth = (TZrBool)(
                (uintptr_t)(void *)callerCallInfo->functionBase.valuePointer !=
                context->initialCallerFrameBase);
        context->stackGrewBeforeCollection = (TZrBool)(
                (TZrSize)(context->ownerState->stackTail.valuePointer -
                          context->ownerState->stackBase.valuePointer) >
                context->initialStackCapacity);
        if (calleeCallInfo->functionBase.valuePointer != ZR_NULL &&
            callerCallInfo->functionBase.valuePointer != ZR_NULL) {
            calleeArgument = ZrCore_Stack_GetValue(
                    calleeCallInfo->functionBase.valuePointer + 1u);
            callerArgument = ZrCore_Stack_GetValue(
                    callerCallInfo->functionBase.valuePointer + 2u);
            context->rootWasObjectBeforeCollection = (TZrBool)(
                    calleeArgument != ZR_NULL && callerArgument != ZR_NULL &&
                    calleeArgument->type == ZR_VALUE_TYPE_OBJECT &&
                    callerArgument->type == ZR_VALUE_TYPE_OBJECT &&
                    calleeArgument->isGarbageCollectable &&
                    callerArgument->isGarbageCollectable &&
                    calleeArgument->value.object != ZR_NULL &&
                    calleeArgument->value.object == callerArgument->value.object);
            if (context->rootWasObjectBeforeCollection) {
                context->rootRegionBeforeCollection =
                        calleeArgument->value.object->garbageCollectMark.regionKind;
                context->rootStorageBeforeCollection =
                        calleeArgument->value.object->garbageCollectMark.storageKind;
            }
        }
    }

    ZrCore_GarbageCollector_GetStatsSnapshot(
            context->ownerState->global, &statsBefore);
    context->minorCollectionCountBefore = statsBefore.minorCollectionCount;
    ZrCore_GarbageCollector_GcStep(&context->collectorState);
    context->collectionStepReturned = ZR_TRUE;
    ZrCore_GarbageCollector_GetStatsSnapshot(
            context->ownerState->global, &statsAfter);
    context->minorCollectionCountAfter = statsAfter.minorCollectionCount;
    context->collectionKindAfterCollection = statsAfter.lastCollectionKind;

    /* GcStep nests the collector's pause; the outer pause keeps frame/root
       inspection race-free until all offsets and values are captured. */
    ZrCore_GcDomain_GetMutatorSnapshot(&context->collectorState, &snapshot);
    context->pauseStayedActiveAcrossCollection = (TZrBool)(snapshot.pauseRequested);
    calleeCallInfo = context->ownerState->callInfoList;
    callerCallInfo = calleeCallInfo != ZR_NULL ? calleeCallInfo->previous : ZR_NULL;
    if (calleeCallInfo != ZR_NULL && callerCallInfo != ZR_NULL &&
        ZrCore_Closure_GetMetadataFunctionFromCallInfo(
                context->ownerState, calleeCallInfo) == context->calleeFunction &&
        ZrCore_Closure_GetMetadataFunctionFromCallInfo(
                context->ownerState, callerCallInfo) == context->callerFunction) {
        calleeArgument = ZrCore_Stack_GetValue(
                calleeCallInfo->functionBase.valuePointer + 1u);
        callerArgument = ZrCore_Stack_GetValue(
                callerCallInfo->functionBase.valuePointer + 2u);
        context->callerPcWasNextInstruction = (TZrBool)(
                context->callerPcWasNextInstruction &&
                callerCallInfo->context.context.programCounter ==
                        context->callerFunction->instructionsList + 1u);
        context->callerFrameMatchedSavedOffset = (TZrBool)(
                context->callerFrameMatchedSavedOffset &&
                callerCallInfo->functionBase.valuePointer ==
                        ZrCore_Stack_LoadOffsetToPointer(
                                context->ownerState, context->callerFrameOffset));
        context->rootWasObjectAfterCollection = (TZrBool)(
                calleeArgument != ZR_NULL && callerArgument != ZR_NULL &&
                calleeArgument->type == ZR_VALUE_TYPE_OBJECT &&
                callerArgument->type == ZR_VALUE_TYPE_OBJECT &&
                calleeArgument->isGarbageCollectable &&
                callerArgument->isGarbageCollectable &&
                calleeArgument->value.object != ZR_NULL &&
                calleeArgument->value.object == callerArgument->value.object);
        if (context->rootWasObjectAfterCollection) {
            context->rootObjectAfterCollection =
                    (uintptr_t)(void *)calleeArgument->value.object;
            context->rootRegionAfterCollection =
                    calleeArgument->value.object->garbageCollectMark.regionKind;
            context->rootStorageAfterCollection =
                    calleeArgument->value.object->garbageCollectMark.storageKind;
            context->calleeArgumentSurvived = ZR_TRUE;
        }
    }
    ZrCore_GcDomain_StopTheWorldEnd(&context->collectorState);
}

#if defined(ZR_PLATFORM_WIN)
static DWORD WINAPI dispatch_vm_call_minor_gc_worker_entry(LPVOID argument) {
    dispatch_vm_call_minor_gc_worker_run(
            (ZrDispatchVmCallMinorGcWorkerContext *)argument);
    return 0;
}
#else
static void *dispatch_vm_call_minor_gc_worker_entry(void *argument) {
    dispatch_vm_call_minor_gc_worker_run(
            (ZrDispatchVmCallMinorGcWorkerContext *)argument);
    return ZR_NULL;
}
#endif

static TZrBool dispatch_vm_call_minor_gc_worker_start(
        ZrDispatchTestThread *thread,
        ZrDispatchVmCallMinorGcWorkerContext *context) {
#if defined(ZR_PLATFORM_WIN)
    *thread = CreateThread(
            ZR_NULL,
            0u,
            dispatch_vm_call_minor_gc_worker_entry,
            context,
            0u,
            ZR_NULL);
    return (TZrBool)(*thread != ZR_NULL);
#else
    return (TZrBool)(pthread_create(
                             thread,
                             ZR_NULL,
                             dispatch_vm_call_minor_gc_worker_entry,
                             context) == 0);
#endif
}

static void test_vm_call_stack_growth_and_minor_gc_reload_caller_frame(void) {
    const TZrUInt32 callerInstructionCount = 3u;
    const TZrUInt32 calleeInstructionCount =
            ZR_DISPATCH_VM_CALL_MINOR_GC_INSTRUCTION_COUNT + 1u;
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrGarbageCollector *collector;
    SZrFunction *callerFunction = ZR_NULL;
    SZrFunction *calleeFunction = ZR_NULL;
    TZrInstruction *callerInstructions = ZR_NULL;
    TZrInstruction *calleeInstructions = ZR_NULL;
    SZrObject *rootObject = ZR_NULL;
    TZrStackValuePointer callerFrameBase = ZR_NULL;
    SZrTypeValue callableValue;
    SZrTypeValue *slotValue;
    SZrCallInfo *callerCallInfo = ZR_NULL;
    ZrDispatchVmCallMinorGcWorkerContext worker;
    ZrDispatchTestThread workerThread = {0};
    TZrBool workerAttached = ZR_FALSE;
    TZrBool workerStarted = ZR_FALSE;
    TZrBool workerReady = ZR_FALSE;
    TZrBool executionReturned = ZR_FALSE;
    TZrBool workerJoined = ZR_FALSE;
    TZrBool callerOffsetSaved = ZR_FALSE;
    TZrMemoryOffset callerFrameOffset = 0u;
    uintptr_t initialStackBase = 0u;
    uintptr_t initialCallerFrameBase = 0u;
    TZrSize initialStackCapacity = 0u;
    uintptr_t returnedObject = 0u;
    EZrThreadStatus finalThreadStatus = ZR_THREAD_STATUS_FINE;
    TZrUInt32 finalResumeOffset = (TZrUInt32)-1;

    ZrCore_Memory_RawSet(&worker, 0, sizeof(worker));
    dispatch_test_atomic_init(&worker.ready);
    dispatch_test_atomic_init(&worker.stop);
    ZrCore_Value_ResetAsNull(&callableValue);
    TEST_ASSERT_NOT_NULL(state);
    collector = state->global->garbageCollector;
    TEST_ASSERT_NOT_NULL(collector);
    collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    collector->scheduledCollectionKind = ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR;
    collector->gcDebtSize = 4096;

    calleeFunction = ZrCore_Function_New(state);
    callerFunction = ZrCore_Function_New(state);
    if (calleeFunction != ZR_NULL) {
        calleeInstructions = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(TZrInstruction) * calleeInstructionCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (callerFunction != ZR_NULL) {
        callerInstructions = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(TZrInstruction) * callerInstructionCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (calleeFunction != ZR_NULL && calleeInstructions != ZR_NULL) {
        for (TZrSize index = 0u; index < calleeInstructionCount; ++index) {
            ZrCore_Memory_RawSet(
                    &calleeInstructions[index], 0, sizeof(TZrInstruction));
            calleeInstructions[index].instruction.operationCode =
                    (TZrUInt16)ZR_INSTRUCTION_ENUM(NOP);
        }
        calleeInstructions[calleeInstructionCount - 1u].instruction.operationCode =
                (TZrUInt16)ZR_INSTRUCTION_ENUM(FUNCTION_RETURN);
        calleeInstructions[calleeInstructionCount - 1u].instruction.operandExtra = 1u;
        calleeInstructions[calleeInstructionCount - 1u].instruction.operand.operand1[0] =
                0u;
        calleeFunction->instructionsList = calleeInstructions;
        calleeFunction->instructionsLength = calleeInstructionCount;
        calleeFunction->constantValueList = ZR_NULL;
        calleeFunction->constantValueLength = 0u;
        calleeFunction->stackSize = 128u;
        calleeFunction->parameterCount = 1u;
        calleeFunction->hasVariableArguments = ZR_FALSE;
        calleeFunction->closureValueLength = 0u;
    }
    if (callerFunction != ZR_NULL && callerInstructions != ZR_NULL) {
        ZrCore_Memory_RawSet(
                callerInstructions,
                0,
                sizeof(TZrInstruction) * callerInstructionCount);
        callerInstructions[0].instruction.operationCode =
                (TZrUInt16)ZR_INSTRUCTION_ENUM(FUNCTION_CALL);
        callerInstructions[0].instruction.operandExtra = 2u;
        callerInstructions[0].instruction.operand.operand1[0] = 0u;
        callerInstructions[0].instruction.operand.operand1[1] = 1u;
        callerInstructions[1].instruction.operationCode =
                (TZrUInt16)ZR_INSTRUCTION_ENUM(GET_STACK);
        callerInstructions[1].instruction.operandExtra = 3u;
        callerInstructions[1].instruction.operand.operand2[0] = 2u;
        callerInstructions[2].instruction.operationCode =
                (TZrUInt16)ZR_INSTRUCTION_ENUM(FUNCTION_RETURN);
        callerInstructions[2].instruction.operandExtra = 1u;
        callerInstructions[2].instruction.operand.operand1[0] = 3u;
        callerFunction->instructionsList = callerInstructions;
        callerFunction->instructionsLength = callerInstructionCount;
        callerFunction->constantValueList = ZR_NULL;
        callerFunction->constantValueLength = 0u;
        callerFunction->stackSize = 4u;
        callerFunction->parameterCount = 0u;
        callerFunction->hasVariableArguments = ZR_FALSE;
        callerFunction->closureValueLength = 0u;
    }
    if (calleeFunction != ZR_NULL && callerFunction != ZR_NULL &&
        calleeInstructions != ZR_NULL && callerInstructions != ZR_NULL) {
        rootObject = ZrCore_Object_New(state, ZR_NULL);
        if (rootObject != ZR_NULL) {
            ZrCore_Object_Init(state, rootObject);
            ZrCore_Value_InitAsRawObject(
                    state,
                    &callableValue,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(callerFunction));
            callableValue.type = ZR_VALUE_TYPE_FUNCTION;
            callableValue.isGarbageCollectable = ZR_TRUE;
            callableValue.isNative = ZR_FALSE;
            callerFrameBase = ZrCore_Function_CheckStackAndGc(
                    state,
                    (TZrSize)(1u + callerFunction->stackSize),
                    state->stackTop.valuePointer);
        }
    }
    if (callerFrameBase != ZR_NULL) {
        slotValue = ZrCore_Stack_GetValue(callerFrameBase);
        if (slotValue != ZR_NULL) {
            ZrCore_Value_Copy(state, slotValue, &callableValue);
            slotValue = ZrCore_Stack_GetValue(callerFrameBase + 1u);
        }
        if (slotValue != ZR_NULL) {
            ZrCore_Value_InitAsRawObject(
                    state,
                    slotValue,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(calleeFunction));
            slotValue->type = ZR_VALUE_TYPE_FUNCTION;
            slotValue->isGarbageCollectable = ZR_TRUE;
            slotValue->isNative = ZR_FALSE;
            slotValue = ZrCore_Stack_GetValue(callerFrameBase + 2u);
        }
        if (slotValue != ZR_NULL) {
            ZrCore_Value_InitAsRawObject(
                    state,
                    slotValue,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(rootObject));
            slotValue->type = ZR_VALUE_TYPE_OBJECT;
            slotValue->isGarbageCollectable = ZR_TRUE;
            slotValue->isNative = ZR_FALSE;
            ZrCore_Value_ResetAsNull(
                    ZrCore_Stack_GetValue(callerFrameBase + 3u));
            ZrCore_Value_ResetAsNull(
                    ZrCore_Stack_GetValue(callerFrameBase + 4u));
            state->stackTop.valuePointer =
                    callerFrameBase + 1u + callerFunction->stackSize;
            initialStackBase = (uintptr_t)(void *)state->stackBase.valuePointer;
            initialCallerFrameBase = (uintptr_t)(void *)callerFrameBase;
            initialStackCapacity = (TZrSize)(
                    state->stackTail.valuePointer - state->stackBase.valuePointer);
            callerFrameOffset = ZrCore_Stack_SavePointerAsOffset(
                    state, callerFrameBase);
            callerOffsetSaved = ZR_TRUE;
            callerCallInfo = ZrCore_CallInfo_Extend(state);
        }
    }
    if (callerCallInfo != ZR_NULL) {
        ZrCore_CallInfo_EntryNativeInit(
                state,
                callerCallInfo,
                state->stackBase,
                state->stackTop,
                state->callInfoList);
        callerCallInfo->functionBase.valuePointer = callerFrameBase;
        callerCallInfo->functionTop.valuePointer =
                callerFrameBase + 1u + callerFunction->stackSize;
        callerCallInfo->context.context.programCounter =
                callerFunction->instructionsList;
        callerCallInfo->callStatus = ZR_CALL_STATUS_CREATE_FRAME;
        callerCallInfo->expectedReturnCount = 1u;
        state->callInfoList = callerCallInfo;
        state->threadStatus = ZR_THREAD_STATUS_FINE;

        worker.ownerState = state;
        worker.callerFunction = callerFunction;
        worker.calleeFunction = calleeFunction;
        worker.callerFrameOffset = callerFrameOffset;
        worker.initialStackBase = initialStackBase;
        worker.initialCallerFrameBase = initialCallerFrameBase;
        worker.initialStackCapacity = initialStackCapacity;
        ZrCore_RawObject_Construct(
                &worker.collectorState.super, ZR_RAW_OBJECT_TYPE_THREAD);
        worker.collectorState.global = state->global;
        workerAttached = ZrCore_GcDomain_MutatorAttach(
                state, &worker.collectorState);
        if (workerAttached) {
            workerStarted = dispatch_vm_call_minor_gc_worker_start(
                    &workerThread, &worker);
            if (workerStarted) {
                for (TZrUInt32 attempt = 0u;
                     attempt < ZR_DISPATCH_TEST_WAIT_LIMIT_MILLISECONDS;
                     ++attempt) {
                    if (dispatch_test_atomic_load(&worker.ready)) {
                        workerReady = ZR_TRUE;
                        break;
                    }
                    dispatch_test_sleep_one_millisecond();
                }
            }
            if (workerReady) {
                ZrCore_Execute(state, callerCallInfo);
                executionReturned = ZR_TRUE;
            }
        }
    }

    if (workerStarted && !workerReady) {
        dispatch_test_atomic_store(&worker.stop, 1);
    }
    if (workerStarted) {
        workerJoined = dispatch_pause_worker_join(workerThread);
    }
    if (workerAttached) {
        ZrCore_GcDomain_MutatorDetach(&worker.collectorState);
    }
    if (state != ZR_NULL) {
        finalThreadStatus = state->threadStatus;
        if (callerCallInfo != ZR_NULL &&
            callerCallInfo->context.context.programCounter != ZR_NULL) {
            finalResumeOffset = (TZrUInt32)(
                    callerCallInfo->context.context.programCounter -
                    callerFunction->instructionsList);
        }
        if (callerOffsetSaved) {
            TZrStackValuePointer finalCallerFrameBase =
                    ZrCore_Stack_LoadOffsetToPointer(state, callerFrameOffset);
            slotValue = finalCallerFrameBase != ZR_NULL
                                ? ZrCore_Stack_GetValue(finalCallerFrameBase)
                                : ZR_NULL;
            if (slotValue != ZR_NULL && slotValue->type == ZR_VALUE_TYPE_OBJECT &&
                slotValue->isGarbageCollectable && slotValue->value.object != ZR_NULL) {
                returnedObject = (uintptr_t)(void *)slotValue->value.object;
            }
        }
        ZrTests_Runtime_State_Destroy(state);
    }

    TEST_ASSERT_TRUE(callerFunction != ZR_NULL);
    TEST_ASSERT_TRUE(calleeFunction != ZR_NULL);
    TEST_ASSERT_TRUE(rootObject != ZR_NULL);
    TEST_ASSERT_TRUE(callerCallInfo != ZR_NULL);
    TEST_ASSERT_TRUE(callerOffsetSaved);
    TEST_ASSERT_TRUE(workerAttached);
    TEST_ASSERT_TRUE(workerStarted);
    TEST_ASSERT_TRUE(workerReady);
    TEST_ASSERT_TRUE(executionReturned);
    TEST_ASSERT_TRUE(workerJoined);
    TEST_ASSERT_TRUE(worker.sawRunningMutator);
    if (worker.timedOut) {
        (void)fprintf(
                stderr,
                "VM-call collector timeout: stage=%u mutatorPolls=%u "
                "pauseAttempts=%u pauseTimeouts=%u missedCallChains=%u\n",
                (unsigned)worker.timeoutStage,
                (unsigned)worker.runningMutatorPollCount,
                (unsigned)worker.stopTheWorldBeginAttemptCount,
                (unsigned)worker.stopTheWorldBeginTimeoutCount,
                (unsigned)worker.callChainMissCount);
        if (worker.stopTheWorldBeginTimeoutCount > 0u) {
            (void)fprintf(
                    stderr,
                    "STW failure: timedOut=%u blockerIsOwner=%u blockerId=%llu "
                    "epoch=%llu registered=%u running=%u parked=%u "
                    "pauseRequested=%u\n",
                    (unsigned)worker.stopTheWorldFailureTimedOut,
                    (unsigned)worker.stopTheWorldFailureBlockedOwner,
                    (unsigned long long)worker.stopTheWorldFailureBlockingMutatorId,
                    (unsigned long long)worker.stopTheWorldFailureSafepointEpoch,
                    (unsigned)worker.failureRegisteredMutatorCount,
                    (unsigned)worker.failureRunningMutatorCount,
                    (unsigned)worker.failureParkedMutatorCount,
                    (unsigned)worker.failurePauseRequested);
        }
        (void)fflush(stderr);
    }
    TEST_ASSERT_FALSE(worker.timedOut);
    TEST_ASSERT_TRUE(worker.pauseAcquired);
    TEST_ASSERT_TRUE(worker.callChainWasVmToVm);
    TEST_ASSERT_TRUE(worker.calleeResumeOffset < ZR_DISPATCH_VM_CALL_MINOR_GC_INSTRUCTION_COUNT);
    TEST_ASSERT_TRUE(worker.callerPcWasNextInstruction);
    TEST_ASSERT_TRUE(worker.stackGrewBeforeCollection);
    TEST_ASSERT_TRUE(worker.callerFrameMatchedSavedOffset);
    if (worker.stackBaseMovedDuringCall) {
        TEST_ASSERT_TRUE(worker.callerFrameMovedDuringGrowth);
    }
    TEST_ASSERT_TRUE(worker.rootWasObjectBeforeCollection);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_GARBAGE_COLLECT_REGION_KIND_EDEN,
            worker.rootRegionBeforeCollection);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE,
            worker.rootStorageBeforeCollection);
    TEST_ASSERT_TRUE(worker.collectionStepReturned);
    TEST_ASSERT_TRUE(worker.pauseStayedActiveAcrossCollection);
    TEST_ASSERT_EQUAL_UINT64(
            worker.minorCollectionCountBefore + 1u,
            worker.minorCollectionCountAfter);
    TEST_ASSERT_EQUAL_INT(
            ZR_GARBAGE_COLLECT_COLLECTION_KIND_MINOR,
            worker.collectionKindAfterCollection);
    TEST_ASSERT_TRUE(worker.rootWasObjectAfterCollection);
    TEST_ASSERT_TRUE(worker.calleeArgumentSurvived);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR,
            worker.rootRegionAfterCollection);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE,
            worker.rootStorageAfterCollection);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, finalThreadStatus);
    TEST_ASSERT_EQUAL_UINT32(2u, finalResumeOffset);
    TEST_ASSERT_EQUAL_UINT64(worker.rootObjectAfterCollection, returnedObject);
}

#undef ZR_DISPATCH_VM_CALL_MINOR_GC_INSTRUCTION_COUNT
#undef ZR_DISPATCH_VM_CALL_MINOR_GC_PAUSE_ATTEMPTS
#undef ZR_DISPATCH_VM_CALL_MINOR_GC_TIMEOUT_NONE
#undef ZR_DISPATCH_VM_CALL_MINOR_GC_TIMEOUT_WAITING_FOR_MUTATOR
#undef ZR_DISPATCH_VM_CALL_MINOR_GC_TIMEOUT_STOP_THE_WORLD
#undef ZR_DISPATCH_VM_CALL_MINOR_GC_TIMEOUT_CALL_CHAIN
