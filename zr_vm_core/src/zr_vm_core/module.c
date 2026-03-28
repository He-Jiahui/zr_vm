//
// Created by HeJiahui on 2025/8/6.
//
#include "zr_vm_core/module.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/function.h"
#include "zr_vm_common/zr_meta_conf.h"
#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_common/zr_string_conf.h"

// 访问修饰符类型定义（值与zr_ast_constants.h中的常量保持一致）
typedef TUInt8 EZrAccessModifier;

// 内部结构：存储prototype创建时的临时信息
typedef struct {
    SZrObjectPrototype *prototype;
    SZrString *typeName;
    EZrObjectPrototypeType prototypeType;
    EZrAccessModifier accessModifier;
    SZrArray inheritTypeNames;  // SZrString*数组，存储继承类型名称
    const SZrCompiledMemberInfo *members;
    TUInt32 membersCount;
} SZrPrototypeCreationInfo;

static void register_prototype_in_global_scope(SZrState *state, SZrString *typeName,
                                               const SZrTypeValue *prototypeValue) {
    if (state == ZR_NULL || state->global == ZR_NULL || typeName == ZR_NULL || prototypeValue == ZR_NULL) {
        return;
    }

    if (state->global->zrObject.type != ZR_VALUE_TYPE_OBJECT) {
        return;
    }

    SZrObject *zrObject = ZR_CAST_OBJECT(state, state->global->zrObject.value.object);
    if (zrObject == ZR_NULL) {
        return;
    }

    SZrTypeValue key;
    ZrValueInitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(typeName));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrObjectSetValue(state, zrObject, &key, prototypeValue);
}

static SZrFunction *get_function_from_constant(SZrState *state, const SZrTypeValue *constant) {
    if (state == ZR_NULL || constant == ZR_NULL) {
        return ZR_NULL;
    }

    if (constant->type != ZR_VALUE_TYPE_FUNCTION && constant->type != ZR_VALUE_TYPE_CLOSURE) {
        return ZR_NULL;
    }

    SZrRawObject *rawObject = constant->value.object;
    if (rawObject == ZR_NULL || rawObject->type != ZR_RAW_OBJECT_TYPE_FUNCTION) {
        return ZR_NULL;
    }

    return ZR_CAST_FUNCTION(state, rawObject);
}

