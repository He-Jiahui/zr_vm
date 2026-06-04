#include "semantic/semantic_analyzer_internal.h"

#include <string.h>

#define ZR_LSP_EXPRESSION_FACT_NEARBY_COLUMN_TOLERANCE ((TZrInt32)4)
#define ZR_LSP_VISIBLE_RANGE_LINE_WIDTH ((TZrSize)100000)

static TZrBool expression_query_source_uri_equals(SZrString *left, SZrString *right) {
    if (left == ZR_NULL || right == ZR_NULL || left == right) {
        return ZR_TRUE;
    }

    return ZrCore_String_Equal(left, right);
}

static TZrBool expression_query_range_contains_position(SZrFileRange range,
                                                        SZrFileRange position) {
    if (!expression_query_source_uri_equals(range.source, position.source)) {
        return ZR_FALSE;
    }

    if (range.start.offset > 0 && range.end.offset > 0 &&
        position.start.offset > 0 && position.end.offset > 0) {
        return range.start.offset <= position.start.offset &&
               position.end.offset <= range.end.offset;
    }

    return (range.start.line < position.start.line ||
            (range.start.line == position.start.line &&
             range.start.column <= position.start.column)) &&
           (position.end.line < range.end.line ||
            (position.end.line == range.end.line &&
             position.end.column <= range.end.column));
}

