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

/* TryRun 同步消费的调用描述；使用栈偏移，不持有可在扩容后失效的 callable 地址。 */
typedef struct SZrCloseMetaGuardCall {
    TZrMemoryOffset callableOffset; /* callable 相对线程栈基址的字节偏移。 */
    TZrBool isYield; /* 选择普通调用或禁止让出的调用入口。 */
} SZrCloseMetaGuardCall;

/* 暂存的原异常会退出 state.currentException；用 local-address 根槽保存其对象地址，
 * 让 callback 期间的 GC 能更新地址，正常恢复时再写回 savedException。 */
static const SZrAotGcRootSlot closure_close_meta_saved_exception_slot = {
    0u, 0u, 0u, 0u, ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u
};
/* 上述异常对象指针的一槽根图；不把整个 SZrTypeValue 当 raw object 指针扫描。 */
static const SZrAotGcRootMap closure_close_meta_saved_exception_map = {
    1u, &closure_close_meta_saved_exception_slot
};

/* 注册给 TryRun 的同步 body，恢复当前 callable 地址并以零结果派发 @close。 */
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

/* 额外原生边界承接 VM 展开与直接 native longjmp；暂藏原异常以允许嵌套调用，
 * 回到边界后核对 callback 的帧/handler/root 状态，再决定恢复原异常或保留替代状态。 */
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

    /* TryRun 的异常路径会恢复进入时的根链；这里仍独立核对 callback 的根深度与链头。
     * 若 callback 留下不平衡的根，先接回仍存活的 guard，
     * 再允许 handler 清理触发 GC，避免扫描失效的 C 局部根。 */
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

    if (callbackStatus == ZR_THREAD_STATUS_FINE &&
        state->threadStatus == ZR_THREAD_STATUS_FINE && !state->hasCurrentException) {
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
