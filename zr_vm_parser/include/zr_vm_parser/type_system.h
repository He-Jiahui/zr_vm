//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_PARSER_TYPE_SYSTEM_H
#define ZR_VM_PARSER_TYPE_SYSTEM_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"

// 前向声明
struct SZrCompilerState;

// 推断的类型结构体
typedef struct SZrInferredType {
    EZrValueType baseType;           // 基础类型（对应EZrValueType枚举）
    TBool isNullable;                // 是否可空
    SZrArray elementTypes;           // 泛型/数组元素类型（SZrInferredType*），可选
    SZrString *typeName;             // 用户定义类型名（struct/class等），可选
} SZrInferredType;

// 类型绑定（变量名到类型的映射）
typedef struct SZrTypeBinding {
    SZrString *name;                 // 变量名
    SZrInferredType type;            // 变量类型
} SZrTypeBinding;

// 函数类型信息
typedef struct SZrFunctionTypeInfo {
    SZrString *name;                 // 函数名
    SZrInferredType returnType;      // 返回类型
    SZrArray paramTypes;             // 参数类型数组（SZrInferredType）
} SZrFunctionTypeInfo;

// 类型环境结构体
typedef struct SZrTypeEnvironment {
    SZrArray variableTypes;          // 变量类型绑定数组（SZrTypeBinding）
    SZrArray functionReturnTypes;    // 函数类型信息数组（SZrFunctionTypeInfo*）
    struct SZrTypeEnvironment *parent; // 父类型环境（用于作用域）
} SZrTypeEnvironment;

// 类型操作函数

// 初始化类型（使用基础类型）
ZR_PARSER_API void ZrInferredTypeInit(SZrState *state, SZrInferredType *type, EZrValueType baseType);

// 初始化类型（完整版本）
ZR_PARSER_API void ZrInferredTypeInitFull(SZrState *state, SZrInferredType *type, EZrValueType baseType, TBool isNullable, SZrString *typeName);

// 释放类型
ZR_PARSER_API void ZrInferredTypeFree(SZrState *state, SZrInferredType *type);

// 复制类型
ZR_PARSER_API void ZrInferredTypeCopy(SZrState *state, SZrInferredType *dest, const SZrInferredType *src);

// 类型相等比较
ZR_PARSER_API TBool ZrInferredTypeEqual(const SZrInferredType *type1, const SZrInferredType *type2);

// 类型兼容性检查（是否可以隐式转换）
ZR_PARSER_API TBool ZrInferredTypeIsCompatible(const SZrInferredType *fromType, const SZrInferredType *toType);

// 获取公共类型（用于类型提升，如 int + float -> float）
ZR_PARSER_API TBool ZrInferredTypeGetCommonType(SZrState *state, SZrInferredType *result, const SZrInferredType *type1, const SZrInferredType *type2);

// 获取类型转换指令（如果需要转换）
// 返回转换指令，如果不需要转换则返回ZR_INSTRUCTION_ENUM(ENUM_MAX)
ZR_PARSER_API EZrInstructionCode ZrInferredTypeGetConversionOpcode(const SZrInferredType *fromType, const SZrInferredType *toType);

// 类型环境管理函数

// 创建类型环境
ZR_PARSER_API SZrTypeEnvironment *ZrTypeEnvironmentNew(SZrState *state);

// 销毁类型环境
ZR_PARSER_API void ZrTypeEnvironmentFree(SZrState *state, SZrTypeEnvironment *env);

// 注册变量类型
ZR_PARSER_API TBool ZrTypeEnvironmentRegisterVariable(SZrState *state, SZrTypeEnvironment *env, SZrString *name, const SZrInferredType *type);

// 查找变量类型
ZR_PARSER_API TBool ZrTypeEnvironmentLookupVariable(SZrState *state, SZrTypeEnvironment *env, SZrString *name, SZrInferredType *result);

// 注册函数类型
ZR_PARSER_API TBool ZrTypeEnvironmentRegisterFunction(SZrState *state, SZrTypeEnvironment *env, SZrString *name, const SZrInferredType *returnType, SZrArray *paramTypes);

// 查找函数类型
ZR_PARSER_API TBool ZrTypeEnvironmentLookupFunction(SZrTypeEnvironment *env, SZrString *name, SZrFunctionTypeInfo **result);

#endif //ZR_VM_PARSER_TYPE_SYSTEM_H

