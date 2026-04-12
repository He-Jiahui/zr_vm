//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

TZrUInt32 allocate_local_var(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || cs->hasError || name == ZR_NULL) {
        return ZR_PARSER_SLOT_NONE;
    }

    TZrUInt32 stackSlot = (TZrUInt32)cs->stackSlotCount;
    SZrFunctionLocalVariable localVar;
    localVar.name = name;
    localVar.stackSlot = stackSlot;
    localVar.offsetActivate = (TZrMemoryOffset) cs->instructionCount;
    localVar.offsetDead = 0; // 将在变量作用域结束时设置
    localVar.scopeDepth = 0;
    localVar.escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE;

    if (cs->scopeStack.length > 0) {
        SZrScope *scope = (SZrScope *)ZrCore_Array_Get(&cs->scopeStack, cs->scopeStack.length - 1);
        if (scope != ZR_NULL) {
            localVar.scopeDepth = scope->depth;
        }
    }

    ZrCore_Array_Push(cs->state, &cs->localVars, &localVar);
    // localVarCount 应该与 localVars.length 保持同步
    cs->localVarCount = cs->localVars.length;
    cs->stackSlotCount++;
    if (cs->maxStackSlotCount < cs->stackSlotCount) {
        cs->maxStackSlotCount = cs->stackSlotCount;
    }

    if (cs->scopeStack.length > 0) {
        SZrScope *scope = (SZrScope *)ZrCore_Array_Get(&cs->scopeStack, cs->scopeStack.length - 1);
        if (scope != ZR_NULL) {
            scope->varCount++;
        }
    }

    return stackSlot;
}

TZrSize ZrParser_Compiler_GetLocalStackFloor(const SZrCompilerState *cs) {
    TZrSize floor = 0;

    if (cs == ZR_NULL || !cs->localVars.isValid) {
        return 0;
    }

    for (TZrSize i = 0; i < cs->localVars.length; i++) {
        const SZrFunctionLocalVariable *var =
                (const SZrFunctionLocalVariable *)ZrCore_Array_Get((SZrArray *)&cs->localVars, i);
        if (var != ZR_NULL) {
            TZrSize nextSlot = (TZrSize)var->stackSlot + 1;
            if (nextSlot > floor) {
                floor = nextSlot;
            }
        }
    }

    return floor;
}

TZrUInt32 compiler_get_cached_null_constant_index(SZrCompilerState *cs) {
    SZrTypeValue nullValue;

    if (cs == ZR_NULL || cs->hasError) {
        return ZR_PARSER_INDEX_NONE;
    }

    if (cs->hasCachedNullConstantIndex) {
        return cs->cachedNullConstantIndex;
    }

    ZrCore_Value_ResetAsNull(&nullValue);
    cs->cachedNullConstantIndex = add_constant(cs, &nullValue);
    cs->hasCachedNullConstantIndex = ZR_TRUE;
    return cs->cachedNullConstantIndex;
}

void ZrParser_Compiler_TrimStackToCount(SZrCompilerState *cs, TZrSize targetCount) {
    TZrSize localFloor;

    if (cs == ZR_NULL) {
        return;
    }

    localFloor = ZrParser_Compiler_GetLocalStackFloor(cs);
    if (targetCount < localFloor) {
        targetCount = localFloor;
    }

    if (cs->stackSlotCount > targetCount) {
        TZrUInt32 nullConstantIndex = compiler_get_cached_null_constant_index(cs);
        for (TZrSize slot = targetCount; slot < cs->stackSlotCount; slot++) {
            emit_instruction(cs,
                             create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                                  (TZrUInt16)slot,
                                                  (TZrInt32)nullConstantIndex));
        }
        cs->stackSlotCount = targetCount;
    }
}

void ZrParser_Compiler_TrimStackToSlot(SZrCompilerState *cs, TZrUInt32 slot) {
    ZrParser_Compiler_TrimStackToCount(cs, (TZrSize)slot + 1);
}

void ZrParser_Compiler_TrimStackBy(SZrCompilerState *cs, TZrSize amount) {
    TZrSize targetCount;

    if (cs == ZR_NULL) {
        return;
    }

    targetCount = (amount < cs->stackSlotCount) ? (cs->stackSlotCount - amount) : 0;
    ZrParser_Compiler_TrimStackToCount(cs, targetCount);
}

