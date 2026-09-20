#include "compiler_internal.h"

TZrBool compiler_semantic_cfg_loop_body_analyze(
        const SZrAstNode *node,
        TZrBool allowBreak,
        TZrBool allowContinue,
        TZrBool allowFinallyTransfer,
        TZrBool *endsWithBreak) {
    TZrSize index;

    if (endsWithBreak != ZR_NULL) {
        *endsWithBreak = ZR_FALSE;
    }
    if (node == ZR_NULL) {
        return ZR_TRUE;
    }
    if (node->type == ZR_AST_BREAK_CONTINUE_STATEMENT) {
        if (node->data.breakContinueStatement.expr != ZR_NULL ||
            (node->data.breakContinueStatement.isBreak
                     ? !allowBreak
                     : !allowContinue)) {
            return ZR_FALSE;
        }
        if (endsWithBreak != ZR_NULL) {
            *endsWithBreak = node->data.breakContinueStatement.isBreak;
        }
        return ZR_TRUE;
    }
    if (node->type == ZR_AST_TRY_CATCH_FINALLY_STATEMENT) {
        const SZrTryCatchFinallyStatement *statement =
                &node->data.tryCatchFinallyStatement;

        return (TZrBool)(
                allowFinallyTransfer && statement->finallyBlock != ZR_NULL &&
                (statement->catchClauses == ZR_NULL ||
                 statement->catchClauses->count == 0U));
    }
    if (node->type != ZR_AST_BLOCK) {
        return compiler_semantic_cfg_arm_falls_through(node);
    }
    if (node->data.block.body == ZR_NULL) {
        return ZR_TRUE;
    }
    for (index = 0U; index < node->data.block.body->count; index++) {
        const SZrAstNode *statement = node->data.block.body->nodes[index];
        TZrSize trailingIndex;

        if (statement != ZR_NULL &&
            statement->type == ZR_AST_BREAK_CONTINUE_STATEMENT) {
            if (statement->data.breakContinueStatement.expr != ZR_NULL ||
                (statement->data.breakContinueStatement.isBreak
                         ? !allowBreak
                         : !allowContinue)) {
                return ZR_FALSE;
            }
            for (trailingIndex = index + 1U;
                 trailingIndex < node->data.block.body->count;
                 trailingIndex++) {
                if (node->data.block.body->nodes[trailingIndex] != ZR_NULL) {
                    return ZR_FALSE;
                }
            }
            if (endsWithBreak != ZR_NULL) {
                *endsWithBreak =
                        statement->data.breakContinueStatement.isBreak;
            }
            return ZR_TRUE;
        }
        if (statement != ZR_NULL &&
            statement->type == ZR_AST_TRY_CATCH_FINALLY_STATEMENT) {
            const SZrTryCatchFinallyStatement *tryStatement =
                    &statement->data.tryCatchFinallyStatement;

            if (!allowFinallyTransfer ||
                tryStatement->finallyBlock == ZR_NULL ||
                (tryStatement->catchClauses != ZR_NULL &&
                 tryStatement->catchClauses->count != 0U)) {
                return ZR_FALSE;
            }
            continue;
        }
        if (!compiler_semantic_cfg_arm_falls_through(statement)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_for_is_supported(
        const SZrAstNode *node,
        TZrBool *bodyEndsWithBreak) {
    const SZrForLoop *loop;
    TZrBool endsWithBreak = ZR_FALSE;

    if (bodyEndsWithBreak != ZR_NULL) {
        *bodyEndsWithBreak = ZR_FALSE;
    }
    if (node == ZR_NULL || node->type != ZR_AST_FOR_LOOP) {
        return ZR_FALSE;
    }
    loop = &node->data.forLoop;
    if (!loop->isStatement ||
        (loop->cond != ZR_NULL &&
         !compiler_semantic_cfg_expression_is_linear(loop->cond)) ||
        (loop->init != ZR_NULL &&
         !compiler_semantic_cfg_arm_falls_through(loop->init) &&
         !compiler_semantic_cfg_expression_is_linear(loop->init)) ||
        (loop->step != ZR_NULL &&
         !compiler_semantic_cfg_expression_is_linear(loop->step)) ||
        !compiler_semantic_cfg_loop_body_analyze(
                loop->block, ZR_TRUE, ZR_TRUE, ZR_FALSE,
                &endsWithBreak)) {
        return ZR_FALSE;
    }
    if (bodyEndsWithBreak != ZR_NULL) {
        *bodyEndsWithBreak = endsWithBreak;
    }
    return ZR_TRUE;
}

static TZrBool compiler_semantic_cfg_type_requires_cleanup(
        SZrCompilerState *cs,
        const SZrType *typeInfo) {
    SZrInferredType inferredType;
    TZrBool converted;
    TZrBool requiresCleanup;

    if (cs == ZR_NULL || typeInfo == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrParser_InferredType_Init(
            cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    converted = ZrParser_AstTypeToInferredType_Convert(
            cs, typeInfo, &inferredType);
    requiresCleanup = (TZrBool)(
            !converted ||
            compiler_inferred_type_requires_scope_cleanup(
                    cs, &inferredType));
    ZrParser_InferredType_Free(cs->state, &inferredType);
    return requiresCleanup;
}

static TZrBool compiler_semantic_cfg_body_requires_cleanup(
        SZrCompilerState *cs,
        const SZrAstNode *node) {
    TZrSize index;

    if (node == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (node->type) {
        case ZR_AST_VARIABLE_DECLARATION:
            if (node->data.variableDeclaration.typeInfo != ZR_NULL) {
                return compiler_semantic_cfg_type_requires_cleanup(
                        cs, node->data.variableDeclaration.typeInfo);
            }
            /* The foreach binding is not in the type environment during
             * preflight. Keep inferred declarations on the legacy path so a
             * late owner/close cleanup can never abandon emitted ITER ops. */
            return (TZrBool)(
                    node->data.variableDeclaration.value != ZR_NULL);
        case ZR_AST_BLOCK:
            if (node->data.block.body == ZR_NULL) {
                return ZR_FALSE;
            }
            for (index = 0U;
                 index < node->data.block.body->count;
                 index++) {
                if (compiler_semantic_cfg_body_requires_cleanup(
                            cs, node->data.block.body->nodes[index])) {
                    return ZR_TRUE;
                }
            }
            return ZR_FALSE;
        case ZR_AST_IF_EXPRESSION:
            return (TZrBool)(
                    compiler_semantic_cfg_body_requires_cleanup(
                            cs, node->data.ifExpression.thenExpr) ||
                    compiler_semantic_cfg_body_requires_cleanup(
                            cs, node->data.ifExpression.elseExpr));
        case ZR_AST_WHILE_LOOP:
            return compiler_semantic_cfg_body_requires_cleanup(
                    cs, node->data.whileLoop.block);
        default:
            return ZR_FALSE;
    }
}

static TZrBool compiler_semantic_cfg_foreach_conditions_are_supported(
        const SZrAstNode *node) {
    TZrSize index;

    if (node == ZR_NULL) {
        return ZR_TRUE;
    }
    switch (node->type) {
        case ZR_AST_BLOCK:
            if (node->data.block.body == ZR_NULL) {
                return ZR_TRUE;
            }
            for (index = 0U;
                 index < node->data.block.body->count;
                 index++) {
                if (!compiler_semantic_cfg_foreach_conditions_are_supported(
                            node->data.block.body->nodes[index])) {
                    return ZR_FALSE;
                }
            }
            return ZR_TRUE;
        case ZR_AST_IF_EXPRESSION:
            if (!node->data.ifExpression.isStatement ||
                (!compiler_semantic_cfg_expression_is_linear(
                         node->data.ifExpression.condition) &&
                 !compiler_semantic_cfg_short_circuit_is_supported(
                         node->data.ifExpression.condition))) {
                return ZR_FALSE;
            }
            return (TZrBool)(
                    compiler_semantic_cfg_foreach_conditions_are_supported(
                            node->data.ifExpression.thenExpr) &&
                    compiler_semantic_cfg_foreach_conditions_are_supported(
                            node->data.ifExpression.elseExpr));
        case ZR_AST_WHILE_LOOP:
            return compiler_semantic_cfg_foreach_conditions_are_supported(
                    node->data.whileLoop.block);
        default:
            return ZR_TRUE;
    }
}

TZrBool compiler_semantic_cfg_foreach_is_supported(
        SZrCompilerState *cs,
        const SZrAstNode *node) {
    const SZrForeachLoop *loop;

    if (cs == ZR_NULL || node == ZR_NULL ||
        node->type != ZR_AST_FOREACH_LOOP) {
        return ZR_FALSE;
    }
    loop = &node->data.foreachLoop;
    return (TZrBool)(
            loop->isStatement &&
            loop->pattern != ZR_NULL &&
            loop->pattern->type == ZR_AST_IDENTIFIER_LITERAL &&
            loop->pattern->data.identifier.name != ZR_NULL &&
            compiler_semantic_cfg_expression_is_linear(loop->expr) &&
            compiler_semantic_cfg_loop_body_analyze(
                    loop->block, ZR_TRUE, ZR_TRUE, ZR_FALSE,
                    ZR_NULL) &&
            compiler_semantic_cfg_foreach_conditions_are_supported(
                    loop->block) &&
            !compiler_semantic_cfg_body_requires_cleanup(
                    cs, loop->block));
}

TZrBool compiler_semantic_cfg_branch_foreach(
        SZrCompilerState *cs,
        TZrUInt32 conditionSlot,
        SZrAstNode *node,
        TZrUInt32 *currentBlock,
        TZrUInt32 *joinBlock) {
    SZrParserCfg *cfg;

    if (cs == ZR_NULL || node == ZR_NULL ||
        node->type != ZR_AST_FOREACH_LOOP ||
        currentBlock == ZR_NULL || joinBlock == ZR_NULL ||
        !cs->preSemanticIrCfgActive) {
        return ZR_FALSE;
    }
    cfg = &cs->preSemanticIr.cfg;
    *currentBlock = ZrParser_Cfg_AppendBlock(
            cs->state,
            cfg,
            ZR_PARSER_CFG_BLOCK_STATEMENT,
            node);
    *joinBlock = ZrParser_Cfg_AppendBlock(
            cs->state,
            cfg,
            ZR_PARSER_CFG_BLOCK_JOIN,
            node);
    if (*currentBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        *joinBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }
    return compiler_semantic_cfg_branch_while(
            cs,
            conditionSlot,
            node,
            *currentBlock,
            *joinBlock);
}

TZrBool compiler_semantic_cfg_close_infinite_loop(
        SZrCompilerState *cs,
        TZrUInt32 exitBlock) {
    SZrSemanticIrFunction *function;

    if (cs == ZR_NULL || !cs->preSemanticIrCfgActive ||
        exitBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }
    function = &cs->preSemanticIr;
    if (exitBlock >= function->cfg.blocks.length ||
        !ZrParser_SemanticIr_BindBlockRange(
                function,
                &function->cfg,
                exitBlock,
                (TZrUInt32)function->instructions.length,
                0U,
                ZR_PARSER_CFG_TERMINATOR_EXIT)) {
        return ZR_FALSE;
    }
    function->cfg.exitBlockId = exitBlock;
    cs->preSemanticIrCfgTerminated = ZR_TRUE;
    cs->preSemanticIrCfgBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    cs->preSemanticIrCfgStart =
            (TZrUInt32)function->instructions.length;
    return ZR_TRUE;
}
