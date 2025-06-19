//
// Created by HeJiahui on 2025/6/15.
//

#ifndef ZR_VM_CORE_STRING_H
#define ZR_VM_CORE_STRING_H

#include "zr_vm_core/conf.h"
struct SZrGlobalState;

ZR_CORE_API void ZrStringTableInit(struct SZrGlobalState *global);

ZR_CORE_API TZrString *ZrStringCreate(struct SZrGlobalState *global, TNativeString string, TZrSize length);
#endif //ZR_VM_CORE_STRING_H
