//
// Created by HeJiahui on 2025/6/18.
//

#include <setjmp.h>
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/exception.h"

#include <setjmp.h>

#include "zr_vm_core/state.h"
#include "zr_vm_core/global.h"

#if defined(__cplusplus) && !defined(ZR_EXCEPTION_WITH_LONG_JUMP)
#define ZR_EXCEPTION_THROW(state, context) throw(context)
#define ZR_EXCEPTION_TRY(state, context, block) \
try { \
block \
} catch(...) {\
if((context)->status == 0) {\
(context)->status = -1;\
}\
}
#elif defined(ZR_PLATFORM_UNIX)
#define ZR_EXCEPTION_THROW(state, context) longjmp((context)->jumpBuffer, 1)
#define ZR_EXCEPTION_TRY(state, context, block) \
if(setjmp((context)->jumpBuffer) == 0){\
block \
}
#else
#define ZR_EXCEPTION_THROW(state, context) longjmp((context)->jumpBuffer, 1)
#define ZR_EXCEPTION_TRY(state, context, block) \
if(setjmp((context)->jumpBuffer) == 0){\
block \
}
#endif

TInt32 ZrExceptionTryRun(SZrState *state, FZrTryFunction tryFunction, void *userData) {
    TUInt32 prevNestedNativeCalls = state->nestedNativeCalls;
    SZrExceptionLongJump exceptionLongJump;
    exceptionLongJump.status = ZR_THREAD_STATUS_FINE;
    exceptionLongJump.previous = state->exceptionRecoverPoint;
    state->exceptionRecoverPoint = &exceptionLongJump;
    ZR_EXCEPTION_TRY(state, &exceptionLongJump, {
                     tryFunction(state, userData);
                     });
    state->exceptionRecoverPoint = exceptionLongJump.previous;
    state->nestedNativeCalls = prevNestedNativeCalls;
    return exceptionLongJump.status;
}

void ZrExceptionThrow(SZrState *state, EZrThreadStatus errorCode) {
    if (state->exceptionRecoverPoint) {
        state->exceptionRecoverPoint->status = errorCode;
        ZR_EXCEPTION_THROW(state, state->exceptionRecoverPoint);
    } else {
        // SZrGlobalState *global = state->global;
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

void ZrExceptionMarkError(SZrState *state, EZrThreadStatus errorCode, TZrStackPointer previousTop) {
    switch (errorCode) {
        case ZR_THREAD_STATUS_FINE: {
            previousTop->value.type = ZR_VALUE_TYPE_NULL;
        }
        break;
        case ZR_THREAD_STATUS_MEMORY_ERROR: {
            // todo: luaD_seterrorobj
        }
        break;
        default: {
            // todo setobjs2s
        }
        break;
    }
    state->stackTop.valuePointer = previousTop + 1;
}
