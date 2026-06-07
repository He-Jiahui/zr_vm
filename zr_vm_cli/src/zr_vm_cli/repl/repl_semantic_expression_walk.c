#include "repl/repl_semantic_expression_walk.h"

static void repl_semantic_expression_walk_list(SZrAstNodeArray *nodes,
                                               FZrCliReplSemanticExpressionVisit visit,
                                               void *userData) {
    TZrSize index;

    if (nodes == ZR_NULL) {
        return;
    }

    for (index = 0; index < nodes->count; ++index) {
        ZrCli_ReplSemanticExpressionWalk(nodes->nodes[index], visit, userData);
    }
}

void ZrCli_ReplSemanticExpressionWalk(SZrAstNode *node,
                                      FZrCliReplSemanticExpressionVisit visit,
                                      void *userData) {
    if (node == ZR_NULL) {
        return;
    }

    if (visit != ZR_NULL && !visit(node, userData)) {
        return;
    }

    switch (node->type) {
        case ZR_AST_BINARY_EXPRESSION:
            ZrCli_ReplSemanticExpressionWalk(node->data.binaryExpression.left, visit, userData);
            ZrCli_ReplSemanticExpressionWalk(node->data.binaryExpression.right, visit, userData);
            break;
        case ZR_AST_LOGICAL_EXPRESSION:
            ZrCli_ReplSemanticExpressionWalk(node->data.logicalExpression.left, visit, userData);
            ZrCli_ReplSemanticExpressionWalk(node->data.logicalExpression.right, visit, userData);
            break;
        case ZR_AST_UNARY_EXPRESSION:
            ZrCli_ReplSemanticExpressionWalk(node->data.unaryExpression.argument, visit, userData);
            break;
        case ZR_AST_TYPE_CAST_EXPRESSION:
            ZrCli_ReplSemanticExpressionWalk(node->data.typeCastExpression.expression, visit, userData);
            break;
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            ZrCli_ReplSemanticExpressionWalk(node->data.typeQueryExpression.operand, visit, userData);
            break;
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            ZrCli_ReplSemanticExpressionWalk(node->data.assignmentExpression.left, visit, userData);
            ZrCli_ReplSemanticExpressionWalk(node->data.assignmentExpression.right, visit, userData);
            break;
        case ZR_AST_CONDITIONAL_EXPRESSION:
            ZrCli_ReplSemanticExpressionWalk(node->data.conditionalExpression.test, visit, userData);
            ZrCli_ReplSemanticExpressionWalk(node->data.conditionalExpression.consequent, visit, userData);
            ZrCli_ReplSemanticExpressionWalk(node->data.conditionalExpression.alternate, visit, userData);
            break;
        case ZR_AST_IF_EXPRESSION:
            ZrCli_ReplSemanticExpressionWalk(node->data.ifExpression.condition, visit, userData);
            ZrCli_ReplSemanticExpressionWalk(node->data.ifExpression.thenExpr, visit, userData);
            ZrCli_ReplSemanticExpressionWalk(node->data.ifExpression.elseExpr, visit, userData);
            break;
        case ZR_AST_WHILE_LOOP:
            ZrCli_ReplSemanticExpressionWalk(node->data.whileLoop.cond, visit, userData);
            ZrCli_ReplSemanticExpressionWalk(node->data.whileLoop.block, visit, userData);
            break;
        case ZR_AST_FOR_LOOP:
            ZrCli_ReplSemanticExpressionWalk(node->data.forLoop.init, visit, userData);
            ZrCli_ReplSemanticExpressionWalk(node->data.forLoop.cond, visit, userData);
            ZrCli_ReplSemanticExpressionWalk(node->data.forLoop.step, visit, userData);
            ZrCli_ReplSemanticExpressionWalk(node->data.forLoop.block, visit, userData);
            break;
        case ZR_AST_FOREACH_LOOP:
            ZrCli_ReplSemanticExpressionWalk(node->data.foreachLoop.pattern, visit, userData);
            ZrCli_ReplSemanticExpressionWalk(node->data.foreachLoop.expr, visit, userData);
            ZrCli_ReplSemanticExpressionWalk(node->data.foreachLoop.block, visit, userData);
            break;
        case ZR_AST_SWITCH_EXPRESSION:
            ZrCli_ReplSemanticExpressionWalk(node->data.switchExpression.expr, visit, userData);
            repl_semantic_expression_walk_list(node->data.switchExpression.cases, visit, userData);
            ZrCli_ReplSemanticExpressionWalk(node->data.switchExpression.defaultCase, visit, userData);
            break;
        case ZR_AST_SWITCH_CASE:
            ZrCli_ReplSemanticExpressionWalk(node->data.switchCase.value, visit, userData);
            ZrCli_ReplSemanticExpressionWalk(node->data.switchCase.block, visit, userData);
            break;
        case ZR_AST_SWITCH_DEFAULT:
            ZrCli_ReplSemanticExpressionWalk(node->data.switchDefault.block, visit, userData);
            break;
        case ZR_AST_LAMBDA_EXPRESSION:
            ZrCli_ReplSemanticExpressionWalk(node->data.lambdaExpression.block, visit, userData);
            break;
        case ZR_AST_FUNCTION_CALL:
            repl_semantic_expression_walk_list(node->data.functionCall.args, visit, userData);
            break;
        case ZR_AST_PRIMARY_EXPRESSION:
            ZrCli_ReplSemanticExpressionWalk(node->data.primaryExpression.property, visit, userData);
            repl_semantic_expression_walk_list(node->data.primaryExpression.members, visit, userData);
            break;
        case ZR_AST_MEMBER_EXPRESSION:
            ZrCli_ReplSemanticExpressionWalk(node->data.memberExpression.property, visit, userData);
            break;
        case ZR_AST_CONSTRUCT_EXPRESSION:
            ZrCli_ReplSemanticExpressionWalk(node->data.constructExpression.target, visit, userData);
            repl_semantic_expression_walk_list(node->data.constructExpression.args, visit, userData);
            break;
        case ZR_AST_ARRAY_LITERAL:
            repl_semantic_expression_walk_list(node->data.arrayLiteral.elements, visit, userData);
            break;
        case ZR_AST_OBJECT_LITERAL:
            repl_semantic_expression_walk_list(node->data.objectLiteral.properties, visit, userData);
            break;
        case ZR_AST_KEY_VALUE_PAIR:
            ZrCli_ReplSemanticExpressionWalk(node->data.keyValuePair.key, visit, userData);
            ZrCli_ReplSemanticExpressionWalk(node->data.keyValuePair.value, visit, userData);
            break;
        case ZR_AST_BLOCK:
            repl_semantic_expression_walk_list(node->data.block.body, visit, userData);
            break;
        case ZR_AST_EXPRESSION_STATEMENT:
            ZrCli_ReplSemanticExpressionWalk(node->data.expressionStatement.expr, visit, userData);
            break;
        case ZR_AST_RETURN_STATEMENT:
            ZrCli_ReplSemanticExpressionWalk(node->data.returnStatement.expr, visit, userData);
            break;
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            ZrCli_ReplSemanticExpressionWalk(node->data.breakContinueStatement.expr, visit, userData);
            break;
        case ZR_AST_THROW_STATEMENT:
            ZrCli_ReplSemanticExpressionWalk(node->data.throwStatement.expr, visit, userData);
            break;
        case ZR_AST_USING_STATEMENT:
            ZrCli_ReplSemanticExpressionWalk(node->data.usingStatement.resource, visit, userData);
            ZrCli_ReplSemanticExpressionWalk(node->data.usingStatement.body, visit, userData);
            break;
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            ZrCli_ReplSemanticExpressionWalk(node->data.tryCatchFinallyStatement.block, visit, userData);
            repl_semantic_expression_walk_list(node->data.tryCatchFinallyStatement.catchClauses, visit, userData);
            ZrCli_ReplSemanticExpressionWalk(node->data.tryCatchFinallyStatement.finallyBlock, visit, userData);
            break;
        case ZR_AST_CATCH_CLAUSE:
            repl_semantic_expression_walk_list(node->data.catchClause.pattern, visit, userData);
            ZrCli_ReplSemanticExpressionWalk(node->data.catchClause.block, visit, userData);
            break;
        case ZR_AST_VARIABLE_DECLARATION:
            ZrCli_ReplSemanticExpressionWalk(node->data.variableDeclaration.value, visit, userData);
            break;
        default:
            break;
    }
}
