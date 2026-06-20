#ifndef ZR_VM_PARSER_CFG_H
#define ZR_VM_PARSER_CFG_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_core/array.h"

#define ZR_PARSER_CFG_INVALID_BLOCK_ID ((TZrUInt32)0xffffffffU)
#define ZR_PARSER_CFG_MAX_SUCCESSORS 2U

typedef enum EZrParserCfgBlockKind {
    ZR_PARSER_CFG_BLOCK_ENTRY = 0,
    ZR_PARSER_CFG_BLOCK_STATEMENT,
    ZR_PARSER_CFG_BLOCK_JOIN,
    ZR_PARSER_CFG_BLOCK_EXIT
} EZrParserCfgBlockKind;

typedef struct SZrParserCfgBlock {
    TZrUInt32 id;
    EZrParserCfgBlockKind kind;
    SZrAstNode *statement;
    TZrUInt32 successors[ZR_PARSER_CFG_MAX_SUCCESSORS];
    TZrUInt32 successorCount;
    TZrUInt32 predecessorCount;
    TZrBool isTerminator;
    TZrBool visited;
    EZrSemanticReachabilityCause unreachableCause;
    SZrAstNode *unreachableCauseNode;
} SZrParserCfgBlock;

typedef struct SZrParserCfg {
    SZrArray blocks;
    TZrUInt32 entryBlockId;
    TZrUInt32 exitBlockId;
} SZrParserCfg;

ZR_PARSER_API void ZrParser_Cfg_Init(SZrState *state, SZrParserCfg *cfg);
ZR_PARSER_API void ZrParser_Cfg_Free(SZrState *state, SZrParserCfg *cfg);
ZR_PARSER_API TZrBool ZrParser_Cfg_Build(SZrState *state, SZrParserCfg *cfg, SZrAstNode *root);
ZR_PARSER_API TZrBool ZrParser_Cfg_EmitReachabilityFacts(SZrSemanticContext *context, SZrParserCfg *cfg);

#endif // ZR_VM_PARSER_CFG_H
