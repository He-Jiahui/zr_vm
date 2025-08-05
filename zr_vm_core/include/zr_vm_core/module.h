//
// Created by HeJiahui on 2025/8/6.
//

#ifndef ZR_VM_CORE_MODULE_H
#define ZR_VM_CORE_MODULE_H
#include "zr_vm_core/conf.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/object.h"

struct SZrState;
struct SZrGlobalState;

ZR_CORE_API SZrObject *ZrModuleCreateFromSource(struct SZrState *state, SZrIoSource *source);
#endif // ZR_VM_CORE_MODULE_H
