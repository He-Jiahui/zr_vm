//
// Created by HeJiahui on 2025/7/6.
//

#include "zr_vm_core/io.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"

static TBool ZrIoRefill(SZrIo *io) {
    SZrState *state = io->state;
    TZrSize readSize = 0;
    ZR_THREAD_UNLOCK(state);
    TBytePtr buffer = io->read(state, io->customData, &readSize);
    ZR_THREAD_LOCK(state);
    if (buffer == ZR_NULL || readSize == 0) {
        return ZR_FALSE;
    }
    io->remained = readSize;
    io->pointer = buffer;
    return ZR_TRUE;
}

static TInt32 ZrIoReadChar(SZrIo *io) {
    if (io->remained == 0) {
        if (!ZrIoRefill(io)) {
            // EOF = -1
            return ZR_IO_EOF;
        }
    }
    // BYTE > 0 && BYTE < 256
    TInt32 c = *io->pointer;
    io->remained--;
    io->pointer++;
    return c;
}

static ZR_FORCE_INLINE TZrSize ZrIoReadSize(SZrIo *io) {
    TZrSize size;
    ZrIoRead(io, (TBytePtr) &size, sizeof(size));
    return size;
}


static TZrString *ZrIoReadStringWithLength(SZrIo *io) {
    SZrGlobalState *global = io->state->global;
    TZrSize length = ZrIoReadSize(io);
    TNativeString nativeString = ZrMemoryRawMallocWithType(global, ZR_VALUE_TYPE_STRING, length);
    ZrIoRead(io, (TBytePtr) nativeString, length);
    TZrString *string = ZrStringCreate(io->state, nativeString, length);
    ZrMemoryRawFree(global, nativeString, length);
    return string;
}


void ZrIoInit(SZrState *state, SZrIo *io, FZrIoRead read, TZrPtr customData) {
    io->state = state;
    io->read = read;
    io->customData = customData;
    io->pointer = ZR_NULL;
    io->remained = 0;
}

TZrSize ZrIoRead(SZrIo *io, TBytePtr buffer, TZrSize size) {
    while (size > 0) {
        if (io->remained == 0) {
            if (!ZrIoRefill(io)) {
                return size;
            }
        }
        TZrSize read = (size <= io->remained) ? size : io->remained;
        ZrMemoryRawCopy(buffer, io->pointer, read);
        io->remained -= read;
        io->pointer += read;
        buffer += read;
        size -= read;
    }
    return 0;
}
