//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_VM_CORE_MEMORY_H
#define ZR_VM_CORE_MEMORY_H

#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/conf.h"

typedef TZrPtr (*FZrAlloc) (TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize);

ZR_CORE_API void* ZrMalloc(TZrSize size);

ZR_CORE_API void ZrMemoryCopy(TZrPtr destination, TZrPtr source, TZrSize size);


ZR_CORE_API void ZrMemoryFree(TZrPtr pointer);
#endif //ZR_VM_CORE_MEMORY_H
