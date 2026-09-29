//
// Created by HeJiahui on 2025/6/5.
//
#include "zr_vm_core/state.h"

#include "zr_vm_common/zr_vm_conf.h"
#include "zr_vm_common/zr_debug_conf.h"
#include "zr_vm_common/zr_runtime_sentinel_conf.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/execution_control.h"
#include "zr_vm_core/gc.h"
#include "gc/gc_domain_internal.h"
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

/* 线程私有栈与入口调用帧的初始化。 */
static void state_stack_init(SZrState *state, SZrState *mainThreadState) {
    /* 依赖 Stack_Construct 填入有效基址后再建立边界；分配失败风险已在 stack.c:125 单独登记。 */
    ZrCore_Stack_Construct(mainThreadState, &state->stackBase, ZR_THREAD_STACK_SIZE_BASIC + ZR_THREAD_STACK_SIZE_EXTRA);
    state->toBeClosedValueList.valuePointer = state->stackBase.valuePointer;
    state->stackTail.valuePointer = state->stackBase.valuePointer + ZR_THREAD_STACK_SIZE_BASIC;
    state->stackTop.valuePointer = state->stackBase.valuePointer;
    // 只清理逻辑栈容量；allocator extra slots 不进入 stackTail 表示的可用范围。
    for (TZrStackValuePointer pointer = state->stackBase.valuePointer; pointer < state->stackTail.valuePointer;
         pointer++) {
        ZrCore_Value_ResetAsNull(&pointer->value);
        pointer->toBeClosedValueOffset = 0u;
    }
    // 入口帧内嵌在 state 中，启动时无需申请扩展 callinfo。
    SZrCallInfo *callInfo = &state->baseCallInfo;
    // functionBase 指向 stackBase；base+1 是原生调用保留区起点。
    // 建立空的线程入口帧链：
    // 0 | 基础栈基址 / functionBase
    // 1 | 原生调用保留槽起点
    // ...
    // ZR_THREAD_STACK_SIZE_MIN | 初始 functionTop
    TZrStackPointer nextTop = state->stackTop;
    nextTop.valuePointer++;
    TZrStackPointer nativeCallInfoTop = nextTop;
    nativeCallInfoTop.valuePointer += ZR_THREAD_STACK_SIZE_MIN;
    ZrCore_CallInfo_EntryNativeInit(state, callInfo, state->stackTop, nativeCallInfoTop, ZR_NULL);
    state->callInfoList = callInfo;
    // when native call is finished
    state->stackTop = nextTop;
}
/* State 对象生命周期与线程状态复用入口。 */

ZR_FORCE_INLINE void ZrStateResetDebugHookCount(SZrState *state) { state->debugHookCount = state->baseDebugHookCount; }

SZrState *ZrCore_State_New(SZrGlobalState *global) {
    // 先清零所有可选字段，再构造 RawObject 头并设置线程默认状态。
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
    // 次 state 可直接加入已存在的 GC 域；主 state 在 GlobalState 创建域后另行附着。
    state->global = global;
    state->gcDomain = ZR_NULL;
    state->executionBudget = ZR_NULL;
    if (global != ZR_NULL && global->gcDomain != ZR_NULL) {
        ZrCore_GcDomain_AttachState(global->gcDomain, state);
    }
    // 栈在 launch 阶段分配；刚初始化的 state 尚无可访问的栈槽。
    state->stackBase.valuePointer = ZR_NULL;
    state->aotGcRootFrameStack = ZR_NULL;
    state->aotGcRootFrameDepth = 0u;
    // 尚无入口栈帧或扩展帧；native 调用计数从空闲状态开始。
    state->callInfoList = ZR_NULL;
    state->callInfoListLength = 0;
    state->nestedNativeCalls = 0;
    state->nestedNativeCallYieldFlag = 0;
    // 异常恢复点、handler 与暂挂控制从空状态开始。
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
    // 每个线程独立保存 hook、观察策略和调用帧 generation。
    state->baseDebugHookCount = 0;
    state->debugFrameGenerationNext = 0u;
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
    
    // 闭包链表为空；自身指针作为“尚未加入全局链”的哨兵。
    state->stackClosureValueList = ZR_NULL;
    state->threadWithStackClosures = state;
    // 新 state 在第一次派发前处于可运行状态。
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
    // 注册表初始化会创建字符串，因此必须先准备字符串表。
    ZrCore_StringTable_Init(state);
    state_trace("main thread after string table init stringTable=%p memoryError=%p",
                global != ZR_NULL ? (void *)global->stringTable : ZR_NULL,
                global != ZR_NULL ? (void *)global->memoryErrorMessage : ZR_NULL);
    // 注册全局对象与模块；依赖上面已经初始化的字符串表。
    ZrCore_GlobalState_InitRegistry(state, global);
    state_trace("main thread after registry init zrObjectType=%d",
                global != ZR_NULL ? (int)global->zrObject.type : -1);
    // 再建立元数据静态项，供注册对象使用。
    ZrCore_Meta_GlobalStaticsInit(state);
    state_trace("main thread after meta init");
    // TODO: lexer 的创建时机尚未确定；当前主线程引导不创建 lexer。

    // 字符串、注册表和元数据根均已就绪后，才恢复 GC 调度。
    global->garbageCollector->stopGcFlag = ZR_FALSE;

    // 后续初始化回调观察到的是已完成引导的 global。
    global->isValid = ZR_TRUE;

    // 宿主回调在主线程状态已完整初始化后运行；回调错误继续走 state 异常路径。
    if (global->callbacks.afterStateInitialized != ZR_NULL) {
        EZrThreadStatus result;
        ZR_CALLBACK_CALL_NO_PARAM(state, FZrAfterStateInitialized, global->callbacks.afterStateInitialized, result)
        if (result != ZR_THREAD_STATUS_FINE) {
            ZrCore_Exception_Throw(state, result);
        }
    }
}

