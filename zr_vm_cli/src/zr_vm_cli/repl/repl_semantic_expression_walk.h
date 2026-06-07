#ifndef ZR_VM_CLI_REPL_SEMANTIC_EXPRESSION_WALK_H
#define ZR_VM_CLI_REPL_SEMANTIC_EXPRESSION_WALK_H

#include "zr_vm_parser/ast.h"

typedef TZrBool (*FZrCliReplSemanticExpressionVisit)(SZrAstNode *node, void *userData);

void ZrCli_ReplSemanticExpressionWalk(SZrAstNode *node,
                                      FZrCliReplSemanticExpressionVisit visit,
                                      void *userData);

#endif
