#ifndef ZR_VM_PARSER_CFG_H
#define ZR_VM_PARSER_CFG_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_core/array.h"

#define ZR_PARSER_CFG_INVALID_BLOCK_ID ((TZrUInt32)0xffffffffU)
#define ZR_PARSER_CFG_INLINE_SUCCESSOR_CAPACITY 2U

typedef enum EZrParserCfgBlockKind {
    ZR_PARSER_CFG_BLOCK_ENTRY = 0,
    ZR_PARSER_CFG_BLOCK_STATEMENT,
    ZR_PARSER_CFG_BLOCK_JOIN,
    ZR_PARSER_CFG_BLOCK_CLEANUP,
    ZR_PARSER_CFG_BLOCK_SUSPENSION,
    ZR_PARSER_CFG_BLOCK_EXIT
} EZrParserCfgBlockKind;

typedef enum EZrParserCfgEdgeKind {
    ZR_PARSER_CFG_EDGE_NORMAL = 0,
    ZR_PARSER_CFG_EDGE_TRUE_BRANCH,
    ZR_PARSER_CFG_EDGE_FALSE_BRANCH,
    ZR_PARSER_CFG_EDGE_SWITCH_CASE,
    ZR_PARSER_CFG_EDGE_SWITCH_DEFAULT,
    ZR_PARSER_CFG_EDGE_EXCEPTION,
    ZR_PARSER_CFG_EDGE_CLEANUP,
    ZR_PARSER_CFG_EDGE_RETURN,
    ZR_PARSER_CFG_EDGE_SUSPEND,
    ZR_PARSER_CFG_EDGE_RESUME,
    ZR_PARSER_CFG_EDGE_ENUM_MAX
} EZrParserCfgEdgeKind;

typedef enum EZrParserCfgTerminatorKind {
    ZR_PARSER_CFG_TERMINATOR_NONE = 0,
    ZR_PARSER_CFG_TERMINATOR_BRANCH,
    ZR_PARSER_CFG_TERMINATOR_SWITCH,
    ZR_PARSER_CFG_TERMINATOR_RETURN,
    ZR_PARSER_CFG_TERMINATOR_THROW,
    ZR_PARSER_CFG_TERMINATOR_BREAK,
    ZR_PARSER_CFG_TERMINATOR_CONTINUE,
    ZR_PARSER_CFG_TERMINATOR_SUSPEND,
    ZR_PARSER_CFG_TERMINATOR_CLEANUP_DISPATCH,
    ZR_PARSER_CFG_TERMINATOR_EXIT
} EZrParserCfgTerminatorKind;

typedef struct SZrParserCfgEdge {
    TZrUInt32 fromBlockId;
    TZrUInt32 toBlockId;
    EZrParserCfgEdgeKind kind;
    SZrAstNode *sourceNode;
} SZrParserCfgEdge;

typedef struct SZrParserCfgBlock {
    TZrUInt32 id;
    EZrParserCfgBlockKind kind;
    SZrAstNode *statement;
    TZrUInt32 firstInstructionIndex;
    TZrUInt32 instructionCount;
    SZrArray outgoingEdges; /* SZrParserCfgEdge */
    /* Compatibility prefix for existing binary CFG tests; use BlockEdgeAt for new code. */
    TZrUInt32 successors[ZR_PARSER_CFG_INLINE_SUCCESSOR_CAPACITY];
    TZrUInt32 successorCount;
    TZrUInt32 predecessorCount;
    EZrParserCfgTerminatorKind terminatorKind;
    TZrBool isTerminator;
    TZrBool visited;
    EZrSemanticReachabilityCause unreachableCause;
    SZrAstNode *unreachableCauseNode;
} SZrParserCfgBlock;

typedef struct SZrParserCfg {
    SZrState *state;
    SZrSemanticContext *semanticContext;
    SZrArray blocks;
    TZrUInt32 entryBlockId;
    TZrUInt32 exitBlockId;
} SZrParserCfg;

ZR_PARSER_API void ZrParser_Cfg_Init(SZrState *state, SZrParserCfg *cfg);
ZR_PARSER_API void ZrParser_Cfg_Free(SZrState *state, SZrParserCfg *cfg);
ZR_PARSER_API TZrUInt32 ZrParser_Cfg_AppendBlock(
        SZrState *state,
        SZrParserCfg *cfg,
        EZrParserCfgBlockKind kind,
        SZrAstNode *statement);
ZR_PARSER_API TZrBool ZrParser_Cfg_Connect(
        SZrParserCfg *cfg,
        TZrUInt32 fromBlockId,
        TZrUInt32 toBlockId,
        EZrParserCfgEdgeKind kind,
        SZrAstNode *sourceNode);
ZR_PARSER_API const SZrParserCfgEdge *ZrParser_Cfg_BlockEdgeAt(
        const SZrParserCfgBlock *block,
        TZrSize index);
ZR_PARSER_API TZrUInt32 ZrParser_Cfg_BlockSuccessorIdAt(
        const SZrParserCfgBlock *block,
        TZrSize index);
ZR_PARSER_API TZrBool ZrParser_Cfg_Build(SZrState *state, SZrParserCfg *cfg, SZrAstNode *root);
ZR_PARSER_API TZrBool ZrParser_Cfg_BuildWithSemanticContext(
        SZrState *state,
        SZrParserCfg *cfg,
        SZrAstNode *root,
        SZrSemanticContext *semanticContext);
ZR_PARSER_API TZrBool ZrParser_Cfg_EmitReachabilityFacts(SZrSemanticContext *context, SZrParserCfg *cfg);

#endif // ZR_VM_PARSER_CFG_H
