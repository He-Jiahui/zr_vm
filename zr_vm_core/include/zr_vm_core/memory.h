//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_VM_CORE_MEMORY_H
#define ZR_VM_CORE_MEMORY_H

#include "zr_vm_common/zr_type_conf.h"


typedef TZrPtr (*FZrAlloc) (TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize);


#endif //ZR_VM_CORE_MEMORY_H
