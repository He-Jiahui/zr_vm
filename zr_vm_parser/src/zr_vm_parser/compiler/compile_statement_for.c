#include "zr_vm_parser/compiler.h"

#include "compiler_internal.h"
#include "compile_statement_internal.h"

void compile_for_statement(SZrCompilerState *cs, SZrAstNode *node) {
    SZrArray semanticSlotSnapshot;
    TZrBool hasSemanticSlotSnapshot = ZR_FALSE;
    TZrBool hasSemanticCfg;
    TZrBool previousSemanticCfgStartupSuppressed = ZR_FALSE;
    TZrBool restoreSemanticCfgStartupSuppression = ZR_FALSE;
    TZrBool restoreSemanticCfgTermination = ZR_FALSE;
    TZrBool enteredScope = ZR_FALSE;
    TZrBool pushedLoopLabel = ZR_FALSE;
    TZrUInt32 conditionBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    TZrUInt32 bodyBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    TZrUInt32 stepBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    TZrUInt32 joinBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    SZrForLoop *forLoop;
    SZrLoopLabel loopLabel;
    TZrSize conditionLabelId;
    TZrSize loopEndLabelId;

    ZrCore_Array_Construct(&semanticSlotSnapshot);
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    if (node->type != ZR_AST_FOR_LOOP) {
        ZrParser_Compiler_Error(cs, "Expected for statement", node->location);
        return;
    }

    forLoop = &node->data.forLoop;
    enter_scope(cs);
    enteredScope = ZR_TRUE;

    loopLabel.breakLabelId = create_label(cs);
    loopLabel.continueLabelId = create_label(cs);
    loopLabel.targetScopeStackDepth = cs->scopeStack.length;
    loopLabel.semanticBreakBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    loopLabel.semanticContinueBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    ZrCore_Array_Push(cs->state, &cs->loopLabelStack, &loopLabel);
    pushedLoopLabel = ZR_TRUE;
    conditionLabelId = create_label(cs);

    if (forLoop->init != ZR_NULL) {
        ZrParser_Statement_Compile(cs, forLoop->init);
        if (cs->hasError) {
            goto cleanup;
        }
    }

    loopEndLabelId = loopLabel.breakLabelId;
    resolve_label(cs, conditionLabelId);

    hasSemanticCfg = compiler_semantic_cfg_begin_for(
            cs, node, &conditionBlock, &bodyBlock, &stepBlock, &joinBlock);
    if (!hasSemanticCfg && cs->preSemanticIrCfgActive &&
        !cs->preSemanticIrCfgTerminated) {
        ZrParser_Compiler_Error(
                cs, "Failed to start semantic for CFG", node->location);
        goto cleanup;
    }
    if (!hasSemanticCfg) {
        previousSemanticCfgStartupSuppressed =
                cs->preSemanticIrCfgStartupSuppressed;
        cs->preSemanticIrCfgStartupSuppressed = ZR_TRUE;
        restoreSemanticCfgStartupSuppression = ZR_TRUE;
    } else {
        SZrLoopLabel *activeLoopLabel =
                (SZrLoopLabel *)ZrCore_Array_Get(
                        &cs->loopLabelStack,
                        cs->loopLabelStack.length - 1U);
        if (activeLoopLabel == ZR_NULL) {
            ZrParser_Compiler_Error(
                    cs, "Failed to bind semantic for loop targets",
                    node->location);
            goto cleanup;
        }
        activeLoopLabel->semanticBreakBlockId = joinBlock;
        activeLoopLabel->semanticContinueBlockId = stepBlock;
        hasSemanticSlotSnapshot = compiler_semantic_cfg_capture_slots(
                cs, &semanticSlotSnapshot);
        if (!hasSemanticSlotSnapshot) {
            ZrParser_Compiler_Error(
                    cs, "Failed to snapshot semantic for values",
                    node->location);
            goto cleanup;
        }
    }

    if (forLoop->cond != ZR_NULL) {
        TZrUInt32 conditionSlot;
        TZrInstruction jumpIfInstruction;
        TZrSize jumpIfIndex;

        ZrParser_Expression_Compile(cs, forLoop->cond);
        conditionSlot = cs->lastExpressionSlot;
        if (conditionSlot == ZR_PARSER_SLOT_NONE) {
            ZrParser_Compiler_Error(
                    cs, "Failed to compile for condition", node->location);
            goto cleanup;
        }
        if (hasSemanticCfg && !compiler_semantic_cfg_branch_for(
                    cs, conditionSlot, node, bodyBlock, joinBlock)) {
            ZrParser_Compiler_Error(
                    cs, "Failed to record semantic for condition",
                    node->location);
            goto cleanup;
        }

        jumpIfInstruction = compiler_create_jump_if_false_for_condition(
                cs, forLoop->cond, conditionSlot);
        jumpIfIndex = cs->instructionCount;
        emit_instruction(cs, jumpIfInstruction);
        add_pending_jump(cs, jumpIfIndex, loopEndLabelId);
    }

    if (forLoop->block != ZR_NULL) {
        ZrParser_Statement_Compile(cs, forLoop->block);
        if (cs->hasError) {
            goto cleanup;
        }
    }
    if (hasSemanticCfg && !cs->preSemanticIrCfgActive) {
        hasSemanticCfg = ZR_FALSE;
    }
    if (hasSemanticCfg && stepBlock != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
        cs->preSemanticIrCfgBlock != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
        !compiler_semantic_cfg_jump(cs, stepBlock, node->location)) {
        ZrParser_Compiler_Error(
                cs, "Failed to record semantic for step edge",
                node->location);
        goto cleanup;
    }
    if (hasSemanticCfg && stepBlock != ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        compiler_semantic_cfg_enter(cs, stepBlock);
    }

    resolve_label(cs, loopLabel.continueLabelId);
    if (hasSemanticCfg && stepBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        cs->preSemanticIrCfgTerminated = ZR_TRUE;
        restoreSemanticCfgTermination = ZR_TRUE;
    }
    if (forLoop->step != ZR_NULL) {
        ZrParser_Expression_Compile(cs, forLoop->step);
        if (cs->hasError) {
            goto cleanup;
        }
    }
    if (restoreSemanticCfgTermination) {
        cs->preSemanticIrCfgTerminated = ZR_FALSE;
        restoreSemanticCfgTermination = ZR_FALSE;
    }
    if (hasSemanticCfg && stepBlock != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
        !compiler_semantic_cfg_jump(
                cs, conditionBlock, node->location)) {
        ZrParser_Compiler_Error(
                cs, "Failed to record semantic for backedge",
                node->location);
        goto cleanup;
    }

    {
        TZrInstruction jumpInstruction =
                create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
        TZrSize jumpIndex = cs->instructionCount;
        emit_instruction(cs, jumpInstruction);
        add_pending_jump(cs, jumpIndex, conditionLabelId);
    }
    resolve_label(cs, loopEndLabelId);

    if (hasSemanticCfg) {
        if (!compiler_semantic_cfg_restore_slots(
                    cs, &semanticSlotSnapshot)) {
            ZrParser_Compiler_Error(
                    cs, "Failed to restore semantic for values",
                    node->location);
            goto cleanup;
        }
        compiler_semantic_cfg_enter(cs, joinBlock);
    }

cleanup:
    if (restoreSemanticCfgTermination) {
        cs->preSemanticIrCfgTerminated = ZR_FALSE;
    }
    if (restoreSemanticCfgStartupSuppression) {
        cs->preSemanticIrCfgStartupSuppressed =
                previousSemanticCfgStartupSuppressed;
    }
    if (hasSemanticSlotSnapshot) {
        compiler_semantic_cfg_free_slots(cs, &semanticSlotSnapshot);
    }
    if (pushedLoopLabel) {
        ZrCore_Array_Pop(&cs->loopLabelStack);
    }
    if (enteredScope) {
        exit_scope(cs);
    }
}
