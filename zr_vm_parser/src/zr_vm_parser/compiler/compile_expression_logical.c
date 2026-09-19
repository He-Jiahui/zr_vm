#include "compile_expression_internal.h"

static TZrBool compile_logical_right_operand(
        SZrCompilerState *cs,
        SZrAstNode *right,
        TZrUInt32 destinationSlot,
        TZrBool hasSemanticCfg,
        SZrFileRange location) {
    TZrUInt32 rightSlot;

    if (!hasSemanticCfg) {
        if (compile_expression_into_slot(
                    cs, right, destinationSlot) == ZR_PARSER_SLOT_NONE ||
            normalize_top_result_to_slot(
                    cs, destinationSlot) == ZR_PARSER_SLOT_NONE) {
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }

    compile_expression_non_tail(cs, right);
    rightSlot = cs->lastExpressionSlot;
    if (cs->hasError || rightSlot == ZR_PARSER_SLOT_NONE) {
        return ZR_FALSE;
    }
    if (!compiler_semantic_ir_lower_store(
                cs, destinationSlot, rightSlot, location)) {
        ZrParser_Compiler_Error(
                cs,
                "Failed to store logical right operand in pre-execution Semantic IR",
                location);
        return ZR_FALSE;
    }
    collapse_stack_to_slot(cs, destinationSlot);
    return ZR_TRUE;
}

void compile_logical_expression(SZrCompilerState *cs, SZrAstNode *node) {
    SZrArray semanticSlotSnapshot;
    const TZrChar *op;
    SZrAstNode *left;
    SZrAstNode *right;
    TZrUInt32 leftSlot;
    TZrUInt32 destinationSlot;
    TZrUInt32 rightBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    TZrUInt32 joinBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    TZrUInt32 resultSlot;
    TZrSize endLabelId;
    TZrSize evaluateRightLabelId = 0U;
    TZrBool hasSemanticCfg;
    TZrBool hasSemanticSlotSnapshot = ZR_FALSE;
    TZrBool previousSemanticCfgStartupSuppressed = ZR_FALSE;
    TZrBool restoreSemanticCfgStartupSuppression = ZR_FALSE;

    ZrCore_Array_Construct(&semanticSlotSnapshot);
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    if (node->type != ZR_AST_LOGICAL_EXPRESSION) {
        ZrParser_Compiler_Error(
                cs, "Expected logical expression", node->location);
        return;
    }

    op = node->data.logicalExpression.op;
    left = node->data.logicalExpression.left;
    right = node->data.logicalExpression.right;
    if (op == ZR_NULL ||
        (strcmp(op, "&&") != 0 && strcmp(op, "||") != 0)) {
        ZrParser_Compiler_Error(
                cs, "Unknown logical operator", node->location);
        return;
    }

    compile_expression_non_tail(cs, left);
    leftSlot = cs->lastExpressionSlot;
    if (cs->hasError || leftSlot == ZR_PARSER_SLOT_NONE) {
        ZrParser_Compiler_Error(
                cs, "Failed to compile logical left operand", node->location);
        return;
    }

    destinationSlot = allocate_stack_slot(cs);
    if (destinationSlot == ZR_PARSER_SLOT_NONE ||
        !compiler_semantic_ir_transfer_expression_result(
                cs, leftSlot, destinationSlot, node->location)) {
        ZrParser_Compiler_Error(
                cs,
                "Failed to initialize logical result in pre-execution Semantic IR",
                node->location);
        return;
    }
    emit_instruction(
            cs,
            create_instruction_1(
                    ZR_INSTRUCTION_ENUM(SET_STACK),
                    (TZrUInt16)destinationSlot,
                    (TZrInt32)leftSlot));

    hasSemanticCfg = compiler_semantic_cfg_begin_short_circuit(
            cs, leftSlot, node, &rightBlock, &joinBlock);
    if (!hasSemanticCfg && cs->preSemanticIrCfgActive &&
        !cs->preSemanticIrCfgTerminated) {
        ZrParser_Compiler_Error(
                cs, "Failed to start semantic short-circuit CFG", node->location);
        return;
    }
    if (!hasSemanticCfg) {
        previousSemanticCfgStartupSuppressed =
                cs->preSemanticIrCfgStartupSuppressed;
        cs->preSemanticIrCfgStartupSuppressed = ZR_TRUE;
        restoreSemanticCfgStartupSuppression = ZR_TRUE;
    }
    if (hasSemanticCfg) {
        hasSemanticSlotSnapshot = compiler_semantic_cfg_capture_slots(
                cs, &semanticSlotSnapshot);
        if (!hasSemanticSlotSnapshot) {
            ZrParser_Compiler_Error(
                    cs, "Failed to snapshot semantic logical values",
                    node->location);
            return;
        }
    }

    endLabelId = create_label(cs);
    if (strcmp(op, "&&") == 0) {
        TZrInstruction jumpIfInst =
                compiler_create_jump_if_false_for_condition(
                        cs, left, leftSlot);
        TZrSize jumpIfIndex = cs->instructionCount;
        emit_instruction(cs, jumpIfInst);
        add_pending_jump(cs, jumpIfIndex, endLabelId);
    } else {
        TZrInstruction jumpIfInst;
        TZrInstruction jumpEndInst;
        TZrSize jumpIfIndex;
        TZrSize jumpEndIndex;

        evaluateRightLabelId = create_label(cs);
        jumpIfInst = compiler_create_jump_if_false_for_condition(
                cs, left, leftSlot);
        jumpIfIndex = cs->instructionCount;
        emit_instruction(cs, jumpIfInst);
        add_pending_jump(cs, jumpIfIndex, evaluateRightLabelId);

        jumpEndInst = create_instruction_1(
                ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
        jumpEndIndex = cs->instructionCount;
        emit_instruction(cs, jumpEndInst);
        add_pending_jump(cs, jumpEndIndex, endLabelId);
        resolve_label(cs, evaluateRightLabelId);
    }

    if (!compile_logical_right_operand(
                cs, right, destinationSlot, hasSemanticCfg, node->location)) {
        goto cleanup;
    }
    if (hasSemanticCfg && !compiler_semantic_cfg_jump(
                cs, joinBlock, node->location)) {
        ZrParser_Compiler_Error(
                cs, "Failed to record semantic logical right branch",
                node->location);
        goto cleanup;
    }

    resolve_label(cs, endLabelId);
    if (!hasSemanticCfg) {
        collapse_stack_to_slot(cs, destinationSlot);
        goto cleanup;
    }
    if (!compiler_semantic_cfg_restore_slots(
                cs, &semanticSlotSnapshot)) {
        ZrParser_Compiler_Error(
                cs, "Failed to restore semantic logical join values",
                node->location);
        goto cleanup;
    }
    compiler_semantic_cfg_enter(cs, joinBlock);
    resultSlot = allocate_fresh_stack_slot_after(cs, destinationSlot);
    if (resultSlot == ZR_PARSER_SLOT_NONE ||
        !compiler_semantic_ir_lower_load(
                cs, destinationSlot, resultSlot, node->location)) {
        ZrParser_Compiler_Error(
                cs, "Failed to load merged logical result",
                node->location);
        goto cleanup;
    }
    collapse_stack_to_slot(cs, resultSlot);

cleanup:
    if (restoreSemanticCfgStartupSuppression) {
        cs->preSemanticIrCfgStartupSuppressed =
                previousSemanticCfgStartupSuppressed;
    }
    if (hasSemanticSlotSnapshot) {
        compiler_semantic_cfg_free_slots(cs, &semanticSlotSnapshot);
    }
}
