#include "cfg_internal.h"

TZrBool cfg_build_while_statement(SZrState *state,
                                  SZrParserCfg *cfg,
                                  SZrAstNode *statement,
                                  TZrUInt32 *inOutPreviousBlockId,
                                  EZrSemanticReachabilityCause pendingCause,
                                  SZrAstNode *pendingCauseNode) {
    SZrParserCfgBlock *previousBlock;
    SZrParserCfgBlock *whileBlock;
    TZrUInt32 whileBlockId;
    TZrUInt32 bodyPreviousBlockId;
    TZrUInt32 bodyLastBlockId;
    TZrUInt32 joinBlockId;
    TZrBool conditionValue = ZR_FALSE;
    TZrBool hasConstantCondition;
    TZrBool includeBody;
    EZrSemanticReachabilityCause bodyCause;
    SZrAstNode *bodyCauseNode;
    SZrParserCfgLoopTargets whileLoopTargets;

    if (state == ZR_NULL || cfg == ZR_NULL || statement == ZR_NULL ||
        inOutPreviousBlockId == ZR_NULL) {
        return ZR_FALSE;
    }

    previousBlock = cfg_get_block(cfg, *inOutPreviousBlockId);
    whileBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, statement);
    whileBlock = cfg_get_block(cfg, whileBlockId);
    if (whileBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID || whileBlock == ZR_NULL) {
        return ZR_FALSE;
    }

    if (pendingCause != ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN) {
        whileBlock->unreachableCause = pendingCause;
        whileBlock->unreachableCauseNode = pendingCauseNode;
    }
    if (previousBlock != ZR_NULL && !previousBlock->isTerminator) {
        if (!cfg_add_edge(cfg, previousBlock->id, whileBlockId)) {
            return ZR_FALSE;
        }
    }

    hasConstantCondition = cfg_node_bool_constant(statement->data.whileLoop.cond, &conditionValue);
    includeBody = !hasConstantCondition || conditionValue;
    bodyPreviousBlockId = includeBody ? whileBlockId : ZR_PARSER_CFG_INVALID_BLOCK_ID;
    bodyCause = pendingCause != ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN
                    ? pendingCause
                    : ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE;
    bodyCauseNode = pendingCause != ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN
                        ? pendingCauseNode
                        : statement->data.whileLoop.cond;

    joinBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, ZR_NULL);
    if (joinBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }
    whileLoopTargets.breakTargetBlockId = joinBlockId;
    whileLoopTargets.continueTargetBlockId = whileBlockId;

    if (!cfg_build_statement_body(state,
                                  cfg,
                                  statement->data.whileLoop.block,
                                  bodyPreviousBlockId,
                                  includeBody ? pendingCause : bodyCause,
                                  includeBody ? pendingCauseNode : bodyCauseNode,
                                  &whileLoopTargets,
                                  &bodyLastBlockId)) {
        return ZR_FALSE;
    }

    if (!cfg_add_edge(cfg, whileBlockId, joinBlockId)) {
        return ZR_FALSE;
    }

    if (includeBody && bodyLastBlockId != whileBlockId &&
        bodyLastBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
        !cfg_connect_fallthrough(cfg, bodyLastBlockId, whileBlockId)) {
        return ZR_FALSE;
    }

    *inOutPreviousBlockId = joinBlockId;
    return ZR_TRUE;
}

