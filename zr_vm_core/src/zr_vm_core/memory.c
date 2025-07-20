//
// Created by HeJiahui on 2025/6/16.
//
#include "zr_vm_core/memory.h"

#include <stdlib.h>
#include <string.h>


// todo:
TZrPtr ZrMemoryGcAndMalloc(SZrState *state, EZrValueType type, TZrSize size) {
    ZR_UNUSED_PARAMETER(type);
    SZrGlobalState *global = state->global;
    // global state is initialized judge by null value
    if (ZrGlobalStateIsInitialized(global) && !global->garbageCollector.isImmediateGcFlag) {
        // todo: call full gc
        ZrGarbageCollectorGcFull(state, ZR_TRUE);
        return ZrMemoryRawMalloc(global, size);
    }
    return ZR_NULL;
}

TZrPtr ZrMemoryGcMalloc(SZrState *state, EZrValueType type, TZrSize size) {
    ZR_ASSERT(size > 0);
    SZrGlobalState *global = state->global;
    TZrPtr pointer = ZrMemoryRawMallocWithType(global, size, type);
    if (ZR_UNLIKELY(pointer == ZR_NULL)) {
        // trigger gc and try again
        pointer = ZrMemoryGcAndMalloc(state, type, size);
        if (pointer == ZR_NULL) {
            ZrExceptionThrow(state, ZR_THREAD_STATUS_MEMORY_ERROR);
        }
    }
    global->garbageCollector.gcDebtSize += (TZrMemoryOffset) size;
    return pointer;
}

/*
 *

 */
