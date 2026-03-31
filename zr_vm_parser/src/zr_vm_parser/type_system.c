//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/type_system.h"
#include "zr_vm_parser/semantic.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

#include <string.h>

// 类型操作函数实现

static TZrBool inferred_type_is_reference_like(EZrValueType baseType) {
    return baseType == ZR_VALUE_TYPE_OBJECT ||
           baseType == ZR_VALUE_TYPE_STRING ||
           baseType == ZR_VALUE_TYPE_ARRAY ||
           baseType == ZR_VALUE_TYPE_BUFFER ||
           baseType == ZR_VALUE_TYPE_FUNCTION ||
           baseType == ZR_VALUE_TYPE_CLOSURE ||
           baseType == ZR_VALUE_TYPE_THREAD;
}

// 初始化类型（使用基础类型）
void ZrParser_InferredType_Init(SZrState *state, SZrInferredType *type, EZrValueType baseType) {
    if (state == ZR_NULL || type == ZR_NULL) {
        return;
    }
    
    type->baseType = baseType;
    type->isNullable = ZR_FALSE;
    type->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    type->typeName = ZR_NULL;
    ZrCore_Array_Construct(&type->elementTypes);
    
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
void ZrParser_InferredType_InitFull(SZrState *state, SZrInferredType *type, EZrValueType baseType, TZrBool isNullable, SZrString *typeName) {
    if (state == ZR_NULL || type == ZR_NULL) {
        return;
    }
    
    type->baseType = baseType;
    type->isNullable = isNullable;
    type->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    type->typeName = typeName;
    ZrCore_Array_Construct(&type->elementTypes);
    
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
void ZrParser_InferredType_Free(SZrState *state, SZrInferredType *type) {
    if (state == ZR_NULL || type == ZR_NULL) {
        return;
    }
    
    // elementTypes stores inline SZrInferredType values.
    if (type->elementTypes.isValid && type->elementTypes.head != ZR_NULL && 
        type->elementTypes.capacity > 0 && type->elementTypes.elementSize > 0) {
        for (TZrSize i = 0; i < type->elementTypes.length; i++) {
            SZrInferredType *elementType = (SZrInferredType *)ZrCore_Array_Get(&type->elementTypes, i);
            if (elementType != ZR_NULL) {
                ZrParser_InferredType_Free(state, elementType);
            }
        }
        ZrCore_Array_Free(state, &type->elementTypes);
    }
    
    // typeName由GC管理，不需要手动释放
    type->typeName = ZR_NULL;
}

// 复制类型
void ZrParser_InferredType_Copy(SZrState *state, SZrInferredType *dest, const SZrInferredType *src) {
    if (state == ZR_NULL || dest == ZR_NULL || src == ZR_NULL) {
        return;
    }
    
    dest->baseType = src->baseType;
    dest->isNullable = src->isNullable;
    dest->ownershipQualifier = src->ownershipQualifier;
    dest->typeName = src->typeName; // 字符串由GC管理，直接复制引用
    
    // Deep-copy nested inline element types.
    if (src->elementTypes.isValid && src->elementTypes.capacity > 0) {
        ZrCore_Array_Init(state, &dest->elementTypes, sizeof(SZrInferredType), src->elementTypes.capacity);
        if (src->elementTypes.length > 0 && src->elementTypes.head != ZR_NULL) {
            for (TZrSize i = 0; i < src->elementTypes.length; i++) {
                SZrInferredType *srcElement = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&src->elementTypes, i);
                if (srcElement != ZR_NULL) {
                    SZrInferredType copiedElement;
                    ZrParser_InferredType_Copy(state, &copiedElement, srcElement);
                    ZrCore_Array_Push(state, &dest->elementTypes, &copiedElement);
                }
            }
        }
    } else {
        ZrCore_Array_Construct(&dest->elementTypes);
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
TZrBool ZrParser_InferredType_Equal(const SZrInferredType *type1, const SZrInferredType *type2) {
    if (type1 == ZR_NULL || type2 == ZR_NULL) {
        return type1 == type2;
    }
    
    if (type1->baseType != type2->baseType) {
        return ZR_FALSE;
    }
    
    if (type1->isNullable != type2->isNullable) {
        return ZR_FALSE;
    }

    if (type1->ownershipQualifier != type2->ownershipQualifier) {
        return ZR_FALSE;
    }
    
    // 比较类型名
    if (type1->typeName != type2->typeName) {
        if (type1->typeName != ZR_NULL && type2->typeName != ZR_NULL) {
            if (!ZrCore_String_Equal(type1->typeName, type2->typeName)) {
                return ZR_FALSE;
            }
        } else {
            return ZR_FALSE;
        }
    }
    
    // Compare inline element type arrays.
    if (type1->elementTypes.length != type2->elementTypes.length) {
        return ZR_FALSE;
    }
    
    if (type1->elementTypes.length > 0) {
        // 两个数组都有元素，逐个比较
        if (type1->elementTypes.head == ZR_NULL || type2->elementTypes.head == ZR_NULL) {
            // 一个为NULL，另一个不为NULL，不相等
            return ZR_FALSE;
        }
        
        for (TZrSize i = 0; i < type1->elementTypes.length; i++) {
            SZrInferredType *elemType1 = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&type1->elementTypes, i);
            SZrInferredType *elemType2 = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&type2->elementTypes, i);
            
            if (elemType1 == ZR_NULL || elemType2 == ZR_NULL) {
                if (elemType1 != elemType2) {
                    return ZR_FALSE;
                }
                continue;
            }
            
            // 递归比较元素类型
            if (!ZrParser_InferredType_Equal(elemType1, elemType2)) {
                return ZR_FALSE;
            }
        }
    }
    
    return ZR_TRUE;
}

static TZrBool ownership_qualifier_is_compatible(EZrOwnershipQualifier fromQualifier,
                                               EZrOwnershipQualifier toQualifier) {
    if (fromQualifier == toQualifier) {
        return ZR_TRUE;
    }

    if (toQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED) {
        return fromQualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
               fromQualifier == ZR_OWNERSHIP_QUALIFIER_SHARED ||
               fromQualifier == ZR_OWNERSHIP_QUALIFIER_WEAK;
    }

    if (toQualifier == ZR_OWNERSHIP_QUALIFIER_NONE) {
        return fromQualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
               fromQualifier == ZR_OWNERSHIP_QUALIFIER_SHARED;
    }

    if (fromQualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE &&
        toQualifier == ZR_OWNERSHIP_QUALIFIER_SHARED) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static EZrOwnershipQualifier ownership_qualifier_get_common(EZrOwnershipQualifier leftQualifier,
                                                            EZrOwnershipQualifier rightQualifier) {
    if (leftQualifier == rightQualifier) {
        return leftQualifier;
    }

    if (leftQualifier == ZR_OWNERSHIP_QUALIFIER_NONE) {
        return rightQualifier;
    }

    if (rightQualifier == ZR_OWNERSHIP_QUALIFIER_NONE) {
        return leftQualifier;
    }

    if ((leftQualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE &&
         rightQualifier == ZR_OWNERSHIP_QUALIFIER_SHARED) ||
        (leftQualifier == ZR_OWNERSHIP_QUALIFIER_SHARED &&
         rightQualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE)) {
        return ZR_OWNERSHIP_QUALIFIER_SHARED;
    }

    return ZR_OWNERSHIP_QUALIFIER_NONE;
}

// 检查类型是否为整数类型
static TZrBool is_integer_type(EZrValueType type) {
    return ZR_VALUE_IS_TYPE_SIGNED_INT(type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(type);
}

// 检查类型是否为浮点类型
static TZrBool is_float_type(EZrValueType type) {
    return ZR_VALUE_IS_TYPE_FLOAT(type);
}

static TZrBool function_type_info_matches_signature(const SZrFunctionTypeInfo *funcInfo,
                                                  const SZrInferredType *returnType,
                                                  const SZrArray *paramTypes) {
    if (funcInfo == ZR_NULL || returnType == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrParser_InferredType_Equal(&funcInfo->returnType, returnType)) {
        return ZR_FALSE;
    }

    if (paramTypes == ZR_NULL) {
        return funcInfo->paramTypes.length == 0;
    }

    if (funcInfo->paramTypes.length != paramTypes->length) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < paramTypes->length; i++) {
        SZrInferredType *existingParam = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&funcInfo->paramTypes, i);
        SZrInferredType *candidateParam = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)paramTypes, i);
        if (existingParam == ZR_NULL || candidateParam == ZR_NULL) {
            if (existingParam != candidateParam) {
                return ZR_FALSE;
            }
            continue;
        }

        if (!ZrParser_InferredType_Equal(existingParam, candidateParam)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

// 检查类型是否为数字类型（保留用于未来扩展）
// static TZrBool is_number_type(EZrValueType type) {
//     return ZR_VALUE_IS_TYPE_NUMBER(type);
// }

// 类型兼容性检查（是否可以隐式转换）
TZrBool ZrParser_InferredType_IsCompatible(const SZrInferredType *fromType, const SZrInferredType *toType) {
    if (fromType == ZR_NULL || toType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 完全相同类型
    if (ZrParser_InferredType_Equal(fromType, toType)) {
        return ZR_TRUE;
    }

    // null 可以转换为显式可空类型，以及现有运行时允许置空的引用类型。
    if (fromType->baseType == ZR_VALUE_TYPE_NULL) {
        if (toType->isNullable || inferred_type_is_reference_like(toType->baseType)) {
            return ZR_TRUE;
        }
    }

    if (!ownership_qualifier_is_compatible(fromType->ownershipQualifier,
                                           toType->ownershipQualifier)) {
        return ZR_FALSE;
    }

    if (toType->baseType == ZR_VALUE_TYPE_OBJECT &&
        toType->typeName == ZR_NULL &&
        toType->elementTypes.length == 0) {
        return ZR_TRUE;
    }

    if (fromType->baseType == toType->baseType &&
        fromType->typeName != ZR_NULL &&
        toType->typeName != ZR_NULL &&
        ZrCore_String_Equal(fromType->typeName, toType->typeName)) {
        if (fromType->isNullable == toType->isNullable) {
            return ZR_TRUE;
        }

        if (toType->isNullable && !fromType->isNullable) {
            return ZR_TRUE;
        }
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
TZrBool ZrParser_InferredType_GetCommonType(SZrState *state, SZrInferredType *result, const SZrInferredType *type1, const SZrInferredType *type2) {
    if (state == ZR_NULL || result == ZR_NULL || type1 == ZR_NULL || type2 == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 如果类型相同，直接返回
    if (ZrParser_InferredType_Equal(type1, type2)) {
        ZrParser_InferredType_Copy(state, result, type1);
        return ZR_TRUE;
    }
    
    // 如果其中一个是null，返回另一个类型（如果是可空的）
    if (type1->baseType == ZR_VALUE_TYPE_NULL) {
        if (type2->isNullable) {
            ZrParser_InferredType_Copy(state, result, type2);
            result->ownershipQualifier = ownership_qualifier_get_common(type1->ownershipQualifier,
                                                                        type2->ownershipQualifier);
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }
    if (type2->baseType == ZR_VALUE_TYPE_NULL) {
        if (type1->isNullable) {
            ZrParser_InferredType_Copy(state, result, type1);
            result->ownershipQualifier = ownership_qualifier_get_common(type1->ownershipQualifier,
                                                                        type2->ownershipQualifier);
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
        ZrParser_InferredType_Init(state, result, floatType);
        result->isNullable = type1->isNullable || type2->isNullable;
        result->ownershipQualifier = ownership_qualifier_get_common(type1->ownershipQualifier,
                                                                    type2->ownershipQualifier);
        return ZR_TRUE;
    }
    
    // 2. 如果都是整数类型，选择较大的整数类型
    // TODO: 简化处理：使用int64
    if (is_integer_type(type1->baseType) && is_integer_type(type2->baseType)) {
        ZrParser_InferredType_Init(state, result, ZR_VALUE_TYPE_INT64);
        result->isNullable = type1->isNullable || type2->isNullable;
        result->ownershipQualifier = ownership_qualifier_get_common(type1->ownershipQualifier,
                                                                    type2->ownershipQualifier);
        return ZR_TRUE;
    }
    
    // 3. 如果类型不兼容，返回false
    return ZR_FALSE;
}

// 获取类型转换指令（如果需要转换）
EZrInstructionCode ZrParser_InferredType_GetConversionOpcode(const SZrInferredType *fromType, const SZrInferredType *toType) {
    if (fromType == ZR_NULL || toType == ZR_NULL) {
        return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }
    
    // 如果类型相同，不需要转换
    if (ZrParser_InferredType_Equal(fromType, toType)) {
        return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }

    if (fromType->baseType == toType->baseType &&
        fromType->isNullable == toType->isNullable &&
        ownership_qualifier_is_compatible(fromType->ownershipQualifier,
                                          toType->ownershipQualifier)) {
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
SZrTypeEnvironment *ZrParser_TypeEnvironment_New(SZrState *state) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrTypeEnvironment *env = (SZrTypeEnvironment *)ZrCore_Memory_RawMallocWithType(
        state->global, sizeof(SZrTypeEnvironment), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (env == ZR_NULL) {
        return ZR_NULL;
    }
    
    ZrCore_Array_Init(state, &env->variableTypes, sizeof(SZrTypeBinding), 8);
    ZrCore_Array_Init(state, &env->functionReturnTypes, sizeof(SZrFunctionTypeInfo *), 8);
    ZrCore_Array_Init(state, &env->typeNames, sizeof(SZrString *), 8);
    env->parent = ZR_NULL;
    env->semanticContext = ZR_NULL;
    
    return env;
}

// 销毁类型环境
void ZrParser_TypeEnvironment_Free(SZrState *state, SZrTypeEnvironment *env) {
    if (state == ZR_NULL || env == ZR_NULL) {
        return;
    }
    
    // 释放变量类型数组
    if (env->variableTypes.isValid && env->variableTypes.head != ZR_NULL && 
        env->variableTypes.capacity > 0 && env->variableTypes.elementSize > 0) {
        // 释放每个类型绑定中的类型
        for (TZrSize i = 0; i < env->variableTypes.length; i++) {
            SZrTypeBinding *binding = (SZrTypeBinding *)ZrCore_Array_Get(&env->variableTypes, i);
            if (binding != ZR_NULL) {
                ZrParser_InferredType_Free(state, &binding->type);
            }
        }
        ZrCore_Array_Free(state, &env->variableTypes);
    }
    
    // 释放函数类型数组
    if (env->functionReturnTypes.isValid && env->functionReturnTypes.head != ZR_NULL && 
        env->functionReturnTypes.capacity > 0 && env->functionReturnTypes.elementSize > 0) {
        // 释放每个函数类型信息
        for (TZrSize i = 0; i < env->functionReturnTypes.length; i++) {
            SZrFunctionTypeInfo **funcInfo = (SZrFunctionTypeInfo **)ZrCore_Array_Get(&env->functionReturnTypes, i);
            if (funcInfo != ZR_NULL && *funcInfo != ZR_NULL) {
                // 释放函数类型信息
                ZrParser_InferredType_Free(state, &(*funcInfo)->returnType);
                if ((*funcInfo)->paramTypes.isValid) {
                    for (TZrSize j = 0; j < (*funcInfo)->paramTypes.length; j++) {
                        SZrInferredType *paramType =
                                (SZrInferredType *)ZrCore_Array_Get(&(*funcInfo)->paramTypes, j);
                        if (paramType != ZR_NULL) {
                            ZrParser_InferredType_Free(state, paramType);
                        }
                    }
                    ZrCore_Array_Free(state, &(*funcInfo)->paramTypes);
                }
                ZrCore_Memory_RawFreeWithType(state->global, *funcInfo, sizeof(SZrFunctionTypeInfo), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
        }
        ZrCore_Array_Free(state, &env->functionReturnTypes);
    }
    
    // 释放类型名称数组（字符串本身由GC管理，只需要释放数组）
    if (env->typeNames.isValid && env->typeNames.head != ZR_NULL && 
        env->typeNames.capacity > 0 && env->typeNames.elementSize > 0) {
        ZrCore_Array_Free(state, &env->typeNames);
    }
    
    // 释放环境本身
    ZrCore_Memory_RawFreeWithType(state->global, env, sizeof(SZrTypeEnvironment), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
}

// 注册变量类型
TZrBool ZrParser_TypeEnvironment_RegisterVariable(SZrState *state, SZrTypeEnvironment *env, SZrString *name, const SZrInferredType *type) {
    TZrTypeId typeId;
    SZrFileRange location = {0};

    if (state == ZR_NULL || env == ZR_NULL || name == ZR_NULL || type == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 检查是否已存在
    for (TZrSize i = 0; i < env->variableTypes.length; i++) {
        SZrTypeBinding *binding = (SZrTypeBinding *)ZrCore_Array_Get(&env->variableTypes, i);
        if (binding != ZR_NULL && binding->name != ZR_NULL && ZrCore_String_Equal(binding->name, name)) {
            // 已存在，更新类型
            ZrParser_InferredType_Free(state, &binding->type);
            ZrParser_InferredType_Copy(state, &binding->type, type);
            return ZR_TRUE;
        }
    }
    
    // 创建新的绑定
    SZrTypeBinding binding;
    binding.name = name;
    ZrParser_InferredType_Copy(state, &binding.type, type);
    
    ZrCore_Array_Push(state, &env->variableTypes, &binding);

    if (env->semanticContext != ZR_NULL) {
        typeId = ZrParser_Semantic_RegisterInferredType(env->semanticContext,
                                                type,
                                                ZR_SEMANTIC_TYPE_KIND_UNKNOWN,
                                                type->typeName,
                                                ZR_NULL);
        ZrParser_Semantic_RegisterSymbol(env->semanticContext,
                                 name,
                                 ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
                                 typeId,
                                 0,
                                 ZR_NULL,
                                 location);
    }
    return ZR_TRUE;
}

// 查找变量类型
TZrBool ZrParser_TypeEnvironment_LookupVariable(SZrState *state, SZrTypeEnvironment *env, SZrString *name, SZrInferredType *result) {
    if (state == ZR_NULL || env == ZR_NULL || name == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 在当前环境中查找
    for (TZrSize i = 0; i < env->variableTypes.length; i++) {
        SZrTypeBinding *binding = (SZrTypeBinding *)ZrCore_Array_Get(&env->variableTypes, i);
        if (binding != ZR_NULL && binding->name != ZR_NULL && ZrCore_String_Equal(binding->name, name)) {
            ZrParser_InferredType_Copy(state, result, &binding->type);
            return ZR_TRUE;
        }
    }
    
    // 在父环境中查找
    if (env->parent != ZR_NULL) {
        return ZrParser_TypeEnvironment_LookupVariable(state, env->parent, name, result);
    }
    
    return ZR_FALSE;
}

// 注册函数类型
TZrBool ZrParser_TypeEnvironment_RegisterFunction(SZrState *state, SZrTypeEnvironment *env, SZrString *name, const SZrInferredType *returnType, SZrArray *paramTypes) {
    TZrTypeId typeId;
    TZrSymbolId symbolId;
    TZrOverloadSetId overloadSetId;
    SZrFileRange location = {0};

    if (state == ZR_NULL || env == ZR_NULL || name == ZR_NULL || returnType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 允许同名重载，但拒绝完全相同的签名重复注册。
    for (TZrSize i = 0; i < env->functionReturnTypes.length; i++) {
        SZrFunctionTypeInfo **funcInfo = (SZrFunctionTypeInfo **)ZrCore_Array_Get(&env->functionReturnTypes, i);
        if (funcInfo != ZR_NULL && *funcInfo != ZR_NULL && 
            (*funcInfo)->name != ZR_NULL && ZrCore_String_Equal((*funcInfo)->name, name) &&
            function_type_info_matches_signature(*funcInfo, returnType, paramTypes)) {
            return ZR_FALSE;
        }
    }
    
    // 创建新的函数类型信息
    SZrFunctionTypeInfo *funcInfo = (SZrFunctionTypeInfo *)ZrCore_Memory_RawMallocWithType(
        state->global, sizeof(SZrFunctionTypeInfo), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (funcInfo == ZR_NULL) {
        return ZR_FALSE;
    }
    
    funcInfo->name = name;
    ZrParser_InferredType_Copy(state, &funcInfo->returnType, returnType);
    
    // Deep-copy parameter type array values.
    if (paramTypes != ZR_NULL && paramTypes->isValid && paramTypes->capacity > 0 && paramTypes->length > 0) {
        ZrCore_Array_Init(state, &funcInfo->paramTypes, sizeof(SZrInferredType), paramTypes->capacity);
        for (TZrSize i = 0; i < paramTypes->length; i++) {
            SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get(paramTypes, i);
            if (paramType != ZR_NULL) {
                SZrInferredType copiedType;
                ZrParser_InferredType_Copy(state, &copiedType, paramType);
                ZrCore_Array_Push(state, &funcInfo->paramTypes, &copiedType);
            }
        }
    } else {
        ZrCore_Array_Construct(&funcInfo->paramTypes);
    }
    
    ZrCore_Array_Push(state, &env->functionReturnTypes, &funcInfo);

    if (env->semanticContext != ZR_NULL) {
        typeId = ZrParser_Semantic_RegisterInferredType(env->semanticContext,
                                                returnType,
                                                ZR_SEMANTIC_TYPE_KIND_UNKNOWN,
                                                returnType->typeName,
                                                ZR_NULL);
        overloadSetId = ZrParser_Semantic_GetOrCreateOverloadSet(env->semanticContext, name);
        symbolId = ZrParser_Semantic_RegisterSymbol(env->semanticContext,
                                            name,
                                            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                                            typeId,
                                            overloadSetId,
                                            ZR_NULL,
                                            location);
        ZrParser_Semantic_AddOverloadMember(env->semanticContext, overloadSetId, symbolId);
    }
    return ZR_TRUE;
}

// 查找函数类型
TZrBool ZrParser_TypeEnvironment_LookupFunction(SZrTypeEnvironment *env, SZrString *name, SZrFunctionTypeInfo **result) {
    if (env == ZR_NULL || name == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 在当前环境中查找
    for (TZrSize i = 0; i < env->functionReturnTypes.length; i++) {
        SZrFunctionTypeInfo **funcInfo = (SZrFunctionTypeInfo **)ZrCore_Array_Get(&env->functionReturnTypes, i);
        if (funcInfo != ZR_NULL && *funcInfo != ZR_NULL && 
            (*funcInfo)->name != ZR_NULL && ZrCore_String_Equal((*funcInfo)->name, name)) {
            *result = *funcInfo;
            return ZR_TRUE;
        }
    }
    
    // 在父环境中查找
    if (env->parent != ZR_NULL) {
        return ZrParser_TypeEnvironment_LookupFunction(env->parent, name, result);
    }
    
    return ZR_FALSE;
}

TZrBool ZrParser_TypeEnvironment_LookupFunctions(SZrState *state, SZrTypeEnvironment *env, SZrString *name, SZrArray *results) {
    SZrTypeEnvironment *currentEnv;

    if (state == ZR_NULL || env == ZR_NULL || name == ZR_NULL || results == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, results, sizeof(SZrFunctionTypeInfo *), 4);

    currentEnv = env;
    while (currentEnv != ZR_NULL) {
        for (TZrSize i = 0; i < currentEnv->functionReturnTypes.length; i++) {
            SZrFunctionTypeInfo **funcInfo =
                (SZrFunctionTypeInfo **)ZrCore_Array_Get(&currentEnv->functionReturnTypes, i);
            if (funcInfo != ZR_NULL && *funcInfo != ZR_NULL &&
                (*funcInfo)->name != ZR_NULL && ZrCore_String_Equal((*funcInfo)->name, name)) {
                SZrFunctionTypeInfo *resolvedInfo = *funcInfo;
                ZrCore_Array_Push(state, results, &resolvedInfo);
            }
        }
        currentEnv = currentEnv->parent;
    }

    if (results->length == 0) {
        ZrCore_Array_Free(state, results);
        ZrCore_Array_Construct(results);
    }

    return results->length > 0;
}

// 注册类型名称
TZrBool ZrParser_TypeEnvironment_RegisterType(SZrState *state, SZrTypeEnvironment *env, SZrString *typeName) {
    TZrTypeId typeId;
    SZrFileRange location = {0};

    if (state == ZR_NULL || env == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 检查是否已存在
    for (TZrSize i = 0; i < env->typeNames.length; i++) {
        SZrString **storedTypeName = (SZrString **)ZrCore_Array_Get(&env->typeNames, i);
        if (storedTypeName != ZR_NULL && *storedTypeName != ZR_NULL && ZrCore_String_Equal(*storedTypeName, typeName)) {
            // 已存在，不需要重复注册
            return ZR_TRUE;
        }
    }
    
    // 添加类型名称（字符串本身由GC管理，只存储引用）
    ZrCore_Array_Push(state, &env->typeNames, &typeName);

    if (env->semanticContext != ZR_NULL) {
        typeId = ZrParser_Semantic_RegisterNamedType(env->semanticContext,
                                             typeName,
                                             ZR_SEMANTIC_TYPE_KIND_UNKNOWN,
                                             ZR_NULL);
        ZrParser_Semantic_RegisterSymbol(env->semanticContext,
                                 typeName,
                                 ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                                 typeId,
                                 0,
                                 ZR_NULL,
                                 location);
    }
    return ZR_TRUE;
}

// 查找类型名称
TZrBool ZrParser_TypeEnvironment_LookupType(SZrTypeEnvironment *env, SZrString *typeName) {
    if (env == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 在当前环境中查找
    for (TZrSize i = 0; i < env->typeNames.length; i++) {
        SZrString **storedTypeName = (SZrString **)ZrCore_Array_Get(&env->typeNames, i);
        if (storedTypeName != ZR_NULL && *storedTypeName != ZR_NULL && ZrCore_String_Equal(*storedTypeName, typeName)) {
            return ZR_TRUE;
        }
    }
    
    // 在父环境中查找
    if (env->parent != ZR_NULL) {
        return ZrParser_TypeEnvironment_LookupType(env->parent, typeName);
    }
    
    return ZR_FALSE;
}
