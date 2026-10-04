#include "zr_vm_parser/exec_ir_branch_facts.h"
#include "exec_ir_branch_intervals.h"
#include "exec_ir_branch_loop_ranges.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void ZrParser_ExecIr_BranchFactsInit(SZrExecIrBranchFacts *r) {
    if (r != ZR_NULL) memset(r, 0, sizeof(*r));
}
void ZrParser_ExecIr_BranchFactsFree(SZrExecIrBranchFacts *r) {
    if (r == ZR_NULL) return;
    free(r->blockEntries); free(r->blockExits); free(r->edges);
    free(r->blockReachable); free(r->edgeReachable);
    free(r->edgeSources); free(r->edgeOrdinals);
    free(r->signedWitnesses); free(r->entryWitnesses);
    memset(r, 0, sizeof(*r));
}
static TZrBool fail(SZrExecIrDiagnostic *d, EZrExecutionDiagnosticCode code,
        const SZrExecIrFunction *f, TZrExecIrBlockId b,
        TZrExecIrInstructionId instruction, TZrUInt32 actual) {
    if (d) {
        memset(d, 0, sizeof(*d)); d->code = code;
        d->functionToken = f ? f->functionToken : 0u;
        d->blockId = b; d->instructionId = instruction; d->actualVersion = actual;
        if (f && instruction && instruction <= f->instructionCount)
            d->sourceId = f->instructions[instruction - 1u].sourceId;
    }
    return ZR_FALSE;
}
static TZrBool product(size_t a, size_t b, size_t *out) {
    if (b && a > SIZE_MAX / b) return ZR_FALSE;
    *out = a * b; return ZR_TRUE;
}
static void *allocate(size_t count, size_t width, SZrExecIrDiagnostic *d) {
    size_t bytes;
    void *p;
    if (!product(count, width, &bytes)) {
        if (d) d->code = ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW;
        return ZR_NULL;
    }
    p = calloc(1u, bytes ? bytes : 1u);
    if (!p && d) d->code = ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;
    return p;
}
static TZrBool valid_range(SZrExecIrRange r, TZrUInt32 count) {
    return (TZrBool)(r.offset <= count && r.count <= count - r.offset);
}

static TZrBool bind_verify(const SZrExecIrModule *m,
        const SZrExecIrFunction *f, SZrExecIrDiagnostic *d) {
    size_t bytes;
    TZrBool bound = ZR_FALSE;
    if (!m || !f || !m->id || !f->id || m->functionCount > m->functionCapacity ||
        (m->functionCount && !m->functions) || m->constantCount > m->constantCapacity ||
        (m->constantCount && !m->constants))
        return fail(d, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, ZR_NULL, 0u, 0u, 0u);
    if (!product(m->functionCount, sizeof(*m->functions), &bytes) ||
        !product(m->constantCount, sizeof(*m->constants), &bytes))
        return fail(d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, ZR_NULL, 0u, 0u, 0u);
    for (TZrUInt32 i = 0u; i < m->functionCount; ++i) {
        if (&m->functions[i] == f) bound = ZR_TRUE;
        if (m->functions[i].id == f->id && &m->functions[i] != f)
            return fail(d, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, ZR_NULL, 0u, 0u, f->id);
    }
    if (!bound) return fail(d, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, ZR_NULL, 0u, 0u, f->id);
#define CHECK_STORAGE(field, count, capacity) do { \
    if (f->count > f->capacity || (f->count && !f->field)) \
        return fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, ZR_NULL, 0u, 0u, f->count); \
    if (!product(f->count, sizeof(*f->field), &bytes)) \
        return fail(d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, ZR_NULL, 0u, 0u, f->count); \
} while (0)
    CHECK_STORAGE(values, valueCount, valueCapacity);
    CHECK_STORAGE(instructions, instructionCount, instructionCapacity);
    CHECK_STORAGE(blocks, blockCount, blockCapacity);
    CHECK_STORAGE(operands, operandCount, operandCapacity);
    CHECK_STORAGE(results, resultCount, resultCapacity);
    CHECK_STORAGE(successors, successorCount, successorCapacity);
    CHECK_STORAGE(predecessors, predecessorCount, predecessorCapacity);
    CHECK_STORAGE(phiPool, phiCount, phiCapacity);
    CHECK_STORAGE(phiIncoming, phiIncomingCount, phiIncomingCapacity);
    CHECK_STORAGE(memoryTokenPool, memoryTokenCount, memoryTokenCapacity);
    CHECK_STORAGE(gcMaps, gcMapCount, gcMapCapacity);
    CHECK_STORAGE(gcRoots, gcRootCount, gcRootCapacity);
    CHECK_STORAGE(deoptStates, deoptStateCount, deoptStateCapacity);
    CHECK_STORAGE(deoptValues, deoptValueCount, deoptValueCapacity);
    CHECK_STORAGE(deoptAggregates, deoptAggregateCount, deoptAggregateCapacity);
    CHECK_STORAGE(deoptAggregateFields, deoptAggregateFieldCount, deoptAggregateFieldCapacity);
    CHECK_STORAGE(sourceMaps, sourceMapCount, sourceMapCapacity);
    CHECK_STORAGE(bindingRows, bindingRowCount, bindingRowCapacity);
