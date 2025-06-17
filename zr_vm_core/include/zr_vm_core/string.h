//
// Created by HeJiahui on 2025/6/15.
//

#ifndef ZR_VM_CORE_STRING_H
#define ZR_VM_CORE_STRING_H

#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/conf.h"
struct SZrGlobalState;

ZR_CORE_API TUInt64 ZrStringHash(struct SZrGlobalState *global, TRawString string, TZrSize length);

ZR_CORE_API TZrConstantString *ZrStringCreate(struct SZrGlobalState *global, TRawString string, TZrSize length);
#endif //ZR_VM_CORE_STRING_H
