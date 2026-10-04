#include "exec_ir_branch_loop_ranges.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Four blocks, four edges, entries/exits/edges plus two header vectors = 14
 * value vectors. A sweep charges 32 visits/value plus 16 instruction/edge
 * visits. Both phases and final closure sweeps share the finite work budget. */
enum { BR_LOOP_BLOCKS = 4, BR_LOOP_EDGES = 4, BR_LOOP_INSTRUCTIONS = 6,
       BR_LOOP_VECTORS = 14, BR_LOOP_MAX_CELLS = 4096,
       BR_LOOP_MAX_WORK = 65536, BR_LOOP_WIDEN_SWEEPS = 8,
       BR_LOOP_NARROW_SWEEPS = 8, BR_LOOP_WORK_PER_VALUE = 32,
       BR_LOOP_WORK_FIXED = 16 };

typedef struct SLoopShape {
    TZrExecIrBlockId entry, header, body, exit;
    TZrExecIrValueId initial, step, limit, index, next;
    TZrUInt32 entryEdge, bodyEdge, exitEdge, backEdge;
    const SZrExecIrInstruction *compare, *update;
} SLoopShape;

typedef struct SLoopState {
    SZrExecIrBranchFacts view; /* Borrowed witnesses; scratch arrays below. */
    SZrExecIrBranchValueFact *storage, *header, *candidate;
    TZrBool blocks[BR_LOOP_BLOCKS], edges[BR_LOOP_EDGES];
    TZrUInt32 work;
    size_t vectorBytes;
} SLoopState;

static TZrBool plain(const SZrExecIrInstruction *op) {
    return (TZrBool)(!op->flags && !op->effectIn && !op->effectOut &&
            !op->memoryIn.count && !op->memoryOut.count && !op->phiRange.count &&
            !op->bindingRow && !op->deoptId && !op->layoutId);
}
static TZrBool single_predecessor(const SZrExecIrFunction *f,
        const SZrExecIrBlock *block, TZrExecIrBlockId expected) {
    return (TZrBool)(block->predecessors.count == 1u &&
            f->predecessors[block->predecessors.offset] == expected);
}
static TZrBool singleton(const SZrExecIrBranchValueFact *a) {
    return (TZrBool)(a->available && a->signedInteger && !a->overflowed &&
            a->hasLower && a->hasUpper && a->lower == a->upper);
}

/* Matching uses validated CFG/value relationships, never type-token numbers or
 * advisory induction metadata. No arbitrary loop body is speculatively run. */
