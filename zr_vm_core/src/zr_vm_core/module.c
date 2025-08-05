//
// Created by HeJiahui on 2025/8/6.
//
#include "zr_vm_core/module.h"

SZrObject *ZrModuleCreateFromSource(struct SZrState *state, SZrIoSource *source) {
    ZR_ASSERT(source != ZR_NULL);
    SZrObject *newModule = ZrObjectNew(state, ZR_NULL);
    // module has
}
