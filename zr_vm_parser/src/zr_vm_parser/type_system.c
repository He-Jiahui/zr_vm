//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/type_system.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

#include <string.h>

// 类型操作函数实现

// 初始化类型（使用基础类型）
void ZrInferredTypeInit(SZrState *state, SZrInferredType *type, EZrValueType baseType) {
    if (state == ZR_NULL || type == ZR_NULL) {
        return;
    }
    
    type->baseType = baseType;
    type->isNullable = ZR_FALSE;
    type->typeName = ZR_NULL;
    ZrArrayConstruct(&type->elementTypes);
    
    // 初始化范围约束
    type->minValue = 0;
    type->maxValue = 0;
    type->hasRangeConstraint = ZR_FALSE;
    
    // 初始化数组大小约束
    type->arrayFixedSize = 0;
    type->arrayMinSize = 0;
    type->arrayMaxSize = 0;
    type->hasArraySizeConstraint = ZR_FALSE;
}

// 初始化类型（完整版本）
void ZrInferredTypeInitFull(SZrState *state, SZrInferredType *type, EZrValueType baseType, TBool isNullable, SZrString *typeName) {
    if (state == ZR_NULL || type == ZR_NULL) {
        return;
    }
    
    type->baseType = baseType;
    type->isNullable = isNullable;
    type->typeName = typeName;
    ZrArrayConstruct(&type->elementTypes);
    
    // 初始化范围约束
    type->minValue = 0;
    type->maxValue = 0;
    type->hasRangeConstraint = ZR_FALSE;
    
    // 初始化数组大小约束
    type->arrayFixedSize = 0;
    type->arrayMinSize = 0;
    type->arrayMaxSize = 0;
    type->hasArraySizeConstraint = ZR_FALSE;
}

