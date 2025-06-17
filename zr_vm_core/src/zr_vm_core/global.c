//
// Created by HeJiahui on 2025/6/16.
//
#include "zr_vm_core/global.h"

SZrGlobalState *ZrGlobalStateNew(FZrAlloc alloc, TZrPtr memoryPrivateData) {
    SZrGlobalState *global = alloc(memoryPrivateData, ZR_NULL, 0, sizeof(SZrGlobalState));
    global->mainThreadState = ZrStateNew(global);

    return global;
}

void ZrGlobalStateFree(SZrGlobalState *global) {
    FZrAlloc alloc = global->alloc;

    ZrStateFree(global, global->mainThreadState);
    // free global at last
    alloc(global->userAllocArguments, global, sizeof(SZrGlobalState), 0);
}
