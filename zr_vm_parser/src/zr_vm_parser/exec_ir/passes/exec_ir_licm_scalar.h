#ifndef ZR_VM_PARSER_EXEC_IR_LICM_SCALAR_H
#define ZR_VM_PARSER_EXEC_IR_LICM_SCALAR_H

#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_parser/exec_ir_builder.h"
#include <limits.h>

/* Explicit Oracle kinds are the representation contract. Metadata tokens and
 * CONSTANT.layoutId alone never establish a scalar value or numeric domain. */
static TZrBool zr_licm_scalar_constant(const SZrExecIrFunction *function,
        const SZrExecIrOracleInput *context, TZrExecIrValueId valueId,
        SZrExecIrOracleValue *value) {
    TZrUInt32 depth;
    if (context == ZR_NULL || context->function != function ||
        context->constants == ZR_NULL || value == ZR_NULL) return ZR_FALSE;
    for (depth = 0u; depth < function->valueCount; ++depth) {
        TZrExecIrInstructionId definition;
        const SZrExecIrInstruction *instruction;
        if (valueId == 0u || valueId > function->valueCount) return ZR_FALSE;
        definition = function->values[valueId - 1u].definition;
        if (definition == 0u || definition > function->instructionCount)
            return ZR_FALSE;
        instruction = &function->instructions[definition - 1u];
        if (instruction->opcode == ZR_EXEC_IR_OPCODE_CONSTANT) {
            if (instruction->layoutId >= context->constantCount) return ZR_FALSE;
            *value = context->constants[instruction->layoutId];
            return (TZrBool)(value->kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED ||
                            value->kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED);
        }
        if (instruction->opcode != ZR_EXEC_IR_OPCODE_COPY ||
            instruction->operands.count != 1u || function->operands == ZR_NULL ||
            instruction->operands.start >= function->operandCount)
            return ZR_FALSE;
        valueId = function->operands[instruction->operands.start];
    }
    return ZR_FALSE;
}

static TZrBool zr_licm_scalar_operands(const SZrExecIrFunction *function,
        const SZrExecIrOracleInput *context,
        const SZrExecIrInstruction *instruction,
        SZrExecIrOracleValue *left, SZrExecIrOracleValue *right) {
    return (TZrBool)(instruction->operands.count == 2u &&
        function->operands != ZR_NULL &&
        instruction->operands.start <= function->operandCount &&
        instruction->operands.count <= function->operandCount - instruction->operands.start &&
        zr_licm_scalar_constant(function, context,
            function->operands[instruction->operands.start], left) &&
        zr_licm_scalar_constant(function, context,
            function->operands[instruction->operands.start + 1u], right) &&
        left->kind == right->kind);
}

static TZrBool zr_licm_arithmetic_is_safe(const SZrExecIrFunction *function,
        const SZrExecIrOracleInput *context,
        const SZrExecIrInstruction *instruction) {
    SZrExecIrOracleValue left, right;
    EZrExecIrOpcode opcode = (EZrExecIrOpcode)instruction->opcode;
    if (opcode == ZR_EXEC_IR_OPCODE_CONSTANT || opcode == ZR_EXEC_IR_OPCODE_COPY)
        return ZR_TRUE;
    if (!zr_licm_scalar_operands(function, context, instruction, &left, &right))
        return ZR_FALSE;
    if (left.kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED) {
        TZrUInt64 a = left.as.unsignedInteger, b = right.as.unsignedInteger;
        switch (opcode) {
            case ZR_EXEC_IR_OPCODE_ADD: return (TZrBool)(a <= UINT64_MAX - b);
            case ZR_EXEC_IR_OPCODE_SUB: return (TZrBool)(a >= b);
            case ZR_EXEC_IR_OPCODE_MUL: return (TZrBool)(b == 0u || a <= UINT64_MAX / b);
            case ZR_EXEC_IR_OPCODE_DIV: return (TZrBool)(b != 0u);
            default: return ZR_FALSE;
        }
    } else {
        TZrInt64 a = left.as.signedInteger, b = right.as.signedInteger;
        switch (opcode) {
            case ZR_EXEC_IR_OPCODE_ADD:
                return (TZrBool)(!((b > 0 && a > INT64_MAX - b) ||
                                  (b < 0 && a < INT64_MIN - b)));
            case ZR_EXEC_IR_OPCODE_SUB:
                return (TZrBool)(!((b < 0 && a > INT64_MAX + b) ||
                                  (b > 0 && a < INT64_MIN + b)));
            case ZR_EXEC_IR_OPCODE_MUL:
                return (TZrBool)(a == 0 || b == 0 ||
                    !((a == INT64_MIN && b == -1) || (b == INT64_MIN && a == -1) ||
                      (a > 0 && b > 0 && a > INT64_MAX / b) ||
                      (a > 0 && b < 0 && b < INT64_MIN / a) ||
                      (a < 0 && b > 0 && a < INT64_MIN / b) ||
                      (a < 0 && b < 0 && a < INT64_MAX / b)));
            case ZR_EXEC_IR_OPCODE_DIV:
                return (TZrBool)(b != 0 && !(a == INT64_MIN && b == -1));
            default: return ZR_FALSE;
        }
    }
}