#undef CHECK_STORAGE
    /* Preserve use-site diagnostics when malformed pool IDs would otherwise
     * fail Core's pool-wide validation without an instruction location. */
    for (TZrUInt32 i = 0u; i < f->instructionCount; ++i) {
        const SZrExecIrInstruction *op = &f->instructions[i];
        if (!valid_range(op->operands, f->operandCount) ||
            !valid_range(op->results, f->resultCount))
            return fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, f, 0u, i + 1u, op->operands.count);
        for (TZrUInt32 j = 0u; j < op->operands.count; ++j) {
            TZrExecIrValueId id = f->operands[op->operands.offset + j];
            if (!id || id > f->valueCount)
                return fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, 0u, i + 1u, id);
        }
        if (m->constantCount && op->opcode == ZR_EXEC_IR_OPCODE_CONSTANT &&
                op->layoutId >= m->constantCount)
            return fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, 0u, i + 1u, op->layoutId);
    }
    for (TZrUInt32 i = 0u; i < f->blockCount; ++i) {
        const SZrExecIrBlock *b = &f->blocks[i];
        if (!valid_range(b->successors, f->successorCount))
            return fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, f, b->id, 0u, b->successors.count);
        for (TZrUInt32 j = 0u; j < b->successors.count; ++j) {
            TZrExecIrBlockId id = f->successors[b->successors.offset + j];
            if (!id || id > f->blockCount)
                return fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f, b->id, 0u, id);
        }
    }
    /* Existing Core verifier owns ranges, opcode arities and SSA dominance. */
    if (!ZrCore_ExecIr_VerifyFunction(f,
            (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE | ZR_EXEC_IR_VERIFY_SSA), d)) return ZR_FALSE;
    if (!f->blockCount || !f->entryBlockId || f->entryBlockId > f->blockCount)
        return fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f, f->entryBlockId, 0u, f->entryBlockId);
    for (TZrUInt32 i = 0u; i < f->valueCount; ++i)
        if (f->values[i].id != i + 1u)
            return fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, 0u, 0u, f->values[i].id);
    for (TZrUInt32 i = 0u; i < f->blockCount; ++i) {
        const SZrExecIrBlock *b = &f->blocks[i];
        if (b->id != i + 1u || !valid_range(b->successors, f->successorCount) ||
            !valid_range(b->predecessors, f->predecessorCount))
            return fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f, b->id, 0u, b->id);
        if (!b->instructions.count || b->terminatorInstructionId !=
                b->instructions.offset + b->instructions.count)
            return fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f, b->id, 0u, b->terminatorInstructionId);
        for (TZrUInt32 j = 0u; j < b->successors.count; ++j) {
            TZrExecIrBlockId target = f->successors[b->successors.offset + j];
            TZrUInt32 outgoing = 0u, incoming = 0u;
            const SZrExecIrBlock *to;
            if (!target || target > f->blockCount)
                return fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f, b->id, 0u, target);
            to = &f->blocks[target - 1u];
            for (TZrUInt32 p = 0u; p < b->successors.count; ++p)
                if (f->successors[b->successors.offset + p] == target) ++outgoing;
            for (TZrUInt32 p = 0u; p < to->predecessors.count; ++p)
                if (f->predecessors[to->predecessors.offset + p] == b->id) ++incoming;
            if (outgoing != incoming)
                return fail(d, ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH, f, target, 0u, incoming);
        }
        for (TZrUInt32 j = 0u; j < b->predecessors.count; ++j) {
            TZrExecIrBlockId pred = f->predecessors[b->predecessors.offset + j];
            TZrUInt32 outgoing = 0u, incoming = 0u;
            const SZrExecIrBlock *from;
            if (!pred || pred > f->blockCount)
                return fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, f, b->id, 0u, pred);
            from = &f->blocks[pred - 1u];
            for (TZrUInt32 p = 0u; p < b->predecessors.count; ++p)
                if (f->predecessors[b->predecessors.offset + p] == pred) ++incoming;
            for (TZrUInt32 p = 0u; p < from->successors.count; ++p)
                if (f->successors[from->successors.offset + p] == b->id) ++outgoing;
            if (outgoing != incoming)
                return fail(d, ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH, f, b->id, 0u, outgoing);
        }
    }
    /* Edge storage must represent occurrences exactly once. */
    for (TZrUInt32 e = 0u; e < f->successorCount; ++e) {
        TZrUInt32 owners = 0u;
        for (TZrUInt32 i = 0u; i < f->blockCount; ++i) {
            SZrExecIrRange range = f->blocks[i].successors;
            if (e >= range.offset && e - range.offset < range.count) ++owners;
        }
        if (owners != 1u) return fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, f, 0u, 0u, e);
    }
    return ZR_TRUE;
}
static TZrUInt64 hash_bytes(TZrUInt64 h, const void *p, size_t n) {
    const unsigned char *bytes = (const unsigned char *)p;
    for (size_t i = 0u; i < n; ++i) { h ^= bytes[i]; h *= UINT64_C(1099511628211); }
    return h;
}
/* Validated storage only. Padding can cause conservative cache misses. */
static TZrUInt64 fingerprint(const SZrExecIrModule *m, const SZrExecIrFunction *f) {
    TZrUInt64 h = UINT64_C(14695981039346656037);
#define HF(o, field) h = hash_bytes(h, &(o)->field, sizeof((o)->field))
#define HA(o, field, count) do { HF(o, count); h = hash_bytes(h, (o)->field, \
        (size_t)(o)->count * sizeof(*(o)->field)); } while (0)
    HF(m, id); HF(m, moduleToken); HF(m, moduleHash); HF(m, contract);
    HF(m, functionCount); HA(m, constants, constantCount);
    HF(f, id); HF(f, functionToken); HF(f, signatureHash); HF(f, entryBlockId); HF(f, contract);
    HA(f, values, valueCount); HA(f, instructions, instructionCount);
    HA(f, blocks, blockCount); HA(f, operands, operandCount); HA(f, results, resultCount);
    HA(f, successors, successorCount); HA(f, predecessors, predecessorCount);
    HA(f, phiPool, phiCount); HA(f, phiIncoming, phiIncomingCount);
