#include "exec_ir_interpreter_internal.h"

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define ZR_ORACLE_DEFAULT_STEP_MULTIPLIER ((TZrUInt32)1024u)
#define ZR_ORACLE_LOCAL_OPERAND_LIMIT ((TZrUInt32)8u)

void zr_oracle_diag(SZrExecIrDiagnostic *d, EZrExecutionDiagnosticCode code,
                           const SZrExecIrFunction *f, TZrExecIrBlockId block,
                           TZrExecIrInstructionId instruction, TZrExecIrSourceId source,
                           TZrUInt32 expected, TZrUInt32 actual) {
    if (d == ZR_NULL) return;
    memset(d, 0, sizeof(*d));
    d->code = code;
    d->functionToken = f != ZR_NULL ? f->functionToken : 0u;
    d->blockId = block;
    d->instructionId = instruction;
    d->sourceId = source;
    d->expectedVersion = expected;
    d->actualVersion = actual;
}

static TZrBool zr_oracle_range(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)(range.start <= count && range.count <= count - range.start);
}

static TZrBool zr_oracle_block_has_successor(const SZrExecIrFunction *f,
                                              const SZrExecIrBlock *block,
                                              TZrExecIrBlockId id) {
    TZrUInt32 i;
    for (i = 0u; i < block->successorRange.count; ++i) {
        if (f->successors[block->successorRange.start + i] == id) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_oracle_block_has_predecessor(const SZrExecIrFunction *f,
                                               const SZrExecIrBlock *block,
                                               TZrExecIrBlockId id) {
    TZrUInt32 i;
    for (i = 0u; i < block->predecessorRange.count; ++i) {
        if (f->predecessors[block->predecessorRange.start + i] == id) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

TZrBool zr_oracle_bytes(TZrUInt32 count, size_t element, size_t *bytes) {
    if (bytes == ZR_NULL || element == 0u || (size_t)count > SIZE_MAX / element) return ZR_FALSE;
    *bytes = (size_t)count * element;
    return ZR_TRUE;
}

static TZrBool zr_oracle_value_kind_valid(EZrExecIrOracleValueKind kind) {
    return (TZrBool)(kind >= ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED &&
                     kind < ZR_EXEC_IR_ORACLE_VALUE_KIND_COUNT);
}

static TZrBool zr_oracle_validate(const SZrExecIrFunction *f, SZrExecIrDiagnostic *d) {
    TZrUInt32 i;
    if (f == ZR_NULL || f->valueCount > f->valueCapacity ||
        f->instructionCount > f->instructionCapacity || f->blockCount > f->blockCapacity ||
        f->operandCount > f->operandCapacity || f->resultCount > f->resultCapacity ||
        f->successorCount > f->successorCapacity || f->predecessorCount > f->predecessorCapacity ||
        f->phiCount > f->phiCapacity || f->phiIncomingCount > f->phiIncomingCapacity ||
        f->memoryTokenCount > f->memoryTokenCapacity ||
        f->gcMapCount > f->gcMapCapacity || f->gcRootCount > f->gcRootCapacity ||
        f->deoptStateCount > f->deoptStateCapacity || f->deoptValueCount > f->deoptValueCapacity ||
        f->sourceMapCount > f->sourceMapCapacity ||
        (f->valueCount && f->values == ZR_NULL) ||
        (f->instructionCount && f->instructions == ZR_NULL) ||
        (f->blockCount && f->blocks == ZR_NULL) ||
        (f->operandCount && f->operands == ZR_NULL) ||
        (f->resultCount && f->results == ZR_NULL) ||
        (f->successorCount && f->successors == ZR_NULL) ||
        (f->predecessorCount && f->predecessors == ZR_NULL) ||
        (f->phiCount && f->phiPool == ZR_NULL) ||
        (f->phiIncomingCount && f->phiIncoming == ZR_NULL) ||
        (f->memoryTokenCount && f->memoryTokenPool == ZR_NULL) ||
        (f->gcMapCount && f->gcMap == ZR_NULL) ||
        (f->gcRootCount && f->gcRoots == ZR_NULL) ||
        (f->deoptStateCount && f->deoptStates == ZR_NULL) ||
        (f->deoptValueCount && f->deoptValues == ZR_NULL) ||
        (f->sourceMapCount && f->sourceMaps == ZR_NULL) ||
        (f->entryBlockId > f->blockCount && f->entryBlockId != ZR_EXEC_IR_BLOCK_ID_INVALID)) {
        zr_oracle_diag(d, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, f, 0u, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    for (i = 0u; i < f->valueCount; ++i) {
        if (f->values[i].id != i + 1u) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, 0u, 0u, 0u, i + 1u, f->values[i].id);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < f->operandCount; ++i) {
        if (f->operands[i] == ZR_EXEC_IR_VALUE_ID_INVALID || f->operands[i] > f->valueCount) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, 0u, 0u, 0u, f->valueCount, f->operands[i]);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < f->resultCount; ++i) {
        if (f->results[i] == ZR_EXEC_IR_VALUE_ID_INVALID || f->results[i] > f->valueCount) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, 0u, 0u, 0u, f->valueCount, f->results[i]);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < f->predecessorCount; ++i) {
        if (f->predecessors[i] == ZR_EXEC_IR_BLOCK_ID_INVALID || f->predecessors[i] > f->blockCount) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f, 0u, 0u, 0u, f->blockCount, f->predecessors[i]);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < f->successorCount; ++i) {
        if (f->successors[i] == ZR_EXEC_IR_BLOCK_ID_INVALID || f->successors[i] > f->blockCount) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f, 0u, 0u, 0u, f->blockCount, f->successors[i]);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < f->phiIncomingCount; ++i) {
        if (f->phiIncoming[i].predecessor == ZR_EXEC_IR_BLOCK_ID_INVALID ||
            f->phiIncoming[i].predecessor > f->blockCount ||
            f->phiIncoming[i].value == ZR_EXEC_IR_VALUE_ID_INVALID ||
            f->phiIncoming[i].value > f->valueCount) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f,
                           f->phiIncoming[i].predecessor, 0u, 0u, f->valueCount,
                           f->phiIncoming[i].value);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < f->phiCount; ++i) {
        const SZrExecIrPhi *p = &f->phiPool[i];
        if (p->result == ZR_EXEC_IR_VALUE_ID_INVALID || p->result > f->valueCount ||
            !zr_oracle_range(p->incomings, f->phiIncomingCount)) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, f, 0u, 0u, 0u,
                           f->phiIncomingCount, p->incomings.start + p->incomings.count);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < f->blockCount; ++i) {
        const SZrExecIrBlock *b = &f->blocks[i];
        if (b->id != i + 1u || !zr_oracle_range(b->instructionRange, f->instructionCount) ||
            !zr_oracle_range(b->predecessorRange, f->predecessorCount) ||
            !zr_oracle_range(b->successorRange, f->successorCount) ||
            !zr_oracle_range(b->phis, f->phiCount)) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f, b->id, 0u, 0u, i + 1u, b->id);
            return ZR_FALSE;
        }
        {
            TZrUInt32 j;
            for (j = 0u; j < b->successorRange.count; ++j) {
                TZrExecIrBlockId successor = f->successors[b->successorRange.start + j];
                const SZrExecIrBlock *target = &f->blocks[successor - 1u];
                if (!zr_oracle_block_has_predecessor(f, target, b->id)) {
                    zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f,
                                   b->id, 0u, 0u, 1u, successor);
                    return ZR_FALSE;
                }
            }
            for (j = 0u; j < b->predecessorRange.count; ++j) {
                TZrExecIrBlockId predecessor = f->predecessors[b->predecessorRange.start + j];
                const SZrExecIrBlock *source = &f->blocks[predecessor - 1u];
                if (!zr_oracle_block_has_successor(f, source, b->id)) {
                    zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f,
                                   b->id, 0u, 0u, 1u, predecessor);
                    return ZR_FALSE;
                }
            }
        }
    }
    for (i = 0u; i < f->blockCount; ++i) {
        const SZrExecIrBlock *b = &f->blocks[i];
        TZrUInt32 j;
        for (j = 0u; j < b->phis.count; ++j) {
            const SZrExecIrPhi *phi = &f->phiPool[b->phis.start + j];
            if (phi->incomings.count != b->predecessorRange.count) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                               f, b->id, 0u, 0u, b->predecessorRange.count,
                               phi->incomings.count);
                return ZR_FALSE;
            }
            for (TZrUInt32 k = 0u; k < phi->incomings.count; ++k) {
                if (f->phiIncoming[phi->incomings.start + k].predecessor !=
                    f->predecessors[b->predecessorRange.start + k]) {
                    zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                   f, b->id, 0u, 0u,
                                   f->predecessors[b->predecessorRange.start + k],
                                   f->phiIncoming[phi->incomings.start + k].predecessor);
                    return ZR_FALSE;
                }
            }
        }
    }
    for (i = 0u; i < f->instructionCount; ++i) {
        const SZrExecIrInstruction *ins = &f->instructions[i];
        const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)ins->opcode);
        if (info == ZR_NULL || ins->opcode == ZR_EXEC_IR_OPCODE_INVALID) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE, f, 0u, i + 1u,
                           ins->sourceId, ZR_EXEC_IR_OPCODE_COUNT - 1u, ins->opcode);
            return ZR_FALSE;
        }
        if ((ins->flags & ~ZR_EXEC_IR_INSTRUCTION_FLAG_KNOWN_MASK) != 0u ||
            !zr_oracle_range(ins->operands, f->operandCount) ||
            !zr_oracle_range(ins->results, f->resultCount) ||
            !zr_oracle_range(ins->successorRange, f->successorCount) ||
            !zr_oracle_range(ins->phiRange, f->phiCount) ||
            !zr_oracle_range(ins->memoryIn, f->memoryTokenCount) ||
            !zr_oracle_range(ins->memoryOut, f->memoryTokenCount) ||
            ins->operands.count < info->minimumOperands ||
            ins->operands.count > info->maximumOperands ||
            (info->resultArity != ZR_EXEC_IR_VARIADIC && ins->results.count != info->resultArity)) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, f, 0u, i + 1u,
                           ins->sourceId, info->minimumOperands, ins->operands.count);
            return ZR_FALSE;
        }
        for (TZrUInt32 j = 0u; j < ins->results.count; ++j) {
            TZrExecIrValueId valueId = f->results[ins->results.start + j];
            if (valueId == ZR_EXEC_IR_VALUE_ID_INVALID || valueId > f->valueCount) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, 0u,
                               i + 1u, ins->sourceId, f->valueCount, valueId);
                return ZR_FALSE;
            }
        }
        for (TZrUInt32 j = 0u; j < ins->successorRange.count; ++j) {
            TZrExecIrBlockId target = f->successors[ins->successorRange.start + j];
            if (target == ZR_EXEC_IR_BLOCK_ID_INVALID || target > f->blockCount) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f, 0u,
                               i + 1u, ins->sourceId, f->blockCount, target);
                return ZR_FALSE;
            }
        }
        {
            EZrExecIrOpcode opcode = (EZrExecIrOpcode)ins->opcode;
            TZrUInt32 successorCount = ins->successorRange.count;
            if ((opcode == ZR_EXEC_IR_OPCODE_BRANCH && successorCount != 1u) ||
                (opcode == ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH && successorCount != 2u) ||
                (opcode == ZR_EXEC_IR_OPCODE_SWITCH && successorCount == 0u)) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, f, 0u,
                               i + 1u, ins->sourceId, 1u, successorCount);
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

