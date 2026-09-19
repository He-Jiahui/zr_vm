#include "compiler_internal.h"

TZrBool compiler_semantic_cfg_loop_body_analyze(
        const SZrAstNode *node,
        TZrBool allowBreak,
        TZrBool allowContinue,
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
                loop->block, ZR_TRUE, ZR_TRUE, &endsWithBreak) ||
        (loop->cond == ZR_NULL && !endsWithBreak)) {
        return ZR_FALSE;
    }
    if (bodyEndsWithBreak != ZR_NULL) {
        *bodyEndsWithBreak = endsWithBreak;
    }
    return ZR_TRUE;
}
