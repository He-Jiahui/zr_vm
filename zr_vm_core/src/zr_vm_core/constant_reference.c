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

/* 路径和步骤数组共用 state->global 分配器；第二次分配失败时回收路径本体。 */
SZrConstantReferencePath *ZrCore_ConstantReferencePath_Create(
    struct SZrState *state,
    TZrUInt32 depth) {
    if (state == ZR_NULL || depth == 0) {
        return ZR_NULL;
    }
    
    SZrGlobalState *global = state->global;
    SZrConstantReferencePath *path = (SZrConstantReferencePath *)ZrCore_Memory_RawMalloc(
        global, sizeof(SZrConstantReferencePath));
    if (path == ZR_NULL) {
        return ZR_NULL;
    }
    
    path->depth = depth;
    // TODO: depth 来自调用方或二进制路径；32 位 TZrSize 下字节数可能回绕。
    // 当前仅内部解码调用且无外部入口；接入前须用 32 位目标验证上限。
    path->steps = (TZrUInt32 *)ZrCore_Memory_RawMalloc(global, depth * sizeof(TZrUInt32));
    if (path->steps == ZR_NULL) {
        ZrCore_Memory_RawFree(global, path, sizeof(SZrConstantReferencePath));
        return ZR_NULL;
    }
    
    path->type = ZR_VALUE_TYPE_UNKNOWN;
    
    return path;
}

/* 调用方须传入创建时的全局分配器所属 state，先释放步骤再释放路径。 */
void ZrCore_ConstantReferencePath_Free(
    struct SZrState *state,
    SZrConstantReferencePath *path) {
    if (state == ZR_NULL || path == ZR_NULL) {
        return;
    }
    
    SZrGlobalState *global = state->global;
    if (path->steps != ZR_NULL) {
        ZrCore_Memory_RawFree(global, path->steps, path->depth * sizeof(TZrUInt32));
    }
    ZrCore_Memory_RawFree(global, path, sizeof(SZrConstantReferencePath));
}

/* 路径解析优先取当前函数的 prototypeData，再从栈帧寻找，找不到时退回首个可识别函数。 */
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
            SZrTypeValue *callableValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
            struct SZrFunction *func = ZrCore_Closure_GetMetadataFunctionFromValue(state, callableValue);
            if (func != ZR_NULL) {
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
        callInfo = callInfo->previous;
    }
    
    return entryFunction;
}

/* 优先按 prototype 实例的导出身份找模块；未实例化时暂取首个候选模块。 */
static struct SZrObjectModule *find_module_by_entry_function(struct SZrState *state, struct SZrFunction *entryFunction) {
    if (state == ZR_NULL || entryFunction == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrGlobalState *global = state->global;
    
    // 检查 loadedModulesRegistry 是否已初始化
    if (!ZrCore_Value_IsGarbageCollectable(&global->loadedModulesRegistry) ||
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
                        TZrBool foundMatch = ZR_FALSE;
                        for (TZrUInt32 j = 0; j < entryFunction->prototypeInstancesLength; j++) {
                            struct SZrObjectPrototype *proto = entryFunction->prototypeInstances[j];
                            if (proto != ZR_NULL && proto->name != ZR_NULL) {
                                // 检查模块的导出中是否有该prototype
                                const SZrTypeValue *exported = ZrCore_Module_GetPubExport(state, module, proto->name);
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
                        // TODO: 实例尚未生成时直接返回首个模块，未核对身份或数量。
                        // 此路径 API 仓内无外部调用；未来接入时须由模块入口身份或测试确认匹配规则。
                        return module;
                    }
                }
            }
            pair = pair->next;
        }
    }
    
    return ZR_NULL;
}

