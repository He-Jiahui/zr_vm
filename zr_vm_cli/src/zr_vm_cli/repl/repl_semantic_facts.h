#ifndef ZR_VM_CLI_REPL_SEMANTIC_FACTS_H
#define ZR_VM_CLI_REPL_SEMANTIC_FACTS_H

#include "zr_vm_core/state.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"

/** 以下 Write* 把已取得的 parser fact 写入 REPL 结果日志；NULL fact 表示没有可展示信息。 */
void ZrCli_ReplSemanticFacts_WriteNumeric(SZrState *state, const SZrSemanticNumericFact *fact);
/** @brief 在表达式树中定位数值事实并避免重复输出同一 fact。 */
void ZrCli_ReplSemanticFacts_WriteNumericForExpression(SZrState *state,
                                                       SZrSemanticContext *semanticContext,
                                                       SZrAstNode *node);
void ZrCli_ReplSemanticFacts_WriteLogical(SZrState *state, const SZrSemanticLogicalFact *fact);
/** @brief 展示表达式内逻辑已知值和短路事实。 */
void ZrCli_ReplSemanticFacts_WriteLogicalForExpression(SZrState *state,
                                                       SZrSemanticContext *semanticContext,
                                                       SZrAstNode *node);
void ZrCli_ReplSemanticFacts_WriteOwnership(SZrState *state, const SZrSemanticOwnershipFact *fact);
/** @brief 展示表达式内的借用、移动和释放事实，不修改所有权状态。 */
void ZrCli_ReplSemanticFacts_WriteOwnershipForExpression(SZrState *state,
                                                         SZrSemanticContext *semanticContext,
                                                         SZrAstNode *node);
void ZrCli_ReplSemanticFacts_WriteExpression(SZrState *state, const SZrSemanticExpressionFact *fact);
/** @brief 展示表达式本身及语义上相关的接收者和子表达式事实。 */
void ZrCli_ReplSemanticFacts_WriteExpressionForExpression(SZrState *state,
                                                          SZrSemanticContext *semanticContext,
                                                          SZrAstNode *node);
/** @brief 从 parser 记录的源位置定位一次非声明引用；range 属于当前临时 AST。 */
void ZrCli_ReplSemanticFacts_WriteReferenceAtRange(SZrState *state,
                                                   SZrSemanticContext *semanticContext,
                                                   SZrFileRange range);
/** @brief 展示表达式内的标识符和成员引用，供 :type 解释解析目标。 */
void ZrCli_ReplSemanticFacts_WriteReferencesForExpression(SZrState *state,
                                                          SZrSemanticContext *semanticContext,
                                                          SZrAstNode *node);
/** @brief 仅报告不可达 fact；lastFact 用于压制相邻节点共享 fact 的重复输出。 */
void ZrCli_ReplSemanticFacts_WriteReachabilityAtRange(SZrState *state,
                                                      SZrSemanticContext *semanticContext,
                                                      SZrFileRange range,
                                                      const SZrSemanticReachabilityFact **lastFact);
/** @brief 沿 :type 表达式树展示编译期不可达区域及成因。 */
void ZrCli_ReplSemanticFacts_WriteReachabilityForExpression(SZrState *state,
                                                            SZrSemanticContext *semanticContext,
                                                            SZrAstNode *node);

#endif