// 查找局部变量
TZrUInt32 find_local_var(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_PARSER_SLOT_NONE;
    }

    // 从当前作用域开始查找
    // 使用 localVars.length 而不是 localVarCount，确保同步
    TZrSize varCount = cs->localVars.length;
    for (TZrSize i = varCount; i > 0; i--) {
        TZrSize index = i - 1;
        // 确保索引在有效范围内
        if (index < cs->localVars.length) {
            SZrFunctionLocalVariable *var = (SZrFunctionLocalVariable *) ZrCore_Array_Get(&cs->localVars, index);
            if (var != ZR_NULL && var->name != ZR_NULL) {
                // 比较字符串
                if (ZrCore_String_Equal(var->name, name)) {
                    return var->stackSlot;
                }
            }
        }
    }

    return ZR_PARSER_SLOT_NONE;
}

// 查找闭包变量
TZrUInt32 find_closure_var(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_PARSER_INDEX_NONE;
    }

    // 在闭包变量数组中查找
    TZrSize closureVarCount = cs->closureVars.length;
    for (TZrSize i = 0; i < closureVarCount; i++) {
        SZrFunctionClosureVariable *var = (SZrFunctionClosureVariable *) ZrCore_Array_Get(&cs->closureVars, i);
        if (var != ZR_NULL && var->name != ZR_NULL) {
            if (ZrCore_String_Equal(var->name, name)) {
                return (TZrUInt32) i;
            }
        }
    }

    return ZR_PARSER_INDEX_NONE;
}

// 分配闭包变量
TZrUInt32 allocate_closure_var(SZrCompilerState *cs, SZrString *name, TZrBool inStack) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_PARSER_INDEX_NONE;
    }

    // 检查是否已存在
    TZrUInt32 existingIndex = find_closure_var(cs, name);
    if (existingIndex != ZR_PARSER_INDEX_NONE) {
        return existingIndex;
    }

    // 创建新的闭包变量
    SZrFunctionClosureVariable closureVar;
    closureVar.name = name;
    closureVar.inStack = inStack;
    closureVar.index = (TZrUInt32) cs->closureVarCount;
    closureVar.valueType = ZR_VALUE_TYPE_NULL; // 类型将在运行时确定
    closureVar.scopeDepth = 0;
    closureVar.escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE;

    if (cs->scopeStack.length > 0) {
        SZrScope *scope = (SZrScope *)ZrCore_Array_Get(&cs->scopeStack, cs->scopeStack.length - 1);
        if (scope != ZR_NULL) {
            closureVar.scopeDepth = scope->depth;
        }
    }

    ZrCore_Array_Push(cs->state, &cs->closureVars, &closureVar);
    cs->closureVarCount++;
    
    return (TZrUInt32) (cs->closureVarCount - 1);
}

// 查找子函数索引（在当前编译器的 childFunctions 中通过函数名查找）
// 返回子函数在 childFunctions 数组中的索引，如果未找到返回 ZR_PARSER_INDEX_NONE
// 注意：这个函数用于在编译时查找子函数索引
// 通过编译时建立的函数名到索引的映射来查找，不依赖遍历比较函数名
TZrUInt32 find_child_function_index(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_PARSER_INDEX_NONE;
    }
    
    // 遍历函数名映射数组，查找匹配的函数名
    // 这个映射在编译函数声明时建立，仅用于编译时查找
    // 运行时查找完全基于索引，不依赖函数名
    for (TZrSize i = 0; i < cs->childFunctionNameMap.length; i++) {
        SZrChildFunctionNameMap *map = (SZrChildFunctionNameMap *)ZrCore_Array_Get(&cs->childFunctionNameMap, i);
        if (map != ZR_NULL && map->name != ZR_NULL) {
            if (ZrCore_String_Equal(map->name, name)) {
                // 找到匹配的函数名，返回对应的子函数索引
                return map->childFunctionIndex;
            }
        }
    }
    
    return ZR_PARSER_INDEX_NONE;
}

