//
// Created by HeJiahui on 2025/6/26.
//
#include "zr_vm_core/debug.h"

#include <stdarg.h>
#include "zr_vm_core/exception.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

TBool ZrDebugInfoGet(struct SZrState *state, EZrDebugInfoType type, SZrDebugInfo *debugInfo) {
    ZR_TODO_PARAMETER(state);
    ZR_TODO_PARAMETER(type);
    ZR_TODO_PARAMETER(debugInfo);
    return ZR_FALSE;
}

void ZrDebugCallError(struct SZrState *state, struct SZrTypeValue *value) {
    ZR_TODO_PARAMETER(state);
    ZR_TODO_PARAMETER(value);
}

TZrDebugSignal ZrDebugTraceExecution(struct SZrState *state, const TZrInstruction *programCounter) {
    ZR_TODO_PARAMETER(state);
    ZR_TODO_PARAMETER(programCounter);

    return 0;
}

ZR_NO_RETURN void ZrDebugRunError(struct SZrState *state, TNativeString format, ...) {
    if (state == ZR_NULL || format == ZR_NULL) {
        ZR_ABORT();
    }

    // 格式化错误消息
    va_list args;
    va_start(args, format);
    TNativeString errorMessage = ZrNativeStringVFormat(state, format, args);
    va_end(args);

    if (errorMessage == ZR_NULL) {
        // 如果格式化失败，使用默认消息
        errorMessage = "Runtime error";
    }

    // 创建错误消息字符串对象
    SZrString *errorString = ZrStringCreateFromNative(state, errorMessage);
    if (errorString == ZR_NULL) {
        // 如果创建字符串失败，检查是否有 panic handling function
        SZrGlobalState *global = state->global;
        if (global != ZR_NULL && global->panicHandlingFunction != ZR_NULL) {
            ZR_THREAD_UNLOCK(state);
            global->panicHandlingFunction(state);
        }
        ZR_ABORT();
    }

    // 确保栈有足够空间
    ZrFunctionCheckStackAndGc(state, 1, state->stackTop.valuePointer);

    // 将错误消息字符串放到栈上
    SZrTypeValue *errorValue = ZrStackGetValue(state->stackTop.valuePointer);
    ZrValueInitAsRawObject(state, errorValue, ZR_CAST_RAW_OBJECT_AS_SUPER(errorString));
    errorValue->type = ZR_VALUE_TYPE_STRING;
    errorValue->isGarbageCollectable = ZR_TRUE;
    errorValue->isNative = ZR_FALSE;
    state->stackTop.valuePointer++;

    // 检查是否有已注册的异常处理函数
    SZrGlobalState *global = state->global;
    if (global != ZR_NULL && global->panicHandlingFunction != ZR_NULL) {
        // 如果有 panic handling function，先调用它
        // 注意：在调用之前需要 unlock thread
        ZR_THREAD_UNLOCK(state);
        global->panicHandlingFunction(state);
        // panic handling function 调用后，应该 abort
        ZR_ABORT();
    } else {
        // 如果没有 panic handling function，尝试抛出异常
        // 如果异常被 catch 捕获，程序可以继续
        // 如果异常没有被捕获，ZrExceptionThrow 内部会 abort
        ZrExceptionThrow(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
        // 不应该到达这里（因为 ZrExceptionThrow 是 noreturn 或会 longjmp）
        ZR_ABORT();
    }
}


void ZrDebugErrorWhenHandlingError(struct SZrState *state) {
    // TODO:
}


void ZrDebugHook(struct SZrState *state, EZrDebugHookEvent event, TUInt32 line, TUInt32 transferStart,
                 TUInt32 transferCount) {
    FZrDebugHook hook = state->debugHook;
    if (hook && state->allowDebugHook) {
        EZrCallStatus mask = ZR_CALL_STATUS_DEBUG_HOOK;
        SZrCallInfo *callInfo = state->callInfoList;
        TZrMemoryOffset top = ZrStackSavePointerAsOffset(state, state->stackTop.valuePointer);
        TZrMemoryOffset callInfoTop = ZrStackSavePointerAsOffset(state, callInfo->functionTop.valuePointer);
        SZrDebugInfo debugInfo;
        debugInfo.event = event;
        debugInfo.currentLine = line;
        debugInfo.callInfo = callInfo;
        if (transferCount != 0) {
            mask |= ZR_CALL_STATUS_CALL_INFO_TRANSFER;
            callInfo->yieldContext.transferStart = transferStart;
            callInfo->yieldContext.transferCount = transferCount;
        }
        if (ZR_CALL_INFO_IS_VM(callInfo) && state->stackTop.valuePointer < callInfo->functionTop.valuePointer) {
            state->stackTop.valuePointer = state->stackTop.valuePointer + ZR_STACK_NATIVE_CALL_MIN;
        }
        state->allowDebugHook = ZR_FALSE;
        callInfo->callStatus |= mask;
        ZR_THREAD_UNLOCK(state);
        hook(state, &debugInfo);
        ZR_THREAD_LOCK(state);

        ZR_ASSERT(!state->allowDebugHook);
        state->allowDebugHook = ZR_TRUE;
        callInfo->functionTop.valuePointer = ZrStackLoadOffsetToPointer(state, callInfoTop);
        state->stackTop.valuePointer = ZrStackLoadOffsetToPointer(state, top);
        callInfo->callStatus &= ~mask;
    }
}

void ZrDebugHookReturn(struct SZrState *state, struct SZrCallInfo *callInfo, TZrSize resultCount) {
    if (state->debugHookSignal & ZR_DEBUG_HOOK_MASK_RETURN) {
        TZrStackValuePointer stackPointer = callInfo->functionTop.valuePointer - resultCount;
        TUInt32 totalArgumentsCount = 0;
        TInt32 transferStart = 0;
        if (ZR_CALL_INFO_IS_VM(callInfo)) {
            SZrTypeValue *functionValue = ZrStackGetValue(callInfo->functionBase.valuePointer);
            SZrFunction *function = (ZR_CAST_VM_CLOSURE(state, functionValue->value.object))->function;
            if (function->hasVariableArguments) {
                totalArgumentsCount = callInfo->context.context.variableArgumentCount + function->parameterCount + 1;
            }
        }
        callInfo->functionBase.valuePointer += totalArgumentsCount;
        transferStart = ZR_CAST_UINT(stackPointer - callInfo->functionBase.valuePointer);
        ZrDebugHook(state, ZR_DEBUG_HOOK_EVENT_RETURN, -1, transferStart, resultCount);
        callInfo->functionBase.valuePointer -= totalArgumentsCount;
    }
    callInfo = callInfo->previous;
    if (ZR_CALL_INFO_IS_VM(callInfo)) {
        SZrTypeValue *functionValue = ZrStackGetValue(callInfo->functionBase.valuePointer);
        SZrFunction *function = (ZR_CAST_VM_CLOSURE(state, functionValue->value.object))->function;
        state->previousProgramCounter =
                ZR_CAST_INT64(callInfo->context.context.programCounter - function->instructionsList) - 1;
    }
}
