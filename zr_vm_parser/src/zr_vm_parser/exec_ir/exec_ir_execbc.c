#include "zr_vm_parser/exec_ir_execbc.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_common/zr_type_conf.h"

static void zr_execbc_diag(SZrExecIrDiagnostic *diagnostic,
                           EZrExecutionDiagnosticCode code,
                           const SZrExecBcProjection *projection,
                           TZrUInt32 block, TZrUInt32 instruction,
                           TZrUInt32 actual) {
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = projection != ZR_NULL
            ? projection->functionToken : 0u;
    diagnostic->blockId = block;
    diagnostic->instructionId = instruction;
    diagnostic->actualVersion = actual;
}

void ZrParser_ExecBcExecutionResult_Init(SZrExecBcExecutionResult *result) {
    if (result != ZR_NULL) {
        memset(result, 0, sizeof(*result));
        result->ownershipTag = ZR_EXEC_BC_EXECUTION_RESULT_TAG;
    }
}

void ZrParser_ExecBcExecutionResult_Free(SZrExecBcExecutionResult *result) {
    if (result != ZR_NULL && result->ownershipTag == ZR_EXEC_BC_EXECUTION_RESULT_TAG) {
        free(result->slots);
        memset(result, 0, sizeof(*result));
    }
}

static TZrBool zr_execbc_truthy(const SZrExecIrOracleValue *value) {
    if (value == ZR_NULL) return ZR_FALSE;
    switch (value->kind) {
        case ZR_EXEC_IR_ORACLE_VALUE_BOOL: return value->as.boolean != 0u;
        case ZR_EXEC_IR_ORACLE_VALUE_SIGNED: return value->as.signedInteger != 0;
        case ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED: return value->as.unsignedInteger != 0u;
        case ZR_EXEC_IR_ORACLE_VALUE_FLOAT: return value->as.floating != 0.0;
        default: return ZR_FALSE;
    }
}

static TZrBool zr_execbc_numeric(const SZrExecIrOracleValue *value) {
    return value != ZR_NULL && value->kind >= ZR_EXEC_IR_ORACLE_VALUE_BOOL &&
           value->kind <= ZR_EXEC_IR_ORACLE_VALUE_FLOAT;
}

static TZrInt64 zr_execbc_signed(const SZrExecIrOracleValue *value) {
    if (value->kind == ZR_EXEC_IR_ORACLE_VALUE_BOOL) return value->as.boolean ? 1 : 0;
    if (value->kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED)
        return (TZrInt64)value->as.unsignedInteger;
    return value->as.signedInteger;
}

static TZrUInt64 zr_execbc_unsigned(const SZrExecIrOracleValue *value) {
    if (value->kind == ZR_EXEC_IR_ORACLE_VALUE_BOOL) return value->as.boolean ? 1u : 0u;
    if (value->kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED)
        return (TZrUInt64)value->as.signedInteger;
    return value->as.unsignedInteger;
}

static TZrFloat64 zr_execbc_float(const SZrExecIrOracleValue *value) {
    if (value->kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT) return value->as.floating;
    if (value->kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED)
        return (TZrFloat64)value->as.unsignedInteger;
    return (TZrFloat64)zr_execbc_signed(value);
}

static TZrBool zr_execbc_operand(const SZrExecBcProjection *projection,
                                 const SZrExecBcInstruction *instruction,
                                 const SZrExecIrOracleValue *slots,
                                 TZrUInt32 index, SZrExecIrOracleValue *value) {
    TZrExecIrValueId id;
    if (index >= instruction->operands.count) return ZR_FALSE;
    id = projection->operands[instruction->operands.start + index];
    if (id == ZR_EXEC_IR_VALUE_ID_INVALID || id > projection->valueSlotCount)
        return ZR_FALSE;
    *value = slots[projection->valueSlots[id - 1u]];
    return value->kind != ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED;
}

