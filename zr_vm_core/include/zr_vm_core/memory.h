//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_VM_CORE_MEMORY_H
#define ZR_VM_CORE_MEMORY_H

#include <string.h>

#include "zr_vm_core/conf.h"
#include "zr_vm_core/global.h"

ZR_FORCE_INLINE TZrPtr ZrMemoryAllocate(SZrGlobalState *global, TZrPtr pointer, TZrSize originalSize,
                                        TZrSize newSize) {
    ZR_ASSERT((pointer != ZR_NULL && originalSize != 0) || newSize != 0);
    return global->allocator(global->userAllocationArguments, pointer, originalSize, newSize);
}

ZR_FORCE_INLINE TZrPtr ZrMemoryMalloc(SZrGlobalState *global, TZrSize size) {
    return global->allocator(global->userAllocationArguments, ZR_NULL, 0, size);
}

ZR_FORCE_INLINE void ZrMemoryFree(SZrGlobalState *global, TZrPtr pointer, TZrSize size) {
    ZR_ASSERT(pointer != ZR_NULL && size != 0);
    global->allocator(global->userAllocationArguments, pointer, size, 0);
}

ZR_FORCE_INLINE void ZrMemoryCopy(TZrPtr destination, TZrPtr source, TZrSize size) {
    ZR_ASSERT(destination != ZR_NULL && source != ZR_NULL && size != 0);
    memcpy(destination, source, size);
}


#endif //ZR_VM_CORE_MEMORY_H