/* 用栈帧中的 callable 元数据定位当前函数，再从上一帧取得父函数。 */
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
            SZrTypeValue *callableValue = ZrCore_Stack_GetValue(currentCallInfo->functionBase.valuePointer);
            struct SZrFunction *resolvedCurrentFunction =
                    ZrCore_Closure_GetMetadataFunctionFromValue(state, callableValue);
            if (resolvedCurrentFunction == currentFunction) {
                // 找到了当前函数的callInfo，获取parent
                if (currentCallInfo->previous != ZR_NULL &&
                    currentCallInfo->previous->functionBase.valuePointer >= state->stackBase.valuePointer &&
                    currentCallInfo->previous->functionBase.valuePointer < state->stackTop.valuePointer) {
                    SZrTypeValue *parentCallableValue = ZrCore_Stack_GetValue(currentCallInfo->previous->functionBase.valuePointer);
                    struct SZrFunction *parentFunction =
                            ZrCore_Closure_GetMetadataFunctionFromValue(state, parentCallableValue);
                    if (parentFunction != ZR_NULL) {
                        return parentFunction;
                    }
                }
                break;
            }
        }
        currentCallInfo = currentCallInfo->previous;
    }
    
    return ZR_NULL;
}

/* 依带符号步骤遍历函数、常量池、模块与 prototype；失败直接返回 false。 */
TZrBool ZrCore_Constant_ResolveReference(
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
    TZrUInt32 stepIndex = 0;
    
    // 按照路径步骤逐步解析
    while (stepIndex < path->depth) {
        TZrUInt32 step = path->steps[stepIndex];
        
        switch ((TZrInt32)step) {
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
                    TZrUInt32 constantIndex = path->steps[stepIndex];
                    if (constantIndex >= currentFunction->constantValueLength) {
                        return ZR_FALSE;
                    }
                    SZrTypeValue *constant = &currentFunction->constantValueList[constantIndex];
                    ZrCore_Value_Copy(state, result, constant);
                    // 后续步骤仍会执行，最终结果由末尾统一写回逻辑决定。
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
                    TZrUInt32 moduleNameIndex = path->steps[stepIndex];
                    TZrUInt32 exportNameIndex = path->steps[stepIndex + 1];
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
                    struct SZrObjectModule *targetModule = ZrCore_Module_GetFromCache(state, moduleName);
                    
                    // 如果直接查找失败，尝试遍历模块注册表查找匹配的模块名
                    if (targetModule == ZR_NULL && state->global != ZR_NULL) {
                        SZrGlobalState *global = state->global;
                        if (ZrCore_Value_IsGarbageCollectable(&global->loadedModulesRegistry) &&
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
                                            SZrString *entryPath = ZR_CAST_STRING(state, pair->key.value.object);
                                            // 检查路径是否与模块名匹配（可以是完整路径或模块名）
                                            if (entryPath != ZR_NULL) {
                                                // 注册键与模块名按字符串内容或对象身份比较。
                                                if (entryPath == moduleName ||
                                                    ZrCore_String_Compare(state, entryPath, moduleName)) {
                                                    // 检查值是否是模块对象
                                                    if (pair->value.type == ZR_VALUE_TYPE_OBJECT) {
                                                        SZrObject *cachedObject = ZR_CAST_OBJECT(state, pair->value.value.object);
                                                        if (cachedObject != ZR_NULL && 
                                                            cachedObject->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
                                                            targetModule = (struct SZrObjectModule *)cachedObject;
                                                            // TODO: moduleName 不匹配时仍保留 targetModule，外层会提前退出。
                                                            // 解析 API 尚无外部调用；接入前须验证注册键与模块名不一致的场景。
                                                            if (targetModule->moduleName != ZR_NULL &&
                                                                (targetModule->moduleName == moduleName ||
                                                                 ZrCore_String_Compare(state, targetModule->moduleName, moduleName))) {
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
                    const SZrTypeValue *exportedValue = ZrCore_Module_GetPubExport(state, targetModule, exportName);
                    if (exportedValue == ZR_NULL) {
                        return ZR_FALSE;
                    }
                    
                    ZrCore_Value_Copy(state, result, exportedValue);
                    continue;
                }
                
            case ZR_CONSTANT_REF_STEP_PROTOTYPE_INDEX:  // -4: 下一个数值读取prototype的index
                stepIndex++;
                if (stepIndex >= path->depth) {
                    return ZR_FALSE;
                }
                {
                    TZrUInt32 prototypeIndex = path->steps[stepIndex];
                    
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
                        ZrCore_Value_InitAsRawObject(state, result, 
                            ZR_CAST_RAW_OBJECT_AS_SUPER(entryFunction->prototypeInstances[prototypeIndex]));
                        result->type = ZR_VALUE_TYPE_OBJECT;
                        stepIndex++;
                        continue;
                    }
                    
                    // 实例化prototype（使用module.c中的解析逻辑）
                    // 如果能定位到模块则同步模块导出，否则走 standalone 注册路径
                    struct SZrObjectModule *targetModule = currentModule;
                    if (targetModule == ZR_NULL) {
                        targetModule = find_module_by_entry_function(state, entryFunction);
                    }
                    
                    // 这会创建所有prototype；如果它们已存在，则只会复用并补齐注册
                    ZrCore_Module_CreatePrototypesFromData(state, targetModule, entryFunction);
                    
                    // 检查是否已创建
                    if (prototypeIndex < entryFunction->prototypeInstancesLength &&
                        entryFunction->prototypeInstances[prototypeIndex] != ZR_NULL) {
                        ZrCore_Value_InitAsRawObject(state, result, 
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
                    TZrUInt32 childIndex = path->steps[stepIndex];
                    if (childIndex >= currentFunction->childFunctionLength) {
                        return ZR_FALSE;
                    }
                    currentFunction = &currentFunction->childFunctionList[childIndex];
                    stepIndex++;
                    continue;
                }
                
            default:
                // 非负步骤（含 CHILD=0）：当前按 childFunctionList 索引解释。
                // TODO: 当前生成端只写 CHILD_FUNC_INDEX 标签；若未来允许无标签索引，
                // 须从消费方契约确认其是子函数索引还是 prototype 索引。
                if (step >= currentFunction->childFunctionLength) {
                    return ZR_FALSE;
                }
                currentFunction = &currentFunction->childFunctionList[step];
                stepIndex++;
                continue;
        }
    }
    
    // TODO: 常量池、模块导出和 prototype 分支已写入 result，此处无条件覆盖为函数。
    // 解析 API 仓内无外部调用；未来接入时须用非函数末端路径确定预期结果。
    if (currentFunction != ZR_NULL) {
        ZrCore_Value_InitAsRawObject(state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(currentFunction));
        result->type = ZR_VALUE_TYPE_FUNCTION;
        return ZR_TRUE;
    }
    
    return ZR_FALSE;
}

/* 将借用的字符串字节复制为独立路径；分配失败仍由 Create 回收本体。 */
SZrConstantReferencePath *ZrCore_ConstantReferencePath_FromConstant(
    struct SZrState *state,
    const SZrTypeValue *constant) {
    if (state == ZR_NULL || constant == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 引用路径被序列化为字符串常量
    // 格式：pathDepth (TZrUInt32) + pathSteps (TZrUInt32数组)
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
    if (stringLength < sizeof(TZrUInt32)) {
        return ZR_NULL;
    }
    
    // 获取序列化数据
    TZrNativeString nativeStr = ZrCore_String_GetNativeString(serializedString);
    if (nativeStr == ZR_NULL) {
        return ZR_NULL;
    }
    
    // TODO: 二进制字符串按 uint32 指针直接读取；生成端与解码入口均无调用。
    // 接入前须用目标平台确认对齐、字节序及长度乘法的边界。
    // 读取路径深度
    TZrUInt32 pathDepth = *(TZrUInt32 *)nativeStr;
    
    // 检查数据长度是否足够
    TZrSize expectedSize = sizeof(TZrUInt32) + pathDepth * sizeof(TZrUInt32);
    if (stringLength < expectedSize) {
        return ZR_NULL;
    }
    
    // 创建路径对象
    SZrConstantReferencePath *path = ZrCore_ConstantReferencePath_Create(state, pathDepth);
    if (path == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 读取路径步骤
    TZrUInt32 *pathSteps = (TZrUInt32 *)(nativeStr + sizeof(TZrUInt32));
    ZrCore_Memory_RawCopy((TZrByte *)path->steps, (TZrByte *)pathSteps, pathDepth * sizeof(TZrUInt32));
    
    // 设置类型（从常量中获取）
    path->type = constant->type;
    
    return path;
}
