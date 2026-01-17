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

// 类型推断函数

// 从AST节点推断类型（主入口函数）
ZR_PARSER_API TBool infer_expression_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从字面量推断类型
ZR_PARSER_API TBool infer_literal_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从标识符推断类型
ZR_PARSER_API TBool infer_identifier_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从二元表达式推断类型
ZR_PARSER_API TBool infer_binary_expression_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从一元表达式推断类型
ZR_PARSER_API TBool infer_unary_expression_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从函数调用推断类型
ZR_PARSER_API TBool infer_function_call_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从Lambda表达式推断类型
ZR_PARSER_API TBool infer_lambda_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从数组字面量推断类型
ZR_PARSER_API TBool infer_array_literal_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从对象字面量推断类型
ZR_PARSER_API TBool infer_object_literal_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从条件表达式推断类型
ZR_PARSER_API TBool infer_conditional_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从赋值表达式推断类型
ZR_PARSER_API TBool infer_assignment_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 从primary expression推断类型（包括函数调用）
ZR_PARSER_API TBool infer_primary_expression_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

// 类型检查函数

// 检查类型兼容性（用于赋值等场景）
ZR_PARSER_API TBool check_type_compatibility(SZrCompilerState *cs, const SZrInferredType *fromType, const SZrInferredType *toType, SZrFileRange location);

// 检查赋值兼容性
ZR_PARSER_API TBool check_assignment_compatibility(SZrCompilerState *cs, const SZrInferredType *leftType, const SZrInferredType *rightType, SZrFileRange location);

// 检查函数调用参数兼容性
ZR_PARSER_API TBool check_function_call_compatibility(SZrCompilerState *cs, SZrFunctionTypeInfo *funcType, SZrAstNodeArray *args, SZrFileRange location);

// 报告类型错误
ZR_PARSER_API void report_type_error(SZrCompilerState *cs, const TChar *message, const SZrInferredType *expectedType, const SZrInferredType *actualType, SZrFileRange location);

// 获取类型名称字符串（用于错误报告）
ZR_PARSER_API const TChar *get_type_name_string(SZrState *state, const SZrInferredType *type, TChar *buffer, TZrSize bufferSize);

// 将AST类型注解转换为推断类型
ZR_PARSER_API TBool convert_ast_type_to_inferred_type(SZrCompilerState *cs, const SZrType *astType, SZrInferredType *result);

// 字面量范围检查函数
ZR_PARSER_API TBool check_literal_range(SZrCompilerState *cs, SZrAstNode *literalNode, const SZrInferredType *targetType, SZrFileRange location);

// 数组索引边界检查函数
ZR_PARSER_API TBool check_array_index_bounds(SZrCompilerState *cs, SZrAstNode *indexExpr, const SZrInferredType *arrayType, SZrFileRange location);

#endif //ZR_VM_PARSER_TYPE_INFERENCE_H

