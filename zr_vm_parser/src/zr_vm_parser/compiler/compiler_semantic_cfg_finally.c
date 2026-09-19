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

TZrBool compiler_semantic_cfg_try_finally_is_supported(
        SZrCompilerState *cs,
        const SZrAstNode *node) {
    const SZrTryCatchFinallyStatement *statement;

    if (cs == ZR_NULL || node == ZR_NULL ||
        node->type != ZR_AST_TRY_CATCH_FINALLY_STATEMENT ||
        cs->preSemanticIrCfgTerminated ||
        cs->preSemanticIrCfgStartupSuppressed ||
        cs->preSemanticIrCfgStartupBlocked ||
        (cs->preSemanticIrCfgActive &&
         cs->preSemanticIrCfgBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID) ||
        cs->preSemanticIrCfgCatchBlock != ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        compiler_has_active_scope_ownership_cleanups(cs)) {
        return ZR_FALSE;
    }
    statement = &node->data.tryCatchFinallyStatement;
    return (TZrBool)(
            statement->finallyBlock != ZR_NULL &&
            (statement->catchClauses == ZR_NULL ||
             statement->catchClauses->count == 0U) &&
            compiler_semantic_cfg_finally_block_is_linear(statement->block) &&
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
    }
    if (plan != ZR_NULL) {
        memset(plan, 0, sizeof(*plan));
        plan->cleanupBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
        plan->joinBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    }
}

TZrBool compiler_semantic_cfg_begin_try_finally(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompilerSemanticFinallyPlan *plan) {
    SZrParserCfg *cfg;

    if (cs == ZR_NULL || node == ZR_NULL || plan == ZR_NULL ||
        !compiler_semantic_cfg_try_finally_is_supported(cs, node)) {
        return ZR_FALSE;
    }
    memset(plan, 0, sizeof(*plan));
    plan->cleanupBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    plan->joinBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    if (!compiler_semantic_cfg_ensure_active(cs)) {
        return ZR_FALSE;
    }
    cfg = &cs->preSemanticIr.cfg;
    plan->cleanupBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_CLEANUP,
            node->data.tryCatchFinallyStatement.finallyBlock);
    plan->joinBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, node);
    if (plan->cleanupBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        plan->joinBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        compiler_semantic_cfg_finally_fail(cs, plan);
        return ZR_FALSE;
    }
    plan->initialized = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_enter_try_finally_cleanup(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompilerSemanticFinallyPlan *plan) {
    if (cs == ZR_NULL || node == ZR_NULL || plan == ZR_NULL ||
        !plan->initialized || plan->cleanupEntered ||
        !cs->preSemanticIrCfgActive ||
        cs->preSemanticIrCfgBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        !compiler_semantic_cfg_jump_edge(
                cs, plan->cleanupBlock, ZR_PARSER_CFG_EDGE_CLEANUP,
                node, node->location)) {
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
        cs->preSemanticIrCfgBlock != plan->cleanupBlock ||
        !compiler_semantic_cfg_jump_edge(
                cs, plan->joinBlock, ZR_PARSER_CFG_EDGE_CLEANUP,
                node, node->location)) {
        compiler_semantic_cfg_finally_fail(cs, plan);
        return ZR_FALSE;
    }
    compiler_semantic_cfg_enter(cs, plan->joinBlock);
    memset(plan, 0, sizeof(*plan));
    plan->cleanupBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    plan->joinBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    return ZR_TRUE;
}
