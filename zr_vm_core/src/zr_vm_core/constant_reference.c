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
// entry function通常包含prototypeConstantIndices
static struct SZrFunction *find_entry_function_from_call_stack(struct SZrState *state, struct SZrFunction *currentFunction) {
    if (state == ZR_NULL || currentFunction == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 如果当前函数已经有prototypeConstantIndices，可能就是entry function
    if (currentFunction->prototypeConstantIndices != ZR_NULL) {
        return currentFunction;
    }
    
    // 否则，向上遍历调用栈，查找包含prototypeConstantIndices的函数
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
                    // 检查是否是entry function（有prototypeConstantIndices）
                    if (func->prototypeConstantIndices != ZR_NULL) {
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
    if (state == ZR_NULL || entryFunction == ZR_NULL) {
        return ZR_NULL;
    }
    
    // TODO: 实现从全局模块注册表中查找模块
    // 当前模块系统可能还没有建立entry function到模块的直接映射
    // 暂时返回ZR_NULL，后续可以通过模块注册机制实现
    // 或者，可以通过遍历loadedModulesRegistry来查找
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
                    // 这里暂时假设常量池引用是最终结果
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
                    // TODO: 实现完整的模块查找逻辑（需要全局模块注册表API支持）
                    // 暂时使用ZrModuleGetFromCache（如果模块名就是路径）
                    struct SZrObjectModule *targetModule = ZrModuleGetFromCache(state, moduleName);
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
                    if (currentFunction->prototypeConstantIndices == ZR_NULL || 
                        prototypeIndex >= currentFunction->prototypeConstantIndicesLength) {
                        return ZR_FALSE;
                    }
                    
                    // 检查是否已经实例化
                    if (currentFunction->prototypeInstances != ZR_NULL && 
                        prototypeIndex < currentFunction->prototypeInstancesLength &&
                        currentFunction->prototypeInstances[prototypeIndex] != ZR_NULL) {
                        // 已经实例化，直接返回
                        ZrValueInitAsRawObject(state, result, 
                            ZR_CAST_RAW_OBJECT_AS_SUPER(currentFunction->prototypeInstances[prototypeIndex]));
                        result->type = ZR_VALUE_TYPE_OBJECT;
                        stepIndex++;
                        continue;
                    }
                    
                    // 需要实例化prototype
                    // 从prototypeConstantIndices获取常量索引
                    TUInt32 constantIndex = currentFunction->prototypeConstantIndices[prototypeIndex];
                    if (constantIndex >= currentFunction->constantValueLength) {
                        return ZR_FALSE;
                    }
                    
                    const SZrTypeValue *constant = &currentFunction->constantValueList[constantIndex];
                    if (constant->type != ZR_VALUE_TYPE_STRING) {
                        return ZR_FALSE;
                    }
                    
                    // 解析prototype二进制数据并实例化
                    // 需要找到entryFunction（包含prototypeConstantIndices的函数）
                    struct SZrFunction *entryFunction = find_entry_function_from_call_stack(state, currentFunction);
                    if (entryFunction == ZR_NULL || entryFunction != currentFunction) {
                        // 如果当前函数不是entryFunction，需要从entryFunction的prototypeConstantIndices查找
                        // 但prototypeIndex是相对于当前函数的，这里需要重新映射
                        // 暂时只支持从当前函数的prototypeConstantIndices读取
                        if (currentFunction->prototypeConstantIndices == ZR_NULL) {
                            return ZR_FALSE;
                        }
                        entryFunction = currentFunction;
                    }
                    
                    // 确保prototype实例数组已分配
                    if (entryFunction->prototypeInstances == ZR_NULL) {
                        SZrGlobalState *global = state->global;
                        entryFunction->prototypeInstances = (struct SZrObjectPrototype **)ZrMemoryRawMalloc(
                            global, entryFunction->prototypeConstantIndicesLength * sizeof(struct SZrObjectPrototype *));
                        if (entryFunction->prototypeInstances == ZR_NULL) {
                            return ZR_FALSE;
                        }
                        ZrMemoryRawSet(entryFunction->prototypeInstances, 0, 
                            entryFunction->prototypeConstantIndicesLength * sizeof(struct SZrObjectPrototype *));
                        entryFunction->prototypeInstancesLength = entryFunction->prototypeConstantIndicesLength;
                    }
                    
                    // 实例化prototype（使用module.c中的解析逻辑）
                    // 注意：这里需要模块上下文来注册prototype到模块导出
                    // 如果提供了module，使用它；否则尝试查找
                    struct SZrObjectModule *targetModule = currentModule;
                    if (targetModule == ZR_NULL) {
                        targetModule = find_module_by_entry_function(state, entryFunction);
                    }
                    
                    // 解析prototype二进制数据
                    SZrString *serializedString = ZR_CAST_STRING(state, constant->value.object);
                    if (serializedString == ZR_NULL) {
                        return ZR_FALSE;
                    }
                    
                    TNativeString strData = ZR_NULL;
                    TZrSize strLength = 0;
                    if (serializedString->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
                        strData = ZrStringGetNativeStringShort(serializedString);
                        strLength = (TZrSize)serializedString->shortStringLength;
                    } else {
                        TNativeString *longStrPtr = ZrStringGetNativeStringLong(serializedString);
                        if (longStrPtr != ZR_NULL) {
                            strData = *longStrPtr;
                            strLength = serializedString->longStringLength;
                        }
                    }
                    
                    if (strData == ZR_NULL || strLength == 0) {
                        return ZR_FALSE;
                    }
                    
                    // 实例化prototype：通过调用ZrModuleCreatePrototypesFromConstants来创建所有prototype
                    // 这会创建所有prototype，但只有第一次调用时会实际创建，后续调用会检查已存在的实例
                    // 如果提供了module，使用它；否则尝试通过entryFunction查找
                    if (targetModule == ZR_NULL) {
                        // 尝试通过其他方式查找module（例如从全局注册表）
                        // 暂时如果找不到module，仍然尝试创建（module可以为ZR_NULL，但不会注册导出）
                        // 注意：如果没有module，prototype无法注册到模块导出，但仍然可以创建
                    }
                    
                    // 触发prototype实例化（延迟加载）
                    // 注意：即使没有module，也尝试创建prototype实例（但不注册到模块导出）
                    // 如果entryFunction已经有prototypeInstances，说明已经初始化过
                    if (targetModule != ZR_NULL || entryFunction->prototypeInstances == ZR_NULL) {
                        // 有module或者是第一次初始化，调用完整创建流程
                        if (targetModule != ZR_NULL) {
                            ZrModuleCreatePrototypesFromConstants(state, targetModule, entryFunction);
                        } else {
                            // 没有module，但仍然需要创建prototype实例数组
                            // 这种情况比较少见，通常prototype应该在模块加载时创建
                            // 这里只确保数组已分配，但不实际创建prototype对象
                            // （因为没有模块上下文，无法正确注册）
                            // 暂时返回失败
                            return ZR_FALSE;
                        }
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
                // 根据上下文判断（暂时先假设是childFunctionList索引）
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
    
    // TODO: 实现从常量中解析引用路径
    // 这需要根据常量在常量池中的存储格式来解析
    // 当前常量系统还未完全支持引用路径的序列化，此功能待后续实现
    
    return ZR_NULL;
}