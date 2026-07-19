#include "cfg_internal.h"

#include <string.h>

SZrParserCfgBlock *cfg_get_block(SZrParserCfg *cfg, TZrUInt32 id) {
    if (cfg == ZR_NULL || !cfg->blocks.isValid || id >= cfg->blocks.length) {
        return ZR_NULL;
    }
    return (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg->blocks, id);
}

const SZrParserCfgEdge *ZrParser_Cfg_BlockEdgeAt(
        const SZrParserCfgBlock *block,
        TZrSize index) {
    if (block == ZR_NULL || !block->outgoingEdges.isValid ||
        index >= block->outgoingEdges.length) {
        return ZR_NULL;
    }
    return (const SZrParserCfgEdge *)ZrCore_Array_Get(
            (SZrArray *)&block->outgoingEdges,
            index);
}

TZrUInt32 ZrParser_Cfg_BlockSuccessorIdAt(
        const SZrParserCfgBlock *block,
        TZrSize index) {
    const SZrParserCfgEdge *edge = ZrParser_Cfg_BlockEdgeAt(block, index);

    if (edge != ZR_NULL) {
        return edge->toBlockId;
    }
    if (block != ZR_NULL && index < block->successorCount &&
        index < ZR_PARSER_CFG_INLINE_SUCCESSOR_CAPACITY) {
        return block->successors[index];
    }
    return ZR_PARSER_CFG_INVALID_BLOCK_ID;
}

TZrUInt32 ZrParser_Cfg_AppendBlock(SZrState *state,
                                   SZrParserCfg *cfg,
                                   EZrParserCfgBlockKind kind,
                                   SZrAstNode *statement) {
    SZrParserCfgBlock block;

    if (state == ZR_NULL || cfg == ZR_NULL || !cfg->blocks.isValid ||
        kind < ZR_PARSER_CFG_BLOCK_ENTRY || kind > ZR_PARSER_CFG_BLOCK_EXIT) {
        return ZR_PARSER_CFG_INVALID_BLOCK_ID;
    }

    memset(&block, 0, sizeof(block));
    block.id = (TZrUInt32)cfg->blocks.length;
    block.kind = kind;
    block.statement = statement;
    block.successors[0] = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    block.successors[1] = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    block.terminatorKind = kind == ZR_PARSER_CFG_BLOCK_EXIT
                                   ? ZR_PARSER_CFG_TERMINATOR_EXIT
                                   : ZR_PARSER_CFG_TERMINATOR_NONE;
    block.unreachableCause = ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN;
    ZrCore_Array_Init(
            state,
            &block.outgoingEdges,
            sizeof(SZrParserCfgEdge),
            ZR_PARSER_CFG_INLINE_SUCCESSOR_CAPACITY);
    ZrCore_Array_Push(state, &cfg->blocks, &block);
    return block.id;
}

TZrUInt32 cfg_add_block(SZrState *state,
                        SZrParserCfg *cfg,
                        EZrParserCfgBlockKind kind,
                        SZrAstNode *statement) {
    return ZrParser_Cfg_AppendBlock(state, cfg, kind, statement);
}

static void cfg_update_terminator_for_edge(SZrParserCfgBlock *block,
                                           EZrParserCfgEdgeKind kind) {
    if (block == ZR_NULL) {
        return;
    }

    switch (kind) {
        case ZR_PARSER_CFG_EDGE_TRUE_BRANCH:
        case ZR_PARSER_CFG_EDGE_FALSE_BRANCH:
            block->terminatorKind = ZR_PARSER_CFG_TERMINATOR_BRANCH;
            break;
        case ZR_PARSER_CFG_EDGE_SWITCH_CASE:
        case ZR_PARSER_CFG_EDGE_SWITCH_DEFAULT:
            block->terminatorKind = ZR_PARSER_CFG_TERMINATOR_SWITCH;
            break;
        case ZR_PARSER_CFG_EDGE_RETURN:
            block->terminatorKind = ZR_PARSER_CFG_TERMINATOR_RETURN;
            break;
        case ZR_PARSER_CFG_EDGE_SUSPEND:
            block->terminatorKind = ZR_PARSER_CFG_TERMINATOR_SUSPEND;
            break;
        default:
            break;
    }
}