static TZrBool match(const SZrExecIrFunction *f,
        const SZrExecIrBranchFacts *r, SLoopShape *s) {
    const SZrExecIrBlock *entry, *header, *body, *exit;
    const SZrExecIrPhi *phi;
    TZrExecIrValueId condition, a, b;
    TZrUInt32 instructionMask = 0u;
    if (f->blockCount != BR_LOOP_BLOCKS || f->successorCount != BR_LOOP_EDGES ||
        f->predecessorCount != BR_LOOP_EDGES || f->instructionCount != BR_LOOP_INSTRUCTIONS ||
        f->phiCount != 1u || f->phiIncomingCount != 2u) return ZR_FALSE;
    memset(s, 0, sizeof(*s)); s->entry = f->entryBlockId;
    entry = &f->blocks[s->entry - 1u];
    if (entry->instructions.count != 1u || entry->successors.count != 1u ||
        entry->predecessors.count || entry->phis.count) return ZR_FALSE;
    s->entryEdge = entry->successors.offset;
    s->header = f->successors[s->entryEdge];
    if (s->header == s->entry) return ZR_FALSE;
    header = &f->blocks[s->header - 1u];
    if (header->instructions.count != 2u || header->successors.count != 2u ||
        header->predecessors.count != 2u || header->phis.count != 1u) return ZR_FALSE;
    s->bodyEdge = header->successors.offset; s->exitEdge = s->bodyEdge + 1u;
    s->body = f->successors[s->bodyEdge]; s->exit = f->successors[s->exitEdge];
    if (s->body == s->exit || s->body == s->entry || s->body == s->header ||
        s->exit == s->entry || s->exit == s->header) return ZR_FALSE;
    body = &f->blocks[s->body - 1u]; exit = &f->blocks[s->exit - 1u];
    if (body->instructions.count != 2u || body->successors.count != 1u || body->phis.count ||
        !single_predecessor(f, body, s->header) || exit->instructions.count != 1u ||
        exit->successors.count || exit->phis.count || !single_predecessor(f, exit, s->header))
        return ZR_FALSE;
    s->backEdge = body->successors.offset;
    if (f->successors[s->backEdge] != s->header) return ZR_FALSE;
    /* Four distinct blocks and all six instructions are covered exactly once. */
    for (TZrUInt32 i = 0u; i < f->blockCount; ++i) {
        const SZrExecIrBlock *block = &f->blocks[i];
        if ((block->flags & ~ZR_EXEC_IR_BLOCK_FLAG_ENTRY) || block->effectPhiResult ||
            block->effectPhiIncomings.count) return ZR_FALSE;
        for (TZrUInt32 region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region)
            if (block->memoryPhiResults[region] || block->memoryPhiIncomings[region].count)
                return ZR_FALSE;
        for (TZrUInt32 j = 0u; j < block->instructions.count; ++j) {
            TZrUInt32 id = block->instructions.offset + j;
            TZrUInt32 bit = (TZrUInt32)1u << id;
            if (instructionMask & bit || !plain(&f->instructions[id])) return ZR_FALSE;
            instructionMask |= bit;
        }
    }
    if (instructionMask != (((TZrUInt32)1u << BR_LOOP_INSTRUCTIONS) - 1u)) return ZR_FALSE;
    s->compare = &f->instructions[header->instructions.offset];
    s->update = &f->instructions[body->instructions.offset];
    if (s->compare->successorRange.count || s->update->successorRange.count) return ZR_FALSE;
    if (f->instructions[entry->instructions.offset].opcode != ZR_EXEC_IR_OPCODE_BRANCH ||
        s->compare->opcode != ZR_EXEC_IR_OPCODE_COMPARE || s->compare->typeToken < 1u ||
        s->compare->typeToken > 4u ||
        f->instructions[header->terminatorInstructionId - 1u].opcode != ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH ||
        f->instructions[body->terminatorInstructionId - 1u].opcode != ZR_EXEC_IR_OPCODE_BRANCH ||
        f->instructions[exit->instructions.offset].opcode != ZR_EXEC_IR_OPCODE_RETURN ||
        (s->update->opcode != ZR_EXEC_IR_OPCODE_ADD && s->update->opcode != ZR_EXEC_IR_OPCODE_SUB))
        return ZR_FALSE;
    phi = &f->phiPool[header->phis.offset]; s->index = phi->result;
    s->next = f->results[s->update->results.offset];
    condition = f->results[s->compare->results.offset];
    if (f->operands[f->instructions[header->terminatorInstructionId - 1u].operands.offset] != condition)
        return ZR_FALSE;
    for (TZrUInt32 i = 0u; i < phi->incomings.count; ++i) {
        const SZrExecIrPhiIncoming *in = &f->phiIncoming[phi->incomings.offset + i];
        if (in->predecessor == s->entry) s->initial = in->value;
        else if (in->predecessor != s->body || in->value != s->next) return ZR_FALSE;
    }
    if (!s->initial || s->index == s->next || !r->signedWitnesses[s->index - 1u] ||
        !r->signedWitnesses[s->next - 1u]) return ZR_FALSE;
    a = f->operands[s->update->operands.offset]; b = f->operands[s->update->operands.offset + 1u];
    if (a == s->index) s->step = b;
    else if (s->update->opcode == ZR_EXEC_IR_OPCODE_ADD && b == s->index) s->step = a;
    else return ZR_FALSE;
    a = f->operands[s->compare->operands.offset]; b = f->operands[s->compare->operands.offset + 1u];
    if (a == s->index) s->limit = b;
    else if (b == s->index) s->limit = a;
    else return ZR_FALSE;
    if (s->initial == s->step || s->initial == s->limit || s->step == s->limit) return ZR_FALSE;
    a = f->operands[f->instructions[exit->instructions.offset].operands.offset];
    if (a != s->index) return ZR_FALSE;
    {
        TZrExecIrValueId seeds[3] = {s->initial, s->step, s->limit};
        for (TZrUInt32 i = 0u; i < 3u; ++i)
            if (!(f->values[seeds[i] - 1u].flags & ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) ||
                !r->signedWitnesses[seeds[i] - 1u]) return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool same(SZrExecIrBranchValueFact a, SZrExecIrBranchValueFact b) {
    return (TZrBool)(a.available == b.available && a.signedInteger == b.signedInteger &&
            a.overflowed == b.overflowed && a.nullState == b.nullState &&
            a.hasLower == b.hasLower && a.hasUpper == b.hasUpper &&
            (!a.hasLower || a.lower == b.lower) && (!a.hasUpper || a.upper == b.upper));
}
/* Unavailable is absence of a usable fact, not an unreachable incoming edge.
 * A published available fact must be justified by every candidate path. */
static TZrBool covers(SZrExecIrBranchValueFact published, SZrExecIrBranchValueFact candidate) {
    if (!published.available) return ZR_TRUE;
    if (!candidate.available || (published.signedInteger && !candidate.signedInteger) ||
        (!published.overflowed && candidate.overflowed) ||
        (published.nullState != ZR_EXEC_IR_NULL_FACT_UNKNOWN && published.nullState != candidate.nullState))
        return ZR_FALSE;
    if (published.hasLower && (!candidate.hasLower || published.lower > candidate.lower)) return ZR_FALSE;
    if (published.hasUpper && (!candidate.hasUpper || published.upper < candidate.upper)) return ZR_FALSE;
    return ZR_TRUE;
}
static TZrBool charge(SLoopState *s, TZrUInt32 values) {
    TZrUInt32 amount;
    if (values > (UINT32_MAX - BR_LOOP_WORK_FIXED) / BR_LOOP_WORK_PER_VALUE) return ZR_FALSE;
    amount = values * BR_LOOP_WORK_PER_VALUE + BR_LOOP_WORK_FIXED;
    if (amount > BR_LOOP_MAX_WORK - s->work) return ZR_FALSE;
    s->work += amount; return ZR_TRUE;
}
static SZrExecIrBranchValueFact *block_entry(SLoopState *s, const SZrExecIrFunction *f, TZrExecIrBlockId b) {
    return &s->view.blockEntries[(size_t)(b - 1u) * f->valueCount];
}
static SZrExecIrBranchValueFact *block_exit(SLoopState *s, const SZrExecIrFunction *f, TZrExecIrBlockId b) {
    return &s->view.blockExits[(size_t)(b - 1u) * f->valueCount];
}
static SZrExecIrBranchValueFact *edge(SLoopState *s, const SZrExecIrFunction *f, TZrUInt32 e) {
    return &s->view.edges[(size_t)e * f->valueCount];
}

/* F(header) composes the complete finite loop region. Bottom edges contribute
 * nothing; real UNKNOWN values on reachable edges do contribute to the join. */
static TZrBool evaluate(const SZrExecIrModule *m, const SZrExecIrFunction *f,
        SLoopState *s, const SLoopShape *shape, const SZrBranchLoopOperations *ops) {
    SZrExecIrBranchValueFact *entry, *headerOut, *bodyIn, *bodyOut, *exitIn;
    TZrBool bodyReach, exitReach;
    if (!charge(s, f->valueCount)) return ZR_FALSE;
    memset(s->view.blockEntries, 0, s->vectorBytes * BR_LOOP_BLOCKS);
    memset(s->view.blockExits, 0, s->vectorBytes * BR_LOOP_BLOCKS);
    memset(s->view.edges, 0, s->vectorBytes * BR_LOOP_EDGES);
    memset(s->blocks, 0, sizeof(s->blocks)); memset(s->edges, 0, sizeof(s->edges));
    entry = block_entry(s, f, shape->entry);
    ops->entry(f, &s->view, entry);
    memcpy(block_exit(s, f, shape->entry), entry, s->vectorBytes);
    memcpy(edge(s, f, shape->entryEdge), entry, s->vectorBytes);
    s->blocks[shape->entry - 1u] = s->edges[shape->entryEdge] = ZR_TRUE;
    memcpy(block_entry(s, f, shape->header), s->header, s->vectorBytes);
    headerOut = block_exit(s, f, shape->header);
    memcpy(headerOut, s->header, s->vectorBytes);
    ops->transfer(m, f, &s->view, shape->compare, headerOut);
    memcpy(edge(s, f, shape->bodyEdge), headerOut, s->vectorBytes);
    memcpy(edge(s, f, shape->exitEdge), headerOut, s->vectorBytes);
    bodyReach = ops->refine(f, &f->instructions[f->blocks[shape->header - 1u].terminatorInstructionId - 1u],
            0u, edge(s, f, shape->bodyEdge));
    exitReach = ops->refine(f, &f->instructions[f->blocks[shape->header - 1u].terminatorInstructionId - 1u],
            1u, edge(s, f, shape->exitEdge));
    s->blocks[shape->header - 1u] = ZR_TRUE;
    s->edges[shape->bodyEdge] = bodyReach; s->edges[shape->exitEdge] = exitReach;
    if (bodyReach) {
        bodyIn = block_entry(s, f, shape->body); bodyOut = block_exit(s, f, shape->body);
        memcpy(bodyIn, edge(s, f, shape->bodyEdge), s->vectorBytes);
        memcpy(bodyOut, bodyIn, s->vectorBytes);
        ops->transfer(m, f, &s->view, shape->update, bodyOut);
        memcpy(edge(s, f, shape->backEdge), bodyOut, s->vectorBytes);
        s->blocks[shape->body - 1u] = s->edges[shape->backEdge] = ZR_TRUE;
        for (TZrUInt32 i = 0u; i < f->valueCount; ++i) s->candidate[i] = ops->join(entry[i], bodyOut[i]);
        s->candidate[shape->index - 1u] = ops->join(entry[shape->initial - 1u], bodyOut[shape->next - 1u]);
    } else {
        memcpy(s->candidate, entry, s->vectorBytes);
        s->candidate[shape->index - 1u] = entry[shape->initial - 1u];
    }
    if (exitReach) {
        exitIn = block_entry(s, f, shape->exit);
        memcpy(exitIn, edge(s, f, shape->exitEdge), s->vectorBytes);
        memcpy(block_exit(s, f, shape->exit), exitIn, s->vectorBytes);
        s->blocks[shape->exit - 1u] = ZR_TRUE;
    }
    return ZR_TRUE;
}

static TZrBool closure(const SZrExecIrFunction *f, const SLoopState *s) {
    for (TZrUInt32 i = 0u; i < f->valueCount; ++i)
        if (!covers(s->header[i], s->candidate[i])) return ZR_FALSE;
    return ZR_TRUE;
}
static TZrBool widen(const SZrExecIrFunction *f, SLoopState *s, const SZrBranchLoopOperations *ops) {
    TZrBool changed = ZR_FALSE;
    for (TZrUInt32 i = 0u; i < f->valueCount; ++i) {
        SZrExecIrBranchValueFact old = s->header[i], next = ops->join(old, s->candidate[i]);
        if (old.hasLower && next.hasLower && next.lower < old.lower) next.hasLower = ZR_FALSE;
        if (old.hasUpper && next.hasUpper && next.upper > old.upper) next.hasUpper = ZR_FALSE;
        if (!same(old, next)) changed = ZR_TRUE;
        s->header[i] = next;
    }
    return changed;
}
static TZrBool narrow(const SZrExecIrFunction *f, SLoopState *s) {
    TZrBool changed = ZR_FALSE;
    for (TZrUInt32 i = 0u; i < f->valueCount; ++i) {
        SZrExecIrBranchValueFact old = s->header[i], next = old;
        const SZrExecIrBranchValueFact *candidate = &s->candidate[i];
        /* Never create availability/domain or clear known poison by narrowing. */
        if (old.available && old.signedInteger && !old.overflowed) {
            next.hasLower = candidate->hasLower; next.hasUpper = candidate->hasUpper;
            next.lower = candidate->lower; next.upper = candidate->upper;
        }
        if (!same(old, next)) changed = ZR_TRUE;
        s->header[i] = next;
    }
    return changed;
}

EZrBranchLoopResult zr_branch_loop_ranges(const SZrExecIrModule *m,
        const SZrExecIrFunction *f, SZrExecIrBranchFacts *r,
        const SZrBranchLoopOperations *ops, SZrExecIrDiagnostic *d) {
    SLoopShape shape;
    SLoopState state;
    EZrBranchLoopResult result = ZR_BRANCH_LOOP_UNPROVEN;
    TZrBool stable = ZR_FALSE, seenEdges[BR_LOOP_EDGES] = {0};
    size_t cells, bytes;
    memset(&state, 0, sizeof(state));
    if (!match(f, r, &shape)) return ZR_BRANCH_LOOP_UNSUPPORTED;
    if (f->valueCount > BR_LOOP_MAX_CELLS / BR_LOOP_VECTORS) return ZR_BRANCH_LOOP_UNPROVEN;
    cells = (size_t)f->valueCount * BR_LOOP_VECTORS;
    if (cells > SIZE_MAX / sizeof(*state.storage)) {
        if (d) { memset(d, 0, sizeof(*d)); d->code = ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW;
                 d->functionToken = f->functionToken; }
        return ZR_BRANCH_LOOP_ERROR;
    }
    bytes = cells * sizeof(*state.storage);
    state.storage = (SZrExecIrBranchValueFact *)calloc(1u, bytes);
    if (!state.storage) {
        if (d) { memset(d, 0, sizeof(*d)); d->code = ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;
                 d->functionToken = f->functionToken; }
        return ZR_BRANCH_LOOP_ERROR;
    }
    state.vectorBytes = (size_t)f->valueCount * sizeof(*state.storage);
    state.view = *r;
    state.view.blockEntries = state.storage;
    state.view.blockExits = state.storage + (size_t)BR_LOOP_BLOCKS * f->valueCount;
    state.view.edges = state.storage + (size_t)(2u * BR_LOOP_BLOCKS) * f->valueCount;
    state.header = state.storage + (size_t)(2u * BR_LOOP_BLOCKS + BR_LOOP_EDGES) * f->valueCount;
    state.candidate = state.header + f->valueCount;
    state.view.blockReachable = state.blocks; state.view.edgeReachable = state.edges;
    ops->entry(f, &state.view, state.header);
    if (!singleton(&state.header[shape.initial - 1u]) || !singleton(&state.header[shape.step - 1u]) ||
        !singleton(&state.header[shape.limit - 1u])) { result = ZR_BRANCH_LOOP_UNSUPPORTED; goto done; }
    state.header[shape.index - 1u] = state.header[shape.initial - 1u];
    for (TZrUInt32 sweep = 0u; sweep < BR_LOOP_WIDEN_SWEEPS; ++sweep) {
        if (!evaluate(m, f, &state, &shape, ops)) goto done;
        for (TZrUInt32 e = 0u; e < BR_LOOP_EDGES; ++e) {
            /* A growing header may add reachability, never silently lose a
             * previously propagated occurrence during widening. */
            if (seenEdges[e] && !state.edges[e]) goto done;
            seenEdges[e] = (TZrBool)(seenEdges[e] || state.edges[e]);
        }
        if (!widen(f, &state, ops)) { stable = ZR_TRUE; break; }
    }
    if (!stable || !evaluate(m, f, &state, &shape, ops) || !closure(f, &state)) goto done;
    stable = ZR_FALSE;
    for (TZrUInt32 sweep = 0u; sweep < BR_LOOP_NARROW_SWEEPS; ++sweep) {
        if (!evaluate(m, f, &state, &shape, ops) || !closure(f, &state)) goto done;
        if (!narrow(f, &state)) { stable = ZR_TRUE; break; }
    }
    /* Rebuild all block/edge/PHI equations from the final header, then verify
     * the induction closure before publishing any finite proof. */
    if (!stable || !evaluate(m, f, &state, &shape, ops) || !closure(f, &state)) goto done;
    memcpy(r->blockEntries, state.view.blockEntries, state.vectorBytes * BR_LOOP_BLOCKS);
    memcpy(r->blockExits, state.view.blockExits, state.vectorBytes * BR_LOOP_BLOCKS);
    memcpy(r->edges, state.view.edges, state.vectorBytes * BR_LOOP_EDGES);
    memcpy(r->blockReachable, state.blocks, sizeof(state.blocks));
    memcpy(r->edgeReachable, state.edges, sizeof(state.edges));
    r->cyclicFallback = ZR_FALSE;
    result = ZR_BRANCH_LOOP_CONVERGED;
done:
    free(state.storage); return result;
}