// 生成函数引用路径常量
// 用于在编译函数调用时，如果是子函数调用，生成引用路径常量
// targetFunction: 目标函数（子函数）
// 返回：常量池索引（存储引用路径常量），失败返回 ZR_PARSER_INDEX_NONE
// 注意：生成的路径格式为：[ZR_CONSTANT_REF_STEP_CHILD_FUNC_INDEX, childIndex]
//       如果目标函数在parent中，则：[ZR_CONSTANT_REF_STEP_PARENT, ZR_CONSTANT_REF_STEP_CHILD_FUNC_INDEX, childIndex]
TZrUInt32 generate_function_reference_path_constant(SZrCompilerState *cs, TZrUInt32 childFunctionIndex) {
    if (cs == ZR_NULL || childFunctionIndex == ZR_PARSER_INDEX_NONE) {
        return ZR_PARSER_INDEX_NONE;
    }
    
    // 生成引用路径：直接子函数引用
    // 路径格式：[ZR_CONSTANT_REF_STEP_CHILD_FUNC_INDEX, childIndex]
    TZrUInt32 pathDepth = 2;
    TZrUInt32 *pathSteps = (TZrUInt32 *)ZrCore_Memory_RawMalloc(cs->state->global, pathDepth * sizeof(TZrUInt32));
    if (pathSteps == ZR_NULL) {
        return ZR_PARSER_INDEX_NONE;
    }
    
    pathSteps[0] = ZR_CONSTANT_REF_STEP_TO_UINT32(ZR_CONSTANT_REF_STEP_CHILD_FUNC_INDEX);
    pathSteps[1] = childFunctionIndex;
    
    // 将路径序列化为字符串类型常量（与prototype类似，使用字符串类型存储二进制数据）
    // 格式：pathDepth (TZrUInt32) + pathSteps (TZrUInt32数组)
    TZrSize serializedSize = sizeof(TZrUInt32) + pathDepth * sizeof(TZrUInt32);
    TZrByte *serializedData = (TZrByte *)ZrCore_Memory_RawMalloc(cs->state->global, serializedSize);
    if (serializedData == ZR_NULL) {
        ZrCore_Memory_RawFree(cs->state->global, pathSteps, pathDepth * sizeof(TZrUInt32));
        return ZR_PARSER_INDEX_NONE;
    }
    
    // 写入路径深度和步骤
    *(TZrUInt32 *)serializedData = pathDepth;
    ZrCore_Memory_RawCopy(serializedData + sizeof(TZrUInt32), (TZrByte *)pathSteps, pathDepth * sizeof(TZrUInt32));
    
    // 创建字符串常量存储二进制数据
    SZrString *serializedString = ZrCore_String_Create(cs->state, (TZrNativeString)serializedData, serializedSize);
    if (serializedString == ZR_NULL) {
        ZrCore_Memory_RawFree(cs->state->global, serializedData, serializedSize);
        ZrCore_Memory_RawFree(cs->state->global, pathSteps, pathDepth * sizeof(TZrUInt32));
        return ZR_PARSER_INDEX_NONE;
    }
    
    // 将字符串存储到常量池
    SZrTypeValue serializedValue;
    ZrCore_Value_InitAsRawObject(cs->state, &serializedValue, ZR_CAST_RAW_OBJECT_AS_SUPER(serializedString));
    serializedValue.type = ZR_VALUE_TYPE_STRING;
    
    TZrUInt32 constantIndex = add_constant(cs, &serializedValue);
    
    // 释放临时分配的内存
    ZrCore_Memory_RawFree(cs->state->global, serializedData, serializedSize);
    ZrCore_Memory_RawFree(cs->state->global, pathSteps, pathDepth * sizeof(TZrUInt32));
    
    return constantIndex;
}

// 分配栈槽
TZrUInt32 allocate_stack_slot(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return ZR_PARSER_SLOT_NONE;
    }

    TZrUInt32 slot = (TZrUInt32) cs->stackSlotCount;
    cs->stackSlotCount++;
    if (cs->maxStackSlotCount < cs->stackSlotCount) {
        cs->maxStackSlotCount = cs->stackSlotCount;
    }
    return slot;
}

