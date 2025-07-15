//
// Created by HeJiahui on 2025/6/16.
//

#ifndef ZR_VM_CORE_GLOBAL_H
#define ZR_VM_CORE_GLOBAL_H
#include "zr_vm_core/string.h"
#include "zr_vm_core/conf.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/log.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/callback.h"

/**
 * 全局API字符串缓存配置参数
 * ZR_GLOBAL_API_STR_CACHE_N 定义主缓存桶数量（质数以优化散列）
 * ZR_GLOBAL_API_STR_CACHE_M 定义每个桶的二级缓存深度
 * 这组参数共同控制字符串缓存的存储结构和性能特性
 */
#define ZR_GLOBAL_API_STR_CACHE_N 193
#define ZR_GLOBAL_API_STR_CACHE_M 2

#if !defined(ZR_STRING_TABLE_INIT_SIZE_LOG2)
#define ZR_STRING_TABLE_INIT_SIZE_LOG2 12 // 2^12 = 4KB
#endif

#if !defined(ZR_OBJECT_TABLE_INIT_SIZE_LOG2)
#define ZR_OBJECT_TABLE_INIT_SIZE_LOG2 2 // 2^2 = 4B
#endif
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
    TZrString *metaFunctionName[ZR_META_ENUM_MAX];

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

    // callbacks
    SZrCallbackGlobal callbacks;

    TBool isValid;
    TUInt8 empty;
};


typedef struct SZrGlobalState SZrGlobalState;


ZR_CORE_API SZrGlobalState *ZrGlobalStateNew(FZrAllocator allocator, TZrPtr userAllocationArguments,
                                             TUInt64 uniqueNumber, SZrCallbackGlobal *callbacks);

ZR_CORE_API void ZrGlobalStateInitRegistry(SZrState *state, SZrGlobalState *global);

ZR_CORE_API void ZrGlobalStateFree(SZrGlobalState *global);

ZR_FORCE_INLINE TBool ZrGlobalStateIsInitialized(SZrGlobalState *global) {
    return global->isValid;
}

ZR_FORCE_INLINE TBool ZrGlobalRawObjectIsDead(SZrGlobalState *global, SZrRawObject *object) {
    return global->garbageCollector.gcGeneration != object->garbageCollectMark.generation;
}

ZR_FORCE_INLINE void ZrGlobalValueStaticAssertIsAlive(SZrState *state, SZrTypeValue *value) {
    ZR_ASSERT(!value->isGarbageCollectable ||
        (
            (value->type == value->value.object->type) &&
            ((state == ZR_NULL) || !ZrGlobalRawObjectIsDead(state->global, value->value.object))
        )
    );
}
#endif //ZR_VM_CORE_GLOBAL_H