TZrBool ZrParser_Cfg_Connect(SZrParserCfg *cfg,
                             TZrUInt32 fromBlockId,
                             TZrUInt32 toBlockId,
                             EZrParserCfgEdgeKind kind,
                             SZrAstNode *sourceNode) {
    SZrParserCfgBlock *from;
    SZrParserCfgBlock *to;
    SZrParserCfgEdge edge;
    TZrSize index;

    if (cfg == ZR_NULL || cfg->state == ZR_NULL ||
        kind < ZR_PARSER_CFG_EDGE_NORMAL || kind >= ZR_PARSER_CFG_EDGE_ENUM_MAX) {
        return ZR_FALSE;
    }

    from = cfg_get_block(cfg, fromBlockId);
    to = cfg_get_block(cfg, toBlockId);
    if (from == ZR_NULL || to == ZR_NULL || !from->outgoingEdges.isValid) {
        return ZR_FALSE;
    }

    for (index = 0; index < from->outgoingEdges.length; index++) {
        const SZrParserCfgEdge *existing = ZrParser_Cfg_BlockEdgeAt(from, index);
        if (existing != ZR_NULL && existing->toBlockId == toBlockId &&
            existing->kind == kind && existing->sourceNode == sourceNode) {
            return ZR_TRUE;
        }
    }

    edge.fromBlockId = fromBlockId;
    edge.toBlockId = toBlockId;
    edge.kind = kind;
    edge.sourceNode = sourceNode;
    ZrCore_Array_Push(cfg->state, &from->outgoingEdges, &edge);
    from->successorCount = (TZrUInt32)from->outgoingEdges.length;
    if (from->successorCount <= ZR_PARSER_CFG_INLINE_SUCCESSOR_CAPACITY) {
        from->successors[from->successorCount - 1U] = toBlockId;
    }
    to->predecessorCount++;
    cfg_update_terminator_for_edge(from, kind);
    return ZR_TRUE;
}

TZrBool cfg_add_edge_kind(SZrParserCfg *cfg,
                          TZrUInt32 fromId,
                          TZrUInt32 toId,
                          EZrParserCfgEdgeKind kind,
                          SZrAstNode *sourceNode) {
    return ZrParser_Cfg_Connect(cfg, fromId, toId, kind, sourceNode);
}

TZrBool cfg_retag_edge_at(SZrParserCfg *cfg,
                          TZrUInt32 fromId,
                          TZrSize edgeIndex,
                          EZrParserCfgEdgeKind kind,
                          SZrAstNode *sourceNode) {
    SZrParserCfgBlock *from = cfg_get_block(cfg, fromId);
    SZrParserCfgEdge *edge;

    if (from == ZR_NULL || !from->outgoingEdges.isValid ||
        edgeIndex >= from->outgoingEdges.length ||
        kind < ZR_PARSER_CFG_EDGE_NORMAL || kind >= ZR_PARSER_CFG_EDGE_ENUM_MAX) {
        return ZR_FALSE;
    }

    edge = (SZrParserCfgEdge *)ZrCore_Array_Get(&from->outgoingEdges, edgeIndex);
    if (edge == ZR_NULL) {
        return ZR_FALSE;
    }
    edge->kind = kind;
    edge->sourceNode = sourceNode;
    cfg_update_terminator_for_edge(from, kind);
    return ZR_TRUE;
}

TZrBool cfg_add_edge(SZrParserCfg *cfg, TZrUInt32 fromId, TZrUInt32 toId) {
    return cfg_add_edge_kind(
            cfg,
            fromId,
            toId,
            ZR_PARSER_CFG_EDGE_NORMAL,
            ZR_NULL);
}

void cfg_clear_blocks(SZrState *state, SZrParserCfg *cfg) {
    TZrSize index;

    if (state == ZR_NULL || cfg == ZR_NULL || !cfg->blocks.isValid) {
        return;
    }
    for (index = 0; index < cfg->blocks.length; index++) {
        SZrParserCfgBlock *block =
                (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg->blocks, index);
        if (block != ZR_NULL) {
            ZrCore_Array_Free(state, &block->outgoingEdges);
        }
    }
    cfg->blocks.length = 0;
}

void ZrParser_Cfg_Init(SZrState *state, SZrParserCfg *cfg) {
    if (state == ZR_NULL || cfg == ZR_NULL) {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->state = state;
    ZrCore_Array_Init(
            state,
            &cfg->blocks,
            sizeof(SZrParserCfgBlock),
            ZR_PARSER_INITIAL_CAPACITY_SMALL);
    cfg->entryBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    cfg->exitBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
}

void ZrParser_Cfg_Free(SZrState *state, SZrParserCfg *cfg) {
    if (state == ZR_NULL || cfg == ZR_NULL) {
        return;
    }

    cfg_clear_blocks(state, cfg);
    ZrCore_Array_Free(state, &cfg->blocks);
    cfg->state = ZR_NULL;
    cfg->entryBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    cfg->exitBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
}
