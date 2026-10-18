#include "exec_ir_sccp_internal.h"

#include <stdlib.h>

/* Owner-only flat lattice: BOTTOM < exact typed constant < TOP. Values and
 * executable successor slots only rise. No provisional fact is rewritten. */
typedef struct SZrSccpOwnerAnalysis {
    SZrExecIrFunction *function;
    SZrExecIrPassContext *context;
    SZrSccpLattice *values;
    TZrUInt8 *blocks;
    TZrUInt32 *edges;
    SZrExecIrDiagnostic *diagnostic;
    TZrBool interrupted;
} SZrSccpOwnerAnalysis;

static SZrSccpLattice zr_owner_fact(EZrExecIrSccpLatticeKind kind,
                                    TZrExecIrTypeToken type, TZrUInt64 bits) {
    SZrSccpLattice value;
    value.kind = kind;
    value.bits = bits;
    value.typeToken = type;
    value.signedRepresentation = (TZrBool)(kind == ZR_EXEC_IR_SCCP_CONSTANT &&
                                           type == ZR_VALUE_TYPE_INT64);
    value.strictInt64 = value.signedRepresentation;
    return value;
}

static SZrSccpLattice zr_owner_bottom(void) {
    return zr_owner_fact(ZR_EXEC_IR_SCCP_UNKNOWN, 0u, 0u);
}

static SZrSccpLattice zr_owner_top(TZrExecIrTypeToken type) {
    return zr_owner_fact(ZR_EXEC_IR_SCCP_OVERDEFINED, type, 0u);
}

static TZrBool zr_owner_type(TZrExecIrTypeToken type) {
    return (TZrBool)(type == ZR_VALUE_TYPE_INT64 || type == ZR_VALUE_TYPE_BOOL);
}

static TZrBool zr_owner_work(SZrSccpOwnerAnalysis *analysis, TZrUInt64 work) {
    if (ZrParser_ExecIr_PassConsumeBudget(analysis->function, analysis->context, work))
        return ZR_TRUE;
    analysis->interrupted = ZR_TRUE;
    return ZR_FALSE;
}

static TZrBool zr_owner_invalid(SZrSccpOwnerAnalysis *analysis,
                                TZrExecIrBlockId block, TZrUInt32 expected,
                                TZrUInt32 actual) {
    ZrParser_ExecIr_PassDiagnostic(analysis->diagnostic,
            ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, analysis->function,
            block, 0u, expected, actual);
    return ZR_FALSE;
}

static SZrSccpLattice zr_owner_join(SZrSccpLattice left,
                                    SZrSccpLattice right,
                                    TZrExecIrTypeToken type) {
    if (left.kind == ZR_EXEC_IR_SCCP_OVERDEFINED ||
        right.kind == ZR_EXEC_IR_SCCP_OVERDEFINED) return zr_owner_top(type);
    if (left.kind == ZR_EXEC_IR_SCCP_UNKNOWN) return right;
    if (right.kind == ZR_EXEC_IR_SCCP_UNKNOWN) return left;
    if (left.bits == right.bits && left.typeToken == right.typeToken &&
        left.strictInt64 == right.strictInt64) return left;
    return zr_owner_top(type);
}

static TZrBool zr_owner_update(SZrSccpOwnerAnalysis *analysis,
                               TZrExecIrValueId id, SZrSccpLattice transfer) {
    SZrSccpLattice old = analysis->values[id];
    SZrSccpLattice next = zr_owner_join(old, transfer,
            analysis->function->values[id - 1u].typeToken);
    if (old.kind == next.kind &&
        (old.kind != ZR_EXEC_IR_SCCP_CONSTANT ||
         (old.bits == next.bits && old.typeToken == next.typeToken &&
          old.strictInt64 == next.strictInt64))) return ZR_FALSE;
    analysis->values[id] = next;
    return ZR_TRUE;
}

