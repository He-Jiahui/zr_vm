#include "cfg_internal.h"

TZrBool cfg_build_while_statement(SZrState *state,
                                  SZrParserCfg *cfg,
                                  SZrAstNode *statement,
                                  TZrUInt32 *inOutPreviousBlockId,
                                  EZrSemanticReachabilityCause pendingCause,
                                  SZrAstNode *pendingCauseNode) {
    SZrParserCfgBlock *whileBlock;
    TZrUInt32 whileBlockId;
    TZrUInt32 bodyPreviousBlockId;
    TZrUInt32 bodyLastBlockId;
    TZrUInt32 joinBlockId;
    TZrSize bodyEdgeIndex;
    TZrBool conditionValue = ZR_FALSE;
    TZrBool hasConstantCondition;
    TZrBool includeBody;
    TZrBool canExitWithoutIteration;
    EZrSemanticReachabilityCause bodyCause;
    SZrAstNode *bodyCauseNode;
    SZrParserCfgLoopTargets whileLoopTargets;

    if (state == ZR_NULL || cfg == ZR_NULL || statement == ZR_NULL ||
        inOutPreviousBlockId == ZR_NULL) {
        return ZR_FALSE;
    }

    whileBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, statement);
    whileBlock = cfg_get_block(cfg, whileBlockId);
    if (whileBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID || whileBlock == ZR_NULL) {
        return ZR_FALSE;
    }

    if (pendingCause != ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN) {
        whileBlock->unreachableCause = pendingCause;
        whileBlock->unreachableCauseNode = pendingCauseNode;
    }
    if (!cfg_connect_fallthrough(cfg, *inOutPreviousBlockId, whileBlockId)) {
        return ZR_FALSE;
    }

    hasConstantCondition = cfg_node_bool_constant(statement->data.whileLoop.cond, &conditionValue);
    includeBody = !hasConstantCondition || conditionValue;
    canExitWithoutIteration = !hasConstantCondition || !conditionValue;
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

    bodyEdgeIndex = cfg_get_block(cfg, whileBlockId)->successorCount;
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
    if (includeBody &&
        cfg_get_block(cfg, whileBlockId)->successorCount > bodyEdgeIndex &&
        !cfg_retag_edge_at(cfg,
                           whileBlockId,
                           bodyEdgeIndex,
                           ZR_PARSER_CFG_EDGE_TRUE_BRANCH,
                           statement->data.whileLoop.cond)) {
        return ZR_FALSE;
    }

    if (canExitWithoutIteration &&
        !cfg_add_edge_kind(cfg,
                           whileBlockId,
                           joinBlockId,
                           ZR_PARSER_CFG_EDGE_FALSE_BRANCH,
                           statement->data.whileLoop.cond)) {
        return ZR_FALSE;
    }

    if (includeBody) {
        if (bodyLastBlockId == whileBlockId) {
            if (!cfg_add_edge_kind(cfg,
                                   whileBlockId,
                                   whileBlockId,
                                   ZR_PARSER_CFG_EDGE_TRUE_BRANCH,
                                   statement->data.whileLoop.cond)) {
                return ZR_FALSE;
            }
        } else if (bodyLastBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
                   !cfg_connect_fallthrough(cfg, bodyLastBlockId, whileBlockId)) {
            return ZR_FALSE;
        }
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
    SZrParserCfgBlock *forBlock;
    TZrUInt32 loopPredecessorBlockId;
    TZrUInt32 forBlockId;
    TZrUInt32 bodyPreviousBlockId;
    TZrUInt32 bodyLastBlockId;
    TZrUInt32 continueTargetBlockId;
    TZrUInt32 stepLastBlockId;
    TZrUInt32 stepEntryBlockId;
    TZrUInt32 joinBlockId;
    TZrSize bodyEdgeIndex;
    TZrBool conditionValue = ZR_FALSE;
    TZrBool hasConstantCondition;
    TZrBool includeBody;
    TZrBool canExitWithoutIteration;
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

    forBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, statement);
    forBlock = cfg_get_block(cfg, forBlockId);
    if (forBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID || forBlock == ZR_NULL) {
        return ZR_FALSE;
    }

    if (pendingCause != ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN) {
        forBlock->unreachableCause = pendingCause;
        forBlock->unreachableCauseNode = pendingCauseNode;
    }
    if (!cfg_connect_fallthrough(cfg, loopPredecessorBlockId, forBlockId)) {
        return ZR_FALSE;
    }

    hasConstantCondition = cfg_node_bool_constant(statement->data.forLoop.cond, &conditionValue);
    includeBody = !hasConstantCondition || conditionValue;
    canExitWithoutIteration = statement->data.forLoop.cond != ZR_NULL &&
                              (!hasConstantCondition || !conditionValue);
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

    bodyEdgeIndex = cfg_get_block(cfg, forBlockId)->successorCount;
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
    if (includeBody &&
        cfg_get_block(cfg, forBlockId)->successorCount > bodyEdgeIndex &&
        !cfg_retag_edge_at(cfg,
                           forBlockId,
                           bodyEdgeIndex,
                           ZR_PARSER_CFG_EDGE_TRUE_BRANCH,
                           statement->data.forLoop.cond)) {
        return ZR_FALSE;
    }

    if (includeBody && statement->data.forLoop.step != ZR_NULL) {
        if (bodyLastBlockId == forBlockId) {
            if (!cfg_add_edge_kind(cfg,
                                   forBlockId,
                                   stepEntryBlockId,
                                   ZR_PARSER_CFG_EDGE_TRUE_BRANCH,
                                   statement->data.forLoop.cond)) {
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
    } else if (includeBody) {
        if (bodyLastBlockId == forBlockId) {
            if (!cfg_add_edge_kind(cfg,
                                   forBlockId,
                                   forBlockId,
                                   ZR_PARSER_CFG_EDGE_TRUE_BRANCH,
                                   statement->data.forLoop.cond)) {
                return ZR_FALSE;
            }
        } else if (bodyLastBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
                   !cfg_connect_fallthrough(cfg, bodyLastBlockId, forBlockId)) {
            return ZR_FALSE;
        }
    }

    if (canExitWithoutIteration &&
        !cfg_add_edge_kind(cfg,
                           forBlockId,
                           joinBlockId,
                           ZR_PARSER_CFG_EDGE_FALSE_BRANCH,
                           statement->data.forLoop.cond)) {
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
    SZrParserCfgBlock *foreachBlock;
    TZrUInt32 foreachBlockId;
    TZrUInt32 bodyLastBlockId;
    TZrUInt32 joinBlockId;
    TZrSize bodyEdgeIndex;
    SZrParserCfgLoopTargets foreachLoopTargets;

    if (state == ZR_NULL || cfg == ZR_NULL || statement == ZR_NULL ||
        inOutPreviousBlockId == ZR_NULL) {
        return ZR_FALSE;
    }

    foreachBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, statement);
    foreachBlock = cfg_get_block(cfg, foreachBlockId);
    if (foreachBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID || foreachBlock == ZR_NULL) {
        return ZR_FALSE;
    }

    if (pendingCause != ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN) {
        foreachBlock->unreachableCause = pendingCause;
        foreachBlock->unreachableCauseNode = pendingCauseNode;
    }
    if (!cfg_connect_fallthrough(cfg, *inOutPreviousBlockId, foreachBlockId)) {
        return ZR_FALSE;
    }

    joinBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, ZR_NULL);
    if (joinBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }
    foreachLoopTargets.breakTargetBlockId = joinBlockId;
    foreachLoopTargets.continueTargetBlockId = foreachBlockId;

    bodyEdgeIndex = cfg_get_block(cfg, foreachBlockId)->successorCount;
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
    if (cfg_get_block(cfg, foreachBlockId)->successorCount > bodyEdgeIndex &&
        !cfg_retag_edge_at(cfg,
                           foreachBlockId,
                           bodyEdgeIndex,
                           ZR_PARSER_CFG_EDGE_TRUE_BRANCH,
                           statement)) {
        return ZR_FALSE;
    }

    if (!cfg_add_edge_kind(cfg,
                           foreachBlockId,
                           joinBlockId,
                           ZR_PARSER_CFG_EDGE_FALSE_BRANCH,
                           statement)) {
        return ZR_FALSE;
    }
    if (bodyLastBlockId == foreachBlockId) {
        if (!cfg_add_edge_kind(cfg,
                               foreachBlockId,
                               foreachBlockId,
                               ZR_PARSER_CFG_EDGE_TRUE_BRANCH,
                               statement)) {
            return ZR_FALSE;
        }
    } else if (bodyLastBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
               !cfg_connect_fallthrough(cfg, bodyLastBlockId, foreachBlockId)) {
        return ZR_FALSE;
    }

    *inOutPreviousBlockId = joinBlockId;
    return ZR_TRUE;
}
