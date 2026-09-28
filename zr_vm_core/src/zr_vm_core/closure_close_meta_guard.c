#include "closure_close_meta_guard.h"

#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/execution_budget.h"
#include "zr_vm_core/execution_control.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"

typedef struct SZrCloseMetaGuardCall {
    TZrMemoryOffset callableOffset;
    TZrBool isYield;
} SZrCloseMetaGuardCall;

static const SZrAotGcRootSlot closure_close_meta_saved_exception_slot = {
    0u, 0u, 0u, 0u, ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u
};
static const SZrAotGcRootMap closure_close_meta_saved_exception_map = {
    1u, &closure_close_meta_saved_exception_slot
};

static void closure_close_meta_call_body(SZrState *state, TZrPtr argument) {
    const SZrCloseMetaGuardCall *call = (const SZrCloseMetaGuardCall *)argument;
    TZrStackValuePointer callable =
            ZrCore_Stack_LoadOffsetToPointer(state, call->callableOffset);

    if (call->isYield) {
        ZrCore_Function_Call(state, callable, 0u);
    } else {
        ZrCore_Function_CallWithoutYield(state, callable, 0u);
    }
}

/* A direct native throw skips its normal PostCall. This one frame can be
 * discarded only when it owns no cleanup registrations or inline frame data. */
static TZrBool closure_close_meta_can_discard_direct_native_frame(
        SZrState *state, SZrCallInfo *boundary,
        TZrMemoryOffset boundarySlotOffset, EZrThreadStatus callbackStatus) {
    SZrCallInfo *child = state->callInfoList;
    TZrStackValuePointer callbackSlot;
    TZrStackValuePointer frameBase;

    if (callbackStatus == ZR_THREAD_STATUS_FINE || child == ZR_NULL ||
        child != boundary->next || child->previous != boundary ||
        child->callStatus != ZR_CALL_STATUS_NATIVE_CALL ||
        child->expectedReturnCount != 0u || child->hasReturnDestination ||
        child->hasArgumentSourceFrame ||
        ZrCore_CallInfo_GetFrameStorageSlotCountPlusOne(child) != 0u ||
        child->context.nativeContext.continuationFunction != ZR_NULL ||
        child->functionBase.valuePointer == ZR_NULL ||
        child->functionTop.valuePointer == ZR_NULL) {
        return ZR_FALSE;
    }
    callbackSlot = ZrCore_Stack_LoadOffsetToPointer(state, boundarySlotOffset) + 1u;
    if (child->functionBase.valuePointer != callbackSlot ||
        ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, child) != ZR_NULL) {
        return ZR_FALSE;
    }
    frameBase = child->functionBase.valuePointer + 1u;
    return (TZrBool)(frameBase <= child->functionTop.valuePointer &&
                     state->toBeClosedValueList.valuePointer < frameBase &&
                     !ZrCore_Closure_HasOpenStackValueInRange(
                             state, frameBase, child->functionTop.valuePointer));
}

