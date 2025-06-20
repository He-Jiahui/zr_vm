//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_VM_CORE_EXCEPTION_H
#define ZR_VM_CORE_EXCEPTION_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/stack.h"
struct SZrState;

typedef void (*FZrTryFunction)(struct SZrState *state, TZrPtr arguments);

typedef void (*FZrPanicHandlingFunction)(struct SZrState *state);

struct SZrExceptionLongJump {
    TZrExceptionLongJump jumpBuffer;
    struct SZrExceptionLongJump *previous;
    volatile EZrThreadStatus status;
};

typedef struct SZrExceptionLongJump SZrExceptionLongJump;

ZR_CORE_API EZrThreadStatus ZrExceptionTryRun(struct SZrState *state, FZrTryFunction tryFunction, TZrPtr arguments);

ZR_CORE_API void ZrExceptionThrow(struct SZrState *state, EZrThreadStatus errorCode);

ZR_CORE_API EZrThreadStatus ZrExceptionTryStop(struct SZrState *state, TZrMemoryOffset level, EZrThreadStatus status);

ZR_CORE_API void ZrExceptionMarkError(struct SZrState *state, EZrThreadStatus errorCode,
                                      TZrStackValuePointer previousTop);
#endif //ZR_VM_CORE_EXCEPTION_H
