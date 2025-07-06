//
// Created by HeJiahui on 2025/7/6.
//

#ifndef ZR_VM_CORE_IO_H
#define ZR_VM_CORE_IO_H

#include "zr_vm_core/conf.h"

struct SZrState;

typedef TBytePtr (*FZrIoRead)(struct SZrState *state, TZrPtr customData, TZrSize *size);

typedef EZrThreadStatus (*FZrIoWrite)(struct SZrState *state, TBytePtr buffer, TZrSize size, TZrPtr customData);

struct SZrIo {
    struct SZrState *state;
    FZrIoRead read;
    TZrSize remained;
    TBytePtr pointer;
    TZrPtr customData;
};

typedef struct SZrIo SZrIo;

ZR_CORE_API void ZrIoInit(struct SZrState *state, SZrIo *io, FZrIoRead read, TZrPtr customData);

ZR_CORE_API TZrSize ZrIoRead(SZrIo *io, TBytePtr buffer, TZrSize size);

#endif //ZR_VM_CORE_IO_H
