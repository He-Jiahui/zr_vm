#ifndef ZR_VM_CLI_REPL_SEMANTIC_EXPRESSION_WALK_H
#define ZR_VM_CLI_REPL_SEMANTIC_EXPRESSION_WALK_H

#include "zr_vm_parser/ast.h"

/** 遍历回调返回 false 时只剪去当前节点的子树，其余兄弟节点仍会被访问。 */
typedef TZrBool (*FZrCliReplSemanticExpressionVisit)(SZrAstNode *node, void *userData);

/** @brief 供 :type 的多类语义事实查询共用 AST 访问顺序。
 *  @note 节点和 userData 均为借用；访问者不得在遍历期间销毁或改写 AST 结构。
 */
void ZrCli_ReplSemanticExpressionWalk(SZrAstNode *node,
                                      FZrCliReplSemanticExpressionVisit visit,
                                      void *userData);

#endif
