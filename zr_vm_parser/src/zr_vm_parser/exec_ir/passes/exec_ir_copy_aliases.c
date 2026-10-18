#include "exec_ir_copy_aliases.h"
#include "exec_ir_phi_identity.h"

static TZrBool zr_copy_has_boundary(const SZrExecIrInstruction *instruction) {
    return (TZrBool)(instruction->flags != 0u || instruction->bindingRow != 0u ||
                     instruction->deoptId != 0u ||
                     instruction->effectIn != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID ||
                     instruction->effectOut != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID ||
                     instruction->memoryIn.count != 0u || instruction->memoryOut.count != 0u);
}

/* Match this ordered predecessor slot to its actual successor occurrence.
 * Duplicate predecessor IDs are separate uses, even for the same target. */
static TZrBool zr_copy_phi_predecessor(
        SZrExecIrFunction *function, SZrExecIrPassContext *context,
        const SZrExecIrBlock *target, TZrUInt32 slot,
        TZrExecIrBlockId predecessor, TZrUInt32 *terminator) {
    const SZrExecIrBlock *block;
    const SZrExecIrInstruction *instruction;
    TZrUInt32 occurrence = 0u;
    TZrUInt32 matched = 0u;
    TZrUInt32 at;
    if (predecessor == ZR_EXEC_IR_BLOCK_ID_INVALID || predecessor > function->blockCount ||
        slot >= target->predecessors.count ||
        function->predecessors[target->predecessors.start + slot] != predecessor)
        return ZR_FALSE;
    block = &function->blocks[predecessor - 1u];
    if ((block->flags & ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0u ||
        block->instructions.count == 0u ||
        block->terminatorInstructionId != block->instructions.start + block->instructions.count)
        return ZR_FALSE;
    instruction = &function->instructions[block->terminatorInstructionId - 1u];
    if ((instruction->opcode != ZR_EXEC_IR_OPCODE_BRANCH &&
         instruction->opcode != ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH &&
         instruction->opcode != ZR_EXEC_IR_OPCODE_SWITCH) ||
        zr_copy_has_boundary(instruction) ||
        instruction->successorRange.start != block->successors.start ||
        instruction->successorRange.count != block->successors.count)
        return ZR_FALSE;
    /* A pure final branch does not erase an interior observable boundary.
     * Consult intrinsic opcode effects too: zero encoded CALL flags do not
     * establish provider purity. This scan gates only legacy predecessor COPY normalization. */
    for (at = block->instructions.start; at < block->terminatorInstructionId - 1u; ++at) {
        const SZrExecIrInstruction *body = &function->instructions[at];
        const SZrExecIrOpcodeInfo *info;
        if (!ZrParser_ExecIr_PassConsumeBudget(function, context, 1u)) return ZR_FALSE;
        info = ZrCore_ExecIr_OpcodeInfo(body->opcode);
        if (info == ZR_NULL || zr_copy_has_boundary(body) ||
            info->memoryReads != 0u || info->memoryWrites != 0u ||
            (info->flags & ~ZR_EXEC_IR_SCHEMA_FLAG_PRODUCES_VALUE) != 0u)
            return ZR_FALSE;
    }
    for (at = 0u; at < slot; ++at) {
        if (!ZrParser_ExecIr_PassConsumeBudget(function, context, 1u)) return ZR_FALSE;
        if (function->predecessors[target->predecessors.start + at] == predecessor)
            ++occurrence;
    }
    for (at = 0u; at < block->successors.count; ++at) {
        if (!ZrParser_ExecIr_PassConsumeBudget(function, context, 1u)) return ZR_FALSE;
        if (function->successors[block->successors.start + at] != target->id) continue;
        if (matched++ == occurrence) {
            *terminator = block->terminatorInstructionId;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

TZrBool ZrParser_ExecIr_RewriteCopyAliases(SZrExecIrFunction *function,
        SZrExecIrPassContext *context, TZrBool *changed,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrPhiIdentityPlan *plan = ZR_NULL;
    TZrBool result = ZrParser_ExecIr_PhiIdentityPrepare(function, context,
            zr_copy_phi_predecessor, &plan, diagnostic);
    if (result) ZrParser_ExecIr_PhiIdentityApply(plan, changed);
    ZrParser_ExecIr_PhiIdentityDispose(plan);
    return result;
}