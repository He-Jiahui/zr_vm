#include "exec_ir_phi_identity.h"
#include "exec_ir_alias_availability.h"

#include <stdlib.h>

enum { ZR_ALIAS_LEAF, ZR_ALIAS_COPY, ZR_ALIAS_PHI };
enum { ZR_ALIAS_UNSEEN, ZR_ALIAS_VISITING, ZR_ALIAS_RESOLVED, ZR_ALIAS_REJECTED };

typedef struct SZrIdentityNode {
    SZrAliasDefinition definition;
    TZrExecIrValueId root;
    TZrExecIrValueId firstRoot;
    TZrUInt32 next;
    TZrUInt8 kind, state, consumed, hasPhi, unequal;
} SZrIdentityNode;

typedef struct SZrIdentitySlot {
    TZrUInt32 owner;
    TZrExecIrValueId replacement;
    TZrExecIrSourceId source;
    TZrUInt8 owners, blocked;
} SZrIdentitySlot;

struct SZrExecIrPhiIdentityPlan {
    SZrExecIrFunction *function;
    SZrExecIrPassContext *context;
    SZrAliasAvailability availability;
    SZrIdentityNode *nodes;
    SZrIdentitySlot *operands;
    SZrIdentitySlot *incomings;
    TZrExecIrValueId *stack;
    TZrExecIrLegacyPhiPredecessor legacy;
    TZrBool freezePhi;
};

static TZrBool zr_identity_work(SZrExecIrPhiIdentityPlan *plan) {
    return ZrParser_ExecIr_PassConsumeBudget(plan->function, plan->context, 1u);
}

static TZrBool zr_identity_exhausted(SZrExecIrPassContext *context) {
    return (TZrBool)(context != ZR_NULL && context->budgetExhausted);
}

static TZrBool zr_identity_frozen(const SZrExecIrFunction *function) {
    return (TZrBool)(function->stateMap != ZR_NULL || function->gcMap != ZR_NULL ||
            function->gcMapCount != 0u || function->gcRootCount != 0u ||
            function->deoptStateCount != 0u || function->deoptValueCount != 0u ||
            function->deoptAggregateCount != 0u || function->deoptAggregateFieldCount != 0u ||
            function->frameLayout != ZR_NULL);
}

static TZrBool zr_identity_value(const SZrExecIrFunction *function,
        TZrExecIrValueId value) {
    const SZrExecIrValue *descriptor;
    if (value == 0u || value > function->valueCount) return ZR_FALSE;
    descriptor = &function->values[value - 1u];
    return (TZrBool)(descriptor->id == value &&
            descriptor->ownership == ZR_EXEC_IR_OWNERSHIP_UNKNOWN &&
            (descriptor->flags & (ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS |
                                 ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE)) == 0u);
}

static TZrBool zr_identity_same_facts(const SZrExecIrFunction *function,
        TZrExecIrValueId left, TZrExecIrValueId right) {
    return (TZrBool)(zr_identity_value(function, left) && zr_identity_value(function, right) &&
            function->values[left - 1u].typeToken == function->values[right - 1u].typeToken &&
            function->values[left - 1u].nullability == function->values[right - 1u].nullability);
}

static TZrBool zr_identity_copy(const SZrExecIrFunction *function,
        const SZrExecIrInstruction *instruction) {
    if (instruction->opcode != ZR_EXEC_IR_OPCODE_COPY ||
        instruction->operands.count != 1u || instruction->results.count != 1u ||
        instruction->flags != 0u || instruction->effectIn != 0u ||
        instruction->effectOut != 0u || instruction->memoryIn.count != 0u ||
        instruction->memoryOut.count != 0u) return ZR_FALSE;
    return zr_identity_same_facts(function,
            function->operands[instruction->operands.start],
            function->results[instruction->results.start]);
}

/* Cheap absence check: COPY-only functions retain propagation, while scalar
 * functions without either kind of candidate do not allocate a CFG graph. */
