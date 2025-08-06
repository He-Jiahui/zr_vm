//
// Created by HeJiahui on 2025/6/18.
//

#include "zr_vm_core/exception.h"
#include <setjmp.h>
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/string.h"

#include <setjmp.h>

#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"

#if defined(__cplusplus) && !defined(ZR_EXCEPTION_WITH_LONG_JUMP)
#define ZR_EXCEPTION_NATIVE_THROW(state, context) throw(context)
#define ZR_EXCEPTION_NATIVE_TRY(state, context, block)                                                                 \
    try {                                                                                                              \
        block                                                                                                          \
    } catch (...) {                                                                                                    \
        if ((context)->status == 0) {                                                                                  \
            (context)->status = -1;                                                                                    \
        }                                                                                                              \
    }
#elif defined(ZR_PLATFORM_UNIX)
#define ZR_EXCEPTION_NATIVE_THROW(state, context) longjmp((context)->jumpBuffer, 1)
#define ZR_EXCEPTION_NATIVE_TRY(state, context, block)                                                                 \
    if (setjmp((context)->jumpBuffer) == 0) {                                                                          \
        block                                                                                                          \
    }
#else
#define ZR_EXCEPTION_NATIVE_THROW(state, context) longjmp((context)->jumpBuffer, 1)
#define ZR_EXCEPTION_NATIVE_TRY(state, context, block)                                                                 \
    if (setjmp((context)->jumpBuffer) == 0) {                                                                          \
        block                                                                                                          \
    }
#endif

EZrThreadStatus ZrExceptionTryRun(SZrState *state, FZrTryFunction tryFunction, TZrPtr arguments) {
    TUInt32 prevNestedNativeCalls = state->nestedNativeCalls;
    SZrExceptionLongJump exceptionLongJump;
    exceptionLongJump.status = ZR_THREAD_STATUS_FINE;
    exceptionLongJump.previous = state->exceptionRecoverPoint;
    state->exceptionRecoverPoint = &exceptionLongJump;
    ZR_EXCEPTION_NATIVE_TRY(state, &exceptionLongJump, { tryFunction(state, arguments); });
    state->exceptionRecoverPoint = exceptionLongJump.previous;
    state->nestedNativeCalls = prevNestedNativeCalls;
    return exceptionLongJump.status;
}

void ZrExceptionThrow(SZrState *state, EZrThreadStatus errorCode) {
    if (state->exceptionRecoverPoint) {
        state->exceptionRecoverPoint->status = errorCode;
        ZR_EXCEPTION_NATIVE_THROW(state, state->exceptionRecoverPoint);
    }
    SZrGlobalState *global = state->global;
    SZrState *mainThreadState = global->mainThreadState;
    // throw in main thread
    if (state != mainThreadState) {
        // not wrapped by try catch block
        errorCode = ZrStateResetThread(state, errorCode);
        state->threadStatus = errorCode;
        if (mainThreadState->exceptionRecoverPoint != ZR_NULL) {
            // set error object to main thread
            ZrStackCopyValue(mainThreadState, mainThreadState->stackTop.valuePointer,
                             ZrStackGetValue(state->stackTop.valuePointer - 1));
            mainThreadState->stackTop.valuePointer++;
            ZrExceptionThrow(mainThreadState, errorCode);
        }
    } else
    // if no catch block and throw in main thread
    {
        if (global->panicHandlingFunction != ZR_NULL) {
            ZR_THREAD_UNLOCK(state);
            global->panicHandlingFunction(state);
        }
        ZR_ABORT();
    }
}

EZrThreadStatus ZrExceptionTryStop(SZrState *state, TZrMemoryOffset level, EZrThreadStatus status) {
    ZR_TODO_PARAMETER(state);
    ZR_TODO_PARAMETER(level);
    ZR_TODO_PARAMETER(status);


    // SZrCallInfo *previousCallInfo = state->callInfoList;
    // TUInt8 previousAllowHook = state->allowDebugHook;
    // // 持续关闭旧的上值直到不再有错误
    // for (;;) {
    //     ZR_ASSERT(ZR_FALSE);
    //     // todo: ldo.c luaD_closeprotected
    // }
    return ZR_THREAD_STATUS_FINE;
}

void ZrExceptionMarkError(SZrState *state, EZrThreadStatus errorCode, TZrStackValuePointer previousTop) {
    switch (errorCode) {
        case ZR_THREAD_STATUS_FINE: {
            ZrValueResetAsNull(ZrStackGetValue(previousTop));
        } break;
        case ZR_THREAD_STATUS_MEMORY_ERROR: {
            ZrStackSetRawObjectValue(state, previousTop,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(state->global->memoryErrorMessage));
        } break;
        default: {
            ZR_ASSERT(ZrExceptionIsStausError(errorCode));
            ZrStackCopyValue(state, previousTop, &(state->stackTop.valuePointer - 1)->value);
        } break;
    }
    state->stackTop.valuePointer = previousTop - 1;
}
