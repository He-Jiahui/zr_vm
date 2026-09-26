#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "zr_vm_core/execution_budget.h"
#include "zr_vm_core/execution_control.h"
#include "zr_vm_core/state.h"

#include <stdlib.h>
#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
#else
#include <time.h>
#endif

struct SZrExecutionCancelToken {
    volatile TZrInt32 cancelled;
};

SZrExecutionCancelToken *ZrCore_ExecutionCancelToken_New(void) {
    return (SZrExecutionCancelToken *)calloc(1, sizeof(SZrExecutionCancelToken));
}

void ZrCore_ExecutionCancelToken_Cancel(SZrExecutionCancelToken *token) {
    if (token == ZR_NULL) {
        return;
    }
#if defined(ZR_PLATFORM_WIN)
    (void)InterlockedExchange((volatile LONG *)&token->cancelled, 1);
#else
    __atomic_store_n(&token->cancelled, 1, __ATOMIC_RELEASE);
#endif
}

TZrBool ZrCore_ExecutionCancelToken_IsCancelled(const SZrExecutionCancelToken *token) {
    if (token == ZR_NULL) {
        return ZR_FALSE;
    }
#if defined(ZR_PLATFORM_WIN)
    return (TZrBool)(InterlockedCompareExchange((volatile LONG *)&token->cancelled, 0, 0) != 0);
#else
    return (TZrBool)(__atomic_load_n(&token->cancelled, __ATOMIC_ACQUIRE) != 0);
#endif
}

void ZrCore_ExecutionCancelToken_Free(SZrExecutionCancelToken *token) {
    free(token);
}

TZrUInt64 ZrCore_ExecutionBudget_NowMicros(void) {
#if defined(ZR_PLATFORM_WIN)
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    if (!QueryPerformanceFrequency(&frequency) || !QueryPerformanceCounter(&counter)) {
        return (TZrUInt64)-1;
    }
    return (TZrUInt64)(counter.QuadPart / frequency.QuadPart) * 1000000u +
           (TZrUInt64)(counter.QuadPart % frequency.QuadPart) * 1000000u / (TZrUInt64)frequency.QuadPart;
#else
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        /* A failed clock must not silently disable an active deadline. */
        return (TZrUInt64)-1;
    }
    return (TZrUInt64)now.tv_sec * 1000000u + (TZrUInt64)now.tv_nsec / 1000u;
#endif
}