// 辅助函数：检查一个常量是否是prototype信息对象
// 通过检查Object的__type属性来判断
static TBool is_prototype_info_constant(struct SZrState *state, const SZrTypeValue *constant) {
    if (constant == ZR_NULL || constant->type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_FALSE;
    }
    
    SZrObject *obj = ZR_CAST_OBJECT(state, constant->value.object);
    if (obj == ZR_NULL || obj->internalType != ZR_OBJECT_INTERNAL_TYPE_PROTOTYPE_INFO) {
        return ZR_FALSE;
    }
    
    // 检查是否有__type属性，且值为"prototype"
    SZrTypeValue typeKey;
    SZrString *typeKeyStr = ZrStringCreate(state, "__type", 6);
    ZrValueInitAsRawObject(state, &typeKey, ZR_CAST_RAW_OBJECT_AS_SUPER(typeKeyStr));
    typeKey.type = ZR_VALUE_TYPE_STRING;
    
    const SZrTypeValue *typeValue = ZrObjectGetValue(state, obj, &typeKey);
    if (typeValue == ZR_NULL || typeValue->type != ZR_VALUE_TYPE_STRING) {
        return ZR_FALSE;
    }
    
    SZrString *typeStr = ZR_CAST_STRING(state, typeValue->value.object);
    if (typeStr == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 比较字符串值是否为"prototype"
    TNativeString typeNativeStr = ZrStringGetNativeStringShort(typeStr);
    if (typeNativeStr == ZR_NULL) {
        typeNativeStr = *ZrStringGetNativeStringLong(typeStr);
    }
    
    if (typeNativeStr == ZR_NULL) {
        return ZR_FALSE;
    }
    
    TZrSize len = (typeStr->shortStringLength < ZR_VM_LONG_STRING_FLAG) ? 
                   (TZrSize)typeStr->shortStringLength : 
                   typeStr->longStringLength;
    
    return (len == 9 && ZrMemoryRawCompare(typeNativeStr, "prototype", 9) == 0);
}

// 辅助函数：从模块中查找prototype（用于继承关系）
static SZrObjectPrototype *find_prototype_by_name(struct SZrState *state, 
                                                   struct SZrObjectModule *module,
                                                   SZrString *typeName) {
    if (state == ZR_NULL || module == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 先在当前模块的导出中查找
    const SZrTypeValue *exported = ZrModuleGetPubExport(state, module, typeName);
    if (exported != ZR_NULL && exported->type == ZR_VALUE_TYPE_OBJECT) {
        SZrObjectPrototype *proto = (SZrObjectPrototype *)ZR_CAST_OBJECT(state, exported->value.object);
        if (proto != ZR_NULL && proto->type != ZR_OBJECT_PROTOTYPE_TYPE_INVALID) {
            return proto;
        }
    }
    
    // 在导入模块中查找：遍历全局模块注册表，查找所有已加载的模块
    // 注意：由于模块对象没有存储导入列表，我们遍历所有模块查找匹配的prototype
    // TODO: 这是一个简化的实现，更高效的方法需要在模块对象中存储导入列表
    if (state->global != ZR_NULL) {
        SZrGlobalState *global = state->global;
        if (ZrValueIsGarbageCollectable(&global->loadedModulesRegistry) &&
            global->loadedModulesRegistry.type == ZR_VALUE_TYPE_OBJECT) {
            SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
            if (registry != ZR_NULL && registry->nodeMap.isValid && 
                registry->nodeMap.buckets != ZR_NULL) {
                // 遍历模块注册表，查找匹配的prototype
                for (TZrSize i = 0; i < registry->nodeMap.capacity; i++) {
                    SZrHashKeyValuePair *pair = registry->nodeMap.buckets[i];
                    while (pair != ZR_NULL) {
                        // 检查值是否是模块对象
                        if (pair->value.type == ZR_VALUE_TYPE_OBJECT) {
                            SZrObject *cachedObject = ZR_CAST_OBJECT(state, pair->value.value.object);
                            if (cachedObject != ZR_NULL && 
                                cachedObject->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
                                struct SZrObjectModule *importedModule = (struct SZrObjectModule *)cachedObject;
                                
                                // 跳过当前模块（已在上面查找过）
                                if (importedModule == module) {
                                    pair = pair->next;
                                    continue;
                                }
                                
                                // 在导入模块的导出中查找
                                const SZrTypeValue *importedExported = ZrModuleGetPubExport(state, importedModule, typeName);
                                if (importedExported != ZR_NULL && importedExported->type == ZR_VALUE_TYPE_OBJECT) {
                                    SZrObjectPrototype *proto = (SZrObjectPrototype *)ZR_CAST_OBJECT(state, importedExported->value.object);
                                    if (proto != ZR_NULL && proto->type != ZR_OBJECT_PROTOTYPE_TYPE_INVALID) {
                                        return proto;
                                    }
                                }
                            }
                        }
                        pair = pair->next;
                    }
                }
            }
        }
    }
    
    return ZR_NULL;
}

static SZrObjectPrototype *find_local_created_prototype_by_name(SZrArray *prototypeInfos, SZrString *typeName) {
    if (prototypeInfos == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize i = 0; i < prototypeInfos->length; i++) {
        SZrPrototypeCreationInfo *protoInfo =
                (SZrPrototypeCreationInfo *)ZrArrayGet(prototypeInfos, i);
        if (protoInfo == ZR_NULL || protoInfo->prototype == ZR_NULL || protoInfo->typeName == ZR_NULL) {
            continue;
        }

        if (ZrStringEqual(protoInfo->typeName, typeName)) {
            return protoInfo->prototype;
        }
    }

    return ZR_NULL;
}

static TBool refill_io_chunk(SZrIo *io) {
    if (io == ZR_NULL || io->read == ZR_NULL || io->state == ZR_NULL) {
        return ZR_FALSE;
    }

    SZrState *state = io->state;
    TZrSize readSize = 0;
    ZR_THREAD_UNLOCK(state);
    TBytePtr buffer = io->read(state, io->customData, &readSize);
    ZR_THREAD_LOCK(state);
    if (buffer == ZR_NULL || readSize == 0) {
        return ZR_FALSE;
    }

    io->pointer = buffer;
    io->remained = readSize;
    return ZR_TRUE;
}

static TBytePtr read_all_from_io(SZrState *state, SZrIo *io, TZrSize *outSize) {
    if (state == ZR_NULL || io == ZR_NULL || outSize == ZR_NULL) {
        return ZR_NULL;
    }

    SZrGlobalState *global = state->global;
    TZrSize capacity = (io->remained > 0) ? io->remained : 4096;
    TBytePtr buffer = (TBytePtr)ZrMemoryRawMallocWithType(global, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (buffer == ZR_NULL) {
        return ZR_NULL;
    }

    TZrSize totalSize = 0;
    while (io->remained > 0 || refill_io_chunk(io)) {
        if (totalSize + io->remained + 1 > capacity) {
            TZrSize newCapacity = capacity;
            while (totalSize + io->remained + 1 > newCapacity) {
                newCapacity *= 2;
            }

            TBytePtr newBuffer =
                    (TBytePtr)ZrMemoryRawMallocWithType(global, newCapacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            if (newBuffer == ZR_NULL) {
                ZrMemoryRawFreeWithType(global, buffer, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
                return ZR_NULL;
            }

            if (totalSize > 0) {
                ZrMemoryRawCopy(newBuffer, buffer, totalSize);
            }
            ZrMemoryRawFreeWithType(global, buffer, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            buffer = newBuffer;
            capacity = newCapacity;
        }

        ZrMemoryRawCopy(buffer + totalSize, io->pointer, io->remained);
        totalSize += io->remained;
        io->pointer += io->remained;
        io->remained = 0;
    }

    buffer[totalSize] = '\0';
    *outSize = totalSize;
    return buffer;
}

static void init_inline_runtime_function(SZrFunction *function) {
    ZrMemoryRawSet(function, 0, sizeof(SZrFunction));
    ZrRawObjectConstruct(&function->super, ZR_RAW_OBJECT_TYPE_FUNCTION);
}

static TBool populate_runtime_function(SZrState *state, const SZrIoFunction *source, SZrFunction *function);

static TBool convert_io_constant_to_runtime_value(SZrState *state, const SZrIoFunctionConstantVariable *source,
                                                  SZrTypeValue *destination) {
    if (state == ZR_NULL || source == ZR_NULL || destination == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (source->type) {
        case ZR_VALUE_TYPE_NULL: {
            ZrValueResetAsNull(destination);
            return ZR_TRUE;
        }
        case ZR_VALUE_TYPE_BOOL: {
            destination->type = ZR_VALUE_TYPE_BOOL;
            destination->value.nativeObject.nativeBool = source->value.nativeObject.nativeBool;
            destination->isGarbageCollectable = ZR_FALSE;
            destination->isNative = ZR_TRUE;
            return ZR_TRUE;
        }
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64: {
            destination->type = source->type;
            destination->value.nativeObject.nativeInt64 = source->value.nativeObject.nativeInt64;
            destination->isGarbageCollectable = ZR_FALSE;
            destination->isNative = ZR_TRUE;
            return ZR_TRUE;
        }
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64: {
            destination->type = source->type;
            destination->value.nativeObject.nativeUInt64 = source->value.nativeObject.nativeUInt64;
            destination->isGarbageCollectable = ZR_FALSE;
            destination->isNative = ZR_TRUE;
            return ZR_TRUE;
        }
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE: {
            destination->type = source->type;
            destination->value.nativeObject.nativeDouble = source->value.nativeObject.nativeDouble;
            destination->isGarbageCollectable = ZR_FALSE;
            destination->isNative = ZR_TRUE;
            return ZR_TRUE;
        }
        case ZR_VALUE_TYPE_STRING: {
            if (source->value.object == ZR_NULL) {
                return ZR_FALSE;
            }
            ZrValueInitAsRawObject(state, destination, source->value.object);
            destination->type = ZR_VALUE_TYPE_STRING;
            return ZR_TRUE;
        }
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE: {
            if (!source->hasFunctionValue || source->functionValue == ZR_NULL) {
                return ZR_FALSE;
            }

            SZrFunction *functionValue = ZrFunctionNew(state);
            if (functionValue == ZR_NULL) {
                return ZR_FALSE;
            }
            if (!populate_runtime_function(state, source->functionValue, functionValue)) {
                ZrFunctionFree(state, functionValue);
                return ZR_FALSE;
            }

            ZrValueInitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(functionValue));
            destination->type = source->type;
            return ZR_TRUE;
        }
        default: {
            return ZR_FALSE;
        }
    }
}

static TBool populate_runtime_function(SZrState *state, const SZrIoFunction *source, SZrFunction *function) {
    if (state == ZR_NULL || source == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    SZrGlobalState *global = state->global;
    function->functionName = source->name;
    function->parameterCount = (TUInt16) source->parametersLength;
    function->hasVariableArguments = source->hasVarArgs ? ZR_TRUE : ZR_FALSE;
    function->stackSize = source->stackSize;
    function->lineInSourceStart = (TUInt32) source->startLine;
    function->lineInSourceEnd = (TUInt32) source->endLine;

    if (source->instructionsLength > 0) {
        TZrSize instructionBytes = sizeof(TZrInstruction) * source->instructionsLength;
        function->instructionsList =
                (TZrInstruction *) ZrMemoryRawMallocWithType(global, instructionBytes, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->instructionsList == ZR_NULL) {
            ZrFunctionFree(state, function);
            return ZR_FALSE;
        }
        ZrMemoryRawCopy(function->instructionsList, source->instructions, instructionBytes);
        function->instructionsLength = (TUInt32) source->instructionsLength;
    }

    if (source->localVariablesLength > 0) {
        TZrSize localBytes = sizeof(SZrFunctionLocalVariable) * source->localVariablesLength;
        function->localVariableList = (SZrFunctionLocalVariable *) ZrMemoryRawMallocWithType(
                global, localBytes, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->localVariableList == ZR_NULL) {
            ZrFunctionFree(state, function);
            return ZR_FALSE;
        }

        for (TZrSize i = 0; i < source->localVariablesLength; i++) {
            function->localVariableList[i].name = ZR_NULL;
            function->localVariableList[i].offsetActivate = (TZrMemoryOffset) source->localVariables[i].instructionStartIndex;
            function->localVariableList[i].offsetDead = (TZrMemoryOffset) source->localVariables[i].instructionEndIndex;
        }
        function->localVariableLength = (TUInt32) source->localVariablesLength;
    }

    if (source->constantVariablesLength > 0) {
        TZrSize constantBytes = sizeof(SZrTypeValue) * source->constantVariablesLength;
        function->constantValueList =
                (SZrTypeValue *) ZrMemoryRawMallocWithType(global, constantBytes, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->constantValueList == ZR_NULL) {
            ZrFunctionFree(state, function);
            return ZR_FALSE;
        }

        for (TZrSize i = 0; i < source->constantVariablesLength; i++) {
            if (!convert_io_constant_to_runtime_value(state, &source->constantVariables[i],
                                                      &function->constantValueList[i])) {
                ZrFunctionFree(state, function);
                return ZR_FALSE;
            }
        }
        function->constantValueLength = (TUInt32) source->constantVariablesLength;
    }

    if (source->exportedVariablesLength > 0) {
        TZrSize exportBytes = sizeof(struct SZrFunctionExportedVariable) * source->exportedVariablesLength;
        function->exportedVariables = (struct SZrFunctionExportedVariable *) ZrMemoryRawMallocWithType(
                global, exportBytes, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->exportedVariables == ZR_NULL) {
            ZrFunctionFree(state, function);
            return ZR_FALSE;
        }

        for (TZrSize i = 0; i < source->exportedVariablesLength; i++) {
            function->exportedVariables[i].name = source->exportedVariables[i].name;
            function->exportedVariables[i].stackSlot = source->exportedVariables[i].stackSlot;
            function->exportedVariables[i].accessModifier = source->exportedVariables[i].accessModifier;
        }
        function->exportedVariableLength = (TUInt32) source->exportedVariablesLength;
    }

    if (source->closuresLength > 0) {
        TZrSize childBytes = sizeof(SZrFunction) * source->closuresLength;
        function->childFunctionList =
                (SZrFunction *) ZrMemoryRawMallocWithType(global, childBytes, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->childFunctionList == ZR_NULL) {
            ZrFunctionFree(state, function);
            return ZR_FALSE;
        }

        for (TZrSize i = 0; i < source->closuresLength; i++) {
            init_inline_runtime_function(&function->childFunctionList[i]);
            if (source->closures[i].subFunction == ZR_NULL ||
                !populate_runtime_function(state, source->closures[i].subFunction, &function->childFunctionList[i])) {
                ZrFunctionFree(state, function);
                return ZR_FALSE;
            }
        }
        function->childFunctionLength = (TUInt32) source->closuresLength;
    }

    return ZR_TRUE;
}

static SZrFunction *convert_io_function_to_runtime(SZrState *state, const SZrIoFunction *source) {
    if (state == ZR_NULL || source == ZR_NULL) {
        return ZR_NULL;
    }

    SZrFunction *function = ZrFunctionNew(state);
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    if (!populate_runtime_function(state, source, function)) {
        ZrFunctionFree(state, function);
        return ZR_NULL;
    }

    return function;
}

static SZrFunction *load_entry_function_from_io_source(SZrState *state, const SZrIoSource *source) {
    if (state == ZR_NULL || source == ZR_NULL || source->modulesLength == 0 || source->modules == ZR_NULL) {
        return ZR_NULL;
    }

    const SZrIoModule *module = &source->modules[0];
    if (module->entryFunction == ZR_NULL) {
        return ZR_NULL;
    }

    return convert_io_function_to_runtime(state, module->entryFunction);
}

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
    
    // 初始化 super.nodeMap（继承自 SZrObject，用于 pub 导出）
    // 注意：ZrObjectNewCustomized 已经调用了 ZrHashSetConstruct，这里只需要调用 ZrHashSetInit
    ZrHashSetInit(state, &module->super.nodeMap, ZR_OBJECT_TABLE_INIT_SIZE_LOG2);
    
    // 初始化 proNodeMap（用于 pro 导出）
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


// zr.import native 函数实现
TInt64 ZrImportNativeFunction(SZrState *state) {
    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }
    
    // 获取函数参数（路径字符串）
    TZrStackValuePointer functionBase = state->callInfoList->functionBase.valuePointer;
    TZrStackValuePointer argBase = functionBase + 1;
#define ZR_RETURN_IMPORT_RESULT()       \
    do {                                \
        state->stackTop.valuePointer = functionBase + 1; \
        return 1;                       \
    } while (0)
    
    // 检查参数数量
    if (state->stackTop.valuePointer <= argBase) {
        // 没有参数，返回 null
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        ZR_RETURN_IMPORT_RESULT();
    }
    
    // 获取路径参数
    SZrTypeValue *pathValue = ZrStackGetValue(argBase);
    if (pathValue->type != ZR_VALUE_TYPE_STRING) {
        // 参数类型错误，返回 null
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        ZR_RETURN_IMPORT_RESULT();
    }
    
    SZrString *path = ZR_CAST_STRING(state, pathValue->value.object);
    if (path == ZR_NULL) {
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        ZR_RETURN_IMPORT_RESULT();
    }
    
    // 检查缓存
    struct SZrObjectModule *cachedModule = ZrModuleGetFromCache(state, path);
    if (cachedModule != ZR_NULL) {
        // 缓存命中，返回缓存的模块
        SZrTypeValue *result = ZrStackGetValue(functionBase);
        ZrValueInitAsRawObject(state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(cachedModule));
        result->type = ZR_VALUE_TYPE_OBJECT;
        ZR_RETURN_IMPORT_RESULT();
    }
    
    // 缓存未命中，需要加载、编译和执行
    SZrGlobalState *global = state->global;
    if (global->sourceLoader == ZR_NULL) {
        // 没有源加载器，返回 null
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        ZR_RETURN_IMPORT_RESULT();
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
        ZR_RETURN_IMPORT_RESULT();
    }
    
    // 加载源文件
    SZrIo io;
    TBool loadSuccess = global->sourceLoader(state, pathStr, ZR_NULL, &io);
    if (!loadSuccess) {
        // 加载失败，返回 null
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        ZR_RETURN_IMPORT_RESULT();
    }
    
    SZrFunction *func = ZR_NULL;

    if (io.isBinary) {
        SZrIoSource *ioSource = ZrIoReadSourceNew(&io);
        if (io.close != ZR_NULL) {
            io.close(state, io.customData);
        }

        if (ioSource == ZR_NULL) {
            ZrValueResetAsNull(ZrStackGetValue(functionBase));
            ZR_RETURN_IMPORT_RESULT();
        }

        func = load_entry_function_from_io_source(state, ioSource);
        ZrIoReadSourceFree(global, ioSource);
        if (func == ZR_NULL) {
            ZrValueResetAsNull(ZrStackGetValue(functionBase));
            ZR_RETURN_IMPORT_RESULT();
        }
    } else {
        // 读取整个源文件内容
        TZrSize sourceSize = 0;
        TBytePtr sourceBuffer = read_all_from_io(state, &io, &sourceSize);
        if (sourceBuffer == ZR_NULL) {
            if (io.close != ZR_NULL) {
                io.close(state, io.customData);
            }
            ZrValueResetAsNull(ZrStackGetValue(functionBase));
            ZR_RETURN_IMPORT_RESULT();
        }

        if (io.close != ZR_NULL) {
            io.close(state, io.customData);
        }

        if (sourceSize == 0) {
            ZrMemoryRawFreeWithType(global, sourceBuffer, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            ZrValueResetAsNull(ZrStackGetValue(functionBase));
            ZR_RETURN_IMPORT_RESULT();
        }

        // 源代码文件，需要parser和compiler
        if (global->compileSource == ZR_NULL) {
            // 没有parser/compiler，无法处理源代码文件
            ZrMemoryRawFreeWithType(global, sourceBuffer, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            ZrValueResetAsNull(ZrStackGetValue(functionBase));
            ZR_RETURN_IMPORT_RESULT();
        }
        
        // 编译源代码（封装了从解析到编译的全流程）
        SZrString *sourceName = ZrStringCreate(state, pathStr, pathLen);
        func = global->compileSource(state, (const TChar *)sourceBuffer, sourceSize, sourceName);
        ZrMemoryRawFreeWithType(global, sourceBuffer, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
        
        if (func == ZR_NULL) {
            // 编译失败，返回 null
            ZrValueResetAsNull(ZrStackGetValue(functionBase));
            ZR_RETURN_IMPORT_RESULT();
        }
    }
    
    // 创建闭包
    SZrClosure *closure = ZrClosureNew(state, 0);
    if (closure == ZR_NULL) {
        ZrFunctionFree(state, func);
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        ZR_RETURN_IMPORT_RESULT();
    }
    
    closure->function = func;
    
    // 创建模块对象（在执行 __entry 之前创建，以便在运行时可以访问）
    struct SZrObjectModule *module = ZrModuleCreate(state);
    if (module == ZR_NULL) {
        // sourceBuffer已经在编译后释放，不需要再次释放
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        ZR_RETURN_IMPORT_RESULT();
    }
    
    // 设置模块信息
    TUInt64 pathHash = ZrModuleCalculatePathHash(state, path);
    ZrModuleSetInfo(state, module, ZR_NULL, pathHash, path);

    if (func != ZR_NULL) {
        ZrModuleCreatePrototypesFromConstants(state, module, func);
    }

    ZrModuleAddToCache(state, path, module);
    
    // 执行函数（调用 __entry）
    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
    TZrStackValuePointer callBase = savedStackTop;
    ZrStackSetRawObjectValue(state, callBase, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    state->stackTop.valuePointer = callBase + 1;
    ZrFunctionCall(state, callBase, 0);
    
    // 执行后，栈上的变量值已经可用
    // 收集 pub 和 pro 变量到 module object
    if (func != ZR_NULL && func->exportedVariables != ZR_NULL && func->exportedVariableLength > 0) {
        TZrStackValuePointer exportedValuesTop = callBase + 1 + func->stackSize;
        for (TUInt32 i = 0; i < func->exportedVariableLength; i++) {
            struct SZrFunctionExportedVariable *exportVar = &func->exportedVariables[i];
            if (exportVar->name != ZR_NULL) {
                // 从栈上获取变量值（需要根据 stackSlot 计算位置）
                // 注意：stackSlot 是相对于 functionBase 的偏移
                TZrStackValuePointer varPointer = callBase + 1 + exportVar->stackSlot;
                if (varPointer < exportedValuesTop) {
                    SZrTypeValue *varValue = ZrStackGetValue(varPointer);
                    if (varValue != ZR_NULL) {
                        if (exportVar->accessModifier == ZR_ACCESS_CONSTANT_PUBLIC) {
                            ZrModuleAddPubExport(state, module, exportVar->name, varValue);
                        } else if (exportVar->accessModifier == ZR_ACCESS_CONSTANT_PROTECTED) {
                            ZrModuleAddProExport(state, module, exportVar->name, varValue);
                        }
                    }
                }
            }
        }
    }
    
    // 返回模块对象
    SZrTypeValue *result = ZrStackGetValue(functionBase);
    ZrValueInitAsRawObject(state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(module));
    result->type = ZR_VALUE_TYPE_OBJECT;

    ZR_UNUSED_PARAMETER(savedStackTop);
    ZR_RETURN_IMPORT_RESULT();
#undef ZR_RETURN_IMPORT_RESULT
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
#define ZR_RETURN_CREATE_PROTOTYPE_RESULT() \
    do {                                     \
        state->stackTop.valuePointer = functionBase + 1; \
        return 1;                            \
    } while (0)
    
    // 检查参数数量（至少需要 4 个参数：module, typeName, prototypeType, accessModifier）
    if (state->stackTop.valuePointer <= argBase + 3) {
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }
    
    // 获取 module 参数
    SZrTypeValue *moduleValue = ZrStackGetValue(argBase);
    if (moduleValue->type != ZR_VALUE_TYPE_OBJECT) {
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }
    
    SZrObject *moduleObject = ZR_CAST_OBJECT(state, moduleValue->value.object);
    if (moduleObject == ZR_NULL || moduleObject->internalType != ZR_OBJECT_INTERNAL_TYPE_MODULE) {
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }
    
    struct SZrObjectModule *module = (struct SZrObjectModule *)moduleObject;
    
    // 获取 typeName 参数
    SZrTypeValue *typeNameValue = ZrStackGetValue(argBase + 1);
    if (typeNameValue->type != ZR_VALUE_TYPE_STRING) {
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }
    
    SZrString *typeName = ZR_CAST_STRING(state, typeNameValue->value.object);
    if (typeName == ZR_NULL) {
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }
    
    // 获取 prototypeType 参数
    SZrTypeValue *typeValue = ZrStackGetValue(argBase + 2);
    if (!ZR_VALUE_IS_TYPE_INT(typeValue->type)) {
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }
    
    EZrObjectPrototypeType prototypeType = (EZrObjectPrototypeType)typeValue->value.nativeObject.nativeUInt64;
    if (prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT && prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }
    
    // 获取 accessModifier 参数
    SZrTypeValue *accessModifierValue = ZrStackGetValue(argBase + 3);
    if (!ZR_VALUE_IS_TYPE_INT(accessModifierValue->type)) {
        ZrValueResetAsNull(ZrStackGetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
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
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }
    
    // 注册到模块导出
    SZrTypeValue prototypeValue;
    ZrValueInitAsRawObject(state, &prototypeValue, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    prototypeValue.type = ZR_VALUE_TYPE_OBJECT;
    
    if (accessModifier == ZR_ACCESS_CONSTANT_PUBLIC) {
        ZrModuleAddPubExport(state, module, typeName, &prototypeValue);
    } else if (accessModifier == ZR_ACCESS_CONSTANT_PROTECTED) {
        ZrModuleAddProExport(state, module, typeName, &prototypeValue);
    } else {
        // PRIVATE 不导出
        // 但仍然创建 prototype 对象
    }
    
    // 返回 prototype 对象
    SZrTypeValue *result = ZrStackGetValue(functionBase);
    ZrValueCopy(state, result, &prototypeValue);

    ZR_RETURN_CREATE_PROTOTYPE_RESULT();
#undef ZR_RETURN_CREATE_PROTOTYPE_RESULT
}

// 辅助函数：从序列化的二进制数据解析prototype信息
// 数据格式：SZrCompiledPrototypeInfo(24字节) + [inheritsCount * TUInt32] + [membersCount * SZrCompiledMemberInfo(44字节)]
// 需要从entryFunction的常量池中读取字符串索引对应的字符串
static TBool parse_compiled_prototype_info(struct SZrState *state,
                                           struct SZrFunction *entryFunction,
                                           const TByte *serializedData,
                                           TZrSize dataSize,
                                           SZrPrototypeCreationInfo *protoInfo) {
    if (state == ZR_NULL || entryFunction == ZR_NULL || serializedData == ZR_NULL || 
        dataSize < sizeof(SZrCompiledPrototypeInfo) || protoInfo == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 读取prototype信息结构
    const SZrCompiledPrototypeInfo *protoInfoHeader = (const SZrCompiledPrototypeInfo *)serializedData;
    TUInt32 nameStringIndex = protoInfoHeader->nameStringIndex;
    TUInt32 type = protoInfoHeader->type;
    TUInt32 accessModifier = protoInfoHeader->accessModifier;
    TUInt32 inheritsCount = protoInfoHeader->inheritsCount;
    TUInt32 membersCount = protoInfoHeader->membersCount;
    
    // 验证数据大小是否足够
    TZrSize expectedSize = sizeof(SZrCompiledPrototypeInfo) + 
                           inheritsCount * sizeof(TUInt32) + 
                           membersCount * sizeof(SZrCompiledMemberInfo);
    if (dataSize < expectedSize) {
        return ZR_FALSE;
    }
    
    // 从常量池读取类型名称
    if (nameStringIndex >= entryFunction->constantValueLength) {
        return ZR_FALSE;
    }
    const SZrTypeValue *nameConstant = &entryFunction->constantValueList[nameStringIndex];
    if (nameConstant->type != ZR_VALUE_TYPE_STRING) {
        return ZR_FALSE;
    }
    SZrString *typeName = ZR_CAST_STRING(state, nameConstant->value.object);
    if (typeName == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 填充protoInfo结构
    protoInfo->typeName = typeName;
    protoInfo->prototypeType = (EZrObjectPrototypeType)type;
    protoInfo->accessModifier = (EZrAccessModifier)accessModifier;
    protoInfo->prototype = ZR_NULL;  // 稍后创建
    protoInfo->membersCount = membersCount;
    
    // 初始化继承类型名称数组
    ZrArrayInit(state, &protoInfo->inheritTypeNames, sizeof(SZrString *), inheritsCount);
    
    // 读取继承类型名称索引数组
    const TUInt32 *inheritIndices = (const TUInt32 *)(serializedData + sizeof(SZrCompiledPrototypeInfo));
    for (TUInt32 i = 0; i < inheritsCount; i++) {
        TUInt32 inheritStringIndex = inheritIndices[i];
        if (inheritStringIndex > 0 && inheritStringIndex < entryFunction->constantValueLength) {
            const SZrTypeValue *inheritConstant = &entryFunction->constantValueList[inheritStringIndex];
            if (inheritConstant->type == ZR_VALUE_TYPE_STRING) {
                SZrString *inheritTypeName = ZR_CAST_STRING(state, inheritConstant->value.object);
                if (inheritTypeName != ZR_NULL) {
                    ZrArrayPush(state, &protoInfo->inheritTypeNames, &inheritTypeName);
                }
            }
        }
    }
    
    // 读取成员信息（只存储指针，稍后处理）
    // 成员数据紧跟在继承数组后面
    // 成员信息存储在序列化数据中，但完整处理需要函数引用解析、字段偏移计算等
    // 这里先读取成员信息指针，完整处理在ZrModuleCreatePrototypesFromData中进行
    // const TByte *membersData = serializedData + sizeof(SZrCompiledPrototypeInfo) + inheritsCount * sizeof(TUInt32);
    // const SZrCompiledMemberInfo *members = (const SZrCompiledMemberInfo *)membersData;
    
    protoInfo->membersCount = membersCount;
    protoInfo->members =
            (const SZrCompiledMemberInfo *)(serializedData + sizeof(SZrCompiledPrototypeInfo) +
                                            inheritsCount * sizeof(TUInt32));
    
    return ZR_TRUE;
}

// 从编译后的函数的prototypeData中解析prototype信息并创建prototype实例
// 实现两遍创建机制：第一遍创建所有prototype，第二遍设置继承关系和成员信息
TZrSize ZrModuleCreatePrototypesFromData(struct SZrState *state, 
                                         struct SZrObjectModule *module,
                                         struct SZrFunction *entryFunction) {
    if (state == ZR_NULL || module == ZR_NULL || entryFunction == ZR_NULL) {
        return 0;
    }
    
    // 检查prototypeData是否有效
    if (entryFunction->prototypeData == ZR_NULL || entryFunction->prototypeDataLength == 0 || 
        entryFunction->prototypeCount == 0) {
        return 0;
    }
    
    // 检查常量池是否有效（用于字符串索引解析）
    if (entryFunction->constantValueList == ZR_NULL || entryFunction->constantValueLength == 0) {
        return 0;
    }
    
    TUInt32 prototypeCount = entryFunction->prototypeCount;
    
    // 第一遍：创建所有prototype实例（不设置继承关系）
    SZrArray prototypeInfos;
    ZrArrayInit(state, &prototypeInfos, sizeof(SZrPrototypeCreationInfo), prototypeCount);
    
    TZrSize createdCount = 0;
    
    // 从prototypeData读取数据（跳过头部的prototypeCount）
    const TByte *prototypeData = entryFunction->prototypeData + sizeof(TUInt32);
    TZrSize remainingDataSize = entryFunction->prototypeDataLength - sizeof(TUInt32);
    const TByte *currentPos = prototypeData;
    
    // 遍历每个prototype
    for (TUInt32 i = 0; i < prototypeCount; i++) {
        if (remainingDataSize < sizeof(SZrCompiledPrototypeInfo)) {
            break;  // 数据不足，退出
        }
        
        // 解析SZrCompiledPrototypeInfo
        const SZrCompiledPrototypeInfo *protoInfo = (const SZrCompiledPrototypeInfo *)currentPos;
        TUInt32 inheritsCount = protoInfo->inheritsCount;
        TUInt32 membersCount = protoInfo->membersCount;
        
        // 计算当前prototype数据的大小
        TZrSize inheritArraySize = inheritsCount * sizeof(TUInt32);
        TZrSize membersArraySize = membersCount * sizeof(SZrCompiledMemberInfo);
        TZrSize currentPrototypeSize = sizeof(SZrCompiledPrototypeInfo) + inheritArraySize + membersArraySize;
        
        if (remainingDataSize < currentPrototypeSize) {
            break;  // 数据不足，退出
        }
        
        // 解析二进制数据并创建prototype信息结构
        SZrPrototypeCreationInfo protoInfoData;
        protoInfoData.prototype = ZR_NULL;
        protoInfoData.typeName = ZR_NULL;
        protoInfoData.prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INVALID;
        protoInfoData.accessModifier = ZR_ACCESS_CONSTANT_PRIVATE;
        ZrArrayInit(state, &protoInfoData.inheritTypeNames, sizeof(SZrString *), 4);
        protoInfoData.members = ZR_NULL;
        protoInfoData.membersCount = 0;
        
        if (parse_compiled_prototype_info(state, entryFunction, currentPos, currentPrototypeSize, &protoInfoData)) {
            // 创建prototype对象（第一遍：不设置继承关系）
            if (protoInfoData.typeName != ZR_NULL && protoInfoData.prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_INVALID) {
                SZrObjectPrototype *prototype = ZR_NULL;
                if (protoInfoData.prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
                    prototype = (SZrObjectPrototype *)ZrStructPrototypeNew(state, protoInfoData.typeName);
                } else if (protoInfoData.prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
                    prototype = ZrObjectPrototypeNew(state, protoInfoData.typeName, protoInfoData.prototypeType);
                    // 初始化元表
                    ZrObjectPrototypeInitMetaTable(state, prototype);
                }
                
                if (prototype != ZR_NULL) {
                    protoInfoData.prototype = prototype;
                    
                    // 注册到模块导出（先注册，第二遍可能还需要查找）
                    SZrTypeValue prototypeValue;
                    ZrValueInitAsRawObject(state, &prototypeValue, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
                    prototypeValue.type = ZR_VALUE_TYPE_OBJECT;
                    
                    if (protoInfoData.accessModifier == ZR_ACCESS_CONSTANT_PUBLIC) {
                        ZrModuleAddPubExport(state, module, protoInfoData.typeName, &prototypeValue);
                    } else if (protoInfoData.accessModifier == ZR_ACCESS_CONSTANT_PROTECTED) {
                        ZrModuleAddProExport(state, module, protoInfoData.typeName, &prototypeValue);
                    }
                    // PRIVATE 不导出，但仍然创建prototype对象
                    register_prototype_in_global_scope(state, protoInfoData.typeName, &prototypeValue);
                    
                    // 保存prototype信息到数组，用于第二遍处理继承关系和成员信息
                    ZrArrayPush(state, &prototypeInfos, &protoInfoData);
                    createdCount++;
                } else {
                    // 创建失败，清理资源
                    ZrArrayFree(state, &protoInfoData.inheritTypeNames);
                }
            } else {
                // 解析失败，清理资源
                ZrArrayFree(state, &protoInfoData.inheritTypeNames);
            }
        }
        
        // 移动到下一个prototype数据
        currentPos += currentPrototypeSize;
        remainingDataSize -= currentPrototypeSize;
    }
    
    // 第二遍：设置继承关系和成员信息
    for (TZrSize i = 0; i < prototypeInfos.length; i++) {
        SZrPrototypeCreationInfo *protoInfo = (SZrPrototypeCreationInfo *)ZrArrayGet(&prototypeInfos, i);
        if (protoInfo == ZR_NULL || protoInfo->prototype == ZR_NULL) {
            continue;
        }
        
        // 处理继承关系（只支持单个基类，TODO: 支持多重继承）
        if (protoInfo->inheritTypeNames.length > 0) {
            SZrString **inheritTypeNamePtr = (SZrString **)ZrArrayGet(&protoInfo->inheritTypeNames, 0);
            if (inheritTypeNamePtr != ZR_NULL && *inheritTypeNamePtr != ZR_NULL) {
                SZrObjectPrototype *superPrototype =
                        find_local_created_prototype_by_name(&prototypeInfos, *inheritTypeNamePtr);
                if (superPrototype == ZR_NULL) {
                    superPrototype = find_prototype_by_name(state, module, *inheritTypeNamePtr);
                }
                if (superPrototype != ZR_NULL) {
                    ZrObjectPrototypeSetSuper(state, protoInfo->prototype, superPrototype);
                }
            }
        }
        
        // 处理成员信息（字段、方法、构造函数）
        if (protoInfo->members != ZR_NULL && entryFunction->constantValueList != ZR_NULL) {
            for (TUInt32 memberIndex = 0; memberIndex < protoInfo->membersCount; memberIndex++) {
                const SZrCompiledMemberInfo *member = &protoInfo->members[memberIndex];
                if (member == ZR_NULL || member->nameStringIndex >= entryFunction->constantValueLength) {
                    continue;
                }

                const SZrTypeValue *nameConstant = &entryFunction->constantValueList[member->nameStringIndex];
                if (nameConstant->type != ZR_VALUE_TYPE_STRING) {
                    continue;
                }

                SZrString *memberName = ZR_CAST_STRING(state, nameConstant->value.object);
                if (memberName == ZR_NULL) {
                    continue;
                }

                if (member->memberType == ZR_AST_CONSTANT_STRUCT_FIELD ||
                    member->memberType == ZR_AST_CONSTANT_CLASS_FIELD) {
                    if (protoInfo->prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT &&
                        member->memberType == ZR_AST_CONSTANT_STRUCT_FIELD) {
                        ZrStructPrototypeAddField(state,
                                                  (SZrStructPrototype *)protoInfo->prototype,
                                                  memberName,
                                                  member->fieldOffset);
                    }

                    if (member->isUsingManaged) {
                        ZrObjectPrototypeAddManagedField(state,
                                                         protoInfo->prototype,
                                                         memberName,
                                                         member->fieldOffset,
                                                         member->fieldSize,
                                                         member->ownershipQualifier,
                                                         member->callsClose ? ZR_TRUE : ZR_FALSE,
                                                         member->callsDestructor ? ZR_TRUE : ZR_FALSE,
                                                         member->declarationOrder);
                    }
                    continue;
                }

                if ((member->memberType == ZR_AST_CONSTANT_CLASS_METHOD ||
                     member->memberType == ZR_AST_CONSTANT_STRUCT_METHOD ||
                     member->memberType == ZR_AST_CONSTANT_CLASS_META_FUNCTION ||
                     member->memberType == ZR_AST_CONSTANT_STRUCT_META_FUNCTION) &&
                    member->functionConstantIndex < entryFunction->constantValueLength) {
                    const SZrTypeValue *functionConstant =
                            &entryFunction->constantValueList[member->functionConstantIndex];
                    SZrFunction *function = get_function_from_constant(state, functionConstant);
                    if (function == ZR_NULL) {
                        continue;
                    }

                    if (member->isMetaMethod) {
                        ZrObjectPrototypeAddMeta(state, protoInfo->prototype, (EZrMetaType)member->metaType, function);
                        if (member->metaType == ZR_META_CONSTRUCTOR) {
                            SZrString *constructorName = ZrStringCreateFromNative(state, "__constructor");
                            if (constructorName != ZR_NULL) {
                                SZrTypeValue constructorKey;
                                SZrTypeValue constructorValue;
                                ZrValueInitAsRawObject(state, &constructorKey,
                                                       ZR_CAST_RAW_OBJECT_AS_SUPER(constructorName));
                                constructorKey.type = ZR_VALUE_TYPE_STRING;
                                ZrValueInitAsRawObject(state, &constructorValue,
                                                       ZR_CAST_RAW_OBJECT_AS_SUPER(function));
                                constructorValue.type = functionConstant->type;
                                ZrObjectSetValue(state, &protoInfo->prototype->super, &constructorKey,
                                                 &constructorValue);
                            }
                        }
                        continue;
                    }

                    SZrTypeValue methodValue;
                    ZrValueInitAsRawObject(state, &methodValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
                    methodValue.type = functionConstant->type;

                    SZrTypeValue methodKey;
                    ZrValueInitAsRawObject(state, &methodKey, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
                    methodKey.type = ZR_VALUE_TYPE_STRING;
                    ZrObjectSetValue(state, &protoInfo->prototype->super, &methodKey, &methodValue);
                }
            }
        }
        
        // 清理继承类型名称数组
        ZrArrayFree(state, &protoInfo->inheritTypeNames);
    }
    
    // 清理prototype信息数组
    ZrArrayFree(state, &prototypeInfos);
    return createdCount;
}

// 向后兼容：保留旧函数名，内部调用新函数
TZrSize ZrModuleCreatePrototypesFromConstants(struct SZrState *state, 
                                              struct SZrObjectModule *module,
                                              struct SZrFunction *entryFunction) {
    // 优先使用新的prototypeData机制
    if (entryFunction->prototypeData != ZR_NULL && entryFunction->prototypeDataLength > 0 && 
        entryFunction->prototypeCount > 0) {
        return ZrModuleCreatePrototypesFromData(state, module, entryFunction);
    }
    
    // 如果没有新格式数据，尝试从常量池读取（向后兼容）
    if (entryFunction->constantValueList == ZR_NULL || entryFunction->constantValueLength == 0) {
        return 0;
    }
    
    // 实现从常量池读取的旧逻辑（向后兼容）
    // 旧格式：prototype信息存储在常量池中，作为对象类型
    // 需要遍历常量池，查找prototype信息对象（internalType为ZR_OBJECT_INTERNAL_TYPE_PROTOTYPE_INFO）
    // 然后解析并创建prototype实例
    // 注意：由于新格式（prototypeData）已经实现，旧格式主要用于向后兼容
    // 如果不需要向后兼容，可以返回0
    // TODO: 这里暂时返回0，因为新格式已经足够，旧格式的完整实现需要：
    // 1. 遍历常量池查找prototype信息对象
    // 2. 从对象中读取prototype信息（类型、名称、成员等）
    // 3. 创建prototype实例并注册到模块
    // 如果需要向后兼容，可以后续实现
    return 0;
}
