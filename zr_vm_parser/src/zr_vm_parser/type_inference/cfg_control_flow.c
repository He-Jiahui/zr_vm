#include "cfg_internal.h"

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
                                                         TZrUInt32 functionExitFinallyEntryBlockId,
                                                         TZrUInt32 breakFinallyEntryBlockId,
                                                         TZrUInt32 continueFinallyEntryBlockId,
                                                         TZrBool *outHasFunctionExit,
                                                         TZrBool *outHasBreak,
                                                         TZrBool *outHasContinue) {
    TZrSize index;

    if (outHasFunctionExit != ZR_NULL) {
        *outHasFunctionExit = ZR_FALSE;
    }
    if (outHasBreak != ZR_NULL) {
        *outHasBreak = ZR_FALSE;
    }
    if (outHasContinue != ZR_NULL) {
        *outHasContinue = ZR_FALSE;
    }

    if (cfg == ZR_NULL || !cfg->blocks.isValid ||
        functionExitFinallyEntryBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        breakFinallyEntryBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        continueFinallyEntryBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }

    if (endIndex > cfg->blocks.length) {
        endIndex = cfg->blocks.length;
    }

    for (index = startIndex; index < endIndex; index++) {
        SZrParserCfgBlock *block = (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg->blocks, index);

        if (block != ZR_NULL &&
            block->isTerminator &&
            cfg_statement_enters_finally_on_abrupt_completion(block->statement)) {
            TZrUInt32 finallyEntryBlockId = functionExitFinallyEntryBlockId;

            if (block->statement->type == ZR_AST_BREAK_CONTINUE_STATEMENT) {
                if (block->statement->data.breakContinueStatement.isBreak) {
                    finallyEntryBlockId = breakFinallyEntryBlockId;
                    if (outHasBreak != ZR_NULL) {
                        *outHasBreak = ZR_TRUE;
                    }
                } else {
                    finallyEntryBlockId = continueFinallyEntryBlockId;
                    if (outHasContinue != ZR_NULL) {
                        *outHasContinue = ZR_TRUE;
                    }
                }
            } else if (outHasFunctionExit != ZR_NULL) {
                *outHasFunctionExit = ZR_TRUE;
            }
            if (finallyEntryBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
                return ZR_FALSE;
            }
            if (!cfg_add_edge(cfg, block->id, finallyEntryBlockId)) {
                return ZR_FALSE;
            }
        }
    }

    return ZR_TRUE;
}

static TZrBool cfg_build_finally_path(SZrState *state,
                                      SZrParserCfg *cfg,
                                      SZrAstNode *finallyBlock,
                                      TZrUInt32 entryBlockId,
                                      TZrUInt32 targetBlockId,
                                      EZrSemanticReachabilityCause pendingCause,
                                      SZrAstNode *pendingCauseNode,
                                      const SZrParserCfgLoopTargets *loopTargets) {
    TZrUInt32 finallyLastBlockId;

    if (state == ZR_NULL || cfg == ZR_NULL || finallyBlock == ZR_NULL ||
        entryBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }

    if (!cfg_build_statement_body(state,
                                  cfg,
                                  finallyBlock,
                                  entryBlockId,
                                  pendingCause,
                                  pendingCauseNode,
                                  loopTargets,
                                  &finallyLastBlockId)) {
        return ZR_FALSE;
    }

    if (targetBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
        finallyLastBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
        !cfg_connect_fallthrough(cfg, finallyLastBlockId, targetBlockId)) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool cfg_add_join_block(SZrState *state,
                                  SZrParserCfg *cfg,
                                  TZrUInt32 *outBlockId) {
    if (outBlockId == ZR_NULL) {
        return ZR_FALSE;
    }

    *outBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, ZR_NULL);
    return (TZrBool)(*outBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID);
}

