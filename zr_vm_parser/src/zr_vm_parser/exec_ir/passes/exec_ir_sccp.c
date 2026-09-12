#include "../exec_ir_pass_internal.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct SZrSccpLattice {
    EZrExecIrSccpLatticeKind kind;
    TZrUInt64 bits;
    TZrExecIrTypeToken typeToken;
} SZrSccpLattice;

static TZrBool zr_sccp_size_mul_overflow(TZrUInt64 count, size_t elementSize) {
    return (TZrBool)(elementSize != 0u &&
                     count > (TZrUInt64)(SIZE_MAX / elementSize));
}

static SZrExecIrRange zr_sccp_empty_range(void) {
    SZrExecIrRange range;
    range.offset = 0u;
    range.count = 0u;
    return range;
}

static TZrBool zr_sccp_range_valid(SZrExecIrRange range, TZrUInt32 count,
                                   const void *pool) {
    return ZrParser_ExecIr_PassRangeValid(range, count) &&
           (range.count == 0u || pool != ZR_NULL);
}

static TZrBool zr_sccp_storage_valid(const SZrExecIrFunction *function,
                                     SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    if (function == ZR_NULL ||
        function->valueCount > function->valueCapacity ||
        function->instructionCount > function->instructionCapacity ||
        function->blockCount > function->blockCapacity ||
        function->operandCount > function->operandCapacity ||
        function->resultCount > function->resultCapacity ||
        function->memoryTokenCount > function->memoryTokenCapacity ||
        function->phiCount > function->phiCapacity ||
        function->phiIncomingCount > function->phiIncomingCapacity ||
        function->predecessorCount > function->predecessorCapacity ||
        function->successorCount > function->successorCapacity ||
        function->sourceMapCount > function->sourceMapCapacity ||
        function->gcMapCount > function->gcMapCapacity ||
        (function->valueCount != 0u && function->values == ZR_NULL) ||
        (function->instructionCount != 0u && function->instructions == ZR_NULL) ||
        (function->blockCount != 0u && function->blocks == ZR_NULL) ||
        (function->operandCount != 0u && function->operands == ZR_NULL) ||
        (function->resultCount != 0u && function->results == ZR_NULL) ||
        (function->successorCount != 0u && function->successors == ZR_NULL) ||
        (function->predecessorCount != 0u && function->predecessors == ZR_NULL) ||
        (function->phiCount != 0u && function->phiPool == ZR_NULL) ||
        (function->phiIncomingCount != 0u && function->phiIncoming == ZR_NULL)) {
        ZrParser_ExecIr_PassDiagnostic(diagnostic,
                                       ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                       function, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        if (block->id != index + 1u) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                           function, block->id, 0u, index + 1u, block->id);
            return ZR_FALSE;
        }
    }
    if (function->blockCount != 0u &&
        (function->entryBlockId == ZR_EXEC_IR_BLOCK_ID_INVALID ||
         function->entryBlockId > function->blockCount)) {
        ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                       function, 0u, 0u, ZR_EXEC_IR_BLOCK_ID_ENTRY,
                                       function->entryBlockId);
        return ZR_FALSE;
    }
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(instruction->opcode);
        if (info == ZR_NULL) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE,
                                           function, 0u, index + 1u,
                                           ZR_EXEC_IR_OPCODE_COUNT - 1u, instruction->opcode);
            return ZR_FALSE;
        }
        if (!zr_sccp_range_valid(instruction->operands, function->operandCount,
                                 function->operands) ||
            !zr_sccp_range_valid(instruction->results, function->resultCount,
                                 function->results) ||
            !zr_sccp_range_valid(instruction->successorRange,
                                 function->successorCount, function->successors) ||
            !zr_sccp_range_valid(instruction->phiRange,
                                 function->phiIncomingCount, function->phiIncoming)) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                           function, 0u, index + 1u, 0u, 0u);
            return ZR_FALSE;
        }
        if (instruction->operands.count < info->minimumOperands ||
            instruction->operands.count > info->maximumOperands ||
            instruction->results.count != info->resultArity) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                           function, 0u, index + 1u,
                                           info->minimumOperands, instruction->operands.count);
            return ZR_FALSE;
        }
        for (TZrUInt32 at = instruction->operands.start;
             at < instruction->operands.start + instruction->operands.count; ++at) {
            if (function->operands[at] == ZR_EXEC_IR_VALUE_ID_INVALID ||
                function->operands[at] > function->valueCount) {
                ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                               function, 0u, index + 1u,
                                               function->valueCount, function->operands[at]);
                return ZR_FALSE;
            }
        }
        for (TZrUInt32 at = instruction->results.start;
             at < instruction->results.start + instruction->results.count; ++at) {
            if (function->results[at] == ZR_EXEC_IR_VALUE_ID_INVALID ||
                function->results[at] > function->valueCount) {
                ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                               function, 0u, index + 1u,
                                               function->valueCount, function->results[at]);
                return ZR_FALSE;
            }
        }
    }
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        if (!zr_sccp_range_valid(block->instructions, function->instructionCount,
                                 function->instructions) ||
            !zr_sccp_range_valid(block->predecessors, function->predecessorCount,
                                 function->predecessors) ||
            !zr_sccp_range_valid(block->successors, function->successorCount,
                                 function->successors) ||
            !zr_sccp_range_valid(block->phis, function->phiCount,
                                 function->phiPool)) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                           function, block->id, 0u, 0u, 0u);
            return ZR_FALSE;
        }
        for (TZrUInt32 at = block->successors.start;
             at < block->successors.start + block->successors.count; ++at) {
            if (function->successors[at] == ZR_EXEC_IR_BLOCK_ID_INVALID ||
                function->successors[at] > function->blockCount) {
                ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                               function, block->id, 0u,
                                               function->blockCount, function->successors[at]);
                return ZR_FALSE;
            }
        }
    }
    for (index = 0u; index < function->phiCount; ++index) {
        if (function->phiPool[index].result == ZR_EXEC_IR_VALUE_ID_INVALID ||
            function->phiPool[index].result > function->valueCount ||
            !zr_sccp_range_valid(function->phiPool[index].incomings,
                                 function->phiIncomingCount, function->phiIncoming)) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                           function, 0u, 0u, 0u, 0u);
            return ZR_FALSE;
        }
        for (TZrUInt32 at = function->phiPool[index].incomings.start;
             at < function->phiPool[index].incomings.start +
                 function->phiPool[index].incomings.count; ++at) {
            const SZrExecIrPhiIncoming *incoming = &function->phiIncoming[at];
            if (incoming->predecessor == ZR_EXEC_IR_BLOCK_ID_INVALID ||
                incoming->predecessor > function->blockCount ||
                incoming->value == ZR_EXEC_IR_VALUE_ID_INVALID ||
                incoming->value > function->valueCount) {
                ZrParser_ExecIr_PassDiagnostic(diagnostic,
                                               ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                               function, incoming->predecessor, 0u,
                                               function->valueCount, incoming->value);
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

static SZrSccpLattice zr_sccp_unknown(void) {
    SZrSccpLattice value;
    value.kind = ZR_EXEC_IR_SCCP_UNKNOWN;
    value.bits = 0u;
    value.typeToken = 0u;
    return value;
}

static SZrSccpLattice zr_sccp_overdefined(TZrExecIrTypeToken typeToken) {
    SZrSccpLattice value;
    value.kind = ZR_EXEC_IR_SCCP_OVERDEFINED;
    value.bits = 0u;
    value.typeToken = typeToken;
    return value;
}

static SZrSccpLattice zr_sccp_must_throw(TZrExecIrTypeToken typeToken) {
    SZrSccpLattice value;
    value.kind = ZR_EXEC_IR_SCCP_MUST_THROW;
    value.bits = 0u;
    value.typeToken = typeToken;
    return value;
}

static SZrSccpLattice zr_sccp_constant(TZrUInt64 bits,
                                       TZrExecIrTypeToken typeToken) {
    SZrSccpLattice value;
    value.kind = ZR_EXEC_IR_SCCP_CONSTANT;
    value.bits = bits;
    value.typeToken = typeToken;
    return value;
}

static TZrBool zr_sccp_equal(SZrSccpLattice left, SZrSccpLattice right) {
    return (TZrBool)(left.kind == right.kind &&
                     (left.kind != ZR_EXEC_IR_SCCP_CONSTANT ||
                      (left.bits == right.bits && left.typeToken == right.typeToken)));
}

static SZrSccpLattice zr_sccp_join(SZrSccpLattice left, SZrSccpLattice right) {
    if (left.kind == ZR_EXEC_IR_SCCP_MUST_THROW &&
        right.kind == ZR_EXEC_IR_SCCP_MUST_THROW)
        return zr_sccp_must_throw(left.typeToken != 0u ? left.typeToken : right.typeToken);
    if (left.kind == ZR_EXEC_IR_SCCP_MUST_THROW) {
        if (right.kind == ZR_EXEC_IR_SCCP_UNKNOWN)
            return left;
        return zr_sccp_overdefined(left.typeToken != 0u ? left.typeToken : right.typeToken);
    }
    if (right.kind == ZR_EXEC_IR_SCCP_MUST_THROW) {
        if (left.kind == ZR_EXEC_IR_SCCP_UNKNOWN)
            return right;
        return zr_sccp_overdefined(left.typeToken != 0u ? left.typeToken : right.typeToken);
    }
    if (left.kind == ZR_EXEC_IR_SCCP_OVERDEFINED ||
        right.kind == ZR_EXEC_IR_SCCP_OVERDEFINED) {
        return zr_sccp_overdefined(left.typeToken != 0u ? left.typeToken : right.typeToken);
    }
    if (left.kind == ZR_EXEC_IR_SCCP_UNKNOWN) return right;
    if (right.kind == ZR_EXEC_IR_SCCP_UNKNOWN) return left;
    if (left.bits == right.bits && left.typeToken == right.typeToken) return left;
    return zr_sccp_overdefined(left.typeToken != 0u ? left.typeToken : right.typeToken);
}

static TZrBool zr_sccp_signed(TZrUInt64 bits, TZrInt64 *value) {
    if (value == ZR_NULL) return ZR_FALSE;
    *value = (TZrInt64)bits;
    return ZR_TRUE;
}

static TZrBool zr_sccp_add(TZrInt64 left, TZrInt64 right, TZrInt64 *out) {
    if ((right > 0 && left > INT64_MAX - right) ||
        (right < 0 && left < INT64_MIN - right)) return ZR_FALSE;
    *out = left + right;
    return ZR_TRUE;
}

static TZrBool zr_sccp_sub(TZrInt64 left, TZrInt64 right, TZrInt64 *out) {
    if ((right < 0 && left > INT64_MAX + right) ||
        (right > 0 && left < INT64_MIN + right)) return ZR_FALSE;
    *out = left - right;
    return ZR_TRUE;
}

static TZrBool zr_sccp_mul(TZrInt64 left, TZrInt64 right, TZrInt64 *out) {
    if (left != 0 && right != 0 &&
        ((left == INT64_MIN && right == -1) ||
         (right == INT64_MIN && left == -1) ||
         (left > 0 && right > 0 && left > INT64_MAX / right) ||
         (left > 0 && right < 0 && right < INT64_MIN / left) ||
         (left < 0 && right > 0 && left < INT64_MIN / right) ||
         (left < 0 && right < 0 && left < INT64_MAX / right))) return ZR_FALSE;
    *out = left * right;
    return ZR_TRUE;
}

static TZrBool zr_sccp_div(TZrInt64 left, TZrInt64 right, TZrInt64 *out) {
    if (right == 0 || (left == INT64_MIN && right == -1)) return ZR_FALSE;
    *out = left / right;
    return ZR_TRUE;
}

static TZrBool zr_sccp_operand(const SZrExecIrFunction *function,
                               const SZrSccpLattice *lattice,
                               const SZrExecIrInstruction *instruction,
                               TZrUInt32 operandIndex,
                               SZrSccpLattice *result) {
    TZrExecIrValueId valueId;
    if (result == ZR_NULL || operandIndex >= instruction->operands.count) return ZR_FALSE;
    valueId = function->operands[instruction->operands.start + operandIndex];
    if (valueId == ZR_EXEC_IR_VALUE_ID_INVALID || valueId > function->valueCount) return ZR_FALSE;
    *result = lattice[valueId];
    return ZR_TRUE;
}

static SZrSccpLattice zr_sccp_instruction_value(
        const SZrExecIrFunction *function, const SZrExecIrPassContext *context,
        const SZrSccpLattice *lattice,
        const SZrExecIrInstruction *instruction, TZrExecIrTypeToken typeToken) {
    SZrSccpLattice left, right;
    TZrInt64 signedLeft, signedRight, output;
    EZrExecIrOpcode opcode = (EZrExecIrOpcode)instruction->opcode;

    switch (opcode) {
        case ZR_EXEC_IR_OPCODE_CONSTANT:
            if (context != ZR_NULL && context->constants != ZR_NULL &&
                instruction->layoutId < context->constantCount) {
                const SZrExecIrConstant *constant = &context->constants[instruction->layoutId];
                return zr_sccp_constant(constant->bits,
                                        constant->typeToken != 0u ? constant->typeToken : typeToken);
            }
            return zr_sccp_constant((TZrUInt64)(TZrInt64)instruction->layoutId,
                                    instruction->typeToken != 0u ? instruction->typeToken : typeToken);
        case ZR_EXEC_IR_OPCODE_COPY:
        case ZR_EXEC_IR_OPCODE_MOVE:
        case ZR_EXEC_IR_OPCODE_CONVERT:
            if (!zr_sccp_operand(function, lattice, instruction, 0u, &left))
                return zr_sccp_overdefined(typeToken);
            if (left.kind == ZR_EXEC_IR_SCCP_CONSTANT)
                return zr_sccp_constant(left.bits, typeToken != 0u ? typeToken : left.typeToken);
            return left.kind == ZR_EXEC_IR_SCCP_UNKNOWN ? left : zr_sccp_overdefined(typeToken);
        case ZR_EXEC_IR_OPCODE_ADD:
        case ZR_EXEC_IR_OPCODE_ARITHMETIC:
        case ZR_EXEC_IR_OPCODE_SUB:
        case ZR_EXEC_IR_OPCODE_MUL:
        case ZR_EXEC_IR_OPCODE_DIV:
            if (!zr_sccp_operand(function, lattice, instruction, 0u, &left) ||
                !zr_sccp_operand(function, lattice, instruction, 1u, &right))
                return zr_sccp_overdefined(typeToken);
            if (left.kind == ZR_EXEC_IR_SCCP_MUST_THROW ||
                right.kind == ZR_EXEC_IR_SCCP_MUST_THROW)
                return zr_sccp_must_throw(typeToken);
            if (left.kind == ZR_EXEC_IR_SCCP_UNKNOWN || right.kind == ZR_EXEC_IR_SCCP_UNKNOWN)
                return zr_sccp_unknown();
            if (left.kind != ZR_EXEC_IR_SCCP_CONSTANT ||
                right.kind != ZR_EXEC_IR_SCCP_CONSTANT ||
                !zr_sccp_signed(left.bits, &signedLeft) ||
                !zr_sccp_signed(right.bits, &signedRight))
                return zr_sccp_overdefined(typeToken);
            switch (opcode) {
                case ZR_EXEC_IR_OPCODE_ADD:
                case ZR_EXEC_IR_OPCODE_ARITHMETIC:
                    if (!zr_sccp_add(signedLeft, signedRight, &output))
                        return zr_sccp_must_throw(typeToken);
                    break;
                case ZR_EXEC_IR_OPCODE_SUB:
                    if (!zr_sccp_sub(signedLeft, signedRight, &output))
                        return zr_sccp_must_throw(typeToken);
                    break;
                case ZR_EXEC_IR_OPCODE_MUL:
                    if (!zr_sccp_mul(signedLeft, signedRight, &output))
                        return zr_sccp_must_throw(typeToken);
                    break;
                case ZR_EXEC_IR_OPCODE_DIV:
                    if (!zr_sccp_div(signedLeft, signedRight, &output))
                        return zr_sccp_must_throw(typeToken);
                    break;
                default:
                    return zr_sccp_overdefined(typeToken);
            }
            return zr_sccp_constant((TZrUInt64)output, typeToken);
        case ZR_EXEC_IR_OPCODE_NEG:
            if (!zr_sccp_operand(function, lattice, instruction, 0u, &left))
                return zr_sccp_overdefined(typeToken);
            if (left.kind == ZR_EXEC_IR_SCCP_UNKNOWN) return zr_sccp_unknown();
            if (left.kind == ZR_EXEC_IR_SCCP_MUST_THROW) return left;
            if (left.kind != ZR_EXEC_IR_SCCP_CONSTANT ||
                !zr_sccp_signed(left.bits, &signedLeft) || signedLeft == INT64_MIN)
                return zr_sccp_overdefined(typeToken);
            return zr_sccp_constant((TZrUInt64)(-signedLeft), typeToken);
        case ZR_EXEC_IR_OPCODE_COMPARE: {
            int comparison;
            if (!zr_sccp_operand(function, lattice, instruction, 0u, &left) ||
                !zr_sccp_operand(function, lattice, instruction, 1u, &right))
                return zr_sccp_overdefined(typeToken);
            if (left.kind == ZR_EXEC_IR_SCCP_UNKNOWN || right.kind == ZR_EXEC_IR_SCCP_UNKNOWN)
                return zr_sccp_unknown();
            if (left.kind == ZR_EXEC_IR_SCCP_MUST_THROW ||
                right.kind == ZR_EXEC_IR_SCCP_MUST_THROW)
                return zr_sccp_must_throw(typeToken);
            if (left.kind != ZR_EXEC_IR_SCCP_CONSTANT || right.kind != ZR_EXEC_IR_SCCP_CONSTANT ||
                !zr_sccp_signed(left.bits, &signedLeft) || !zr_sccp_signed(right.bits, &signedRight))
                return zr_sccp_overdefined(typeToken);
            comparison = signedLeft < signedRight ? -1 : (signedLeft > signedRight ? 1 : 0);
            switch (instruction->typeToken) {
                case 1u: comparison = comparison < 0; break;
                case 2u: comparison = comparison <= 0; break;
                case 3u: comparison = comparison > 0; break;
                case 4u: comparison = comparison >= 0; break;
                case 5u: comparison = comparison != 0; break;
                default: comparison = comparison == 0; break;
            }
            return zr_sccp_constant((TZrUInt64)(comparison != 0), typeToken);
        }
        default:
            return instruction->results.count == 0u
                       ? zr_sccp_unknown() : zr_sccp_overdefined(typeToken);
    }
}

static TZrBool zr_sccp_edge_is_executable(const SZrExecIrFunction *function,
                                          TZrUInt32 *edgeExecutable,
                                          TZrExecIrBlockId predecessor,
                                          TZrExecIrBlockId successor) {
    const SZrExecIrBlock *block;
    TZrUInt32 index;
    if (predecessor == ZR_EXEC_IR_BLOCK_ID_INVALID || predecessor > function->blockCount)
        return ZR_FALSE;
    block = &function->blocks[predecessor - 1u];
    for (index = block->successors.start;
         index < block->successors.start + block->successors.count; ++index) {
        if (function->successors[index] == successor && edgeExecutable[index] != 0u)
            return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool zr_sccp_mark_edge(const SZrExecIrFunction *function,
                                 TZrUInt32 *edgeExecutable,
                                 TZrExecIrBlockId predecessor,
                                 TZrUInt32 edgeIndex,
                                 TZrExecIrBlockId successor,
                                 TZrUInt8 *blockExecutable) {
    TZrBool changed = ZR_FALSE;
    if (edgeIndex >= function->successorCount || successor == ZR_EXEC_IR_BLOCK_ID_INVALID ||
        successor > function->blockCount) return ZR_FALSE;
    if (edgeExecutable[edgeIndex] == 0u) {
        edgeExecutable[edgeIndex] = 1u;
        changed = ZR_TRUE;
    }
    if (blockExecutable[successor] == 0u) {
        blockExecutable[successor] = 1u;
        changed = ZR_TRUE;
    }
    (void)predecessor;
    return changed;
}

static void zr_sccp_mark_successors(const SZrExecIrFunction *function,
                                    const SZrSccpLattice *lattice,
                                    const SZrExecIrInstruction *instruction,
                                    TZrExecIrBlockId blockId,
                                    TZrUInt32 *edgeExecutable,
                                    TZrUInt8 *blockExecutable,
                                    TZrBool *changed) {
    TZrUInt32 index;
    TZrUInt32 first = instruction->successorRange.start;
    TZrUInt32 count = instruction->successorRange.count;
    TZrBool conditional = (TZrBool)(instruction->opcode == ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH);
    TZrBool known = ZR_FALSE;
    TZrBool takeTrue = ZR_FALSE;
    if (conditional && instruction->operands.count != 0u) {
        TZrExecIrValueId condition = function->operands[instruction->operands.start];
        if (condition != ZR_EXEC_IR_VALUE_ID_INVALID && condition <= function->valueCount &&
            lattice[condition].kind == ZR_EXEC_IR_SCCP_CONSTANT) {
            known = ZR_TRUE;
            takeTrue = lattice[condition].bits != 0u;
        } else if (condition != ZR_EXEC_IR_VALUE_ID_INVALID && condition <= function->valueCount &&
                   lattice[condition].kind == ZR_EXEC_IR_SCCP_MUST_THROW) {
            /* There is no normal successor after a definition that is proven
             * to throw.  Exception-edge construction remains the job of the
             * effect/CFG builder. */
            return;
        }
    }
    for (index = 0u; index < count; ++index) {
        if (!conditional || !known || (takeTrue && index == 0u) || (!takeTrue && index == 1u)) {
            TZrExecIrBlockId successor = function->successors[first + index];
            TZrUInt32 edgeIndex = first + index;
            const SZrExecIrBlock *block = &function->blocks[blockId - 1u];
            TZrBool found = ZR_FALSE;
            for (TZrUInt32 edge = block->successors.start;
                 edge < block->successors.start + block->successors.count; ++edge) {
                if (function->successors[edge] == successor) {
                    edgeIndex = edge;
                    found = ZR_TRUE;
                    break;
                }
            }
            if (found && zr_sccp_mark_edge(function, edgeExecutable, blockId, edgeIndex,
                                           successor, blockExecutable)) *changed = ZR_TRUE;
        }
    }
}

static TZrBool zr_sccp_alloc_arrays(const SZrExecIrFunction *function,
                                    SZrSccpLattice **lattice,
                                    TZrUInt8 **blocks,
                                    TZrUInt32 **edges) {
    size_t valueBytes, blockBytes, edgeBytes;
    if (zr_sccp_size_mul_overflow((TZrUInt64)function->valueCount + 1u,
                                  sizeof(**lattice)) ||
        zr_sccp_size_mul_overflow((TZrUInt64)function->blockCount + 1u,
                                  sizeof(**blocks)) ||
        zr_sccp_size_mul_overflow((TZrUInt64)function->successorCount,
                                  sizeof(**edges))) return ZR_FALSE;
    valueBytes = (size_t)((TZrUInt64)function->valueCount + 1u) * sizeof(**lattice);
    blockBytes = (size_t)((TZrUInt64)function->blockCount + 1u) * sizeof(**blocks);
    edgeBytes = (size_t)function->successorCount * sizeof(**edges);
    *lattice = (SZrSccpLattice *)calloc(1u, valueBytes);
    *blocks = (TZrUInt8 *)calloc(1u, blockBytes);
    *edges = edgeBytes == 0u ? ZR_NULL : (TZrUInt32 *)calloc(1u, edgeBytes);
    if (*lattice == ZR_NULL || *blocks == ZR_NULL || (edgeBytes != 0u && *edges == ZR_NULL)) {
        free(*lattice);
        free(*blocks);
        free(*edges);
        *lattice = ZR_NULL;
        *blocks = ZR_NULL;
        *edges = ZR_NULL;
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool zr_sccp_compute(SZrExecIrFunction *function,
                               SZrExecIrPassContext *context,
                               SZrSccpLattice *lattice,
                               TZrUInt8 *blockExecutable,
                               TZrUInt32 *edgeExecutable,
                               SZrExecIrDiagnostic *diagnostic) {
    TZrUInt64 rounds, round;
    TZrUInt32 index;
    TZrBool changed;
    if (function->blockCount == 0u) {
        for (index = 0u; index <= function->valueCount; ++index)
            lattice[index] = zr_sccp_unknown();
    } else {
        if (function->entryBlockId == ZR_EXEC_IR_BLOCK_ID_INVALID ||
            function->entryBlockId > function->blockCount) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                           function, 0u, 0u, ZR_EXEC_IR_BLOCK_ID_ENTRY,
                                           function->entryBlockId);
            return ZR_FALSE;
        }
        blockExecutable[function->entryBlockId] = 1u;
    }
    rounds = (TZrUInt64)function->valueCount + (TZrUInt64)function->instructionCount +
             (TZrUInt64)function->blockCount + (TZrUInt64)function->successorCount + 1u;
    if (rounds > UINT32_MAX) rounds = UINT32_MAX;
    for (round = 0u; round < rounds; ++round) {
        changed = ZR_FALSE;
        if (!ZrParser_ExecIr_PassConsumeBudget(function, context,
                                                (TZrUInt64)function->instructionCount + 1u))
            return ZR_TRUE;
        if (function->blockCount == 0u) {
            for (index = 0u; index < function->instructionCount; ++index) {
                SZrExecIrInstruction *instruction = &function->instructions[index];
                TZrExecIrValueId valueId;
                SZrSccpLattice next;
                if (instruction->results.count == 0u) continue;
                valueId = function->results[instruction->results.start];
                if (valueId == ZR_EXEC_IR_VALUE_ID_INVALID || valueId > function->valueCount) {
                    ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                                   function, 0u, index + 1u,
                                                   function->valueCount, valueId);
                    return ZR_FALSE;
                }
                next = zr_sccp_instruction_value(function, context, lattice, instruction,
                                                 function->values[valueId - 1u].typeToken);
                if (context != ZR_NULL && next.kind == ZR_EXEC_IR_SCCP_MUST_THROW)
                    context->lastReasonCode = ZR_EXEC_IR_PASS_REASON_WILL_THROW;
                if (!zr_sccp_equal(lattice[valueId], next)) {
                    lattice[valueId] = zr_sccp_join(lattice[valueId], next);
                    changed = ZR_TRUE;
                }
            }
        } else {
            for (index = 0u; index < function->blockCount; ++index) {
                const SZrExecIrBlock *block = &function->blocks[index];
                TZrUInt32 phiIndex, instructionIndex;
                if (blockExecutable[block->id] == 0u) continue;
                for (phiIndex = block->phis.start;
                     phiIndex < block->phis.start + block->phis.count; ++phiIndex) {
                    const SZrExecIrPhi *phi = &function->phiPool[phiIndex];
                    SZrSccpLattice merged = zr_sccp_unknown();
                    TZrUInt32 incomingIndex;
                    if (!ZrParser_ExecIr_PassConsumeBudget(function, context, 1u))
                        return ZR_TRUE;
                    for (incomingIndex = 0u;
                         incomingIndex < phi->incomings.count; ++incomingIndex) {
                        const SZrExecIrPhiIncoming *incoming =
                                &function->phiIncoming[phi->incomings.start + incomingIndex];
                        TZrExecIrBlockId predecessor = incoming->predecessor;
                        if (zr_sccp_edge_is_executable(function, edgeExecutable, predecessor,
                                                       block->id) &&
                            incoming->value != ZR_EXEC_IR_VALUE_ID_INVALID &&
                            incoming->value <= function->valueCount)
                            merged = zr_sccp_join(merged, lattice[incoming->value]);
                    }
                    if (phi->result == ZR_EXEC_IR_VALUE_ID_INVALID ||
                        phi->result > function->valueCount) {
                        ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                                       function, block->id, 0u,
                                                       function->valueCount, phi->result);
                        return ZR_FALSE;
                    }
                    if (!zr_sccp_equal(lattice[phi->result], merged)) {
                        lattice[phi->result] = zr_sccp_join(lattice[phi->result], merged);
                        changed = ZR_TRUE;
                    }
                }
                for (instructionIndex = block->instructions.start;
                     instructionIndex < block->instructions.start + block->instructions.count;
                     ++instructionIndex) {
                    SZrExecIrInstruction *instruction = &function->instructions[instructionIndex];
                    TZrExecIrValueId valueId;
                    SZrSccpLattice next;
                    if (instruction->results.count != 0u) {
                        valueId = function->results[instruction->results.start];
                        if (valueId == ZR_EXEC_IR_VALUE_ID_INVALID || valueId > function->valueCount) {
                            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                                           function, block->id, instructionIndex + 1u,
                                                           function->valueCount, valueId);
                            return ZR_FALSE;
                        }
                        next = zr_sccp_instruction_value(function, context, lattice, instruction,
                                                         function->values[valueId - 1u].typeToken);
                        if (context != ZR_NULL && next.kind == ZR_EXEC_IR_SCCP_MUST_THROW)
                            context->lastReasonCode = ZR_EXEC_IR_PASS_REASON_WILL_THROW;
                        if (!zr_sccp_equal(lattice[valueId], next)) {
                            lattice[valueId] = zr_sccp_join(lattice[valueId], next);
                            changed = ZR_TRUE;
                        }
                    }
                    if (instructionIndex + 1u == block->instructions.start + block->instructions.count)
                        zr_sccp_mark_successors(function, lattice, instruction, block->id,
                                                edgeExecutable, blockExecutable, &changed);
                }
            }
        }
        if (!changed) break;
    }
    return ZR_TRUE;
}

static TZrExecIrBlockId zr_sccp_containing_block(const SZrExecIrFunction *function,
                                                 TZrUInt32 instructionIndex) {
    TZrUInt32 index;
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        if (instructionIndex >= block->instructions.start &&
            instructionIndex - block->instructions.start < block->instructions.count)
            return block->id;
    }
    return ZR_EXEC_IR_BLOCK_ID_INVALID;
}