static SZrSccpLattice zr_owner_value(const SZrSccpOwnerAnalysis *analysis,
                                    TZrExecIrValueId id,
                                    TZrExecIrTypeToken type) {
    SZrSccpLattice value;
    if (id == ZR_EXEC_IR_VALUE_ID_INVALID || id > analysis->function->valueCount ||
        !zr_owner_type(type) || analysis->function->values[id - 1u].typeToken != type)
        return zr_owner_top(type);
    value = analysis->values[id];
    if (value.kind == ZR_EXEC_IR_SCCP_CONSTANT &&
        (value.typeToken != type ||
         (type == ZR_VALUE_TYPE_INT64 && !value.strictInt64)))
        return zr_owner_top(type);
    return value;
}

static SZrSccpLattice zr_owner_operand(const SZrSccpOwnerAnalysis *analysis,
                                      const SZrExecIrInstruction *instruction,
                                      TZrUInt32 at, TZrExecIrTypeToken type) {
    if (at >= instruction->operands.count) return zr_owner_top(type);
    return zr_owner_value(analysis,
            analysis->function->operands[instruction->operands.start + at], type);
}

static TZrBool zr_owner_supported(const SZrExecIrInstruction *instruction,
                                  TZrExecIrTypeToken type) {
    if (!zr_owner_type(type) || instruction->results.count != 1u) return ZR_FALSE;
    if (instruction->opcode == ZR_EXEC_IR_OPCODE_COMPARE)
        return (TZrBool)(type == ZR_VALUE_TYPE_BOOL &&
                         instruction->typeToken <= ZR_EXEC_IR_COMPARE_KIND_NOT_EQUAL);
    if (instruction->typeToken != 0u && instruction->typeToken != type) return ZR_FALSE;
    switch (instruction->opcode) {
        case ZR_EXEC_IR_OPCODE_CONSTANT:
        case ZR_EXEC_IR_OPCODE_COPY:
        case ZR_EXEC_IR_OPCODE_MOVE: return ZR_TRUE;
        case ZR_EXEC_IR_OPCODE_CONVERT:
        case ZR_EXEC_IR_OPCODE_ADD:
        case ZR_EXEC_IR_OPCODE_ARITHMETIC:
        case ZR_EXEC_IR_OPCODE_SUB:
        case ZR_EXEC_IR_OPCODE_MUL:
        case ZR_EXEC_IR_OPCODE_DIV:
        case ZR_EXEC_IR_OPCODE_NEG: return (TZrBool)(type == ZR_VALUE_TYPE_INT64);
        default: return ZR_FALSE;
    }
}

/* A definition field of zero is permitted for ordinary results. Discover real
 * definitions from result/PHI rows; all missing, external and unsupported
 * producers stay TOP. Reject conflicts instead of strengthening those seeds. */