static TZrBool cfg_add_finally_abrupt_entries(SZrState *state,
                                              SZrParserCfg *cfg,
                                              TZrUInt32 *outFunctionExitEntryBlockId,
                                              TZrUInt32 *outBreakEntryBlockId,
                                              TZrUInt32 *outContinueEntryBlockId) {
    if (!cfg_add_join_block(state, cfg, outFunctionExitEntryBlockId)) {
        return ZR_FALSE;
    }
    if (!cfg_add_join_block(state, cfg, outBreakEntryBlockId)) {
        return ZR_FALSE;
    }
    if (!cfg_add_join_block(state, cfg, outContinueEntryBlockId)) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool cfg_build_finally_paths(SZrState *state,
                                       SZrParserCfg *cfg,
                                       SZrAstNode *finallyBlock,
                                       TZrUInt32 normalEntryBlockId,
                                       TZrUInt32 functionExitEntryBlockId,
                                       TZrUInt32 breakEntryBlockId,
                                       TZrUInt32 continueEntryBlockId,
                                       TZrUInt32 finalJoinBlockId,
                                       TZrBool hasNormalCompletion,
                                       TZrBool hasFunctionExitCompletion,
                                       TZrBool hasBreakCompletion,
                                       TZrBool hasContinueCompletion,
                                       EZrSemanticReachabilityCause pendingCause,
                                       SZrAstNode *pendingCauseNode,
                                       const SZrParserCfgLoopTargets *loopTargets) {
    if (hasNormalCompletion &&
        !cfg_build_finally_path(state,
                                cfg,
                                finallyBlock,
                                normalEntryBlockId,
                                finalJoinBlockId,
                                pendingCause,
                                pendingCauseNode,
                                loopTargets)) {
        return ZR_FALSE;
    }
    if (hasFunctionExitCompletion &&
        !cfg_build_finally_path(state,
                                cfg,
                                finallyBlock,
                                functionExitEntryBlockId,
                                ZR_PARSER_CFG_INVALID_BLOCK_ID,
                                pendingCause,
                                pendingCauseNode,
                                loopTargets)) {
        return ZR_FALSE;
    }
    if (hasBreakCompletion &&
        loopTargets != ZR_NULL &&
        loopTargets->breakTargetBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
        !cfg_build_finally_path(state,
                                cfg,
                                finallyBlock,
                                breakEntryBlockId,
                                loopTargets->breakTargetBlockId,
                                pendingCause,
                                pendingCauseNode,
                                loopTargets)) {
        return ZR_FALSE;
    }
    if (hasContinueCompletion &&
        loopTargets != ZR_NULL &&
        loopTargets->continueTargetBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
        !cfg_build_finally_path(state,
                                cfg,
                                finallyBlock,
                                continueEntryBlockId,
                                loopTargets->continueTargetBlockId,
                                pendingCause,
                                pendingCauseNode,
                                loopTargets)) {
        return ZR_FALSE;
    }

    if (hasBreakCompletion &&
        (loopTargets == ZR_NULL ||
         loopTargets->breakTargetBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID)) {
        if (!cfg_build_finally_path(state,
                                    cfg,
                                    finallyBlock,
                                    breakEntryBlockId,
                                    ZR_PARSER_CFG_INVALID_BLOCK_ID,
                                    pendingCause,
                                    pendingCauseNode,
                                    loopTargets)) {
            return ZR_FALSE;
        }
    }
    if (hasContinueCompletion &&
        (loopTargets == ZR_NULL ||
         loopTargets->continueTargetBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID)) {
        if (!cfg_build_finally_path(state,
                                    cfg,
                                    finallyBlock,
                                    continueEntryBlockId,
                                    ZR_PARSER_CFG_INVALID_BLOCK_ID,
                                    pendingCause,
                                    pendingCauseNode,
                                    loopTargets)) {
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

    switchBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, statement);
    switchBlock = cfg_get_block(cfg, switchBlockId);
    if (switchBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID || switchBlock == ZR_NULL) {
        return ZR_FALSE;
    }

    if (pendingCause != ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN) {
        switchBlock->unreachableCause = pendingCause;
        switchBlock->unreachableCauseNode = pendingCauseNode;
    }
    if (!cfg_connect_fallthrough(cfg, *inOutPreviousBlockId, switchBlockId)) {
        return ZR_FALSE;
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
    SZrParserCfgBlock *tryBlock;
    SZrAstNodeArray *catchClauses;
    TZrUInt32 finalJoinBlockId;
    TZrUInt32 tryBlockId;
    TZrUInt32 bodyLastBlockId;
    TZrUInt32 joinBlockId;
    TZrUInt32 functionExitFinallyEntryBlockId;
    TZrUInt32 breakFinallyEntryBlockId;
    TZrUInt32 continueFinallyEntryBlockId;
    TZrUInt32 currentCatchDispatchBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    TZrSize abruptExitStartIndex;
    TZrSize abruptExitEndIndex;
    TZrBool hasNormalFinallyCompletion = ZR_FALSE;
    TZrBool hasFunctionExitIntoFinally = ZR_FALSE;
    TZrBool hasBreakIntoFinally = ZR_FALSE;
    TZrBool hasContinueIntoFinally = ZR_FALSE;
    SZrParserCfgLoopTargets protectedLoopTargets;
    const SZrParserCfgLoopTargets *bodyLoopTargets = loopTargets;
    TZrSize catchIndex;
    TZrBool protectedBodyMayEnterCatch;
    TZrBool catchAllAlreadyMatched = ZR_FALSE;
    TZrUInt32 protectedBodyKnownThrowKindMask = 0u;
    TZrUInt32 remainingKnownThrowKindMask = 0u;
    TZrBool protectedBodyHasUnknownThrowSource = ZR_FALSE;
    TZrBool protectedBodyHasOnlyKnownThrowKinds = ZR_FALSE;

    if (state == ZR_NULL || cfg == ZR_NULL || statement == ZR_NULL ||
        inOutPreviousBlockId == ZR_NULL) {
        return ZR_FALSE;
    }

    if (statement->data.tryCatchFinallyStatement.finallyBlock != ZR_NULL &&
        loopTargets != ZR_NULL) {
        protectedLoopTargets = *loopTargets;
        protectedLoopTargets.breakTargetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
        protectedLoopTargets.continueTargetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
        bodyLoopTargets = &protectedLoopTargets;
    }

    tryBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, statement);
    tryBlock = cfg_get_block(cfg, tryBlockId);
    if (tryBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID || tryBlock == ZR_NULL) {
        return ZR_FALSE;
    }

    if (pendingCause != ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN) {
        tryBlock->unreachableCause = pendingCause;
        tryBlock->unreachableCauseNode = pendingCauseNode;
    }
    if (!cfg_connect_fallthrough(cfg, *inOutPreviousBlockId, tryBlockId)) {
        return ZR_FALSE;
    }

    abruptExitStartIndex = cfg->blocks.length;
    if (!cfg_build_statement_body(state,
                                  cfg,
                                  statement->data.tryCatchFinallyStatement.block,
                                  tryBlockId,
                                  pendingCause,
                                  pendingCauseNode,
                                  bodyLoopTargets,
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
    protectedBodyMayEnterCatch =
            cfg_node_may_enter_catch(statement->data.tryCatchFinallyStatement.block);
    if (!cfg_try_body_throw_profile(
            statement->data.tryCatchFinallyStatement.block,
            &protectedBodyKnownThrowKindMask,
            &protectedBodyHasUnknownThrowSource)) {
        protectedBodyKnownThrowKindMask = 0u;
        protectedBodyHasUnknownThrowSource = ZR_TRUE;
    }
    protectedBodyHasOnlyKnownThrowKinds =
            (TZrBool)(protectedBodyKnownThrowKindMask != 0u &&
                      !protectedBodyHasUnknownThrowSource);
    remainingKnownThrowKindMask = protectedBodyKnownThrowKindMask;
    if (protectedBodyMayEnterCatch &&
        catchClauses != ZR_NULL &&
        catchClauses->count > 0) {
        currentCatchDispatchBlockId =
                cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, ZR_NULL);
        if (currentCatchDispatchBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
            !cfg_add_edge(cfg, tryBlockId, currentCatchDispatchBlockId)) {
            return ZR_FALSE;
        }
    }
    if (catchClauses != ZR_NULL) {
        for (catchIndex = 0; catchIndex < catchClauses->count; catchIndex++) {
            SZrAstNode *catchNode = catchClauses->nodes[catchIndex];
            TZrUInt32 catchDispatchBlockId = currentCatchDispatchBlockId;
            TZrBool catchCanEnter;
            TZrBool catchMayContinueToLater = ZR_FALSE;
            EZrParserCfgCatchMatch catchMatch = ZR_PARSER_CFG_CATCH_MATCH_UNKNOWN;
            TZrBool catchMatchIsPrecise = ZR_FALSE;
            TZrUInt32 catchMatchedKnownMask = 0u;
            TZrBool catchRejectedByKnownThrowTypes = ZR_FALSE;
            TZrBool catchSuppressedByEarlierCatch = ZR_FALSE;
            EZrSemanticReachabilityCause catchCause;
            SZrAstNode *catchCauseNode;
            TZrUInt32 catchPredecessorBlockId;
            TZrUInt32 catchLastBlockId;

            if (catchNode == ZR_NULL || catchNode->type != ZR_AST_CATCH_CLAUSE) {
                continue;
            }
            if (protectedBodyHasOnlyKnownThrowKinds) {
                if (!cfg_catch_clause_match_known_throw_kinds(
                        catchNode,
                        remainingKnownThrowKindMask,
                        &catchMatchIsPrecise,
                        &catchMatchedKnownMask)) {
                    catchMatchIsPrecise = ZR_FALSE;
                    catchMatchedKnownMask = 0u;
                }
                if (catchMatchIsPrecise) {
                    catchMatch = catchMatchedKnownMask != 0u
                                         ? ZR_PARSER_CFG_CATCH_MATCH_YES
                                         : ZR_PARSER_CFG_CATCH_MATCH_NO;
                }
            }
            catchCanEnter = (TZrBool)(protectedBodyMayEnterCatch &&
                                      !catchAllAlreadyMatched &&
                                      catchDispatchBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID);
            if (catchCanEnter && protectedBodyHasOnlyKnownThrowKinds) {
                if (remainingKnownThrowKindMask == 0u) {
                    catchSuppressedByEarlierCatch = ZR_TRUE;
                    catchCanEnter = ZR_FALSE;
                } else if (catchMatchIsPrecise &&
                           catchMatch == ZR_PARSER_CFG_CATCH_MATCH_NO) {
                    catchRejectedByKnownThrowTypes = ZR_TRUE;
                    catchCanEnter = ZR_FALSE;
                }
            } else if (catchCanEnter && catchMatch == ZR_PARSER_CFG_CATCH_MATCH_NO) {
                catchRejectedByKnownThrowTypes = ZR_TRUE;
                catchCanEnter = ZR_FALSE;
            }
            catchCause = pendingCause;
            catchCauseNode = pendingCauseNode;
            if (!catchCanEnter && protectedBodyMayEnterCatch &&
                (catchAllAlreadyMatched || catchSuppressedByEarlierCatch ||
                 catchRejectedByKnownThrowTypes) &&
                catchCause == ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN) {
                catchCause = ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH;
                catchCauseNode = catchNode;
            }
            catchPredecessorBlockId = catchCanEnter
                                              ? catchDispatchBlockId
                                              : ZR_PARSER_CFG_INVALID_BLOCK_ID;
            if (!cfg_build_statement_body(state,
                                          cfg,
                                          catchNode->data.catchClause.block,
                                          catchPredecessorBlockId,
                                          catchCause,
                                          catchCauseNode,
                                          bodyLoopTargets,
                                          &catchLastBlockId)) {
                return ZR_FALSE;
            }
            if (catchCanEnter &&
                catchLastBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
                !cfg_connect_fallthrough(cfg, catchLastBlockId, joinBlockId)) {
                return ZR_FALSE;
            }
            if (catchCanEnter &&
                protectedBodyHasOnlyKnownThrowKinds &&
                catchMatchIsPrecise) {
                remainingKnownThrowKindMask &= ~catchMatchedKnownMask;
                if (remainingKnownThrowKindMask == 0u) {
                    catchAllAlreadyMatched = ZR_TRUE;
                }
            } else if (catchCanEnter &&
                       (cfg_catch_clause_is_catch_all(catchNode) ||
                        catchMatch == ZR_PARSER_CFG_CATCH_MATCH_YES)) {
                catchAllAlreadyMatched = ZR_TRUE;
            }

            if (protectedBodyMayEnterCatch &&
                catchDispatchBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID) {
                if (protectedBodyHasOnlyKnownThrowKinds) {
                    catchMayContinueToLater =
                            (TZrBool)(remainingKnownThrowKindMask != 0u);
                } else {
                    catchMayContinueToLater = (TZrBool)!catchAllAlreadyMatched;
                }
            }
            if (catchMayContinueToLater &&
                catchIndex + 1 < catchClauses->count) {
                TZrUInt32 nextCatchDispatchBlockId =
                        cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, ZR_NULL);
                if (nextCatchDispatchBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
                    !cfg_add_edge(cfg, catchDispatchBlockId, nextCatchDispatchBlockId)) {
                    return ZR_FALSE;
                }
                currentCatchDispatchBlockId = nextCatchDispatchBlockId;
            } else {
                currentCatchDispatchBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
            }
        }
    }

    if (statement->data.tryCatchFinallyStatement.finallyBlock != ZR_NULL) {
        SZrParserCfgBlock *joinBlock;

        abruptExitEndIndex = cfg->blocks.length;
        joinBlock = cfg_get_block(cfg, joinBlockId);
        hasNormalFinallyCompletion =
                (TZrBool)(joinBlock != ZR_NULL && joinBlock->predecessorCount > 0);

        if (!cfg_add_finally_abrupt_entries(state,
                                            cfg,
                                            &functionExitFinallyEntryBlockId,
                                            &breakFinallyEntryBlockId,
                                            &continueFinallyEntryBlockId)) {
            return ZR_FALSE;
        }
        finalJoinBlockId = cfg_add_block(state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, ZR_NULL);
        if (finalJoinBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
            return ZR_FALSE;
        }
        if (!cfg_connect_abrupt_completions_to_finally(cfg,
                                                       abruptExitStartIndex,
                                                       abruptExitEndIndex,
                                                       functionExitFinallyEntryBlockId,
                                                       breakFinallyEntryBlockId,
                                                       continueFinallyEntryBlockId,
                                                       &hasFunctionExitIntoFinally,
                                                       &hasBreakIntoFinally,
                                                       &hasContinueIntoFinally)) {
            return ZR_FALSE;
        }
        if (!cfg_build_finally_paths(state,
                                     cfg,
                                     statement->data.tryCatchFinallyStatement.finallyBlock,
                                     joinBlockId,
                                     functionExitFinallyEntryBlockId,
                                     breakFinallyEntryBlockId,
                                     continueFinallyEntryBlockId,
                                     finalJoinBlockId,
                                     hasNormalFinallyCompletion,
                                     hasFunctionExitIntoFinally,
                                     hasBreakIntoFinally,
                                     hasContinueIntoFinally,
                                     pendingCause,
                                     pendingCauseNode,
                                     loopTargets)) {
            return ZR_FALSE;
        }
        *inOutPreviousBlockId = finalJoinBlockId;
        return ZR_TRUE;
    }

    *inOutPreviousBlockId = joinBlockId;
    return ZR_TRUE;
}
