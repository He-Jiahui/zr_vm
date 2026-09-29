#ifndef ZR_VM_PARSER_TYPE_INFERENCE_CAST_H
#define ZR_VM_PARSER_TYPE_INFERENCE_CAST_H

#include "zr_vm_parser/type_inference.h"

/**
 * @brief 为显式类型转换表达式保留操作数语义事实并推导转换后的静态类型。
 *
 * 由 ZrParser_ExpressionType_Infer 的 cast 分支调用；表达式级语义事实随后
 * 由外层推断流程登记。此处不执行转换，也不判定运行时转换是否合法；编译器
 * 的 cast 发码路径负责按目标类型选择实际指令。
 * @pre cs 必须持有有效 state；node 必须是带有目标类型的 cast AST；调用方应以已初始化的 result 接收结果。
 * @return 输入形态有效且目标类型可转换为推断类型时返回真。
 * @note 操作数推断失败不会否决已知目标类型，目的是保留其可产生的诊断和语义事实。
 */
TZrBool type_inference_cast_expression(
        SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);

#endif