static TZrBool zr_sccp_block_dominates(const SZrExecIrFunction *function,
                                       TZrExecIrBlockId definition,
                                       TZrExecIrBlockId use) {
    TZrUInt32 steps = 0u;
    if (definition == ZR_EXEC_IR_BLOCK_ID_INVALID || use == ZR_EXEC_IR_BLOCK_ID_INVALID ||
        definition > function->blockCount || use > function->blockCount) return ZR_FALSE;
    while (use != ZR_EXEC_IR_BLOCK_ID_INVALID && steps++ <= function->blockCount) {
        if (use == definition) return ZR_TRUE;
        use = function->blocks[use - 1u].immediateDominator;
    }
    return ZR_FALSE;
}

static TZrExecIrValueId zr_sccp_alias_for_use(const SZrExecIrFunction *function,
                                              const TZrExecIrValueId *aliases,
                                              const TZrUInt32 *aliasDefinitions,
                                              const TZrExecIrBlockId *aliasBlocks,
                                              TZrExecIrValueId value,
                                              TZrUInt32 useInstructionIndex) {
    TZrUInt32 steps = 0u;
    TZrExecIrBlockId useBlock = zr_sccp_containing_block(function, useInstructionIndex);
    while (value != ZR_EXEC_IR_VALUE_ID_INVALID && value <= function->valueCount &&
           aliases[value] != ZR_EXEC_IR_VALUE_ID_INVALID && aliases[value] != value &&
           steps++ <= function->valueCount) {
        TZrUInt32 definition = aliasDefinitions[value];
        if (definition == 0u) break;
        if (function->blockCount == 0u) {
            if (definition >= useInstructionIndex + 1u) break;
        } else {
            TZrExecIrBlockId definitionBlock = aliasBlocks[value];
            if (definitionBlock == useBlock) {
                if (definition >= useInstructionIndex + 1u) break;
            } else if (!zr_sccp_block_dominates(function, definitionBlock, useBlock)) break;
        }
        value = aliases[value];
    }
    return value;
}

