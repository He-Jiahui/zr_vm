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
#include "zr_vm_core/closure.h"
#include "zr_vm_core/module.h"
#include "zr_vm_common/zr_type_conf.h"

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
    // init global zr object
    ZrValueResetAsNull(&global->zrObject);

    // exception
    global->panicHandlingFunction = ZR_NULL;

    // injected data
    global->userData = ZR_NULL;

    // loader
    global->sourceLoader = ZR_NULL;
    
    // parser and compiler (初始化为NULL，需要外部注入)
    // 封装了从源代码解析到编译的全流程
    global->compileSource = ZR_NULL;

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
        
        // 为基本类型注册默认元方法
        ZrMetaInitBuiltinTypeMetaMethods(state, (EZrValueType)i);
    }
}

void ZrGlobalStateInitRegistry(SZrState *state, SZrGlobalState *global) {
    SZrObject *object = ZrObjectNew(state, ZR_NULL);
    ZrObjectInit(state, object);  // 初始化对象的 nodeMap
    ZrValueInitAsRawObject(state, &global->loadedModulesRegistry, ZR_CAST_RAW_OBJECT(object));
    
    // 初始化基本类型对象原型
    ZrGlobalStateInitBasicTypeObjectPrototypes(state, global);
    
    // 创建全局 zr 对象
    SZrObject *zrObject = ZrObjectNew(state, ZR_NULL);
    ZrObjectInit(state, zrObject);
    
    // 创建 zr.import native 函数
    SZrClosureNative *importClosure = ZrClosureNativeNew(state, 0);
    if (importClosure != ZR_NULL) {
        importClosure->nativeFunction = ZrImportNativeFunction;
        
        // 标记为永久对象（避免被 GC 回收，必须在设置值之前标记）
        ZrRawObjectMarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(importClosure));
        
        // 创建函数值
        // 注意：ZrClosureNative 的类型是 ZR_VALUE_TYPE_CLOSURE，不是 ZR_VALUE_TYPE_FUNCTION
        // 不要手动覆盖类型，使用 ZrValueInitAsRawObject 设置的原始类型
        SZrTypeValue importValue;
        ZrValueInitAsRawObject(state, &importValue, ZR_CAST_RAW_OBJECT_AS_SUPER(importClosure));
        // importValue.type 已经由 ZrValueInitAsRawObject 设置为 ZR_VALUE_TYPE_CLOSURE
        importValue.isNative = ZR_TRUE;
        
        // 创建 "import" 字符串键
        SZrString *importName = ZrStringCreate(state, "import", 6);
        // 标记字符串为永久对象（避免被 GC 回收，必须在设置值之前标记）
        // 注意：如果字符串已存在，可能已经被标记为永久对象，ZrRawObjectMarkAsPermanent 会处理这种情况
        ZrRawObjectMarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(importName));
        
        SZrTypeValue importKey;
        ZrValueInitAsRawObject(state, &importKey, ZR_CAST_RAW_OBJECT_AS_SUPER(importName));
        // importKey.type 已经由 ZrValueInitAsRawObject 设置为 ZR_VALUE_TYPE_STRING
        
        // 将 import 函数添加到 zr 对象
        ZrObjectSetValue(state, zrObject, &importKey, &importValue);
    }
    
    // 将 zr 对象存储到 global->zrObject 中，供 GET_GLOBAL 指令使用
    SZrTypeValue zrValue;
    ZrValueInitAsRawObject(state, &zrValue, ZR_CAST_RAW_OBJECT_AS_SUPER(zrObject));
    zrValue.type = ZR_VALUE_TYPE_OBJECT;
    global->zrObject = zrValue;
    
    // 标记 zr 对象为永久对象（避免被 GC 回收）
    ZrRawObjectMarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(zrObject));
    
    // 将 zr 对象添加到全局状态（TODO: 需要确认如何访问全局作用域）
    // 根据计划，zr 除非被局部作用域名字覆盖，否则全局只读
    // 这里暂时先创建对象，后续需要在全局作用域中注册
    
    // 注意：compileSource 函数指针由 parser 模块通过 ZrGlobalStateSetCompileSource 设置
    // 这里不直接调用 parser 模块的函数，以避免循环依赖
    
    // todo: load state value
    // todo: load global value
    // todo: register zr object to global scope
}

// 设置 compileSource 函数指针（由 parser 模块调用）
void ZrGlobalStateSetCompileSource(SZrGlobalState *global, 
    struct SZrFunction *(*compileSource)(struct SZrState *state, const TChar *source, TZrSize sourceLength, struct SZrString *sourceName)) {
    if (global != ZR_NULL) {
        global->compileSource = compileSource;
    }
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
