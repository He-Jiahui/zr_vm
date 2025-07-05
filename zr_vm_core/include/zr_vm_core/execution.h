//
// Created by HeJiahui on 2025/6/15.
//

#ifndef ZR_VM_CORE_EXECUTION_H
#define ZR_VM_CORE_EXECUTION_H
#include "zr_vm_core/conf.h"
struct SZrState;
struct SZrCallInfo;
ZR_CORE_API void ZrExecute(struct SZrState *state, struct SZrCallInfo *callInfo);


#endif //ZR_VM_CORE_EXECUTION_H