#undef HA
#undef HF
    return h;
}
static TZrBool allocate_result(SZrExecIrBranchFacts *r,
        const SZrExecIrFunction *f, SZrExecIrDiagnostic *d) {
    size_t cells;
    if (!product(f->blockCount, f->valueCount, &cells))
        return fail(d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, f, 0u, 0u, 0u);
    r->blockEntries = allocate(cells, sizeof(*r->blockEntries), d);
    r->blockExits = allocate(cells, sizeof(*r->blockExits), d);
    if (!product(f->successorCount, f->valueCount, &cells))
        return fail(d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, f, 0u, 0u, 0u);
    r->edges = allocate(cells, sizeof(*r->edges), d);
    r->blockReachable = allocate(f->blockCount, sizeof(*r->blockReachable), d);
    r->edgeReachable = allocate(f->successorCount, sizeof(*r->edgeReachable), d);
    r->edgeSources = allocate(f->successorCount, sizeof(*r->edgeSources), d);
    r->edgeOrdinals = allocate(f->successorCount, sizeof(*r->edgeOrdinals), d);
    r->signedWitnesses = allocate(f->valueCount, sizeof(*r->signedWitnesses), d);
    return (TZrBool)(r->blockEntries && r->blockExits && r->edges && r->blockReachable &&
            r->edgeReachable && r->edgeSources && r->edgeOrdinals && r->signedWitnesses);
}
static TZrBool copy_witnesses(SZrExecIrBranchFacts *r,
        const SZrExecIrFunction *f, const SZrExecIrBranchFactsInput *in, SZrExecIrDiagnostic *d) {
    size_t bytes;
    if ((in->signedValueCount && !in->signedValues) || (in->entryCount && !in->entries))
        return fail(d, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, f, 0u, 0u, 0u);
    if (!product(in->signedValueCount, sizeof(*in->signedValues), &bytes) ||
        !product(in->entryCount, sizeof(*in->entries), &bytes))
        return fail(d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, f, 0u, 0u, 0u);
    for (TZrUInt32 i = 0u; i < in->signedValueCount; ++i) {
        TZrExecIrValueId id = in->signedValues[i];
        if (!id || id > f->valueCount || r->signedWitnesses[id - 1u])
            return fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, 0u, 0u, id);
        r->signedWitnesses[id - 1u] = ZR_TRUE;
    }
    r->entryWitnesses = allocate(in->entryCount, sizeof(*r->entryWitnesses), d);
    if (!r->entryWitnesses) return ZR_FALSE;
    for (TZrUInt32 i = 0u; i < in->entryCount; ++i) {
        const SZrExecIrBranchEntryFact *e = &in->entries[i];
        if (!e->valueId || e->valueId > f->valueCount ||
            f->values[e->valueId - 1u].definition != 0u ||
            !(f->values[e->valueId - 1u].flags & ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) ||
            e->fact.nullState < ZR_EXEC_IR_NULL_FACT_UNKNOWN ||
            e->fact.nullState > ZR_EXEC_IR_NULL_FACT_NULL ||
            (e->fact.nullState == ZR_EXEC_IR_NULL_FACT_NULL &&
             f->values[e->valueId - 1u].nullability == ZR_EXEC_IR_NULLABILITY_NONNULL) ||
            ((e->fact.hasLower || e->fact.hasUpper) && !e->fact.signedInteger) ||
            (e->fact.hasLower && e->fact.hasUpper && e->fact.lower > e->fact.upper))
            return fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, 0u, 0u, e->valueId);
        for (TZrUInt32 j = 0u; j < i; ++j)
            if (in->entries[j].valueId == e->valueId)
                return fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, 0u, 0u, e->valueId);
        r->entryWitnesses[i] = *e; r->entryWitnesses[i].fact.available = ZR_TRUE;
        if (e->fact.signedInteger) r->signedWitnesses[e->valueId - 1u] = ZR_TRUE;
    }
    r->entryWitnessCount = in->entryCount; return ZR_TRUE;
}
static SZrExecIrBranchValueFact join(SZrExecIrBranchValueFact a, SZrExecIrBranchValueFact b) {
    SZrExecIrBranchValueFact out = {0};
    out.available = (TZrBool)(a.available && b.available);
    if (!out.available) return out;
    out.signedInteger = (TZrBool)(a.signedInteger && b.signedInteger);
    out.overflowed = (TZrBool)(a.overflowed || b.overflowed);
    if (out.signedInteger && !out.overflowed) {
        out.hasLower = (TZrBool)(a.hasLower && b.hasLower);
        out.hasUpper = (TZrBool)(a.hasUpper && b.hasUpper);
        out.lower = a.lower < b.lower ? a.lower : b.lower;
        out.upper = a.upper > b.upper ? a.upper : b.upper;
    }
    out.nullState = a.nullState == b.nullState ? a.nullState : ZR_EXEC_IR_NULL_FACT_UNKNOWN;
    return out;
}
static void entry_environment(const SZrExecIrFunction *f,
        const SZrExecIrBranchFacts *r, SZrExecIrBranchValueFact *env) {
    for (TZrUInt32 i = 0u; i < f->valueCount; ++i) {
        if (!(f->values[i].flags & ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY)) continue;
        env[i].available = ZR_TRUE; env[i].signedInteger = r->signedWitnesses[i];
        if (f->values[i].nullability == ZR_EXEC_IR_NULLABILITY_NONNULL)
            env[i].nullState = ZR_EXEC_IR_NULL_FACT_NONNULL;
    }
    for (TZrUInt32 i = 0u; i < r->entryWitnessCount; ++i) {
        const SZrExecIrBranchEntryFact *e = &r->entryWitnesses[i];
        env[e->valueId - 1u] = e->fact;
        env[e->valueId - 1u].signedInteger = r->signedWitnesses[e->valueId - 1u];
        if (f->values[e->valueId - 1u].nullability == ZR_EXEC_IR_NULLABILITY_NONNULL &&
            e->fact.nullState == ZR_EXEC_IR_NULL_FACT_UNKNOWN)
            env[e->valueId - 1u].nullState = ZR_EXEC_IR_NULL_FACT_NONNULL;
    }
}
static TZrBool singleton(SZrExecIrBranchValueFact a) {
    return (TZrBool)(a.available && a.signedInteger && !a.overflowed &&
            a.hasLower && a.hasUpper && a.lower == a.upper);
}
static TZrBool restrict_interval(SZrExecIrBranchValueFact *a, TZrUInt32 p, TZrInt64 c) {
    TZrInt64 bound;
    if (!a->available || !a->signedInteger || a->overflowed) return ZR_TRUE;
    if (p == 0u) {
        if ((a->hasLower && a->lower > c) || (a->hasUpper && a->upper < c)) return ZR_FALSE;
        a->hasLower = a->hasUpper = ZR_TRUE; a->lower = a->upper = c;
    } else if (p == 5u) {
        if (singleton(*a) && a->lower == c) return ZR_FALSE;
        if (a->hasLower && a->lower == c) {
            if (c == INT64_MAX) return ZR_FALSE;
            a->lower = c + 1;
        }
        if (a->hasUpper && a->upper == c) {
            if (c == INT64_MIN) return ZR_FALSE;
            a->upper = c - 1;
        }
    } else if (p == 1u || p == 2u) {
        if (p == 1u && c == INT64_MIN) return ZR_FALSE;
        bound = p == 1u ? c - 1 : c;
        if (!a->hasUpper || bound < a->upper) a->upper = bound;
        a->hasUpper = ZR_TRUE;
    } else {
        if (p == 3u && c == INT64_MAX) return ZR_FALSE;
        bound = p == 3u ? c + 1 : c;
        if (!a->hasLower || bound > a->lower) a->lower = bound;
        a->hasLower = ZR_TRUE;
    }
    return (TZrBool)(!a->hasLower || !a->hasUpper || a->lower <= a->upper);
}
static TZrBool refine(const SZrExecIrFunction *f, const SZrExecIrInstruction *term,
        TZrUInt32 ordinal, SZrExecIrBranchValueFact *env) {
    static const TZrUInt32 inverse[6] = {5u, 4u, 3u, 2u, 1u, 0u};
    TZrExecIrValueId condition, left, right;
    TZrExecIrInstructionId definition;
    const SZrExecIrInstruction *cmp;
    TZrUInt32 p;
    if (term->opcode != ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH || ordinal > 1u) return ZR_TRUE;
    condition = f->operands[term->operands.offset]; definition = f->values[condition - 1u].definition;
    if (!definition) return ZR_TRUE;
    cmp = &f->instructions[definition - 1u];
    if (cmp->opcode != ZR_EXEC_IR_OPCODE_COMPARE || cmp->typeToken > 5u || cmp->flags ||
        cmp->effectIn || cmp->effectOut ||
        cmp->memoryIn.count || cmp->memoryOut.count) return ZR_TRUE;
    left = f->operands[cmp->operands.offset]; right = f->operands[cmp->operands.offset + 1u];
    if (!env[left - 1u].signedInteger || !env[right - 1u].signedInteger ||
        !env[left - 1u].available || !env[right - 1u].available ||
        env[left - 1u].overflowed || env[right - 1u].overflowed) return ZR_TRUE;
    p = ordinal ? inverse[cmp->typeToken] : cmp->typeToken;
    if (p == 5u) {
        /* Unequal intervals need a disjoint-union domain to express a hole.
         * Retain the existing exact singleton/endpoint exclusion behavior. */
        if (singleton(env[right - 1u]))
            return restrict_interval(&env[left - 1u], p, env[right - 1u].lower);
        if (singleton(env[left - 1u]))
            return restrict_interval(&env[right - 1u], p, env[left - 1u].lower);
        return ZR_TRUE;
    }
    return branch_interval_relation(&env[left - 1u], &env[right - 1u], p);
}
static void transfer(const SZrExecIrModule *m, const SZrExecIrFunction *f,
        const SZrExecIrBranchFacts *r, const SZrExecIrInstruction *op, SZrExecIrBranchValueFact *env) {
    for (TZrUInt32 j = 0u; j < op->results.count; ++j) {
        TZrExecIrValueId id = f->results[op->results.offset + j];
        SZrExecIrBranchValueFact a = {0};
        a.available = ZR_TRUE; a.signedInteger = r->signedWitnesses[id - 1u];
        if (op->opcode == ZR_EXEC_IR_OPCODE_COPY || op->opcode == ZR_EXEC_IR_OPCODE_MOVE)
            a = env[f->operands[op->operands.offset] - 1u];
        else if ((op->opcode == ZR_EXEC_IR_OPCODE_ADD || op->opcode == ZR_EXEC_IR_OPCODE_SUB) &&
                 !op->flags && !op->effectIn && !op->effectOut &&
                 !op->memoryIn.count && !op->memoryOut.count) {
            branch_interval_arithmetic((EZrExecIrOpcode)op->opcode,
                    &env[f->operands[op->operands.offset] - 1u],
                    &env[f->operands[op->operands.offset + 1u] - 1u], &a);
        }
        else if (op->opcode == ZR_EXEC_IR_OPCODE_CONSTANT && !m->constantCount &&
                 a.signedInteger && !op->flags) {
            a.hasLower = a.hasUpper = ZR_TRUE; a.lower = a.upper = (TZrInt64)op->layoutId;
        }
        if (op->opcode == ZR_EXEC_IR_OPCODE_CONSTANT && m->constantCount)
            a.signedInteger = ZR_FALSE;
        if (f->values[id - 1u].nullability == ZR_EXEC_IR_NULLABILITY_NONNULL)
            a.nullState = ZR_EXEC_IR_NULL_FACT_NONNULL;
        env[id - 1u] = a;
    }
}
static void merge_phis(const SZrExecIrFunction *f, SZrExecIrBranchFacts *r,
        const SZrExecIrBlock *b, SZrExecIrBranchValueFact *env) {
    for (TZrUInt32 j = 0u; j < b->phis.count; ++j) {
        const SZrExecIrPhi *phi = &f->phiPool[b->phis.offset + j];
        SZrExecIrBranchValueFact a = {0}; TZrBool first = ZR_TRUE;
        for (TZrUInt32 k = 0u; k < phi->incomings.count; ++k) {
            const SZrExecIrPhiIncoming *in = &f->phiIncoming[phi->incomings.offset + k];
            const SZrExecIrBlock *pred = &f->blocks[in->predecessor - 1u];
            /* Parallel occurrences all contribute: predecessor ID alone cannot
             * select a more restrictive true/false occurrence. */
            for (TZrUInt32 n = 0u; n < pred->successors.count; ++n) {
                TZrUInt32 e = pred->successors.offset + n;
                if (f->successors[e] == b->id && r->edgeReachable[e]) {
                    SZrExecIrBranchValueFact v = r->edges[(size_t)e * f->valueCount + in->value - 1u];
                    a = first ? v : join(a, v); first = ZR_FALSE;
                }
            }
        }
        env[phi->result - 1u] = a;
    }
}
static TZrBool run_analysis(const SZrExecIrModule *m, const SZrExecIrFunction *f,
        SZrExecIrBranchFacts *r, SZrExecIrDiagnostic *d) {
    TZrUInt32 *indegree = ZR_NULL, *order = ZR_NULL;
    TZrBool *reachable = ZR_NULL, ok = ZR_FALSE;
    TZrUInt32 count = 0u, cursor = 0u, reachableCount;
    size_t envBytes;
    if (!product(f->valueCount, sizeof(*r->edges), &envBytes))
        return fail(d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, f, 0u, 0u, 0u);
    indegree = allocate(f->blockCount, sizeof(*indegree), d);
    order = allocate(f->blockCount, sizeof(*order), d);
    reachable = allocate(f->blockCount, sizeof(*reachable), d);
    if (!indegree || !order || !reachable) goto done;
    order[count++] = f->entryBlockId - 1u; reachable[f->entryBlockId - 1u] = ZR_TRUE;
    while (cursor < count) {
        const SZrExecIrBlock *b = &f->blocks[order[cursor++]];
        for (TZrUInt32 j = 0u; j < b->successors.count; ++j) {
            TZrUInt32 target = f->successors[b->successors.offset + j] - 1u;
            if (!reachable[target]) { reachable[target] = ZR_TRUE; order[count++] = target; }
        }
    }
    reachableCount = count;
    for (TZrUInt32 i = 0u; i < f->blockCount; ++i) {
        const SZrExecIrBlock *b = &f->blocks[i];
        for (TZrUInt32 j = 0u; j < b->successors.count; ++j) {
            TZrUInt32 e = b->successors.offset + j;
            r->edgeSources[e] = b->id; r->edgeOrdinals[e] = j;
            if (reachable[i]) ++indegree[f->successors[e] - 1u];
        }
    }
    cursor = count = 0u;
    for (TZrUInt32 i = 0u; i < f->blockCount; ++i)
        if (reachable[i] && !indegree[i]) order[count++] = i;
    while (cursor < count) {
        const SZrExecIrBlock *b = &f->blocks[order[cursor++]];
        for (TZrUInt32 j = 0u; j < b->successors.count; ++j) {
            TZrUInt32 target = f->successors[b->successors.offset + j] - 1u;
            if (--indegree[target] == 0u) order[count++] = target;
        }
    }
    if (count != reachableCount) {
        const SZrBranchLoopOperations operations = {entry_environment, transfer, refine, join};
        EZrBranchLoopResult loop = zr_branch_loop_ranges(m, f, r, &operations, d);
        if (loop == ZR_BRANCH_LOOP_ERROR) goto done;
        if (loop == ZR_BRANCH_LOOP_CONVERGED) { ok = ZR_TRUE; goto done; }
        r->cyclicFallback = ZR_TRUE;
        for (TZrUInt32 i = 0u; i < f->blockCount; ++i) {
            const SZrExecIrBlock *b = &f->blocks[i];
            r->blockReachable[i] = reachable[i]; if (!reachable[i]) continue;
            for (TZrUInt32 v = 0u; v < f->valueCount; ++v)
                if (f->values[v].flags & ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY)
                    r->blockEntries[(size_t)i * f->valueCount + v].available = ZR_TRUE;
            for (TZrUInt32 j = 0u; j < b->successors.count; ++j) {
                TZrUInt32 e = b->successors.offset + j; r->edgeReachable[e] = ZR_TRUE;
                for (TZrUInt32 v = 0u; v < f->valueCount; ++v)
                    if (f->values[v].flags & ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY)
                        r->edges[(size_t)e * f->valueCount + v].available = ZR_TRUE;
            }
        }
        ok = ZR_TRUE; goto done;
    }
    for (TZrUInt32 oi = 0u; oi < count; ++oi) {
        TZrUInt32 bi = order[oi]; const SZrExecIrBlock *b = &f->blocks[bi];
        SZrExecIrBranchValueFact *entry = &r->blockEntries[(size_t)bi * f->valueCount];
        SZrExecIrBranchValueFact *exit = &r->blockExits[(size_t)bi * f->valueCount];
        TZrBool first = ZR_TRUE;
        if (b->id == f->entryBlockId) { entry_environment(f, r, entry); first = ZR_FALSE; }
        else for (TZrUInt32 e = 0u; e < f->successorCount; ++e) {
            SZrExecIrBranchValueFact *from;
            if (f->successors[e] != b->id || !r->edgeReachable[e]) continue;
            from = &r->edges[(size_t)e * f->valueCount];
            if (first) memcpy(entry, from, envBytes);
            else for (TZrUInt32 v = 0u; v < f->valueCount; ++v) entry[v] = join(entry[v], from[v]);
            first = ZR_FALSE;
        }
        if (first) continue;
        r->blockReachable[bi] = ZR_TRUE;
        merge_phis(f, r, b, entry); memcpy(exit, entry, envBytes);
        for (TZrUInt32 j = 0u; j < b->instructions.count; ++j) {
            const SZrExecIrInstruction *op = &f->instructions[b->instructions.offset + j];
            if (op->opcode != ZR_EXEC_IR_OPCODE_PHI) transfer(m, f, r, op, exit);
        }
        const SZrExecIrInstruction *term = &f->instructions[b->terminatorInstructionId - 1u];
        for (TZrUInt32 j = 0u; j < b->successors.count; ++j) {
            TZrUInt32 e = b->successors.offset + j;
            SZrExecIrBranchValueFact *to = &r->edges[(size_t)e * f->valueCount];
            memcpy(to, exit, envBytes); r->edgeReachable[e] = refine(f, term, j, to);
            /* Throwing terminator results exist only on the normal edge.
             * Suspension result availability is outside this finite producer. */
            const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)term->opcode);
            if (term->opcode == ZR_EXEC_IR_OPCODE_SUSPEND ||
                (j != 0u && (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW))) {
                for (TZrUInt32 k = 0u; k < term->results.count; ++k)
                    memset(&to[f->results[term->results.offset + k] - 1u], 0, sizeof(*to));
            }
        }
    }
    ok = ZR_TRUE;