static TZrBool zr_owner_define(SZrSccpOwnerAnalysis *analysis, TZrUInt8 *defined,
                               TZrExecIrValueId id, TZrBool supported) {
    const SZrExecIrFunction *function = analysis->function;
    if (!zr_owner_work(analysis, 1u)) return ZR_FALSE;
    if (id == ZR_EXEC_IR_VALUE_ID_INVALID || id > function->valueCount ||
        defined[id] != 0u ||
        (function->values[id - 1u].flags & ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u)
        return zr_owner_invalid(analysis, 0u, function->valueCount, id);
    defined[id] = 1u;
    if (supported) analysis->values[id] = zr_owner_bottom();
    return ZR_TRUE;
}

static TZrBool zr_owner_seed(SZrSccpOwnerAnalysis *analysis, TZrUInt8 *defined) {
    SZrExecIrFunction *function = analysis->function;
    TZrUInt32 index, at;
    for (index = 0u; index < function->valueCount; ++index) {
        if (!zr_owner_work(analysis, 1u)) return ZR_FALSE;
        analysis->values[index + 1u] = zr_owner_top(function->values[index].typeToken);
    }
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        if (!zr_owner_work(analysis, 1u)) return ZR_FALSE;
        for (at = 0u; at < instruction->results.count; ++at) {
            TZrExecIrValueId id = function->results[instruction->results.start + at];
            TZrBool supported = zr_owner_supported(instruction, function->values[id - 1u].typeToken);
            if (!zr_owner_define(analysis, defined, id, supported)) return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->phiCount; ++index) {
        TZrExecIrValueId id = function->phiPool[index].result;
        if (!zr_owner_define(analysis, defined, id,
                zr_owner_type(function->values[id - 1u].typeToken))) return ZR_FALSE;
    }
    return ZR_TRUE;
}

static SZrSccpLattice zr_owner_transfer(SZrSccpOwnerAnalysis *analysis,
                                       const SZrExecIrInstruction *instruction,
                                       TZrExecIrTypeToken type) {
    SZrSccpLattice left, right;
    TZrInt64 a, b, output;
    TZrBool valid = ZR_FALSE, comparison;
    if (!zr_owner_supported(instruction, type)) return zr_owner_top(type);
    if (instruction->opcode == ZR_EXEC_IR_OPCODE_CONSTANT) {
        const SZrExecIrConstant *constant;
        if (analysis->context->constants == ZR_NULL ||
            instruction->layoutId >= analysis->context->constantCount) return zr_owner_top(type);
        constant = &analysis->context->constants[instruction->layoutId];
        if (constant->typeToken != type || constant->flags != 0u ||
            (type == ZR_VALUE_TYPE_BOOL && constant->bits > 1u)) return zr_owner_top(type);
        /* ExecBc's owned BOOL decoder only accepts0/1. Core alone does not
         * validate raw BOOL bits; other encodings remain TOP, not truthy. */
        return zr_owner_fact(ZR_EXEC_IR_SCCP_CONSTANT, type, constant->bits);
    }
    left = zr_owner_operand(analysis, instruction, 0u,
            instruction->opcode == ZR_EXEC_IR_OPCODE_COMPARE ? ZR_VALUE_TYPE_INT64 : type);
    switch (instruction->opcode) {
        case ZR_EXEC_IR_OPCODE_COPY:
        case ZR_EXEC_IR_OPCODE_MOVE:
        case ZR_EXEC_IR_OPCODE_CONVERT: return left;
        case ZR_EXEC_IR_OPCODE_NEG:
            if (left.kind != ZR_EXEC_IR_SCCP_CONSTANT) return left;
            a = (TZrInt64)left.bits;
            return a == INT64_MIN ? zr_owner_top(type) :
                    zr_owner_fact(ZR_EXEC_IR_SCCP_CONSTANT, type, (TZrUInt64)(-a));
        default: break;
    }
    right = zr_owner_operand(analysis, instruction, 1u, ZR_VALUE_TYPE_INT64);
    /* TOP dominates a still unresolved operand. This is independent of the
     * legacy numeric UNKNOWN join and never launders a type retag. */
    if (left.kind == ZR_EXEC_IR_SCCP_OVERDEFINED ||
        right.kind == ZR_EXEC_IR_SCCP_OVERDEFINED) return zr_owner_top(type);
    if (left.kind == ZR_EXEC_IR_SCCP_UNKNOWN || right.kind == ZR_EXEC_IR_SCCP_UNKNOWN)
        return zr_owner_bottom();
    a = (TZrInt64)left.bits;
    b = (TZrInt64)right.bits;
    switch (instruction->opcode) {
        case ZR_EXEC_IR_OPCODE_COMPARE:
            switch (instruction->typeToken) {
                case ZR_EXEC_IR_COMPARE_KIND_EQUAL: comparison = (TZrBool)(a == b); break;
                case ZR_EXEC_IR_COMPARE_KIND_LESS: comparison = (TZrBool)(a < b); break;
                case ZR_EXEC_IR_COMPARE_KIND_LESS_EQUAL: comparison = (TZrBool)(a <= b); break;
                case ZR_EXEC_IR_COMPARE_KIND_GREATER: comparison = (TZrBool)(a > b); break;
                case ZR_EXEC_IR_COMPARE_KIND_GREATER_EQUAL: comparison = (TZrBool)(a >= b); break;
                case ZR_EXEC_IR_COMPARE_KIND_NOT_EQUAL: comparison = (TZrBool)(a != b); break;
                default: return zr_owner_top(type);
            }
            return zr_owner_fact(ZR_EXEC_IR_SCCP_CONSTANT, type, (TZrUInt64)comparison);
        case ZR_EXEC_IR_OPCODE_ADD:
        case ZR_EXEC_IR_OPCODE_ARITHMETIC: valid = zr_sccp_add(a, b, &output); break;
        case ZR_EXEC_IR_OPCODE_SUB: valid = zr_sccp_sub(a, b, &output); break;
        case ZR_EXEC_IR_OPCODE_MUL: valid = zr_sccp_mul(a, b, &output); break;
        case ZR_EXEC_IR_OPCODE_DIV: valid = zr_sccp_div(a, b, &output); break;
        default: return zr_owner_top(type);
    }
    if (!valid) {
        analysis->context->lastReasonCode = ZR_EXEC_IR_PASS_REASON_WILL_THROW;
        return zr_owner_top(type);
    }
    /* A normal DIV result may feed pure consumers; DIV/effects are never
     * removed by the owner rewrite gate in the orchestration TU. */
    return zr_owner_fact(ZR_EXEC_IR_SCCP_CONSTANT, type, (TZrUInt64)output);
}

/* PHI slot is a predecessor occurrence, not a (source,target) pair. Count
 * that source in the target row, then select the corresponding target in
 * its source successor row. Every examined slot consumes real work. */
static TZrBool zr_owner_phi_edge(SZrSccpOwnerAnalysis *analysis,
                                 const SZrExecIrBlock *target, TZrUInt32 slot,
                                 TZrExecIrBlockId predecessor, TZrBool *live) {
    const SZrExecIrFunction *function = analysis->function;
    const SZrExecIrBlock *source;
    TZrUInt32 at, occurrence = 0u, matched = 0u;
    *live = ZR_FALSE;
    if (slot >= target->predecessors.count || predecessor == 0u ||
        predecessor > function->blockCount ||
        function->predecessors[target->predecessors.start + slot] != predecessor)
        return zr_owner_invalid(analysis, target->id, target->predecessors.count, slot);
    source = &function->blocks[predecessor - 1u];
    for (at = 0u; at <= slot; ++at) {
        if (!zr_owner_work(analysis, 1u)) return ZR_FALSE;
        if (function->predecessors[target->predecessors.start + at] == predecessor) ++occurrence;
    }
    for (at = 0u; at < source->successors.count; ++at) {
        if (!zr_owner_work(analysis, 1u)) return ZR_FALSE;
        if (function->successors[source->successors.start + at] != target->id) continue;
        if (++matched == occurrence) {
            *live = (TZrBool)(analysis->edges[source->successors.start + at] != 0u);
            return ZR_TRUE;
        }
    }
    return zr_owner_invalid(analysis, target->id, occurrence, matched);
}

static TZrBool zr_owner_phi(SZrSccpOwnerAnalysis *analysis,
                            const SZrExecIrBlock *block, const SZrExecIrPhi *phi,
                            TZrBool *changed) {
    TZrUInt32 at;
    TZrExecIrTypeToken type = analysis->function->values[phi->result - 1u].typeToken;
    SZrSccpLattice merged = zr_owner_bottom();
    if (!zr_owner_work(analysis, 1u)) return ZR_FALSE;
    if (phi->incomings.count != block->predecessors.count)
        return zr_owner_invalid(analysis, block->id, block->predecessors.count, phi->incomings.count);
    for (at = 0u; at < phi->incomings.count; ++at) {
        const SZrExecIrPhiIncoming *incoming =
                &analysis->function->phiIncoming[phi->incomings.start + at];
        TZrBool live;
        if (!zr_owner_work(analysis, 1u) ||
            !zr_owner_phi_edge(analysis, block, at, incoming->predecessor, &live)) return ZR_FALSE;
        if (live) merged = zr_owner_join(merged,
                zr_owner_value(analysis, incoming->value, type), type);
    }
    if (zr_owner_update(analysis, phi->result, merged)) *changed = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool zr_owner_instruction(SZrSccpOwnerAnalysis *analysis,
                                    const SZrExecIrInstruction *instruction,
                                    TZrBool *changed) {
    TZrExecIrValueId id;
    if (!zr_owner_work(analysis, (TZrUInt64)instruction->operands.count + 1u)) return ZR_FALSE;
    if (instruction->results.count != 1u) return ZR_TRUE;
    id = analysis->function->results[instruction->results.start];
    if (zr_owner_update(analysis, id, zr_owner_transfer(analysis, instruction,
            analysis->function->values[id - 1u].typeToken))) *changed = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool zr_owner_successors(SZrSccpOwnerAnalysis *analysis,
                                   const SZrExecIrBlock *block,
                                   const SZrExecIrInstruction *instruction,
                                   TZrBool *changed) {
    TZrUInt32 at;
    TZrBool conditional = (TZrBool)(instruction->opcode == ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH);
    SZrSccpLattice condition = zr_owner_top(ZR_VALUE_TYPE_BOOL);
    if (!zr_owner_work(analysis, 1u)) return ZR_FALSE;
    if (instruction->successorRange.count != block->successors.count)
        return zr_owner_invalid(analysis, block->id, block->successors.count,
                                instruction->successorRange.count);
    if (conditional) {
        if (block->successors.count != 2u) return zr_owner_invalid(analysis, block->id, 2u, block->successors.count);
        condition = zr_owner_operand(analysis, instruction, 0u, ZR_VALUE_TYPE_BOOL);
    }
    for (at = 0u; at < block->successors.count; ++at) {
        TZrUInt32 edge = block->successors.start + at;
        TZrExecIrBlockId target = analysis->function->successors[edge];
        if (!zr_owner_work(analysis, 1u)) return ZR_FALSE;
        /* Core permits distinct flat rows with equal ordered contents. Use
         * the block row slot while preserving the terminator ordinal. */
        if (analysis->function->successors[instruction->successorRange.start + at] != target)
            return zr_owner_invalid(analysis, block->id, target,
                    analysis->function->successors[instruction->successorRange.start + at]);
        if (conditional && condition.kind == ZR_EXEC_IR_SCCP_UNKNOWN) continue;
        if (conditional && condition.kind == ZR_EXEC_IR_SCCP_CONSTANT &&
            at != (condition.bits != 0u ? 0u : 1u)) continue;
        if (analysis->edges[edge] == 0u) { analysis->edges[edge] = 1u; *changed = ZR_TRUE; }
        if (analysis->blocks[target] == 0u) { analysis->blocks[target] = 1u; *changed = ZR_TRUE; }
    }
    return ZR_TRUE;
}

static TZrBool zr_owner_scan(SZrSccpOwnerAnalysis *analysis, TZrBool *changed) {
    const SZrExecIrFunction *function = analysis->function;
    TZrUInt32 at, index;
    if (function->blockCount == 0u) {
        for (at = 0u; at < function->instructionCount; ++at)
            if (!zr_owner_instruction(analysis, &function->instructions[at], changed)) return ZR_FALSE;
        return ZR_TRUE;
    }
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        if (!zr_owner_work(analysis, 1u)) return ZR_FALSE;
        if (analysis->blocks[block->id] == 0u) continue;
        for (at = 0u; at < block->phis.count; ++at)
            if (!zr_owner_phi(analysis, block, &function->phiPool[block->phis.start + at], changed))
                return ZR_FALSE;
        for (at = 0u; at < block->instructions.count; ++at)
            if (!zr_owner_instruction(analysis,
                    &function->instructions[block->instructions.start + at], changed)) return ZR_FALSE;
        if (block->instructions.count != 0u) {
            TZrUInt32 end = block->instructions.start + block->instructions.count;
            if (block->terminatorInstructionId != ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
                block->terminatorInstructionId != end)
                return zr_owner_invalid(analysis, block->id, end, block->terminatorInstructionId);
            if (!zr_owner_successors(analysis, block, &function->instructions[end - 1u], changed))
                return ZR_FALSE;
        } else if (block->successors.count != 0u)
            return zr_owner_invalid(analysis, block->id, 0u, block->successors.count);
    }
    return ZR_TRUE;
}

/* At a stalled fixed point, an unresolved executable condition must not hide
 * feasible arms forever. Raise it to TOP, add both slots and solve again. */
static TZrBool zr_owner_residual(SZrSccpOwnerAnalysis *analysis, TZrBool *changed) {
    const SZrExecIrFunction *function = analysis->function;
    TZrUInt32 index;
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        const SZrExecIrInstruction *instruction;
        TZrExecIrValueId id;
        if (!zr_owner_work(analysis, 1u)) return ZR_FALSE;
        if (analysis->blocks[block->id] == 0u || block->instructions.count == 0u) continue;
        instruction = &function->instructions[
                block->instructions.start + block->instructions.count - 1u];
        if (instruction->opcode != ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH) continue;
        id = function->operands[instruction->operands.start];
        if (analysis->values[id].kind == ZR_EXEC_IR_SCCP_UNKNOWN) {
            if (zr_owner_update(analysis, id, zr_owner_top(function->values[id - 1u].typeToken)))
                *changed = ZR_TRUE;
            if (!zr_owner_successors(analysis, block, instruction, changed)) return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_ComputeOwnerSccp(SZrExecIrFunction *function,
        SZrExecIrPassContext *context, SZrSccpLattice *lattice,
        TZrUInt8 *blocks, TZrUInt32 *edges, TZrBool *complete,
        SZrExecIrDiagnostic *diagnostic) {
    SZrSccpOwnerAnalysis analysis = {function, context, lattice, blocks, edges, diagnostic, ZR_FALSE};
    TZrUInt8 *defined;
    TZrUInt64 round, rounds = 2u * (TZrUInt64)function->valueCount +
            (TZrUInt64)function->successorCount + (TZrUInt64)function->blockCount + 2u;
    TZrBool changed;
    *complete = ZR_FALSE;
    if (zr_sccp_size_mul_overflow((TZrUInt64)function->valueCount + 1u, sizeof(*defined))) {
        ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                function, 0u, 0u, UINT32_MAX, function->valueCount);
        return ZR_FALSE;
    }
    defined = (TZrUInt8 *)calloc((size_t)((TZrUInt64)function->valueCount + 1u), sizeof(*defined));
    if (defined == ZR_NULL) {
        ZrParser_ExecIr_PassDiagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                function, 0u, 0u, function->valueCount, 0u);
        return ZR_FALSE;
    }
    if (!zr_owner_seed(&analysis, defined)) {
        free(defined);
        return analysis.interrupted;
    }
    free(defined);
    if (function->blockCount != 0u) blocks[function->entryBlockId] = 1u;
    /* At most two rises/value, one/edge and one/block. The extra two scans
     * cover the final unchanged scan, including residual-bottom fallback.
     * A defensive cap never publishes facts, even with an unlimited budget. */
    for (round = 0u; round < rounds; ++round) {
        changed = ZR_FALSE;
        if (!zr_owner_scan(&analysis, &changed)) return analysis.interrupted;
        if (!changed && !zr_owner_residual(&analysis, &changed)) return analysis.interrupted;
        if (!changed) { *complete = ZR_TRUE; break; }
    }
    return ZR_TRUE;
}