static TZrBool zr_execbc_assign(const SZrExecBcProjection *projection,
                                const SZrExecBcInstruction *instruction,
                                SZrExecIrOracleValue *slots,
                                const SZrExecIrOracleValue *value) {
    TZrExecIrValueId id;
    if (instruction->results.count == 0u) return ZR_TRUE;
    id = projection->results[instruction->results.start];
    if (id == ZR_EXEC_IR_VALUE_ID_INVALID || id > projection->valueSlotCount)
        return ZR_FALSE;
    slots[projection->valueSlots[id - 1u]] = *value;
    return ZR_TRUE;
}

static TZrBool zr_execbc_binary(EZrExecIrOpcode opcode,
                                const SZrExecIrOracleValue *left,
                                const SZrExecIrOracleValue *right,
                                SZrExecIrOracleValue *result) {
    if (!zr_execbc_numeric(left) || !zr_execbc_numeric(right)) return ZR_FALSE;
    if (left->kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT ||
        right->kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT) {
        TZrFloat64 a = zr_execbc_float(left), b = zr_execbc_float(right);
        result->kind = ZR_EXEC_IR_ORACLE_VALUE_FLOAT;
        switch (opcode) {
            case ZR_EXEC_IR_OPCODE_ADD: case ZR_EXEC_IR_OPCODE_ARITHMETIC:
                result->as.floating = a + b; return ZR_TRUE;
            case ZR_EXEC_IR_OPCODE_SUB: result->as.floating = a - b; return ZR_TRUE;
            case ZR_EXEC_IR_OPCODE_MUL: result->as.floating = a * b; return ZR_TRUE;
            case ZR_EXEC_IR_OPCODE_DIV:
                if (b == 0.0) return ZR_FALSE;
                result->as.floating = a / b; return ZR_TRUE;
            default: return ZR_FALSE;
        }
    }
    if (left->kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED ||
        right->kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED) {
        TZrUInt64 a = zr_execbc_unsigned(left), b = zr_execbc_unsigned(right);
        result->kind = ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED;
        switch (opcode) {
            case ZR_EXEC_IR_OPCODE_ADD: case ZR_EXEC_IR_OPCODE_ARITHMETIC:
                if (a > UINT64_MAX - b) return ZR_FALSE;
                result->as.unsignedInteger = a + b; return ZR_TRUE;
            case ZR_EXEC_IR_OPCODE_SUB:
                if (a < b) return ZR_FALSE;
                result->as.unsignedInteger = a - b; return ZR_TRUE;
            case ZR_EXEC_IR_OPCODE_MUL:
                if (b != 0u && a > UINT64_MAX / b) return ZR_FALSE;
                result->as.unsignedInteger = a * b; return ZR_TRUE;
            case ZR_EXEC_IR_OPCODE_DIV:
                if (b == 0u) return ZR_FALSE;
                result->as.unsignedInteger = a / b; return ZR_TRUE;
            default: return ZR_FALSE;
        }
    }
    {
        TZrInt64 a = zr_execbc_signed(left), b = zr_execbc_signed(right);
        result->kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
        switch (opcode) {
            case ZR_EXEC_IR_OPCODE_ADD: case ZR_EXEC_IR_OPCODE_ARITHMETIC:
                if ((b > 0 && a > INT64_MAX - b) ||
                    (b < 0 && a < INT64_MIN - b))
                    return ZR_FALSE;
                result->as.signedInteger = a + b; return ZR_TRUE;
            case ZR_EXEC_IR_OPCODE_SUB:
                if ((b < 0 && a > INT64_MAX + b) ||
                    (b > 0 && a < INT64_MIN + b))
                    return ZR_FALSE;
                result->as.signedInteger = a - b; return ZR_TRUE;
            case ZR_EXEC_IR_OPCODE_MUL:
                if (a != 0 && b != 0) {
                    if ((a == -1 && b == INT64_MIN) ||
                        (b == -1 && a == INT64_MIN))
                        return ZR_FALSE;
                    if (a > 0) {
                        if (b > 0 && a > INT64_MAX / b)
                            return ZR_FALSE;
                        if (b < 0 && b < INT64_MIN / a)
                            return ZR_FALSE;
                    } else if (b > 0) {
                        if (a < INT64_MIN / b)
                            return ZR_FALSE;
                    } else if (a < INT64_MAX / b) {
                        return ZR_FALSE;
                    }
                }
                result->as.signedInteger = a * b; return ZR_TRUE;
            case ZR_EXEC_IR_OPCODE_DIV:
                if (b == 0 || (a == INT64_MIN && b == -1))
                    return ZR_FALSE;
                result->as.signedInteger = a / b; return ZR_TRUE;
            default: return ZR_FALSE;
        }
    }
}

