//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_HASH_H
#define ZR_VM_CORE_HASH_H
#include "zr_vm_core/conf.h"
#include "zr_vm_core/value.h"

struct ZR_STRUCT_ALIGN SZrHashNode {
    SZrTypeValue key;
    SZrTypeValue value;
    TZrMemoryOffset nextCollided;
};

typedef struct SZrHashNode SZrHashNode;


#endif //ZR_VM_CORE_HASH_H
