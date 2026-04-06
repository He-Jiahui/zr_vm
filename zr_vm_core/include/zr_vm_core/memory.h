//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_VM_CORE_MEMORY_H
#define ZR_VM_CORE_MEMORY_H

#include <string.h>

#include "zr_vm_core/conf.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"

ZR_FORCE_INLINE TZrPtr ZrCore_Memory_Allocate(SZrGlobalState *global, TZrPtr pointer, TZrSize originalSize, TZrSize newSize,
                                        EZrMemoryNativeType type) {
    ZR_ASSERT((pointer != ZR_NULL && originalSize != 0) || newSize != 0);
    return global->allocator(global->userAllocationArguments, pointer, originalSize, newSize, type);
}

ZR_FORCE_INLINE TZrPtr ZrCore_Memory_RawMalloc(SZrGlobalState *global, TZrSize size) {
    return global->allocator(global->userAllocationArguments, ZR_NULL, 0, size, ZR_MEMORY_NATIVE_TYPE_NONE);
}

ZR_FORCE_INLINE TZrPtr ZrCore_Memory_RawMallocWithType(SZrGlobalState *global, TZrSize size, EZrMemoryNativeType type) {
    // type may be helpful for some allocator
    ZR_UNUSED_PARAMETER(type)
    return global->allocator(global->userAllocationArguments, ZR_NULL, 0, size, type);
}

ZR_CORE_API TZrPtr ZrCore_Memory_GcAndMalloc(SZrState *state, EZrMemoryNativeType type, TZrSize size);

ZR_CORE_API TZrPtr ZrCore_Memory_GcMalloc(SZrState *state, EZrMemoryNativeType type, TZrSize size);

ZR_CORE_API TZrPtr ZrCore_Memory_GcReallocate(SZrState *state,
                                              TZrPtr pointer,
                                              TZrSize originalSize,
                                              TZrSize newSize,
                                              EZrMemoryNativeType type);

ZR_FORCE_INLINE void ZrCore_Memory_RawSet(TZrPtr destination, TZrByte byte, TZrSize byteCount) {
    ZR_ASSERT(destination != ZR_NULL && byteCount != 0);
    memset(destination, byte, byteCount);
}

ZR_FORCE_INLINE void ZrCore_Memory_RawFree(SZrGlobalState *global, TZrPtr pointer, TZrSize size) {
    ZR_ASSERT(pointer != ZR_NULL && size != 0);
    global->allocator(global->userAllocationArguments, pointer, size, 0, ZR_MEMORY_NATIVE_TYPE_NONE);
}
ZR_FORCE_INLINE void ZrCore_Memory_RawFreeWithType(SZrGlobalState *global, TZrPtr pointer, TZrSize size,
                                             EZrMemoryNativeType type) {
    ZR_ASSERT(pointer != ZR_NULL && size != 0);
    global->allocator(global->userAllocationArguments, pointer, size, 0, type);
}
#define ZR_MEMORY_RAW_FREE_LIST(GLOBAL, POINTER, SIZE)                                                                 \
    ZrCore_Memory_RawFreeWithType((GLOBAL), (POINTER), (SIZE) * sizeof(*(POINTER)), ZR_MEMORY_NATIVE_TYPE_FUNCTION)

ZR_FORCE_INLINE void ZrCore_Memory_RawCopy(TZrPtr destination, TZrPtr source, TZrSize size) {
    if (size == 0) {
        return;
    }
    ZR_ASSERT(destination != ZR_NULL && source != ZR_NULL);
    memcpy(destination, source, size);
}

ZR_FORCE_INLINE TZrInt32 ZrCore_Memory_RawCompare(TZrPtr destination, TZrPtr source, TZrSize size) {
    if (size == 0) {
        return 0;
    }
    ZR_ASSERT(destination != ZR_NULL && source != ZR_NULL);
    return memcmp(destination, source, size);
}

#endif // ZR_VM_CORE_MEMORY_H