static TZrBool zr_identity_potential(SZrExecIrFunction *function,
        SZrExecIrPassContext *context, TZrBool freeze) {
    TZrUInt32 index;
    TZrBool potential = ZR_FALSE;
    for (index = 0u; index < function->instructionCount; ++index) {
        if (!ZrParser_ExecIr_PassConsumeBudget(function, context, 1u)) return ZR_FALSE;
        if (zr_identity_copy(function, &function->instructions[index])) potential = ZR_TRUE;
    }
    if (freeze || function->phiCount == 0u) return potential;
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        TZrUInt32 at;
        if (!ZrParser_ExecIr_PassConsumeBudget(function, context, 1u)) return ZR_FALSE;
        for (at = 0u; at < block->phis.count; ++at) {
            const SZrExecIrPhi *phi = &function->phiPool[block->phis.start + at];
            if (!ZrParser_ExecIr_PassConsumeBudget(function, context, 1u)) return ZR_FALSE;
            if (phi->incomings.count != 0u &&
                phi->incomings.count == block->predecessors.count &&
                zr_identity_value(function, phi->result)) potential = ZR_TRUE;
        }
    }
    return potential;
}

void ZrParser_ExecIr_PhiIdentityDispose(SZrExecIrPhiIdentityPlan *plan) {
    if (plan == ZR_NULL) return;
    ZrParser_ExecIr_AliasAvailabilityDispose(&plan->availability);
    free(plan->nodes);
    free(plan->operands);
    free(plan->incomings);
    free(plan->stack);
    free(plan);
}

static TZrBool zr_identity_block_slots(SZrExecIrPhiIdentityPlan *plan,
        SZrExecIrRange range) {
    TZrUInt32 at;
    if (!ZrParser_ExecIr_PassRangeValid(range, plan->function->phiIncomingCount)) return ZR_FALSE;
    for (at = 0u; at < range.count; ++at) {
        if (!zr_identity_work(plan)) return ZR_FALSE;
        plan->incomings[range.start + at].blocked = 1u;
    }
    return ZR_TRUE;
}

static void zr_identity_owner(SZrIdentitySlot *slot, TZrUInt32 owner) {
    if (slot->owners < 2u) ++slot->owners;
    slot->owner = owner;
}

/* This is collected before any old or new store. Shared operands, token rows
 * and instruction phiRange backing never become writable ordinary slots. */