static void zr_oracle_undefined(SZrExecIrOracleValue *v) {
    if (v != ZR_NULL) { memset(v, 0, sizeof(*v)); v->kind = ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED; }
}

static TZrBool zr_oracle_numeric(const SZrExecIrOracleValue *v) {
    return (TZrBool)(v != ZR_NULL && v->kind >= ZR_EXEC_IR_ORACLE_VALUE_BOOL &&
                     v->kind <= ZR_EXEC_IR_ORACLE_VALUE_FLOAT);
}

static TZrInt64 zr_oracle_signed(const SZrExecIrOracleValue *v) {
    if (v->kind == ZR_EXEC_IR_ORACLE_VALUE_BOOL) return v->as.boolean ? 1 : 0;
    if (v->kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED) return (TZrInt64)v->as.unsignedInteger;
    return v->as.signedInteger;
}

static TZrUInt64 zr_oracle_unsigned(const SZrExecIrOracleValue *v) {
    if (v->kind == ZR_EXEC_IR_ORACLE_VALUE_BOOL) return v->as.boolean ? 1u : 0u;
    if (v->kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED) return (TZrUInt64)v->as.signedInteger;
    return v->as.unsignedInteger;
}

static TZrFloat64 zr_oracle_float(const SZrExecIrOracleValue *v) {
    if (v->kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT) return v->as.floating;
    return v->kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED
               ? (TZrFloat64)v->as.unsignedInteger : (TZrFloat64)zr_oracle_signed(v);
}

