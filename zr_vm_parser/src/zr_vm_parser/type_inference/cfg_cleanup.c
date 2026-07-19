#include "cfg_internal.h"

static TZrBool cfg_statement_exits_function(SZrAstNode *statement) {
    return (TZrBool)(statement != ZR_NULL &&
                     (statement->type == ZR_AST_RETURN_STATEMENT ||
                      statement->type == ZR_AST_THROW_STATEMENT));
}

static TZrUInt32 cfg_cleanup_successor(const SZrParserCfgBlock *block) {
    TZrSize index;

    if (block == ZR_NULL) {
        return ZR_PARSER_CFG_INVALID_BLOCK_ID;
    }
    for (index = 0; index < block->successorCount; index++) {
        const SZrParserCfgEdge *edge = ZrParser_Cfg_BlockEdgeAt(block, index);
        if (edge != ZR_NULL && edge->kind == ZR_PARSER_CFG_EDGE_CLEANUP) {
            return edge->toBlockId;
        }
    }
    return ZR_PARSER_CFG_INVALID_BLOCK_ID;
}

static TZrBool cfg_route_cleanup_completion_to_exit(
        SZrParserCfg *cfg,
        TZrUInt32 blockId,
        EZrParserCfgEdgeKind exitEdgeKind,
        SZrAstNode *sourceNode,
        TZrSize depth) {
    SZrParserCfgBlock *block = cfg_get_block(cfg, blockId);
    TZrBool onlyExitSuccessors = ZR_TRUE;
    TZrBool hasDifferentExitKind = ZR_FALSE;
    TZrSize index;

    if (block == ZR_NULL || block->visited || depth > cfg->blocks.length) {
        return ZR_TRUE;
    }
    block->visited = ZR_TRUE;

    if (block->isTerminator && cfg_statement_exits_function(block->statement)) {
        return ZR_TRUE;
    }
    for (index = 0; index < block->successorCount; index++) {
        const SZrParserCfgEdge *edge = ZrParser_Cfg_BlockEdgeAt(block, index);
        if (edge == ZR_NULL || edge->toBlockId != cfg->exitBlockId) {
            onlyExitSuccessors = ZR_FALSE;
        } else if (edge->kind != exitEdgeKind) {
            hasDifferentExitKind = ZR_TRUE;
        }
    }
    if (onlyExitSuccessors) {
        block->isTerminator = ZR_TRUE;
        block->terminatorKind = hasDifferentExitKind
                                        ? ZR_PARSER_CFG_TERMINATOR_CLEANUP_DISPATCH
                                : exitEdgeKind == ZR_PARSER_CFG_EDGE_RETURN
                                        ? ZR_PARSER_CFG_TERMINATOR_RETURN
                                        : ZR_PARSER_CFG_TERMINATOR_THROW;
        return cfg_add_edge_kind(
                cfg,
                blockId,
                cfg->exitBlockId,
                exitEdgeKind,
                sourceNode);
    }

    for (index = 0; index < block->successorCount; index++) {
        const SZrParserCfgEdge *edge = ZrParser_Cfg_BlockEdgeAt(block, index);
        if (edge == ZR_NULL || edge->toBlockId == cfg->exitBlockId) {
            continue;
        }
        if (!cfg_route_cleanup_completion_to_exit(
                    cfg,
                    edge->toBlockId,
                    exitEdgeKind,
                    sourceNode,
                    depth + 1U)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static void cfg_reset_visit_marks(SZrParserCfg *cfg) {
    TZrSize index;

    if (cfg == ZR_NULL || !cfg->blocks.isValid) {
        return;
    }
    for (index = 0; index < cfg->blocks.length; index++) {
        SZrParserCfgBlock *block =
                (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg->blocks, index);
        if (block != ZR_NULL) {
            block->visited = ZR_FALSE;
        }
    }
}

TZrBool cfg_connect_function_exits(SZrParserCfg *cfg) {
    TZrSize index;

    if (cfg == ZR_NULL || !cfg->blocks.isValid ||
        cfg->exitBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }

    for (index = 0; index < cfg->blocks.length; index++) {
        SZrParserCfgBlock *block =
                (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg->blocks, index);
        if (block != ZR_NULL && block->isTerminator &&
            cfg_statement_exits_function(block->statement)) {
            EZrParserCfgEdgeKind edgeKind =
                    block->statement->type == ZR_AST_RETURN_STATEMENT
                            ? ZR_PARSER_CFG_EDGE_RETURN
                            : ZR_PARSER_CFG_EDGE_EXCEPTION;
            TZrUInt32 cleanupSuccessor = cfg_cleanup_successor(block);

            if (cleanupSuccessor != ZR_PARSER_CFG_INVALID_BLOCK_ID) {
                cfg_reset_visit_marks(cfg);
                if (!cfg_route_cleanup_completion_to_exit(
                            cfg,
                            cleanupSuccessor,
                            edgeKind,
                            block->statement,
                            0U)) {
                    return ZR_FALSE;
                }
                continue;
            }
            if (!cfg_add_edge_kind(
                        cfg,
                        block->id,
                        cfg->exitBlockId,
                        edgeKind,
                        block->statement)) {
                return ZR_FALSE;
            }
        }
    }
    cfg_reset_visit_marks(cfg);
    return ZR_TRUE;
}
