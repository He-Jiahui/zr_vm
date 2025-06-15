//
// Created by HeJiahui on 2025/6/15.
//

#ifndef ZR_VM_CORE_STRING_H
#define ZR_VM_CORE_STRING_H

#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/conf.h"
#define ZR_VM_CONSTANT_STRING_HASH_SEED (0x1234987612345678ULL)
ZR_CORE_API TUInt64 ZrHashString(TRawString string, TZrSize length, TUInt64 seed);

ZR_CORE_API TConstantString* ZrCreateString(TRawString string, TZrSize length);
#endif //ZR_VM_CORE_STRING_H