static int zr_oracle_compare_values(const SZrExecIrOracleValue *left,
                                    const SZrExecIrOracleValue *right) {
    if (left->kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT ||
        right->kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT) {
        TZrFloat64 a = zr_oracle_float(left);
        TZrFloat64 b = zr_oracle_float(right);
        if (a < b) return -1;
        if (a > b) return 1;
        return 0;
    }
    if (left->kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED ||
        right->kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED) {
        TZrUInt64 a = zr_oracle_unsigned(left);
        TZrUInt64 b = zr_oracle_unsigned(right);
        if (a < b) return -1;
        if (a > b) return 1;
        return 0;
    }
    {
        TZrInt64 a = zr_oracle_signed(left);
        TZrInt64 b = zr_oracle_signed(right);
        if (a < b) return -1;
        if (a > b) return 1;
        return 0;
    }
}

static TZrBool zr_oracle_signed_op(TZrInt64 a, TZrInt64 b, EZrExecIrOpcode op, TZrInt64 *out) {
    if (out == ZR_NULL) return ZR_FALSE;
    switch (op) {
        case ZR_EXEC_IR_OPCODE_ADD:
        case ZR_EXEC_IR_OPCODE_ARITHMETIC:
            if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) return ZR_FALSE;
            *out = a + b; return ZR_TRUE;
        case ZR_EXEC_IR_OPCODE_SUB:
            if ((b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b)) return ZR_FALSE;
            *out = a - b; return ZR_TRUE;
        case ZR_EXEC_IR_OPCODE_MUL:
            if (a != 0 && b != 0 && ((a == INT64_MIN && b == -1) || (b == INT64_MIN && a == -1) ||
                (a > 0 && b > 0 && a > INT64_MAX / b) || (a > 0 && b < 0 && b < INT64_MIN / a) ||
                (a < 0 && b > 0 && a < INT64_MIN / b) || (a < 0 && b < 0 && a < INT64_MAX / b))) return ZR_FALSE;
            *out = a * b; return ZR_TRUE;
        case ZR_EXEC_IR_OPCODE_DIV:
            if (b == 0 || (a == INT64_MIN && b == -1)) return ZR_FALSE;
            *out = a / b; return ZR_TRUE;
        default: return ZR_FALSE;
    }
}

static TZrBool zr_oracle_binary(const SZrExecIrInstruction *ins,
                                const SZrExecIrOracleValue *a,
                                const SZrExecIrOracleValue *b,
                                SZrExecIrOracleValue *out) {
    if (ins == ZR_NULL || out == ZR_NULL || !zr_oracle_numeric(a) || !zr_oracle_numeric(b)) return ZR_FALSE;
    if (a->kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT || b->kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT) {
        TZrFloat64 x = zr_oracle_float(a), y = zr_oracle_float(b);
        out->kind = ZR_EXEC_IR_ORACLE_VALUE_FLOAT;
        switch ((EZrExecIrOpcode)ins->opcode) {
            case ZR_EXEC_IR_OPCODE_ADD: case ZR_EXEC_IR_OPCODE_ARITHMETIC: out->as.floating = x + y; break;
            case ZR_EXEC_IR_OPCODE_SUB: out->as.floating = x - y; break;
            case ZR_EXEC_IR_OPCODE_MUL: out->as.floating = x * y; break;
            case ZR_EXEC_IR_OPCODE_DIV:
                if (y == 0.0) return ZR_FALSE;
                out->as.floating = x / y;
                break;
            default: return ZR_FALSE;
        }
        return ZR_TRUE;
    }
    if (a->kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED || b->kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED) {
        TZrUInt64 x = zr_oracle_unsigned(a), y = zr_oracle_unsigned(b);
        out->kind = ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED;
        switch ((EZrExecIrOpcode)ins->opcode) {
            case ZR_EXEC_IR_OPCODE_ADD: case ZR_EXEC_IR_OPCODE_ARITHMETIC:
                if (x > UINT64_MAX - y) return ZR_FALSE;
                out->as.unsignedInteger = x + y;
                break;
            case ZR_EXEC_IR_OPCODE_SUB:
                if (x < y) return ZR_FALSE;
                out->as.unsignedInteger = x - y;
                break;
            case ZR_EXEC_IR_OPCODE_MUL:
                if (y != 0u && x > UINT64_MAX / y) return ZR_FALSE;
                out->as.unsignedInteger = x * y;
                break;
            case ZR_EXEC_IR_OPCODE_DIV:
                if (y == 0u) return ZR_FALSE;
                out->as.unsignedInteger = x / y;
                break;
            default: return ZR_FALSE;
        }
        return ZR_TRUE;
    }
    out->kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    return zr_oracle_signed_op(zr_oracle_signed(a), zr_oracle_signed(b),
                               (EZrExecIrOpcode)ins->opcode, &out->as.signedInteger);
}

