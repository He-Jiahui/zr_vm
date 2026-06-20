#include "cfg_internal.h"

typedef enum EZrParserCfgConstantKind {
    ZR_PARSER_CFG_CONSTANT_UNKNOWN = 0,
    ZR_PARSER_CFG_CONSTANT_BOOL,
    ZR_PARSER_CFG_CONSTANT_INTEGER,
    ZR_PARSER_CFG_CONSTANT_STRING,
    ZR_PARSER_CFG_CONSTANT_CHAR,
    ZR_PARSER_CFG_CONSTANT_FLOAT,
} EZrParserCfgConstantKind;

typedef struct SZrParserCfgConstant {
    EZrParserCfgConstantKind kind;
    TZrBool boolValue;
    TZrInt64 integerValue;
    SZrString *stringValue;
    TZrChar charValue;
    TZrDouble floatValue;
} SZrParserCfgConstant;

static TZrBool cfg_node_constant(SZrAstNode *node, SZrParserCfgConstant *outValue) {
    TZrBool boolValue = ZR_FALSE;

    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    outValue->kind = ZR_PARSER_CFG_CONSTANT_UNKNOWN;
    outValue->boolValue = ZR_FALSE;
    outValue->integerValue = 0;
    outValue->stringValue = ZR_NULL;
    outValue->charValue = '\0';
    outValue->floatValue = 0.0;
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    if (cfg_node_bool_constant(node, &boolValue)) {
        outValue->kind = ZR_PARSER_CFG_CONSTANT_BOOL;
        outValue->boolValue = boolValue;
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_INTEGER_LITERAL:
            outValue->kind = ZR_PARSER_CFG_CONSTANT_INTEGER;
            outValue->integerValue = node->data.integerLiteral.value;
            return ZR_TRUE;
        case ZR_AST_STRING_LITERAL:
            if (node->data.stringLiteral.hasError ||
                node->data.stringLiteral.value == ZR_NULL) {
                return ZR_FALSE;
            }
            outValue->kind = ZR_PARSER_CFG_CONSTANT_STRING;
            outValue->stringValue = node->data.stringLiteral.value;
            return ZR_TRUE;
        case ZR_AST_CHAR_LITERAL:
            if (node->data.charLiteral.hasError) {
                return ZR_FALSE;
            }
            outValue->kind = ZR_PARSER_CFG_CONSTANT_CHAR;
            outValue->charValue = node->data.charLiteral.value;
            return ZR_TRUE;
        case ZR_AST_FLOAT_LITERAL:
            outValue->kind = ZR_PARSER_CFG_CONSTANT_FLOAT;
            outValue->floatValue = node->data.floatLiteral.value;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool cfg_constants_can_compare(const SZrParserCfgConstant *left,
                                         const SZrParserCfgConstant *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        left->kind == ZR_PARSER_CFG_CONSTANT_UNKNOWN ||
        right->kind == ZR_PARSER_CFG_CONSTANT_UNKNOWN) {
        return ZR_FALSE;
    }

    return (TZrBool)(left->kind == right->kind);
}

static TZrBool cfg_constants_equal(const SZrParserCfgConstant *left,
                                   const SZrParserCfgConstant *right) {
    if (!cfg_constants_can_compare(left, right)) {
        return ZR_FALSE;
    }

    switch (left->kind) {
        case ZR_PARSER_CFG_CONSTANT_BOOL:
            return (TZrBool)(left->boolValue == right->boolValue);
        case ZR_PARSER_CFG_CONSTANT_INTEGER:
            return (TZrBool)(left->integerValue == right->integerValue);
        case ZR_PARSER_CFG_CONSTANT_STRING:
            return ZrCore_String_Equal(left->stringValue, right->stringValue);
        case ZR_PARSER_CFG_CONSTANT_CHAR:
            return (TZrBool)(left->charValue == right->charValue);
        case ZR_PARSER_CFG_CONSTANT_FLOAT:
            return (TZrBool)(left->floatValue == right->floatValue);
        default:
            return ZR_FALSE;
    }
}

static TZrBool cfg_statement_enters_finally_on_abrupt_completion(SZrAstNode *statement) {
    if (statement == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(statement->type == ZR_AST_RETURN_STATEMENT ||
                     statement->type == ZR_AST_THROW_STATEMENT ||
                     statement->type == ZR_AST_BREAK_CONTINUE_STATEMENT);
}

static TZrBool cfg_connect_abrupt_completions_to_finally(SZrParserCfg *cfg,
                                                         TZrSize startIndex,
                                                         TZrSize endIndex,
                                                         TZrUInt32 finallyEntryBlockId) {
    TZrSize index;

    if (cfg == ZR_NULL || !cfg->blocks.isValid ||
        finallyEntryBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }

    if (endIndex > cfg->blocks.length) {
        endIndex = cfg->blocks.length;
    }

    for (index = startIndex; index < endIndex; index++) {
        SZrParserCfgBlock *block = (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg->blocks, index);

        if (block != ZR_NULL &&
            block->isTerminator &&
            cfg_statement_enters_finally_on_abrupt_completion(block->statement) &&
            !cfg_add_edge(cfg, block->id, finallyEntryBlockId)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool cfg_build_switch_statement(SZrState *state,
                                   SZrParserCfg *cfg,
                                   SZrAstNode *statement,
                                   TZrUInt32 *inOutPreviousBlockId,
                                   EZrSemanticReachabilityCause pendingCause,
                                   SZrAstNode *pendingCauseNode,
                                   const SZrParserCfgLoopTargets *loopTargets) {
    SZrParserCfgBlock *previousBlock;
    SZrParserCfgBlock *switchBlock;
    SZrAstNodeArray *cases;
    TZrUInt32 switchBlockId;
    TZrUInt32 selectorBlockId;
    TZrUInt32 joinBlockId;
    TZrBool selectorHasConstant;
    TZrBool selectorResolvedByPreviousCase = ZR_FALSE;
    SZrParserCfgConstant selectorValue;
    TZrSize caseIndex;

    if (state == ZR_NULL || cfg == ZR_NULL || statement == ZR_NULL ||
        inOutPreviousBlockId == ZR_NULL) {
        return ZR_FALSE;
    }

    previousBlock = cfg_get_block(cfg, *inOutPreviousBlockId);
    switchBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, statement);
    switchBlock = cfg_get_block(cfg, switchBlockId);
    if (switchBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID || switchBlock == ZR_NULL) {
        return ZR_FALSE;
    }

    if (pendingCause != ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN) {
        switchBlock->unreachableCause = pendingCause;
        switchBlock->unreachableCauseNode = pendingCauseNode;
    }
    if (previousBlock != ZR_NULL && !previousBlock->isTerminator) {
        if (!cfg_add_edge(cfg, previousBlock->id, switchBlockId)) {
            return ZR_FALSE;
        }
    }

    joinBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, ZR_NULL);
    if (joinBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }

    selectorBlockId = switchBlockId;
    cases = statement->data.switchExpression.cases;
    selectorHasConstant = cfg_node_constant(statement->data.switchExpression.expr, &selectorValue);
    if (cases != ZR_NULL) {
        for (caseIndex = 0; caseIndex < cases->count; caseIndex++) {
            SZrAstNode *caseNode = cases->nodes[caseIndex];
            SZrParserCfgBlock *caseBlock;
            TZrBool caseIsUnreachable = ZR_FALSE;
            TZrBool caseHasConstant = ZR_FALSE;
            TZrBool caseCanCompareSelector = ZR_FALSE;
            TZrBool caseMatchesSelector = ZR_FALSE;
            SZrParserCfgConstant caseValue;
            EZrSemanticReachabilityCause caseCause;
            SZrAstNode *caseCauseNode;
            TZrUInt32 caseBlockId;
            TZrUInt32 casePredecessorBlockId;
            TZrUInt32 caseLastBlockId;

            if (caseNode == ZR_NULL || caseNode->type != ZR_AST_SWITCH_CASE) {
                continue;
            }

            caseHasConstant = cfg_node_constant(caseNode->data.switchCase.value, &caseValue);
            caseCanCompareSelector = (TZrBool)(selectorHasConstant && caseHasConstant &&
                                               cfg_constants_can_compare(&selectorValue, &caseValue));
            if (caseCanCompareSelector) {
                caseMatchesSelector = cfg_constants_equal(&selectorValue, &caseValue);
                caseIsUnreachable = (TZrBool)(selectorResolvedByPreviousCase || !caseMatchesSelector);
            } else if (selectorResolvedByPreviousCase) {
                caseIsUnreachable = ZR_TRUE;
            }
            caseCause = pendingCause;
            caseCauseNode = pendingCauseNode;
            if (caseCause == ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN && caseIsUnreachable) {
                caseCause = ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH;
                caseCauseNode = statement->data.switchExpression.expr;
            }

            caseBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, caseNode);
            caseBlock = cfg_get_block(cfg, caseBlockId);
            if (caseBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID || caseBlock == ZR_NULL) {
                return ZR_FALSE;
            }
            if (caseCause != ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN) {
                caseBlock->unreachableCause = caseCause;
                caseBlock->unreachableCauseNode = caseCauseNode;
            }
            if (!caseIsUnreachable && !cfg_connect_fallthrough(cfg, selectorBlockId, caseBlockId)) {
                return ZR_FALSE;
            }

            casePredecessorBlockId =
                    caseIsUnreachable ? ZR_PARSER_CFG_INVALID_BLOCK_ID : caseBlockId;
            if (!cfg_build_statement_body(state,
                                          cfg,
                                          caseNode->data.switchCase.block,
                                          casePredecessorBlockId,
                                          caseCause,
                                          caseCauseNode,
                                          loopTargets,
                                          &caseLastBlockId)) {
                return ZR_FALSE;
            }
            if (caseLastBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
                !cfg_connect_fallthrough(cfg, caseLastBlockId, joinBlockId)) {
                return ZR_FALSE;
            }

            if (caseMatchesSelector) {
                selectorResolvedByPreviousCase = ZR_TRUE;
            }
            if (!caseIsUnreachable && !caseMatchesSelector) {
                selectorBlockId = caseBlockId;
            }
        }
    }

    if (statement->data.switchExpression.defaultCase != ZR_NULL &&
        statement->data.switchExpression.defaultCase->type == ZR_AST_SWITCH_DEFAULT) {
        SZrAstNode *defaultNode = statement->data.switchExpression.defaultCase;
        SZrParserCfgBlock *defaultBlock;
        TZrUInt32 defaultBlockId;
        TZrUInt32 defaultLastBlockId;

        defaultBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, defaultNode);
        defaultBlock = cfg_get_block(cfg, defaultBlockId);
        if (defaultBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID || defaultBlock == ZR_NULL) {
            return ZR_FALSE;
        }
        if (pendingCause != ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN) {
            defaultBlock->unreachableCause = pendingCause;
            defaultBlock->unreachableCauseNode = pendingCauseNode;
        } else if (selectorResolvedByPreviousCase) {
            defaultBlock->unreachableCause = ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH;
            defaultBlock->unreachableCauseNode = statement->data.switchExpression.expr;
        }
        if (!selectorResolvedByPreviousCase &&
            !cfg_connect_fallthrough(cfg, selectorBlockId, defaultBlockId)) {
            return ZR_FALSE;
        }
        if (!cfg_build_statement_body(state,
                                      cfg,
                                      defaultNode->data.switchDefault.block,
                                      defaultBlockId,
                                      selectorResolvedByPreviousCase
                                          ? ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH
                                          : pendingCause,
                                      selectorResolvedByPreviousCase
                                          ? statement->data.switchExpression.expr
                                          : pendingCauseNode,
                                      loopTargets,
                                      &defaultLastBlockId)) {
            return ZR_FALSE;
        }
        if (defaultLastBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
            !cfg_connect_fallthrough(cfg, defaultLastBlockId, joinBlockId)) {
            return ZR_FALSE;
        }
    } else if (!selectorResolvedByPreviousCase &&
               !cfg_connect_fallthrough(cfg, selectorBlockId, joinBlockId)) {
        return ZR_FALSE;
    }

    *inOutPreviousBlockId = joinBlockId;
    return ZR_TRUE;
}

TZrBool cfg_build_try_statement(SZrState *state,
                                SZrParserCfg *cfg,
                                SZrAstNode *statement,
                                TZrUInt32 *inOutPreviousBlockId,
                                EZrSemanticReachabilityCause pendingCause,
                                SZrAstNode *pendingCauseNode,
                                const SZrParserCfgLoopTargets *loopTargets) {
    SZrParserCfgBlock *previousBlock;
    SZrParserCfgBlock *tryBlock;
    SZrAstNodeArray *catchClauses;
    TZrUInt32 finalJoinBlockId;
    TZrUInt32 tryBlockId;
    TZrUInt32 bodyLastBlockId;
    TZrUInt32 joinBlockId;
    TZrUInt32 finallyLastBlockId;
    TZrSize abruptExitStartIndex;
    TZrSize abruptExitEndIndex;
    TZrBool hasNormalFinallyCompletion = ZR_FALSE;
    TZrSize catchIndex;

    if (state == ZR_NULL || cfg == ZR_NULL || statement == ZR_NULL ||
        inOutPreviousBlockId == ZR_NULL) {
        return ZR_FALSE;
    }

    previousBlock = cfg_get_block(cfg, *inOutPreviousBlockId);
    tryBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, statement);
    tryBlock = cfg_get_block(cfg, tryBlockId);
    if (tryBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID || tryBlock == ZR_NULL) {
        return ZR_FALSE;
    }

    if (pendingCause != ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN) {
        tryBlock->unreachableCause = pendingCause;
        tryBlock->unreachableCauseNode = pendingCauseNode;
    }
    if (previousBlock != ZR_NULL && !previousBlock->isTerminator) {
        if (!cfg_add_edge(cfg, previousBlock->id, tryBlockId)) {
            return ZR_FALSE;
        }
    }

    abruptExitStartIndex = cfg->blocks.length;
    if (!cfg_build_statement_body(state,
                                  cfg,
                                  statement->data.tryCatchFinallyStatement.block,
                                  tryBlockId,
                                  pendingCause,
                                  pendingCauseNode,
                                  loopTargets,
                                  &bodyLastBlockId)) {
        return ZR_FALSE;
    }

    joinBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, ZR_NULL);
    if (joinBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }
    if (bodyLastBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
        !cfg_connect_fallthrough(cfg, bodyLastBlockId, joinBlockId)) {
        return ZR_FALSE;
    }

    catchClauses = statement->data.tryCatchFinallyStatement.catchClauses;
    if (catchClauses != ZR_NULL) {
        for (catchIndex = 0; catchIndex < catchClauses->count; catchIndex++) {
            SZrAstNode *catchNode = catchClauses->nodes[catchIndex];
            TZrUInt32 catchLastBlockId;

            if (catchNode == ZR_NULL || catchNode->type != ZR_AST_CATCH_CLAUSE) {
                continue;
            }
            if (!cfg_build_statement_body(state,
                                          cfg,
                                          catchNode->data.catchClause.block,
                                          tryBlockId,
                                          pendingCause,
                                          pendingCauseNode,
                                          loopTargets,
                                          &catchLastBlockId)) {
                return ZR_FALSE;
            }
            if (catchLastBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
                !cfg_connect_fallthrough(cfg, catchLastBlockId, joinBlockId)) {
                return ZR_FALSE;
            }
        }
    }

    if (statement->data.tryCatchFinallyStatement.finallyBlock != ZR_NULL) {
        SZrParserCfgBlock *joinBlock;

        abruptExitEndIndex = cfg->blocks.length;
        joinBlock = cfg_get_block(cfg, joinBlockId);
        hasNormalFinallyCompletion =
                (TZrBool)(joinBlock != ZR_NULL && joinBlock->predecessorCount > 0);

        if (!cfg_connect_abrupt_completions_to_finally(cfg,
                                                       abruptExitStartIndex,
                                                       abruptExitEndIndex,
                                                       joinBlockId)) {
            return ZR_FALSE;
        }
        if (!cfg_build_statement_body(state,
                                      cfg,
                                      statement->data.tryCatchFinallyStatement.finallyBlock,
                                      joinBlockId,
                                      pendingCause,
                                      pendingCauseNode,
                                      loopTargets,
                                      &finallyLastBlockId)) {
            return ZR_FALSE;
        }
        finalJoinBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, ZR_NULL);
        if (finalJoinBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
            return ZR_FALSE;
        }
        if (hasNormalFinallyCompletion &&
            finallyLastBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
            !cfg_connect_fallthrough(cfg, finallyLastBlockId, finalJoinBlockId)) {
            return ZR_FALSE;
        }
        *inOutPreviousBlockId = finalJoinBlockId;
        return ZR_TRUE;
    }

    *inOutPreviousBlockId = joinBlockId;
    return ZR_TRUE;
}