static TZrBool expression_query_visible_range_contains_position(SZrFileRange range,
                                                                SZrFileRange position) {
    TZrInt32 queryLine;
    TZrInt32 queryColumn;

    if (!expression_query_source_uri_equals(range.source, position.source)) {
        return ZR_FALSE;
    }

    queryLine = position.start.line;
    queryColumn = position.start.column;
    if (queryLine < range.start.line || queryLine > range.end.line) {
        return ZR_FALSE;
    }
    if (queryLine == range.start.line && queryColumn < range.start.column) {
        return ZR_FALSE;
    }
    if (queryLine == range.end.line && queryColumn > range.end.column) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrSize expression_query_visible_range_width(SZrFileRange range) {
    TZrInt32 lineSpan;

    if (range.end.line < range.start.line) {
        return 0;
    }
    if (range.end.line == range.start.line) {
        return range.end.column >= range.start.column
                   ? (TZrSize)(range.end.column - range.start.column)
                   : 0;
    }

    lineSpan = range.end.line - range.start.line;
    return (TZrSize)lineSpan * ZR_LSP_VISIBLE_RANGE_LINE_WIDTH +
           (range.end.column >= 0 ? (TZrSize)range.end.column : 0);
}

static TZrSize expression_query_visible_range_distance(SZrFileRange range,
                                                       SZrFileRange position) {
    TZrInt32 queryLine;
    TZrInt32 queryColumn;

    if (!expression_query_source_uri_equals(range.source, position.source)) {
        return ZR_MAX_SIZE;
    }
    if (expression_query_visible_range_contains_position(range, position)) {
        return 0;
    }

    queryLine = position.start.line;
    queryColumn = position.start.column;
    if (queryLine < range.start.line) {
        return (TZrSize)(range.start.line - queryLine) *
               ZR_LSP_VISIBLE_RANGE_LINE_WIDTH;
    }
    if (queryLine > range.end.line) {
        return (TZrSize)(queryLine - range.end.line) *
               ZR_LSP_VISIBLE_RANGE_LINE_WIDTH;
    }
    if (queryColumn < range.start.column) {
        return (TZrSize)(range.start.column - queryColumn);
    }
    if (queryColumn > range.end.column) {
        return (TZrSize)(queryColumn - range.end.column);
    }
    return 0;
}

static TZrBool expression_query_node_contains_position(SZrAstNode *node,
                                                       SZrFileRange position) {
    return node != ZR_NULL &&
           expression_query_range_contains_position(node->location, position);
}

static TZrBool expression_query_node_visibly_contains_position(SZrAstNode *node,
                                                               SZrFileRange position) {
    return node != ZR_NULL &&
           expression_query_visible_range_contains_position(node->location, position);
}

static TZrBool expression_query_compound_gap_contains_position(SZrAstNode *left,
                                                               SZrAstNode *right,
                                                               SZrFileRange position) {
    TZrInt32 queryLine;
    TZrInt32 queryColumn;

    if (left == ZR_NULL || right == ZR_NULL ||
        !expression_query_source_uri_equals(left->location.source, position.source) ||
        !expression_query_source_uri_equals(right->location.source, position.source)) {
        return ZR_FALSE;
    }

    queryLine = position.start.line;
    queryColumn = position.start.column;
    if (queryLine < left->location.end.line ||
        queryLine > right->location.start.line) {
        return ZR_FALSE;
    }
    if (queryLine == left->location.end.line &&
        queryColumn <= left->location.end.column) {
        return ZR_FALSE;
    }
    if (queryLine == right->location.start.line &&
        queryColumn >= right->location.start.column) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool expression_query_unary_operator_contains_position(SZrAstNode *node,
                                                                 SZrFileRange position) {
    SZrAstNode *argument;
    TZrSize opLength;

    if (node == ZR_NULL ||
        node->type != ZR_AST_UNARY_EXPRESSION ||
        node->data.unaryExpression.argument == ZR_NULL ||
        !expression_query_source_uri_equals(node->location.source, position.source)) {
        return ZR_FALSE;
    }

    argument = node->data.unaryExpression.argument;
    opLength = node->data.unaryExpression.op.op != ZR_NULL
                   ? strlen(node->data.unaryExpression.op.op)
                   : 0;

    if (opLength > 0 &&
        position.start.line == node->location.start.line &&
        position.start.column >= node->location.start.column &&
        position.start.column < node->location.start.column + (TZrInt32)opLength) {
        return ZR_TRUE;
    }

    if (position.start.line == node->location.start.line &&
        argument->location.start.line == node->location.start.line &&
        position.start.column >= node->location.start.column &&
        position.start.column < argument->location.start.column) {
        return ZR_TRUE;
    }

    return position.start.offset >= node->location.start.offset &&
           position.start.offset < argument->location.start.offset;
}

static SZrAstNode *expression_query_find_node_in_array(SZrAstNodeArray *nodes,
                                                       SZrFileRange position);

SZrAstNode *ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
        SZrAstNode *node,
        SZrFileRange position) {
    SZrAstNode *nested = ZR_NULL;

    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            return expression_query_find_node_in_array(node->data.script.statements, position);

        case ZR_AST_BLOCK:
            return expression_query_find_node_in_array(node->data.block.body, position);

        case ZR_AST_VARIABLE_DECLARATION:
            return ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.variableDeclaration.value,
                    position);

        case ZR_AST_FUNCTION_DECLARATION:
            return ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.functionDeclaration.body,
                    position);

        case ZR_AST_TEST_DECLARATION:
            return ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.testDeclaration.body,
                    position);

        case ZR_AST_COMPILE_TIME_DECLARATION:
            return ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.compileTimeDeclaration.declaration,
                    position);

        case ZR_AST_CLASS_DECLARATION:
            return expression_query_find_node_in_array(node->data.classDeclaration.members, position);

        case ZR_AST_STRUCT_DECLARATION:
            return expression_query_find_node_in_array(node->data.structDeclaration.members, position);

        case ZR_AST_INTERFACE_DECLARATION:
            return expression_query_find_node_in_array(node->data.interfaceDeclaration.members, position);

        case ZR_AST_CLASS_FIELD:
            return ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.classField.init,
                    position);

        case ZR_AST_STRUCT_FIELD:
            return ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.structField.init,
                    position);

        case ZR_AST_ENUM_MEMBER:
            return ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.enumMember.value,
                    position);

        case ZR_AST_CLASS_METHOD:
            return ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.classMethod.body,
                    position);

        case ZR_AST_STRUCT_METHOD:
            return ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.structMethod.body,
                    position);

        case ZR_AST_CLASS_META_FUNCTION:
            return ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.classMetaFunction.body,
                    position);

        case ZR_AST_STRUCT_META_FUNCTION:
            return ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.structMetaFunction.body,
                    position);

        case ZR_AST_CLASS_PROPERTY:
            return ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.classProperty.modifier,
                    position);

        case ZR_AST_PROPERTY_GET:
            return ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.propertyGet.body,
                    position);

        case ZR_AST_PROPERTY_SET:
            return ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.propertySet.body,
                    position);

        case ZR_AST_EXPRESSION_STATEMENT:
            return ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.expressionStatement.expr,
                    position);

        case ZR_AST_RETURN_STATEMENT:
            return ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.returnStatement.expr,
                    position);

        case ZR_AST_PRIMARY_EXPRESSION:
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.primaryExpression.property,
                    position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = expression_query_find_node_in_array(node->data.primaryExpression.members, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return expression_query_node_contains_position(node, position) ? node : ZR_NULL;

        case ZR_AST_MEMBER_EXPRESSION:
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.memberExpression.property,
                    position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return expression_query_node_contains_position(node, position) ? node : ZR_NULL;

        case ZR_AST_CONSTRUCT_EXPRESSION:
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.constructExpression.target,
                    position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = expression_query_find_node_in_array(node->data.constructExpression.args, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return expression_query_node_contains_position(node, position) ? node : ZR_NULL;

        case ZR_AST_ASSIGNMENT_EXPRESSION:
            if (expression_query_compound_gap_contains_position(
                    node->data.assignmentExpression.left,
                    node->data.assignmentExpression.right,
                    position)) {
                return node;
            }
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.assignmentExpression.left,
                    position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.assignmentExpression.right,
                    position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return expression_query_node_contains_position(node, position) ||
                           expression_query_node_visibly_contains_position(node, position)
                       ? node
                       : ZR_NULL;

        case ZR_AST_BINARY_EXPRESSION:
            if (expression_query_compound_gap_contains_position(node->data.binaryExpression.left,
                                                               node->data.binaryExpression.right,
                                                               position)) {
                return node;
            }
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.binaryExpression.left,
                    position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.binaryExpression.right,
                    position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return expression_query_node_contains_position(node, position) ||
                           expression_query_node_visibly_contains_position(node, position)
                       ? node
                       : ZR_NULL;

        case ZR_AST_LOGICAL_EXPRESSION:
            if (expression_query_compound_gap_contains_position(node->data.logicalExpression.left,
                                                               node->data.logicalExpression.right,
                                                               position)) {
                return node;
            }
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.logicalExpression.left,
                    position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.logicalExpression.right,
                    position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return expression_query_node_contains_position(node, position) ||
                           expression_query_node_visibly_contains_position(node, position)
                       ? node
                       : ZR_NULL;

        case ZR_AST_UNARY_EXPRESSION:
            if (expression_query_unary_operator_contains_position(node, position)) {
                return node;
            }
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.unaryExpression.argument,
                    position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return expression_query_node_contains_position(node, position) ? node : ZR_NULL;

        case ZR_AST_FUNCTION_CALL:
            nested = expression_query_find_node_in_array(node->data.functionCall.args, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = expression_query_find_node_in_array(node->data.functionCall.genericArguments, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return expression_query_node_contains_position(node, position) ? node : ZR_NULL;

        case ZR_AST_ARRAY_LITERAL:
            nested = expression_query_find_node_in_array(node->data.arrayLiteral.elements, position);
            return nested != ZR_NULL
                       ? nested
                       : (expression_query_node_contains_position(node, position) ? node : ZR_NULL);

        case ZR_AST_OBJECT_LITERAL:
            nested = expression_query_find_node_in_array(node->data.objectLiteral.properties, position);
            return nested != ZR_NULL
                       ? nested
                       : (expression_query_node_contains_position(node, position) ? node : ZR_NULL);

        case ZR_AST_KEY_VALUE_PAIR:
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.keyValuePair.key,
                    position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.keyValuePair.value,
                    position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return expression_query_node_contains_position(node, position) ? node : ZR_NULL;

        case ZR_AST_IF_EXPRESSION:
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.ifExpression.condition,
                    position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.ifExpression.thenExpr,
                    position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.ifExpression.elseExpr,
                    position);
            return nested != ZR_NULL
                       ? nested
                       : (expression_query_node_contains_position(node, position) ? node : ZR_NULL);

        case ZR_AST_SWITCH_EXPRESSION:
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.switchExpression.expr,
                    position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = expression_query_find_node_in_array(node->data.switchExpression.cases, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.switchExpression.defaultCase,
                    position);
            return nested != ZR_NULL
                       ? nested
                       : (expression_query_node_contains_position(node, position) ? node : ZR_NULL);

        case ZR_AST_SWITCH_CASE:
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.switchCase.value,
                    position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.switchCase.block,
                    position);

        case ZR_AST_SWITCH_DEFAULT:
            return ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.switchDefault.block,
                    position);

        case ZR_AST_WHILE_LOOP:
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.whileLoop.cond,
                    position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.whileLoop.block,
                    position);

        case ZR_AST_FOR_LOOP:
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.forLoop.init,
                    position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.forLoop.cond,
                    position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.forLoop.step,
                    position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.forLoop.block,
                    position);

        case ZR_AST_FOREACH_LOOP:
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.foreachLoop.expr,
                    position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.foreachLoop.block,
                    position);

        case ZR_AST_LAMBDA_EXPRESSION:
            nested = ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(
                    node->data.lambdaExpression.block,
                    position);
            return nested != ZR_NULL
                       ? nested
                       : (expression_query_node_contains_position(node, position) ? node : ZR_NULL);

        case ZR_AST_IMPORT_EXPRESSION:
        case ZR_AST_BOOLEAN_LITERAL:
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_STRING_LITERAL:
        case ZR_AST_TEMPLATE_STRING_LITERAL:
        case ZR_AST_CHAR_LITERAL:
        case ZR_AST_NULL_LITERAL:
        case ZR_AST_IDENTIFIER_LITERAL:
            return expression_query_node_contains_position(node, position) ? node : ZR_NULL;

        default:
            return ZR_NULL;
    }
}

