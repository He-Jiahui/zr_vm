//
// Created by HeJiahui on 2025/8/6.
//
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/stack.h"
// 不直接引用parser/compiler模块，通过globalState注入的函数指针调用
#include "xxHash/xxhash.h"
// 访问修饰符定义（从 parser 模块复制，避免直接依赖）
// 注意：这里使用数值常量，与 parser 中的定义保持一致
#define ZR_ACCESS_PUBLIC 0
#define ZR_ACCESS_PRIVATE 1
#define ZR_ACCESS_PROTECTED 2
typedef TUInt8 EZrAccessModifier;

// 创建模块对象
struct SZrObjectModule *ZrModuleCreate(SZrState *state) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrObject *object = ZrObjectNewCustomized(state, sizeof(struct SZrObjectModule), ZR_OBJECT_INTERNAL_TYPE_MODULE);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }
    
    struct SZrObjectModule *module = (struct SZrObjectModule *)object;
    
    // 初始化模块信息
    module->moduleName = ZR_NULL;
    module->pathHash = 0;
    module->fullPath = ZR_NULL;
    
    // 初始化 proNodeMap
    ZrHashSetConstruct(&module->proNodeMap);
    ZrHashSetInit(state, &module->proNodeMap, ZR_OBJECT_TABLE_INIT_SIZE_LOG2);
    
    return module;
}

// 设置模块信息
void ZrModuleSetInfo(SZrState *state, struct SZrObjectModule *module, 
                      SZrString *moduleName, TUInt64 pathHash, SZrString *fullPath) {
    if (state == ZR_NULL || module == ZR_NULL) {
        return;
    }
    
    module->moduleName = moduleName;
    module->pathHash = pathHash;
    module->fullPath = fullPath;
}

// 添加 pub 导出（同时添加到 pub 和 pro）
void ZrModuleAddPubExport(SZrState *state, struct SZrObjectModule *module, 
                           SZrString *name, const SZrTypeValue *value) {
    if (state == ZR_NULL || module == ZR_NULL || name == ZR_NULL || value == ZR_NULL) {
        return;
    }
    
    // 创建键值
    SZrTypeValue key;
    ZrValueInitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(name));
    key.type = ZR_VALUE_TYPE_STRING;
    
    // 添加到 pub（super.nodeMap）
    ZrObjectSetValue(state, &module->super, &key, value);
    
    // 同时添加到 pro（proNodeMap）
    SZrHashKeyValuePair *pair = ZrHashSetFind(state, &module->proNodeMap, &key);
    if (pair == ZR_NULL) {
        pair = ZrHashSetAdd(state, &module->proNodeMap, &key);
    }
    ZrValueCopy(state, &pair->value, value);
}

// 添加 pro 导出（仅添加到 pro）
void ZrModuleAddProExport(SZrState *state, struct SZrObjectModule *module, 
                           SZrString *name, const SZrTypeValue *value) {
    if (state == ZR_NULL || module == ZR_NULL || name == ZR_NULL || value == ZR_NULL) {
        return;
    }
    
    // 创建键值
    SZrTypeValue key;
    ZrValueInitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(name));
    key.type = ZR_VALUE_TYPE_STRING;
    
    // 仅添加到 pro（proNodeMap）
    SZrHashKeyValuePair *pair = ZrHashSetFind(state, &module->proNodeMap, &key);
    if (pair == ZR_NULL) {
        pair = ZrHashSetAdd(state, &module->proNodeMap, &key);
    }
    ZrValueCopy(state, &pair->value, value);
}

// 获取 pub 导出（跨模块访问）
const SZrTypeValue *ZrModuleGetPubExport(SZrState *state, struct SZrObjectModule *module, 
                                           SZrString *name) {
    if (state == ZR_NULL || module == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 创建键值
    SZrTypeValue key;
    ZrValueInitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(name));
    key.type = ZR_VALUE_TYPE_STRING;
    
    // 从 pub（super.nodeMap）获取
    return ZrObjectGetValue(state, &module->super, &key);
}