// 释放类型（递归释放嵌套的类型，避免循环引用导致的内存泄漏）
void ZrInferredTypeFree(SZrState *state, SZrInferredType *type) {
    if (state == ZR_NULL || type == ZR_NULL) {
        return;
    }
    
    // 释放元素类型数组（递归释放嵌套类型，避免循环引用）
    if (type->elementTypes.isValid && type->elementTypes.head != ZR_NULL && 
        type->elementTypes.capacity > 0 && type->elementTypes.elementSize > 0) {
        // 递归释放每个元素类型（elementTypes存储的是SZrInferredType*指针）
        for (TZrSize i = 0; i < type->elementTypes.length; i++) {
            SZrInferredType **elementTypePtr = (SZrInferredType**)ZrArrayGet(&type->elementTypes, i);
            if (elementTypePtr != ZR_NULL && *elementTypePtr != ZR_NULL) {
                // 递归释放嵌套的类型（包括其elementTypes）
                ZrInferredTypeFree(state, *elementTypePtr);
                // 释放类型对象本身
                ZrMemoryRawFreeWithType(state->global, *elementTypePtr, sizeof(SZrInferredType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
        }
        ZrArrayFree(state, &type->elementTypes);
    }
    
    // typeName由GC管理，不需要手动释放
    type->typeName = ZR_NULL;
}

// 复制类型
void ZrInferredTypeCopy(SZrState *state, SZrInferredType *dest, const SZrInferredType *src) {
    if (state == ZR_NULL || dest == ZR_NULL || src == ZR_NULL) {
        return;
    }
    
    dest->baseType = src->baseType;
    dest->isNullable = src->isNullable;
    dest->typeName = src->typeName; // 字符串由GC管理，直接复制引用
    
    // TODO: 复制元素类型数组（简化处理，仅复制数组结构）
    if (src->elementTypes.isValid && src->elementTypes.capacity > 0) {
        ZrArrayInit(state, &dest->elementTypes, sizeof(SZrInferredType *), src->elementTypes.capacity);
        if (src->elementTypes.length > 0 && src->elementTypes.head != ZR_NULL) {
            memcpy(dest->elementTypes.head, src->elementTypes.head, 
                   src->elementTypes.length * src->elementTypes.elementSize);
            dest->elementTypes.length = src->elementTypes.length;
        }
    } else {
        ZrArrayConstruct(&dest->elementTypes);
    }
    
    // 复制范围约束
    dest->minValue = src->minValue;
    dest->maxValue = src->maxValue;
    dest->hasRangeConstraint = src->hasRangeConstraint;
    
    // 复制数组大小约束
    dest->arrayFixedSize = src->arrayFixedSize;
    dest->arrayMinSize = src->arrayMinSize;
    dest->arrayMaxSize = src->arrayMaxSize;
    dest->hasArraySizeConstraint = src->hasArraySizeConstraint;
}

// 类型相等比较
TBool ZrInferredTypeEqual(const SZrInferredType *type1, const SZrInferredType *type2) {
    if (type1 == ZR_NULL || type2 == ZR_NULL) {
        return type1 == type2;
    }
    
    if (type1->baseType != type2->baseType) {
        return ZR_FALSE;
    }
    
    if (type1->isNullable != type2->isNullable) {
        return ZR_FALSE;
    }
    
    // 比较类型名
    if (type1->typeName != type2->typeName) {
        if (type1->typeName != ZR_NULL && type2->typeName != ZR_NULL) {
            if (!ZrStringEqual(type1->typeName, type2->typeName)) {
                return ZR_FALSE;
            }
        } else {
            return ZR_FALSE;
        }
    }
    
    // 比较元素类型数组
    // elementTypes存储的是SZrInferredType*指针数组
    if (type1->elementTypes.length != type2->elementTypes.length) {
        return ZR_FALSE;
    }
    
    if (type1->elementTypes.length > 0) {
        // 两个数组都有元素，逐个比较
        if (type1->elementTypes.head == ZR_NULL || type2->elementTypes.head == ZR_NULL) {
            // 一个为NULL，另一个不为NULL，不相等
            return ZR_FALSE;
        }
        
        // 遍历元素类型数组，比较每个元素类型
        for (TZrSize i = 0; i < type1->elementTypes.length; i++) {
            SZrInferredType *elemType1 = (SZrInferredType *)ZrArrayGet(&type1->elementTypes, i);
            SZrInferredType *elemType2 = (SZrInferredType *)ZrArrayGet(&type2->elementTypes, i);
            
            if (elemType1 == ZR_NULL || elemType2 == ZR_NULL) {
                if (elemType1 != elemType2) {
                    return ZR_FALSE;
                }
                continue;
            }
            
            // 递归比较元素类型
            if (!ZrInferredTypeEqual(elemType1, elemType2)) {
                return ZR_FALSE;
            }
        }
    }
    
    return ZR_TRUE;
}

// 检查类型是否为整数类型
static TBool is_integer_type(EZrValueType type) {
    return ZR_VALUE_IS_TYPE_SIGNED_INT(type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(type);
}

// 检查类型是否为浮点类型
static TBool is_float_type(EZrValueType type) {
    return ZR_VALUE_IS_TYPE_FLOAT(type);
}

// 检查类型是否为数字类型（保留用于未来扩展）
// static TBool is_number_type(EZrValueType type) {
//     return ZR_VALUE_IS_TYPE_NUMBER(type);
// }

// 类型兼容性检查（是否可以隐式转换）
TBool ZrInferredTypeIsCompatible(const SZrInferredType *fromType, const SZrInferredType *toType) {
    if (fromType == ZR_NULL || toType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 完全相同类型
    if (ZrInferredTypeEqual(fromType, toType)) {
        return ZR_TRUE;
    }
    
    // null可以转换为任何可空类型
    if (fromType->baseType == ZR_VALUE_TYPE_NULL && toType->isNullable) {
        return ZR_TRUE;
    }
    
    // 整数类型提升：较小的整数类型可以转换为较大的整数类型
    if (is_integer_type(fromType->baseType) && is_integer_type(toType->baseType)) {
        // TODO: 简化处理：允许所有整数类型之间的转换（实际应该检查大小）
        return ZR_TRUE;
    }
    
    // 整数到浮点：int可以转换为float/double
    if (is_integer_type(fromType->baseType) && is_float_type(toType->baseType)) {
        return ZR_TRUE;
    }
    
    // 浮点类型提升：float可以转换为double
    if (fromType->baseType == ZR_VALUE_TYPE_FLOAT && toType->baseType == ZR_VALUE_TYPE_DOUBLE) {
        return ZR_TRUE;
    }
    
    // 相同的基础类型但可空性不同（如果toType可空，fromType不可空也可以转换）
    if (fromType->baseType == toType->baseType && toType->isNullable && !fromType->isNullable) {
        return ZR_TRUE;
    }
    
    return ZR_FALSE;
}

// 获取公共类型（用于类型提升，如 int + float -> float）
TBool ZrInferredTypeGetCommonType(SZrState *state, SZrInferredType *result, const SZrInferredType *type1, const SZrInferredType *type2) {
    if (state == ZR_NULL || result == ZR_NULL || type1 == ZR_NULL || type2 == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 如果类型相同，直接返回
    if (ZrInferredTypeEqual(type1, type2)) {
        ZrInferredTypeCopy(state, result, type1);
        return ZR_TRUE;
    }
    
    // 如果其中一个是null，返回另一个类型（如果是可空的）
    if (type1->baseType == ZR_VALUE_TYPE_NULL) {
        if (type2->isNullable) {
            ZrInferredTypeCopy(state, result, type2);
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }
    if (type2->baseType == ZR_VALUE_TYPE_NULL) {
        if (type1->isNullable) {
            ZrInferredTypeCopy(state, result, type1);
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }
    
    // 类型提升规则：
    // 1. 如果一个是浮点类型，结果是浮点类型（double优先）
    if (is_float_type(type1->baseType) || is_float_type(type2->baseType)) {
        EZrValueType floatType = ZR_VALUE_TYPE_DOUBLE;
        if (type1->baseType == ZR_VALUE_TYPE_DOUBLE || type2->baseType == ZR_VALUE_TYPE_DOUBLE) {
            floatType = ZR_VALUE_TYPE_DOUBLE;
        } else {
            floatType = ZR_VALUE_TYPE_FLOAT;
        }
        ZrInferredTypeInit(state, result, floatType);
        result->isNullable = type1->isNullable || type2->isNullable;
        return ZR_TRUE;
    }
    
    // 2. 如果都是整数类型，选择较大的整数类型
    // TODO: 简化处理：使用int64
    if (is_integer_type(type1->baseType) && is_integer_type(type2->baseType)) {
        ZrInferredTypeInit(state, result, ZR_VALUE_TYPE_INT64);
        result->isNullable = type1->isNullable || type2->isNullable;
        return ZR_TRUE;
    }
    
    // 3. 如果类型不兼容，返回false
    return ZR_FALSE;
}

// 获取类型转换指令（如果需要转换）
EZrInstructionCode ZrInferredTypeGetConversionOpcode(const SZrInferredType *fromType, const SZrInferredType *toType) {
    if (fromType == ZR_NULL || toType == ZR_NULL) {
        return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }
    
    // 如果类型相同，不需要转换
    if (ZrInferredTypeEqual(fromType, toType)) {
        return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }
    
    // null转换（不需要指令，运行时处理）
    if (fromType->baseType == ZR_VALUE_TYPE_NULL) {
        return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }
    
    // 整数到浮点：使用TO_FLOAT
    if (is_integer_type(fromType->baseType) && is_float_type(toType->baseType)) {
        return ZR_INSTRUCTION_ENUM(TO_FLOAT);
    }
    
    // 浮点到整数：使用TO_INT
    if (is_float_type(fromType->baseType) && is_integer_type(toType->baseType)) {
        return ZR_INSTRUCTION_ENUM(TO_INT);
    }
    
    // 到布尔：使用TO_BOOL
    if (toType->baseType == ZR_VALUE_TYPE_BOOL && fromType->baseType != ZR_VALUE_TYPE_BOOL) {
        return ZR_INSTRUCTION_ENUM(TO_BOOL);
    }
    
    // 到字符串：使用TO_STRING
    if (toType->baseType == ZR_VALUE_TYPE_STRING && fromType->baseType != ZR_VALUE_TYPE_STRING) {
        return ZR_INSTRUCTION_ENUM(TO_STRING);
    }
    
    // TODO: 整数类型之间的转换（简化处理，不需要转换指令，运行时处理）
    if (is_integer_type(fromType->baseType) && is_integer_type(toType->baseType)) {
        return ZR_INSTRUCTION_ENUM(ENUM_MAX); // 不需要转换指令
    }
    
    return ZR_INSTRUCTION_ENUM(ENUM_MAX);
}

// 类型环境管理函数实现

// 创建类型环境
SZrTypeEnvironment *ZrTypeEnvironmentNew(SZrState *state) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrTypeEnvironment *env = (SZrTypeEnvironment *)ZrMemoryRawMallocWithType(
        state->global, sizeof(SZrTypeEnvironment), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (env == ZR_NULL) {
        return ZR_NULL;
    }
    
    ZrArrayInit(state, &env->variableTypes, sizeof(SZrTypeBinding), 8);
    ZrArrayInit(state, &env->functionReturnTypes, sizeof(SZrFunctionTypeInfo *), 8);
    ZrArrayInit(state, &env->typeNames, sizeof(SZrString *), 8);
    env->parent = ZR_NULL;
    
    return env;
}

// 销毁类型环境
void ZrTypeEnvironmentFree(SZrState *state, SZrTypeEnvironment *env) {
    if (state == ZR_NULL || env == ZR_NULL) {
        return;
    }
    
    // 释放变量类型数组
    if (env->variableTypes.isValid && env->variableTypes.head != ZR_NULL && 
        env->variableTypes.capacity > 0 && env->variableTypes.elementSize > 0) {
        // 释放每个类型绑定中的类型
        for (TZrSize i = 0; i < env->variableTypes.length; i++) {
            SZrTypeBinding *binding = (SZrTypeBinding *)ZrArrayGet(&env->variableTypes, i);
            if (binding != ZR_NULL) {
                ZrInferredTypeFree(state, &binding->type);
            }
        }
        ZrArrayFree(state, &env->variableTypes);
    }
    
    // 释放函数类型数组
    if (env->functionReturnTypes.isValid && env->functionReturnTypes.head != ZR_NULL && 
        env->functionReturnTypes.capacity > 0 && env->functionReturnTypes.elementSize > 0) {
        // 释放每个函数类型信息
        for (TZrSize i = 0; i < env->functionReturnTypes.length; i++) {
            SZrFunctionTypeInfo **funcInfo = (SZrFunctionTypeInfo **)ZrArrayGet(&env->functionReturnTypes, i);
            if (funcInfo != ZR_NULL && *funcInfo != ZR_NULL) {
                // 释放函数类型信息
                ZrInferredTypeFree(state, &(*funcInfo)->returnType);
                if ((*funcInfo)->paramTypes.isValid) {
                    ZrArrayFree(state, &(*funcInfo)->paramTypes);
                }
                ZrMemoryRawFreeWithType(state->global, *funcInfo, sizeof(SZrFunctionTypeInfo), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
        }
        ZrArrayFree(state, &env->functionReturnTypes);
    }
    
    // 释放类型名称数组（字符串本身由GC管理，只需要释放数组）
    if (env->typeNames.isValid && env->typeNames.head != ZR_NULL && 
        env->typeNames.capacity > 0 && env->typeNames.elementSize > 0) {
        ZrArrayFree(state, &env->typeNames);
    }
    
    // 释放环境本身
    ZrMemoryRawFreeWithType(state->global, env, sizeof(SZrTypeEnvironment), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
}

// 注册变量类型
TBool ZrTypeEnvironmentRegisterVariable(SZrState *state, SZrTypeEnvironment *env, SZrString *name, const SZrInferredType *type) {
    if (state == ZR_NULL || env == ZR_NULL || name == ZR_NULL || type == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 检查是否已存在
    for (TZrSize i = 0; i < env->variableTypes.length; i++) {
        SZrTypeBinding *binding = (SZrTypeBinding *)ZrArrayGet(&env->variableTypes, i);
        if (binding != ZR_NULL && binding->name != ZR_NULL && ZrStringEqual(binding->name, name)) {
            // 已存在，更新类型
            ZrInferredTypeFree(state, &binding->type);
            ZrInferredTypeCopy(state, &binding->type, type);
            return ZR_TRUE;
        }
    }
    
    // 创建新的绑定
    SZrTypeBinding binding;
    binding.name = name;
    ZrInferredTypeCopy(state, &binding.type, type);
    
    ZrArrayPush(state, &env->variableTypes, &binding);
    return ZR_TRUE;
}

// 查找变量类型
TBool ZrTypeEnvironmentLookupVariable(SZrState *state, SZrTypeEnvironment *env, SZrString *name, SZrInferredType *result) {
    if (state == ZR_NULL || env == ZR_NULL || name == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 在当前环境中查找
    for (TZrSize i = 0; i < env->variableTypes.length; i++) {
        SZrTypeBinding *binding = (SZrTypeBinding *)ZrArrayGet(&env->variableTypes, i);
        if (binding != ZR_NULL && binding->name != ZR_NULL && ZrStringEqual(binding->name, name)) {
            ZrInferredTypeCopy(state, result, &binding->type);
            return ZR_TRUE;
        }
    }
    
    // 在父环境中查找
    if (env->parent != ZR_NULL) {
        return ZrTypeEnvironmentLookupVariable(state, env->parent, name, result);
    }
    
    return ZR_FALSE;
}

// 注册函数类型
TBool ZrTypeEnvironmentRegisterFunction(SZrState *state, SZrTypeEnvironment *env, SZrString *name, const SZrInferredType *returnType, SZrArray *paramTypes) {
    if (state == ZR_NULL || env == ZR_NULL || name == ZR_NULL || returnType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 检查是否已存在
    for (TZrSize i = 0; i < env->functionReturnTypes.length; i++) {
        SZrFunctionTypeInfo **funcInfo = (SZrFunctionTypeInfo **)ZrArrayGet(&env->functionReturnTypes, i);
        if (funcInfo != ZR_NULL && *funcInfo != ZR_NULL && 
            (*funcInfo)->name != ZR_NULL && ZrStringEqual((*funcInfo)->name, name)) {
            // TODO: 已存在，更新类型（简化处理，暂时不支持重载）
            return ZR_FALSE; // 函数已存在，不允许重复注册
        }
    }
    
    // 创建新的函数类型信息
    SZrFunctionTypeInfo *funcInfo = (SZrFunctionTypeInfo *)ZrMemoryRawMallocWithType(
        state->global, sizeof(SZrFunctionTypeInfo), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (funcInfo == ZR_NULL) {
        return ZR_FALSE;
    }
    
    funcInfo->name = name;
    ZrInferredTypeCopy(state, &funcInfo->returnType, returnType);
    
    // 复制参数类型数组
    if (paramTypes != ZR_NULL && paramTypes->isValid && paramTypes->capacity > 0 && paramTypes->length > 0) {
        ZrArrayInit(state, &funcInfo->paramTypes, sizeof(SZrInferredType), paramTypes->capacity);
        memcpy(funcInfo->paramTypes.head, paramTypes->head, paramTypes->length * paramTypes->elementSize);
        funcInfo->paramTypes.length = paramTypes->length;
    } else {
        ZrArrayConstruct(&funcInfo->paramTypes);
    }
    
    ZrArrayPush(state, &env->functionReturnTypes, &funcInfo);
    return ZR_TRUE;
}

// 查找函数类型
TBool ZrTypeEnvironmentLookupFunction(SZrTypeEnvironment *env, SZrString *name, SZrFunctionTypeInfo **result) {
    if (env == ZR_NULL || name == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 在当前环境中查找
    for (TZrSize i = 0; i < env->functionReturnTypes.length; i++) {
        SZrFunctionTypeInfo **funcInfo = (SZrFunctionTypeInfo **)ZrArrayGet(&env->functionReturnTypes, i);
        if (funcInfo != ZR_NULL && *funcInfo != ZR_NULL && 
            (*funcInfo)->name != ZR_NULL && ZrStringEqual((*funcInfo)->name, name)) {
            *result = *funcInfo;
            return ZR_TRUE;
        }
    }
    
    // 在父环境中查找
    if (env->parent != ZR_NULL) {
        return ZrTypeEnvironmentLookupFunction(env->parent, name, result);
    }
    
    return ZR_FALSE;
}

// 注册类型名称
TBool ZrTypeEnvironmentRegisterType(SZrState *state, SZrTypeEnvironment *env, SZrString *typeName) {
    if (state == ZR_NULL || env == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 检查是否已存在
    for (TZrSize i = 0; i < env->typeNames.length; i++) {
        SZrString **storedTypeName = (SZrString **)ZrArrayGet(&env->typeNames, i);
        if (storedTypeName != ZR_NULL && *storedTypeName != ZR_NULL && ZrStringEqual(*storedTypeName, typeName)) {
            // 已存在，不需要重复注册
            return ZR_TRUE;
        }
    }
    
    // 添加类型名称（字符串本身由GC管理，只存储引用）
    ZrArrayPush(state, &env->typeNames, &typeName);
    return ZR_TRUE;
}

// 查找类型名称
TBool ZrTypeEnvironmentLookupType(SZrTypeEnvironment *env, SZrString *typeName) {
    if (env == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 在当前环境中查找
    for (TZrSize i = 0; i < env->typeNames.length; i++) {
        SZrString **storedTypeName = (SZrString **)ZrArrayGet(&env->typeNames, i);
        if (storedTypeName != ZR_NULL && *storedTypeName != ZR_NULL && ZrStringEqual(*storedTypeName, typeName)) {
            return ZR_TRUE;
        }
    }
    
    // 在父环境中查找
    if (env->parent != ZR_NULL) {
        return ZrTypeEnvironmentLookupType(env->parent, typeName);
    }
    
    return ZR_FALSE;
}
