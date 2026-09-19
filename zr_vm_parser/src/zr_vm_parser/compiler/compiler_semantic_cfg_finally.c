#include "compiler_internal.h"

static TZrBool compiler_semantic_cfg_finally_block_is_linear(
        const SZrAstNode *node) {
    TZrSize index;

    if (node == ZR_NULL || node->type != ZR_AST_BLOCK) {
        return ZR_FALSE;
    }
    if (node->data.block.body == ZR_NULL) {
        return ZR_TRUE;
    }
    for (index = 0U; index < node->data.block.body->count; index++) {
        const SZrAstNode *statement = node->data.block.body->nodes[index];

        if (statement == ZR_NULL) {
            continue;
        }
        if (statement->type == ZR_AST_BLOCK) {
            if (!compiler_semantic_cfg_finally_block_is_linear(statement)) {
                return ZR_FALSE;
            }
            continue;
        }
        if (statement->type != ZR_AST_EXPRESSION_STATEMENT ||
            !compiler_semantic_cfg_expression_is_linear(
                    statement->data.expressionStatement.expr)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool compiler_semantic_cfg_finally_protected_block_flow(
        const SZrAstNode *node,
        EZrSemanticIrOpcode *outCompletionOpcode) {
    EZrSemanticIrOpcode completionOpcode = ZR_SEMANTIC_IR_INVALID;
    TZrSize index;

    if (node == ZR_NULL || node->type != ZR_AST_BLOCK ||
        outCompletionOpcode == ZR_NULL) {
        return ZR_FALSE;
    }
    if (node->data.block.body == ZR_NULL) {
        *outCompletionOpcode = ZR_SEMANTIC_IR_INVALID;
        return ZR_TRUE;
    }
    for (index = 0U; index < node->data.block.body->count; index++) {
        const SZrAstNode *statement = node->data.block.body->nodes[index];
        EZrSemanticIrOpcode statementCompletionOpcode =
                ZR_SEMANTIC_IR_INVALID;
        TZrSize trailingIndex;

        if (statement == ZR_NULL) {
            continue;
        }
        if (completionOpcode != ZR_SEMANTIC_IR_INVALID) {
            return ZR_FALSE;
        }
        if (statement->type == ZR_AST_BLOCK) {
            if (!compiler_semantic_cfg_finally_protected_block_flow(
                        statement, &statementCompletionOpcode)) {
                return ZR_FALSE;
            }
        } else if (statement->type == ZR_AST_EXPRESSION_STATEMENT) {
            if (!compiler_semantic_cfg_expression_is_linear(
                        statement->data.expressionStatement.expr)) {
                return ZR_FALSE;
            }
        } else if (statement->type == ZR_AST_RETURN_STATEMENT) {
            if (!compiler_semantic_cfg_expression_is_linear(
                        statement->data.returnStatement.expr)) {
                return ZR_FALSE;
            }
            statementCompletionOpcode = ZR_SEMANTIC_IR_RETURN;
        } else if (statement->type == ZR_AST_THROW_STATEMENT) {
            if (!compiler_semantic_cfg_expression_is_linear(
                        statement->data.throwStatement.expr)) {
                return ZR_FALSE;
            }
            statementCompletionOpcode = ZR_SEMANTIC_IR_THROW;
        } else {
            return ZR_FALSE;
        }
        if (statementCompletionOpcode == ZR_SEMANTIC_IR_INVALID) {
            continue;
        }
        for (trailingIndex = index + 1U;
             trailingIndex < node->data.block.body->count;
             trailingIndex++) {
            if (node->data.block.body->nodes[trailingIndex] != ZR_NULL) {
                return ZR_FALSE;
            }
        }
        completionOpcode = statementCompletionOpcode;
    }
    *outCompletionOpcode = completionOpcode;
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_try_finally_is_supported(
        SZrCompilerState *cs,
        const SZrAstNode *node) {
    const SZrTryCatchFinallyStatement *statement;
    EZrSemanticIrOpcode completionOpcode = ZR_SEMANTIC_IR_INVALID;

    if (cs == ZR_NULL || node == ZR_NULL ||
        node->type != ZR_AST_TRY_CATCH_FINALLY_STATEMENT ||
        cs->preSemanticIrCfgTerminated ||
        cs->preSemanticIrCfgStartupSuppressed ||
        cs->preSemanticIrCfgStartupBlocked ||
        (cs->preSemanticIrCfgActive &&
         cs->preSemanticIrCfgBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID) ||
        cs->preSemanticIrCfgCatchBlock != ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        cs->preSemanticIrCfgFinallyPlan != ZR_NULL ||
        compiler_has_active_scope_ownership_cleanups(cs)) {
        return ZR_FALSE;
    }
    statement = &node->data.tryCatchFinallyStatement;
    return (TZrBool)(
            statement->finallyBlock != ZR_NULL &&
            (statement->catchClauses == ZR_NULL ||
             statement->catchClauses->count == 0U) &&
            compiler_semantic_cfg_finally_protected_block_flow(
                    statement->block, &completionOpcode) &&
            compiler_semantic_cfg_finally_block_is_linear(
                    statement->finallyBlock));
}

static void compiler_semantic_cfg_finally_fail(
        SZrCompilerState *cs,
        SZrCompilerSemanticFinallyPlan *plan) {
    if (cs != ZR_NULL) {
        if (cs->preSemanticIrCfgActive) {
            (void)compiler_semantic_cfg_abandon(cs);
        }
        cs->preSemanticIrCfgStartupBlocked = ZR_TRUE;
        if (cs->preSemanticIrCfgFinallyPlan == plan) {
            cs->preSemanticIrCfgFinallyPlan = ZR_NULL;
        }
    }
    if (plan != ZR_NULL) {
        memset(plan, 0, sizeof(*plan));
        plan->cleanupBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
        plan->joinBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
        plan->completionBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
        plan->completionValueId = ZR_VALUE_ID_INVALID;
    }
}

TZrBool compiler_semantic_cfg_begin_try_finally(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompilerSemanticFinallyPlan *plan) {
    SZrParserCfg *cfg;
    EZrSemanticIrOpcode completionOpcode = ZR_SEMANTIC_IR_INVALID;

    if (cs == ZR_NULL || node == ZR_NULL || plan == ZR_NULL ||
        !compiler_semantic_cfg_try_finally_is_supported(cs, node)) {
        return ZR_FALSE;
    }
    memset(plan, 0, sizeof(*plan));
    plan->cleanupBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    plan->joinBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    plan->completionBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    plan->completionValueId = ZR_VALUE_ID_INVALID;
    if (!compiler_semantic_cfg_finally_protected_block_flow(
                node->data.tryCatchFinallyStatement.block,
                &completionOpcode)) {
        return ZR_FALSE;
    }
    plan->completionOpcode = completionOpcode;
    if (!compiler_semantic_cfg_ensure_active(cs)) {
        return ZR_FALSE;
    }
    cfg = &cs->preSemanticIr.cfg;
    plan->cleanupBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_CLEANUP,
            node->data.tryCatchFinallyStatement.finallyBlock);
    if (plan->completionOpcode != ZR_SEMANTIC_IR_INVALID) {
        plan->completionBlock = ZrParser_Cfg_AppendBlock(
                cs->state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, node);
    } else {
        plan->joinBlock = ZrParser_Cfg_AppendBlock(
                cs->state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, node);
    }
    if (plan->cleanupBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        (plan->completionOpcode != ZR_SEMANTIC_IR_INVALID
                 ? plan->completionBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID
                 : plan->joinBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID)) {
        compiler_semantic_cfg_finally_fail(cs, plan);
        return ZR_FALSE;
    }
    plan->initialized = ZR_TRUE;
    cs->preSemanticIrCfgFinallyPlan = plan;
    return ZR_TRUE;
}

static TZrBool compiler_semantic_cfg_completion_through_finally_is_active(
        const SZrCompilerState *cs,
        EZrSemanticIrOpcode completionOpcode) {
    const SZrCompilerSemanticFinallyPlan *plan =
            cs != ZR_NULL ? cs->preSemanticIrCfgFinallyPlan : ZR_NULL;

    return (TZrBool)((completionOpcode == ZR_SEMANTIC_IR_RETURN ||
                      completionOpcode == ZR_SEMANTIC_IR_THROW) &&
                     plan != ZR_NULL && plan->initialized &&
                     plan->completionOpcode == completionOpcode &&
                     !plan->completionPending &&
                     !plan->cleanupEntered);
}

static TZrBool compiler_semantic_cfg_redirect_completion_through_finally(
        SZrCompilerState *cs,
        TZrUInt32 valueSlot,
        SZrFileRange range,
        EZrSemanticIrOpcode completionOpcode) {
    SZrCompilerSemanticFinallyPlan *plan;
    TZrValueId valueId;

    if (!compiler_semantic_cfg_completion_through_finally_is_active(
                cs, completionOpcode)) {
        return ZR_FALSE;
    }
    plan = cs->preSemanticIrCfgFinallyPlan;
    valueId = compiler_semantic_ir_slot_value(cs, valueSlot);
    if (valueId == ZR_VALUE_ID_INVALID ||
        !compiler_semantic_cfg_jump_edge(
                cs, plan->cleanupBlock, ZR_PARSER_CFG_EDGE_CLEANUP,
                cs->currentAst, range)) {
        return ZR_FALSE;
    }
    plan->completionValueId = valueId;
    plan->completionRange = range;
    plan->completionPending = ZR_TRUE;
    cs->preSemanticIrCfgBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    cs->preSemanticIrCfgStart =
            (TZrUInt32)cs->preSemanticIr.instructions.length;
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_return_through_finally_is_active(
        const SZrCompilerState *cs) {
    return compiler_semantic_cfg_completion_through_finally_is_active(
            cs, ZR_SEMANTIC_IR_RETURN);
}

TZrBool compiler_semantic_cfg_redirect_return_through_finally(
        SZrCompilerState *cs,
        TZrUInt32 valueSlot,
        SZrFileRange range) {
    return compiler_semantic_cfg_redirect_completion_through_finally(
            cs, valueSlot, range, ZR_SEMANTIC_IR_RETURN);
}

TZrBool compiler_semantic_cfg_throw_through_finally_is_active(
        const SZrCompilerState *cs) {
    return compiler_semantic_cfg_completion_through_finally_is_active(
            cs, ZR_SEMANTIC_IR_THROW);
}

TZrBool compiler_semantic_cfg_redirect_throw_through_finally(
        SZrCompilerState *cs,
        TZrUInt32 valueSlot,
        SZrFileRange range) {
    return compiler_semantic_cfg_redirect_completion_through_finally(
            cs, valueSlot, range, ZR_SEMANTIC_IR_THROW);
}

TZrBool compiler_semantic_cfg_enter_try_finally_cleanup(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompilerSemanticFinallyPlan *plan) {
    if (cs == ZR_NULL || node == ZR_NULL || plan == ZR_NULL ||
        !plan->initialized || plan->cleanupEntered ||
        !cs->preSemanticIrCfgActive ||
        (plan->completionOpcode != ZR_SEMANTIC_IR_INVALID
                 ? (!plan->completionPending ||
                    cs->preSemanticIrCfgBlock !=
                            ZR_PARSER_CFG_INVALID_BLOCK_ID)
                 : (cs->preSemanticIrCfgBlock ==
                            ZR_PARSER_CFG_INVALID_BLOCK_ID ||
                    !compiler_semantic_cfg_jump_edge(
                            cs, plan->cleanupBlock,
                            ZR_PARSER_CFG_EDGE_CLEANUP,
                            node, node->location)))) {
        compiler_semantic_cfg_finally_fail(cs, plan);
        return ZR_FALSE;
    }
    compiler_semantic_cfg_enter(cs, plan->cleanupBlock);
    plan->cleanupEntered = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_complete_try_finally(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompilerSemanticFinallyPlan *plan) {
    if (cs == ZR_NULL || node == ZR_NULL || plan == ZR_NULL ||
        !plan->initialized || !plan->cleanupEntered ||
        !cs->preSemanticIrCfgActive ||
        cs->preSemanticIrCfgBlock != plan->cleanupBlock) {
        compiler_semantic_cfg_finally_fail(cs, plan);
        return ZR_FALSE;
    }
    if (plan->completionOpcode != ZR_SEMANTIC_IR_INVALID) {
        TZrBool terminated;

        if (!plan->completionPending ||
            plan->completionValueId == ZR_VALUE_ID_INVALID ||
            !compiler_semantic_cfg_jump_edge(
                    cs, plan->completionBlock, ZR_PARSER_CFG_EDGE_CLEANUP,
                    node, node->location)) {
            compiler_semantic_cfg_finally_fail(cs, plan);
            return ZR_FALSE;
        }
        compiler_semantic_cfg_enter(cs, plan->completionBlock);
        cs->preSemanticIrCfgFinallyPlan = ZR_NULL;
        if (plan->completionOpcode == ZR_SEMANTIC_IR_RETURN) {
            terminated = compiler_semantic_cfg_terminate_return_value(
                    cs, plan->completionValueId,
                    plan->completionRange);
        } else if (plan->completionOpcode == ZR_SEMANTIC_IR_THROW) {
            terminated = compiler_semantic_cfg_terminate_throw_value(
                    cs, plan->completionValueId,
                    plan->completionRange);
        } else {
            terminated = ZR_FALSE;
        }
        if (!terminated) {
            compiler_semantic_cfg_finally_fail(cs, plan);
            return ZR_FALSE;
        }
    } else {
        if (!compiler_semantic_cfg_jump_edge(
                    cs, plan->joinBlock, ZR_PARSER_CFG_EDGE_CLEANUP,
                    node, node->location)) {
            compiler_semantic_cfg_finally_fail(cs, plan);
            return ZR_FALSE;
        }
        compiler_semantic_cfg_enter(cs, plan->joinBlock);
        cs->preSemanticIrCfgFinallyPlan = ZR_NULL;
    }
    memset(plan, 0, sizeof(*plan));
    plan->cleanupBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    plan->joinBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    plan->completionBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    plan->completionValueId = ZR_VALUE_ID_INVALID;
    return ZR_TRUE;
}
