//
// Created by HeJiahui on 2025/6/5.
//
#include "zr_vm_core/state.h"

#include "zr_vm_common/zr_vm_conf.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
ZR_FORCE_INLINE void ZrStateResetDebugHookCount(SZrState *state) {
    state->debugHookCount = state->baseDebugHookCount;
}

SZrState *ZrStateNew(SZrGlobalState *global) {
    // FZrAllocator allocator = global->allocator;
    SZrState *newState = ZrAllocate(global, NULL, 0, sizeof(SZrState));
    ZrObjectInit(&newState->super, ZR_VALUE_TYPE_THREAD);
    ZrStateInit(newState, global);
    return newState;
}

void ZrStateInit(SZrState *state, SZrGlobalState *global) {
    // global
    state->global = global;
    // stack
    state->stackBase.valuePointer = ZR_NULL;
    // call info
    state->callInfoList = ZR_NULL;
    state->callInfoListLength = 0;
    state->nestedNativeCalls = 0;
    // exception
    state->exceptionRecoverPoint = ZR_NULL;
    state->exceptionHandlingFunctionOffset = 0;
    // debug
    state->baseDebugHookCount = 0;
    state->debugHook = ZR_NULL;
    state->debugHookSignal = 0;
    ZrStateResetDebugHookCount(state);
    state->allowDebugHook = ZR_TRUE;
    // closures
    state->aliveClosureValueList = ZR_NULL;
    state->threadWithAliveClosures = state;
    // thread
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    state->previousProgramCounter = 0;
}


void ZrStateFree(SZrGlobalState *global, SZrState *state) {
    ZrAllocate(global, state, sizeof(SZrState), 0);
}

TInt32 ZrStateResetThread(SZrState *state, EZrThreadStatus status) {
    // 重置线程状态
    // 调用栈回到创建时基础调用栈
    SZrCallInfo *callInfo = state->callInfoList = &state->baseCallInfo;
    // 重置栈到基础栈
    state->stackBase.valuePointer->value.type = ZR_VALUE_TYPE_NULL;
    callInfo->functionIndex.valuePointer = state->stackBase.valuePointer;
    callInfo->callStatus = ZR_CALL_STATUS_NATIVE_CALL;
    if (status == ZR_THREAD_STATUS_YIELD) {
        status = ZR_THREAD_STATUS_FINE;
    }
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    state->exceptionRecoverPoint = ZR_NULL;
    status = ZrExceptionTryStop(state, 1, status);
    if (status != ZR_THREAD_STATUS_FINE) {
        ZrExceptionMarkError(state, status, state->stackTop.valuePointer + 1);
    } else {
        state->stackTop.valuePointer = state->stackBase.valuePointer + 1;
    }
    callInfo->functionTop.valuePointer = state->stackTop.valuePointer + ZR_STACK_NATIVE_CALL_MIN;

    // todo:

    return status;
}

ZR_FORCE_INLINE TZrMemoryOffset ZrStateStackSaveAsOffset(SZrState *state, TZrStackPointer pointer) {
    return pointer - state->stackBase.valuePointer;
}

static void ZrStateStackMarkStackAsRelative(SZrState *state) {
    state->stackTop.reusableValueOffset = ZrStateStackSaveAsOffset(state, state->stackTop.valuePointer);
    state->waitToReleaseList.reusableValueOffset = ZrStateStackSaveAsOffset(
        state, state->waitToReleaseList.valuePointer);
    // todo: upval
    for (SZrCallInfo *callInfo = state->callInfoList; callInfo != ZR_NULL; callInfo = callInfo->previous) {
        callInfo->functionIndex.reusableValueOffset = ZrStateStackSaveAsOffset(
            state, callInfo->functionIndex.valuePointer);
        callInfo->functionTop.reusableValueOffset = ZrStateStackSaveAsOffset(state, callInfo->functionTop.valuePointer);
    }
}

TBool ZrStateStackRealloc(SZrState *state, TUInt64 newSize, TBool throwError) {
    TZrSize previousStackSize = ZrStateStackGetSize(state);
    TZrStackPointer newStackPointer;
    TBool previousStopGcFlag = state->global->stopGcFlag;
    ZR_ASSERT(newSize <= ZR_VM_MAX_STACK || newSize == ZR_VM_ERROR_STACK);
    ZrStateStackMarkStackAsRelative(state);
    state->global->stopGcFlag = ZR_TRUE;
    // todo: luaD_reallocstack
    // todo: luaM_reallocvector
    // todo: newStackPointer =
    ZR_ASSERT(ZR_FALSE);

    return ZR_FALSE;
}
