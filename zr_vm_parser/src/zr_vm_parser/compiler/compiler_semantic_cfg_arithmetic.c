#include "compiler_internal.h"

#include <string.h>

TZrBool compiler_semantic_cfg_return_expression_is_supported(
        const SZrAstNode *node) {
    const TZrChar *op;
    const SZrAstNode *left;
    const SZrAstNode *right;
    if (compiler_semantic_cfg_expression_is_linear(node)) return ZR_TRUE;
    if (node == ZR_NULL || node->type != ZR_AST_BINARY_EXPRESSION) return ZR_FALSE;
    op = node->data.binaryExpression.op.op;
    left = node->data.binaryExpression.left;
    right = node->data.binaryExpression.right;
    return (TZrBool)(op != ZR_NULL && left != ZR_NULL && right != ZR_NULL &&
            left->type == ZR_AST_INTEGER_LITERAL &&
            right->type == ZR_AST_INTEGER_LITERAL &&
            (strcmp(op, "+") == 0 || strcmp(op, "-") == 0));
}
