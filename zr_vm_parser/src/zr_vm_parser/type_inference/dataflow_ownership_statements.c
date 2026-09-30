#include "dataflow_ownership_statements.h"

static TZrBool ownership_statement_same_source(SZrString *left, SZrString *right) {
    if (left == right) {
        return ZR_TRUE;
    }
    return left != ZR_NULL &&
           right != ZR_NULL &&
           ZrCore_String_Equal(left, right);
}

/* TODO: 核实 source 非空、坐标全零的占位范围是否会进入事实归属；位置类型允许空占位。
 * 下一步追踪所有生产 READ/WRITE/DECLARATION 事实及 AST 合成范围来源，再决定是否增加有效坐标条件并补回归。 */
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

/* 子表达式中的事实会以更窄范围映回所属节点；同源校验避免相同坐标跨文件串接，
 * 缺少可用字节偏移时仍可用行列位置完成同源映射。 */
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

/* CFG 保存的表达式根可能包含更深层的事实节点；节点身份优先，范围用于接回子树关系。 */
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

    /* 复合语句只归属本 CFG 转移负责的条件、选择器、绑定或清理资源；
     * 分支体、case/default 体和循环步骤由各自的 CFG 块处理。 */
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