TZrBool ZrCore_State_MutatorLaunch(SZrState *state) {
    if (state == ZR_NULL || state->global == ZR_NULL || state->global->mainThreadState == ZR_NULL ||
        state->gcDomain == ZR_NULL || state->stackBase.valuePointer != ZR_NULL || state->callInfoList != ZR_NULL) {
        return ZR_FALSE;
    }

    // 先建立次线程栈与基础帧，再登记为正在运行的 mutator。
    state_stack_init(state, state->global->mainThreadState);
    if (!ZrCore_GcDomain_MutatorEnter(state)) {
        ZrCore_Stack_Deconstruct(state,
                                 &state->stackBase,
                                 ZrCore_State_StackGetSize(state) + ZR_THREAD_STACK_SIZE_EXTRA);
        state->stackBase.valuePointer = ZR_NULL;
        state->stackTop.valuePointer = ZR_NULL;
        state->stackTail.valuePointer = ZR_NULL;
        state->callInfoList = ZR_NULL;
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

void ZrCore_State_MutatorExit(SZrState *state) {
    if (state == ZR_NULL) {
        return;
    }
    ZrCore_GcDomain_MutatorLeave(state);
}

void ZrCore_State_Exit(SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
    // TODO: 当前唯一调用方在主线程启动失败后立即执行 GlobalState_Free；需确认此钩子是否仍需独立清理职责。
}

/* 回收嵌入基础帧之后缓存的扩展帧；callInfoListLength 统计的正是这条分配链。 */
static void state_call_info_chain_free(SZrGlobalState *global, SZrState *state) {
    SZrCallInfo *callInfo = state->baseCallInfo.next;

    while (callInfo != ZR_NULL) {
        SZrCallInfo *next = callInfo->next;

        ZrCore_Memory_RawFreeWithType(global,
                                      callInfo,
                                      sizeof(SZrCallInfo),
                                      ZR_MEMORY_NATIVE_TYPE_CALL_INFO);
        callInfo = next;
    }

    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->callInfoListLength = 0u;
}


void ZrCore_State_Free(SZrGlobalState *global, SZrState *state) {
    // 参数必须仍属于同一 live GlobalState；空参数保持幂等返回。
    if (state == ZR_NULL || global == ZR_NULL) {
        return;
    }
    
    // 低地址保护只能过滤哨兵值，不能验证普通指针的实际归属。
    if ((TZrPtr)state < (TZrPtr)ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND) {
        return;  // 无效指针，不释放
    }

    // 先退出 GC 域登记，之后才释放该线程持有的运行时缓冲区。
    if (state->gcDomain != ZR_NULL) {
        ZrCore_GcDomain_DetachState(state->gcDomain, state);
    }

    state_call_info_chain_free(global, state);
    
    /* BUG: 此处检查的是字段地址而非 stackBase.valuePointer；runtime_workers.c:187/197 在线程创建失败后
     * 回收尚未 launch 的 worker 时仍进入此分支，对空边界求差并释放未分配的栈。 */
    if ((TZrPtr)&state->stackBase >= (TZrPtr)ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND) {
        ZrCore_Stack_Deconstruct(state, &state->stackBase, ZrCore_State_StackGetSize(state) + ZR_THREAD_STACK_SIZE_EXTRA);
        state->stackBase.valuePointer = ZR_NULL;
    }

    // handler 数组是独立 raw allocation；其长度是有效项数，释放尺寸须用容量。
    if (state->exceptionHandlerStack != ZR_NULL && state->exceptionHandlerStackCapacity > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                state->exceptionHandlerStack,
                                state->exceptionHandlerStackCapacity * sizeof(SZrVmExceptionHandlerState),
                                ZR_MEMORY_NATIVE_TYPE_STATE);
        state->exceptionHandlerStack = ZR_NULL;
        state->exceptionHandlerStackLength = 0;
        state->exceptionHandlerStackCapacity = 0;
    }
    
    // 最后释放由 GlobalState allocator 分配的线程对象。
    ZrCore_Memory_Allocate(global, state, sizeof(SZrState), 0, ZR_MEMORY_NATIVE_TYPE_STATE);
}

/* 通过 TryRun 包住 pending 值清理，避免其关闭回调越过 ResetThread 的状态恢复。 */
static void state_clear_pending_control(SZrState *state, TZrPtr argument) {
    ZR_UNUSED_PARAMETER(argument);
    execution_clear_pending_control(state);
}

TZrInt32 ZrCore_State_ResetThread(SZrState *state, EZrThreadStatus status) {
    EZrThreadStatus handlerStatus = execution_discard_exception_handlers_to_depth(state, 0u);
    EZrThreadStatus pendingStatus = ZrCore_Exception_TryRun(
            state, state_clear_pending_control, ZR_NULL);
    if (handlerStatus != ZR_THREAD_STATUS_FINE) {
        status = handlerStatus;
    } else if (pendingStatus != ZR_THREAD_STATUS_FINE) {
        status = pendingStatus;
    }
    // 复用线程时恢复内嵌 Native 入口帧，避免上一轮脚本调用帧继续可达。
    SZrCallInfo *callInfo = state->callInfoList = &state->baseCallInfo;
    // 清除入口槽；随后按返回状态重建错误槽或空闲栈顶。
    ZrCore_Value_ResetAsNull(&state->stackBase.valuePointer->value);
    callInfo->functionBase.valuePointer = state->stackBase.valuePointer;
    callInfo->callStatus = ZR_CALL_STATUS_NATIVE_CALL;
    callInfo->metadataFunction = ZR_NULL;
    if (status == ZR_THREAD_STATUS_YIELD) {
        status = ZR_THREAD_STATUS_FINE;
    }
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    state->exceptionRecoverPoint = ZR_NULL;
    /* BUG: 主线程有恢复点时，exception.c:590-595 会在 ResetThread 后复制 worker 异常；这里先清空三字段使其丢失。 */
    ZrCore_Value_ResetAsNull(&state->currentException);
    state->currentExceptionStatus = ZR_THREAD_STATUS_FINE;
    state->hasCurrentException = ZR_FALSE;
    state->aotGcRootFrameStack = ZR_NULL;
    state->aotGcRootFrameDepth = 0u;
    status = ZrCore_Exception_TryStop(state, 1, status);
    if (status != ZR_THREAD_STATUS_FINE) {
        /* MarkError leaves stackTop one slot before its destination. Collapse the
         * failed frame to the same base boundary as a clean reset. */
        ZrCore_Exception_MarkError(state, status, state->stackBase.valuePointer + 2);
    } else {
        state->stackTop.valuePointer = state->stackBase.valuePointer + 1;
    }
    callInfo->functionTop.valuePointer = state->stackTop.valuePointer + ZR_STACK_NATIVE_CALL_RESERVED_MIN;

    // TODO: 对照 CLI/REPL、task 与 Rust 重用入口，确认 provider/debug/AOT 配置哪些应跨轮保留或清除。

    return status;
}

EZrThreadStatus ZrCore_State_DoRun(SZrState *state, TZrNativeString entry) {
    // TODO: 仓内尚无从已加载模块继续执行的调用路径。

    SZrIoSource *source = ZrCore_Io_LoadSource(state, entry, ZR_NULL);
    if (source == ZR_NULL) {
        return ZR_THREAD_STATUS_RUNTIME_ERROR;
    }
    // TODO: 当前仅加载 source 后返回成功；源码转换与执行链尚未接入，也没有仓内调用者。

    return ZR_THREAD_STATUS_FINE;
}

/* 启动追踪开关按进程首次读取 getenv 的结果缓存，之后修改环境变量不会生效。 */
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

/* 仅在显式启用时向 stderr 输出 bootstrap 诊断，不改变 state 状态。 */
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
