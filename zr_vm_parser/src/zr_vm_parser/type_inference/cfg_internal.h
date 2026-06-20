#ifndef ZR_VM_PARSER_TYPE_INFERENCE_CFG_INTERNAL_H
#define ZR_VM_PARSER_TYPE_INFERENCE_CFG_INTERNAL_H

#include "zr_vm_parser/cfg.h"
#include "zr_vm_parser/semantic.h"

typedef struct SZrParserCfgLoopTargets {
    TZrUInt32 breakTargetBlockId;
    TZrUInt32 continueTargetBlockId;
} SZrParserCfgLoopTargets;

SZrParserCfgBlock *cfg_get_block(SZrParserCfg *cfg, TZrUInt32 id);
TZrUInt32 cfg_add_block(SZrState *state,
                         SZrParserCfg *cfg,
                         EZrParserCfgBlockKind kind,
                         SZrAstNode *statement);
TZrBool cfg_add_edge(SZrParserCfg *cfg, TZrUInt32 fromId, TZrUInt32 toId);
TZrBool cfg_node_bool_constant(SZrAstNode *node, TZrBool *outValue);
TZrBool cfg_connect_fallthrough(SZrParserCfg *cfg, TZrUInt32 fromId, TZrUInt32 toId);
TZrBool cfg_build_statement_body(SZrState *state,
                                 SZrParserCfg *cfg,
                                 SZrAstNode *body,
                                 TZrUInt32 predecessorBlockId,
                                 EZrSemanticReachabilityCause inheritedCause,
                                 SZrAstNode *inheritedCauseNode,
                                 const SZrParserCfgLoopTargets *loopTargets,
                                 TZrUInt32 *outLastBlockId);

TZrBool cfg_build_switch_statement(SZrState *state,
                                   SZrParserCfg *cfg,
                                   SZrAstNode *statement,
                                   TZrUInt32 *inOutPreviousBlockId,
                                   EZrSemanticReachabilityCause pendingCause,
                                   SZrAstNode *pendingCauseNode,
                                   const SZrParserCfgLoopTargets *loopTargets);
TZrBool cfg_build_try_statement(SZrState *state,
                                SZrParserCfg *cfg,
                                SZrAstNode *statement,
                                TZrUInt32 *inOutPreviousBlockId,
                                EZrSemanticReachabilityCause pendingCause,
                                SZrAstNode *pendingCauseNode,
                                const SZrParserCfgLoopTargets *loopTargets);
TZrBool cfg_build_while_statement(SZrState *state,
                                  SZrParserCfg *cfg,
                                  SZrAstNode *statement,
                                  TZrUInt32 *inOutPreviousBlockId,
                                  EZrSemanticReachabilityCause pendingCause,
                                  SZrAstNode *pendingCauseNode);
TZrBool cfg_build_for_statement(SZrState *state,
                                SZrParserCfg *cfg,
                                SZrAstNode *statement,
                                TZrUInt32 *inOutPreviousBlockId,
                                EZrSemanticReachabilityCause pendingCause,
                                SZrAstNode *pendingCauseNode);
TZrBool cfg_build_foreach_statement(SZrState *state,
                                    SZrParserCfg *cfg,
                                    SZrAstNode *statement,
                                    TZrUInt32 *inOutPreviousBlockId,
                                    EZrSemanticReachabilityCause pendingCause,
                                    SZrAstNode *pendingCauseNode);

#endif // ZR_VM_PARSER_TYPE_INFERENCE_CFG_INTERNAL_H
