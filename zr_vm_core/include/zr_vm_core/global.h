//
// Created by HeJiahui on 2025/6/16.
//

#ifndef ZR_VM_CORE_GLOBAL_H
#define ZR_VM_CORE_GLOBAL_H
#include "state.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/value.h"
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
    FZrAlloc alloc;
    TZrPtr userAllocArguments;
    TZrSize allocatedMemories;
    TZrSize waitToGCMemories;
    TZrSize aliveMemories;
    TZrConstantString *memoryErrorMessage;

    // State
    SZrState *mainThreadState;

    // String Table
    SZrConstantStringTable constantStringTable;
    TZrConstantString *stringHashApiCache[ZR_GLOBAL_API_STR_CACHE_N][ZR_GLOBAL_API_STR_CACHE_M];

    // Global Module Registry
    SZrTypeValue loadedModulesRegistry;
    SZrTypeValue nullValue;

    // GC
    SGcObject *gcObjectList;
    SGcObject **gcObjectListSweeper;
    SGcObject *waitToReleaseObjectList;
    SGcObject *releasedObjectList;
    SGcObject *permanentObjectList;
    TBool stopGcFlag;


    // exceptions
    // todo:

    TUInt8 empty;
};

typedef struct SZrGlobalState SZrGlobalState;


struct ZR_STRUCT_ALIGN SZrThreadState {
    SZrGlobalState *global;
};

typedef struct SZrThreadState SZrThreadState;

ZR_CORE_API SZrGlobalState *ZrGlobalStateNew(FZrAlloc alloc, TZrPtr memoryPrivateData);

ZR_CORE_API void ZrGlobalStateFree(SZrGlobalState *global);
#endif //ZR_VM_CORE_GLOBAL_H
