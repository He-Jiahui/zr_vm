//
// Created by HeJiahui on 2025/6/5.
//
#include "zr_vm_core/state.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"

SZrState *ZrStateNew(SZrGlobalState *global) {
    // FZrAlloc alloc = global->alloc;
    SZrState *newState = ZrAlloc(global, global->memoryPrivateData, NULL, 0, sizeof(SZrState));

    return newState;
}

