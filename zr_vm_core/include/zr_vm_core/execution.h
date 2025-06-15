//
// Created by HeJiahui on 2025/6/15.
//

#ifndef ZR_VM_CORE_EXECUTION_H
#define ZR_VM_CORE_EXECUTION_H
#include "zr_vm_core/state.h"
#include "zr_vm_core/call_info.h"

ZR_CORE_API void ZrExecute(SZrState* state, SZrCallInfo* callInfo);

#endif //ZR_VM_CORE_EXECUTION_H
