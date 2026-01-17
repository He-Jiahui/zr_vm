//
// Created by HeJiahui on 2025/6/5.
//
#include "zr_vm_core/state.h"

#include "zr_vm_common/zr_vm_conf.h"
#include "zr_vm_common/zr_debug_conf.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/string.h"

/*
 * ===== State Stack Functions =====
 */
static void ZrStateStackInit(SZrState *state, SZrState *mainThreadState) {
    /*TZrPtr stackEndExtra = */
    ZrStackConstruct(mainThreadState, &state->stackBase, ZR_THREAD_STACK_SIZE_BASIC + ZR_THREAD_STACK_SIZE_EXTRA);
    state->toBeClosedValueList.valuePointer = state->stackBase.valuePointer;
    state->stackTail.valuePointer = state->stackBase.valuePointer + ZR_THREAD_STACK_SIZE_BASIC;
    state->stackTop.valuePointer = state->stackBase.valuePointer;
    // reset stack
    for (TZrStackValuePointer pointer = state->stackBase.valuePointer; pointer < state->stackTail.valuePointer;
         pointer++) {
        ZrValueResetAsNull(&pointer->value);
    }
    // init call info
    SZrCallInfo *callInfo = &state->baseCallInfo;
    // assume an empty call info as start entry
    // init call info list
    // 0|-- base -- NULL(functionBase)
    // 1|-- top1 -- Native Call Stack Empty Space
    // ...
    // STACK_SIZE_MIN|-- top2 -- (functionTop) —
    TZrStackPointer nextTop = state->stackTop;
    nextTop.valuePointer++;
    TZrStackPointer nativeCallInfoTop = nextTop;
    nativeCallInfoTop.valuePointer += ZR_THREAD_STACK_SIZE_MIN;
    ZrCallInfoEntryNativeInit(state, callInfo, state->stackTop, nativeCallInfoTop, ZR_NULL);
    state->callInfoList = callInfo;
    // when native call is finished
    state->stackTop = nextTop;
}


/*
 * ===== State Functions =====
 */

ZR_FORCE_INLINE void ZrStateResetDebugHookCount(SZrState *state) { state->debugHookCount = state->baseDebugHookCount; }

SZrState *ZrStateNew(SZrGlobalState *global) {
    // FZrAllocator allocator = global->allocator;
    SZrState *newState = ZrMemoryAllocate(global, NULL, 0, sizeof(SZrState), ZR_MEMORY_NATIVE_TYPE_STATE);
    ZrRawObjectConstruct(&newState->super, ZR_VALUE_TYPE_THREAD);
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
    state->nestedNativeCallYieldFlag = 0;
    // exception
    state->exceptionRecoverPoint = ZR_NULL;
    state->exceptionHandlingFunctionOffset = 0;
    // debug
    state->baseDebugHookCount = 0;
    state->debugHook = ZR_NULL;
    state->debugHookSignal = 0;
    ZrStateResetDebugHookCount(state);
    state->allowDebugHook = ZR_TRUE;
    
    // 运行时检查标志（使用默认配置）
    state->enableRuntimeBoundsCheck = ZR_ENABLE_RUNTIME_BOUNDS_CHECK;
    state->enableRuntimeTypeCheck = ZR_ENABLE_RUNTIME_TYPE_CHECK;
    state->enableRuntimeRangeCheck = ZR_ENABLE_RUNTIME_RANGE_CHECK;
    
    // closures
    state->stackClosureValueList = ZR_NULL;
    // link to self as thread with stack closures
    state->threadWithStackClosures = state;
    // thread
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    state->previousProgramCounter = 0;
}

void ZrStateMainThreadLaunch(SZrState *state, TZrPtr arguments) {
    ZR_UNUSED_PARAMETER(arguments);
    SZrGlobalState *global = state->global;
    ZrStateStackInit(state, global->mainThreadState);
    // string table init (必须在 ZrGlobalStateInitRegistry 之前，因为后者会创建字符串)
    ZrStringTableInit(state);
    // global registry module init
    ZrGlobalStateInitRegistry(state, global);
    // meta name init
    ZrMetaGlobalStaticsInit(state);
    // maybe we can create a lexer

    // allow gc to run
    global->garbageCollector->stopGcFlag = ZR_FALSE;

    // we finish the global state initialization, mark it as valid
    global->isValid = ZR_TRUE;

    // callback after global state initialization
    if (global->callbacks.afterStateInitialized != ZR_NULL) {
        EZrThreadStatus result;
        ZR_CALLBACK_CALL_NO_PARAM(state, FZrAfterStateInitialized, global->callbacks.afterStateInitialized, result)
        if (result != ZR_THREAD_STATUS_FINE) {
            ZrExceptionThrow(state, result);
        }
    }
}

void ZrStateExit(SZrState *state) {
    // SZrGlobalState *global = state->global;
    // todo
}


void ZrStateFree(SZrGlobalState *global, SZrState *state) {
    // 检查参数有效性
    if (state == ZR_NULL || global == ZR_NULL) {
        return;
    }
    
    // 检查state指针是否在合理范围内（避免访问无效内存）
    if ((TZrPtr)state < (TZrPtr)0x1000) {
        return;  // 无效指针，不释放
    }
    
    // 检查stackBase是否有效（在访问之前）
    if ((TZrPtr)&state->stackBase >= (TZrPtr)0x1000) {
        ZrStackDeconstruct(state, &state->stackBase, ZrStateStackGetSize(state) + ZR_THREAD_STACK_SIZE_EXTRA);
        state->stackBase.valuePointer = ZR_NULL;
    }
    
    // 释放state本身
    ZrMemoryAllocate(global, state, sizeof(SZrState), 0, ZR_MEMORY_NATIVE_TYPE_STATE);
}

TInt32 ZrStateResetThread(SZrState *state, EZrThreadStatus status) {
    // 重置线程状态
    // 调用栈回到创建时基础调用栈
    SZrCallInfo *callInfo = state->callInfoList = &state->baseCallInfo;
    // 重置栈到基础栈
    state->stackBase.valuePointer->value.type = ZR_VALUE_TYPE_NULL;
    callInfo->functionBase.valuePointer = state->stackBase.valuePointer;
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

EZrThreadStatus ZrStateDoRun(SZrState *state, TNativeString entry) {
    SZrGlobalState *global = state->global;

    // todo: use loaded module is currently not supported

    SZrIoSource *source = ZrIoLoadSource(state, entry, ZR_NULL);
    if (source == ZR_NULL) {
        return ZR_THREAD_STATUS_RUNTIME_ERROR;
    }
    // todo: convert source to object

    return ZR_THREAD_STATUS_FINE;
}