// 获取 pro 导出（同模块库访问）
const SZrTypeValue *ZrModuleGetProExport(SZrState *state, struct SZrObjectModule *module, 
                                           SZrString *name) {
    if (state == ZR_NULL || module == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 创建键值
    SZrTypeValue key;
    ZrValueInitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(name));
    key.type = ZR_VALUE_TYPE_STRING;
    
    // 从 pro（proNodeMap）获取
    SZrHashKeyValuePair *pair = ZrHashSetFind(state, &module->proNodeMap, &key);
    if (pair == ZR_NULL) {
        return ZR_NULL;
    }
    return &pair->value;
}

// 计算路径哈希（使用 xxhash）
TUInt64 ZrModuleCalculatePathHash(SZrState *state, SZrString *fullPath) {
    ZR_UNUSED_PARAMETER(state);
    if (fullPath == ZR_NULL) {
        return 0;
    }
    
    // 获取字符串内容
    TNativeString pathStr;
    TZrSize pathLen;
    if (fullPath->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        pathStr = ZrStringGetNativeStringShort(fullPath);
        pathLen = fullPath->shortStringLength;
    } else {
        pathStr = *ZrStringGetNativeStringLong(fullPath);
        pathLen = fullPath->longStringLength;
    }
    
    if (pathStr == ZR_NULL || pathLen == 0) {
        return 0;
    }
    
    // 使用 XXH3_64bits 计算路径哈希（不使用seed，直接计算）
    return XXH3_64bits(pathStr, pathLen);
}

// 从缓存中获取模块
struct SZrObjectModule *ZrModuleGetFromCache(SZrState *state, SZrString *path) {
    if (state == ZR_NULL || path == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrGlobalState *global = state->global;
    
    // 检查 loadedModulesRegistry 是否已初始化
    if (!ZrValueIsGarbageCollectable(&global->loadedModulesRegistry) ||
        global->loadedModulesRegistry.type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }
    
    SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    if (registry == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 创建键值
    SZrTypeValue key;
    ZrValueInitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(path));
    key.type = ZR_VALUE_TYPE_STRING;
    
    // 从缓存中查找
    const SZrTypeValue *cachedValue = ZrObjectGetValue(state, registry, &key);
    if (cachedValue == ZR_NULL || cachedValue->type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }
    
    SZrObject *cachedObject = ZR_CAST_OBJECT(state, cachedValue->value.object);
    if (cachedObject == ZR_NULL || cachedObject->internalType != ZR_OBJECT_INTERNAL_TYPE_MODULE) {
        return ZR_NULL;
    }
    
    return (struct SZrObjectModule *)cachedObject;
}

// 将模块添加到缓存
void ZrModuleAddToCache(SZrState *state, SZrString *path, struct SZrObjectModule *module) {
    if (state == ZR_NULL || path == ZR_NULL || module == ZR_NULL || state->global == ZR_NULL) {
        return;
    }
    
    SZrGlobalState *global = state->global;
    
    // 检查 loadedModulesRegistry 是否已初始化
    if (!ZrValueIsGarbageCollectable(&global->loadedModulesRegistry) ||
        global->loadedModulesRegistry.type != ZR_VALUE_TYPE_OBJECT) {
        return;
    }
    
    SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    if (registry == ZR_NULL) {
        return;
    }
    
    // 创建键值
    SZrTypeValue key;
    ZrValueInitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(path));
    key.type = ZR_VALUE_TYPE_STRING;
    
    // 创建模块值
    SZrTypeValue moduleValue;
    ZrValueInitAsRawObject(state, &moduleValue, ZR_CAST_RAW_OBJECT_AS_SUPER(module));
    moduleValue.type = ZR_VALUE_TYPE_OBJECT;
    
    // 添加到缓存（覆盖旧缓存，支持热重载）
    ZrObjectSetValue(state, registry, &key, &moduleValue);
}

