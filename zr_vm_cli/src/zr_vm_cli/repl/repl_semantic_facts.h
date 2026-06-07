#ifndef ZR_VM_CLI_REPL_SEMANTIC_FACTS_H
#define ZR_VM_CLI_REPL_SEMANTIC_FACTS_H

#include "zr_vm_core/state.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"

void ZrCli_ReplSemanticFacts_WriteNumeric(SZrState *state, const SZrSemanticNumericFact *fact);
void ZrCli_ReplSemanticFacts_WriteNumericForExpression(SZrState *state,
                                                       SZrSemanticContext *semanticContext,
                                                       SZrAstNode *node);
void ZrCli_ReplSemanticFacts_WriteLogical(SZrState *state, const SZrSemanticLogicalFact *fact);
void ZrCli_ReplSemanticFacts_WriteLogicalForExpression(SZrState *state,
                                                       SZrSemanticContext *semanticContext,
                                                       SZrAstNode *node);
void ZrCli_ReplSemanticFacts_WriteOwnership(SZrState *state, const SZrSemanticOwnershipFact *fact);
void ZrCli_ReplSemanticFacts_WriteOwnershipForExpression(SZrState *state,
                                                         SZrSemanticContext *semanticContext,
                                                         SZrAstNode *node);
void ZrCli_ReplSemanticFacts_WriteExpression(SZrState *state, const SZrSemanticExpressionFact *fact);
void ZrCli_ReplSemanticFacts_WriteExpressionForExpression(SZrState *state,
                                                          SZrSemanticContext *semanticContext,
                                                          SZrAstNode *node);
void ZrCli_ReplSemanticFacts_WriteReferenceAtRange(SZrState *state,
                                                   SZrSemanticContext *semanticContext,
                                                   SZrFileRange range);
void ZrCli_ReplSemanticFacts_WriteReferencesForExpression(SZrState *state,
                                                          SZrSemanticContext *semanticContext,
                                                          SZrAstNode *node);
void ZrCli_ReplSemanticFacts_WriteReachabilityAtRange(SZrState *state,
                                                      SZrSemanticContext *semanticContext,
                                                      SZrFileRange range,
                                                      const SZrSemanticReachabilityFact **lastFact);
void ZrCli_ReplSemanticFacts_WriteReachabilityForExpression(SZrState *state,
                                                            SZrSemanticContext *semanticContext,
                                                            SZrAstNode *node);

#endif
