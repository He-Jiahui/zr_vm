//
// Created by HeJiahui on 2025/6/16.
//

#ifndef ZR_VM_CORE_GLOBAL_H
#define ZR_VM_CORE_GLOBAL_H
#include "zr_vm_core/conf.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/log.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/object.h"

/**
 * 全局API字符串缓存配置参数
 * ZR_GLOBAL_API_STR_CACHE_N 定义主缓存桶数量（质数以优化散列）
 * ZR_GLOBAL_API_STR_CACHE_M 定义每个桶的二级缓存深度
 * 这组参数共同控制字符串缓存的存储结构和性能特性
 */
#define ZR_GLOBAL_API_STR_CACHE_N 193
#define ZR_GLOBAL_API_STR_CACHE_M 2


struct ZR_STRUCT_ALIGN SZrGlobalState {
    // Memory
    FZrAllocator allocator;
    TZrPtr userAllocationArguments;
    TZrSize allocatedMemories;
    TZrSize waitToGCMemories;
    TZrSize aliveMemories;
    TZrString *memoryErrorMessage;

    // State
    SZrState *mainThreadState;

    // hash
    TUInt64 hashSeed;

    // String Table
    SZrStringTable stringTable;
    // FOR API STRING CACHE
    TZrString *stringHashApiCache[ZR_GLOBAL_API_STR_CACHE_N][ZR_GLOBAL_API_STR_CACHE_M];

    // Global Module Registry
    SZrTypeValue loadedModulesRegistry;
    SZrTypeValue nullValue;

    // GC
    SZrGarbageCollector garbageCollector;

    // Logger
    FZrLog logFunction;


    // exceptions
    // todo:
    FZrPanicHandlingFunction panicHandlingFunction;

    SZrObjectPrototype *basicTypeObjectPrototype[ZR_VALUE_TYPE_ENUM_MAX];

    TUInt8 empty;
};

typedef struct SZrGlobalState SZrGlobalState;


struct ZR_STRUCT_ALIGN SZrThreadState {
    SZrGlobalState *global;
};

typedef struct SZrThreadState SZrThreadState;

ZR_CORE_API SZrGlobalState *ZrGlobalStateNew(FZrAllocator allocator, TZrPtr userAllocationArguments,
                                             TUInt64 uniqueNumber);

ZR_CORE_API void ZrGlobalStateInitRegistry(SZrState *state, SZrGlobalState *global);

ZR_CORE_API void ZrGlobalStateFree(SZrGlobalState *global);

ZR_FORCE_INLINE TBool ZrGlobalStateIsInitialized(SZrGlobalState *global) {
    return global->nullValue.type == ZR_VALUE_TYPE_NULL;
}
#endif //ZR_VM_CORE_GLOBAL_H