done:
    free(indegree); free(order); free(reachable); return ok;
}
TZrBool ZrParser_ExecIr_AnalyzeBranchFacts(const SZrExecIrModule *m,
        const SZrExecIrFunction *f, const SZrExecIrBranchFactsInput *in,
        SZrExecIrBranchFacts *out, SZrExecIrDiagnostic *d) {
    SZrExecIrBranchFacts r = {0};
    if (d) memset(d, 0, sizeof(*d));
    if (!out) return fail(d, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, ZR_NULL, 0u, 0u, 0u);
    ZrParser_ExecIr_BranchFactsFree(out);
    if (!in || !in->revision || !in->generation)
        return fail(d, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, ZR_NULL, 0u, 0u, 0u);
    if (!bind_verify(m, f, d)) return ZR_FALSE;
    r.module = m; r.function = f; r.revision = in->revision; r.generation = in->generation;
    r.blockCount = f->blockCount; r.valueCount = f->valueCount; r.edgeCount = f->successorCount;
    if (!allocate_result(&r, f, d) || !copy_witnesses(&r, f, in, d) || !run_analysis(m, f, &r, d)) {
        ZrParser_ExecIr_BranchFactsFree(&r); return ZR_FALSE;
    }
    r.irHash = fingerprint(m, f); *out = r; return ZR_TRUE;
}
TZrBool ZrParser_ExecIr_BranchFactsIsCurrent(const SZrExecIrModule *m,
        const SZrExecIrFunction *f, const SZrExecIrBranchFacts *r,
        TZrUInt64 revision, TZrUInt64 generation) {
    return (TZrBool)(r && r->function == f && r->module == m && revision && generation &&
        r->revision == revision && r->generation == generation && bind_verify(m, f, ZR_NULL) &&
        r->irHash == fingerprint(m, f));
}
const SZrExecIrBranchValueFact *ZrParser_ExecIr_BranchFactsAtBlockEntry(
        const SZrExecIrBranchFacts *r, TZrExecIrBlockId b, TZrExecIrValueId v) {
    const SZrExecIrBranchValueFact *a;
    if (!r || !r->blockEntries || !b || b > r->blockCount || !v || v > r->valueCount ||
        !r->blockReachable[b - 1u]) return ZR_NULL;
    a = &r->blockEntries[(size_t)(b - 1u) * r->valueCount + v - 1u];
    return a->available ? a : ZR_NULL;
}
const SZrExecIrBranchValueFact *ZrParser_ExecIr_BranchFactsAtEdge(
        const SZrExecIrBranchFacts *r, TZrExecIrBlockId pred, TZrUInt32 ordinal, TZrExecIrValueId v) {
    if (!r || !r->edges || !v || v > r->valueCount) return ZR_NULL;
    for (TZrUInt32 e = 0u; e < r->edgeCount; ++e) {
        if (r->edgeSources[e] == pred && r->edgeOrdinals[e] == ordinal && r->edgeReachable[e]) {
            const SZrExecIrBranchValueFact *a = &r->edges[(size_t)e * r->valueCount + v - 1u];
            return a->available ? a : ZR_NULL;
        }
    }
    return ZR_NULL;
}
