#include "dataflow_ownership_statements.h"

static TZrBool ownership_statement_same_source(SZrString *left, SZrString *right) {
    if (left == ZR_NULL || right == ZR_NULL || left == right) {
        return ZR_TRUE;
    }
    return ZrCore_String_Equal(left, right);
}

static TZrBool ownership_statement_range_is_known(const SZrFileRange *range) {
    return range != ZR_NULL &&
           (range->source != ZR_NULL ||
            range->start.line != 0 ||
            range->start.column != 0 ||
            range->start.offset != 0 ||
            range->end.line != 0 ||
            range->end.column != 0 ||
            range->end.offset != 0);
}

static TZrBool ownership_statement_range_contains(const SZrFileRange *outer,
                                                   const SZrFileRange *inner) {
    if (outer == ZR_NULL || inner == ZR_NULL ||
        !ownership_statement_range_is_known(outer) ||
        !ownership_statement_range_is_known(inner) ||
        !ownership_statement_same_source(outer->source, inner->source)) {
        return ZR_FALSE;
    }
    if ((outer->start.offset > 0 || outer->end.offset > 0) &&
        (inner->start.offset > 0 || inner->end.offset > 0)) {
        return inner->start.offset >= outer->start.offset && inner->end.offset <= outer->end.offset;
    }
    if (inner->start.line < outer->start.line || inner->end.line > outer->end.line) {
        return ZR_FALSE;
    }
    if (inner->start.line == outer->start.line && inner->start.column < outer->start.column) {
        return ZR_FALSE;
    }
    return inner->end.line != outer->end.line || inner->end.column <= outer->end.column;
}

static TZrBool ownership_statement_node_contains_fact(
        SZrAstNode *node,
        const SZrSemanticReferenceFact *fact) {
    return node != ZR_NULL &&
           fact != ZR_NULL &&
           (node == fact->node || ownership_statement_range_contains(&node->location, &fact->range));
}

TZrBool ZrParser_DataflowOwnership_FactInStatement(
        SZrAstNode *statement,
        const SZrSemanticReferenceFact *fact) {
    if (statement == ZR_NULL || fact == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (statement->type) {
        case ZR_AST_VARIABLE_DECLARATION:
            return ownership_statement_node_contains_fact(
                           statement->data.variableDeclaration.pattern,
                           fact) ||
                   ownership_statement_node_contains_fact(
                           statement->data.variableDeclaration.value,
                           fact);
        case ZR_AST_EXPRESSION_STATEMENT:
            return ownership_statement_node_contains_fact(
                    statement->data.expressionStatement.expr,
                    fact);
        case ZR_AST_RETURN_STATEMENT:
            return ownership_statement_node_contains_fact(
                    statement->data.returnStatement.expr,
                    fact);
        case ZR_AST_THROW_STATEMENT:
            return ownership_statement_node_contains_fact(
                    statement->data.throwStatement.expr,
                    fact);
        case ZR_AST_OUT_STATEMENT:
            return ownership_statement_node_contains_fact(
                    statement->data.outStatement.expr,
                    fact);
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return ownership_statement_node_contains_fact(
                    statement->data.breakContinueStatement.expr,
                    fact);
        case ZR_AST_IF_EXPRESSION:
            return ownership_statement_node_contains_fact(
                    statement->data.ifExpression.condition,
                    fact);
        case ZR_AST_WHILE_LOOP:
            return ownership_statement_node_contains_fact(statement->data.whileLoop.cond, fact);
        case ZR_AST_FOR_LOOP:
            return ownership_statement_node_contains_fact(statement->data.forLoop.cond, fact);
        case ZR_AST_FOREACH_LOOP:
            return ownership_statement_node_contains_fact(statement->data.foreachLoop.expr, fact) ||
                   ownership_statement_node_contains_fact(statement->data.foreachLoop.pattern, fact);
        case ZR_AST_SWITCH_EXPRESSION:
            return ownership_statement_node_contains_fact(
                    statement->data.switchExpression.expr,
                    fact);
        case ZR_AST_SWITCH_CASE:
            return ownership_statement_node_contains_fact(statement->data.switchCase.value, fact);
        case ZR_AST_USING_STATEMENT:
            return ownership_statement_node_contains_fact(
                    statement->data.usingStatement.resource,
                    fact);
        case ZR_AST_FUNCTION_DECLARATION:
        case ZR_AST_STRUCT_DECLARATION:
        case ZR_AST_CLASS_DECLARATION:
        case ZR_AST_INTERFACE_DECLARATION:
        case ZR_AST_ENUM_DECLARATION:
        case ZR_AST_STRUCT_METHOD:
        case ZR_AST_STRUCT_META_FUNCTION:
        case ZR_AST_CLASS_METHOD:
        case ZR_AST_CLASS_META_FUNCTION:
        case ZR_AST_BLOCK:
        case ZR_AST_CATCH_CLAUSE:
        case ZR_AST_SWITCH_DEFAULT:
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            return ZR_FALSE;
        default:
            return ownership_statement_node_contains_fact(statement, fact);
    }
}
