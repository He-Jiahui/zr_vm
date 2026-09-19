#include "zr_vm_parser/compiler.h"

#include "compiler_internal.h"
#include "compile_statement_internal.h"

static void resolve_while_backedge_label(SZrCompilerState *cs,
                                         TZrSize labelId,
                                         TZrSize instructionIndex) {
    SZrLabel *label = (SZrLabel *)ZrCore_Array_Get(&cs->labels, labelId);
    TZrSize index;

    if (label == ZR_NULL) {
        return;
    }
    label->instructionIndex = instructionIndex;
    label->isResolved = ZR_TRUE;
    for (index = 0U; index < cs->pendingJumps.length; index++) {
        SZrPendingJump *pending = (SZrPendingJump *)ZrCore_Array_Get(
                &cs->pendingJumps, index);
        TZrInstruction *instruction;

        if (pending == ZR_NULL || pending->labelId != labelId ||
            pending->instructionIndex >= cs->instructions.length) {
            continue;
        }
        instruction = (TZrInstruction *)ZrCore_Array_Get(
                &cs->instructions, pending->instructionIndex);
        if (instruction != ZR_NULL) {
            instruction->instruction.operand.operand2[0] =
                    (TZrInt32)instructionIndex -
                    (TZrInt32)pending->instructionIndex - 1;
        }
    }
    for (index = 0U; index < cs->pendingAbsolutePatches.length; index++) {
        SZrPendingAbsolutePatch *pending =
                (SZrPendingAbsolutePatch *)ZrCore_Array_Get(
                        &cs->pendingAbsolutePatches, index);
        TZrInstruction *instruction;

        if (pending == ZR_NULL || pending->labelId != labelId ||
            pending->instructionIndex >= cs->instructions.length) {
            continue;
        }
        instruction = (TZrInstruction *)ZrCore_Array_Get(
                &cs->instructions, pending->instructionIndex);
        if (instruction != ZR_NULL) {
            instruction->instruction.operand.operand2[0] =
                    (TZrInt32)instructionIndex;
        }
    }
}

