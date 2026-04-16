//
// Created by HeJiahui on 2025/6/5.
//
#include "zr_vm_core/state.h"

#include "zr_vm_common/zr_vm_conf.h"
#include "zr_vm_common/zr_debug_conf.h"
#include "zr_vm_common/zr_runtime_sentinel_conf.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/profile.h"
#include "zr_vm_core/string.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static TZrBool state_trace_enabled(void);
static void state_trace(const TZrChar *format, ...);

/*
 * ===== State Stack Functions =====
 */
static void state_stack_init(SZrState *state, SZrState *mainThreadState) {
    /*TZrPtr stackEndExtra = */
    ZrCore_Stack_Construct(mainThreadState, &state->stackBase, ZR_THREAD_STACK_SIZE_BASIC + ZR_THREAD_STACK_SIZE_EXTRA);
    state->toBeClosedValueList.valuePointer = state->stackBase.valuePointer;
    state->stackTail.valuePointer = state->stackBase.valuePointer + ZR_THREAD_STACK_SIZE_BASIC;
    state->stackTop.valuePointer = state->stackBase.valuePointer;
    // reset stack
    for (TZrStackValuePointer pointer = state->stackBase.valuePointer; pointer < state->stackTail.valuePointer;
         pointer++) {
        ZrCore_Value_ResetAsNull(&pointer->value);
        pointer->toBeClosedValueOffset = 0u;
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
    ZrCore_CallInfo_EntryNativeInit(state, callInfo, state->stackTop, nativeCallInfoTop, ZR_NULL);
    state->callInfoList = callInfo;
    // when native call is finished
    state->stackTop = nextTop;
}


/*
 * ===== State Functions =====
 */

ZR_FORCE_INLINE void ZrStateResetDebugHookCount(SZrState *state) { state->debugHookCount = state->baseDebugHookCount; }

SZrState *ZrCore_State_New(SZrGlobalState *global) {
    // FZrAllocator allocator = global->allocator;
    SZrState *newState = ZrCore_Memory_Allocate(global, NULL, 0, sizeof(SZrState), ZR_MEMORY_NATIVE_TYPE_STATE);
    if (newState == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Memory_RawSet(newState, 0, sizeof(SZrState));
    ZrCore_RawObject_Construct(&newState->super, ZR_RAW_OBJECT_TYPE_THREAD);
    ZrCore_State_Init(newState, global);
    return newState;
}

void ZrCore_State_Init(SZrState *state, SZrGlobalState *global) {
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
    ZrCore_Value_ResetAsNull(&state->currentException);
    state->currentExceptionStatus = ZR_THREAD_STATUS_FINE;
    state->hasCurrentException = ZR_FALSE;
    state->exceptionHandlerStack = ZR_NULL;
    state->exceptionHandlerStackLength = 0;
    state->exceptionHandlerStackCapacity = 0;
    state->pendingControl.kind = ZR_VM_PENDING_CONTROL_NONE;
    state->pendingControl.callInfo = ZR_NULL;
    state->pendingControl.targetInstructionOffset = 0;
    state->pendingControl.valueSlot = 0;
    ZrCore_Value_ResetAsNull(&state->pendingControl.value);
    state->pendingControl.hasValue = ZR_FALSE;
    // debug
    state->baseDebugHookCount = 0;
    state->debugHook = ZR_NULL;
    state->debugHookSignal = 0;
    state->debugTraceObserver = ZR_NULL;
    state->debugTraceUserData = ZR_NULL;
    state->debugLastFunction = ZR_NULL;
    state->debugLastLine = ZR_RUNTIME_DEBUG_HOOK_LINE_NONE;
    state->aotObservationMask = 0;
    state->hasAotObservationPolicyOverride = ZR_FALSE;
    state->aotPublishAllInstructions = ZR_FALSE;
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
    ZrCore_Profile_SetCurrentState(state);
}

void ZrCore_State_MainThreadLaunch(SZrState *state, TZrPtr arguments) {
    ZR_UNUSED_PARAMETER(arguments);
    SZrGlobalState *global = state->global;
    state_trace("main thread launch enter state=%p global=%p", (void *)state, (void *)global);
    state_stack_init(state, global->mainThreadState);
    state_trace("main thread after stack init stackBase=%p stackTop=%p stackTail=%p",
                (void *)state->stackBase.valuePointer,
                (void *)state->stackTop.valuePointer,
                (void *)state->stackTail.valuePointer);
    // string table init (必须在 ZrCore_GlobalState_InitRegistry 之前，因为后者会创建字符串)
    ZrCore_StringTable_Init(state);
    state_trace("main thread after string table init stringTable=%p memoryError=%p",
                global != ZR_NULL ? (void *)global->stringTable : ZR_NULL,
                global != ZR_NULL ? (void *)global->memoryErrorMessage : ZR_NULL);
    // global registry module init
    ZrCore_GlobalState_InitRegistry(state, global);
    state_trace("main thread after registry init zrObjectType=%d",
                global != ZR_NULL ? (int)global->zrObject.type : -1);
    // meta name init
    ZrCore_Meta_GlobalStaticsInit(state);
    state_trace("main thread after meta init");
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
            ZrCore_Exception_Throw(state, result);
        }
    }
}

