//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_VM_CORE_STATE_H
#define ZR_VM_CORE_STATE_H
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/conf.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/exception.h"

struct SZrGlobalState;

struct SZrState {
    ZR_GC_HEADER
    // reverse pointer to global
    struct SZrGlobalState *global;
    // thread management
    EZrThreadStatus threadStatus;
    TUInt8 allowHook;

    SZrCallInfo *currentCallInfo;
    SZrCallInfo baseCallInfo;


    TZrStackIndicator stackTop;
    TZrStackIndicator stackBottom;
    TZrStackIndicator stackBase;

    TZrStackIndicator waitToReleaseList;
    // for exceptions
    TUInt32 nestedNativeCalls;
    SZrExceptionLongJump *exceptionRecoverPoint;

    TUInt64 lastProgramCounter;
};

typedef struct SZrState SZrState;


ZR_CORE_API SZrState *ZrStateNew(struct SZrGlobalState *global);

ZR_CORE_API void ZrStateFree(struct SZrGlobalState *global, SZrState *state);

ZR_CORE_API TInt32 ZrStateResetThread(SZrState *state, EZrThreadStatus status);

ZR_FORCE_INLINE TZrSize ZrStateStackGetSize(SZrState *state) {
    return state->stackTop.valuePointer - state->stackBottom.valuePointer;
}

ZR_CORE_API TBool ZrStateStackRealloc(SZrState *state, TUInt64 newSize, TBool throwError);
#endif //ZR_VM_CORE_STATE_H
