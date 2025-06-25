//
// Created by HeJiahui on 2025/6/20.
//

#ifndef ZR_VM_CORE_LOG_H
#define ZR_VM_CORE_LOG_H
#include "zr_vm_core/conf.h"

struct SZrState;

typedef void (*FZrLog)(struct SZrState *state, EZrLogLevel level, TNativeString message);


ZR_CORE_API void ZrLogError(struct SZrState *state, TNativeString format, ...);

#endif //ZR_VM_CORE_LOG_H
