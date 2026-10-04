#include "compiler_internal.h"
#include "compiler_semantic_cfg_loop.h"

typedef enum EZrCompilerWhileBreakFlow {
    ZR_COMPILER_WHILE_BREAK_UNSUPPORTED = 0,
    ZR_COMPILER_WHILE_BREAK_FALLS_THROUGH,
    ZR_COMPILER_WHILE_BREAK_TERMINATES
} EZrCompilerWhileBreakFlow;

/* This fallback models plain conditional breaks only. Continue and cleanup
 * transfers retain the existing shared analyzer's admission rules. */
static EZrCompilerWhileBreakFlow compiler_while_break_body_flow(
        const SZrAstNode *node) {
    TZrSize index;
    if (node == ZR_NULL) return ZR_COMPILER_WHILE_BREAK_FALLS_THROUGH;
    if (node->type == ZR_AST_BREAK_CONTINUE_STATEMENT) {
        return node->data.breakContinueStatement.isBreak &&
                       node->data.breakContinueStatement.expr == ZR_NULL
                       ? ZR_COMPILER_WHILE_BREAK_TERMINATES
                       : ZR_COMPILER_WHILE_BREAK_UNSUPPORTED;
    }
    if (node->type == ZR_AST_IF_EXPRESSION) {
        EZrCompilerWhileBreakFlow thenFlow;
        EZrCompilerWhileBreakFlow elseFlow;
        if (!node->data.ifExpression.isStatement ||
            !compiler_semantic_cfg_loop_condition_is_supported(
                    node->data.ifExpression.condition)) {
            return ZR_COMPILER_WHILE_BREAK_UNSUPPORTED;
        }
        thenFlow = compiler_while_break_body_flow(node->data.ifExpression.thenExpr);
        elseFlow = compiler_while_break_body_flow(node->data.ifExpression.elseExpr);
        if (thenFlow == ZR_COMPILER_WHILE_BREAK_UNSUPPORTED ||
            elseFlow == ZR_COMPILER_WHILE_BREAK_UNSUPPORTED) {
            return ZR_COMPILER_WHILE_BREAK_UNSUPPORTED;
        }
        return thenFlow == ZR_COMPILER_WHILE_BREAK_TERMINATES &&
                       elseFlow == ZR_COMPILER_WHILE_BREAK_TERMINATES
                       ? ZR_COMPILER_WHILE_BREAK_TERMINATES
                       : ZR_COMPILER_WHILE_BREAK_FALLS_THROUGH;
    }
    if (node->type != ZR_AST_BLOCK) {
        return compiler_semantic_cfg_arm_falls_through(node)
                       ? ZR_COMPILER_WHILE_BREAK_FALLS_THROUGH
                       : ZR_COMPILER_WHILE_BREAK_UNSUPPORTED;
    }
    if (node->data.block.body == ZR_NULL) {
        return ZR_COMPILER_WHILE_BREAK_FALLS_THROUGH;
    }
    for (index = 0U; index < node->data.block.body->count; ++index) {
        EZrCompilerWhileBreakFlow flow = compiler_while_break_body_flow(
                node->data.block.body->nodes[index]);
        TZrSize trailingIndex;
        if (flow == ZR_COMPILER_WHILE_BREAK_UNSUPPORTED) return flow;
        if (flow != ZR_COMPILER_WHILE_BREAK_TERMINATES) continue;
        for (trailingIndex = index + 1U;
             trailingIndex < node->data.block.body->count; ++trailingIndex) {
            if (node->data.block.body->nodes[trailingIndex] != ZR_NULL) {
                return ZR_COMPILER_WHILE_BREAK_UNSUPPORTED;
            }
        }
        return flow;
    }
    return ZR_COMPILER_WHILE_BREAK_FALLS_THROUGH;
}

TZrBool compiler_semantic_cfg_while_body_is_supported(const SZrAstNode *node) {
    return (TZrBool)(compiler_semantic_cfg_loop_body_analyze(
                            node, ZR_TRUE, ZR_TRUE, ZR_TRUE, ZR_NULL) ||
                    compiler_while_break_body_flow(node) !=
                            ZR_COMPILER_WHILE_BREAK_UNSUPPORTED);
}

/* CFG 只接管可按现有表达式/短路分支规则建模的循环条件；其余情况留给传统编译路径。 */
TZrBool compiler_semantic_cfg_loop_condition_is_supported(
        const SZrAstNode *node) {
    return (TZrBool)(compiler_semantic_cfg_expression_is_linear(node) ||
                     compiler_semantic_cfg_short_circuit_is_supported(node));
}

/* 预检循环体中的 break/continue 形状，避免 CFG 已接管后才遇到无法表达的控制转移。
 * endsWithBreak 专供 for 布局决策：末尾 break 时不应再建立可达 step 块。 */
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
    /* 仅把最后一个有效语句的 break/continue 作为循环体出口；后续非空语句意味着此简化 CFG 不适用。 */
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

            /* try/finally 的完整 lowering 另有准入检查；此处只校验循环转移不会穿过 catch 分支。 */
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

/* for CFG 的准入检查同时约束 init/condition/step 和循环体；输出只描述末尾 break。 */
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
         !compiler_semantic_cfg_loop_condition_is_supported(loop->cond)) ||
        (loop->init != ZR_NULL &&
         !compiler_semantic_cfg_arm_falls_through(loop->init) &&
         !compiler_semantic_cfg_expression_is_linear(loop->init)) ||
        (loop->step != ZR_NULL &&
         !compiler_semantic_cfg_expression_is_linear(loop->step)) ||
        !compiler_semantic_cfg_loop_body_analyze(
                loop->block, ZR_TRUE, ZR_TRUE, ZR_TRUE,
                &endsWithBreak)) {
        return ZR_FALSE;
    }
    if (bodyEndsWithBreak != ZR_NULL) {
        *bodyEndsWithBreak = endsWithBreak;
    }
    return ZR_TRUE;
}

/* 无法可靠转换类型时按“需要清理”处理，使 foreach 退回旧路径而非漏掉资源收尾。 */
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

/* 只遍历此预检认识的局部声明、块、分支和 while；未知 AST 形态不据此判定需清理。 */
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
            /* 预检时 foreach 绑定尚未进入类型环境；推断声明保留旧路径，
             * 避免迭代指令已发射后才因 owner/close 清理要求放弃 CFG。 */
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

/* foreach 内嵌 if 的条件也须能拆成 CFG 分支；只接受语句式 if 并递归检查分支体。 */
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

/* foreach 仅在绑定、迭代源、控制转移、嵌套条件和清理均可预检时启用语义 CFG。
 * 这是保守准入门；失败后调用方放弃 CFG，并继续其传统迭代 lowering。 */
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
                    loop->block, ZR_TRUE, ZR_TRUE, ZR_TRUE,
                    ZR_NULL) &&
            compiler_semantic_cfg_foreach_conditions_are_supported(
                    loop->block) &&
            !compiler_semantic_cfg_body_requires_cleanup(
                    cs, loop->block));
}

/* 在迭代器 move-next 条件已生成后接入 foreach 的 body/join CFG 边。
 * 失败可发生在部分 block 已追加后，调用方必须走统一 CFG abandon 清理路径。 */
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

/* 为无条件且可达的 for 循环封闭显式 CFG exit，避免把回边误当函数正常出口。
 * 只接受活动 CFG 中有效的 exit block；成功后状态标记为 terminated。 */
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