void ZrCore_State_Exit(SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
    // SZrGlobalState *global = state->global;
    // todo
}


void ZrCore_State_Free(SZrGlobalState *global, SZrState *state) {
    // 检查参数有效性
    if (state == ZR_NULL || global == ZR_NULL) {
        return;
    }
    
    // 检查state指针是否在合理范围内（避免访问无效内存）
    if ((TZrPtr)state < (TZrPtr)ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND) {
        return;  // 无效指针，不释放
    }
    
    // 检查stackBase是否有效（在访问之前）
    if ((TZrPtr)&state->stackBase >= (TZrPtr)ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND) {
        ZrCore_Stack_Deconstruct(state, &state->stackBase, ZrCore_State_StackGetSize(state) + ZR_THREAD_STACK_SIZE_EXTRA);
        state->stackBase.valuePointer = ZR_NULL;
    }

    if (state->exceptionHandlerStack != ZR_NULL && state->exceptionHandlerStackCapacity > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                state->exceptionHandlerStack,
                                state->exceptionHandlerStackCapacity * sizeof(SZrVmExceptionHandlerState),
                                ZR_MEMORY_NATIVE_TYPE_STATE);
        state->exceptionHandlerStack = ZR_NULL;
        state->exceptionHandlerStackLength = 0;
        state->exceptionHandlerStackCapacity = 0;
    }
    
    // 释放state本身
    ZrCore_Memory_Allocate(global, state, sizeof(SZrState), 0, ZR_MEMORY_NATIVE_TYPE_STATE);
}

TZrInt32 ZrCore_State_ResetThread(SZrState *state, EZrThreadStatus status) {
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
    ZrCore_Value_ResetAsNull(&state->currentException);
    state->currentExceptionStatus = ZR_THREAD_STATUS_FINE;
    state->hasCurrentException = ZR_FALSE;
    state->exceptionHandlerStackLength = 0;
    state->pendingControl.kind = ZR_VM_PENDING_CONTROL_NONE;
    state->pendingControl.callInfo = ZR_NULL;
    state->pendingControl.targetInstructionOffset = 0;
    state->pendingControl.valueSlot = 0;
    ZrCore_Value_ResetAsNull(&state->pendingControl.value);
    state->pendingControl.hasValue = ZR_FALSE;
    status = ZrCore_Exception_TryStop(state, 1, status);
    if (status != ZR_THREAD_STATUS_FINE) {
        ZrCore_Exception_MarkError(state, status, state->stackTop.valuePointer + 1);
    } else {
        state->stackTop.valuePointer = state->stackBase.valuePointer + 1;
    }
    callInfo->functionTop.valuePointer = state->stackTop.valuePointer + ZR_STACK_NATIVE_CALL_RESERVED_MIN;

    // todo:

    return status;
}

EZrThreadStatus ZrCore_State_DoRun(SZrState *state, TZrNativeString entry) {
    // todo: use loaded module is currently not supported

    SZrIoSource *source = ZrCore_Io_LoadSource(state, entry, ZR_NULL);
    if (source == ZR_NULL) {
        return ZR_THREAD_STATUS_RUNTIME_ERROR;
    }
    // todo: convert source to object

    return ZR_THREAD_STATUS_FINE;
}

static TZrBool state_trace_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        const TZrChar *flag = getenv("ZR_VM_TRACE_CORE_BOOTSTRAP");
        enabled = (flag != ZR_NULL && flag[0] != '\0') ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

static void state_trace(const TZrChar *format, ...) {
    va_list arguments;

    if (!state_trace_enabled() || format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    fprintf(stderr, "[zr-state] ");
    vfprintf(stderr, format, arguments);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(arguments);
}
