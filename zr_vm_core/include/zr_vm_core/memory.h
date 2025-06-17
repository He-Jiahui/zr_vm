//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_VM_CORE_MEMORY_H
#define ZR_VM_CORE_MEMORY_H

#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/conf.h"
#include "zr_vm_core/global.h"

ZR_FORCE_INLINE void *ZrAlloc(SZrGlobalState *global, TZrPtr pointer, TZrSize originalSize,
                              TZrSize newSize) {
    return global->alloc(global->userAllocArguments, pointer, originalSize, newSize);
}

ZR_FORCE_INLINE void *ZrMalloc(SZrGlobalState *global, TZrSize size) {
    return global->alloc(global->userAllocArguments, ZR_NULL, 0, size);
}

ZR_CORE_API void ZrMemoryCopy(TZrPtr destination, TZrPtr source, TZrSize size);


ZR_CORE_API void ZrMemoryFree(TZrPtr pointer);
#endif //ZR_VM_CORE_MEMORY_H
