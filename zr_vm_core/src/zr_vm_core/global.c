//
// Created by HeJiahui on 2025/6/16.
//
#include "zr_vm_core/global.h"

#include "zr_vm_core/convertion.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

static void ZrGlobalStatePanic(SZrState *state) {
    // todo
}

SZrGlobalState *ZrGlobalStateNew(FZrAllocator allocator, TZrPtr userAllocationArguments, TUInt64 uniqueNumber,
                                 SZrCallbackGlobal *callbacks) {
    SZrGlobalState *global = allocator(userAllocationArguments, ZR_NULL, 0, sizeof(SZrGlobalState),
                                       ZR_VALUE_TYPE_VM_MEMORY);
    if (global == ZR_NULL) {
        return ZR_NULL;
    }
    // when create and init global state, we make the global is not valid
    global->isValid = ZR_FALSE;
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
    ZrStringTableNew(global);

    // init global module registry
    ZrValueResetAsNull(&global->loadedModulesRegistry);
    // init global null value
    ZrValueResetAsNull(&global->nullValue);

    // exception
    global->panicHandlingFunction = ZR_NULL;

    // reset basic type object prototype
    for (TUInt64 i = 0; i < ZR_VALUE_TYPE_ENUM_MAX; i++) {
        global->basicTypeObjectPrototype[i] = ZR_NULL;
    }
    // write callbacks to  global
    if (callbacks != ZR_NULL) {
        global->callbacks = *callbacks;
    } else {
        ZrMemoryRawSet(&global->callbacks, 0, sizeof(SZrCallbackGlobal));
    }
    // launch new state with try-catch
    if (ZrExceptionTryRun(newState, ZrStateMainThreadLaunch, ZR_NULL) != ZR_THREAD_STATUS_FINE) {
        ZrStateExit(newState);
        ZrGlobalStateFree(global);
        global = ZR_NULL;
        return ZR_NULL;
    }
    // set warning function

    // set panic function
    return global;
}

void ZrGlobalStateInitRegistry(SZrState *state, SZrGlobalState *global) {
    SZrObject *object = ZrObjectNew(state, ZR_NULL);
    ZrValueInitAsRawObject(state, &global->loadedModulesRegistry, ZR_CAST_RAW_OBJECT(object));
    // todo: load state value
    // todo: load global value
}


void ZrGlobalStateFree(SZrGlobalState *global) {
    FZrAllocator allocator = global->allocator;

    ZrStateFree(global, global->mainThreadState);
    // free global at last
    allocator(global->userAllocationArguments, global, sizeof(SZrGlobalState), 0, ZR_VALUE_TYPE_VM_MEMORY);
}
