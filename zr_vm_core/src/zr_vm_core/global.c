//
// Created by HeJiahui on 2025/6/16.
//
#include "zr_vm_core/global.h"

#include "zr_vm_core/convertion.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/string.h"

SZrGlobalState *ZrGlobalStateNew(FZrAllocator allocator, TZrPtr userAllocationArguments, TUInt64 uniqueNumber) {
    SZrGlobalState *global = allocator(userAllocationArguments, ZR_NULL, 0, sizeof(SZrGlobalState));
    if (global == ZR_NULL) {
        return ZR_NULL;
    }
    global->allocator = allocator;
    global->userAllocationArguments = userAllocationArguments;
    SZrState *newState = ZrStateNew(global);
    global->mainThreadState = newState;
    ZrStateInit(newState, global);
    // todo: main thread cannot yield

    // todo:
    global->logFunction = ZR_NULL;

    // generate seed
    global->hashSeed = ZrHashSeedCreate(global, uniqueNumber);

    // gc
    ZrGarbageCollectorInit(global);

    // init string table
    ZrStringTableInit(global);

    // init global module registry
    ZrValueInitAsNull(&global->loadedModulesRegistry);
    // init global null value
    ZrValueInitAsNull(&global->nullValue);

    // exception
    global->panicHandlingFunction = ZR_NULL;

    for (TUInt64 i = 0; i < ZR_VALUE_TYPE_ENUM_MAX; i++) {
        global->basicTypeObjectPrototype[i] = ZR_NULL;
    }
    // todo: open new state with try-catch

    return global;
}

void ZrGlobalStateFree(SZrGlobalState *global) {
    FZrAllocator allocator = global->allocator;

    ZrStateFree(global, global->mainThreadState);
    // free global at last
    allocator(global->userAllocationArguments, global, sizeof(SZrGlobalState), 0);
}
