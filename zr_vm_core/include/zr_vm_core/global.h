//
// Created by HeJiahui on 2025/6/16.
//

#ifndef ZR_VM_CORE_GLOBAL_H
#define ZR_VM_CORE_GLOBAL_H
#include "zr_vm_core/callback.h"
#include "zr_vm_core/conf.h"
#include "zr_vm_core/io.h"

#include "zr_vm_core/log.h"
#include "zr_vm_core/value.h"

/**
 * 全局API字符串缓存配置参数
 * ZR_GLOBAL_API_STR_CACHE_N 定义主缓存桶数量（质数以优化散列）
 * ZR_GLOBAL_API_STR_CACHE_M 定义每个桶的二级缓存深度
 * 这组参数共同控制字符串缓存的存储结构和性能特性
 */
#define ZR_GLOBAL_API_STR_CACHE_N 193
#define ZR_GLOBAL_API_STR_CACHE_M 2

// from string.h
struct SZrStringTable;

// from object.h
struct SZrObjectPrototype;

// from state.h
struct SZrState;

// from gc.h
struct SZrGarbageCollector;

// from string.h
struct SZrString;

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

    // injected data
    TZrPtr userData;

    struct SZrString *memoryErrorMessage;

    // State
    struct SZrState *mainThreadState;
    // closure
    struct SZrState *threadWithStackClosures;

    // hash
    TUInt64 hashSeed;

    // String Table
    struct SZrStringTable *stringTable;
    // FOR API STRING CACHE
    struct SZrString *stringHashApiCache[ZR_GLOBAL_API_STR_CACHE_N][ZR_GLOBAL_API_STR_CACHE_M];
    struct SZrString *metaFunctionName[ZR_META_ENUM_MAX];

    // Global Module Registry
    SZrTypeValue loadedModulesRegistry;
    SZrTypeValue nullValue;
    // Global zr object (contains import, etc.)
    SZrTypeValue zrObject;

    // GC
    struct SZrGarbageCollector *garbageCollector;

    // Logger
    FZrLog logFunction;

    // IO
    FZrIoLoadSource sourceLoader;
    
    // Parser and Compiler (injected, can be NULL)
    // 如果为NULL，则只支持加载.zro二进制文件
    // 封装了从源代码解析到编译的全流程
    struct SZrFunction *(*compileSource)(struct SZrState *state, const TChar *source, TZrSize sourceLength, struct SZrString *sourceName);


    // exceptions
    // todo:
    FZrPanicHandlingFunction panicHandlingFunction;

    struct SZrObjectPrototype *basicTypeObjectPrototype[ZR_VALUE_TYPE_ENUM_MAX];

    // callbacks
    SZrCallbackGlobal callbacks;

    TBool isValid;
    TUInt8 empty;
};


typedef struct SZrGlobalState SZrGlobalState;


ZR_CORE_API SZrGlobalState *ZrGlobalStateNew(FZrAllocator allocator, TZrPtr userAllocationArguments,
                                             TUInt64 uniqueNumber, SZrCallbackGlobal *callbacks);

ZR_CORE_API void ZrGlobalStateInitRegistry(struct SZrState *state, SZrGlobalState *global);

// 设置 compileSource 函数指针（由 parser 模块调用）
ZR_CORE_API void ZrGlobalStateSetCompileSource(SZrGlobalState *global, 
    struct SZrFunction *(*compileSource)(struct SZrState *state, const TChar *source, TZrSize sourceLength, struct SZrString *sourceName));

ZR_CORE_API void ZrGlobalStateFree(SZrGlobalState *global);

ZR_FORCE_INLINE TBool ZrGlobalStateIsInitialized(SZrGlobalState *global) { return global->isValid; }


#endif // ZR_VM_CORE_GLOBAL_H