void ZrCore_ClosureCloseMetaGuard_Invoke(SZrState *state,
                                         TZrMemoryOffset boundarySlotOffset,
                                         TZrBool isYield) {
    SZrCallInfo *outer = state->callInfoList;
    SZrCallInfo *boundary = outer->next;
    SZrCallInfo *savedChild;
    SZrCallInfo *savedCountedNativeFrame = state->executionBudget != ZR_NULL
                                              ? state->executionBudget->countedNativeFrame
                                              : ZR_NULL;
    SZrAotGcRootFrame *savedRootFrameTop = state->aotGcRootFrameStack;
    SZrTypeValue savedException;
    SZrRawObject *savedExceptionRoot;
    SZrAotGcRootFrame rootFrame;
    SZrCloseMetaGuardCall call;
    EZrThreadStatus savedExceptionStatus;
    EZrThreadStatus savedThreadStatus;
    EZrThreadStatus callbackStatus;
    TZrBool hadException;
    TZrBool handlerUnderflow;
    TZrBool callbackFrameActive;
    TZrBool rootFrameUnderflow;
    TZrBool rootFrameUnbalanced;
    TZrBool rootFramePopFailed;
    TZrUInt32 handlerDepth;
    TZrUInt32 savedRootFrameDepth = state->aotGcRootFrameDepth;
    TZrUInt32 savedYieldCount = state->nestedNativeCallYieldFlag;
    TZrStackPointer boundarySlot;
    TZrStackPointer boundaryTop;

    if (state->pendingControl.kind != ZR_VM_PENDING_CONTROL_NONE ||
        state->pendingControl.hasValue) {
        ZrCore_Debug_RunError(state,
                "@close exception callback entered with pending control");
    }
    /* Allocation may move the value stack, so keep only offsets until it ends. */
    if (boundary == ZR_NULL) {
        boundary = ZrCore_CallInfo_Extend(state);
        if (boundary == ZR_NULL) {
            ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
            return;
        }
    }
    outer = state->callInfoList;
    savedException = state->currentException;
    savedExceptionStatus = state->currentExceptionStatus;
    savedThreadStatus = state->threadStatus;
    hadException = state->hasCurrentException;
    handlerDepth = state->exceptionHandlerStackLength;
    savedExceptionRoot = hadException && ZrCore_Value_IsGarbageCollectable(&savedException)
                                 ? ZrCore_Value_GetRawObject(&savedException)
                                 : ZR_NULL;
    if (!ZrCore_Gc_AotRootFramePush(state, &rootFrame,
                                    (TZrStackValuePointer)&savedExceptionRoot,
                                    &closure_close_meta_saved_exception_map)) {
        ZrCore_Exception_ClearCurrent(state);
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
        return;
    }

    boundarySlot.valuePointer = ZrCore_Stack_LoadOffsetToPointer(
            state, boundarySlotOffset);
    boundaryTop = state->stackTop;
    savedChild = boundary->next;
    ZrCore_CallInfo_EntryNativeInit(state, boundary, boundarySlot, boundaryTop, outer);
    boundary->next = savedChild;
    state->callInfoList = boundary;
    call.callableOffset = boundarySlotOffset + sizeof(SZrTypeValueOnStack);
    call.isYield = isYield;

    /* The Error argument is already in the callback frame. Keep the ambient
     * exception out of nested VM/native return checks until @close completes. */
    ZrCore_Exception_ClearCurrent(state);
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    callbackStatus = ZrCore_Exception_TryRun(state, closure_close_meta_call_body, &call);

    /* TryRun cannot pop root frames created by a callback that longjumped.
     * Their C stack storage is already dead, so cut the chain directly back
     * to this still-live guard frame before any handler cleanup can run GC. */
    rootFrameUnderflow = state->aotGcRootFrameDepth < savedRootFrameDepth + 1u;
    rootFrameUnbalanced = state->aotGcRootFrameStack != &rootFrame ||
                          state->aotGcRootFrameDepth != savedRootFrameDepth + 1u;
    if (rootFrameUnbalanced) {
        state->aotGcRootFrameStack = &rootFrame;
        state->aotGcRootFrameDepth = savedRootFrameDepth + 1u;
    }
    state->nestedNativeCallYieldFlag = savedYieldCount;
    if (state->executionBudget != ZR_NULL) {
        state->executionBudget->countedNativeFrame = savedCountedNativeFrame;
    }

    handlerUnderflow = state->exceptionHandlerStackLength < handlerDepth;
    callbackFrameActive = state->callInfoList != boundary;
    if (callbackFrameActive &&
        closure_close_meta_can_discard_direct_native_frame(
                state, boundary, boundarySlotOffset, callbackStatus)) {
        /* The callback frame is a reusable child of the boundary. It has no
         * return destination or owned frame cleanup, so dropping the active
         * link is the native throw equivalent of its ordinary PostCall. */
        state->callInfoList = boundary;
        callbackFrameActive = ZR_FALSE;
    }
    if (state->exceptionHandlerStackLength > handlerDepth) {
        EZrThreadStatus discardStatus =
                execution_discard_exception_handlers_to_depth(state, handlerDepth);
        if (callbackStatus == ZR_THREAD_STATUS_FINE) {
            callbackStatus = discardStatus;
        }
    }
    /* A VM throw unwinds to the native boundary. A direct native throw may
     * longjmp first; remove any remaining callback frame from the active chain. */
    state->callInfoList = outer;
    state->stackTop.valuePointer = ZrCore_Stack_LoadOffsetToPointer(
            state, boundarySlotOffset);

    if (callbackStatus == ZR_THREAD_STATUS_FINE && !state->hasCurrentException) {
        state->currentException = savedException;
        if (savedExceptionRoot != ZR_NULL) {
            state->currentException.value.object = savedExceptionRoot;
        }
        state->currentExceptionStatus = savedExceptionStatus;
        state->hasCurrentException = hadException;
        state->threadStatus = savedThreadStatus;
    } else if (callbackStatus != ZR_THREAD_STATUS_FINE) {
        state->threadStatus = callbackStatus;
    }
    rootFramePopFailed = !ZrCore_Gc_AotRootFramePop(state, &rootFrame) ||
                         state->aotGcRootFrameStack != savedRootFrameTop ||
                         state->aotGcRootFrameDepth != savedRootFrameDepth;
    if (rootFramePopFailed) {
        state->aotGcRootFrameStack = savedRootFrameTop;
        state->aotGcRootFrameDepth = savedRootFrameDepth;
    }
    if (handlerUnderflow || callbackFrameActive ||
        rootFrameUnderflow || rootFramePopFailed ||
        (callbackStatus == ZR_THREAD_STATUS_FINE && rootFrameUnbalanced) ||
        state->pendingControl.kind != ZR_VM_PENDING_CONTROL_NONE ||
        state->pendingControl.hasValue) {
        ZrCore_Debug_RunError(state,
                "@close exception callback escaped its native boundary");
    }
}
