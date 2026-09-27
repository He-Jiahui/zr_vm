#include "semantic/semantic_analyzer_internal.h"
#include "zr_vm_parser/semantic_source_metadata.h"

static void typecheck_children(SZrState *state, SZrSemanticAnalyzer *analyzer,
                               SZrAstNodeArray *children) {
    if (children == ZR_NULL || children->nodes == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < children->count; index++) {
        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, children->nodes[index]);
    }
}

/* 值表达式沿用调用方的词法与类型环境；声明、分支和可调用体仍由主调度器建作用域。 */
TZrBool ZrLanguageServer_SemanticAnalyzer_TypecheckValueChildren(
        SZrState *state, SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    SZrAstNode *first = ZR_NULL;
    SZrAstNode *second = ZR_NULL;
    SZrAstNode *third = ZR_NULL;
    SZrAstNodeArray *children = ZR_NULL;
    switch (node->type) {
        case ZR_AST_BINARY_EXPRESSION:
            first = node->data.binaryExpression.left;
            second = node->data.binaryExpression.right;
            break;
        case ZR_AST_UNARY_EXPRESSION:
            first = node->data.unaryExpression.argument;
            break;
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            first = node->data.assignmentExpression.left;
            second = node->data.assignmentExpression.right;
            break;
        case ZR_AST_LOGICAL_EXPRESSION:
            first = node->data.logicalExpression.left;
            second = node->data.logicalExpression.right;
            break;
        case ZR_AST_CONDITIONAL_EXPRESSION:
            first = node->data.conditionalExpression.test;
            second = node->data.conditionalExpression.consequent;
            third = node->data.conditionalExpression.alternate;
            break;
        case ZR_AST_PRIMARY_EXPRESSION:
            first = node->data.primaryExpression.property;
            children = node->data.primaryExpression.members;
            break;
        case ZR_AST_MEMBER_EXPRESSION:
            if (node->data.memberExpression.computed) {
                first = node->data.memberExpression.property;
            }
            break;
        case ZR_AST_FUNCTION_CALL:
            children = node->data.functionCall.args;
            break;
        case ZR_AST_ARRAY_LITERAL:
            children = node->data.arrayLiteral.elements;
            break;
        case ZR_AST_OBJECT_LITERAL:
            children = node->data.objectLiteral.properties;
            break;
        case ZR_AST_KEY_VALUE_PAIR:
            if (node->data.keyValuePair.keyIsComputed) {
                first = node->data.keyValuePair.key;
            }
            second = node->data.keyValuePair.value;
            break;
        case ZR_AST_CLASS_FIELD:
            first = node->data.classField.init;
            break;
        case ZR_AST_STRUCT_FIELD:
            first = node->data.structField.init;
            break;
        case ZR_AST_PARAMETER:
            first = node->data.parameter.defaultValue;
            break;
        case ZR_AST_TEMPLATE_STRING_LITERAL:
            ZrParser_SemanticMetadata_RecordTemplateSegments(analyzer->semanticContext, node);
            children = node->data.templateStringLiteral.segments;
            break;
        case ZR_AST_INTERPOLATED_SEGMENT:
            first = node->data.interpolatedSegment.expression;
            break;
        case ZR_AST_EXPRESSION_STATEMENT:
            first = node->data.expressionStatement.expr;
            break;
        case ZR_AST_RETURN_STATEMENT:
            first = node->data.returnStatement.expr;
            break;
        case ZR_AST_THROW_STATEMENT:
            first = node->data.throwStatement.expr;
            break;
        case ZR_AST_OUT_STATEMENT:
            first = node->data.outStatement.expr;
            break;
        case ZR_AST_YIELD_STATEMENT:
            first = node->data.yieldStatement.expr;
            break;
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            first = node->data.breakContinueStatement.expr;
            break;
        case ZR_AST_TYPE_CAST_EXPRESSION:
            first = node->data.typeCastExpression.expression;
            break;
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            first = node->data.typeQueryExpression.operand;
            break;
        case ZR_AST_CONSTRUCT_EXPRESSION:
            first = node->data.constructExpression.target;
            children = node->data.constructExpression.args;
            break;
        case ZR_AST_STRUCT_INIT_EXPRESSION:
            children = node->data.structInitExpression.args;
            break;
        case ZR_AST_SPREAD_ARGUMENT:
            first = node->data.spreadArgument.expression;
            break;
        case ZR_AST_UNPACK_LITERAL:
            first = node->data.unpackLiteral.element;
            break;
        case ZR_AST_AWAIT_EXPRESSION:
            first = node->data.awaitExpression.operand;
            break;
        case ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION:
            first = node->data.ownershipIntrinsicExpression.argument;
            break;
        case ZR_AST_GENERATOR_EXPRESSION:
            first = node->data.generatorExpression.block;
            break;
        default:
            return ZR_FALSE;
    }
    ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, first);
    ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, second);
    ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, third);
    typecheck_children(state, analyzer, children);
    return ZR_TRUE;
}