static TZrBool zr_oracle_append_event(SZrExecIrOracleExecutionResult *r,
                                       EZrExecIrOracleEventKind kind,
                                       TZrExecIrInstructionId id, TZrExecIrSourceId source,
                                       const SZrExecIrOracleValue *ops, TZrUInt32 count,
                                       const SZrExecIrFunction *f, TZrExecIrBlockId block,
                                       SZrExecIrDiagnostic *d) {
    TZrUInt32 cap, copyCount;
    size_t bytes;
    SZrExecIrOracleEvent *events;
    if (r->eventCount == UINT32_MAX) {
        zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, f, block, id,
                       source, UINT32_MAX, r->eventCount);
        return ZR_FALSE;
    }
    if (r->eventCount == r->eventCapacity) {
        cap = r->eventCapacity == 0u ? 8u : r->eventCapacity;
        if (cap > UINT32_MAX / 2u) cap = UINT32_MAX; else cap *= 2u;
        if (!zr_oracle_bytes(cap, sizeof(*events), &bytes)) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, f, block, id, source, UINT32_MAX, cap);
            return ZR_FALSE;
        }
        events = (SZrExecIrOracleEvent *)realloc(r->events, bytes);
        if (events == ZR_NULL) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, f, block, id, source, cap, r->eventCapacity);
            return ZR_FALSE;
        }
        r->events = events; r->eventCapacity = cap;
    }
    events = &r->events[r->eventCount++];
    memset(events, 0, sizeof(*events));
    events->kind = kind; events->instructionId = id; events->sourceId = source;
    copyCount = count < ZR_EXEC_IR_ORACLE_EVENT_OPERAND_LIMIT ? count : ZR_EXEC_IR_ORACLE_EVENT_OPERAND_LIMIT;
    events->operandCount = copyCount;
    if (copyCount != 0u && ops != ZR_NULL) memcpy(events->operands, ops, (size_t)copyCount * sizeof(*ops));
    return ZR_TRUE;
}

static TZrBool zr_oracle_assign(const SZrExecIrFunction *f, const SZrExecIrInstruction *ins,
                                SZrExecIrOracleExecutionResult *r, const SZrExecIrOracleValue *v,
                                TZrExecIrBlockId block, TZrExecIrInstructionId id,
                                SZrExecIrDiagnostic *d) {
    TZrExecIrValueId valueId;
    if (ins->results.count == 0u) return ZR_TRUE;
    valueId = f->results[ins->results.start];
    if (valueId == ZR_EXEC_IR_VALUE_ID_INVALID || valueId > r->valueCount) {
        zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, block, id, ins->sourceId, r->valueCount, valueId);
        return ZR_FALSE;
    }
    r->values[valueId - 1u] = *v;
    return ZR_TRUE;
}

static void zr_oracle_consume_operands(const SZrExecIrFunction *f,
                                       const SZrExecIrInstruction *ins,
                                       SZrExecIrOracleExecutionResult *r) {
    TZrUInt32 i;
    if (f == ZR_NULL || ins == ZR_NULL || r == ZR_NULL) return;
    for (i = 0u; i < ins->operands.count; ++i) {
        TZrExecIrValueId valueId = f->operands[ins->operands.start + i];
        /* Structural validation and the operand load already checked these
         * identities.  Keep the guard so a future caller cannot turn the
         * ownership transition into an out-of-bounds write. */
        if (valueId != ZR_EXEC_IR_VALUE_ID_INVALID && valueId <= r->valueCount) {
            zr_oracle_undefined(&r->values[valueId - 1u]);
        }
    }
}

static TZrBool zr_oracle_supported(EZrExecIrOpcode op, const SZrExecIrOracleInput *input) {
    if (op == ZR_EXEC_IR_OPCODE_CALL) {
        return (TZrBool)(input != ZR_NULL && input->call != ZR_NULL);
    }
    /* A load has no meaningful default value.  Require an explicit provider
     * so an oracle run cannot accidentally turn an unmodelled heap read into
     * a successful constant.  Stores retain the historical event-only mode
     * when no provider is supplied, while a provider enables stateful replay.
     */
    if (op == ZR_EXEC_IR_OPCODE_LOAD) {
        return (TZrBool)(input != ZR_NULL && input->memory != ZR_NULL);
    }
    if (op == ZR_EXEC_IR_OPCODE_ALLOC) {
        return (TZrBool)(input != ZR_NULL && input->allocate != ZR_NULL);
    }
    switch (op) {
        case ZR_EXEC_IR_OPCODE_NOP: case ZR_EXEC_IR_OPCODE_CONSTANT: case ZR_EXEC_IR_OPCODE_CONVERT:
        case ZR_EXEC_IR_OPCODE_ARITHMETIC: case ZR_EXEC_IR_OPCODE_COPY: case ZR_EXEC_IR_OPCODE_MOVE:
        case ZR_EXEC_IR_OPCODE_ADD: case ZR_EXEC_IR_OPCODE_SUB: case ZR_EXEC_IR_OPCODE_MUL:
        case ZR_EXEC_IR_OPCODE_DIV: case ZR_EXEC_IR_OPCODE_NEG: case ZR_EXEC_IR_OPCODE_COMPARE:
        case ZR_EXEC_IR_OPCODE_STORE: case ZR_EXEC_IR_OPCODE_DROP: case ZR_EXEC_IR_OPCODE_BARRIER:
        case ZR_EXEC_IR_OPCODE_BRANCH: case ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH: case ZR_EXEC_IR_OPCODE_SWITCH:
        case ZR_EXEC_IR_OPCODE_THROW: case ZR_EXEC_IR_OPCODE_SUSPEND: case ZR_EXEC_IR_OPCODE_RETURN:
        case ZR_EXEC_IR_OPCODE_PHI: return ZR_TRUE;
        default: return ZR_FALSE;
    }
}

