//
// Created by HeJiahui on 2025/6/20.
//

#ifndef ZR_VM_CORE_LOG_H
#define ZR_VM_CORE_LOG_H
#include "zr_vm_core/conf.h"

struct SZrState;

typedef void (*FZrLog)(struct SZrState *state, EZrLogLevel level, TZrNativeString message);


ZR_CORE_API void ZrCore_Log_Error(struct SZrState *state, TZrNativeString format, ...);

#endif //ZR_VM_CORE_LOG_H