TZrBool ZrCore_ExecutionBudget_Poll(SZrState *state, TZrBool consumeInstruction) {
    SZrExecutionBudget *budget = state != ZR_NULL ? state->executionBudget : ZR_NULL;
    if (budget == ZR_NULL) {
        return ZR_TRUE;
    }
    if (state->global != ZR_NULL) {
        TZrUInt64 peak = ZrCore_ExecutionBudget_MemoryPeak(state->global);
        if (peak > budget->peakHeapBytes) {
            budget->peakHeapBytes = peak;
        }
    }
    if (budget->termination == ZR_EXECUTION_TERMINATION_NONE) {
        if (ZrCore_ExecutionCancelToken_IsCancelled(budget->cancelToken)) {
            budget->termination = ZR_EXECUTION_TERMINATION_CANCELLED;
        } else if (budget->hasDeadline && ZrCore_ExecutionBudget_NowMicros() >= budget->deadlineMicros) {
            budget->termination = ZR_EXECUTION_TERMINATION_DEADLINE;
        } else if (budget->hasHeapLimit && budget->peakHeapBytes > budget->maxHeapBytes) {
            budget->termination = ZR_EXECUTION_TERMINATION_HEAP_LIMIT;
        } else if (budget->hasGcTimeLimit && budget->gcMicros > budget->maxGcMicros) {
            budget->termination = ZR_EXECUTION_TERMINATION_GC_TIME_LIMIT;
        } else if (consumeInstruction && budget->hasInstructionLimit &&
                   budget->executedInstructions >= budget->maxInstructions) {
            budget->termination = ZR_EXECUTION_TERMINATION_INSTRUCTION_LIMIT;
        }
    }
    if (budget->termination != ZR_EXECUTION_TERMINATION_NONE) {
        state->threadStatus = ZR_THREAD_STATUS_EXECUTION_TERMINATED;
        state->hasCurrentException = ZR_FALSE;
        return ZR_FALSE;
    }
    if (consumeInstruction && budget->executedInstructions != (TZrUInt64)-1) {
        budget->executedInstructions++;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_ExecutionBudget_NativeEnter(SZrState *state, TZrBool throughBinding) {
    SZrExecutionBudget *budget = state != ZR_NULL ? state->executionBudget : ZR_NULL;
    if (budget == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!ZrCore_ExecutionBudget_Poll(state, ZR_FALSE)) {
        return ZR_FALSE;
    }
    /* A registered native function can pass through both function.c and the
     * binding dispatch. Consume its call frame once, while direct binding
     * operations inside a VM frame remain distinct calls. */
    if (throughBinding && budget->countedNativeFrame != ZR_NULL &&
        budget->countedNativeFrame == state->callInfoList) {
        budget->countedNativeFrame = ZR_NULL;
        return ZR_TRUE;
    }
    if (budget->hasNativeCallLimit && budget->nativeCalls >= budget->maxNativeCalls) {
        budget->termination = ZR_EXECUTION_TERMINATION_NATIVE_CALL_LIMIT;
        return ZrCore_ExecutionBudget_Poll(state, ZR_FALSE);
    }
    if (budget->nativeCalls != (TZrUInt64)-1) {
        budget->nativeCalls++;
    }
    budget->countedNativeFrame = throughBinding ? ZR_NULL : state->callInfoList;
    return ZR_TRUE;
}

void ZrCore_ExecutionBudget_GcBegin(SZrState *state) {
    SZrExecutionBudget *budget = state != ZR_NULL ? state->executionBudget : ZR_NULL;
    if (budget != ZR_NULL && budget->gcDepth++ == 0u) {
        budget->gcStartedMicros = ZrCore_ExecutionBudget_NowMicros();
    }
}

void ZrCore_ExecutionBudget_GcEnd(SZrState *state) {
    SZrExecutionBudget *budget = state != ZR_NULL ? state->executionBudget : ZR_NULL;
    if (budget != ZR_NULL && budget->gcDepth > 0u && --budget->gcDepth == 0u) {
        TZrUInt64 finished = ZrCore_ExecutionBudget_NowMicros();
        TZrUInt64 elapsed = finished >= budget->gcStartedMicros ? finished - budget->gcStartedMicros : 0;
        budget->gcMicros = (TZrUInt64)-1 - budget->gcMicros >= elapsed
                                  ? budget->gcMicros + elapsed : (TZrUInt64)-1;
        (void)ZrCore_ExecutionBudget_Poll(state, ZR_FALSE);
    }
}

static void execution_budget_release_frame(SZrState *state, TZrPtr argument) {
    SZrCallInfo *callInfo = (SZrCallInfo *)argument;
    SZrFunction *function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo);
    TZrStackValuePointer frameBase = callInfo->functionBase.valuePointer + 1;
    TZrMemoryOffset frameOffset = ZrCore_Stack_SavePointerAsOffset(state, frameBase);
    ZrCore_Closure_CloseStackValue(state, frameBase);
    if (function != ZR_NULL) {
        (void)ZrCore_Function_DropInlineFrameValuesOnUnwind(
                state, function, frameBase, ZrCore_Function_ResolvePrototypeFrameTypeLayout, state);
    }
    (void)ZrCore_Closure_CloseClosure(state, ZrCore_Stack_LoadOffsetToPointer(state, frameOffset),
                                     ZR_THREAD_STATUS_EXECUTION_TERMINATED, ZR_FALSE);
}

void ZrCore_ExecutionBudget_UnwindVmFrames(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    /* Unwind only to the enclosing native frame. Budget termination skips guest
     * catch/finally and inhibits guest close callbacks, while releasing frame
     * ownership/upvalues through the existing cleanup path. C recovery remains
     * inside this helper and cannot jump across a foreign native callback. */
    while (callInfo != ZR_NULL && ZR_CALL_INFO_IS_VM(callInfo)) {
        TZrUInt32 depth = state->exceptionHandlerStackLength;
        while (depth > 0 && state->exceptionHandlerStack[depth - 1].callInfo == callInfo) {
            --depth;
        }
        (void)execution_discard_exception_handlers_to_depth(state, depth);
        state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
        (void)ZrCore_Exception_TryRun(state, execution_budget_release_frame, callInfo);
        state->callInfoList = callInfo = callInfo->previous;
    }
    state->threadStatus = ZR_THREAD_STATUS_EXECUTION_TERMINATED;
    state->hasCurrentException = ZR_FALSE;
}