TZrBool cfg_build_for_statement(SZrState *state,
                                SZrParserCfg *cfg,
                                SZrAstNode *statement,
                                TZrUInt32 *inOutPreviousBlockId,
                                EZrSemanticReachabilityCause pendingCause,
                                SZrAstNode *pendingCauseNode) {
    SZrParserCfgBlock *previousBlock;
    SZrParserCfgBlock *forBlock;
    TZrUInt32 loopPredecessorBlockId;
    TZrUInt32 forBlockId;
    TZrUInt32 bodyPreviousBlockId;
    TZrUInt32 bodyLastBlockId;
    TZrUInt32 continueTargetBlockId;
    TZrUInt32 stepLastBlockId;
    TZrUInt32 stepEntryBlockId;
    TZrUInt32 joinBlockId;
    TZrBool conditionValue = ZR_FALSE;
    TZrBool hasConstantCondition;
    TZrBool includeBody;
    EZrSemanticReachabilityCause bodyCause;
    SZrAstNode *bodyCauseNode;
    SZrParserCfgLoopTargets forLoopTargets;

    if (state == ZR_NULL || cfg == ZR_NULL || statement == ZR_NULL ||
        inOutPreviousBlockId == ZR_NULL) {
        return ZR_FALSE;
    }

    loopPredecessorBlockId = *inOutPreviousBlockId;
    if (statement->data.forLoop.init != ZR_NULL &&
        !cfg_build_statement_body(state,
                                  cfg,
                                  statement->data.forLoop.init,
                                  loopPredecessorBlockId,
                                  pendingCause,
                                  pendingCauseNode,
                                  ZR_NULL,
                                  &loopPredecessorBlockId)) {
        return ZR_FALSE;
    }

    previousBlock = cfg_get_block(cfg, loopPredecessorBlockId);
    forBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, statement);
    forBlock = cfg_get_block(cfg, forBlockId);
    if (forBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID || forBlock == ZR_NULL) {
        return ZR_FALSE;
    }

    if (pendingCause != ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN) {
        forBlock->unreachableCause = pendingCause;
        forBlock->unreachableCauseNode = pendingCauseNode;
    }
    if (previousBlock != ZR_NULL && !previousBlock->isTerminator) {
        if (!cfg_add_edge(cfg, previousBlock->id, forBlockId)) {
            return ZR_FALSE;
        }
    }

    hasConstantCondition = cfg_node_bool_constant(statement->data.forLoop.cond, &conditionValue);
    includeBody = !hasConstantCondition || conditionValue;
    bodyPreviousBlockId = includeBody ? forBlockId : ZR_PARSER_CFG_INVALID_BLOCK_ID;
    bodyCause = pendingCause != ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN
                    ? pendingCause
                    : ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE;
    bodyCauseNode = pendingCause != ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN
                        ? pendingCauseNode
                        : statement->data.forLoop.cond;

    joinBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, ZR_NULL);
    if (joinBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }
    stepEntryBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    if (statement->data.forLoop.step != ZR_NULL) {
        stepEntryBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, ZR_NULL);
        if (stepEntryBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
            return ZR_FALSE;
        }
    }
    continueTargetBlockId = stepEntryBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID
                                    ? stepEntryBlockId
                                    : forBlockId;
    forLoopTargets.breakTargetBlockId = joinBlockId;
    forLoopTargets.continueTargetBlockId = continueTargetBlockId;

    if (!cfg_build_statement_body(state,
                                  cfg,
                                  statement->data.forLoop.block,
                                  bodyPreviousBlockId,
                                  includeBody ? pendingCause : bodyCause,
                                  includeBody ? pendingCauseNode : bodyCauseNode,
                                  &forLoopTargets,
                                  &bodyLastBlockId)) {
        return ZR_FALSE;
    }

    if (includeBody && statement->data.forLoop.step != ZR_NULL) {
        if (bodyLastBlockId == forBlockId) {
            if (!cfg_add_edge(cfg, forBlockId, stepEntryBlockId)) {
                return ZR_FALSE;
            }
        } else if (bodyLastBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
                   !cfg_connect_fallthrough(cfg, bodyLastBlockId, stepEntryBlockId)) {
            return ZR_FALSE;
        }
        stepLastBlockId = stepEntryBlockId;
        if (!cfg_build_statement_body(state,
                                      cfg,
                                      statement->data.forLoop.step,
                                      stepEntryBlockId,
                                      pendingCause,
                                      pendingCauseNode,
                                      ZR_NULL,
                                      &stepLastBlockId)) {
            return ZR_FALSE;
        }
        if (stepLastBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
            !cfg_connect_fallthrough(cfg, stepLastBlockId, forBlockId)) {
            return ZR_FALSE;
        }
    } else if (includeBody && bodyLastBlockId != forBlockId &&
               bodyLastBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
               !cfg_connect_fallthrough(cfg, bodyLastBlockId, forBlockId)) {
        return ZR_FALSE;
    }

    if (!cfg_add_edge(cfg, forBlockId, joinBlockId)) {
        return ZR_FALSE;
    }

    *inOutPreviousBlockId = joinBlockId;
    return ZR_TRUE;
}

TZrBool cfg_build_foreach_statement(SZrState *state,
                                    SZrParserCfg *cfg,
                                    SZrAstNode *statement,
                                    TZrUInt32 *inOutPreviousBlockId,
                                    EZrSemanticReachabilityCause pendingCause,
                                    SZrAstNode *pendingCauseNode) {
    SZrParserCfgBlock *previousBlock;
    SZrParserCfgBlock *foreachBlock;
    TZrUInt32 foreachBlockId;
    TZrUInt32 bodyLastBlockId;
    TZrUInt32 joinBlockId;
    SZrParserCfgLoopTargets foreachLoopTargets;

    if (state == ZR_NULL || cfg == ZR_NULL || statement == ZR_NULL ||
        inOutPreviousBlockId == ZR_NULL) {
        return ZR_FALSE;
    }

    previousBlock = cfg_get_block(cfg, *inOutPreviousBlockId);
    foreachBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, statement);
    foreachBlock = cfg_get_block(cfg, foreachBlockId);
    if (foreachBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID || foreachBlock == ZR_NULL) {
        return ZR_FALSE;
    }

    if (pendingCause != ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN) {
        foreachBlock->unreachableCause = pendingCause;
        foreachBlock->unreachableCauseNode = pendingCauseNode;
    }
    if (previousBlock != ZR_NULL && !previousBlock->isTerminator) {
        if (!cfg_add_edge(cfg, previousBlock->id, foreachBlockId)) {
            return ZR_FALSE;
        }
    }

    joinBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, ZR_NULL);
    if (joinBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }
    foreachLoopTargets.breakTargetBlockId = joinBlockId;
    foreachLoopTargets.continueTargetBlockId = foreachBlockId;

    if (!cfg_build_statement_body(state,
                                  cfg,
                                  statement->data.foreachLoop.block,
                                  foreachBlockId,
                                  pendingCause,
                                  pendingCauseNode,
                                  &foreachLoopTargets,
                                  &bodyLastBlockId)) {
        return ZR_FALSE;
    }

    if (!cfg_add_edge(cfg, foreachBlockId, joinBlockId)) {
        return ZR_FALSE;
    }
    if (bodyLastBlockId != foreachBlockId &&
        bodyLastBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
        !cfg_connect_fallthrough(cfg, bodyLastBlockId, foreachBlockId)) {
        return ZR_FALSE;
    }

    *inOutPreviousBlockId = joinBlockId;
    return ZR_TRUE;
}
