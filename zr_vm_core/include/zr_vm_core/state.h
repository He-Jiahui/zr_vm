//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_VM_CORE_STATE_H
#define ZR_VM_CORE_STATE_H
#include "zr_vm_core/conf.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/debug.h"

struct SZrGlobalState;

struct ZR_STRUCT_ALIGN SZrState {
    SZrRawObject super;
    // reverse pointer to global
    struct SZrGlobalState *global;

    SZrRawObject *gcList;
    // thread management
    EZrThreadStatus threadStatus;
    TZrMemoryOffset previousProgramCounter;


    SZrCallInfo *callInfoList;
    TUInt32 callInfoListLength;
    SZrCallInfo baseCallInfo;


    TZrStackObject stackTop;
    TZrStackObject stackBottom;
    TZrStackObject stackBase;

    TZrStackObject waitToReleaseList;
    // closures
    struct SZrState *threadWithAliveClosures;
    SZrClosureValue *aliveClosureValueList;


    // for exceptions
    TUInt32 nestedNativeCalls;
    SZrExceptionLongJump *exceptionRecoverPoint;
    TZrMemoryOffset exceptionHandlingFunctionOffset;


    // debug
    TBool allowDebugHook;
    TUInt32 baseDebugHookCount;
    TUInt32 debugHookCount;
    volatile FZrDebugHook debugHook;
    volatile TZrDebugSignal debugHookSignal;
};

typedef struct SZrState SZrState;


ZR_CORE_API SZrState *ZrStateNew(struct SZrGlobalState *global);

ZR_CORE_API void ZrStateInit(SZrState *state, struct SZrGlobalState *global);

ZR_CORE_API void ZrStateFree(struct SZrGlobalState *global, SZrState *state);

ZR_CORE_API TInt32 ZrStateResetThread(SZrState *state, EZrThreadStatus status);

ZR_FORCE_INLINE TZrSize ZrStateStackGetSize(SZrState *state) {
    return state->stackTop.valuePointer - state->stackBottom.valuePointer;
}

ZR_CORE_API TBool ZrStateStackRealloc(SZrState *state, TUInt64 newSize, TBool throwError);
#endif //ZR_VM_CORE_STATE_H
