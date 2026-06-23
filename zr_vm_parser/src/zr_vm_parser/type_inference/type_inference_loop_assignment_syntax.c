#include "type_inference_loop_assignment_syntax.h"

#include <string.h>

SZrAstNode *ZrParser_TypeInferenceLoopAssignment_StatementExpression(SZrAstNode *statement) {
    if (statement == ZR_NULL) {
        return ZR_NULL;
    }

    if (statement->type == ZR_AST_EXPRESSION_STATEMENT) {
        return statement->data.expressionStatement.expr;
    }
    return statement;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_StatementIsPlainBreak(SZrAstNode *statement) {
    return statement != ZR_NULL &&
           statement->type == ZR_AST_BREAK_CONTINUE_STATEMENT &&
           statement->data.breakContinueStatement.isBreak &&
           statement->data.breakContinueStatement.expr == ZR_NULL;
}

static TZrBool type_inference_loop_assignment_block_guarantees_plain_break(SZrAstNode *block);

TZrBool ZrParser_TypeInferenceLoopAssignment_StatementGuaranteesPlainBreak(SZrAstNode *statement) {
    SZrAstNode *expression;
    SZrIfExpression *ifExpression;

    if (statement == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrParser_TypeInferenceLoopAssignment_StatementIsPlainBreak(statement)) {
        return ZR_TRUE;
    }
    if (statement->type == ZR_AST_BREAK_CONTINUE_STATEMENT) {
        return ZR_FALSE;
    }
    if (statement->type == ZR_AST_BLOCK) {
        return type_inference_loop_assignment_block_guarantees_plain_break(statement);
    }

    expression = ZrParser_TypeInferenceLoopAssignment_StatementExpression(statement);
    if (expression == ZR_NULL || expression->type != ZR_AST_IF_EXPRESSION) {
        return ZR_FALSE;
    }

    ifExpression = &expression->data.ifExpression;
    return ifExpression->thenExpr != ZR_NULL &&
           ifExpression->elseExpr != ZR_NULL &&
           ZrParser_TypeInferenceLoopAssignment_StatementGuaranteesPlainBreak(
                   ifExpression->thenExpr) &&
           ZrParser_TypeInferenceLoopAssignment_StatementGuaranteesPlainBreak(
                   ifExpression->elseExpr);
}

static TZrBool type_inference_loop_assignment_block_guarantees_plain_break(SZrAstNode *block) {
    SZrAstNodeArray *body;
    TZrSize index;

    if (block == ZR_NULL || block->type != ZR_AST_BLOCK) {
        return ZrParser_TypeInferenceLoopAssignment_StatementGuaranteesPlainBreak(block);
    }

    body = block->data.block.body;
    if (body == ZR_NULL || body->nodes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < body->count; index++) {
        SZrAstNode *statement = body->nodes[index];
        SZrAstNode *expression;

        if (ZrParser_TypeInferenceLoopAssignment_StatementGuaranteesPlainBreak(statement)) {
            return ZR_TRUE;
        }
        if (statement != ZR_NULL && statement->type == ZR_AST_BREAK_CONTINUE_STATEMENT) {
            return ZR_FALSE;
        }
        expression = ZrParser_TypeInferenceLoopAssignment_StatementExpression(statement);
        if (expression != ZR_NULL && expression->type == ZR_AST_IF_EXPRESSION) {
            return ZR_FALSE;
        }
    }
    return ZR_FALSE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_AssignmentParts(SZrAstNode *assignmentNode,
                                                             SZrString **outName,
                                                             SZrAstNode **outRight) {
    SZrAssignmentExpression *assignment;

    if (outName != ZR_NULL) {
        *outName = ZR_NULL;
    }
    if (outRight != ZR_NULL) {
        *outRight = ZR_NULL;
    }

    if (assignmentNode == ZR_NULL ||
        assignmentNode->type != ZR_AST_ASSIGNMENT_EXPRESSION ||
        outName == ZR_NULL ||
        outRight == ZR_NULL) {
        return ZR_FALSE;
    }

    assignment = &assignmentNode->data.assignmentExpression;
    if (assignment->op.op == ZR_NULL ||
        strcmp(assignment->op.op, "=") != 0 ||
        assignment->left == ZR_NULL ||
        assignment->left->type != ZR_AST_IDENTIFIER_LITERAL ||
        assignment->left->data.identifier.name == ZR_NULL ||
        assignment->right == ZR_NULL) {
        return ZR_FALSE;
    }

    *outName = assignment->left->data.identifier.name;
    *outRight = assignment->right;
    return ZR_TRUE;
}
