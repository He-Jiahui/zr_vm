#include "type_inference_cast.h"

#include "zr_vm_parser/compiler.h"

/**
 * @brief 将 cast 节点作为“先分析源表达式、再采用目标类型”的类型推断边界。
 *
 * 上游表达式分派器将结果标记为 conversion 语义事实，供调用方查询；类型转换
 * 指令和转换时的运行期行为属于编译器路径。本函数即使源表达式无法确定类型，
 * 仍尝试分析它，以免 cast 抹去嵌套表达式的引用、调用身份和诊断。
 * @pre cs 必须持有有效 state；node 应来自完整解析的 cast 表达式，result 应已初始化且可接收推断结果。
 * @return 结构前置条件通过且目标类型能够转换时返回真；源表达式自身失败不改变
 *         目标类型决定的 cast 结果。
 * @note TODO: 解析恢复或外部手工构造的 AST 是否允许缺少 expression 尚未形成统一契约；
 *       当前实现容忍空操作数并返回目标类型，而常规解析器不会产生这种节点。
 */
TZrBool type_inference_cast_expression(
        SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    SZrInferredType operandType;

    /* 拒绝不能代表有效转换目标的节点；此处只守类型推断入口形态，不替代编译器校验。 */
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL ||
        node->type != ZR_AST_TYPE_CAST_EXPRESSION ||
        node->data.typeCastExpression.targetType == ZR_NULL) {
        return ZR_FALSE;
    }

    /* 先走完源表达式，令调用引用及嵌套子表达式事实可供语义查询；
     * 其失败可能只表示源类型未知，不能覆盖显式 cast 已声明的目标类型。 */
    if (node->data.typeCastExpression.expression != ZR_NULL) {
        ZrParser_InferredType_Init(cs->state, &operandType, ZR_VALUE_TYPE_OBJECT);
        (void)ZrParser_ExpressionType_Infer(
                cs, node->data.typeCastExpression.expression, &operandType);
        ZrParser_InferredType_Free(cs->state, &operandType);
    }

    /* 外层表达式分派器据此登记转换事实；运行期数值/对象转换由编译器后续处理。 */
    return ZrParser_AstTypeToInferredType_Convert(
            cs, node->data.typeCastExpression.targetType, result);
}
