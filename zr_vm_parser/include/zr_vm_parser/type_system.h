//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_PARSER_TYPE_SYSTEM_H
#define ZR_VM_PARSER_TYPE_SYSTEM_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"

// 前向声明
struct SZrCompilerState;
struct SZrSemanticContext;

// 推断的类型结构体
typedef struct SZrInferredType {
    EZrValueType baseType;           // 基础类型（对应EZrValueType枚举）
    TZrBool isNullable;                // 是否可空
    EZrOwnershipQualifier ownershipQualifier; // 特殊所有权限定
    SZrArray elementTypes;           // 泛型/数组元素类型（SZrInferredType*），可选
    SZrString *typeName;             // 用户定义类型名（struct/class等），可选
    
    // 范围约束（用于整数类型和数组长度）
    TZrInt64 minValue;                 // 最小值
    TZrInt64 maxValue;                 // 最大值
    TZrBool hasRangeConstraint;        // 是否有范围约束
    
    // 数组大小约束
    TZrSize arrayFixedSize;          // 数组固定大小（0表示未固定）
    TZrSize arrayMinSize;            // 数组最小大小
    TZrSize arrayMaxSize;            // 数组最大大小
    TZrBool hasArraySizeConstraint;    // 是否有数组大小约束
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
    SZrArray genericParameters;      // 泛型参数数组（SZrTypeGenericParameterInfo）
    SZrArray parameterPassingModes;  // 参数传递模式数组（EZrParameterPassingMode）
    SZrAstNode *declarationNode;     // 源声明节点（可选）
} SZrFunctionTypeInfo;

// 类型环境结构体
typedef struct SZrTypeEnvironment {
    SZrArray variableTypes;          // 变量类型绑定数组（SZrTypeBinding）
    SZrArray functionReturnTypes;    // 函数类型信息数组（SZrFunctionTypeInfo*）
    SZrArray typeNames;              // 类型名称数组（SZrString*），用于存储已定义的struct/class类型名称
    struct SZrTypeEnvironment *parent; // 父类型环境（用于作用域）
    struct SZrSemanticContext *semanticContext; // 共享语义记录上下文（可选）
} SZrTypeEnvironment;

// 类型操作函数

// 初始化类型（使用基础类型）
ZR_PARSER_API void ZrParser_InferredType_Init(SZrState *state, SZrInferredType *type, EZrValueType baseType);

// 初始化类型（完整版本）
ZR_PARSER_API void ZrParser_InferredType_InitFull(SZrState *state, SZrInferredType *type, EZrValueType baseType, TZrBool isNullable, SZrString *typeName);

// 释放类型
ZR_PARSER_API void ZrParser_InferredType_Free(SZrState *state, SZrInferredType *type);

// 复制类型
ZR_PARSER_API void ZrParser_InferredType_Copy(SZrState *state, SZrInferredType *dest, const SZrInferredType *src);

// 类型相等比较
ZR_PARSER_API TZrBool ZrParser_InferredType_Equal(const SZrInferredType *type1, const SZrInferredType *type2);

// 类型兼容性检查（是否可以隐式转换）
ZR_PARSER_API TZrBool ZrParser_InferredType_IsCompatible(const SZrInferredType *fromType, const SZrInferredType *toType);

// 获取公共类型（用于类型提升，如 int + float -> float）
ZR_PARSER_API TZrBool ZrParser_InferredType_GetCommonType(SZrState *state, SZrInferredType *result, const SZrInferredType *type1, const SZrInferredType *type2);

// 获取类型转换指令（如果需要转换）
// 返回转换指令，如果不需要转换则返回ZR_INSTRUCTION_ENUM(ENUM_MAX)
ZR_PARSER_API EZrInstructionCode ZrParser_InferredType_GetConversionOpcode(const SZrInferredType *fromType, const SZrInferredType *toType);

// 类型环境管理函数

// 创建类型环境
ZR_PARSER_API SZrTypeEnvironment *ZrParser_TypeEnvironment_New(SZrState *state);

// 销毁类型环境
ZR_PARSER_API void ZrParser_TypeEnvironment_Free(SZrState *state, SZrTypeEnvironment *env);

// 注册变量类型
ZR_PARSER_API TZrBool ZrParser_TypeEnvironment_RegisterVariable(SZrState *state, SZrTypeEnvironment *env, SZrString *name, const SZrInferredType *type);

// 查找变量类型
ZR_PARSER_API TZrBool ZrParser_TypeEnvironment_LookupVariable(SZrState *state, SZrTypeEnvironment *env, SZrString *name, SZrInferredType *result);

// 注册函数类型
ZR_PARSER_API TZrBool ZrParser_TypeEnvironment_RegisterFunction(SZrState *state, SZrTypeEnvironment *env, SZrString *name, const SZrInferredType *returnType, SZrArray *paramTypes);
ZR_PARSER_API TZrBool ZrParser_TypeEnvironment_RegisterFunctionEx(SZrState *state,
                                                                  SZrTypeEnvironment *env,
                                                                  SZrString *name,
                                                                  const SZrInferredType *returnType,
                                                                  SZrArray *paramTypes,
                                                                  SZrArray *genericParameters,
                                                                  SZrArray *parameterPassingModes,
                                                                  SZrAstNode *declarationNode);

// 查找函数类型
ZR_PARSER_API TZrBool ZrParser_TypeEnvironment_LookupFunction(SZrTypeEnvironment *env, SZrString *name, SZrFunctionTypeInfo **result);

// 查找同名函数候选集，results 为输出数组（元素类型为 SZrFunctionTypeInfo*）
ZR_PARSER_API TZrBool ZrParser_TypeEnvironment_LookupFunctions(SZrState *state, SZrTypeEnvironment *env, SZrString *name, SZrArray *results);

// 注册类型名称
ZR_PARSER_API TZrBool ZrParser_TypeEnvironment_RegisterType(SZrState *state, SZrTypeEnvironment *env, SZrString *typeName);

// 查找类型名称
ZR_PARSER_API TZrBool ZrParser_TypeEnvironment_LookupType(SZrTypeEnvironment *env, SZrString *typeName);

#endif //ZR_VM_PARSER_TYPE_SYSTEM_H
