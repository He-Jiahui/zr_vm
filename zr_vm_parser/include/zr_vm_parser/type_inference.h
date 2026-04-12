//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_PARSER_TYPE_INFERENCE_H
#define ZR_VM_PARSER_TYPE_INFERENCE_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/type_system.h"
#include "zr_vm_parser/ast.h"

// 前向声明
struct SZrCompilerState;
typedef struct SZrCompilerState SZrCompilerState;

typedef struct SZrResolvedCallSignature {
    SZrInferredType returnType;
    SZrArray parameterTypes;          // SZrInferredType
    SZrArray parameterPassingModes;   // EZrParameterPassingMode
} SZrResolvedCallSignature;

// 类型推断函数

// 从AST节点推断类型（主入口函数）
ZR_PARSER_API TZrBool ZrParser_ExpressionType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从字面量推断类型
ZR_PARSER_API TZrBool ZrParser_LiteralType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从标识符推断类型
ZR_PARSER_API TZrBool ZrParser_IdentifierType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从二元表达式推断类型
ZR_PARSER_API TZrBool ZrParser_BinaryExpressionType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从一元表达式推断类型
ZR_PARSER_API TZrBool ZrParser_UnaryExpressionType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从函数调用推断类型
ZR_PARSER_API TZrBool ZrParser_FunctionCallType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从Lambda表达式推断类型
ZR_PARSER_API TZrBool ZrParser_LambdaType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从数组字面量推断类型
ZR_PARSER_API TZrBool ZrParser_ArrayLiteralType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从对象字面量推断类型
ZR_PARSER_API TZrBool ZrParser_ObjectLiteralType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从条件表达式推断类型
ZR_PARSER_API TZrBool ZrParser_ConditionalType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从赋值表达式推断类型
ZR_PARSER_API TZrBool ZrParser_AssignmentType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从primary expression推断类型（包括函数调用）
ZR_PARSER_API TZrBool ZrParser_PrimaryExpressionType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 类型检查函数

// 检查类型兼容性（用于赋值等场景）
ZR_PARSER_API TZrBool ZrParser_TypeCompatibility_Check(SZrCompilerState *cs, const SZrInferredType *fromType, const SZrInferredType *toType, SZrFileRange location);

// 检查赋值兼容性
ZR_PARSER_API TZrBool ZrParser_AssignmentCompatibility_Check(SZrCompilerState *cs, const SZrInferredType *leftType, const SZrInferredType *rightType, SZrFileRange location);
ZR_PARSER_API TZrBool ZrParser_InferredType_SatisfiesNamedConstraint(SZrCompilerState *cs,
                                                                     const SZrInferredType *actualType,
                                                                     SZrString *constraintTypeName);

// 检查函数调用参数兼容性
ZR_PARSER_API TZrBool ZrParser_FunctionCallCompatibility_Check(SZrCompilerState *cs,
                                                      SZrTypeEnvironment *env,
                                                      SZrString *funcName,
                                                      SZrFunctionCall *call,
                                                      SZrFunctionTypeInfo *funcType,
                                                      const SZrResolvedCallSignature *resolvedSignature,
                                                      SZrFileRange location);

// 解析函数调用的最佳重载
ZR_PARSER_API TZrBool ZrParser_FunctionCallOverload_Resolve(SZrCompilerState *cs,
                                                   SZrTypeEnvironment *env,
                                                   SZrString *funcName,
                                                   SZrFunctionCall *call,
                                                   SZrFileRange location,
                                                   SZrFunctionTypeInfo **resolvedFunction,
                                                   SZrResolvedCallSignature *resolvedSignature);

// 报告类型错误
ZR_PARSER_API void ZrParser_TypeError_Report(SZrCompilerState *cs, const TZrChar *message, const SZrInferredType *expectedType, const SZrInferredType *actualType, SZrFileRange location);

// 获取类型名称字符串（用于错误报告）
ZR_PARSER_API const TZrChar *ZrParser_TypeNameString_Get(SZrState *state, const SZrInferredType *type, TZrChar *buffer, TZrSize bufferSize);

// 将AST类型注解转换为推断类型
ZR_PARSER_API TZrBool ZrParser_AstTypeToInferredType_Convert(SZrCompilerState *cs, const SZrType *astType, SZrInferredType *result);

// 字面量范围检查函数
ZR_PARSER_API TZrBool ZrParser_LiteralRange_Check(SZrCompilerState *cs, SZrAstNode *literalNode, const SZrInferredType *targetType, SZrFileRange location);

// 数组索引边界检查函数
ZR_PARSER_API TZrBool ZrParser_ArrayIndexBounds_Check(SZrCompilerState *cs, SZrAstNode *indexExpr, const SZrInferredType *arrayType, SZrFileRange location);

// 从 foreach 可迭代对象推断元素类型
ZR_PARSER_API TZrBool bind_foreach_element_type_from_inferred_iterable(SZrCompilerState *cs,
                                                                       const SZrInferredType *iterableType,
                                                                       SZrInferredType *outType);

#endif //ZR_VM_PARSER_TYPE_INFERENCE_H
