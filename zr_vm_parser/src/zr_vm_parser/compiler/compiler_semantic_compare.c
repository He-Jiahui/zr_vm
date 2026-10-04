#include "compiler_semantic_compare.h"
#include "../semantic_ir_compare_internal.h"

#include <string.h>

TZrBool compiler_semantic_compare_source_supported(const SZrAstNode *node) {
    const TZrChar *op;
    if (node == ZR_NULL || node->type != ZR_AST_BINARY_EXPRESSION ||
        node->data.binaryExpression.left == ZR_NULL ||
        node->data.binaryExpression.right == ZR_NULL ||
        node->data.binaryExpression.left->type != ZR_AST_INTEGER_LITERAL ||
        node->data.binaryExpression.right->type != ZR_AST_INTEGER_LITERAL)
        return ZR_FALSE;
    op = node->data.binaryExpression.op.op;
    return (TZrBool)(op != ZR_NULL &&
            (strcmp(op, "<") == 0 || strcmp(op, ">") == 0));
}

static TZrBool primitive_matches(const SZrSemanticContext *context,
                                TZrTypeId id, EZrValueType primitive) {
    const SZrCanonicalTypeNode *node = ZrParser_CanonicalType_Find(context, id);
    return (TZrBool)(node != ZR_NULL && node->id == id &&
            node->kind == ZR_CANONICAL_TYPE_PRIMITIVE &&
            node->data.primitive.valueType == primitive);
}

EZrCompilerSemanticCompareResult compiler_semantic_compare_lower(
        SZrCompilerState *cs, const SZrAstNode *node,
        EZrInstructionCode opcode, TZrUInt32 leftSlot, TZrUInt32 rightSlot,
        TZrUInt32 resultSlot, const SZrInferredType *resultType) {
    SZrSemanticIrInstructionSpec spec = {0};
    const SZrCompilerSemanticIrSlot *left, *right;
    TZrValueId operands[2], resultId;
    TZrTypeId operandType, boolType;
    TZrUInt32 predicate;
    if (!compiler_semantic_compare_source_supported(node) ||
        (cs != ZR_NULL && cs->preSemanticIrCfgTerminated))
        return ZR_COMPILER_SEMANTIC_COMPARE_NOT_APPLICABLE;
    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || resultType == ZR_NULL ||
        !cs->preSemanticIrInitialized || cs->preSemanticIrCfgTerminated ||
        resultSlot == ZR_PARSER_SLOT_NONE) goto failure;
    predicate = strcmp(node->data.binaryExpression.op.op, "<") == 0
            ? ZR_EXEC_IR_COMPARE_KIND_LESS : ZR_EXEC_IR_COMPARE_KIND_GREATER;
    if (opcode != (predicate == ZR_EXEC_IR_COMPARE_KIND_LESS
                    ? ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED)
                    : ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED))) goto failure;
    left = compiler_semantic_ir_find_slot(cs, leftSlot);
    right = compiler_semantic_ir_find_slot(cs, rightSlot);
    if (left == ZR_NULL || right == ZR_NULL || left->valueId == 0u ||
        right->valueId == 0u || left->typeId != right->typeId) goto failure;
    /* Preserve IDs before registration/binding can grow compiler arrays. */
    operands[0] = left->valueId;
    operands[1] = right->valueId;
    operandType = left->typeId;
    if (!primitive_matches(cs->semanticContext, operandType, ZR_VALUE_TYPE_INT64))
        goto failure;
    boolType = ZrParser_Semantic_RegisterInferredType(cs->semanticContext,
            resultType, ZR_SEMANTIC_TYPE_KIND_UNKNOWN, ZR_NULL, ZR_NULL);
    if (!primitive_matches(cs->semanticContext, boolType, ZR_VALUE_TYPE_BOOL))
        goto failure;
    resultId = ZrParser_SemanticIr_AddValue(&cs->preSemanticIr, boolType, node->location);
    if (resultId == 0u || !compiler_semantic_ir_bind_result_value(
            cs, resultSlot, boolType, resultId, node->location)) goto failure;
    spec.opcode = ZR_SEMANTIC_IR_COMPARE;
    spec.typeId = boolType;
    spec.resultValueId = resultId;
    spec.operands = operands;
    spec.operandCount = 2u;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = node->location;
    spec.comparisonPredicate = predicate;
    spec.comparisonOperandTypeId = operandType;
    if (!compiler_semantic_ir_emit(cs, &spec)) goto failure;
    emit_instruction(cs, create_instruction_2(opcode,
            (TZrUInt16)resultSlot, (TZrUInt16)leftSlot, (TZrUInt16)rightSlot));
    return ZR_COMPILER_SEMANTIC_COMPARE_LOWERED;
failure:
    if (cs != ZR_NULL && node != ZR_NULL)
        ZrParser_Compiler_Error(cs, "Cannot produce canonical signed literal comparison", node->location);
    return ZR_COMPILER_SEMANTIC_COMPARE_FAILED;
}

TZrBool compiler_semantic_compare_validate(const SZrCompilerState *cs) {
    TZrSize index;
    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL) return ZR_FALSE;
    for (index = 0u; index < cs->preSemanticIr.instructions.length; ++index) {
        const SZrSemanticIrInstruction *in = ZrParser_SemanticIr_InstructionAt(
                &cs->preSemanticIr, index);
        if (in == ZR_NULL) return ZR_FALSE;
        if (in->opcode != ZR_SEMANTIC_IR_COMPARE) continue;
        if (!primitive_matches(cs->semanticContext, in->comparisonOperandTypeId,
                               ZR_VALUE_TYPE_INT64) ||
            !primitive_matches(cs->semanticContext, in->typeId, ZR_VALUE_TYPE_BOOL))
            return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool compiler_semantic_compare_condition_valid(
        const SZrCompilerState *cs, const SZrAstNode *node,
        const SZrSemanticIrValue *condition) {
    const SZrSemanticIrInstruction *in;
    TZrUInt32 predicate;
    if (cs == ZR_NULL || condition == ZR_NULL ||
        condition->definitionInstructionId == 0u ||
        condition->definitionInstructionId > cs->preSemanticIr.instructions.length)
        return ZR_FALSE;
    in = ZrParser_SemanticIr_InstructionAt(&cs->preSemanticIr,
            condition->definitionInstructionId - 1u);
    if (in == ZR_NULL) return ZR_FALSE;
    if (in->opcode != ZR_SEMANTIC_IR_COMPARE) return ZR_TRUE;
    if (!compiler_semantic_compare_source_supported(node)) return ZR_FALSE;
    predicate = strcmp(node->data.binaryExpression.op.op, "<") == 0
            ? ZR_EXEC_IR_COMPARE_KIND_LESS : ZR_EXEC_IR_COMPARE_KIND_GREATER;
    return (TZrBool)(in->comparisonPredicate == predicate &&
            in->resultValueId == condition->id && in->typeId == condition->typeId &&
            in->sourceRange.start.offset == node->location.start.offset &&
            in->sourceRange.end.offset == node->location.end.offset &&
            primitive_matches(cs->semanticContext, condition->typeId, ZR_VALUE_TYPE_BOOL));
}