static TZrBool zr_identity_collect(SZrExecIrPhiIdentityPlan *plan) {
    SZrExecIrFunction *function = plan->function;
    TZrUInt32 index;
    for (index = 0u; index < function->valueCount; ++index) {
        const SZrExecIrValue *value = &function->values[index];
        SZrIdentityNode *node = &plan->nodes[index + 1u];
        if (!zr_identity_work(plan)) return ZR_FALSE;
        if (value->id != index + 1u) return ZR_FALSE;
        node->root = value->id;
        node->state = ZR_ALIAS_RESOLVED;
        if ((value->flags & ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u) {
            if (value->definition != 0u) return ZR_FALSE;
            node->definition.kind = ZR_ALIAS_EXTERNAL;
            node->definition.block = function->entryBlockId;
        }
    }
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        TZrUInt32 at;
        TZrBool consuming = (TZrBool)(instruction->opcode == ZR_EXEC_IR_OPCODE_MOVE ||
                instruction->opcode == ZR_EXEC_IR_OPCODE_DROP ||
                instruction->opcode == ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED);
        if (!zr_identity_work(plan) || !zr_identity_block_slots(plan, instruction->phiRange))
            return ZR_FALSE;
        for (at = 0u; at < instruction->operands.count; ++at) {
            TZrUInt32 slot = instruction->operands.start + at;
            TZrExecIrValueId value = function->operands[slot];
            if (!zr_identity_work(plan)) return ZR_FALSE;
            if (value == 0u || value > function->valueCount) return ZR_FALSE;
            zr_identity_owner(&plan->operands[slot], index + 1u);
            if (consuming) plan->nodes[value].consumed = 1u;
        }
        for (at = 0u; at < instruction->results.count; ++at) {
            TZrExecIrValueId value = function->results[instruction->results.start + at];
            SZrIdentityNode *node;
            if (!zr_identity_work(plan)) return ZR_FALSE;
            if (value == 0u || value > function->valueCount) return ZR_FALSE;
            node = &plan->nodes[value];
            if (node->definition.kind != ZR_ALIAS_MISSING ||
                (function->values[value - 1u].definition != 0u &&
                 function->values[value - 1u].definition != index + 1u)) return ZR_FALSE;
            node->definition.kind = ZR_ALIAS_INSTRUCTION;
            node->definition.instruction = index + 1u;
            node->definition.block = plan->availability.instructionBlocks[index + 1u];
        }
    }
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        TZrUInt32 at, region;
        if (!zr_identity_work(plan) ||
            !zr_identity_block_slots(plan, block->effectPhiIncomings)) return ZR_FALSE;
        for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
            if (!zr_identity_work(plan) ||
                !zr_identity_block_slots(plan, block->memoryPhiIncomings[region])) return ZR_FALSE;
        }
        for (at = 0u; at < block->phis.count; ++at) {
            TZrUInt32 phiIndex = block->phis.start + at;
            const SZrExecIrPhi *phi = &function->phiPool[phiIndex];
            SZrIdentityNode *node;
            TZrUInt32 slot;
            if (!zr_identity_work(plan)) return ZR_FALSE;
            if (phi->result == 0u || phi->result > function->valueCount) return ZR_FALSE;
            node = &plan->nodes[phi->result];
            if (node->definition.kind != ZR_ALIAS_MISSING) return ZR_FALSE;
            node->definition.kind = ZR_ALIAS_PHI_ENTRY;
            node->definition.block = block->id;
            node->definition.phi = phiIndex;
            for (slot = 0u; slot < phi->incomings.count; ++slot) {
                if (!zr_identity_work(plan)) return ZR_FALSE;
                zr_identity_owner(&plan->incomings[phi->incomings.start + slot], phiIndex + 1u);
            }
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_identity_edge_facts(SZrExecIrPhiIdentityPlan *plan,
        TZrExecIrValueId source, TZrExecIrValueId destination) {
    return (TZrBool)(zr_identity_same_facts(plan->function, source, destination) &&
            !plan->nodes[source].consumed && !plan->nodes[destination].consumed);
}

static TZrBool zr_identity_enroll(SZrExecIrPhiIdentityPlan *plan) {
    SZrExecIrFunction *function = plan->function;
    TZrUInt32 index;
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        TZrExecIrValueId source, destination;
        if (!zr_identity_work(plan)) return ZR_FALSE;
        if (!zr_identity_copy(function, instruction)) continue;
        source = function->operands[instruction->operands.start];
        destination = function->results[instruction->results.start];
        if (!zr_identity_edge_facts(plan, source, destination)) continue;
        plan->nodes[destination].kind = ZR_ALIAS_COPY;
        plan->nodes[destination].state = ZR_ALIAS_UNSEEN;
    }
    if (plan->freezePhi) return ZR_TRUE;
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        TZrUInt32 at;
        if (!zr_identity_work(plan)) return ZR_FALSE;
        for (at = 0u; at < block->phis.count; ++at) {
            const SZrExecIrPhi *phi = &function->phiPool[block->phis.start + at];
            TZrUInt32 slot;
            TZrBool eligible = ZR_TRUE;
            if (!zr_identity_work(plan)) return ZR_FALSE;
            if (phi->incomings.count == 0u ||
                phi->incomings.count != block->predecessors.count ||
                !zr_identity_value(function, phi->result) || plan->nodes[phi->result].consumed)
                continue;
            for (slot = 0u; slot < phi->incomings.count; ++slot) {
                TZrUInt32 incomingIndex = phi->incomings.start + slot;
                const SZrExecIrPhiIncoming *incoming = &function->phiIncoming[incomingIndex];
                if (!zr_identity_work(plan)) return ZR_FALSE;
                if (plan->incomings[incomingIndex].blocked ||
                    !zr_identity_edge_facts(plan, incoming->value, phi->result) ||
                    !ZrParser_ExecIr_AliasPhiEdge(&plan->availability, block,
                            slot, incoming->predecessor)) eligible = ZR_FALSE;
                if (zr_identity_exhausted(plan->context)) return ZR_FALSE;
            }
            if (eligible) {
                plan->nodes[phi->result].kind = ZR_ALIAS_PHI;
                plan->nodes[phi->result].state = ZR_ALIAS_UNSEEN;
            }
        }
    }
    return ZR_TRUE;
}