static TZrBool zr_execbc_convert(const SZrExecBcInstruction *instruction,
                                 const SZrExecIrOracleValue *source,
                                 SZrExecIrOracleValue *result) {
    long double numeric;
    if (instruction == ZR_NULL || !zr_execbc_numeric(source) || result == ZR_NULL)
        return ZR_FALSE;
    memset(result, 0, sizeof(*result));
    if (instruction->typeToken == ZR_VALUE_TYPE_BOOL) {
        result->kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
        result->as.boolean = zr_execbc_truthy(source);
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(instruction->typeToken)) {
        result->kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
        switch (source->kind) {
            case ZR_EXEC_IR_ORACLE_VALUE_BOOL: result->as.signedInteger = source->as.boolean != 0u; return ZR_TRUE;
            case ZR_EXEC_IR_ORACLE_VALUE_SIGNED: result->as.signedInteger = source->as.signedInteger; return ZR_TRUE;
            case ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED:
                result->as.signedInteger = (TZrInt64)source->as.unsignedInteger; return ZR_TRUE;
            case ZR_EXEC_IR_ORACLE_VALUE_FLOAT:
                numeric = (long double)source->as.floating;
                if (!isfinite(numeric) || numeric < (long double)INT64_MIN ||
                    numeric >= 9223372036854775808.0L) return ZR_FALSE;
                result->as.signedInteger = (TZrInt64)numeric; return ZR_TRUE;
            default: return ZR_FALSE;
        }
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(instruction->typeToken)) {
        result->kind = ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED;
        switch (source->kind) {
            case ZR_EXEC_IR_ORACLE_VALUE_BOOL: result->as.unsignedInteger = source->as.boolean != 0u; return ZR_TRUE;
            case ZR_EXEC_IR_ORACLE_VALUE_SIGNED:
                result->as.unsignedInteger = (TZrUInt64)source->as.signedInteger; return ZR_TRUE;
            case ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED: result->as.unsignedInteger = source->as.unsignedInteger; return ZR_TRUE;
            case ZR_EXEC_IR_ORACLE_VALUE_FLOAT:
                numeric = (long double)source->as.floating;
                if (!isfinite(numeric) || numeric < 0.0L ||
                    numeric >= 18446744073709551616.0L)
                    return ZR_FALSE;
                result->as.unsignedInteger = (TZrUInt64)numeric; return ZR_TRUE;
            default: return ZR_FALSE;
        }
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(instruction->typeToken)) {
        result->kind = ZR_EXEC_IR_ORACLE_VALUE_FLOAT;
        switch (source->kind) {
            case ZR_EXEC_IR_ORACLE_VALUE_BOOL: result->as.floating = source->as.boolean != 0u; return ZR_TRUE;
            case ZR_EXEC_IR_ORACLE_VALUE_SIGNED:
            case ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED:
            case ZR_EXEC_IR_ORACLE_VALUE_FLOAT:
                result->as.floating = zr_execbc_float(source); return ZR_TRUE;
            default: return ZR_FALSE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_execbc_compare(const SZrExecBcInstruction *instruction,
                                 const SZrExecIrOracleValue *left,
                                 const SZrExecIrOracleValue *right,
                                 SZrExecIrOracleValue *result) {
    int comparison;
    if (!zr_execbc_numeric(left) || !zr_execbc_numeric(right)) return ZR_FALSE;
    if (left->kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT ||
        right->kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT) {
        TZrFloat64 a = zr_execbc_float(left), b = zr_execbc_float(right);
        comparison = a < b ? -1 : (a > b ? 1 : 0);
    } else if (left->kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED ||
               right->kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED) {
        TZrUInt64 a = zr_execbc_unsigned(left), b = zr_execbc_unsigned(right);
        comparison = a < b ? -1 : (a > b ? 1 : 0);
    } else {
        TZrInt64 a = zr_execbc_signed(left), b = zr_execbc_signed(right);
        comparison = a < b ? -1 : (a > b ? 1 : 0);
    }
    result->kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
    switch (instruction->typeToken) {
        case 1u: result->as.boolean = comparison < 0; break;
        case 2u: result->as.boolean = comparison <= 0; break;
        case 3u: result->as.boolean = comparison > 0; break;
        case 4u: result->as.boolean = comparison >= 0; break;
        case 5u: result->as.boolean = comparison != 0; break;
        default: result->as.boolean = comparison == 0; break;
    }
    return ZR_TRUE;
}

static TZrBool zr_execbc_copy_slot(void *userData, TZrUInt32 destination,
                                   TZrUInt32 source) {
    SZrExecIrOracleValue *slots = (SZrExecIrOracleValue *)userData;
    slots[destination] = slots[source];
    return ZR_TRUE;
}

TZrBool ZrParser_ExecBcProjection_Run(
        const SZrExecBcProjection *projection,
        const SZrExecBcExecutionInput *input,
        SZrExecBcExecutionResult *result,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecBcExecutionResult candidate;
    TZrExecIrBlockId block, previous = ZR_EXEC_IR_BLOCK_ID_INVALID;
    TZrExecIrBlockId selectedSuccessor = ZR_EXEC_IR_BLOCK_ID_INVALID;
    TZrUInt32 ordinal = 0u, steps = 0u;
    TZrBool terminated = ZR_FALSE;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (projection == ZR_NULL || result == ZR_NULL ||
        projection->ownershipTag != ZR_EXEC_IR_PROJECTION_TAG ||
        !projection->runnable || (input != ZR_NULL &&
         (input->initialValueCount > projection->valueSlotCount ||
          (input->initialValueCount != 0u && input->initialValues == ZR_NULL) ||
          (input->constantCount != 0u && input->constants == ZR_NULL)))) {
        zr_execbc_diag(diagnostic, projection != ZR_NULL && !projection->runnable
                       ? ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED
                       : ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                       projection, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    if ((projection->valueSlotCount != 0u && projection->valueSlots == ZR_NULL) ||
        (projection->phiMoveCount != 0u && projection->phiMoves == ZR_NULL)) {
        zr_execbc_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                       projection, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    ZrParser_ExecBcExecutionResult_Init(&candidate);
    candidate.slotCount = projection->valueSlotCount + projection->temporarySlotCount;
    if (candidate.slotCount < projection->valueSlotCount) {
        zr_execbc_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                       projection, 0u, 0u, candidate.slotCount);
        return ZR_FALSE;
    }
    if (projection->temporarySlotCount != 0u) {
        if (projection->phiTemporarySlot == UINT32_MAX ||
            projection->phiTemporarySlot + 1u < projection->phiTemporarySlot) {
            zr_execbc_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                           projection, 0u, 0u, projection->phiTemporarySlot);
            return ZR_FALSE;
        }
        if (candidate.slotCount < projection->phiTemporarySlot + 1u)
            candidate.slotCount = projection->phiTemporarySlot + 1u;
    }
    if (candidate.slotCount != 0u) {
        candidate.slots = (SZrExecIrOracleValue *)calloc(candidate.slotCount,
                                                          sizeof(*candidate.slots));
        if (candidate.slots == ZR_NULL) {
            zr_execbc_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                           projection, 0u, 0u, candidate.slotCount);
            return ZR_FALSE;
        }
    }
    for (TZrUInt32 i = 0u; i < projection->valueSlotCount; ++i) {
        if (projection->valueSlots[i] >= candidate.slotCount) {
            zr_execbc_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                           projection, 0u, 0u, projection->valueSlots[i]);
            ZrParser_ExecBcExecutionResult_Free(&candidate);
            return ZR_FALSE;
        }
    }
    if (input != ZR_NULL && input->initialValueCount != 0u) {
        for (TZrUInt32 i = 0u; i < input->initialValueCount; ++i)
            candidate.slots[projection->valueSlots[i]] = input->initialValues[i];
    }
    block = projection->blockCount == 0u ? 1u :
            (projection->entryBlockId != ZR_EXEC_IR_BLOCK_ID_INVALID
             ? projection->entryBlockId : 1u);
    while (block != ZR_EXEC_IR_BLOCK_ID_INVALID && block != 0u) {
        const SZrExecBcBlock *current;
        if (projection->blockCount != 0u && block > projection->blockCount)
            goto invalid;
        current = projection->blockCount == 0u ? ZR_NULL
                                               : &projection->blocks[block - 1u];
        if (previous != ZR_EXEC_IR_BLOCK_ID_INVALID &&
            !ZrParser_ExecBcProjection_ExecutePhiMoves(
                projection, previous, candidate.slots, candidate.slotCount,
                zr_execbc_copy_slot, candidate.slots, diagnostic))
            goto fail;
        for (TZrUInt32 index = current != ZR_NULL ? current->instructions.start : 0u;
             index < (current != ZR_NULL
                      ? current->instructions.start + current->instructions.count
                      : projection->instructionCount); ++index) {
            const SZrExecBcInstruction *instruction = &projection->instructions[index];
            SZrExecIrOracleValue left, right, value, converted;
            if (steps == UINT32_MAX ||
                (input != ZR_NULL && input->maxSteps != 0u &&
                 steps >= input->maxSteps)) goto step_limit;
            ++steps;
            ++candidate.executedInstructionCount;
            switch ((EZrExecIrOpcode)instruction->opcode) {
                case ZR_EXEC_IR_OPCODE_NOP: case ZR_EXEC_IR_OPCODE_PHI: break;
                case ZR_EXEC_IR_OPCODE_CONSTANT:
                    if (input != ZR_NULL && input->constants != ZR_NULL &&
                        instruction->layoutId < input->constantCount)
                        value = input->constants[instruction->layoutId];
                    else { value.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED; value.as.signedInteger = instruction->layoutId; }
                    if (!zr_execbc_assign(projection, instruction, candidate.slots, &value)) goto invalid;
                    break;
                case ZR_EXEC_IR_OPCODE_COPY:
                    if (!zr_execbc_operand(projection, instruction, candidate.slots, 0u, &value) ||
                        !zr_execbc_assign(projection, instruction, candidate.slots, &value)) goto invalid;
                    break;
                case ZR_EXEC_IR_OPCODE_MOVE:
                    if (!zr_execbc_operand(projection, instruction, candidate.slots, 0u, &value) ||
                        !zr_execbc_assign(projection, instruction, candidate.slots, &value)) goto invalid;
                    candidate.slots[projection->valueSlots[
                        projection->operands[instruction->operands.start] - 1u]].kind =
                            ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED;
                    break;
                case ZR_EXEC_IR_OPCODE_CONVERT:
                    if (!zr_execbc_operand(projection, instruction, candidate.slots, 0u, &value))
                        goto invalid;
                    if (ZR_VALUE_IS_TYPE_BOOL(instruction->typeToken) ||
                        ZR_VALUE_IS_TYPE_NUMBER(instruction->typeToken)) {
                        if (!zr_execbc_convert(instruction, &value, &converted))
                            goto arithmetic;
                        value = converted;
                    }
                    if (!zr_execbc_assign(projection, instruction, candidate.slots, &value))
                        goto invalid;
                    break;
                case ZR_EXEC_IR_OPCODE_ADD: case ZR_EXEC_IR_OPCODE_SUB:
                case ZR_EXEC_IR_OPCODE_MUL: case ZR_EXEC_IR_OPCODE_DIV:
                case ZR_EXEC_IR_OPCODE_ARITHMETIC:
                    if (!zr_execbc_operand(projection, instruction, candidate.slots, 0u, &left) ||
                        !zr_execbc_operand(projection, instruction, candidate.slots, 1u, &right) ||
                        !zr_execbc_binary((EZrExecIrOpcode)instruction->opcode, &left, &right, &value) ||
                        !zr_execbc_assign(projection, instruction, candidate.slots, &value)) goto arithmetic;
                    break;
                case ZR_EXEC_IR_OPCODE_NEG:
                    if (!zr_execbc_operand(projection, instruction, candidate.slots, 0u, &value) ||
                        !zr_execbc_numeric(&value)) goto arithmetic;
                    if (value.kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT)
                        value.as.floating = -value.as.floating;
                    else {
                        TZrInt64 signedValue = zr_execbc_signed(&value);
                        if (signedValue == INT64_MIN) goto arithmetic;
                        value.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
                        value.as.signedInteger = -signedValue;
                    }
                    if (!zr_execbc_assign(projection, instruction, candidate.slots, &value)) goto invalid;
                    break;
                case ZR_EXEC_IR_OPCODE_COMPARE:
                    if (!zr_execbc_operand(projection, instruction, candidate.slots, 0u, &left) ||
                        !zr_execbc_operand(projection, instruction, candidate.slots, 1u, &right) ||
                        !zr_execbc_compare(instruction, &left, &right, &value)) goto arithmetic;
                    if (!zr_execbc_assign(projection, instruction, candidate.slots, &value)) goto invalid;
                    break;
                case ZR_EXEC_IR_OPCODE_RETURN:
                    if (!zr_execbc_operand(projection, instruction, candidate.slots, 0u, &candidate.returnValue)) goto invalid;
                    candidate.returned = ZR_TRUE;
                    terminated = ZR_TRUE;
                    break;
                case ZR_EXEC_IR_OPCODE_BRANCH:
                    ordinal = 0u; terminated = ZR_TRUE;
                    goto select_successor;
                case ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH:
                    if (!zr_execbc_operand(projection, instruction, candidate.slots, 0u, &value)) goto invalid;
                    ordinal = zr_execbc_truthy(&value) ? 0u : 1u;
                    terminated = ZR_TRUE;
                    goto select_successor;
                case ZR_EXEC_IR_OPCODE_SWITCH:
                    if (!zr_execbc_operand(projection, instruction, candidate.slots, 0u, &value)) goto invalid;
                    if (instruction->successorRange.count == 0u) goto invalid;
                    ordinal = value.kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED
                            ? (TZrUInt32)value.as.unsignedInteger : (TZrUInt32)value.as.signedInteger;
                    if (ordinal >= instruction->successorRange.count) ordinal = instruction->successorRange.count - 1u;
                    terminated = ZR_TRUE;
                    goto select_successor;
                default:
                    zr_execbc_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                                   projection, block, index + 1u, instruction->opcode);
                    goto fail;
            }
            goto instruction_done;
select_successor:
            if (ordinal >= instruction->successorRange.count ||
                instruction->successorRange.start > projection->successorCount ||
                ordinal >= projection->successorCount - instruction->successorRange.start)
                goto invalid;
            selectedSuccessor = projection->successors[instruction->successorRange.start + ordinal];
instruction_done:
            if (candidate.returned) break;
            if (terminated) break;
        }
        if (candidate.returned) break;
        if (current == ZR_NULL) break;
        if (!terminated) {
            if (current->successors.count != 1u) goto invalid;
            ordinal = 0u;
            if (current->successors.start >= projection->successorCount) goto invalid;
            selectedSuccessor = projection->successors[current->successors.start];
        }
        previous = block;
        block = selectedSuccessor;
        selectedSuccessor = ZR_EXEC_IR_BLOCK_ID_INVALID;
        terminated = ZR_FALSE;
    }
    ZrParser_ExecBcExecutionResult_Free(result);
    *result = candidate;
    return ZR_TRUE;
step_limit:
    zr_execbc_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_ORACLE_STEP_LIMIT,
                   projection, block, 0u, steps);
    goto fail;
arithmetic:
    zr_execbc_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_ARITHMETIC_ERROR,
                   projection, block, 0u, 0u);
    goto fail;
invalid:
    zr_execbc_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                   projection, block, 0u, 0u);
fail:
    ZrParser_ExecBcExecutionResult_Free(&candidate);
    return ZR_FALSE;
}
