#include "type_inference_cast.h"

#include "zr_vm_parser/compiler.h"

TZrBool type_inference_cast_expression(
        SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    SZrInferredType operandType;

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL ||
        node->type != ZR_AST_TYPE_CAST_EXPRESSION ||
        node->data.typeCastExpression.targetType == ZR_NULL) {
        return ZR_FALSE;
    }

    if (node->data.typeCastExpression.expression != ZR_NULL) {
        ZrParser_InferredType_Init(cs->state, &operandType, ZR_VALUE_TYPE_OBJECT);
        /* Keep operand facts and diagnostics even when its type is unresolved. */
        (void)ZrParser_ExpressionType_Infer(
                cs, node->data.typeCastExpression.expression, &operandType);
        ZrParser_InferredType_Free(cs->state, &operandType);
    }

    return ZrParser_AstTypeToInferredType_Convert(
            cs, node->data.typeCastExpression.targetType, result);
}
