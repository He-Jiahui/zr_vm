//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_common/zr_string_conf.h"

// 常量引用路径步骤类型（与parser模块中的定义保持一致）
// 注意：这些值作为TUInt32存储，但表示负数（通过类型转换）
#define ZR_CONSTANT_REF_STEP_PARENT ((TUInt32)(TInt32)-1)        // -1: 向上引用parent function
#define ZR_CONSTANT_REF_STEP_CONSTANT_POOL ((TUInt32)(TInt32)-2) // -2: 当前函数的常量池索引
#define ZR_CONSTANT_REF_STEP_MODULE ((TUInt32)(TInt32)-3)        // -3: 模块引用
#define ZR_CONSTANT_REF_STEP_PROTOTYPE_INDEX ((TUInt32)(TInt32)-4) // -4: 下一个数值读取prototype的index
#define ZR_CONSTANT_REF_STEP_CHILD_FUNC_INDEX ((TUInt32)(TInt32)-5) // -5: 下一个数值读取childFunctionList的index

// 创建常量引用路径（分配内存）
SZrConstantReferencePath *ZrConstantReferencePathCreate(
    struct SZrState *state,
    TUInt32 depth) {
    if (state == ZR_NULL || depth == 0) {
        return ZR_NULL;
    }
    
    SZrGlobalState *global = state->global;
    SZrConstantReferencePath *path = (SZrConstantReferencePath *)ZrMemoryRawMalloc(
        global, sizeof(SZrConstantReferencePath));
    if (path == ZR_NULL) {
        return ZR_NULL;
    }
    
    path->depth = depth;
    path->steps = (TUInt32 *)ZrMemoryRawMalloc(global, depth * sizeof(TUInt32));
    if (path->steps == ZR_NULL) {
        ZrMemoryRawFree(global, path, sizeof(SZrConstantReferencePath));
        return ZR_NULL;
    }
    
    path->type = ZR_VALUE_TYPE_UNKNOWN;
    
    return path;
}

// 释放常量引用路径（释放内存）
void ZrConstantReferencePathFree(
    struct SZrState *state,
    SZrConstantReferencePath *path) {
    if (state == ZR_NULL || path == ZR_NULL) {
        return;
    }
    
    SZrGlobalState *global = state->global;
    if (path->steps != ZR_NULL) {
        ZrMemoryRawFree(global, path->steps, path->depth * sizeof(TUInt32));
    }
    ZrMemoryRawFree(global, path, sizeof(SZrConstantReferencePath));
}

// 辅助函数：查找函数的entry function（最顶层的函数，通常是模块入口函数）
// entry function通常包含prototypeData
static struct SZrFunction *find_entry_function_from_call_stack(struct SZrState *state, struct SZrFunction *currentFunction) {
    if (state == ZR_NULL || currentFunction == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 如果当前函数已经有prototypeData，可能就是entry function
    if (currentFunction->prototypeData != ZR_NULL && currentFunction->prototypeCount > 0) {
        return currentFunction;
    }
    
    // 否则，向上遍历调用栈，查找包含prototypeData的函数
    // 查找调用栈中最顶层的函数
    SZrCallInfo *callInfo = state->callInfoList;
    struct SZrFunction *entryFunction = ZR_NULL;
    
    while (callInfo != ZR_NULL) {
        if (callInfo->functionBase.valuePointer >= state->stackBase.valuePointer &&
            callInfo->functionBase.valuePointer < state->stackTop.valuePointer) {
            SZrTypeValue *closureValue = ZrStackGetValue(callInfo->functionBase.valuePointer);
            if (closureValue != ZR_NULL && closureValue->type == ZR_VALUE_TYPE_CLOSURE) {
                SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, closureValue->value.object);
                if (closure != ZR_NULL && closure->function != ZR_NULL) {
                    struct SZrFunction *func = closure->function;
                    // 检查是否是entry function（有prototypeData）
                    if (func->prototypeData != ZR_NULL && func->prototypeCount > 0) {
                        entryFunction = func;
                        break;  // 找到第一个包含prototype的，通常就是entry function
                    }
                    // 如果没有找到，继续向上查找
                    if (entryFunction == ZR_NULL) {
                        entryFunction = func;  // 记录当前函数，作为备选
                    }
                }
            }
        }
        callInfo = callInfo->previous;
    }
    
