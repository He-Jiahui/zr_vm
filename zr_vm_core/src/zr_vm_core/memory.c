//
// Created by HeJiahui on 2025/6/16.
//
#include "zr_vm_core/memory.h"

#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/gc.h"


// todo:
TZrPtr ZrCore_Memory_GcAndMalloc(SZrState *state, EZrMemoryNativeType type, TZrSize size) {
    ZR_UNUSED_PARAMETER(type);
    SZrGlobalState *global = state->global;
    // global state is initialized judge by null value
    if (ZrCore_GlobalState_IsInitialized(global) && !global->garbageCollector->isImmediateGcFlag) {
        // todo: call full gc
        ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
        return ZrCore_Memory_RawMallocWithType(global, size, type);
    }
    return ZR_NULL;
}

static TZrPtr zr_core_memory_gc_and_reallocate(SZrState *state,
                                               TZrPtr pointer,
                                               TZrSize originalSize,
                                               TZrSize newSize,
                                               EZrMemoryNativeType type) {
    SZrGlobalState *global = state->global;

    if (ZrCore_GlobalState_IsInitialized(global) && !global->garbageCollector->isImmediateGcFlag) {
        ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
        return ZrCore_Memory_Allocate(global, pointer, originalSize, newSize, type);
    }
    return ZR_NULL;
}

TZrPtr ZrCore_Memory_GcMalloc(SZrState *state, EZrMemoryNativeType type, TZrSize size) {
    ZR_ASSERT(size > 0);
    SZrGlobalState *global = state->global;
    TZrPtr pointer = ZrCore_Memory_RawMallocWithType(global, size, type);
    if (ZR_UNLIKELY(pointer == ZR_NULL)) {
        // trigger gc and try again
        pointer = ZrCore_Memory_GcAndMalloc(state, type, size);
        if (pointer == ZR_NULL) {
            ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
        }
    }
    global->garbageCollector->gcDebtSize += (TZrMemoryOffset) size;
    return pointer;
}

TZrPtr ZrCore_Memory_GcReallocate(SZrState *state,
                                  TZrPtr pointer,
                                  TZrSize originalSize,
                                  TZrSize newSize,
                                  EZrMemoryNativeType type) {
    SZrGlobalState *global;
    TZrPtr result;

    ZR_ASSERT(state != ZR_NULL);
    global = state->global;
    result = ZrCore_Memory_Allocate(global, pointer, originalSize, newSize, type);
    if (ZR_UNLIKELY(result == ZR_NULL && newSize > 0)) {
        result = zr_core_memory_gc_and_reallocate(state, pointer, originalSize, newSize, type);
        if (result == ZR_NULL) {
            ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
        }
    }
    if (result != ZR_NULL && newSize > originalSize) {
        global->garbageCollector->gcDebtSize += (TZrMemoryOffset)(newSize - originalSize);
    }
    return result;
}

/*
 *

 */
