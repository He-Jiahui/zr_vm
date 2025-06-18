//
// Created by HeJiahui on 2025/6/16.
//
#include "zr_vm_core/global.h"

#include "zr_vm_core/convertion.h"

SZrGlobalState *ZrGlobalStateNew(FZrAllocator allocator, TZrPtr userAllocationArguments) {
    SZrGlobalState *global = allocator(userAllocationArguments, ZR_NULL, 0, sizeof(SZrGlobalState));
    if (global == ZR_NULL) {
        return ZR_NULL;
    }
    global->allocator = allocator;
    global->userAllocationArguments = userAllocationArguments;
    SZrState *newState = ZrStateNew(global);
    global->mainThreadState = newState;
    global->gcObjectList = ZR_CAST_RAW_OBJECT(newState);
    // todo: main thread cannot yield

    // todo:
    return global;
}

void ZrGlobalStateFree(SZrGlobalState *global) {
    FZrAllocator allocator = global->allocator;

    ZrStateFree(global, global->mainThreadState);
    // free global at last
    allocator(global->userAllocationArguments, global, sizeof(SZrGlobalState), 0);
}