static void zr_sccp_set_changed(SZrExecIrPassContext *context,
                                TZrBool *changed,
                                TZrExecIrSourceId sourceId) {
    if (changed != ZR_NULL && !*changed) *changed = ZR_TRUE;
    if (context != ZR_NULL && context->lastSourceId == 0u) context->lastSourceId = sourceId;
}

static TZrBool zr_sccp_rewrite(SZrExecIrFunction *function,
                               SZrExecIrPassContext *context,
                               const SZrSccpLattice *lattice,
                               TZrBool *changed,
                               SZrExecIrDiagnostic *diagnostic) {
    TZrExecIrValueId *aliases;
    TZrUInt32 *aliasDefinitions;
    TZrExecIrBlockId *aliasBlocks;
    TZrUInt32 index;
    size_t bytes;
    if (function->valueCount == UINT32_MAX ||
        zr_sccp_size_mul_overflow((TZrUInt64)function->valueCount + 1u,
                                  sizeof(*aliases))) {
        ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                       function, 0u, 0u, UINT32_MAX, function->valueCount);
        return ZR_FALSE;
    }
    bytes = (size_t)((TZrUInt64)function->valueCount + 1u) * sizeof(*aliases);
    aliases = (TZrExecIrValueId *)calloc(1u, bytes);
    aliasDefinitions = (TZrUInt32 *)calloc(1u, bytes);
    aliasBlocks = (TZrExecIrBlockId *)calloc(1u, bytes);
    if (aliases == ZR_NULL || aliasDefinitions == ZR_NULL || aliasBlocks == ZR_NULL) {
        free(aliases);
        free(aliasDefinitions);
        free(aliasBlocks);
        ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                       function, 0u, 0u, function->valueCount, 0u);
        return ZR_FALSE;
    }
    for (index = 1u; index <= function->valueCount; ++index) aliases[index] = index;
    for (index = 0u; index < function->instructionCount; ++index) {
        SZrExecIrInstruction *instruction = &function->instructions[index];
        if (instruction->opcode == ZR_EXEC_IR_OPCODE_COPY &&
            instruction->operands.count == 1u && instruction->results.count == 1u) {
            TZrExecIrValueId source = function->operands[instruction->operands.start];
            TZrExecIrValueId destination = function->results[instruction->results.start];
            if (source != ZR_EXEC_IR_VALUE_ID_INVALID && source <= function->valueCount &&
                destination != ZR_EXEC_IR_VALUE_ID_INVALID && destination <= function->valueCount)
                if (instruction->flags == 0u &&
                    instruction->effectIn == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID &&
                    instruction->effectOut == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID &&
                    instruction->memoryIn.count == 0u && instruction->memoryOut.count == 0u) {
                    aliases[destination] = source;
                    aliasDefinitions[destination] = index + 1u;
                    aliasBlocks[destination] = zr_sccp_containing_block(function, index);
                }
        }
    }
    for (index = 0u; index < function->instructionCount; ++index) {
        SZrExecIrInstruction *instruction = &function->instructions[index];
        TZrUInt32 operandIndex;
        for (operandIndex = instruction->operands.start;
             operandIndex < instruction->operands.start + instruction->operands.count;
             ++operandIndex) {
            TZrExecIrValueId oldValue = function->operands[operandIndex];
            TZrExecIrValueId newValue = zr_sccp_alias_for_use(function, aliases,
                                                              aliasDefinitions, aliasBlocks,
                                                              oldValue, index);
            if (newValue != oldValue) {
                function->operands[operandIndex] = newValue;
                zr_sccp_set_changed(context, changed, instruction->sourceId);
            }
        }
    }
    /* Phi incoming values are edge uses.  Without a per-edge dominance
     * context, retaining their original IDs is the conservative choice. */
    for (index = 0u; index < function->instructionCount; ++index) {
        SZrExecIrInstruction *instruction = &function->instructions[index];
        TZrExecIrValueId result;
        if (instruction->results.count != 1u) continue;
        result = function->results[instruction->results.start];
        if (result == ZR_EXEC_IR_VALUE_ID_INVALID || result > function->valueCount ||
            lattice[result].kind != ZR_EXEC_IR_SCCP_CONSTANT || lattice[result].bits > UINT32_MAX)
            continue;
        /* layoutId is an index when a module constant pool is present.  A
         * folded immediate cannot be encoded in that same field without
         * risking an accidental lookup of a different pool entry; retain the
         * original operation until a module-aware constant materializer is
         * available.  Copy/alias propagation remains safe in this mode. */
        if (context != ZR_NULL && context->constantCount != 0u) continue;
        if (instruction->opcode != ZR_EXEC_IR_OPCODE_ADD &&
            instruction->opcode != ZR_EXEC_IR_OPCODE_ARITHMETIC &&
            instruction->opcode != ZR_EXEC_IR_OPCODE_SUB &&
            instruction->opcode != ZR_EXEC_IR_OPCODE_MUL &&
            instruction->opcode != ZR_EXEC_IR_OPCODE_DIV &&
            instruction->opcode != ZR_EXEC_IR_OPCODE_NEG &&
            instruction->opcode != ZR_EXEC_IR_OPCODE_COMPARE &&
            instruction->opcode != ZR_EXEC_IR_OPCODE_CONVERT) continue;
        /* A proven constant transfer is only materialized when the operation
         * has no independent observable boundary.  DIV is safe here because
         * the evaluator returns OVERDEFINED for zero/overflow; an unknown
         * call/GC/throw flag remains in the original instruction. */
        if ((instruction->flags & (ZR_EXEC_IR_FLAG_MAY_ALLOCATE |
                                   ZR_EXEC_IR_FLAG_MAY_GC |
                                   ZR_EXEC_IR_FLAG_MAY_SUSPEND |
                                   ZR_EXEC_IR_FLAG_DEBUG_POLL |
                                   ZR_EXEC_IR_FLAG_GUARD_EXIT)) != 0u ||
            (instruction->opcode != ZR_EXEC_IR_OPCODE_DIV &&
             (instruction->flags & ZR_EXEC_IR_FLAG_MAY_THROW) != 0u)) continue;
        if (instruction->effectIn != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID ||
            instruction->effectOut != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID ||
            instruction->memoryIn.count != 0u || instruction->memoryOut.count != 0u)
            continue;
        if (!ZrParser_ExecIr_PassConsumeBudget(function, context, 1u)) break;
        instruction->opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_CONSTANT;
        instruction->flags = 0u;
        instruction->operands = zr_sccp_empty_range();
        instruction->phiRange = zr_sccp_empty_range();
        instruction->successorRange = zr_sccp_empty_range();
        instruction->memoryIn = zr_sccp_empty_range();
        instruction->memoryOut = zr_sccp_empty_range();
        instruction->effectIn = ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID;
        instruction->effectOut = ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID;
        instruction->layoutId = (TZrUInt32)lattice[result].bits;
        zr_sccp_set_changed(context, changed, instruction->sourceId);
    }
    free(aliases);
    free(aliasDefinitions);
    free(aliasBlocks);
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_ComputeSccp(SZrExecIrFunction *function,
                                    SZrExecIrPassContext *context,
                                    TZrBool rewrite,
                                    TZrBool *changed,
                                    SZrExecIrDiagnostic *diagnostic) {
    SZrSccpLattice *lattice = ZR_NULL;
    TZrUInt8 *blockExecutable = ZR_NULL;
    TZrUInt32 *edgeExecutable = ZR_NULL;
    SZrExecIrSccpValue *cacheValues = ZR_NULL;
    TZrUInt32 index;
    size_t bytes;
    if (changed != ZR_NULL) *changed = ZR_FALSE;
    if (!zr_sccp_storage_valid(function, diagnostic) ||
        !zr_sccp_alloc_arrays(function, &lattice, &blockExecutable, &edgeExecutable)) {
        if (diagnostic != ZR_NULL && diagnostic->code == ZR_EXECUTION_DIAGNOSTIC_NONE)
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                           function, 0u, 0u, 0u, 0u);
        free(lattice);
        free(blockExecutable);
        free(edgeExecutable);
        return ZR_FALSE;
    }
    if (!zr_sccp_compute(function, context, lattice, blockExecutable, edgeExecutable, diagnostic)) {
        free(lattice);
        free(blockExecutable);
        free(edgeExecutable);
        return ZR_FALSE;
    }
    if (context != ZR_NULL && context->budgetExhausted) {
        free(lattice);
        free(blockExecutable);
        free(edgeExecutable);
        return ZR_TRUE;
    }
    if (context != ZR_NULL && context->cache != ZR_NULL) {
        if (zr_sccp_size_mul_overflow((TZrUInt64)function->valueCount,
                                      sizeof(*cacheValues))) {
            ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                           function, 0u, 0u, UINT32_MAX, function->valueCount);
            free(lattice);
            free(blockExecutable);
            free(edgeExecutable);
            return ZR_FALSE;
        }
        bytes = (size_t)function->valueCount * sizeof(*cacheValues);
        if (bytes != 0u) {
            cacheValues = (SZrExecIrSccpValue *)malloc(bytes);
            if (cacheValues == ZR_NULL) {
                ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                               function, 0u, 0u, function->valueCount, 0u);
                free(lattice);
                free(blockExecutable);
                free(edgeExecutable);
                return ZR_FALSE;
            }
            for (index = 0u; index < function->valueCount; ++index) {
                cacheValues[index].kind = lattice[index + 1u].kind;
                cacheValues[index].bits = lattice[index + 1u].bits;
                cacheValues[index].typeToken = lattice[index + 1u].typeToken;
            }
        }
        free(context->cache->sccpValues);
        context->cache->sccpValues = cacheValues;
        context->cache->sccpValueCount = function->valueCount;
        context->cache->validMask |= ZR_EXEC_IR_ANALYSIS_SCCP;
    }
    if (rewrite) {
        if (!zr_sccp_rewrite(function, context, lattice, changed, diagnostic)) {
            free(lattice);
            free(blockExecutable);
            free(edgeExecutable);
            return ZR_FALSE;
        }
        if (context != ZR_NULL && context->cache != ZR_NULL &&
            changed != ZR_NULL && *changed) {
            /* The cache describes the pre-rewrite value graph.  Do not leave
             * it marked valid for a caller that invokes this helper directly
             * (the manager also performs the broader invalidation protocol). */
            context->cache->validMask &= ~ZR_EXEC_IR_ANALYSIS_SCCP;
            free(context->cache->sccpValues);
            context->cache->sccpValues = ZR_NULL;
            context->cache->sccpValueCount = 0u;
        }
    }
    free(lattice);
    free(blockExecutable);
    free(edgeExecutable);
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_RunSccpPass(SZrExecIrFunction *function,
                                    SZrExecIrPassContext *context,
                                    TZrBool *changed,
                                    SZrExecIrDiagnostic *diagnostic) {
    return ZrParser_ExecIr_ComputeSccp(function, context, ZR_TRUE, changed, diagnostic);
}
