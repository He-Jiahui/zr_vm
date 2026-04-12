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
 * ZR_GLOBAL_API_STRING_CACHE_BUCKET_COUNT 定义主缓存桶数量（质数以优化散列）
 * ZR_GLOBAL_API_STRING_CACHE_BUCKET_DEPTH 定义每个桶的二级缓存深度
 * 这组参数共同控制字符串缓存的存储结构和性能特性
 */
// from string.h
struct SZrStringTable;

// from object.h
struct SZrObjectPrototype;
struct SZrObjectModule;

// from state.h
struct SZrState;
struct SZrProfileRuntime;

// from gc.h
struct SZrGarbageCollector;

// from string.h
struct SZrString;

typedef struct SZrObjectModule *(*FZrNativeModuleLoader)(struct SZrState *state,
                                                         struct SZrString *moduleName,
                                                         TZrPtr userData);

typedef struct SZrObjectModule *(*FZrAotModuleLoader)(struct SZrState *state,
                                                      struct SZrString *moduleName,
                                                      TZrPtr userData);

typedef void (*FZrGlobalOpaqueStateCleanup)(struct SZrGlobalState *global, TZrPtr state);

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
    TZrUInt64 hashSeed;
    TZrUInt64 cacheIdentity;

    // String Table
    struct SZrStringTable *stringTable;
    // FOR API STRING CACHE
    struct SZrString *stringHashApiCache[ZR_GLOBAL_API_STRING_CACHE_BUCKET_COUNT]
                                        [ZR_GLOBAL_API_STRING_CACHE_BUCKET_DEPTH];
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
    struct SZrProfileRuntime *profileRuntime;

    // IO
    FZrIoLoadSource sourceLoader;
    TZrPtr sourceLoaderUserData;
    FZrAotModuleLoader aotModuleLoader;
    TZrPtr aotModuleLoaderUserData;
    FZrNativeModuleLoader nativeModuleLoader;
    TZrPtr nativeModuleLoaderUserData;
    
    // Parser and Compiler (injected, can be NULL)
    // 如果为NULL，则只支持加载.zro二进制文件
    // 封装了从源代码解析到编译的全流程
    struct SZrFunction *(*compileSource)(struct SZrState *state, const TZrChar *source, TZrSize sourceLength, struct SZrString *sourceName);
    TZrBool emitCompileTimeRuntimeSupport;
    SZrArray importCompileInfoStack;
    TZrPtr parserModuleInitState;
    FZrGlobalOpaqueStateCleanup parserModuleInitStateCleanup;


    // exceptions
    // todo:
    FZrPanicHandlingFunction panicHandlingFunction;
    struct SZrObjectPrototype *errorPrototype;
    struct SZrObjectPrototype *stackFramePrototype;
    SZrTypeValue unhandledExceptionHandler;
    TZrBool hasUnhandledExceptionHandler;

    struct SZrObjectPrototype *basicTypeObjectPrototype[ZR_VALUE_TYPE_ENUM_MAX];

    // callbacks
    SZrCallbackGlobal callbacks;

    TZrBool isValid;
    TZrBool registryInitialized;
    TZrUInt8 empty;
};


typedef struct SZrGlobalState SZrGlobalState;


ZR_CORE_API SZrGlobalState *ZrCore_GlobalState_New(FZrAllocator allocator, TZrPtr userAllocationArguments,
                                             TZrUInt64 uniqueNumber, SZrCallbackGlobal *callbacks);

ZR_CORE_API void ZrCore_GlobalState_InitRegistry(struct SZrState *state, SZrGlobalState *global);

// 设置 compileSource 函数指针（由 parser 模块调用）
ZR_CORE_API void ZrCore_GlobalState_SetCompileSource(SZrGlobalState *global, 
    struct SZrFunction *(*compileSource)(struct SZrState *state, const TZrChar *source, TZrSize sourceLength, struct SZrString *sourceName));

ZR_CORE_API void ZrCore_GlobalState_SetNativeModuleLoader(SZrGlobalState *global,
                                                          FZrNativeModuleLoader loader,
                                                          TZrPtr userData);

ZR_CORE_API void ZrCore_GlobalState_SetAotModuleLoader(SZrGlobalState *global,
                                                       FZrAotModuleLoader loader,
                                                       TZrPtr userData);

ZR_CORE_API void ZrCore_GlobalState_Free(SZrGlobalState *global);

ZR_FORCE_INLINE TZrBool ZrCore_GlobalState_IsInitialized(SZrGlobalState *global) { return global->isValid; }


#endif // ZR_VM_CORE_GLOBAL_H
