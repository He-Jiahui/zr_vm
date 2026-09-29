#include "exec_ir_fusion_internal.h"

#include "zr_vm_common/zr_type_conf.h"

TZrBool zr_fusion_operand_at(const SZrExecIrFunction *function,
                             const SZrExecIrInstruction *instruction,
                             TZrUInt32 index,
                             TZrExecIrValueId *value) {
    if (value == ZR_NULL || function == ZR_NULL || instruction == ZR_NULL ||
        instruction->operands.count == ZR_EXEC_IR_VARIADIC ||
        index >= instruction->operands.count ||
        instruction->operands.start > function->operandCount ||
        index >= function->operandCount - instruction->operands.start) {
        return ZR_FALSE;
    }
    *value = function->operands[instruction->operands.start + index];
    return ZR_TRUE;
}

TZrBool zr_fusion_result_at(const SZrExecIrFunction *function,
                            const SZrExecIrInstruction *instruction,
                            TZrUInt32 index,
                            TZrExecIrValueId *value) {
    if (value == ZR_NULL || function == ZR_NULL || instruction == ZR_NULL ||
        instruction->results.count == ZR_EXEC_IR_VARIADIC ||
        index >= instruction->results.count ||
        instruction->results.start > function->resultCount ||
        index >= function->resultCount - instruction->results.start) {
        return ZR_FALSE;
    }
    *value = function->results[instruction->results.start + index];
    return ZR_TRUE;
}

TZrBool zr_fusion_typed_instruction_is_compatible(
        const SZrExecIrFunction *function,
        const SZrExecIrInstruction *instruction) {
    TZrUInt32 index;
    TZrExecIrTypeToken declaredType;
    if (function == ZR_NULL || instruction == ZR_NULL ||
        instruction->operands.count == ZR_EXEC_IR_VARIADIC ||
        instruction->results.count == ZR_EXEC_IR_VARIADIC ||
        (instruction->operands.count == 0u &&
         instruction->results.count == 0u)) {
        return ZR_FALSE;
    }
    /* Compare's typeToken is an operation selector (for example, 2 means
     * <=), not its result type.  The integer branch fusion is intentionally
     * limited to signed i64 inputs and a BOOL result; the selector is copied
     * to the side entry separately for future execution. */
    if ((EZrExecIrOpcode)instruction->opcode == ZR_EXEC_IR_OPCODE_COMPARE) {
        TZrExecIrValueId left;
        TZrExecIrValueId right;
        TZrExecIrValueId result;
        /* The canonical selectors are 0 (EQ) and 1..5 (LT, LE, GT, GE,
         * NE).  Oracle's default branch currently treats other values as EQ,
         * but that is not a declared mode that a future fused handler can
         * safely assume. */
        if (instruction->typeToken > 5u ||
            instruction->operands.count != 2u ||
            instruction->results.count != 1u ||
            !zr_fusion_operand_at(function, instruction, 0u, &left) ||
            !zr_fusion_operand_at(function, instruction, 1u, &right) ||
            !zr_fusion_result_at(function, instruction, 0u, &result) ||
            left == ZR_EXEC_IR_VALUE_ID_INVALID ||
            right == ZR_EXEC_IR_VALUE_ID_INVALID ||
            result == ZR_EXEC_IR_VALUE_ID_INVALID ||
            left > function->valueCount || right > function->valueCount ||
            result > function->valueCount) {
            return ZR_FALSE;
        }
        return (TZrBool)(function->values[left - 1u].typeToken ==
                                 ZR_VALUE_TYPE_INT64 &&
                         function->values[right - 1u].typeToken ==
                                 ZR_VALUE_TYPE_INT64 &&
                         function->values[result - 1u].typeToken ==
                                 ZR_VALUE_TYPE_BOOL);
    }
    /* ExecIR operation type tokens describe the produced value when there is
     * one.  A place projection and its receiver/index operands are expected
     * to have different types, so requiring every operand to equal that
     * token would discard valid typed index/load and binding windows.  The
     * actual proof needed by fusion is that every referenced value has a
     * known type, plus an explicit instruction token (when present) agrees
     * with its result, or with its sole consumer operand for a no-result
     * instruction such as a branch. */
    declaredType = instruction->typeToken;
    for (index = 0u; index < instruction->operands.count; ++index) {
        TZrExecIrValueId valueId;
        if (!zr_fusion_operand_at(function, instruction, index, &valueId) ||
            valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
            valueId > function->valueCount ||
            function->values[valueId - 1u].typeToken == 0u) {
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < instruction->results.count; ++index) {
        TZrExecIrValueId valueId;
        if (!zr_fusion_result_at(function, instruction, index, &valueId) ||
            valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
            valueId > function->valueCount ||
            function->values[valueId - 1u].typeToken == 0u ||
            (declaredType != 0u &&
             function->values[valueId - 1u].typeToken != declaredType)) {
            return ZR_FALSE;
        }
    }
    if (declaredType != 0u && instruction->results.count == 0u) {
        TZrExecIrValueId valueId;
        if (!zr_fusion_operand_at(function, instruction, 0u, &valueId) ||
            valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
            valueId > function->valueCount ||
            function->values[valueId - 1u].typeToken != declaredType) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool zr_fusion_increment_loop_branch_types_are_compatible(
        const SZrExecIrFunction *function,
        const SZrExecIrInstruction *head,
        const SZrExecIrInstruction *tail) {
    TZrExecIrValueId left;
    TZrExecIrValueId right;
    TZrExecIrValueId result;
    TZrExecIrValueId condition;

    if (function == ZR_NULL || head == ZR_NULL || tail == ZR_NULL ||
        (EZrExecIrOpcode)head->opcode != ZR_EXEC_IR_OPCODE_ADD ||
        (EZrExecIrOpcode)tail->opcode !=
                ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH ||
        head->typeToken != ZR_VALUE_TYPE_INT64 ||
        tail->typeToken != ZR_VALUE_TYPE_INT64 ||
        head->operands.count != 2u || head->results.count != 1u ||
        tail->operands.count != 1u || tail->results.count != 0u ||
        !zr_fusion_operand_at(function, head, 0u, &left) ||
        !zr_fusion_operand_at(function, head, 1u, &right) ||
        !zr_fusion_result_at(function, head, 0u, &result) ||
        !zr_fusion_operand_at(function, tail, 0u, &condition) ||
        left == ZR_EXEC_IR_VALUE_ID_INVALID ||
        right == ZR_EXEC_IR_VALUE_ID_INVALID ||
        result == ZR_EXEC_IR_VALUE_ID_INVALID ||
        condition == ZR_EXEC_IR_VALUE_ID_INVALID ||
        left > function->valueCount || right > function->valueCount ||
        result > function->valueCount || condition > function->valueCount ||
        result != condition) {
        return ZR_FALSE;
    }

    return (TZrBool)(function->values[left - 1u].typeToken ==
                                 ZR_VALUE_TYPE_INT64 &&
                     function->values[right - 1u].typeToken ==
                                 ZR_VALUE_TYPE_INT64 &&
                     function->values[result - 1u].typeToken ==
                                 ZR_VALUE_TYPE_INT64);
}
