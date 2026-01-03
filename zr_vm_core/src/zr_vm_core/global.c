//
// Created by HeJiahui on 2025/6/16.
//
#include "zr_vm_core/global.h"

#include "zr_vm_core/conversion.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

static void ZrGlobalStatePanic(SZrState *state) {
    // todo
}

SZrGlobalState *ZrGlobalStateNew(FZrAllocator allocator, TZrPtr userAllocationArguments, TUInt64 uniqueNumber,
                                 SZrCallbackGlobal *callbacks) {
    SZrGlobalState *global =
            allocator(userAllocationArguments, ZR_NULL, 0, sizeof(SZrGlobalState), ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (global == ZR_NULL) {
        return ZR_NULL;
    }
    // when create and init global state, we make the global is not valid
    global->isValid = ZR_FALSE;
    global->allocator = allocator;
    global->userAllocationArguments = userAllocationArguments;
    SZrState *newState = ZrStateNew(global);
    global->mainThreadState = newState;
    global->threadWithStackClosures = ZR_NULL;
    ZrStateInit(newState, global);
    // todo: main thread cannot yield

    // todo:
    global->logFunction = ZR_NULL;

    // generate seed
    global->hashSeed = ZrHashSeedCreate(global, uniqueNumber);

    // gc
    ZrGarbageCollectorNew(global);

    // io
    global->sourceLoader = ZR_NULL;

    // init string table
    ZrStringTableNew(global);

    // init global module registry
    ZrValueResetAsNull(&global->loadedModulesRegistry);
    // init global null value
    ZrValueResetAsNull(&global->nullValue);

    // exception
    global->panicHandlingFunction = ZR_NULL;

    // injected data
    global->userData = ZR_NULL;

    // loader
    global->sourceLoader = ZR_NULL;

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

// 初始化基本类型对象原型
static void ZrGlobalStateInitBasicTypeObjectPrototypes(SZrState *state, SZrGlobalState *global) {
    // 为每个值类型创建 ObjectPrototype 对象
    for (TZrSize i = 0; i < ZR_VALUE_TYPE_ENUM_MAX; i++) {
        // 创建 ObjectPrototype 对象（ZrObjectNewCustomized 已经设置了 internalType 并调用了 ZrHashSetConstruct）
        SZrObject *objectBase = ZrObjectNewCustomized(state, sizeof(SZrObjectPrototype), ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE);
        if (objectBase == ZR_NULL) {
            continue;
        }
        
        SZrObjectPrototype *prototype = (SZrObjectPrototype *)objectBase;
        
        // 初始化哈希集（ZrObjectNewCustomized 只调用了 Construct，需要调用 Init）
        ZrHashSetInit(state, &prototype->super.nodeMap, ZR_OBJECT_TABLE_INIT_SIZE_LOG2);
        
        // 初始化 ObjectPrototype 特定字段
        prototype->name = ZR_NULL;
        prototype->type = ZR_OBJECT_PROTOTYPE_TYPE_NATIVE;
        prototype->superPrototype = ZR_NULL;
        
        // 初始化 metaTable
        ZrMetaTableConstruct(&prototype->metaTable);
        
        // 将原型存储到全局数组中
        global->basicTypeObjectPrototype[i] = prototype;
        
        // 标记为永久对象（避免被 GC 回收）
        ZrRawObjectMarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    }
}

void ZrGlobalStateInitRegistry(SZrState *state, SZrGlobalState *global) {
    SZrObject *object = ZrObjectNew(state, ZR_NULL);
    ZrValueInitAsRawObject(state, &global->loadedModulesRegistry, ZR_CAST_RAW_OBJECT(object));
    
    // 初始化基本类型对象原型
    ZrGlobalStateInitBasicTypeObjectPrototypes(state, global);
    
    // todo: load state value
    // todo: load global value
}


void ZrGlobalStateFree(SZrGlobalState *global) {
    FZrAllocator allocator = global->allocator;

    ZrStringTableFree(global, global->stringTable);
    global->stringTable = ZR_NULL;

    ZrGarbageCollectorFree(global, global->garbageCollector);
    global->garbageCollector = ZR_NULL;

    ZrStateFree(global, global->mainThreadState);
    // free global at last
    allocator(global->userAllocationArguments, global, sizeof(SZrGlobalState), 0, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
}
