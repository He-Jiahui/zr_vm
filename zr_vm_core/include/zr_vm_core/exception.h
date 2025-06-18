//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_VM_CORE_EXCEPTION_H
#define ZR_VM_CORE_EXCEPTION_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/stack.h"
struct SZrState;

typedef void (*FZrTryFunction)(struct SZrState *state, void *userData);

struct ZR_STRUCT_ALIGN SZrExceptionLongJump {
    struct SZrExceptionLongJump *previous;
    TZrExceptionLongJump jumpBuffer;
    volatile EZrThreadStatus status;
};

typedef struct SZrExceptionLongJump SZrExceptionLongJump;

ZR_CORE_API TInt32 ZrExceptionTryRun(struct SZrState *state, FZrTryFunction tryFunction, void *userData);

ZR_CORE_API void ZrExceptionThrow(struct SZrState *state, EZrThreadStatus errorCode);

ZR_CORE_API EZrThreadStatus ZrExceptionTryStop(struct SZrState *state, TZrMemoryOffset level, EZrThreadStatus status);

ZR_CORE_API void ZrExceptionMarkError(struct SZrState *state, EZrThreadStatus errorCode, TZrStackPointer previousTop);
#endif //ZR_VM_CORE_EXCEPTION_H