static TZrBool zr_licm_strength_identity(const SZrExecIrFunction *function,
        const SZrExecIrOracleInput *context,
        const SZrExecIrInstruction *instruction, TZrExecIrValueId *baseValue) {
    SZrExecIrOracleValue left, right;
    TZrBool leftZero, rightZero, rightOne;
    if (baseValue == ZR_NULL || instruction->results.count != 1u ||
        !zr_licm_instruction_is_pure(instruction) ||
        !zr_licm_scalar_operands(function, context, instruction, &left, &right))
        return ZR_FALSE;
    if (left.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED) {
        leftZero = (TZrBool)(left.as.signedInteger == 0);
        rightZero = (TZrBool)(right.as.signedInteger == 0);
        rightOne = (TZrBool)(right.as.signedInteger == 1);
    } else {
        leftZero = (TZrBool)(left.as.unsignedInteger == 0u);
        rightZero = (TZrBool)(right.as.unsignedInteger == 0u);
        rightOne = (TZrBool)(right.as.unsignedInteger == 1u);
    }
    *baseValue = function->operands[instruction->operands.start];
    if (instruction->opcode == ZR_EXEC_IR_OPCODE_MUL) return rightOne;
    if (instruction->opcode == ZR_EXEC_IR_OPCODE_ADD ||
        instruction->opcode == ZR_EXEC_IR_OPCODE_SUB) {
        if (rightZero) return ZR_TRUE;
        if (instruction->opcode == ZR_EXEC_IR_OPCODE_ADD && leftZero) {
            *baseValue = function->operands[instruction->operands.start + 1u];
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* A proven DIV still carries the schema's MAY_THROW effect chain. Restrict
 * its relocation to scalar CFGs whose effect chains can be rebuilt from the
 * existing synthesizer; never erase memory, calls or other observable effects. */
static TZrBool zr_licm_scalar_effect_cfg(const SZrExecIrFunction *function) {
    TZrUInt32 i, region, phiIndex;
    if (function->memoryTokenCount != 0u || function->stateMap != ZR_NULL ||
        function->deoptStateCount != 0u || function->gcMap != ZR_NULL)
        return ZR_FALSE;
    for (i = 0u; i < function->blockCount; ++i) {
        SZrExecIrRange effectRange = function->blocks[i].effectPhiIncomings;
        for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region)
            if (function->blocks[i].memoryPhiResults[region] != 0u ||
                function->blocks[i].memoryPhiIncomings[region].count != 0u)
                return ZR_FALSE;
        /* Effect and value phis share storage but must retain distinct rows. */
        for (phiIndex = 0u; phiIndex < function->phiCount; ++phiIndex) {
            SZrExecIrRange valueRange = function->phiPool[phiIndex].incomings;
            if (effectRange.count != 0u && valueRange.count != 0u &&
                effectRange.start < valueRange.start + valueRange.count &&
                valueRange.start < effectRange.start + effectRange.count)
                return ZR_FALSE;
        }
    }
    for (i = 0u; i < function->instructionCount; ++i) {
        const SZrExecIrInstruction *ins = &function->instructions[i];
        if (ins->memoryIn.count != 0u || ins->memoryOut.count != 0u ||
            ins->deoptId != 0u || ins->bindingRow != 0u ||
            ins->phiRange.count != 0u) return ZR_FALSE;
        if (ins->opcode == ZR_EXEC_IR_OPCODE_DIV) {
            if ((ins->flags & ~ZR_EXEC_IR_FLAG_MAY_THROW) != 0u) return ZR_FALSE;
            continue;
        }
        if (ins->flags != 0u || ins->effectIn != 0u || ins->effectOut != 0u)
            return ZR_FALSE;
        switch ((EZrExecIrOpcode)ins->opcode) {
            case ZR_EXEC_IR_OPCODE_CONSTANT: case ZR_EXEC_IR_OPCODE_COPY:
            case ZR_EXEC_IR_OPCODE_CONVERT: case ZR_EXEC_IR_OPCODE_ADD:
            case ZR_EXEC_IR_OPCODE_SUB: case ZR_EXEC_IR_OPCODE_MUL:
            case ZR_EXEC_IR_OPCODE_COMPARE: case ZR_EXEC_IR_OPCODE_BRANCH:
            case ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH: case ZR_EXEC_IR_OPCODE_RETURN:
                break;
            default: return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_licm_safe_div_candidate(const SZrExecIrFunction *function,
        const SZrExecIrOracleInput *context,
        const SZrExecIrInstruction *instruction) {
    return (TZrBool)(instruction->opcode == ZR_EXEC_IR_OPCODE_DIV &&
        instruction->phiRange.count == 0u && instruction->successorRange.count == 0u &&
        zr_licm_arithmetic_is_safe(function, context, instruction) &&
        zr_licm_scalar_effect_cfg(function));
}

static TZrBool zr_licm_is_effect_incoming(const SZrExecIrFunction *function,
                                         TZrUInt32 incomingIndex) {
    TZrUInt32 blockIndex;
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        SZrExecIrRange range = function->blocks[blockIndex].effectPhiIncomings;
        if (incomingIndex >= range.start && incomingIndex - range.start < range.count)
            return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool zr_licm_rebuild_scalar_effects(SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 i, phiIndex, writeIndex = 0u;
    /* The structural verifier validates every pooled incoming row. Retire old
     * effect-only rows while their ranges still classify them, otherwise token
     * zero or a token above valueCount becomes an invalid ordinary SSA value.
     * Preserve all other rows and remap the value-PHI ranges before synthesis. */
    for (phiIndex = 0u; phiIndex < function->phiCount; ++phiIndex) {
        SZrExecIrRange *range = &function->phiPool[phiIndex].incomings;
        TZrUInt32 removedBefore = 0u;
        for (i = 0u; i < range->start; ++i)
            if (zr_licm_is_effect_incoming(function, i)) ++removedBefore;
        range->start -= removedBefore;
    }
    for (i = 0u; i < function->phiIncomingCount; ++i)
        if (!zr_licm_is_effect_incoming(function, i))
            function->phiIncoming[writeIndex++] = function->phiIncoming[i];
    function->phiIncomingCount = writeIndex;
    for (i = 0u; i < function->instructionCount; ++i) {
        function->instructions[i].effectIn = 0u;
        function->instructions[i].effectOut = 0u;
        function->instructions[i].phiRange.start = 0u;
    }
    for (i = 0u; i < function->blockCount; ++i) {
        function->blocks[i].effectPhiResult = 0u;
        function->blocks[i].effectPhiIncomings.start = 0u;
        function->blocks[i].effectPhiIncomings.count = 0u;
    }
    return ZrParser_ExecIr_SynthesizeCfgEffects(function, diagnostic);
}
#endif
