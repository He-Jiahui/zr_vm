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
typedef TZrUInt8 EZrAccessModifier;

// 内部结构：存储prototype创建时的临时信息
typedef struct {
    SZrObjectPrototype *prototype;
    SZrString *typeName;
    EZrObjectPrototypeType prototypeType;
    EZrAccessModifier accessModifier;
    SZrArray inheritTypeNames;  // SZrString*数组，存储继承类型名称
    const SZrCompiledMemberInfo *members;
    TZrUInt32 membersCount;
    TZrBool needsPostCreateSetup;
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
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(typeName));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, zrObject, &key, prototypeValue);
}

static SZrObjectPrototype *find_prototype_in_global_scope(SZrState *state, SZrString *typeName) {
    SZrObject *zrObject;
    SZrTypeValue key;
    const SZrTypeValue *registeredValue;
    SZrObjectPrototype *prototype;

    if (state == ZR_NULL || state->global == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    if (state->global->zrObject.type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }

    zrObject = ZR_CAST_OBJECT(state, state->global->zrObject.value.object);
    if (zrObject == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(typeName));
    key.type = ZR_VALUE_TYPE_STRING;
    registeredValue = ZrCore_Object_GetValue(state, zrObject, &key);
    if (registeredValue == ZR_NULL || registeredValue->type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }

    prototype = (SZrObjectPrototype *)ZR_CAST_OBJECT(state, registeredValue->value.object);
    if (prototype == ZR_NULL || prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_INVALID) {
        return ZR_NULL;
    }

    return prototype;
}

static TZrBool ensure_prototype_instance_storage(SZrState *state, SZrFunction *entryFunction) {
    struct SZrObjectPrototype **newStorage;
    TZrSize storageBytes;

    if (state == ZR_NULL || state->global == ZR_NULL || entryFunction == ZR_NULL || entryFunction->prototypeCount == 0) {
        return ZR_FALSE;
    }

    if (entryFunction->prototypeInstances != ZR_NULL &&
        entryFunction->prototypeInstancesLength >= entryFunction->prototypeCount) {
        return ZR_TRUE;
    }

    storageBytes = entryFunction->prototypeCount * sizeof(struct SZrObjectPrototype *);
    newStorage = (struct SZrObjectPrototype **)ZrCore_Memory_RawMalloc(state->global, storageBytes);
    if (newStorage == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(newStorage, 0, storageBytes);
    if (entryFunction->prototypeInstances != ZR_NULL && entryFunction->prototypeInstancesLength > 0) {
        TZrSize copyCount = entryFunction->prototypeInstancesLength;
        if (copyCount > entryFunction->prototypeCount) {
            copyCount = entryFunction->prototypeCount;
        }
        ZrCore_Memory_RawCopy(newStorage,
                              entryFunction->prototypeInstances,
                              copyCount * sizeof(struct SZrObjectPrototype *));
        ZrCore_Memory_RawFree(state->global,
                              entryFunction->prototypeInstances,
                              entryFunction->prototypeInstancesLength * sizeof(struct SZrObjectPrototype *));
    }

    entryFunction->prototypeInstances = newStorage;
    entryFunction->prototypeInstancesLength = entryFunction->prototypeCount;
    return ZR_TRUE;
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

// 辅助函数：从模块中查找prototype（用于继承关系）
static SZrObjectPrototype *find_prototype_by_name(struct SZrState *state, 
                                                   struct SZrObjectModule *module,
                                                   SZrString *typeName) {
    if (state == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }
    
    if (module != ZR_NULL) {
        const SZrTypeValue *exported = ZrCore_Module_GetProExport(state, module, typeName);
        if (exported != ZR_NULL && exported->type == ZR_VALUE_TYPE_OBJECT) {
            SZrObjectPrototype *proto = (SZrObjectPrototype *)ZR_CAST_OBJECT(state, exported->value.object);
            if (proto != ZR_NULL && proto->type != ZR_OBJECT_PROTOTYPE_TYPE_INVALID) {
                return proto;
            }
        }
    }

    {
        SZrObjectPrototype *globalPrototype = find_prototype_in_global_scope(state, typeName);
        if (globalPrototype != ZR_NULL) {
            return globalPrototype;
        }
    }
    
    // 在导入模块中查找：遍历全局模块注册表，查找所有已加载的模块
    // 注意：由于模块对象没有存储导入列表，我们遍历所有模块查找匹配的prototype
    // TODO: 这是一个简化的实现，更高效的方法需要在模块对象中存储导入列表
    if (state->global != ZR_NULL) {
        SZrGlobalState *global = state->global;
        if (ZrCore_Value_IsGarbageCollectable(&global->loadedModulesRegistry) &&
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
                                const SZrTypeValue *importedExported = ZrCore_Module_GetPubExport(state, importedModule, typeName);
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
                (SZrPrototypeCreationInfo *)ZrCore_Array_Get(prototypeInfos, i);
        if (protoInfo == ZR_NULL || protoInfo->prototype == ZR_NULL || protoInfo->typeName == ZR_NULL) {
            continue;
        }

        if (ZrCore_String_Equal(protoInfo->typeName, typeName)) {
            return protoInfo->prototype;
        }
    }

    return ZR_NULL;
}

static TZrBool refill_io_chunk(SZrIo *io) {
    if (io == ZR_NULL || io->read == ZR_NULL || io->state == ZR_NULL) {
        return ZR_FALSE;
    }

    SZrState *state = io->state;
    TZrSize readSize = 0;
    ZR_THREAD_UNLOCK(state);
    TZrBytePtr buffer = io->read(state, io->customData, &readSize);
    ZR_THREAD_LOCK(state);
    if (buffer == ZR_NULL || readSize == 0) {
        return ZR_FALSE;
    }

    io->pointer = buffer;
    io->remained = readSize;
    return ZR_TRUE;
}

static TZrBytePtr read_all_from_io(SZrState *state, SZrIo *io, TZrSize *outSize) {
    if (state == ZR_NULL || io == ZR_NULL || outSize == ZR_NULL) {
        return ZR_NULL;
    }

    SZrGlobalState *global = state->global;
    TZrSize capacity = (io->remained > 0) ? io->remained : 4096;
    TZrBytePtr buffer = (TZrBytePtr)ZrCore_Memory_RawMallocWithType(global, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
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

            TZrBytePtr newBuffer =
                    (TZrBytePtr)ZrCore_Memory_RawMallocWithType(global, newCapacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            if (newBuffer == ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(global, buffer, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
                return ZR_NULL;
            }

            if (totalSize > 0) {
                ZrCore_Memory_RawCopy(newBuffer, buffer, totalSize);
            }
            ZrCore_Memory_RawFreeWithType(global, buffer, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            buffer = newBuffer;
            capacity = newCapacity;
        }

        ZrCore_Memory_RawCopy(buffer + totalSize, io->pointer, io->remained);
        totalSize += io->remained;
        io->pointer += io->remained;
        io->remained = 0;
    }

    buffer[totalSize] = '\0';
    *outSize = totalSize;
    return buffer;
}

static void init_inline_runtime_function(SZrFunction *function) {
    ZrCore_Memory_RawSet(function, 0, sizeof(SZrFunction));
    ZrCore_RawObject_Construct(&function->super, ZR_RAW_OBJECT_TYPE_FUNCTION);
}

static TZrBool populate_runtime_function(SZrState *state, const SZrIoFunction *source, SZrFunction *function);

static TZrBool convert_io_constant_to_runtime_value(SZrState *state, const SZrIoFunctionConstantVariable *source,
                                                  SZrTypeValue *destination) {
    if (state == ZR_NULL || source == ZR_NULL || destination == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (source->type) {
        case ZR_VALUE_TYPE_NULL: {
            ZrCore_Value_ResetAsNull(destination);
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
            ZrCore_Value_InitAsRawObject(state, destination, source->value.object);
            destination->type = ZR_VALUE_TYPE_STRING;
            return ZR_TRUE;
        }
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE: {
            if (!source->hasFunctionValue || source->functionValue == ZR_NULL) {
                return ZR_FALSE;
            }

            SZrFunction *functionValue = ZrCore_Function_New(state);
            if (functionValue == ZR_NULL) {
                return ZR_FALSE;
            }
            if (!populate_runtime_function(state, source->functionValue, functionValue)) {
                ZrCore_Function_Free(state, functionValue);
                return ZR_FALSE;
            }

            if (source->type == ZR_VALUE_TYPE_CLOSURE) {
                SZrClosure *closure = ZrCore_Closure_New(state, 0);
                if (closure == ZR_NULL) {
                    ZrCore_Function_Free(state, functionValue);
                    return ZR_FALSE;
                }
                closure->function = functionValue;
                ZrCore_Closure_InitValue(state, closure);
                ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
                destination->type = ZR_VALUE_TYPE_CLOSURE;
            } else {
                ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(functionValue));
                destination->type = ZR_VALUE_TYPE_FUNCTION;
            }
            return ZR_TRUE;
        }
        default: {
            return ZR_FALSE;
        }
    }
}

static TZrBool populate_runtime_function(SZrState *state, const SZrIoFunction *source, SZrFunction *function) {
    if (state == ZR_NULL || source == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    SZrGlobalState *global = state->global;
    function->functionName = source->name;
    function->parameterCount = (TZrUInt16) source->parametersLength;
    function->hasVariableArguments = source->hasVarArgs ? ZR_TRUE : ZR_FALSE;
    function->stackSize = source->stackSize;
    function->lineInSourceStart = (TZrUInt32) source->startLine;
    function->lineInSourceEnd = (TZrUInt32) source->endLine;

    if (source->instructionsLength > 0) {
        TZrSize instructionBytes = sizeof(TZrInstruction) * source->instructionsLength;
        function->instructionsList =
                (TZrInstruction *) ZrCore_Memory_RawMallocWithType(global, instructionBytes, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->instructionsList == ZR_NULL) {
            ZrCore_Function_Free(state, function);
            return ZR_FALSE;
        }
        ZrCore_Memory_RawCopy(function->instructionsList, source->instructions, instructionBytes);
        function->instructionsLength = (TZrUInt32) source->instructionsLength;
    }

    if (source->localVariablesLength > 0) {
        TZrSize localBytes = sizeof(SZrFunctionLocalVariable) * source->localVariablesLength;
        function->localVariableList = (SZrFunctionLocalVariable *) ZrCore_Memory_RawMallocWithType(
                global, localBytes, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->localVariableList == ZR_NULL) {
            ZrCore_Function_Free(state, function);
            return ZR_FALSE;
        }

        for (TZrSize i = 0; i < source->localVariablesLength; i++) {
            function->localVariableList[i].name = ZR_NULL;
            function->localVariableList[i].stackSlot = (TZrUInt32)i;
            function->localVariableList[i].offsetActivate = (TZrMemoryOffset) source->localVariables[i].instructionStartIndex;
            function->localVariableList[i].offsetDead = (TZrMemoryOffset) source->localVariables[i].instructionEndIndex;
        }
        function->localVariableLength = (TZrUInt32) source->localVariablesLength;
    }

    if (source->constantVariablesLength > 0) {
        TZrSize constantBytes = sizeof(SZrTypeValue) * source->constantVariablesLength;
        function->constantValueList =
                (SZrTypeValue *) ZrCore_Memory_RawMallocWithType(global, constantBytes, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->constantValueList == ZR_NULL) {
            ZrCore_Function_Free(state, function);
            return ZR_FALSE;
        }

        for (TZrSize i = 0; i < source->constantVariablesLength; i++) {
            if (!convert_io_constant_to_runtime_value(state, &source->constantVariables[i],
                                                      &function->constantValueList[i])) {
                ZrCore_Function_Free(state, function);
                return ZR_FALSE;
            }
        }
        function->constantValueLength = (TZrUInt32) source->constantVariablesLength;
    }

    if (source->exportedVariablesLength > 0) {
        TZrSize exportBytes = sizeof(struct SZrFunctionExportedVariable) * source->exportedVariablesLength;
        function->exportedVariables = (struct SZrFunctionExportedVariable *) ZrCore_Memory_RawMallocWithType(
                global, exportBytes, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->exportedVariables == ZR_NULL) {
            ZrCore_Function_Free(state, function);
            return ZR_FALSE;
        }

        for (TZrSize i = 0; i < source->exportedVariablesLength; i++) {
            function->exportedVariables[i].name = source->exportedVariables[i].name;
            function->exportedVariables[i].stackSlot = source->exportedVariables[i].stackSlot;
            function->exportedVariables[i].accessModifier = source->exportedVariables[i].accessModifier;
        }
        function->exportedVariableLength = (TZrUInt32) source->exportedVariablesLength;
    }

    if (source->closuresLength > 0) {
        TZrSize childBytes = sizeof(SZrFunction) * source->closuresLength;
        function->childFunctionList =
                (SZrFunction *) ZrCore_Memory_RawMallocWithType(global, childBytes, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->childFunctionList == ZR_NULL) {
            ZrCore_Function_Free(state, function);
            return ZR_FALSE;
        }

        for (TZrSize i = 0; i < source->closuresLength; i++) {
            init_inline_runtime_function(&function->childFunctionList[i]);
            if (source->closures[i].subFunction == ZR_NULL ||
                !populate_runtime_function(state, source->closures[i].subFunction, &function->childFunctionList[i])) {
                ZrCore_Function_Free(state, function);
                return ZR_FALSE;
            }
        }
        function->childFunctionLength = (TZrUInt32) source->closuresLength;
    }

    return ZR_TRUE;
}

static SZrFunction *convert_io_function_to_runtime(SZrState *state, const SZrIoFunction *source) {
    if (state == ZR_NULL || source == ZR_NULL) {
        return ZR_NULL;
    }

    SZrFunction *function = ZrCore_Function_New(state);
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    if (!populate_runtime_function(state, source, function)) {
        ZrCore_Function_Free(state, function);
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
struct SZrObjectModule *ZrCore_Module_Create(SZrState *state) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrObject *object = ZrCore_Object_NewCustomized(state, sizeof(struct SZrObjectModule), ZR_OBJECT_INTERNAL_TYPE_MODULE);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }
    
    struct SZrObjectModule *module = (struct SZrObjectModule *)object;
    
    // 初始化模块信息
    module->moduleName = ZR_NULL;
    module->pathHash = 0;
    module->fullPath = ZR_NULL;
    
    // 初始化 super.nodeMap（继承自 SZrObject，用于 pub 导出）
    // 注意：ZrCore_Object_NewCustomized 已经调用了 ZrCore_HashSet_Construct，这里只需要调用 ZrCore_HashSet_Init
    ZrCore_HashSet_Init(state, &module->super.nodeMap, ZR_OBJECT_TABLE_INIT_SIZE_LOG2);
    
    // 初始化 proNodeMap（用于 pro 导出）
    ZrCore_HashSet_Construct(&module->proNodeMap);
    ZrCore_HashSet_Init(state, &module->proNodeMap, ZR_OBJECT_TABLE_INIT_SIZE_LOG2);
    
    return module;
}

// 设置模块信息
void ZrCore_Module_SetInfo(SZrState *state, struct SZrObjectModule *module, 
                      SZrString *moduleName, TZrUInt64 pathHash, SZrString *fullPath) {
    if (state == ZR_NULL || module == ZR_NULL) {
        return;
    }
    
    module->moduleName = moduleName;
    module->pathHash = pathHash;
    module->fullPath = fullPath;
}

// 添加 pub 导出（同时添加到 pub 和 pro）
void ZrCore_Module_AddPubExport(SZrState *state, struct SZrObjectModule *module, 
                           SZrString *name, const SZrTypeValue *value) {
    if (state == ZR_NULL || module == ZR_NULL || name == ZR_NULL || value == ZR_NULL) {
        return;
    }
    
    // 创建键值
    SZrTypeValue key;
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(name));
    key.type = ZR_VALUE_TYPE_STRING;
    
    // 添加到 pub（super.nodeMap）
    ZrCore_Object_SetValue(state, &module->super, &key, value);
    
    // 同时添加到 pro（proNodeMap）
    SZrHashKeyValuePair *pair = ZrCore_HashSet_Find(state, &module->proNodeMap, &key);
    if (pair == ZR_NULL) {
        pair = ZrCore_HashSet_Add(state, &module->proNodeMap, &key);
    }
    ZrCore_Value_Copy(state, &pair->value, value);
}

// 添加 pro 导出（仅添加到 pro）
void ZrCore_Module_AddProExport(SZrState *state, struct SZrObjectModule *module, 
                           SZrString *name, const SZrTypeValue *value) {
    if (state == ZR_NULL || module == ZR_NULL || name == ZR_NULL || value == ZR_NULL) {
        return;
    }
    
    // 创建键值
    SZrTypeValue key;
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(name));
    key.type = ZR_VALUE_TYPE_STRING;
    
    // 仅添加到 pro（proNodeMap）
    SZrHashKeyValuePair *pair = ZrCore_HashSet_Find(state, &module->proNodeMap, &key);
    if (pair == ZR_NULL) {
        pair = ZrCore_HashSet_Add(state, &module->proNodeMap, &key);
    }
    ZrCore_Value_Copy(state, &pair->value, value);
}

// 获取 pub 导出（跨模块访问）
const SZrTypeValue *ZrCore_Module_GetPubExport(SZrState *state, struct SZrObjectModule *module, 
                                           SZrString *name) {
    if (state == ZR_NULL || module == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 创建键值
    SZrTypeValue key;
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(name));
    key.type = ZR_VALUE_TYPE_STRING;
    
    // 从 pub（super.nodeMap）获取
    return ZrCore_Object_GetValue(state, &module->super, &key);
}

// 获取 pro 导出（同模块库访问）
const SZrTypeValue *ZrCore_Module_GetProExport(SZrState *state, struct SZrObjectModule *module, 
                                           SZrString *name) {
    if (state == ZR_NULL || module == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 创建键值
    SZrTypeValue key;
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(name));
    key.type = ZR_VALUE_TYPE_STRING;
    
    // 从 pro（proNodeMap）获取
    SZrHashKeyValuePair *pair = ZrCore_HashSet_Find(state, &module->proNodeMap, &key);
    if (pair == ZR_NULL) {
        return ZR_NULL;
    }
    return &pair->value;
}

// 计算路径哈希（使用 xxhash）
TZrUInt64 ZrCore_Module_CalculatePathHash(SZrState *state, SZrString *fullPath) {
    ZR_UNUSED_PARAMETER(state);
    if (fullPath == ZR_NULL) {
        return 0;
    }
    
    // 获取字符串内容
    TZrNativeString pathStr;
    TZrSize pathLen;
    if (fullPath->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        pathStr = ZrCore_String_GetNativeStringShort(fullPath);
        pathLen = fullPath->shortStringLength;
    } else {
        pathStr = *ZrCore_String_GetNativeStringLong(fullPath);
        pathLen = fullPath->longStringLength;
    }
    
    if (pathStr == ZR_NULL || pathLen == 0) {
        return 0;
    }
    
    // 使用 XXH3_64bits 计算路径哈希（不使用seed，直接计算）
    return XXH3_64bits(pathStr, pathLen);
}

// 从缓存中获取模块
struct SZrObjectModule *ZrCore_Module_GetFromCache(SZrState *state, SZrString *path) {
    if (state == ZR_NULL || path == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrGlobalState *global = state->global;
    
    // 检查 loadedModulesRegistry 是否已初始化
    if (!ZrCore_Value_IsGarbageCollectable(&global->loadedModulesRegistry) ||
        global->loadedModulesRegistry.type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }
    
    SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    if (registry == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 创建键值
    SZrTypeValue key;
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(path));
    key.type = ZR_VALUE_TYPE_STRING;
    
    // 从缓存中查找
    const SZrTypeValue *cachedValue = ZrCore_Object_GetValue(state, registry, &key);
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
void ZrCore_Module_AddToCache(SZrState *state, SZrString *path, struct SZrObjectModule *module) {
    if (state == ZR_NULL || path == ZR_NULL || module == ZR_NULL || state->global == ZR_NULL) {
        return;
    }
    
    SZrGlobalState *global = state->global;
    
    // 检查 loadedModulesRegistry 是否已初始化
    if (!ZrCore_Value_IsGarbageCollectable(&global->loadedModulesRegistry) ||
        global->loadedModulesRegistry.type != ZR_VALUE_TYPE_OBJECT) {
        return;
    }
    
    SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    if (registry == ZR_NULL) {
        return;
    }
    
    // 创建键值
    SZrTypeValue key;
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(path));
    key.type = ZR_VALUE_TYPE_STRING;
    
    // 创建模块值
    SZrTypeValue moduleValue;
    ZrCore_Value_InitAsRawObject(state, &moduleValue, ZR_CAST_RAW_OBJECT_AS_SUPER(module));
    moduleValue.type = ZR_VALUE_TYPE_OBJECT;
    
    // 添加到缓存（覆盖旧缓存，支持热重载）
    ZrCore_Object_SetValue(state, registry, &key, &moduleValue);
}


struct SZrObjectModule *ZrCore_Module_ImportByPath(SZrState *state, SZrString *path) {
    struct SZrObjectModule *cachedModule;
    SZrGlobalState *global;
    TZrNativeString pathStr;
    TZrSize pathLen;
    SZrIo io;
    SZrFunction *func = ZR_NULL;
    SZrClosure *closure;
    struct SZrObjectModule *module;
    TZrUInt64 pathHash;
    TZrStackValuePointer savedStackTop;
    TZrStackValuePointer callBase;
    SZrFunctionStackAnchor callBaseAnchor;

    if (state == ZR_NULL || state->global == ZR_NULL || path == ZR_NULL) {
        return ZR_NULL;
    }

    cachedModule = ZrCore_Module_GetFromCache(state, path);
    if (cachedModule != ZR_NULL) {
        return cachedModule;
    }

    global = state->global;
    if (global->nativeModuleLoader != ZR_NULL) {
        struct SZrObjectModule *nativeModule =
                global->nativeModuleLoader(state, path, global->nativeModuleLoaderUserData);
        if (nativeModule != ZR_NULL) {
            if (nativeModule->fullPath == ZR_NULL || nativeModule->moduleName == ZR_NULL) {
                TZrUInt64 nativePathHash = ZrCore_Module_CalculatePathHash(state, path);
                ZrCore_Module_SetInfo(state, nativeModule, path, nativePathHash, path);
            }

            ZrCore_Module_AddToCache(state, path, nativeModule);
            return nativeModule;
        }
    }

    if (global->sourceLoader == ZR_NULL) {
        return ZR_NULL;
    }

    if (path->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        pathStr = ZrCore_String_GetNativeStringShort(path);
        pathLen = path->shortStringLength;
    } else {
        pathStr = *ZrCore_String_GetNativeStringLong(path);
        pathLen = path->longStringLength;
    }

    if (pathStr == ZR_NULL || pathLen == 0) {
        return ZR_NULL;
    }

    if (!global->sourceLoader(state, pathStr, ZR_NULL, &io)) {
        return ZR_NULL;
    }

    if (io.isBinary) {
        SZrIoSource *ioSource = ZrCore_Io_ReadSourceNew(&io);
        if (io.close != ZR_NULL) {
            io.close(state, io.customData);
        }

        if (ioSource == ZR_NULL) {
            return ZR_NULL;
        }

        func = load_entry_function_from_io_source(state, ioSource);
        ZrCore_Io_ReadSourceFree(global, ioSource);
        if (func == ZR_NULL) {
            return ZR_NULL;
        }
    } else {
        TZrSize sourceSize = 0;
        TZrBytePtr sourceBuffer = read_all_from_io(state, &io, &sourceSize);
        if (sourceBuffer == ZR_NULL) {
            if (io.close != ZR_NULL) {
                io.close(state, io.customData);
            }
            return ZR_NULL;
        }

        if (io.close != ZR_NULL) {
            io.close(state, io.customData);
        }

        if (sourceSize == 0) {
            ZrCore_Memory_RawFreeWithType(global, sourceBuffer, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            return ZR_NULL;
        }

        if (global->compileSource == ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global, sourceBuffer, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            return ZR_NULL;
        }

        {
            SZrString *sourceName = ZrCore_String_Create(state, pathStr, pathLen);
            func = global->compileSource(state, (const TZrChar *)sourceBuffer, sourceSize, sourceName);
        }
        ZrCore_Memory_RawFreeWithType(global, sourceBuffer, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);

        if (func == ZR_NULL) {
            return ZR_NULL;
        }
    }

    closure = ZrCore_Closure_New(state, 0);
    if (closure == ZR_NULL) {
        ZrCore_Function_Free(state, func);
        return ZR_NULL;
    }
    closure->function = func;

    module = ZrCore_Module_Create(state);
    if (module == ZR_NULL) {
        return ZR_NULL;
    }

    pathHash = ZrCore_Module_CalculatePathHash(state, path);
    ZrCore_Module_SetInfo(state, module, ZR_NULL, pathHash, path);

    if (func != ZR_NULL) {
        ZrCore_Module_CreatePrototypesFromConstants(state, module, func);
    }

    ZrCore_Module_AddToCache(state, path, module);

    savedStackTop = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndAnchor(state, 1, savedStackTop, savedStackTop, &callBaseAnchor);
    ZrCore_Stack_SetRawObjectValue(state, callBase, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    state->stackTop.valuePointer = callBase + 1;
    callBase = ZrCore_Function_CallAndRestoreAnchor(state, &callBaseAnchor, 0);

    if (func != ZR_NULL && func->exportedVariables != ZR_NULL && func->exportedVariableLength > 0) {
        TZrStackValuePointer exportedValuesTop = callBase + 1 + func->stackSize;
        for (TZrUInt32 i = 0; i < func->exportedVariableLength; i++) {
            struct SZrFunctionExportedVariable *exportVar = &func->exportedVariables[i];
            if (exportVar->name == ZR_NULL) {
                continue;
            }

            TZrStackValuePointer varPointer = callBase + 1 + exportVar->stackSlot;
            if (varPointer >= exportedValuesTop) {
                continue;
            }

            {
                SZrTypeValue *varValue = ZrCore_Stack_GetValue(varPointer);
                if (varValue == ZR_NULL) {
                    continue;
                }

                if (exportVar->accessModifier == ZR_ACCESS_CONSTANT_PUBLIC) {
                    ZrCore_Module_AddPubExport(state, module, exportVar->name, varValue);
                } else if (exportVar->accessModifier == ZR_ACCESS_CONSTANT_PROTECTED) {
                    ZrCore_Module_AddProExport(state, module, exportVar->name, varValue);
                }

            }
        }
    }

    state->stackTop.valuePointer = savedStackTop;
    return module;
}

// 创建并注册 prototype 的 native 函数
// 参数: (module, typeName, prototypeType, accessModifier)
// 返回: prototype 对象
TZrInt64 ZrCore_PrototypeNativeFunction_Create(SZrState *state) {
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
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }
    
    // 获取 module 参数
    SZrTypeValue *moduleValue = ZrCore_Stack_GetValue(argBase);
    if (moduleValue->type != ZR_VALUE_TYPE_OBJECT) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }
    
    SZrObject *moduleObject = ZR_CAST_OBJECT(state, moduleValue->value.object);
    if (moduleObject == ZR_NULL || moduleObject->internalType != ZR_OBJECT_INTERNAL_TYPE_MODULE) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }
    
    struct SZrObjectModule *module = (struct SZrObjectModule *)moduleObject;
    
    // 获取 typeName 参数
    SZrTypeValue *typeNameValue = ZrCore_Stack_GetValue(argBase + 1);
    if (typeNameValue->type != ZR_VALUE_TYPE_STRING) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }
    
    SZrString *typeName = ZR_CAST_STRING(state, typeNameValue->value.object);
    if (typeName == ZR_NULL) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }
    
    // 获取 prototypeType 参数
    SZrTypeValue *typeValue = ZrCore_Stack_GetValue(argBase + 2);
    if (!ZR_VALUE_IS_TYPE_INT(typeValue->type)) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }
    
    EZrObjectPrototypeType prototypeType = (EZrObjectPrototypeType)typeValue->value.nativeObject.nativeUInt64;
    if (prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT && prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }
    
    // 获取 accessModifier 参数
    SZrTypeValue *accessModifierValue = ZrCore_Stack_GetValue(argBase + 3);
    if (!ZR_VALUE_IS_TYPE_INT(accessModifierValue->type)) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }
    
    EZrAccessModifier accessModifier = (EZrAccessModifier)accessModifierValue->value.nativeObject.nativeUInt64;
    
    // 创建 prototype 对象
    SZrObjectPrototype *prototype = ZR_NULL;
    if (prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        prototype = (SZrObjectPrototype *)ZrCore_StructPrototype_New(state, typeName);
    } else {
        prototype = ZrCore_ObjectPrototype_New(state, typeName, prototypeType);
    }
    
    if (prototype == ZR_NULL) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }
    
    // 注册到模块导出
    SZrTypeValue prototypeValue;
    ZrCore_Value_InitAsRawObject(state, &prototypeValue, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    prototypeValue.type = ZR_VALUE_TYPE_OBJECT;
    
    if (accessModifier == ZR_ACCESS_CONSTANT_PUBLIC) {
        ZrCore_Module_AddPubExport(state, module, typeName, &prototypeValue);
    } else if (accessModifier == ZR_ACCESS_CONSTANT_PROTECTED) {
        ZrCore_Module_AddProExport(state, module, typeName, &prototypeValue);
    } else {
        // PRIVATE 不导出
        // 但仍然创建 prototype 对象
    }
    
    // 返回 prototype 对象
    SZrTypeValue *result = ZrCore_Stack_GetValue(functionBase);
    ZrCore_Value_Copy(state, result, &prototypeValue);

    ZR_RETURN_CREATE_PROTOTYPE_RESULT();
#undef ZR_RETURN_CREATE_PROTOTYPE_RESULT
}

// 辅助函数：从序列化的二进制数据解析prototype信息
// 数据格式：SZrCompiledPrototypeInfo(24字节) + [inheritsCount * TZrUInt32] + [membersCount * SZrCompiledMemberInfo(44字节)]
// 需要从entryFunction的常量池中读取字符串索引对应的字符串
static TZrBool parse_compiled_prototype_info(struct SZrState *state,
                                           struct SZrFunction *entryFunction,
                                           const TZrByte *serializedData,
                                           TZrSize dataSize,
                                           SZrPrototypeCreationInfo *protoInfo) {
    if (state == ZR_NULL || entryFunction == ZR_NULL || serializedData == ZR_NULL || 
        dataSize < sizeof(SZrCompiledPrototypeInfo) || protoInfo == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 读取prototype信息结构
    const SZrCompiledPrototypeInfo *protoInfoHeader = (const SZrCompiledPrototypeInfo *)serializedData;
    TZrUInt32 nameStringIndex = protoInfoHeader->nameStringIndex;
    TZrUInt32 type = protoInfoHeader->type;
    TZrUInt32 accessModifier = protoInfoHeader->accessModifier;
    TZrUInt32 inheritsCount = protoInfoHeader->inheritsCount;
    TZrUInt32 membersCount = protoInfoHeader->membersCount;
    
    // 验证数据大小是否足够
    TZrSize expectedSize = sizeof(SZrCompiledPrototypeInfo) + 
                           inheritsCount * sizeof(TZrUInt32) + 
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
    protoInfo->needsPostCreateSetup = ZR_FALSE;
    
    // 初始化继承类型名称数组
    ZrCore_Array_Init(state, &protoInfo->inheritTypeNames, sizeof(SZrString *), inheritsCount);
    
    // 读取继承类型名称索引数组
    const TZrUInt32 *inheritIndices = (const TZrUInt32 *)(serializedData + sizeof(SZrCompiledPrototypeInfo));
    for (TZrUInt32 i = 0; i < inheritsCount; i++) {
        TZrUInt32 inheritStringIndex = inheritIndices[i];
        if (inheritStringIndex > 0 && inheritStringIndex < entryFunction->constantValueLength) {
            const SZrTypeValue *inheritConstant = &entryFunction->constantValueList[inheritStringIndex];
            if (inheritConstant->type == ZR_VALUE_TYPE_STRING) {
                SZrString *inheritTypeName = ZR_CAST_STRING(state, inheritConstant->value.object);
                if (inheritTypeName != ZR_NULL) {
                    ZrCore_Array_Push(state, &protoInfo->inheritTypeNames, &inheritTypeName);
                }
            }
        }
    }
    
    // 读取成员信息（只存储指针，稍后处理）
    // 成员数据紧跟在继承数组后面
    // 成员信息存储在序列化数据中，但完整处理需要函数引用解析、字段偏移计算等
    // 这里先读取成员信息指针，完整处理在ZrModuleCreatePrototypesFromData中进行
    // const TZrByte *membersData = serializedData + sizeof(SZrCompiledPrototypeInfo) + inheritsCount * sizeof(TZrUInt32);
    // const SZrCompiledMemberInfo *members = (const SZrCompiledMemberInfo *)membersData;
    
    protoInfo->membersCount = membersCount;
    protoInfo->members =
            (const SZrCompiledMemberInfo *)(serializedData + sizeof(SZrCompiledPrototypeInfo) +
                                            inheritsCount * sizeof(TZrUInt32));
    
    return ZR_TRUE;
}

// 从编译后的函数的prototypeData中解析prototype信息并创建prototype实例
// 实现两遍创建机制：第一遍创建所有prototype，第二遍设置继承关系和成员信息
TZrSize ZrCore_Module_CreatePrototypesFromData(struct SZrState *state, 
                                         struct SZrObjectModule *module,
                                         struct SZrFunction *entryFunction) {
    if (state == ZR_NULL || entryFunction == ZR_NULL) {
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
    
    TZrUInt32 prototypeCount = entryFunction->prototypeCount;
    if (!ensure_prototype_instance_storage(state, entryFunction)) {
        return 0;
    }
    
    // 第一遍：创建所有prototype实例（不设置继承关系）
    SZrArray prototypeInfos;
    ZrCore_Array_Init(state, &prototypeInfos, sizeof(SZrPrototypeCreationInfo), prototypeCount);
    
    TZrSize createdCount = 0;
    
    // 从prototypeData读取数据（跳过头部的prototypeCount）
    const TZrByte *prototypeData = entryFunction->prototypeData + sizeof(TZrUInt32);
    TZrSize remainingDataSize = entryFunction->prototypeDataLength - sizeof(TZrUInt32);
    const TZrByte *currentPos = prototypeData;
    
    // 遍历每个prototype
    for (TZrUInt32 i = 0; i < prototypeCount; i++) {
        if (remainingDataSize < sizeof(SZrCompiledPrototypeInfo)) {
            break;  // 数据不足，退出
        }
        
        // 解析SZrCompiledPrototypeInfo
        const SZrCompiledPrototypeInfo *protoInfo = (const SZrCompiledPrototypeInfo *)currentPos;
        TZrUInt32 inheritsCount = protoInfo->inheritsCount;
        TZrUInt32 membersCount = protoInfo->membersCount;
        
        // 计算当前prototype数据的大小
        TZrSize inheritArraySize = inheritsCount * sizeof(TZrUInt32);
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
        ZrCore_Array_Init(state, &protoInfoData.inheritTypeNames, sizeof(SZrString *), 4);
        protoInfoData.members = ZR_NULL;
        protoInfoData.membersCount = 0;
        
        if (parse_compiled_prototype_info(state, entryFunction, currentPos, currentPrototypeSize, &protoInfoData)) {
            // 创建prototype对象（第一遍：不设置继承关系）
            if (protoInfoData.typeName != ZR_NULL && protoInfoData.prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_INVALID) {
                TZrBool prototypeWasCreated = ZR_FALSE;
                SZrObjectPrototype *prototype = entryFunction->prototypeInstances[i];
                if (prototype == ZR_NULL) {
                    if (protoInfoData.prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
                        prototype = (SZrObjectPrototype *)ZrCore_StructPrototype_New(state, protoInfoData.typeName);
                    } else if (protoInfoData.prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
                        prototype = ZrCore_ObjectPrototype_New(state, protoInfoData.typeName, protoInfoData.prototypeType);
                        // 初始化元表
                        ZrCore_ObjectPrototype_InitMetaTable(state, prototype);
                    }
                    if (prototype != ZR_NULL) {
                        entryFunction->prototypeInstances[i] = prototype;
                        prototypeWasCreated = ZR_TRUE;
                    }
                }
                
                if (prototype != ZR_NULL) {
                    protoInfoData.prototype = prototype;
                    protoInfoData.needsPostCreateSetup = prototypeWasCreated;
                    
                    // 注册到模块导出（如果有模块上下文），第二遍可能还需要查找
                    SZrTypeValue prototypeValue;
                    ZrCore_Value_InitAsRawObject(state, &prototypeValue, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
                    prototypeValue.type = ZR_VALUE_TYPE_OBJECT;
                    
                    if (module != ZR_NULL) {
                        if (protoInfoData.accessModifier == ZR_ACCESS_CONSTANT_PUBLIC) {
                            ZrCore_Module_AddPubExport(state, module, protoInfoData.typeName, &prototypeValue);
                        } else if (protoInfoData.accessModifier == ZR_ACCESS_CONSTANT_PROTECTED) {
                            ZrCore_Module_AddProExport(state, module, protoInfoData.typeName, &prototypeValue);
                        }
                    }
                    // PRIVATE 不导出，但仍然创建prototype对象
                    register_prototype_in_global_scope(state, protoInfoData.typeName, &prototypeValue);
                    
                    // 保存prototype信息到数组，用于第二遍处理继承关系和成员信息
                    ZrCore_Array_Push(state, &prototypeInfos, &protoInfoData);
                    if (prototypeWasCreated) {
                        createdCount++;
                    }
                } else {
                    // 创建失败，清理资源
                    ZrCore_Array_Free(state, &protoInfoData.inheritTypeNames);
                }
            } else {
                // 解析失败，清理资源
                ZrCore_Array_Free(state, &protoInfoData.inheritTypeNames);
            }
        }
        
        // 移动到下一个prototype数据
        currentPos += currentPrototypeSize;
        remainingDataSize -= currentPrototypeSize;
    }
    
    // 第二遍：设置继承关系和成员信息
    for (TZrSize i = 0; i < prototypeInfos.length; i++) {
        SZrPrototypeCreationInfo *protoInfo = (SZrPrototypeCreationInfo *)ZrCore_Array_Get(&prototypeInfos, i);
        if (protoInfo == ZR_NULL || protoInfo->prototype == ZR_NULL) {
            continue;
        }

        if (!protoInfo->needsPostCreateSetup) {
            ZrCore_Array_Free(state, &protoInfo->inheritTypeNames);
            continue;
        }
        
        // 处理继承关系（只支持单个基类，TODO: 支持多重继承）
        if (protoInfo->inheritTypeNames.length > 0) {
            SZrString **inheritTypeNamePtr = (SZrString **)ZrCore_Array_Get(&protoInfo->inheritTypeNames, 0);
            if (inheritTypeNamePtr != ZR_NULL && *inheritTypeNamePtr != ZR_NULL) {
                SZrObjectPrototype *superPrototype =
                        find_local_created_prototype_by_name(&prototypeInfos, *inheritTypeNamePtr);
                if (superPrototype == ZR_NULL) {
                    superPrototype = find_prototype_by_name(state, module, *inheritTypeNamePtr);
                }
                if (superPrototype != ZR_NULL) {
                    ZrCore_ObjectPrototype_SetSuper(state, protoInfo->prototype, superPrototype);
                }
            }
        }
        
        // 处理成员信息（字段、方法、构造函数）
        if (protoInfo->members != ZR_NULL && entryFunction->constantValueList != ZR_NULL) {
            for (TZrUInt32 memberIndex = 0; memberIndex < protoInfo->membersCount; memberIndex++) {
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
                        ZrCore_StructPrototype_AddField(state,
                                                  (SZrStructPrototype *)protoInfo->prototype,
                                                  memberName,
                                                  member->fieldOffset);
                    }

                    if (member->isUsingManaged) {
                        ZrCore_ObjectPrototype_AddManagedField(state,
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
                        ZrCore_ObjectPrototype_AddMeta(state, protoInfo->prototype, (EZrMetaType)member->metaType, function);
                        if (member->metaType == ZR_META_CONSTRUCTOR) {
                            SZrString *constructorName = ZrCore_String_CreateFromNative(state, "__constructor");
                            if (constructorName != ZR_NULL) {
                                SZrTypeValue constructorKey;
                                SZrTypeValue constructorValue;
                                ZrCore_Value_InitAsRawObject(state, &constructorKey,
                                                       ZR_CAST_RAW_OBJECT_AS_SUPER(constructorName));
                                constructorKey.type = ZR_VALUE_TYPE_STRING;
                                ZrCore_Value_InitAsRawObject(state, &constructorValue,
                                                       ZR_CAST_RAW_OBJECT_AS_SUPER(function));
                                constructorValue.type = functionConstant->type;
                                ZrCore_Object_SetValue(state, &protoInfo->prototype->super, &constructorKey,
                                                 &constructorValue);
                            }
                        }
                        continue;
                    }

                    SZrTypeValue methodValue;
                    ZrCore_Value_InitAsRawObject(state, &methodValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
                    methodValue.type = functionConstant->type;

                    SZrTypeValue methodKey;
                    ZrCore_Value_InitAsRawObject(state, &methodKey, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
                    methodKey.type = ZR_VALUE_TYPE_STRING;
                    ZrCore_Object_SetValue(state, &protoInfo->prototype->super, &methodKey, &methodValue);
                }
            }
        }
        
        // 清理继承类型名称数组
        ZrCore_Array_Free(state, &protoInfo->inheritTypeNames);
    }
    
    // 清理prototype信息数组
    ZrCore_Array_Free(state, &prototypeInfos);
    return createdCount;
}

// 向后兼容：保留旧函数名，内部调用新函数
TZrSize ZrCore_Module_CreatePrototypesFromConstants(struct SZrState *state, 
                                              struct SZrObjectModule *module,
                                              struct SZrFunction *entryFunction) {
    // 优先使用新的prototypeData机制
    if (entryFunction->prototypeData != ZR_NULL && entryFunction->prototypeDataLength > 0 && 
        entryFunction->prototypeCount > 0) {
        return ZrCore_Module_CreatePrototypesFromData(state, module, entryFunction);
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
