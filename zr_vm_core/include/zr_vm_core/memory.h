//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_VM_CORE_MEMORY_H
#define ZR_VM_CORE_MEMORY_H

#include <string.h>

#include "zr_vm_core/conf.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"

ZR_FORCE_INLINE TZrPtr ZrMemoryAllocate(SZrGlobalState *global, TZrPtr pointer, TZrSize originalSize, TZrSize newSize,
                                        EZrMemoryNativeType type) {
    ZR_ASSERT((pointer != ZR_NULL && originalSize != 0) || newSize != 0);
    return global->allocator(global->userAllocationArguments, pointer, originalSize, newSize, type);
}

ZR_FORCE_INLINE TZrPtr ZrMemoryRawMalloc(SZrGlobalState *global, TZrSize size) {
    return global->allocator(global->userAllocationArguments, ZR_NULL, 0, size, ZR_MEMORY_NATIVE_TYPE_NONE);
}

ZR_FORCE_INLINE TZrPtr ZrMemoryRawMallocWithType(SZrGlobalState *global, TZrSize size, EZrMemoryNativeType type) {
    // type may be helpful for some allocator
    ZR_UNUSED_PARAMETER(type)
    return global->allocator(global->userAllocationArguments, ZR_NULL, 0, size, type);
}

ZR_CORE_API TZrPtr ZrMemoryGcAndMalloc(SZrState *state, EZrMemoryNativeType type, TZrSize size);

ZR_CORE_API TZrPtr ZrMemoryGcMalloc(SZrState *state, EZrMemoryNativeType type, TZrSize size);

ZR_FORCE_INLINE void ZrMemoryRawSet(TZrPtr destination, TByte byte, TZrSize byteCount) {
    ZR_ASSERT(destination != ZR_NULL && byteCount != 0);
    memset(destination, byte, byteCount);
}

ZR_FORCE_INLINE void ZrMemoryRawFree(SZrGlobalState *global, TZrPtr pointer, TZrSize size) {
    ZR_ASSERT(pointer != ZR_NULL && size != 0);
    global->allocator(global->userAllocationArguments, pointer, size, 0, ZR_MEMORY_NATIVE_TYPE_NONE);
}
ZR_FORCE_INLINE void ZrMemoryRawFreeWithType(SZrGlobalState *global, TZrPtr pointer, TZrSize size,
                                             EZrMemoryNativeType type) {
    ZR_ASSERT(pointer != ZR_NULL && size != 0);
    global->allocator(global->userAllocationArguments, pointer, size, 0, type);
}
#define ZR_MEMORY_RAW_FREE_LIST(GLOBAL, POINTER, SIZE)                                                                 \
    ZrMemoryRawFreeWithType((GLOBAL), (POINTER), (SIZE) * sizeof(*(POINTER)), ZR_MEMORY_NATIVE_TYPE_FUNCTION)

ZR_FORCE_INLINE void ZrMemoryRawCopy(TZrPtr destination, TZrPtr source, TZrSize size) {
    ZR_ASSERT(destination != ZR_NULL && source != ZR_NULL && size != 0);
    memcpy(destination, source, size);
}

ZR_FORCE_INLINE TInt32 ZrMemoryRawCompare(TZrPtr destination, TZrPtr source, TZrSize size) {
    ZR_ASSERT(destination != ZR_NULL && source != ZR_NULL && size != 0);
    return memcmp(destination, source, size);
}

#endif // ZR_VM_CORE_MEMORY_H