static SZrAstNode *expression_query_find_node_in_array(SZrAstNodeArray *nodes,
                                                       SZrFileRange position) {
    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        SZrAstNode *nested =
            ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(nodes->nodes[index],
                                                                           position);
        if (nested != ZR_NULL) {
            return nested;
        }
    }

    return ZR_NULL;
}

const SZrSemanticExpressionFact *ZrLanguageServer_SemanticAnalyzer_FindExpressionFactAtPosition(
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange position) {
    const SZrSemanticExpressionFact *fact;
    const SZrSemanticExpressionFact *bestNearbyFact = ZR_NULL;
    TZrSize bestNearbyDistance = ZR_MAX_SIZE;
    TZrSize bestNearbyWidth = ZR_MAX_SIZE;
    SZrAstNode *expressionNode;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
        return ZR_NULL;
    }

    fact = ZrParser_SemanticFacts_FindExpressionAtPosition(analyzer->semanticContext, position);
    if (fact != ZR_NULL) {
        return fact;
    }

    expressionNode =
        ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(analyzer->ast, position);
    if (expressionNode != ZR_NULL) {
        fact = ZrParser_SemanticFacts_FindExpressionByNode(analyzer->semanticContext, expressionNode);
        if (fact != ZR_NULL) {
            return fact;
        }
    }

    if (!analyzer->semanticContext->expressionFacts.isValid) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < analyzer->semanticContext->expressionFacts.length; index++) {
        const SZrSemanticExpressionFact *candidate =
            (const SZrSemanticExpressionFact *)ZrCore_Array_Get(&analyzer->semanticContext->expressionFacts, index);
        TZrSize distance;
        TZrSize width;

        if (candidate == ZR_NULL ||
            candidate->exactness != ZR_SEMANTIC_FACT_EXACT ||
            !expression_query_source_uri_equals(candidate->range.source, position.source)) {
            continue;
        }

        distance = expression_query_visible_range_distance(candidate->range, position);
        if (distance > (TZrSize)ZR_LSP_EXPRESSION_FACT_NEARBY_COLUMN_TOLERANCE) {
            continue;
        }

        width = expression_query_visible_range_width(candidate->range);
        if (bestNearbyFact == ZR_NULL ||
            distance < bestNearbyDistance ||
            (distance == bestNearbyDistance && width < bestNearbyWidth)) {
            bestNearbyFact = candidate;
            bestNearbyDistance = distance;
            bestNearbyWidth = width;
        }
    }

    return bestNearbyFact;
}