void compile_while_statement(SZrCompilerState *cs, SZrAstNode *node) {
    SZrArray semanticSlotSnapshot;
    TZrBool hasSemanticSlotSnapshot = ZR_FALSE;
    TZrBool hasSemanticCfg;
    TZrBool previousSemanticCfgStartupSuppressed = ZR_FALSE;
    TZrBool restoreSemanticCfgStartupSuppression = ZR_FALSE;
    TZrUInt32 conditionBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    TZrUInt32 bodyBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    TZrUInt32 joinBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    SZrWhileLoop *whileLoop;
    SZrLoopLabel loopLabel;
    TZrSize loopStartLabelId;
    TZrSize loopStartInstructionIndex;
    TZrUInt32 conditionSlot;
    TZrSize loopEndLabelId;
    TZrInstruction instruction;
    TZrSize instructionIndex;

    ZrCore_Array_Construct(&semanticSlotSnapshot);
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    if (node->type != ZR_AST_WHILE_LOOP) {
        ZrParser_Compiler_Error(cs, "Expected while statement", node->location);
        return;
    }
    whileLoop = &node->data.whileLoop;
    loopLabel.breakLabelId = create_label(cs);
    loopLabel.continueLabelId = create_label(cs);
    loopLabel.targetScopeStackDepth = cs->scopeStack.length;
    loopLabel.semanticBreakBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    loopLabel.semanticContinueBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    ZrCore_Array_Push(cs->state, &cs->loopLabelStack, &loopLabel);
    loopStartLabelId = loopLabel.continueLabelId;
    loopStartInstructionIndex = cs->instructionCount;

    hasSemanticCfg = compiler_semantic_cfg_begin_while(
            cs, node, &conditionBlock, &bodyBlock, &joinBlock);
    if (!hasSemanticCfg && cs->preSemanticIrCfgActive &&
        !cs->preSemanticIrCfgTerminated) {
        ZrParser_Compiler_Error(
                cs, "Failed to start semantic while CFG", node->location);
        goto cleanup;
    }
    if (!hasSemanticCfg) {
        previousSemanticCfgStartupSuppressed =
                cs->preSemanticIrCfgStartupSuppressed;
        cs->preSemanticIrCfgStartupSuppressed = ZR_TRUE;
        restoreSemanticCfgStartupSuppression = ZR_TRUE;
    }
    if (hasSemanticCfg) {
        SZrLoopLabel *activeLoopLabel =
                (SZrLoopLabel *)ZrCore_Array_Get(
                        &cs->loopLabelStack,
                        cs->loopLabelStack.length - 1U);
        if (activeLoopLabel == ZR_NULL) {
            ZrParser_Compiler_Error(
                    cs, "Failed to bind semantic while loop targets",
                    node->location);
            goto cleanup;
        }
        activeLoopLabel->semanticBreakBlockId = joinBlock;
        activeLoopLabel->semanticContinueBlockId = conditionBlock;
        hasSemanticSlotSnapshot = compiler_semantic_cfg_capture_slots(
                cs, &semanticSlotSnapshot);
        if (!hasSemanticSlotSnapshot) {
            ZrParser_Compiler_Error(
                    cs, "Failed to snapshot semantic loop values",
                    node->location);
            goto cleanup;
        }
    }

    ZrParser_Expression_Compile(cs, whileLoop->cond);
    conditionSlot = cs->lastExpressionSlot;
    if (conditionSlot == ZR_PARSER_SLOT_NONE) {
        ZrParser_Compiler_Error(
                cs, "Failed to compile while condition", node->location);
        goto cleanup;
    }
    if (hasSemanticCfg && !compiler_semantic_cfg_branch_while(
                cs, conditionSlot, node, bodyBlock, joinBlock)) {
        ZrParser_Compiler_Error(
                cs, "Failed to record semantic while condition",
                node->location);
        goto cleanup;
    }

    loopEndLabelId = loopLabel.breakLabelId;
    instruction = compiler_create_jump_if_false_for_condition(
            cs, whileLoop->cond, conditionSlot);
    instructionIndex = cs->instructionCount;
    emit_instruction(cs, instruction);
    add_pending_jump(cs, instructionIndex, loopEndLabelId);

    if (whileLoop->block != ZR_NULL) {
        ZrParser_Statement_Compile(cs, whileLoop->block);
    }
    if (cs->hasError) {
        goto cleanup;
    }
    if (hasSemanticCfg && !cs->preSemanticIrCfgActive) {
        hasSemanticCfg = ZR_FALSE;
    }
    if (hasSemanticCfg &&
        cs->preSemanticIrCfgBlock != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
        !compiler_semantic_cfg_jump(cs, conditionBlock, node->location)) {
        ZrParser_Compiler_Error(
                cs, "Failed to record semantic while backedge",
                node->location);
        goto cleanup;
    }

    instruction = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
    instructionIndex = cs->instructionCount;
    emit_instruction(cs, instruction);
    add_pending_jump(cs, instructionIndex, loopStartLabelId);
    resolve_while_backedge_label(
            cs, loopStartLabelId, loopStartInstructionIndex);
    resolve_label(cs, loopEndLabelId);

    if (hasSemanticCfg) {
        if (!compiler_semantic_cfg_restore_slots(
                    cs, &semanticSlotSnapshot)) {
            ZrParser_Compiler_Error(
                    cs, "Failed to restore semantic loop values",
                    node->location);
            goto cleanup;
        }
        compiler_semantic_cfg_enter(cs, joinBlock);
    }

cleanup:
    if (restoreSemanticCfgStartupSuppression) {
        cs->preSemanticIrCfgStartupSuppressed =
                previousSemanticCfgStartupSuppressed;
    }
    if (hasSemanticSlotSnapshot) {
        compiler_semantic_cfg_free_slots(cs, &semanticSlotSnapshot);
    }
    ZrCore_Array_Pop(&cs->loopLabelStack);
}
