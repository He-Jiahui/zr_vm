#ifndef ZR_VM_PARSER_SEMANTIC_IR_COMPARE_INTERNAL_H
#define ZR_VM_PARSER_SEMANTIC_IR_COMPARE_INTERNAL_H

#include "zr_vm_core/exec_ir.h"
#include "zr_vm_parser/semantic_ir.h"

/* This relational check is independent of a canonical context. Primitive
 * INT64/BOOL meaning is checked by the compiler and typed VM boundary. */
static ZR_FORCE_INLINE TZrBool semantic_ir_compare_values_valid(
        const SZrSemanticIrFunction *function, EZrSemanticIrOpcode opcode,
        TZrUInt32 predicate, TZrTypeId operandType, TZrTypeId resultType,
        TZrValueId resultId, const TZrValueId *operands, TZrSize count) {
    const SZrSemanticIrValue *result;
    TZrSize index;
    if (opcode != ZR_SEMANTIC_IR_COMPARE)
        return (TZrBool)(predicate == 0u && operandType == 0u);
    if (function == ZR_NULL ||
        (predicate != ZR_EXEC_IR_COMPARE_KIND_LESS &&
         predicate != ZR_EXEC_IR_COMPARE_KIND_GREATER) ||
        operandType == 0u || resultType == 0u || operandType == resultType ||
        count != 2u || operands == ZR_NULL || resultId == 0u ||
        resultId > function->values.length) return ZR_FALSE;
    result = (const SZrSemanticIrValue *)ZrCore_Array_Get(
            (SZrArray *)&function->values, resultId - 1u);
    if (result == ZR_NULL || result->id != resultId ||
        result->typeId != resultType) return ZR_FALSE;
    for (index = 0u; index < count; ++index) {
        const SZrSemanticIrValue *value;
        if (operands[index] == 0u || operands[index] > function->values.length ||
            operands[index] == resultId) return ZR_FALSE;
        value = (const SZrSemanticIrValue *)ZrCore_Array_Get(
                (SZrArray *)&function->values, operands[index] - 1u);
        if (value == ZR_NULL || value->id != operands[index] ||
            value->typeId != operandType) return ZR_FALSE;
    }
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool semantic_ir_compare_instruction_valid(
        const SZrSemanticIrFunction *function,
        const SZrSemanticIrInstruction *in) {
    const TZrValueId *operands = ZR_NULL;
    if (function == ZR_NULL || in == ZR_NULL) return ZR_FALSE;
    if (in->operandStart > function->valueOperands.length ||
        in->operandCount > function->valueOperands.length - in->operandStart)
        return ZR_FALSE;
    if (in->operandCount != 0u) {
        operands = (const TZrValueId *)ZrCore_Array_Get(
                (SZrArray *)&function->valueOperands, in->operandStart);
        if (operands == ZR_NULL) return ZR_FALSE;
    }
    return semantic_ir_compare_values_valid(function, in->opcode,
            in->comparisonPredicate, in->comparisonOperandTypeId, in->typeId,
            in->resultValueId, operands, in->operandCount);
}

#endif
