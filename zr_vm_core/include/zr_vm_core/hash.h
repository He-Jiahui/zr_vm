//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_HASH_H
#define ZR_VM_CORE_HASH_H
#include "zr_vm_core/conf.h"
#include "zr_vm_core/value.h"

struct SZrGlobalState;

struct ZR_STRUCT_ALIGN SZrHashNode {
    SZrTypeValue key;
    SZrTypeValue value;
    TZrMemoryOffset nextCollided;
};

typedef struct SZrHashNode SZrHashNode;


ZR_CORE_API TUInt64 ZrHashSeedCreate(struct SZrGlobalState *global, TUInt64 uniqueNumber);

ZR_CORE_API TUInt64 ZrHashCreate(struct SZrGlobalState *global, TNativeString string, TZrSize length);


#endif //ZR_VM_CORE_HASH_H