    return entryFunction;
}

// 辅助函数：从全局模块注册表中查找模块
// 通过遍历已加载的模块，查找包含指定entry function的模块
static struct SZrObjectModule *find_module_by_entry_function(struct SZrState *state, struct SZrFunction *entryFunction) {
    if (state == ZR_NULL || entryFunction == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrGlobalState *global = state->global;
    
    // 检查 loadedModulesRegistry 是否已初始化
    if (!ZrValueIsGarbageCollectable(&global->loadedModulesRegistry) ||
        global->loadedModulesRegistry.type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }
    
    SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
    if (registry == ZR_NULL || !registry->nodeMap.isValid || registry->nodeMap.buckets == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 遍历模块注册表中的所有模块
    // 检查每个模块是否与entry function相关
    // 通过检查模块的prototype导出是否与entry function的prototypeInstances匹配
    for (TZrSize i = 0; i < registry->nodeMap.capacity; i++) {
        SZrHashKeyValuePair *pair = registry->nodeMap.buckets[i];
        while (pair != ZR_NULL) {
            // 检查值是否是模块对象
            if (pair->key.type == ZR_VALUE_TYPE_STRING && 
                pair->value.type == ZR_VALUE_TYPE_OBJECT) {
                SZrObject *cachedObject = ZR_CAST_OBJECT(state, pair->value.value.object);
                if (cachedObject != ZR_NULL && 
                    cachedObject->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
                    struct SZrObjectModule *module = (struct SZrObjectModule *)cachedObject;
                    
                    // 检查entry function是否有prototypeInstances
                    // 如果有，检查模块的导出中是否有对应的prototype
                    if (entryFunction->prototypeInstances != ZR_NULL && 
                        entryFunction->prototypeInstancesLength > 0) {
                        // 检查模块的导出中是否有entry function的prototype实例
                        // 遍历entry function的prototypeInstances，检查是否在模块导出中
                        TBool foundMatch = ZR_FALSE;
                        for (TUInt32 j = 0; j < entryFunction->prototypeInstancesLength; j++) {
                            struct SZrObjectPrototype *proto = entryFunction->prototypeInstances[j];
                            if (proto != ZR_NULL && proto->name != ZR_NULL) {
                                // 检查模块的导出中是否有该prototype
                                const SZrTypeValue *exported = ZrModuleGetPubExport(state, module, proto->name);
                                if (exported != ZR_NULL && exported->type == ZR_VALUE_TYPE_OBJECT) {
                                    SZrObjectPrototype *exportedProto = (SZrObjectPrototype *)ZR_CAST_OBJECT(state, exported->value.object);
                                    if (exportedProto == proto) {
                                        foundMatch = ZR_TRUE;
                                        break;
                                    }
                                }
                            }
                        }
                        
                        if (foundMatch) {
                            return module;
                        }
                    } else if (entryFunction->prototypeData != ZR_NULL && 
                               entryFunction->prototypeCount > 0) {
                        // 如果prototypeInstances还未创建，但prototypeData存在，
                        // 则通过检查模块是否已创建prototype来判断
                        // 这是一个启发式方法：如果模块的prototype数量与entry function的prototypeCount匹配，
                        // 则很可能是匹配的模块
                        // 注意：这种方法不是100%准确，但在大多数情况下有效
                        // 更准确的方法需要在模块对象中存储entry function引用（需要修改模块结构）
                        return module;
                    }
                }
            }
            pair = pair->next;
        }
    }
    
    return ZR_NULL;
}

// 辅助函数：从调用栈获取parent function
// 通过查找调用栈中上一个callInfo的closure来获取parent function
static SZrFunction *get_parent_function_from_call_stack(struct SZrState *state, struct SZrFunction *currentFunction) {
    if (state == ZR_NULL || currentFunction == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 查找调用栈，找到当前函数的callInfo
    SZrCallInfo *currentCallInfo = state->callInfoList;
    while (currentCallInfo != ZR_NULL) {
        // 检查当前callInfo是否属于currentFunction
        // 注意：这里需要比较function，但callInfo中存储的是closure
        // 我们需要从closure中获取function
        if (currentCallInfo->functionBase.valuePointer >= state->stackBase.valuePointer &&
            currentCallInfo->functionBase.valuePointer < state->stackTop.valuePointer) {
            SZrTypeValue *closureValue = ZrStackGetValue(currentCallInfo->functionBase.valuePointer);
            if (closureValue != ZR_NULL && closureValue->type == ZR_VALUE_TYPE_CLOSURE) {
                SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, closureValue->value.object);
                if (closure != ZR_NULL && closure->function == currentFunction) {
                    // 找到了当前函数的callInfo，获取parent
                    if (currentCallInfo->previous != ZR_NULL && 
                        currentCallInfo->previous->functionBase.valuePointer >= state->stackBase.valuePointer &&
                        currentCallInfo->previous->functionBase.valuePointer < state->stackTop.valuePointer) {
                        SZrTypeValue *parentClosureValue = ZrStackGetValue(currentCallInfo->previous->functionBase.valuePointer);
                        if (parentClosureValue != ZR_NULL && parentClosureValue->type == ZR_VALUE_TYPE_CLOSURE) {
                            SZrClosure *parentClosure = ZR_CAST_VM_CLOSURE(state, parentClosureValue->value.object);
                            if (parentClosure != ZR_NULL) {
                                return parentClosure->function;
                            }
                        }
                    }
                    break;
                }
            }
        }
        currentCallInfo = currentCallInfo->previous;
    }
    
    return ZR_NULL;
}

// 解析常量引用路径，返回目标对象
TBool ZrConstantResolveReference(
    struct SZrState *state,
    struct SZrFunction *startFunction,
    const SZrConstantReferencePath *path,
    struct SZrObjectModule *module,
    SZrTypeValue *result) {
    if (state == ZR_NULL || startFunction == ZR_NULL || path == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 如果没有提供module，尝试从调用栈查找
    struct SZrObjectModule *currentModule = module;
    if (currentModule == ZR_NULL) {
        struct SZrFunction *entryFunction = find_entry_function_from_call_stack(state, startFunction);
        if (entryFunction != ZR_NULL) {
            currentModule = find_module_by_entry_function(state, entryFunction);
        }
    }
    
    if (path->depth == 0 || path->steps == ZR_NULL) {
        return ZR_FALSE;
    }
    
    SZrFunction *currentFunction = startFunction;
    TUInt32 stepIndex = 0;
    
    // 按照路径步骤逐步解析
    while (stepIndex < path->depth) {
        TUInt32 step = path->steps[stepIndex];
        
        switch ((TInt32)step) {
            case ZR_CONSTANT_REF_STEP_PARENT:  // -1: 向上引用parent function
                // 从调用栈获取parent function
                {
                    SZrFunction *parentFunction = get_parent_function_from_call_stack(state, currentFunction);
                    if (parentFunction == ZR_NULL) {
                        return ZR_FALSE;
                    }
                    currentFunction = parentFunction;
                    stepIndex++;
                    continue;
                }
                
            case ZR_CONSTANT_REF_STEP_CONSTANT_POOL:  // -2: 当前函数的常量池索引
                // 下一个步骤应该是常量池索引
                stepIndex++;
                if (stepIndex >= path->depth) {
                    return ZR_FALSE;
                }
                {
                    TUInt32 constantIndex = path->steps[stepIndex];
                    if (constantIndex >= currentFunction->constantValueLength) {
                        return ZR_FALSE;
                    }
                    SZrTypeValue *constant = &currentFunction->constantValueList[constantIndex];
                    ZrValueCopy(state, result, constant);
                    // 如果还有后续步骤，需要继续解析
                    // TODO: 这里暂时假设常量池引用是最终结果
                    stepIndex++;
                    continue;
                }
                
            case ZR_CONSTANT_REF_STEP_MODULE:  // -3: 模块引用
                // 模块引用格式：-3, moduleNameStringIndex, exportNameStringIndex
                // 或者：-3, moduleNameStringIndex, constantIndex
                stepIndex++;
                if (stepIndex + 1 >= path->depth) {
                    return ZR_FALSE;
                }
                {
                    TUInt32 moduleNameIndex = path->steps[stepIndex];
                    TUInt32 exportNameIndex = path->steps[stepIndex + 1];
                    stepIndex += 2;
                    
                    // 从常量池读取模块名和导出名
                    // 注意：需要从entryFunction读取，因为模块名可能在entryFunction的常量池中
                    struct SZrFunction *entryFunction = find_entry_function_from_call_stack(state, currentFunction);
                    if (entryFunction == ZR_NULL || 
                        moduleNameIndex >= entryFunction->constantValueLength ||
                        exportNameIndex >= entryFunction->constantValueLength) {
                        return ZR_FALSE;
                    }
                    
                    const SZrTypeValue *moduleNameConstant = &entryFunction->constantValueList[moduleNameIndex];
                    const SZrTypeValue *exportNameConstant = &entryFunction->constantValueList[exportNameIndex];
                    
                    if (moduleNameConstant->type != ZR_VALUE_TYPE_STRING || 
                        exportNameConstant->type != ZR_VALUE_TYPE_STRING) {
                        return ZR_FALSE;
                    }
                    
                    SZrString *moduleName = ZR_CAST_STRING(state, moduleNameConstant->value.object);
                    SZrString *exportName = ZR_CAST_STRING(state, exportNameConstant->value.object);
                    
                    if (moduleName == ZR_NULL || exportName == ZR_NULL) {
                        return ZR_FALSE;
                    }
                    
                    // 从全局模块注册表中查找模块
                    // 首先尝试通过路径直接查找（如果模块名就是路径）
                    struct SZrObjectModule *targetModule = ZrModuleGetFromCache(state, moduleName);
                    
                    // 如果直接查找失败，尝试遍历模块注册表查找匹配的模块名
                    if (targetModule == ZR_NULL && state->global != ZR_NULL) {
                        SZrGlobalState *global = state->global;
                        if (ZrValueIsGarbageCollectable(&global->loadedModulesRegistry) &&
                            global->loadedModulesRegistry.type == ZR_VALUE_TYPE_OBJECT) {
                            SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
                            if (registry != ZR_NULL && registry->nodeMap.isValid && 
                                registry->nodeMap.buckets != ZR_NULL) {
                                // 遍历模块注册表，查找模块名匹配的模块
                                for (TZrSize i = 0; i < registry->nodeMap.capacity; i++) {
                                    SZrHashKeyValuePair *pair = registry->nodeMap.buckets[i];
                                    while (pair != ZR_NULL) {
                                        // 检查键是否是字符串（模块路径）
                                        if (pair->key.type == ZR_VALUE_TYPE_STRING) {
                                            SZrString *path = ZR_CAST_STRING(state, pair->key.value.object);
                                            // 检查路径是否与模块名匹配（可以是完整路径或模块名）
                                            if (path != ZR_NULL) {
                                                // 比较字符串：检查路径是否包含模块名，或模块名是否匹配路径
                                                // TODO: 这里简化处理：如果路径的哈希与模块名的哈希匹配，或路径等于模块名
                                                // 更精确的匹配需要字符串比较
                                                if (path == moduleName || 
                                                    ZrStringCompare(state, path, moduleName)) {
                                                    // 检查值是否是模块对象
                                                    if (pair->value.type == ZR_VALUE_TYPE_OBJECT) {
                                                        SZrObject *cachedObject = ZR_CAST_OBJECT(state, pair->value.value.object);
                                                        if (cachedObject != ZR_NULL && 
                                                            cachedObject->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
                                                            targetModule = (struct SZrObjectModule *)cachedObject;
                                                            // 同时检查模块的moduleName是否匹配
                                                            if (targetModule->moduleName != ZR_NULL &&
                                                                (targetModule->moduleName == moduleName ||
                                                                 ZrStringCompare(state, targetModule->moduleName, moduleName))) {
                                                                break;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        pair = pair->next;
                                    }
                                    if (targetModule != ZR_NULL) {
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    
                    if (targetModule == ZR_NULL) {
                        return ZR_FALSE;
                    }
                    
                    // 从模块导出中获取值
                    const SZrTypeValue *exportedValue = ZrModuleGetPubExport(state, targetModule, exportName);
                    if (exportedValue == ZR_NULL) {
                        return ZR_FALSE;
                    }
                    
                    ZrValueCopy(state, result, exportedValue);
                    continue;
                }
                
            case ZR_CONSTANT_REF_STEP_PROTOTYPE_INDEX:  // -4: 下一个数值读取prototype的index
                stepIndex++;
                if (stepIndex >= path->depth) {
                    return ZR_FALSE;
                }
                {
                    TUInt32 prototypeIndex = path->steps[stepIndex];
                    
                    // 延迟加载prototype实例
                    // 需要找到entryFunction（包含prototypeData的函数）
                    struct SZrFunction *entryFunction = find_entry_function_from_call_stack(state, currentFunction);
                    if (entryFunction == ZR_NULL || entryFunction->prototypeData == ZR_NULL || 
                        entryFunction->prototypeCount == 0) {
                        return ZR_FALSE;
                    }
                    
                    // 检查prototypeIndex是否有效
                    if (prototypeIndex >= entryFunction->prototypeCount) {
                        return ZR_FALSE;
                    }
                    
                    // 检查是否已经实例化
                    if (entryFunction->prototypeInstances != ZR_NULL && 
                        prototypeIndex < entryFunction->prototypeInstancesLength &&
                        entryFunction->prototypeInstances[prototypeIndex] != ZR_NULL) {
                        // 已经实例化，直接返回
                        ZrValueInitAsRawObject(state, result, 
                            ZR_CAST_RAW_OBJECT_AS_SUPER(entryFunction->prototypeInstances[prototypeIndex]));
                        result->type = ZR_VALUE_TYPE_OBJECT;
                        stepIndex++;
                        continue;
                    }
                    
                    // 确保prototype实例数组已分配
                    if (entryFunction->prototypeInstances == ZR_NULL) {
                        SZrGlobalState *global = state->global;
                        entryFunction->prototypeInstances = (struct SZrObjectPrototype **)ZrMemoryRawMalloc(
                            global, entryFunction->prototypeCount * sizeof(struct SZrObjectPrototype *));
                        if (entryFunction->prototypeInstances == ZR_NULL) {
                            return ZR_FALSE;
                        }
                        ZrMemoryRawSet(entryFunction->prototypeInstances, 0, 
                            entryFunction->prototypeCount * sizeof(struct SZrObjectPrototype *));
                        entryFunction->prototypeInstancesLength = entryFunction->prototypeCount;
                    }
                    
                    // 实例化prototype（使用module.c中的解析逻辑）
                    // 注意：这里需要模块上下文来注册prototype到模块导出
                    // 如果提供了module，使用它；否则尝试查找
                    struct SZrObjectModule *targetModule = currentModule;
                    if (targetModule == ZR_NULL) {
                        targetModule = find_module_by_entry_function(state, entryFunction);
                    }
                    
                    // 实例化prototype：通过调用ZrModuleCreatePrototypesFromData来创建所有prototype
                    // 这会创建所有prototype，但只有第一次调用时会实际创建，后续调用会检查已存在的实例
                    if (targetModule != ZR_NULL) {
                        ZrModuleCreatePrototypesFromData(state, targetModule, entryFunction);
                    } else {
                        // 没有module，无法正确注册prototype到模块导出
                        // TODO: 暂时返回失败
                        return ZR_FALSE;
                    }
                    
                    // 检查是否已创建
                    if (prototypeIndex < entryFunction->prototypeInstancesLength &&
                        entryFunction->prototypeInstances[prototypeIndex] != ZR_NULL) {
                        ZrValueInitAsRawObject(state, result, 
                            ZR_CAST_RAW_OBJECT_AS_SUPER(entryFunction->prototypeInstances[prototypeIndex]));
                        result->type = ZR_VALUE_TYPE_OBJECT;
                        stepIndex++;
                        continue;
                    }
                    
                    return ZR_FALSE;  // 无法实例化prototype（可能是prototypeIndex无效或创建失败）
                }
                
            case ZR_CONSTANT_REF_STEP_CHILD_FUNC_INDEX:  // -5: 下一个数值读取childFunctionList的index
                stepIndex++;
                if (stepIndex >= path->depth) {
                    return ZR_FALSE;
                }
                {
                    TUInt32 childIndex = path->steps[stepIndex];
                    if (childIndex >= currentFunction->childFunctionLength) {
                        return ZR_FALSE;
                    }
                    currentFunction = &currentFunction->childFunctionList[childIndex];
                    stepIndex++;
                    continue;
                }
                
            default:
                // 正数: 作为childFunctionList或prototypes的索引
                // TODO: 根据上下文判断（暂时先假设是childFunctionList索引）
                if (step >= currentFunction->childFunctionLength) {
                    return ZR_FALSE;
                }
                currentFunction = &currentFunction->childFunctionList[step];
                stepIndex++;
                continue;
        }
    }
    
    // 如果路径解析完成，返回当前函数作为结果
    if (currentFunction != ZR_NULL) {
        ZrValueInitAsRawObject(state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(currentFunction));
        result->type = ZR_VALUE_TYPE_FUNCTION;
        return ZR_TRUE;
    }
    
    return ZR_FALSE;
}

// 从常量池中的引用常量解析路径
SZrConstantReferencePath *ZrConstantReferencePathFromConstant(
    struct SZrState *state,
    const SZrTypeValue *constant) {
    if (state == ZR_NULL || constant == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 引用路径被序列化为字符串常量
    // 格式：pathDepth (TUInt32) + pathSteps (TUInt32数组)
    if (constant->type != ZR_VALUE_TYPE_STRING) {
        return ZR_NULL;
    }
    
    SZrString *serializedString = ZR_CAST_STRING(state, constant->value.object);
    if (serializedString == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 获取序列化数据的长度
    TZrSize stringLength = (serializedString->shortStringLength < ZR_VM_LONG_STRING_FLAG) ?
                           (TZrSize)serializedString->shortStringLength :
                           serializedString->longStringLength;
    
    // 检查最小长度（至少需要pathDepth）
    if (stringLength < sizeof(TUInt32)) {
        return ZR_NULL;
    }
    
    // 获取序列化数据
    TNativeString nativeStr = ZrStringGetNativeString(serializedString);
    if (nativeStr == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 读取路径深度
    TUInt32 pathDepth = *(TUInt32 *)nativeStr;
    
    // 检查数据长度是否足够
    TZrSize expectedSize = sizeof(TUInt32) + pathDepth * sizeof(TUInt32);
    if (stringLength < expectedSize) {
        return ZR_NULL;
    }
    
    // 创建路径对象
    SZrConstantReferencePath *path = ZrConstantReferencePathCreate(state, pathDepth);
    if (path == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 读取路径步骤
    TUInt32 *pathSteps = (TUInt32 *)(nativeStr + sizeof(TUInt32));
    ZrMemoryRawCopy((TByte *)path->steps, (TByte *)pathSteps, pathDepth * sizeof(TUInt32));
    
    // 设置类型（从常量中获取）
    path->type = constant->type;
    
    return path;
}