static TZrBool zr_oracle_truthy(const SZrExecIrOracleValue *v) {
    if (v == ZR_NULL) return ZR_FALSE;
    switch (v->kind) {
        case ZR_EXEC_IR_ORACLE_VALUE_BOOL: return v->as.boolean != 0u;
        case ZR_EXEC_IR_ORACLE_VALUE_SIGNED: return v->as.signedInteger != 0;
        case ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED: return v->as.unsignedInteger != 0u;
        case ZR_EXEC_IR_ORACLE_VALUE_FLOAT: return v->as.floating != 0.0;
        default: return ZR_FALSE;
    }
}

static TZrBool zr_oracle_exec(const SZrExecIrOracleInput *input,
                              SZrExecIrOracleExecutionResult *r,
                              const SZrExecIrInstruction *ins,
                              TZrExecIrInstructionId id, TZrExecIrBlockId block,
                              TZrBool *terminated, TZrExecIrBlockId *next,
                              TZrUInt32 *nextOrdinal,
                              SZrExecIrDiagnostic *d) {
    const SZrExecIrFunction *f = input->function;
    TZrUInt32 n = ins->operands.count, i;
    SZrExecIrOracleValue local[ZR_ORACLE_LOCAL_OPERAND_LIMIT];
    SZrExecIrOracleValue *ops = local;
    SZrExecIrOracleValue v, callback;
    size_t bytes;
    EZrExecIrOpcode op = (EZrExecIrOpcode)ins->opcode;
    if (terminated != ZR_NULL) *terminated = ZR_FALSE;
    if (next != ZR_NULL) *next = ZR_EXEC_IR_BLOCK_ID_INVALID;
    if (nextOrdinal != ZR_NULL) *nextOrdinal = 0u;
    /* Reject a known-but-unmodeled operation before allocating an operand
     * scratch area or touching any value slot. */
    if (!zr_oracle_supported(op, input)) {
        r->unsupportedInstructionId = id;
        zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, f, block, id,
                       ins->sourceId, 0u, op);
        return ZR_FALSE;
    }
    if (n > ZR_ORACLE_LOCAL_OPERAND_LIMIT) {
        if (!zr_oracle_bytes(n, sizeof(*ops), &bytes)) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, f, block, id, ins->sourceId, UINT32_MAX, n); return ZR_FALSE;
        }
        ops = (SZrExecIrOracleValue *)malloc(bytes);
        if (ops == ZR_NULL) { zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, f, block, id, ins->sourceId, n, 0u); return ZR_FALSE; }
    }
    for (i = 0u; i < n; ++i) {
        TZrExecIrValueId valueId = f->operands[ins->operands.start + i];
        if (valueId == ZR_EXEC_IR_VALUE_ID_INVALID || valueId > r->valueCount ||
            r->values[valueId - 1u].kind == ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED) {
            if (ops != local) free(ops);
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, block, id, ins->sourceId, r->valueCount, valueId); return ZR_FALSE;
        }
        ops[i] = r->values[valueId - 1u];
    }
    ++r->supportedInstructionCount;
    zr_oracle_undefined(&v);
    switch (op) {
        case ZR_EXEC_IR_OPCODE_NOP: break;
        case ZR_EXEC_IR_OPCODE_CONSTANT:
            if (input->constants != ZR_NULL && ins->layoutId < input->constantCount) v = input->constants[ins->layoutId];
            else { v.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED; v.as.signedInteger = (TZrInt64)ins->layoutId; }
            if (!zr_oracle_assign(f, ins, r, &v, block, id, d)) goto fail;
            break;
        case ZR_EXEC_IR_OPCODE_COPY: case ZR_EXEC_IR_OPCODE_CONVERT:
            if (n != 1u) goto invalid;
            v = ops[0];
            if (!zr_oracle_assign(f, ins, r, &v, block, id, d)) goto fail;
            break;
        case ZR_EXEC_IR_OPCODE_MOVE:
            if (n != 1u) goto invalid;
            v = ops[0];
            if (!zr_oracle_assign(f, ins, r, &v, block, id, d)) goto fail;
            /* MOVE transfers the value into its SSA result, then makes the
             * source unavailable just as the state-map ownership contract
             * records it as moved. */
            zr_oracle_consume_operands(f, ins, r);
            break;
        case ZR_EXEC_IR_OPCODE_ADD: case ZR_EXEC_IR_OPCODE_SUB: case ZR_EXEC_IR_OPCODE_MUL:
        case ZR_EXEC_IR_OPCODE_DIV: case ZR_EXEC_IR_OPCODE_ARITHMETIC:
            if (n != 2u || !zr_oracle_binary(ins, &ops[0], &ops[1], &v)) goto arithmetic;
            if (!zr_oracle_assign(f, ins, r, &v, block, id, d)) goto fail;
            break;
        case ZR_EXEC_IR_OPCODE_NEG:
            if (n != 1u || !zr_oracle_numeric(&ops[0])) goto arithmetic;
            v = ops[0];
            if (v.kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT) v.as.floating = -v.as.floating;
            else { TZrInt64 x = zr_oracle_signed(&ops[0]); if (x == INT64_MIN) goto arithmetic; v.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED; v.as.signedInteger = -x; }
            if (!zr_oracle_assign(f, ins, r, &v, block, id, d)) goto fail;
            break;
        case ZR_EXEC_IR_OPCODE_COMPARE:
            if (n != 2u || !zr_oracle_numeric(&ops[0]) || !zr_oracle_numeric(&ops[1])) goto arithmetic;
            v.kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
            {
                int comparison = zr_oracle_compare_values(&ops[0], &ops[1]);
                switch (ins->typeToken) {
                    case 1u: v.as.boolean = (TZrBool)(comparison < 0); break;
                    case 2u: v.as.boolean = (TZrBool)(comparison <= 0); break;
                    case 3u: v.as.boolean = (TZrBool)(comparison > 0); break;
                    case 4u: v.as.boolean = (TZrBool)(comparison >= 0); break;
                    case 5u: v.as.boolean = (TZrBool)(comparison != 0); break;
                    default: v.as.boolean = (TZrBool)(comparison == 0); break;
                }
            }
            if (!zr_oracle_assign(f, ins, r, &v, block, id, d)) goto fail;
            break;
        case ZR_EXEC_IR_OPCODE_ALLOC:
            zr_oracle_undefined(&callback);
            if (input->allocate == ZR_NULL ||
                !input->allocate(input->allocateUserData, ins, ops, n,
                                 &callback)) {
                zr_oracle_diag(d,
                               ZR_EXEC_IR_DIAGNOSTIC_ORACLE_ALLOCATION_ERROR,
                               f, block, id, ins->sourceId, 1u, n);
                goto fail;
            }
            if (!zr_oracle_value_kind_valid(callback.kind) ||
                callback.kind == ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f,
                               block, id, ins->sourceId,
                               ZR_EXEC_IR_ORACLE_VALUE_KIND_COUNT,
                               (TZrUInt32)callback.kind);
                goto fail;
            }
            if (!zr_oracle_append_event(r,
                                        ZR_EXEC_IR_ORACLE_EVENT_ALLOCATE, id,
                                        ins->sourceId, ops, n, f, block, d) ||
                !zr_oracle_assign(f, ins, r, &callback, block, id, d)) {
                goto fail;
            }
            break;
        case ZR_EXEC_IR_OPCODE_LOAD:
            if (n != 1u || input->memory == ZR_NULL ||
                !input->memory(input->memoryUserData, ins,
                               ZR_EXEC_IR_ORACLE_MEMORY_LOAD, ops, n, &v)) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_ORACLE_MEMORY_ERROR,
                               f, block, id, ins->sourceId, 1u, n);
                goto fail;
            }
            if (!zr_oracle_value_kind_valid(v.kind) ||
                v.kind == ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                               f, block, id, ins->sourceId,
                               ZR_EXEC_IR_ORACLE_VALUE_KIND_COUNT,
                               (TZrUInt32)v.kind);
                goto fail;
            }
            if (!zr_oracle_append_event(r, ZR_EXEC_IR_ORACLE_EVENT_LOAD,
                                        id, ins->sourceId, ops, n, f, block, d) ||
                !zr_oracle_assign(f, ins, r, &v, block, id, d)) {
                goto fail;
            }
            break;
        case ZR_EXEC_IR_OPCODE_STORE:
            if (input->memory != ZR_NULL &&
                !input->memory(input->memoryUserData, ins,
                               ZR_EXEC_IR_ORACLE_MEMORY_STORE, ops, n,
                               ZR_NULL)) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_ORACLE_MEMORY_ERROR,
                               f, block, id, ins->sourceId, 2u, n);
                goto fail;
            }
            if (!zr_oracle_append_event(r, ZR_EXEC_IR_ORACLE_EVENT_STORE, id, ins->sourceId, ops, n, f, block, d)) goto fail;
            break;
        case ZR_EXEC_IR_OPCODE_DROP:
            if (!zr_oracle_append_event(r, ZR_EXEC_IR_ORACLE_EVENT_DROP, id,
                                        ins->sourceId, ops, n, f, block, d)) {
                goto fail;
            }
            /* Publish the bounded observation before clearing the consumed
             * slot.  A later use then fails through the normal invalid-value
             * path, matching the runtime's OWN_DROP slot reset. */
            zr_oracle_consume_operands(f, ins, r);
            break;
        case ZR_EXEC_IR_OPCODE_BARRIER:
            if (!zr_oracle_append_event(r, ZR_EXEC_IR_ORACLE_EVENT_BARRIER, id, ins->sourceId, ops, n, f, block, d)) goto fail;
            break;
        case ZR_EXEC_IR_OPCODE_CALL:
            zr_oracle_undefined(&callback);
            if (!input->call(input->userData, ins, ops, n, &callback)) goto fail;
            if (!zr_oracle_value_kind_valid(callback.kind) ||
                callback.kind == ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, block,
                               id, ins->sourceId, ZR_EXEC_IR_ORACLE_VALUE_KIND_COUNT,
                               (TZrUInt32)callback.kind);
                goto fail;
            }
            if (!zr_oracle_append_event(r, ZR_EXEC_IR_ORACLE_EVENT_CALL, id, ins->sourceId, ops, n, f, block, d) ||
                !zr_oracle_assign(f, ins, r, &callback, block, id, d)) goto fail;
            break;
        case ZR_EXEC_IR_OPCODE_BRANCH:
            if (ins->successorRange.count < 1u) goto invalid;
            *next = f->successors[ins->successorRange.start];
            *terminated = ZR_TRUE;
            break;
        case ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH:
            if (n != 1u || ins->successorRange.count < 2u) goto invalid;
            *nextOrdinal = zr_oracle_truthy(&ops[0]) ? 0u : 1u;
            *next = f->successors[ins->successorRange.start + *nextOrdinal];
            *terminated = ZR_TRUE;
            break;
        case ZR_EXEC_IR_OPCODE_SWITCH: { TZrUInt64 choice; if (n < 1u || ins->successorRange.count == 0u) goto invalid; choice = zr_oracle_unsigned(&ops[0]); if (choice >= ins->successorRange.count) choice = ins->successorRange.count - 1u; *nextOrdinal = (TZrUInt32)choice; *next = f->successors[ins->successorRange.start + *nextOrdinal]; *terminated = ZR_TRUE; break; }
        case ZR_EXEC_IR_OPCODE_RETURN:
            if (n > 0u) r->returnValue = ops[0];
            r->returned = ZR_TRUE;
            *terminated = ZR_TRUE;
            break;
        case ZR_EXEC_IR_OPCODE_THROW:
            if (!zr_oracle_append_event(r, ZR_EXEC_IR_ORACLE_EVENT_THROW, id, ins->sourceId, ops, n, f, block, d)) goto fail;
            r->terminatedByThrow = ZR_TRUE;
            *terminated = ZR_TRUE;
            break;
        case ZR_EXEC_IR_OPCODE_SUSPEND:
            if (!zr_oracle_append_event(r, ZR_EXEC_IR_ORACLE_EVENT_SUSPEND, id, ins->sourceId, ops, n, f, block, d)) goto fail;
            if (n > 0u) r->returnValue = ops[0];
            if (n > 0u && !zr_oracle_assign(f, ins, r, &ops[0], block, id, d)) goto fail;
            r->suspended = ZR_TRUE;
            *terminated = ZR_TRUE;
            break;
        case ZR_EXEC_IR_OPCODE_PHI: break;
        default: goto fail;
    }
    if (ops != local) free(ops);
    return ZR_TRUE;
