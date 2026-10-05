#include "compiler_semantic_scalar_expression.h"

#include <string.h>

static TZrBool scalar_i64_type(const SZrCompilerState *cs, TZrTypeId id) {
    const SZrCanonicalTypeNode *type = cs == ZR_NULL || cs->semanticContext == ZR_NULL ? ZR_NULL :
            ZrParser_CanonicalType_Find(cs->semanticContext, id);
    return (TZrBool)(type != ZR_NULL && type->id == id &&
            type->kind == ZR_CANONICAL_TYPE_PRIMITIVE &&
            type->data.primitive.valueType == ZR_VALUE_TYPE_INT64);
}

TZrBool compiler_semantic_scalar_expression_shape(const SZrAstNode *node) {
    const TZrChar *op;
    if (node == ZR_NULL) return ZR_FALSE;
    if (node->type == ZR_AST_INTEGER_LITERAL ||
        node->type == ZR_AST_IDENTIFIER_LITERAL) return ZR_TRUE;
    if (node->type != ZR_AST_BINARY_EXPRESSION) return ZR_FALSE;
    op = node->data.binaryExpression.op.op;
    return (TZrBool)(op != ZR_NULL &&
            (strcmp(op, "+") == 0 || strcmp(op, "-") == 0) &&
            compiler_semantic_scalar_expression_shape(node->data.binaryExpression.left) &&
            compiler_semantic_scalar_expression_shape(node->data.binaryExpression.right));
}

TZrBool compiler_semantic_scalar_expression_typed(SZrCompilerState *cs,
                                                 const SZrAstNode *node) {
    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL ||
        !compiler_semantic_scalar_expression_shape(node)) return ZR_FALSE;
    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        TZrUInt32 slotId = find_local_var(cs, node->data.identifier.name);
        const SZrCompilerSemanticIrSlot *slot =
                compiler_semantic_ir_find_slot(cs, slotId);
        return (TZrBool)(slotId != ZR_PARSER_SLOT_NONE && slot != ZR_NULL &&
                scalar_i64_type(cs, slot->typeId));
    }
    if (node->type == ZR_AST_INTEGER_LITERAL) {
        SZrInferredType inferred;
        TZrTypeId id = ZR_SEMANTIC_ID_INVALID;
        ZrParser_InferredType_Init(cs->state, &inferred, ZR_VALUE_TYPE_NULL);
        if (ZrParser_ExpressionType_Infer(cs, (SZrAstNode *)node, &inferred))
            id = ZrParser_Semantic_RegisterInferredType(cs->semanticContext,
                    &inferred, ZR_SEMANTIC_TYPE_KIND_UNKNOWN, ZR_NULL, ZR_NULL);
        ZrParser_InferredType_Free(cs->state, &inferred);
        return scalar_i64_type(cs, id);
    }
    return (TZrBool)(compiler_semantic_scalar_expression_typed(
                            cs, node->data.binaryExpression.left) &&
                    compiler_semantic_scalar_expression_typed(
                            cs, node->data.binaryExpression.right));
}

/* This extension has no conditional transfer, cleanup, or nested loop surface. */
TZrBool compiler_semantic_scalar_while_body_supported(SZrCompilerState *cs,
                                                     const SZrAstNode *node) {
    TZrSize index;
    const SZrAstNode *expression;
    if (node == ZR_NULL) return ZR_TRUE;
    if (node->type == ZR_AST_BLOCK) {
        if (node->data.block.body == ZR_NULL) return ZR_TRUE;
        for (index = 0U; index < node->data.block.body->count; ++index)
            if (!compiler_semantic_scalar_while_body_supported(
                    cs, node->data.block.body->nodes[index])) return ZR_FALSE;
        return ZR_TRUE;
    }
    if (node->type != ZR_AST_EXPRESSION_STATEMENT) return ZR_FALSE;
    expression = node->data.expressionStatement.expr;
    if (expression != ZR_NULL && expression->type == ZR_AST_ASSIGNMENT_EXPRESSION)
        return (TZrBool)(expression->data.assignmentExpression.op.op != ZR_NULL &&
                strcmp(expression->data.assignmentExpression.op.op, "=") == 0 &&
                expression->data.assignmentExpression.left != ZR_NULL &&
                expression->data.assignmentExpression.left->type == ZR_AST_IDENTIFIER_LITERAL &&
                compiler_semantic_scalar_expression_typed(
                        cs, expression->data.assignmentExpression.left) &&
                compiler_semantic_scalar_expression_typed(
                        cs, expression->data.assignmentExpression.right));
    return compiler_semantic_scalar_expression_typed(cs, expression);
}

TZrBool compiler_semantic_scalar_value_matches(const SZrCompilerState *cs,
        const SZrAstNode *node, TZrValueId valueId) {
    const SZrSemanticIrValue *value;
    const SZrSemanticIrInstruction *in;
    const TZrValueId *left, *right;
    if (cs == ZR_NULL || !compiler_semantic_scalar_expression_shape(node)) return ZR_FALSE;
    value = ZrParser_SemanticIr_Value(&cs->preSemanticIr, valueId);
    if (value == ZR_NULL || !scalar_i64_type(cs, value->typeId) ||
        value->definitionInstructionId == 0U ||
        value->definitionInstructionId > cs->preSemanticIr.instructions.length) return ZR_FALSE;
    in = ZrParser_SemanticIr_InstructionAt(&cs->preSemanticIr,
                                         value->definitionInstructionId - 1U);
    if (in == ZR_NULL || in->resultValueId != valueId || in->typeId != value->typeId ||
        in->sourceRange.source != node->location.source ||
        in->sourceRange.start.offset != node->location.start.offset ||
        in->sourceRange.end.offset != node->location.end.offset) return ZR_FALSE;
    if (node->type == ZR_AST_INTEGER_LITERAL)
        return (TZrBool)(in->opcode == ZR_SEMANTIC_IR_CONSTANT && in->hasConstantPoolIndex);
    if (node->type == ZR_AST_IDENTIFIER_LITERAL)
        return (TZrBool)(in->opcode == ZR_SEMANTIC_IR_LOAD &&
                in->placeId != ZR_SEMANTIC_ID_INVALID);
    if (in->opcode != (strcmp(node->data.binaryExpression.op.op, "+") == 0
            ? ZR_SEMANTIC_IR_ADD : ZR_SEMANTIC_IR_SUB) || in->operandCount != 2U ||
        in->operandStart > cs->preSemanticIr.valueOperands.length ||
        cs->preSemanticIr.valueOperands.length - in->operandStart < 2U) return ZR_FALSE;
    left = (const TZrValueId *)ZrCore_Array_Get((SZrArray *)&cs->preSemanticIr.valueOperands, in->operandStart);
    right = (const TZrValueId *)ZrCore_Array_Get((SZrArray *)&cs->preSemanticIr.valueOperands, in->operandStart + 1U);
    return (TZrBool)(left != ZR_NULL && right != ZR_NULL &&
            compiler_semantic_scalar_value_matches(cs, node->data.binaryExpression.left, *left) &&
            compiler_semantic_scalar_value_matches(cs, node->data.binaryExpression.right, *right));
}