static TZrExecIrValueId zr_identity_child(SZrExecIrPhiIdentityPlan *plan,
        const SZrIdentityNode *node, TZrUInt32 at) {
    const SZrExecIrFunction *function = plan->function;
    if (node->kind == ZR_ALIAS_COPY) {
        const SZrExecIrInstruction *copy = &function->instructions[node->definition.instruction - 1u];
        return function->operands[copy->operands.start];
    }
    return function->phiIncoming[function->phiPool[node->definition.phi].incomings.start + at].value;
}

static TZrBool zr_identity_prove_node(SZrExecIrPhiIdentityPlan *plan,
        SZrIdentityNode *node) {
    SZrExecIrFunction *function = plan->function;
    const SZrAliasDefinition *root = &plan->nodes[node->firstRoot].definition;
    if (node->kind == ZR_ALIAS_COPY) {
        TZrExecIrValueId child = zr_identity_child(plan, node, 0u);
        return (TZrBool)(ZrParser_ExecIr_AliasAvailable(&plan->availability,
                &plan->nodes[child].definition, node->definition.block,
                node->definition.instruction, 0u) &&
                ZrParser_ExecIr_AliasAvailable(&plan->availability, root,
                node->definition.block, node->definition.instruction, 0u));
    } else {
        const SZrExecIrPhi *phi = &function->phiPool[node->definition.phi];
        TZrUInt32 at;
        if (!ZrParser_ExecIr_AliasAvailable(&plan->availability, root,
                node->definition.block, 0u, 0u)) return ZR_FALSE;
        for (at = 0u; at < phi->incomings.count; ++at) {
            const SZrExecIrPhiIncoming *incoming = &function->phiIncoming[phi->incomings.start + at];
            if (!zr_identity_work(plan) ||
                !ZrParser_ExecIr_AliasAvailable(&plan->availability,
                    &plan->nodes[incoming->value].definition, incoming->predecessor,
                    0u, node->definition.block) ||
                !ZrParser_ExecIr_AliasAvailable(&plan->availability, root,
                    incoming->predecessor, 0u, node->definition.block)) return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/* Explicit DFS over the original COPY/PHI graph. A differing-input PHI is an
 * opaque entry leaf; a failed or cyclic identity is never relabeled a leaf. */
static TZrBool zr_identity_resolve(SZrExecIrPhiIdentityPlan *plan) {
    SZrExecIrFunction *function = plan->function;
    TZrUInt32 index;
    for (index = 0u; index < function->valueCount; ++index) {
        TZrUInt32 depth = 0u;
        if (!zr_identity_work(plan)) return ZR_FALSE;
        if (plan->nodes[index + 1u].state != ZR_ALIAS_UNSEEN) continue;
        plan->stack[depth++] = index + 1u;
        plan->nodes[index + 1u].state = ZR_ALIAS_VISITING;
        while (depth != 0u) {
            TZrExecIrValueId value = plan->stack[depth - 1u];
            SZrIdentityNode *node = &plan->nodes[value];
            TZrUInt32 children = node->kind == ZR_ALIAS_COPY ? 1u :
                    function->phiPool[node->definition.phi].incomings.count;
            if (!zr_identity_work(plan)) return ZR_FALSE;
            if (node->next < children) {
                TZrExecIrValueId child = zr_identity_child(plan, node, node->next);
                SZrIdentityNode *dependency = &plan->nodes[child];
                if (dependency->state == ZR_ALIAS_UNSEEN) {
                    if (depth >= function->valueCount) return ZR_FALSE;
                    dependency->state = ZR_ALIAS_VISITING;
                    plan->stack[depth++] = child;
                    continue;
                }
                if (dependency->state == ZR_ALIAS_VISITING ||
                    dependency->state == ZR_ALIAS_REJECTED) {
                    node->state = ZR_ALIAS_REJECTED;
                    --depth;
                    continue;
                }
                if (node->next == 0u) node->firstRoot = dependency->root;
                else if (node->firstRoot != dependency->root) node->unequal = 1u;
                if (dependency->hasPhi) node->hasPhi = 1u;
                ++node->next;
                continue;
            }
            if (node->unequal && node->kind == ZR_ALIAS_PHI) {
                node->root = value;
                node->hasPhi = 0u;
                node->state = ZR_ALIAS_RESOLVED;
            } else if (node->firstRoot == value || !zr_identity_prove_node(plan, node)) {
                node->state = ZR_ALIAS_REJECTED;
            } else {
                node->root = node->firstRoot;
                if (node->kind == ZR_ALIAS_PHI) node->hasPhi = 1u;
                node->state = ZR_ALIAS_RESOLVED;
            }
            if (zr_identity_exhausted(plan->context)) return ZR_FALSE;
            --depth;
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_identity_use(SZrExecIrPhiIdentityPlan *plan,
        TZrExecIrValueId value, TZrExecIrBlockId block,
        TZrExecIrInstructionId instruction, TZrExecIrBlockId successor) {
    const SZrIdentityNode *node = &plan->nodes[value];
    if (node->state != ZR_ALIAS_RESOLVED || node->root == value ||
        !zr_identity_edge_facts(plan, value, node->root)) return ZR_FALSE;
    return (TZrBool)(ZrParser_ExecIr_AliasAvailable(&plan->availability,
                &node->definition, block, instruction, successor) &&
            ZrParser_ExecIr_AliasAvailable(&plan->availability,
                &plan->nodes[node->root].definition, block, instruction, successor));
}

/* Retain the old predecessor-local decreasing COPY chain contract. This gate
 * is deliberately not consulted for a resolved PHI identity dependency. */
static TZrBool zr_identity_legacy_edge(SZrExecIrPhiIdentityPlan *plan,
        const SZrExecIrBlock *block, TZrUInt32 slot,
        TZrExecIrValueId value, TZrExecIrBlockId predecessor,
        TZrExecIrSourceId *source, TZrExecIrValueId *replacement) {
    SZrExecIrFunction *function = plan->function;
    TZrUInt32 before, steps = 0u;
    TZrExecIrValueId original = value;
    TZrExecIrValueId root = plan->nodes[value].root;
    if (function->stateMap != ZR_NULL || function->gcMapCount != 0u ||
        function->deoptStateCount != 0u || function->deoptValueCount != 0u ||
        function->deoptAggregateCount != 0u ||
        (block->flags & ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0u ||
        plan->legacy == ZR_NULL ||
        !plan->legacy(function, plan->context, block, slot, predecessor, &before))
        return ZR_FALSE;
    *source = function->instructions[before - 1u].sourceId;
    while (value != root) {
        const SZrIdentityNode *node = &plan->nodes[value];
        const SZrExecIrInstruction *copy;
        if (!zr_identity_work(plan) || steps++ >= function->valueCount) return ZR_FALSE;
        if (node->kind != ZR_ALIAS_COPY || node->definition.block != predecessor ||
            node->definition.instruction == 0u || node->definition.instruction >= before)
            break;
        copy = &function->instructions[node->definition.instruction - 1u];
        if (copy->flags != 0u || copy->bindingRow != 0u || copy->deoptId != 0u ||
            copy->effectIn != 0u || copy->effectOut != 0u ||
            copy->memoryIn.count != 0u || copy->memoryOut.count != 0u ||
            copy->phiRange.count != 0u || copy->successorRange.count != 0u) break;
        before = node->definition.instruction;
        value = zr_identity_child(plan, node, 0u);
    }
    *replacement = value;
    return (TZrBool)(value != original);
}

static TZrBool zr_identity_plan_slots(SZrExecIrPhiIdentityPlan *plan) {
    SZrExecIrFunction *function = plan->function;
    TZrUInt32 index;
    for (index = 0u; index < function->operandCount; ++index) {
        SZrIdentitySlot *slot = &plan->operands[index];
        TZrExecIrValueId value;
        const SZrExecIrInstruction *instruction;
        if (!zr_identity_work(plan)) return ZR_FALSE;
        if (slot->owners != 1u) continue;
        value = function->operands[index];
        instruction = &function->instructions[slot->owner - 1u];
        if (!zr_identity_use(plan, value,
                plan->availability.instructionBlocks[slot->owner], slot->owner, 0u)) {
            if (zr_identity_exhausted(plan->context)) return ZR_FALSE;
            continue;
        }
        if (!zr_identity_work(plan)) return ZR_FALSE;
        slot->replacement = plan->nodes[value].root;
        slot->source = instruction->sourceId;
    }
    for (index = 0u; index < function->phiIncomingCount; ++index) {
        SZrIdentitySlot *slot = &plan->incomings[index];
        const SZrExecIrPhiIncoming *incoming = &function->phiIncoming[index];
        const SZrExecIrPhi *phi;
        const SZrExecIrBlock *block;
        TZrUInt32 offset;
        TZrExecIrSourceId source;
        TZrExecIrValueId replacement;
        if (!zr_identity_work(plan)) return ZR_FALSE;
        if (slot->blocked || slot->owners != 1u) continue;
        phi = &function->phiPool[slot->owner - 1u];
        block = &function->blocks[plan->nodes[phi->result].definition.block - 1u];
        offset = index - phi->incomings.start;
        if (phi->incomings.count != block->predecessors.count ||
            !ZrParser_ExecIr_AliasPhiEdge(&plan->availability, block, offset,
                    incoming->predecessor) ||
            !zr_identity_use(plan, incoming->value, incoming->predecessor, 0u, block->id)) {
            if (zr_identity_exhausted(plan->context)) return ZR_FALSE;
            continue;
        }
        source = block->instructions.count == 0u ? 0u : function->instructions[
                block->instructions.start + block->instructions.count - 1u].sourceId;
        replacement = plan->nodes[incoming->value].root;
        if (!plan->nodes[incoming->value].hasPhi &&
            !zr_identity_legacy_edge(plan, block, offset, incoming->value,
                    incoming->predecessor, &source, &replacement)) {
            if (zr_identity_exhausted(plan->context)) return ZR_FALSE;
            continue;
        }
        if (!ZrParser_ExecIr_AliasAvailable(&plan->availability,
                &plan->nodes[replacement].definition, incoming->predecessor, 0u, block->id)) {
            if (zr_identity_exhausted(plan->context)) return ZR_FALSE;
            continue;
        }
        if (!zr_identity_work(plan)) return ZR_FALSE;
        slot->replacement = replacement;
        slot->source = source;
    }
    /* Prepay both Apply scans: Apply has no proof, allocation or failing work. */
    return ZrParser_ExecIr_PassConsumeBudget(function, plan->context,
            (TZrUInt64)function->operandCount + function->phiIncomingCount);
}

TZrBool ZrParser_ExecIr_PhiIdentityPrepare(SZrExecIrFunction *function,
        SZrExecIrPassContext *context, TZrExecIrLegacyPhiPredecessor legacy,
        SZrExecIrPhiIdentityPlan **outPlan, SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrPhiIdentityPlan *plan;
    TZrBool freeze = zr_identity_frozen(function);
    *outPlan = ZR_NULL;
    if (!zr_identity_potential(function, context, freeze) || zr_identity_exhausted(context))
        return ZR_TRUE;
    plan = (SZrExecIrPhiIdentityPlan *)ZrParser_ExecIr_AliasAllocate(function,
            context, 1u, sizeof(*plan), diagnostic);
    if (plan == ZR_NULL) return zr_identity_exhausted(context);
    plan->function = function;
    plan->context = context;
    plan->legacy = legacy;
    plan->freezePhi = freeze;
    plan->nodes = (SZrIdentityNode *)ZrParser_ExecIr_AliasAllocate(function, context,
            (TZrUInt64)function->valueCount + 1u, sizeof(*plan->nodes), diagnostic);
    if (plan->nodes == ZR_NULL) goto allocation_failed;
    plan->stack = (TZrExecIrValueId *)ZrParser_ExecIr_AliasAllocate(function, context,
            (TZrUInt64)function->valueCount + 1u, sizeof(*plan->stack), diagnostic);
    if (plan->stack == ZR_NULL) goto allocation_failed;
    plan->operands = (SZrIdentitySlot *)ZrParser_ExecIr_AliasAllocate(function, context,
            (TZrUInt64)function->operandCount + 1u, sizeof(*plan->operands), diagnostic);
    if (plan->operands == ZR_NULL) goto allocation_failed;
    plan->incomings = (SZrIdentitySlot *)ZrParser_ExecIr_AliasAllocate(function, context,
            (TZrUInt64)function->phiIncomingCount + 1u, sizeof(*plan->incomings), diagnostic);
    if (plan->incomings == ZR_NULL) goto allocation_failed;
    if (!ZrParser_ExecIr_AliasAvailabilityPrepare(&plan->availability,
            function, context, diagnostic)) goto allocation_failed;
    if (!plan->availability.valid || !zr_identity_collect(plan) ||
        !zr_identity_enroll(plan) || !zr_identity_resolve(plan) ||
        !zr_identity_plan_slots(plan)) {
        ZrParser_ExecIr_PhiIdentityDispose(plan);
        return ZR_TRUE;
    }
    *outPlan = plan;
    return ZR_TRUE;
allocation_failed:
    ZrParser_ExecIr_PhiIdentityDispose(plan);
    return zr_identity_exhausted(context);
}

static void zr_identity_changed(SZrExecIrPhiIdentityPlan *plan,
        TZrBool *changed, TZrExecIrSourceId source) {
    if (changed != ZR_NULL) *changed = ZR_TRUE;
    if (plan->context != ZR_NULL && plan->context->lastSourceId == 0u)
        plan->context->lastSourceId = source;
}

void ZrParser_ExecIr_PhiIdentityApply(SZrExecIrPhiIdentityPlan *plan,
        TZrBool *changed) {
    TZrUInt32 index;
    if (plan == ZR_NULL) return;
    for (index = 0u; index < plan->function->operandCount; ++index) {
        const SZrIdentitySlot *slot = &plan->operands[index];
        if (slot->replacement == 0u) continue;
        plan->function->operands[index] = slot->replacement;
        zr_identity_changed(plan, changed, slot->source);
    }
    for (index = 0u; index < plan->function->phiIncomingCount; ++index) {
        const SZrIdentitySlot *slot = &plan->incomings[index];
        if (slot->replacement == 0u) continue;
        plan->function->phiIncoming[index].value = slot->replacement;
        zr_identity_changed(plan, changed, slot->source);
    }
}