arithmetic:
    zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_ARITHMETIC_ERROR, f, block, id, ins->sourceId, 1u, 0u);
    goto fail;
invalid:
    zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, f, block, id, ins->sourceId, 1u, n);
fail:
    if (ops != local) free(ops);
    return ZR_FALSE;
}

void ZrCore_ExecIr_OracleResultInit(SZrExecIrOracleExecutionResult *r) {
    if (r != ZR_NULL) {
        memset(r, 0, sizeof(*r));
        r->ownershipTag = ZR_EXEC_IR_ORACLE_RESULT_TAG;
    }
}

void ZrCore_ExecIr_OracleResultFree(SZrExecIrOracleExecutionResult *r) {
    if (r != ZR_NULL && r->ownershipTag == ZR_EXEC_IR_ORACLE_RESULT_TAG) {
        free(r->values);
        free(r->events);
        memset(r, 0, sizeof(*r));
    }
}

TZrBool ZrCore_ExecIr_RunOracleEx(const SZrExecIrOracleInput *input,
                                  SZrExecIrOracleExecutionResult *result,
                                  SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrFunction *f;
    SZrExecIrOracleExecutionResult prepared;
    TZrUInt32 i, maxSteps, steps = 0u;
    TZrExecIrBlockId current, previous = ZR_EXEC_IR_BLOCK_ID_INVALID;
    TZrUInt32 previousOrdinal = 0u;
    size_t bytes;

    memset(&prepared, 0, sizeof(prepared));
    prepared.ownershipTag = ZR_EXEC_IR_ORACLE_RESULT_TAG;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (input == ZR_NULL || result == ZR_NULL || input->function == ZR_NULL) {
        zr_oracle_diag(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                       input != ZR_NULL ? input->function : ZR_NULL,
                       0u, 0u, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    f = input->function;
    if (!zr_oracle_validate(f, diagnostic) || input->initialValueCount > f->valueCount ||
        (input->initialValueCount && input->initialValues == ZR_NULL) || (input->constantCount && input->constants == ZR_NULL)) {
        if (diagnostic != ZR_NULL && diagnostic->code == ZR_EXECUTION_DIAGNOSTIC_NONE) {
            zr_oracle_diag(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                           f, 0u, 0u, 0u, f->valueCount, input->initialValueCount);
        }
        return ZR_FALSE;
    }
    for (i = 0u; i < input->initialValueCount; ++i) {
        if (!zr_oracle_value_kind_valid(input->initialValues[i].kind)) {
            zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f,
                           0u, 0u, 0u, ZR_EXEC_IR_ORACLE_VALUE_KIND_COUNT,
                           (TZrUInt32)input->initialValues[i].kind);
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < input->constantCount; ++i) {
        if (!zr_oracle_value_kind_valid(input->constants[i].kind)) {
            zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f,
                           0u, 0u, 0u, ZR_EXEC_IR_ORACLE_VALUE_KIND_COUNT,
                           (TZrUInt32)input->constants[i].kind);
            return ZR_FALSE;
        }
    }
    prepared.instructionCount = f->instructionCount;
    prepared.valueCount = f->valueCount;
    prepared.valueCapacity = f->valueCount;
    if (f->valueCount != 0u) {
        if (!zr_oracle_bytes(f->valueCount, sizeof(*prepared.values), &bytes)) {
            zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                           f, 0u, 0u, 0u, UINT32_MAX, f->valueCount);
            return ZR_FALSE;
        }
        prepared.values = (SZrExecIrOracleValue *)malloc(bytes);
        if (prepared.values == ZR_NULL) {
            zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                           f, 0u, 0u, 0u, f->valueCount, 0u);
            return ZR_FALSE;
        }
        for (i = 0u; i < f->valueCount; ++i) zr_oracle_undefined(&prepared.values[i]);
        if (input->initialValueCount) {
            memcpy(prepared.values, input->initialValues,
                   (size_t)input->initialValueCount * sizeof(*prepared.values));
        }
    }
    maxSteps = input->maxSteps;
    if (maxSteps == 0u) {
        maxSteps = f->instructionCount > UINT32_MAX / ZR_ORACLE_DEFAULT_STEP_MULTIPLIER
                       ? UINT32_MAX
                       : f->instructionCount * ZR_ORACLE_DEFAULT_STEP_MULTIPLIER + 1u;
    }
    if (maxSteps == 0u) maxSteps = UINT32_MAX;
    current = f->blockCount ? (f->entryBlockId ? f->entryBlockId : ZR_EXEC_IR_BLOCK_ID_ENTRY) : ZR_EXEC_IR_BLOCK_ID_INVALID;
    for (;;) {
        TZrUInt32 first, count, cursor;
        TZrBool blockTerminated = ZR_FALSE;
        TZrBool done = ZR_FALSE;
        TZrExecIrBlockId next = ZR_EXEC_IR_BLOCK_ID_INVALID;
        TZrUInt32 nextOrdinal = 0u;
        if (steps >= maxSteps) {
            zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_ORACLE_STEP_LIMIT,
                           f, current, 0u, 0u, maxSteps, steps);
            ZrCore_ExecIr_OracleResultFree(&prepared);
            return ZR_FALSE;
        }
        if (!f->blockCount) {
            first = 0u;
            count = f->instructionCount;
        } else {
            const SZrExecIrBlock *b;
            if (current == ZR_EXEC_IR_BLOCK_ID_INVALID || current > f->blockCount ||
                !zr_oracle_enter(f, current, previous, previousOrdinal,
                                 &prepared, diagnostic)) {
                if (diagnostic != ZR_NULL && diagnostic->code == ZR_EXECUTION_DIAGNOSTIC_NONE) {
                    zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                   f, current, 0u, 0u, f->blockCount, current);
                }
                ZrCore_ExecIr_OracleResultFree(&prepared);
                return ZR_FALSE;
            }
            b = &f->blocks[current - 1u];
            first = b->instructionRange.start;
            count = b->instructionRange.count;
        }
        for (cursor = 0u; cursor < count; ++cursor) {
            TZrExecIrInstructionId id = first + cursor + 1u;
            TZrBool terminated = ZR_FALSE;
            if (steps == UINT32_MAX) {
                zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_ORACLE_STEP_LIMIT,
                               f, current, id, f->instructions[id - 1u].sourceId,
                               maxSteps, steps);
                ZrCore_ExecIr_OracleResultFree(&prepared);
                return ZR_FALSE;
            }
            ++steps;
            ++prepared.executedInstructionCount;
            if (!zr_oracle_exec(input, &prepared, &f->instructions[id - 1u],
                                id, current, &terminated, &next, &nextOrdinal,
                                diagnostic)) {
                ZrCore_ExecIr_OracleResultFree(&prepared);
                return ZR_FALSE;
            }
            if (terminated) {
                blockTerminated = ZR_TRUE;
                if (prepared.returned || prepared.terminatedByThrow || prepared.suspended) done = ZR_TRUE;
                break;
            }
        }
        if (done || !f->blockCount) {
            prepared.currentBlock = current;
            ZrCore_ExecIr_OracleResultFree(result);
            if (result->ownershipTag != ZR_EXEC_IR_ORACLE_RESULT_TAG) {
                memset(result, 0, sizeof(*result));
            }
            *result = prepared;
            return ZR_TRUE;
        }
        if (!blockTerminated) {
            const SZrExecIrBlock *b = &f->blocks[current - 1u];
            if (b->successorRange.count == 1u) {
                next = f->successors[b->successorRange.start];
            } else {
                zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MISSING_TERMINATOR,
                               f, current, 0u, 0u, 1u, b->successorRange.count);
                ZrCore_ExecIr_OracleResultFree(&prepared);
                return ZR_FALSE;
            }
        }
        if (next == ZR_EXEC_IR_BLOCK_ID_INVALID || next > f->blockCount) {
            zr_oracle_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                           f, current, 0u, 0u, f->blockCount, next);
            ZrCore_ExecIr_OracleResultFree(&prepared);
            return ZR_FALSE;
        }
        previous = current;
        previousOrdinal = nextOrdinal;
        current = next;
        prepared.currentBlock = current;
    }
}

TZrBool ZrCore_ExecIr_RunOracle(const SZrExecIrFunction *function, SZrExecIrOracleResult *result, SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrOracleInput input; SZrExecIrOracleExecutionResult execution; TZrBool ok;
    memset(&input, 0, sizeof(input)); memset(&execution, 0, sizeof(execution)); if (result != ZR_NULL) memset(result, 0, sizeof(*result)); input.function = function;
    ok = ZrCore_ExecIr_RunOracleEx(&input, &execution, diagnostic);
    if (result != ZR_NULL) { result->instructionCount = execution.instructionCount; result->supportedInstructionCount = execution.supportedInstructionCount; result->unsupportedInstructionId = execution.unsupportedInstructionId; }
    ZrCore_ExecIr_OracleResultFree(&execution); return ok;
}