// 从源文件创建模块（旧接口，保持向后兼容）
SZrObject *ZrModuleCreateFromSource(SZrState *state, SZrIoSource *source) {
    ZR_UNUSED_PARAMETER(state);
    ZR_ASSERT(source != ZR_NULL);
    // TODO: 实现从源文件创建模块的逻辑
    return ZR_NULL;
}

// zr.import native 函数实现
TInt64 ZrImportNativeFunction(SZrState *state) {
    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }
    
    // 获取函数参数（路径字符串）
    TZrStackValuePointer functionBase = state->callInfoList->functionBase.valuePointer;
    TZrStackValuePointer argBase = functionBase + 1;
    
    // 检查参数数量
    if (state->stackTop.valuePointer <= argBase) {
        // 没有参数，返回 null
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        return 1;
    }
    
    // 获取路径参数
    SZrTypeValue *pathValue = ZrStackGetValue(argBase);
    if (pathValue->type != ZR_VALUE_TYPE_STRING) {
        // 参数类型错误，返回 null
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        return 1;
    }
    
    SZrString *path = ZR_CAST_STRING(state, pathValue->value.object);
    if (path == ZR_NULL) {
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        return 1;
    }
    
    // 检查缓存
    struct SZrObjectModule *cachedModule = ZrModuleGetFromCache(state, path);
    if (cachedModule != ZR_NULL) {
        // 缓存命中，返回缓存的模块
        SZrTypeValue *result = ZrStackGetValue(functionBase);
        ZrValueInitAsRawObject(state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(cachedModule));
        result->type = ZR_VALUE_TYPE_OBJECT;
        return 1;
    }
    
    // 缓存未命中，需要加载、编译和执行
    SZrGlobalState *global = state->global;
    if (global->sourceLoader == ZR_NULL) {
        // 没有源加载器，返回 null
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        return 1;
    }
    
    // 获取路径字符串内容
    TNativeString pathStr;
    TZrSize pathLen;
    if (path->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        pathStr = ZrStringGetNativeStringShort(path);
        pathLen = path->shortStringLength;
    } else {
        pathStr = *ZrStringGetNativeStringLong(path);
        pathLen = path->longStringLength;
    }
    
    if (pathStr == ZR_NULL || pathLen == 0) {
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        return 1;
    }
    
    // 加载源文件
    SZrIo io;
    TBool loadSuccess = global->sourceLoader(state, pathStr, ZR_NULL, &io);
    if (!loadSuccess || io.pointer == ZR_NULL) {
        // 加载失败，返回 null
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        return 1;
    }
    
    // 读取源文件内容
    TZrSize sourceSize = io.remained;
    TBytePtr sourceBuffer = (TBytePtr)ZrMemoryRawMallocWithType(global, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (sourceBuffer == ZR_NULL) {
        if (io.close != ZR_NULL) {
            io.close(state, io.customData);
        }
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        return 1;
    }
    
    TZrSize readSize = ZrIoRead(&io, sourceBuffer, sourceSize);
    sourceBuffer[readSize] = '\0';
    if (io.close != ZR_NULL) {
        io.close(state, io.customData);
    }
    
    if (readSize == 0) {
        if (io.close != ZR_NULL) {
            io.close(state, io.customData);
        }
        ZrMemoryRawFreeWithType(global, sourceBuffer, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        return 1;
    }
    
    // 检查文件扩展名，判断是源代码还是二进制文件
    TBool isBinaryFile = ZR_FALSE;
    if (pathLen >= 4) {
        const TChar *ext = pathStr + pathLen - 4;
        if (ext[0] == '.' && ext[1] == 'z' && ext[2] == 'r' && ext[3] == 'o') {
            isBinaryFile = ZR_TRUE;
        }
    }
    
    SZrFunction *func = ZR_NULL;
    
    if (isBinaryFile) {
        // 加载.zro二进制文件
        if (io.close != ZR_NULL) {
            io.close(state, io.customData);
        }
        ZrMemoryRawFreeWithType(global, sourceBuffer, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
        
        // 重新加载为二进制文件
        SZrIoSource *ioSource = ZrIoLoadSource(state, pathStr, ZR_NULL);
        if (ioSource == ZR_NULL) {
            ZrValueResetAsNull(ZrStackGetValue(functionBase));
            return 1;
        }
        
        // 从二进制文件创建模块（TODO: 实现从SZrIoSource创建函数的逻辑）
        // 目前先返回null，等待实现
        ZrIoReadSourceFree(global, ioSource);
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        return 1;
    } else {
        // 源代码文件，需要parser和compiler
        if (global->compileSource == ZR_NULL) {
            // 没有parser/compiler，无法处理源代码文件
            if (io.close != ZR_NULL) {
                io.close(state, io.customData);
            }
            ZrMemoryRawFreeWithType(global, sourceBuffer, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            ZrValueResetAsNull(ZrStackGetValue(functionBase));
            return 1;
        }
        
        // 编译源代码（封装了从解析到编译的全流程）
        SZrString *sourceName = ZrStringCreate(state, pathStr, pathLen);
        func = global->compileSource(state, (const TChar *)sourceBuffer, readSize, sourceName);
        ZrMemoryRawFreeWithType(global, sourceBuffer, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
        
        if (func == ZR_NULL) {
            // 编译失败，返回 null
            ZrValueResetAsNull(ZrStackGetValue(functionBase));
            return 1;
        }
    }
    
    // 创建闭包
    SZrClosure *closure = ZrClosureNew(state, 0);
    if (closure == ZR_NULL) {
        ZrFunctionFree(state, func);
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        return 1;
    }
    
    closure->function = func;
    
    // 创建模块对象（在执行 __entry 之前创建，以便在运行时可以访问）
    struct SZrObjectModule *module = ZrModuleCreate(state);
    if (module == ZR_NULL) {
        ZrMemoryRawFreeWithType(global, sourceBuffer, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        return 1;
    }
    
    // 设置模块信息
    TUInt64 pathHash = ZrModuleCalculatePathHash(state, path);
    ZrModuleSetInfo(state, module, ZR_NULL, pathHash, path);
    
    // 执行函数（调用 __entry）
    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
    TZrStackValuePointer callBase = savedStackTop;
    ZrStackSetRawObjectValue(state, callBase, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    state->stackTop.valuePointer = callBase + 1;
    ZrFunctionCall(state, callBase, 0);
    
    // 执行后，栈上的变量值已经可用
    // 收集 pub 和 pro 变量到 module object
    if (func != ZR_NULL && func->exportedVariables != ZR_NULL && func->exportedVariableLength > 0) {
        for (TUInt32 i = 0; i < func->exportedVariableLength; i++) {
            struct SZrFunctionExportedVariable *exportVar = &func->exportedVariables[i];
            if (exportVar->name != ZR_NULL) {
                // 从栈上获取变量值（需要根据 stackSlot 计算位置）
                // 注意：stackSlot 是相对于 functionBase 的偏移
                TZrStackValuePointer varPointer = callBase + 1 + exportVar->stackSlot;
                if (varPointer < state->stackTop.valuePointer) {
                    SZrTypeValue *varValue = ZrStackGetValue(varPointer);
                    if (varValue != ZR_NULL) {
                        if (exportVar->accessModifier == ZR_ACCESS_PUBLIC) {
                            ZrModuleAddPubExport(state, module, exportVar->name, varValue);
                        } else if (exportVar->accessModifier == ZR_ACCESS_PROTECTED) {
                            ZrModuleAddProExport(state, module, exportVar->name, varValue);
                        }
                    }
                }
            }
        }
    }
    
    // 创建 prototype（从编译时收集的信息）
    // 从函数的常量池中读取 prototype 信息并创建
    // 注意：这是一个简化实现，实际应该使用更明确的标记来标识 prototype 信息
    // TODO: 实现更优雅的存储和读取方式（例如使用函数的额外字段或特殊标记常量）
    
    // 添加到缓存
    ZrModuleAddToCache(state, path, module);
    
    // 返回模块对象
    SZrTypeValue *result = ZrStackGetValue(functionBase);
    ZrValueInitAsRawObject(state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(module));
    result->type = ZR_VALUE_TYPE_OBJECT;
    
    state->stackTop.valuePointer = savedStackTop;
    return 1;
}

// 创建并注册 prototype 的 native 函数
// 参数: (module, typeName, prototypeType, accessModifier)
// 返回: prototype 对象
TInt64 ZrCreatePrototypeNativeFunction(SZrState *state) {
    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }
    
    // 获取函数参数
    TZrStackValuePointer functionBase = state->callInfoList->functionBase.valuePointer;
    TZrStackValuePointer argBase = functionBase + 1;
    
    // 检查参数数量（至少需要 4 个参数：module, typeName, prototypeType, accessModifier）
    if (state->stackTop.valuePointer <= argBase + 3) {
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        return 1;
    }
    
    // 获取 module 参数
    SZrTypeValue *moduleValue = ZrStackGetValue(argBase);
    if (moduleValue->type != ZR_VALUE_TYPE_OBJECT) {
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        return 1;
    }
    
    SZrObject *moduleObject = ZR_CAST_OBJECT(state, moduleValue->value.object);
    if (moduleObject == ZR_NULL || moduleObject->internalType != ZR_OBJECT_INTERNAL_TYPE_MODULE) {
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        return 1;
    }
    
    struct SZrObjectModule *module = (struct SZrObjectModule *)moduleObject;
    
    // 获取 typeName 参数
    SZrTypeValue *typeNameValue = ZrStackGetValue(argBase + 1);
    if (typeNameValue->type != ZR_VALUE_TYPE_STRING) {
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        return 1;
    }
    
    SZrString *typeName = ZR_CAST_STRING(state, typeNameValue->value.object);
    if (typeName == ZR_NULL) {
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        return 1;
    }
    
    // 获取 prototypeType 参数
    SZrTypeValue *typeValue = ZrStackGetValue(argBase + 2);
    if (!ZR_VALUE_IS_TYPE_INT(typeValue->type)) {
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        return 1;
    }
    
    EZrObjectPrototypeType prototypeType = (EZrObjectPrototypeType)typeValue->value.nativeObject.nativeUInt64;
    if (prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT && prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        return 1;
    }
    
    // 获取 accessModifier 参数
    SZrTypeValue *accessModifierValue = ZrStackGetValue(argBase + 3);
    if (!ZR_VALUE_IS_TYPE_INT(accessModifierValue->type)) {
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        return 1;
    }
    
    EZrAccessModifier accessModifier = (EZrAccessModifier)accessModifierValue->value.nativeObject.nativeUInt64;
    
    // 创建 prototype 对象
    SZrObjectPrototype *prototype = ZR_NULL;
    if (prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        prototype = (SZrObjectPrototype *)ZrStructPrototypeNew(state, typeName);
    } else {
        prototype = ZrObjectPrototypeNew(state, typeName, prototypeType);
    }
    
    if (prototype == ZR_NULL) {
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        return 1;
    }
    
    // 注册到模块导出
    SZrTypeValue prototypeValue;
    ZrValueInitAsRawObject(state, &prototypeValue, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    prototypeValue.type = ZR_VALUE_TYPE_OBJECT;
    
    if (accessModifier == ZR_ACCESS_PUBLIC) {
        ZrModuleAddPubExport(state, module, typeName, &prototypeValue);
    } else if (accessModifier == ZR_ACCESS_PROTECTED) {
        ZrModuleAddProExport(state, module, typeName, &prototypeValue);
    } else {
        // PRIVATE 不导出
        // 但仍然创建 prototype 对象
    }
    
    // 返回 prototype 对象
    SZrTypeValue *result = ZrStackGetValue(functionBase);
    ZrValueCopy(state, result, &prototypeValue);
    
    return 1;
}
