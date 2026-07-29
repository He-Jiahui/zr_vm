#include "cfg_internal.h"

#include "zr_vm_core/string.h"
#include "zr_vm_parser/semantic.h"

static TZrBool cfg_statement_is_terminator(SZrAstNode *statement,
                                           EZrSemanticReachabilityCause *outCause) {
    if (outCause != ZR_NULL) {
        *outCause = ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN;
    }

    if (statement == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (statement->type) {
        case ZR_AST_RETURN_STATEMENT:
            if (outCause != ZR_NULL) {
                *outCause = ZR_SEMANTIC_REACHABILITY_AFTER_RETURN;
            }
            return ZR_TRUE;
        case ZR_AST_THROW_STATEMENT:
            if (outCause != ZR_NULL) {
                *outCause = ZR_SEMANTIC_REACHABILITY_AFTER_THROW;
            }
            return ZR_TRUE;
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            if (outCause != ZR_NULL) {
                *outCause = statement->data.breakContinueStatement.isBreak
                                    ? ZR_SEMANTIC_REACHABILITY_AFTER_BREAK
                                    : ZR_SEMANTIC_REACHABILITY_AFTER_CONTINUE;
            }
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static SZrAstNode *cfg_statement_direct_await(SZrAstNode *statement) {
    SZrAstNode *expression = ZR_NULL;

    if (statement == ZR_NULL) {
        return ZR_NULL;
    }

    switch (statement->type) {
        case ZR_AST_EXPRESSION_STATEMENT:
            expression = statement->data.expressionStatement.expr;
            break;
        case ZR_AST_RETURN_STATEMENT:
            expression = statement->data.returnStatement.expr;
            break;
        case ZR_AST_THROW_STATEMENT:
            expression = statement->data.throwStatement.expr;
            break;
        case ZR_AST_OUT_STATEMENT:
            expression = statement->data.outStatement.expr;
            break;
        case ZR_AST_YIELD_STATEMENT:
            expression = statement->data.yieldStatement.expr;
            break;
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            expression = statement->data.breakContinueStatement.expr;
            break;
        case ZR_AST_VARIABLE_DECLARATION:
            expression = statement->data.variableDeclaration.value;
            break;
        default:
            return ZR_NULL;
    }

    return expression != ZR_NULL && expression->type == ZR_AST_AWAIT_EXPRESSION
                   ? expression
                   : ZR_NULL;
}

static const SZrParserCfgLoopTargets g_cfg_no_loop_targets = {
    ZR_PARSER_CFG_INVALID_BLOCK_ID,
    ZR_PARSER_CFG_INVALID_BLOCK_ID,
};

static TZrBool cfg_connect_loop_control_target(SZrParserCfg *cfg,
                                               SZrParserCfgBlock *block,
                                               const SZrParserCfgLoopTargets *loopTargets) {
    SZrAstNode *statement;

    if (cfg == ZR_NULL || block == ZR_NULL) {
        return ZR_FALSE;
    }
    if (loopTargets == ZR_NULL) {
        loopTargets = &g_cfg_no_loop_targets;
    }

    statement = block->statement;
    if (statement == ZR_NULL || statement->type != ZR_AST_BREAK_CONTINUE_STATEMENT) {
        return ZR_TRUE;
    }

    if (statement->data.breakContinueStatement.isBreak) {
        return loopTargets->breakTargetBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID
                       ? ZR_TRUE
                       : cfg_add_edge_kind(
                                 cfg,
                                 block->id,
                                 loopTargets->breakTargetBlockId,
                                 ZR_PARSER_CFG_EDGE_NORMAL,
                                 statement);
    }

    return loopTargets->continueTargetBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID
                   ? ZR_TRUE
                   : cfg_add_edge_kind(
                             cfg,
                             block->id,
                             loopTargets->continueTargetBlockId,
                             ZR_PARSER_CFG_EDGE_NORMAL,
                             statement);
}

static SZrAstNodeArray *cfg_statement_array_from_root(SZrAstNode *root) {
    if (root == ZR_NULL) {
        return ZR_NULL;
    }

    switch (root->type) {
        case ZR_AST_SCRIPT:
            return root->data.script.statements;
        case ZR_AST_BLOCK:
            return root->data.block.body;
        case ZR_AST_FUNCTION_DECLARATION:
            return root->data.functionDeclaration.body != ZR_NULL &&
                           root->data.functionDeclaration.body->type == ZR_AST_BLOCK
                       ? root->data.functionDeclaration.body->data.block.body
                       : ZR_NULL;
        default:
            return ZR_NULL;
    }
}

static TZrBool cfg_build_statement_list(SZrState *state,
                                        SZrParserCfg *cfg,
                                        SZrAstNodeArray *statements,
                                        TZrUInt32 *inOutPreviousBlockId,
                                        EZrSemanticReachabilityCause inheritedCause,
                                        SZrAstNode *inheritedCauseNode,
                                        const SZrParserCfgLoopTargets *loopTargets);

static TZrBool cfg_compare_equal_value(TZrBool valuesEqual,
                                       const TZrChar *op,
                                       TZrBool *outValue) {
    if (op == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if (op[0] == '=' && op[1] == '=' && op[2] == '\0') {
        *outValue = valuesEqual;
        return ZR_TRUE;
    }
    if (op[0] == '!' && op[1] == '=' && op[2] == '\0') {
        *outValue = (TZrBool)!valuesEqual;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool cfg_compare_integer_values(TZrInt64 leftValue,
                                          const TZrChar *op,
                                          TZrInt64 rightValue,
                                          TZrBool *outValue) {
    if (cfg_compare_equal_value((TZrBool)(leftValue == rightValue), op, outValue)) {
        return ZR_TRUE;
    }
    if (op == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if (op[0] == '<' && op[1] == '\0') {
        *outValue = (TZrBool)(leftValue < rightValue);
        return ZR_TRUE;
    }
    if (op[0] == '>' && op[1] == '\0') {
        *outValue = (TZrBool)(leftValue > rightValue);
        return ZR_TRUE;
    }
    if (op[0] == '<' && op[1] == '=' && op[2] == '\0') {
        *outValue = (TZrBool)(leftValue <= rightValue);
        return ZR_TRUE;
    }
    if (op[0] == '>' && op[1] == '=' && op[2] == '\0') {
        *outValue = (TZrBool)(leftValue >= rightValue);
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool cfg_compare_float_values(TZrDouble leftValue,
                                        const TZrChar *op,
                                        TZrDouble rightValue,
                                        TZrBool *outValue) {
    if (cfg_compare_equal_value((TZrBool)(leftValue == rightValue), op, outValue)) {
        return ZR_TRUE;
    }
    if (op == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if (op[0] == '<' && op[1] == '\0') {
        *outValue = (TZrBool)(leftValue < rightValue);
        return ZR_TRUE;
    }
    if (op[0] == '>' && op[1] == '\0') {
        *outValue = (TZrBool)(leftValue > rightValue);
        return ZR_TRUE;
    }
    if (op[0] == '<' && op[1] == '=' && op[2] == '\0') {
        *outValue = (TZrBool)(leftValue <= rightValue);
        return ZR_TRUE;
    }
    if (op[0] == '>' && op[1] == '=' && op[2] == '\0') {
        *outValue = (TZrBool)(leftValue >= rightValue);
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool cfg_compare_char_values(TZrChar leftValue,
                                       const TZrChar *op,
                                       TZrChar rightValue,
                                       TZrBool *outValue) {
    if (cfg_compare_equal_value((TZrBool)(leftValue == rightValue), op, outValue)) {
        return ZR_TRUE;
    }
    if (op == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if (op[0] == '<' && op[1] == '\0') {
        *outValue = (TZrBool)(leftValue < rightValue);
        return ZR_TRUE;
    }
    if (op[0] == '>' && op[1] == '\0') {
        *outValue = (TZrBool)(leftValue > rightValue);
        return ZR_TRUE;
    }
    if (op[0] == '<' && op[1] == '=' && op[2] == '\0') {
        *outValue = (TZrBool)(leftValue <= rightValue);
        return ZR_TRUE;
    }
    if (op[0] == '>' && op[1] == '=' && op[2] == '\0') {
        *outValue = (TZrBool)(leftValue >= rightValue);
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool cfg_compare_constant_values(
        const SZrParserCfgConstant *leftValue,
        const TZrChar *op,
        const SZrParserCfgConstant *rightValue,
        TZrBool *outValue) {
    if (leftValue == ZR_NULL ||
        rightValue == ZR_NULL ||
        op == ZR_NULL ||
        outValue == ZR_NULL ||
        !cfg_constants_can_compare(leftValue, rightValue)) {
        return ZR_FALSE;
    }
    if (cfg_compare_equal_value(cfg_constants_equal(leftValue, rightValue),
                                op,
                                outValue)) {
        return ZR_TRUE;
    }
    if (leftValue->kind != rightValue->kind) {
        return ZR_FALSE;
    }

    switch (leftValue->kind) {
        case ZR_PARSER_CFG_CONSTANT_INTEGER:
            return cfg_compare_integer_values(leftValue->integerValue,
                                              op,
                                              rightValue->integerValue,
                                              outValue);
        case ZR_PARSER_CFG_CONSTANT_FLOAT:
            return cfg_compare_float_values(leftValue->floatValue,
                                            op,
                                            rightValue->floatValue,
                                            outValue);
        case ZR_PARSER_CFG_CONSTANT_CHAR:
            return cfg_compare_char_values(leftValue->charValue,
                                           op,
                                           rightValue->charValue,
                                           outValue);
        default:
            return ZR_FALSE;
    }
}

static TZrBool cfg_binary_comparison_bool_constant(SZrAstNode *leftNode,
                                                   const TZrChar *op,
                                                   SZrAstNode *rightNode,
                                                   TZrBool *outValue) {
    SZrParserCfgConstant leftConstant;
    SZrParserCfgConstant rightConstant;

    if (leftNode == ZR_NULL || rightNode == ZR_NULL || op == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (cfg_node_constant(leftNode, &leftConstant) &&
        cfg_node_constant(rightNode, &rightConstant) &&
        cfg_compare_constant_values(&leftConstant,
                                    op,
                                    &rightConstant,
                                    outValue)) {
        return ZR_TRUE;
    }

    if (leftNode->type == ZR_AST_INTEGER_LITERAL && rightNode->type == ZR_AST_INTEGER_LITERAL) {
        return cfg_compare_integer_values(leftNode->data.integerLiteral.value,
                                          op,
                                          rightNode->data.integerLiteral.value,
                                          outValue);
    }

    if (leftNode->type == ZR_AST_FLOAT_LITERAL && rightNode->type == ZR_AST_FLOAT_LITERAL) {
        return cfg_compare_float_values(leftNode->data.floatLiteral.value,
                                        op,
                                        rightNode->data.floatLiteral.value,
                                        outValue);
    }

    if (leftNode->type == ZR_AST_CHAR_LITERAL && rightNode->type == ZR_AST_CHAR_LITERAL) {
        if (leftNode->data.charLiteral.hasError || rightNode->data.charLiteral.hasError) {
            return ZR_FALSE;
        }
        return cfg_compare_char_values(leftNode->data.charLiteral.value,
                                       op,
                                       rightNode->data.charLiteral.value,
                                       outValue);
    }

    if (leftNode->type == ZR_AST_STRING_LITERAL && rightNode->type == ZR_AST_STRING_LITERAL) {
        if (leftNode->data.stringLiteral.hasError ||
            rightNode->data.stringLiteral.hasError ||
            leftNode->data.stringLiteral.value == ZR_NULL ||
            rightNode->data.stringLiteral.value == ZR_NULL) {
            return ZR_FALSE;
        }
        return cfg_compare_equal_value(
            ZrCore_String_Equal(leftNode->data.stringLiteral.value, rightNode->data.stringLiteral.value),
            op,
            outValue);
    }

    return ZR_FALSE;
}

TZrBool cfg_node_bool_constant(SZrAstNode *node, TZrBool *outValue) {
    TZrBool argumentValue = ZR_FALSE;
    TZrBool leftValue = ZR_FALSE;
    TZrBool rightValue = ZR_FALSE;
    TZrBool leftKnown = ZR_FALSE;
    TZrBool rightKnown = ZR_FALSE;
    const TZrChar *op;

    if (outValue != ZR_NULL) {
        *outValue = ZR_FALSE;
    }
    if (node == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_BOOLEAN_LITERAL:
            *outValue = node->data.booleanLiteral.value;
            return ZR_TRUE;
        case ZR_AST_UNARY_EXPRESSION:
            if (node->data.unaryExpression.op.op != ZR_NULL &&
                node->data.unaryExpression.op.op[0] == '!' &&
                node->data.unaryExpression.op.op[1] == '\0' &&
                cfg_node_bool_constant(node->data.unaryExpression.argument, &argumentValue)) {
                *outValue = (TZrBool)!argumentValue;
                return ZR_TRUE;
            }
            return ZR_FALSE;
        case ZR_AST_BINARY_EXPRESSION:
            op = node->data.binaryExpression.op.op;
            return cfg_binary_comparison_bool_constant(node->data.binaryExpression.left,
                                                       op,
                                                       node->data.binaryExpression.right,
                                                       outValue);
        case ZR_AST_LOGICAL_EXPRESSION:
            op = node->data.logicalExpression.op;
            if (op == ZR_NULL) {
                return ZR_FALSE;
            }
            leftKnown = cfg_node_bool_constant(node->data.logicalExpression.left, &leftValue);
            rightKnown = cfg_node_bool_constant(node->data.logicalExpression.right, &rightValue);
            if (op[0] == '&' && op[1] == '&' && op[2] == '\0') {
                if ((leftKnown && !leftValue) || (rightKnown && !rightValue)) {
                    *outValue = ZR_FALSE;
                    return ZR_TRUE;
                }
                if (leftKnown && rightKnown) {
                    *outValue = (TZrBool)(leftValue && rightValue);
                    return ZR_TRUE;
                }
                return ZR_FALSE;
            }
            if (op[0] == '|' && op[1] == '|' && op[2] == '\0') {
                if ((leftKnown && leftValue) || (rightKnown && rightValue)) {
                    *outValue = ZR_TRUE;
                    return ZR_TRUE;
                }
                if (leftKnown && rightKnown) {
                    *outValue = (TZrBool)(leftValue || rightValue);
                    return ZR_TRUE;
                }
                return ZR_FALSE;
            }
            return ZR_FALSE;
        default:
            return ZR_FALSE;
    }
}

TZrBool cfg_connect_fallthrough(SZrParserCfg *cfg, TZrUInt32 fromId, TZrUInt32 toId) {
    SZrParserCfgBlock *from = cfg_get_block(cfg, fromId);

    if (from == ZR_NULL) {
        return ZR_TRUE;
    }
    if (from->isTerminator) {
        return ZR_TRUE;
    }
    return cfg_add_edge(cfg, fromId, toId);
}

TZrBool cfg_build_statement_body(SZrState *state,
                                 SZrParserCfg *cfg,
                                 SZrAstNode *body,
                                 TZrUInt32 predecessorBlockId,
                                 EZrSemanticReachabilityCause inheritedCause,
                                 SZrAstNode *inheritedCauseNode,
                                 const SZrParserCfgLoopTargets *loopTargets,
                                 TZrUInt32 *outLastBlockId) {
    SZrAstNodeArray *statements;

    if (outLastBlockId != ZR_NULL) {
        *outLastBlockId = predecessorBlockId;
    }
    if (state == ZR_NULL || cfg == ZR_NULL || outLastBlockId == ZR_NULL) {
        return ZR_FALSE;
    }
    if (body == ZR_NULL) {
        return ZR_TRUE;
    }

    statements = cfg_statement_array_from_root(body);
    if (statements != ZR_NULL) {
        return cfg_build_statement_list(state,
                                        cfg,
                                        statements,
                                        outLastBlockId,
                                        inheritedCause,
                                        inheritedCauseNode,
                                        loopTargets);
    }

    {
        SZrAstNodeArray singleStatementArray;
        SZrAstNode *singleStatement = body;

        singleStatementArray.nodes = &singleStatement;
        singleStatementArray.count = 1;
        singleStatementArray.capacity = 1;
        return cfg_build_statement_list(state,
                                        cfg,
                                        &singleStatementArray,
                                        outLastBlockId,
                                        inheritedCause,
                                        inheritedCauseNode,
                                        loopTargets);
    }
}

static TZrBool cfg_build_if_statement(SZrState *state,
                                      SZrParserCfg *cfg,
                                      SZrAstNode *statement,
                                      TZrUInt32 *inOutPreviousBlockId,
                                      EZrSemanticReachabilityCause pendingCause,
                                      SZrAstNode *pendingCauseNode,
                                      const SZrParserCfgLoopTargets *loopTargets) {
    SZrParserCfgBlock *ifBlock;
    TZrUInt32 ifBlockId;
    TZrUInt32 joinBlockId;
    TZrUInt32 thenPreviousBlockId;
    TZrUInt32 thenLastBlockId;
    TZrUInt32 elsePreviousBlockId;
    TZrUInt32 elseLastBlockId;
    TZrSize thenEdgeIndex;
    TZrSize elseEdgeIndex;
    TZrBool conditionValue = ZR_FALSE;
    TZrBool hasConstantCondition;
    TZrBool includeThenBranch;
    TZrBool includeElseBranch;

    if (state == ZR_NULL || cfg == ZR_NULL || statement == ZR_NULL ||
        inOutPreviousBlockId == ZR_NULL) {
        return ZR_FALSE;
    }

    ifBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, statement);
    ifBlock = cfg_get_block(cfg, ifBlockId);
    if (ifBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID || ifBlock == ZR_NULL) {
        return ZR_FALSE;
    }

    if (pendingCause != ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN) {
        ifBlock->unreachableCause = pendingCause;
        ifBlock->unreachableCauseNode = pendingCauseNode;
    }
    if (!cfg_connect_fallthrough(cfg, *inOutPreviousBlockId, ifBlockId)) {
        return ZR_FALSE;
    }

    hasConstantCondition = cfg_node_bool_constant(statement->data.ifExpression.condition, &conditionValue);
    includeThenBranch = !hasConstantCondition || conditionValue;
    includeElseBranch = !hasConstantCondition || !conditionValue;

    thenPreviousBlockId = includeThenBranch ? ifBlockId : ZR_PARSER_CFG_INVALID_BLOCK_ID;
    thenEdgeIndex = cfg_get_block(cfg, ifBlockId)->successorCount;
    if (!cfg_build_statement_body(state,
                                  cfg,
                                  statement->data.ifExpression.thenExpr,
                                  thenPreviousBlockId,
                                  includeThenBranch ? pendingCause : ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH,
                                  includeThenBranch ? pendingCauseNode : statement->data.ifExpression.condition,
                                  loopTargets,
                                  &thenLastBlockId)) {
        return ZR_FALSE;
    }
    if (includeThenBranch &&
        cfg_get_block(cfg, ifBlockId)->successorCount > thenEdgeIndex &&
        !cfg_retag_edge_at(
                cfg,
                ifBlockId,
                thenEdgeIndex,
                ZR_PARSER_CFG_EDGE_TRUE_BRANCH,
                statement->data.ifExpression.condition)) {
        return ZR_FALSE;
    }

    elsePreviousBlockId = includeElseBranch ? ifBlockId : ZR_PARSER_CFG_INVALID_BLOCK_ID;
    elseEdgeIndex = cfg_get_block(cfg, ifBlockId)->successorCount;
    if (!cfg_build_statement_body(state,
                                  cfg,
                                  statement->data.ifExpression.elseExpr,
                                  elsePreviousBlockId,
                                  includeElseBranch ? pendingCause : ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH,
                                  includeElseBranch ? pendingCauseNode : statement->data.ifExpression.condition,
                                  loopTargets,
                                  &elseLastBlockId)) {
        return ZR_FALSE;
    }
    if (includeElseBranch &&
        cfg_get_block(cfg, ifBlockId)->successorCount > elseEdgeIndex &&
        !cfg_retag_edge_at(
                cfg,
                ifBlockId,
                elseEdgeIndex,
                ZR_PARSER_CFG_EDGE_FALSE_BRANCH,
                statement->data.ifExpression.condition)) {
        return ZR_FALSE;
    }

    joinBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, ZR_NULL);
    if (joinBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }

    if (includeThenBranch && thenLastBlockId == ifBlockId) {
        if (!cfg_add_edge_kind(cfg,
                               ifBlockId,
                               joinBlockId,
                               ZR_PARSER_CFG_EDGE_TRUE_BRANCH,
                               statement->data.ifExpression.condition)) {
            return ZR_FALSE;
        }
    } else if (thenLastBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
               !cfg_connect_fallthrough(cfg, thenLastBlockId, joinBlockId)) {
        return ZR_FALSE;
    }

    if (statement->data.ifExpression.elseExpr == ZR_NULL) {
        if (includeElseBranch &&
            !cfg_add_edge_kind(cfg,
                               ifBlockId,
                               joinBlockId,
                               ZR_PARSER_CFG_EDGE_FALSE_BRANCH,
                               statement->data.ifExpression.condition)) {
            return ZR_FALSE;
        }
    } else if (includeElseBranch && elseLastBlockId == ifBlockId) {
        if (!cfg_add_edge_kind(cfg,
                               ifBlockId,
                               joinBlockId,
                               ZR_PARSER_CFG_EDGE_FALSE_BRANCH,
                               statement->data.ifExpression.condition)) {
            return ZR_FALSE;
        }
    } else if (elseLastBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
               !cfg_connect_fallthrough(cfg, elseLastBlockId, joinBlockId)) {
        return ZR_FALSE;
    }

    *inOutPreviousBlockId = joinBlockId;
    return ZR_TRUE;
}

static TZrBool cfg_build_statement_list(SZrState *state,
                                        SZrParserCfg *cfg,
                                        SZrAstNodeArray *statements,
                                        TZrUInt32 *inOutPreviousBlockId,
                                        EZrSemanticReachabilityCause inheritedCause,
                                        SZrAstNode *inheritedCauseNode,
                                        const SZrParserCfgLoopTargets *loopTargets) {
    TZrSize index;
    EZrSemanticReachabilityCause pendingCause = inheritedCause;
    SZrAstNode *pendingCauseNode = inheritedCauseNode;

    if (state == ZR_NULL || cfg == ZR_NULL || inOutPreviousBlockId == ZR_NULL) {
        return ZR_FALSE;
    }
    if (statements == ZR_NULL || statements->nodes == ZR_NULL) {
        return ZR_TRUE;
    }

    for (index = 0; index < statements->count; index++) {
        SZrAstNode *statement = statements->nodes[index];
        SZrAstNode *awaitNode;
        TZrUInt32 blockId;
        TZrUInt32 resumeBlockId;
        TZrUInt32 suspensionBlockId;
        SZrParserCfgBlock *block;
        EZrSemanticReachabilityCause terminatorCause;

        if (statement != ZR_NULL && statement->type == ZR_AST_IF_EXPRESSION &&
            statement->data.ifExpression.isStatement) {
            if (!cfg_build_if_statement(state,
                                        cfg,
                                        statement,
                                        inOutPreviousBlockId,
                                        pendingCause,
                                        pendingCauseNode,
                                        loopTargets)) {
                return ZR_FALSE;
            }
            continue;
        }
        if (statement != ZR_NULL && statement->type == ZR_AST_WHILE_LOOP &&
            statement->data.whileLoop.isStatement) {
            if (!cfg_build_while_statement(state,
                                           cfg,
                                           statement,
                                           inOutPreviousBlockId,
                                           pendingCause,
                                           pendingCauseNode)) {
                return ZR_FALSE;
            }
            continue;
        }
        if (statement != ZR_NULL && statement->type == ZR_AST_SWITCH_EXPRESSION &&
            statement->data.switchExpression.isStatement) {
            if (!cfg_build_switch_statement(state,
                                            cfg,
                                            statement,
                                            inOutPreviousBlockId,
                                            pendingCause,
                                            pendingCauseNode,
                                            loopTargets)) {
                return ZR_FALSE;
            }
            continue;
        }
        if (statement != ZR_NULL && statement->type == ZR_AST_FOR_LOOP &&
            statement->data.forLoop.isStatement) {
            if (!cfg_build_for_statement(state,
                                         cfg,
                                         statement,
                                         inOutPreviousBlockId,
                                         pendingCause,
                                         pendingCauseNode)) {
                return ZR_FALSE;
            }
            continue;
        }
        if (statement != ZR_NULL && statement->type == ZR_AST_FOREACH_LOOP &&
            statement->data.foreachLoop.isStatement) {
            if (!cfg_build_foreach_statement(state,
                                             cfg,
                                             statement,
                                             inOutPreviousBlockId,
                                             pendingCause,
                                             pendingCauseNode)) {
                return ZR_FALSE;
            }
            continue;
        }
        if (statement != ZR_NULL && statement->type == ZR_AST_TRY_CATCH_FINALLY_STATEMENT) {
            if (!cfg_build_try_statement(state,
                                         cfg,
                                         statement,
                                         inOutPreviousBlockId,
                                         pendingCause,
                                         pendingCauseNode,
                                         loopTargets)) {
                return ZR_FALSE;
            }
            continue;
        }
        if (statement != ZR_NULL &&
            statement->type == ZR_AST_USING_STATEMENT &&
            statement->data.usingStatement.isBlockScoped &&
            statement->data.usingStatement.guardKind == ZR_USING_GUARD_DROP &&
            statement->data.usingStatement.body != ZR_NULL) {
            if (!cfg_build_using_statement(state,
                                           cfg,
                                           statement,
                                           inOutPreviousBlockId,
                                           pendingCause,
                                           pendingCauseNode,
                                           loopTargets)) {
                return ZR_FALSE;
            }
            continue;
        }

        blockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, statement);
        block = cfg_get_block(cfg, blockId);
        if (blockId == ZR_PARSER_CFG_INVALID_BLOCK_ID || block == ZR_NULL) {
            return ZR_FALSE;
        }

        if (pendingCause != ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN) {
            block->unreachableCause = pendingCause;
            block->unreachableCauseNode = pendingCauseNode;
        }

        awaitNode = cfg_statement_direct_await(statement);
        if (awaitNode == ZR_NULL) {
            if (!cfg_connect_fallthrough(cfg, *inOutPreviousBlockId, blockId)) {
                return ZR_FALSE;
            }
        } else {
            suspensionBlockId = cfg_add_block(
                    state, cfg, ZR_PARSER_CFG_BLOCK_SUSPENSION, awaitNode);
            resumeBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, awaitNode);
            if (suspensionBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
                resumeBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
                !cfg_connect_fallthrough(cfg, *inOutPreviousBlockId, suspensionBlockId) ||
                !cfg_add_edge_kind(cfg,
                                   suspensionBlockId,
                                   resumeBlockId,
                                   ZR_PARSER_CFG_EDGE_SUSPEND,
                                   awaitNode) ||
                !cfg_add_edge_kind(cfg,
                                   resumeBlockId,
                                   blockId,
                                   ZR_PARSER_CFG_EDGE_RESUME,
                                   awaitNode)) {
                return ZR_FALSE;
            }
        }

        if (cfg_statement_is_terminator(statement, &terminatorCause)) {
            block->isTerminator = ZR_TRUE;
            if (statement->type == ZR_AST_RETURN_STATEMENT) {
                block->terminatorKind = ZR_PARSER_CFG_TERMINATOR_RETURN;
            } else if (statement->type == ZR_AST_THROW_STATEMENT) {
                block->terminatorKind = ZR_PARSER_CFG_TERMINATOR_THROW;
            } else if (statement->type == ZR_AST_BREAK_CONTINUE_STATEMENT) {
                block->terminatorKind = statement->data.breakContinueStatement.isBreak
                                                ? ZR_PARSER_CFG_TERMINATOR_BREAK
                                                : ZR_PARSER_CFG_TERMINATOR_CONTINUE;
            }
            if (!cfg_connect_loop_control_target(cfg, block, loopTargets)) {
                return ZR_FALSE;
            }
            pendingCause = terminatorCause;
            pendingCauseNode = statement;
        }

        *inOutPreviousBlockId = blockId;
    }

    return ZR_TRUE;
}

static void cfg_mark_reachable(SZrParserCfg *cfg, TZrUInt32 blockId) {
    SZrParserCfgBlock *block = cfg_get_block(cfg, blockId);
    TZrUInt32 index;

    if (block == ZR_NULL || block->visited) {
        return;
    }

    block->visited = ZR_TRUE;
    for (index = 0; index < block->successorCount; index++) {
        cfg_mark_reachable(
                cfg,
                ZrParser_Cfg_BlockSuccessorIdAt(block, index));
    }
}

TZrBool ZrParser_Cfg_Build(SZrState *state, SZrParserCfg *cfg, SZrAstNode *root) {
    TZrUInt32 previousBlockId;
    SZrAstNodeArray *statements;
    SZrParserCfgBlock *previousBlock;

    if (state == ZR_NULL || cfg == ZR_NULL || root == ZR_NULL || !cfg->blocks.isValid) {
        return ZR_FALSE;
    }

    cfg_clear_blocks(state, cfg);
    cfg->entryBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_ENTRY, ZR_NULL);
    if (cfg->entryBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }

    previousBlockId = cfg->entryBlockId;
    statements = cfg_statement_array_from_root(root);
    if (statements != ZR_NULL) {
        if (!cfg_build_statement_list(state,
                                      cfg,
                                      statements,
                                      &previousBlockId,
                                      ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN,
                                      ZR_NULL,
                                      &g_cfg_no_loop_targets)) {
            return ZR_FALSE;
        }
    } else {
        SZrAstNodeArray singleStatementArray;
        SZrAstNode *singleStatement = root;

        singleStatementArray.nodes = &singleStatement;
        singleStatementArray.count = 1;
        singleStatementArray.capacity = 1;
        if (!cfg_build_statement_list(state,
                                      cfg,
                                      &singleStatementArray,
                                      &previousBlockId,
                                      ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN,
                                      ZR_NULL,
                                      &g_cfg_no_loop_targets)) {
            return ZR_FALSE;
        }
    }

    cfg->exitBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_EXIT, ZR_NULL);
    if (cfg->exitBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }
    if (!cfg_connect_function_exits(cfg)) {
        return ZR_FALSE;
    }

    previousBlock = cfg_get_block(cfg, previousBlockId);
    if (previousBlock != ZR_NULL && !previousBlock->isTerminator) {
        return cfg_add_edge(cfg, previousBlockId, cfg->exitBlockId);
    }

    return ZR_TRUE;
}

TZrBool ZrParser_Cfg_EmitReachabilityFacts(SZrSemanticContext *context, SZrParserCfg *cfg) {
    TZrSize index;

    if (context == ZR_NULL || cfg == ZR_NULL || !cfg->blocks.isValid ||
        cfg->entryBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }

    for (index = 0; index < cfg->blocks.length; index++) {
        SZrParserCfgBlock *block = (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg->blocks, index);
        if (block != ZR_NULL) {
            block->visited = ZR_FALSE;
        }
    }
    cfg_mark_reachable(cfg, cfg->entryBlockId);

    for (index = 0; index < cfg->blocks.length; index++) {
        SZrParserCfgBlock *block = (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg->blocks, index);
        SZrSemanticReachabilityFact fact;

        if (block == ZR_NULL ||
            block->kind != ZR_PARSER_CFG_BLOCK_STATEMENT ||
            block->statement == ZR_NULL ||
            block->visited) {
            continue;
        }

        fact.node = block->statement;
        fact.range = block->statement->location;
        fact.state = ZR_SEMANTIC_REACHABILITY_UNREACHABLE;
        fact.cause = block->unreachableCause;
        fact.causeNode = block->unreachableCauseNode;
        if (!ZrParser_SemanticFacts_AppendReachability(context, &fact)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}
