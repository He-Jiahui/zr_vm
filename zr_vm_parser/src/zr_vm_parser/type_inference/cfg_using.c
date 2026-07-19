#include "cfg_internal.h"

TZrBool cfg_build_using_statement(
        SZrState *state,
        SZrParserCfg *cfg,
        SZrAstNode *statement,
        TZrUInt32 *inOutPreviousBlockId,
        EZrSemanticReachabilityCause pendingCause,
        SZrAstNode *pendingCauseNode,
        const SZrParserCfgLoopTargets *loopTargets) {
    SZrParserCfgBlock *exitBlock;
    TZrUInt32 bodyLastBlockId;
    TZrUInt32 exitBlockId;

    if (state == ZR_NULL ||
        cfg == ZR_NULL ||
        statement == ZR_NULL ||
        statement->type != ZR_AST_USING_STATEMENT ||
        !statement->data.usingStatement.isBlockScoped ||
        statement->data.usingStatement.guardKind != ZR_USING_GUARD_DROP ||
        statement->data.usingStatement.body == ZR_NULL ||
        inOutPreviousBlockId == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!cfg_build_statement_body(
                state,
                cfg,
                statement->data.usingStatement.body,
                *inOutPreviousBlockId,
                pendingCause,
                pendingCauseNode,
                loopTargets,
                &bodyLastBlockId)) {
        return ZR_FALSE;
    }

    exitBlockId = cfg_add_block(
            state,
            cfg,
            ZR_PARSER_CFG_BLOCK_CLEANUP,
            statement);
    exitBlock = cfg_get_block(cfg, exitBlockId);
    if (exitBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID || exitBlock == ZR_NULL) {
        return ZR_FALSE;
    }
    if (pendingCause != ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN) {
        exitBlock->unreachableCause = pendingCause;
        exitBlock->unreachableCauseNode = pendingCauseNode;
    }
    if (!cfg_add_edge_kind(cfg,
                           bodyLastBlockId,
                           exitBlockId,
                           ZR_PARSER_CFG_EDGE_CLEANUP,
                           statement)) {
        return ZR_FALSE;
    }

    *inOutPreviousBlockId = exitBlockId;
    return ZR_TRUE;
}
