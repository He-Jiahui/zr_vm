#include "dataflow_ownership_regions.h"

#include <string.h>

#include "zr_vm_parser/semantic.h"

static TZrBool ownership_region_same_source(SZrString *left, SZrString *right) {
    return left == ZR_NULL || right == ZR_NULL || left == right || ZrCore_String_Equal(left, right);
}

static TZrBool ownership_region_range_contains(const SZrFileRange *outer,
                                                const SZrFileRange *inner) {
    if (outer == ZR_NULL || inner == ZR_NULL ||
        !ownership_region_same_source(outer->source, inner->source)) {
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

static TZrBool ownership_region_node_contains_reference(
        SZrAstNode *node,
        const SZrSemanticReferenceFact *fact) {
    return node != ZR_NULL &&
           fact != ZR_NULL &&
           (node == fact->node || ownership_region_range_contains(&node->location, &fact->range));
}

static const SZrSemanticReferenceFact *ownership_region_find_reference(
        const SZrSemanticContext *context,
        SZrAstNode *node,
        EZrSemanticReferenceKind kind) {
    const SZrSemanticReferenceFact *result = ZR_NULL;
    TZrSize index;

    if (context == ZR_NULL || node == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_NULL;
    }
    for (index = 0; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts,
                        index);
        if (fact != ZR_NULL &&
            fact->kind == kind &&
            fact->isResolved &&
            fact->symbolId != ZR_SEMANTIC_ID_INVALID &&
            ownership_region_node_contains_reference(node, fact)) {
            if (result == ZR_NULL || fact->symbolId < result->symbolId) {
                result = fact;
            }
        }
    }
    return result;
}

const SZrSemanticReferenceFact *ZrParser_DataflowOwnership_ConstructTargetRead(
        const SZrSemanticContext *context,
        SZrAstNode *constructNode) {
    if (constructNode == ZR_NULL || constructNode->type != ZR_AST_CONSTRUCT_EXPRESSION) {
        return ZR_NULL;
    }
    return ownership_region_find_reference(context,
                                           constructNode->data.constructExpression.target,
                                           ZR_SEMANTIC_REFERENCE_READ);
}

static TZrBool ownership_region_current_member_projection(
        SZrAstNode *node,
        EZrOwnershipBuiltinKind *outBuiltinKind,
        SZrAstNode **outTarget) {
    if (outBuiltinKind != ZR_NULL) {
        *outBuiltinKind = ZR_OWNERSHIP_BUILTIN_KIND_NONE;
    }
    if (outTarget != ZR_NULL) {
        *outTarget = ZR_NULL;
    }
    if (node == ZR_NULL ||
        node->type != ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION ||
        node->data.ownershipIntrinsicExpression.operation !=
                ZR_OWNERSHIP_INTRINSIC_DEGRADE ||
        node->data.ownershipIntrinsicExpression.argument == ZR_NULL) {
        return ZR_FALSE;
    }
    if (outBuiltinKind != ZR_NULL) {
        *outBuiltinKind = ZR_OWNERSHIP_BUILTIN_KIND_WEAK;
    }
    if (outTarget != ZR_NULL) {
        *outTarget = node->data.ownershipIntrinsicExpression.argument;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DataflowOwnership_StatementRegionBinding(
        const SZrSemanticContext *context,
        SZrAstNode *statement,
        SZrDataflowOwnershipRegionBinding *outBinding) {
    SZrAstNode *constructNode;
    SZrAstNode *aliasNode;
    SZrAstNode *ownerNode = ZR_NULL;
    EZrSemanticReferenceKind aliasReferenceKind;
    EZrOwnershipBuiltinKind builtinKind;

    if (outBinding != ZR_NULL) {
        memset(outBinding, 0, sizeof(*outBinding));
    }
    if (context == ZR_NULL ||
        statement == ZR_NULL ||
        outBinding == ZR_NULL) {
        return ZR_FALSE;
    }

    if (statement->type == ZR_AST_VARIABLE_DECLARATION) {
        constructNode = statement->data.variableDeclaration.value;
        aliasNode = statement->data.variableDeclaration.pattern;
        aliasReferenceKind = ZR_SEMANTIC_REFERENCE_DECLARATION;
        outBinding->isDeclaration = ZR_TRUE;
    } else if (statement->type == ZR_AST_EXPRESSION_STATEMENT &&
               statement->data.expressionStatement.expr != ZR_NULL &&
               statement->data.expressionStatement.expr->type == ZR_AST_ASSIGNMENT_EXPRESSION &&
               statement->data.expressionStatement.expr->data.assignmentExpression.op.op != ZR_NULL &&
               strcmp(statement->data.expressionStatement.expr->data.assignmentExpression.op.op,
                      "=") == 0) {
        SZrAssignmentExpression *assignment =
                &statement->data.expressionStatement.expr->data.assignmentExpression;
        constructNode = assignment->right;
        aliasNode = assignment->left;
        aliasReferenceKind = ZR_SEMANTIC_REFERENCE_WRITE;
    } else {
        return ZR_FALSE;
    }
    if (constructNode == ZR_NULL) {
        return ZR_FALSE;
    }
    if (constructNode->type == ZR_AST_CONSTRUCT_EXPRESSION) {
        builtinKind = constructNode->data.constructExpression.builtinKind;
        ownerNode = constructNode->data.constructExpression.target;
    } else if (!ownership_region_current_member_projection(
                       constructNode,
                       &builtinKind,
                       &ownerNode)) {
        return ZR_FALSE;
    }
    if (builtinKind != ZR_OWNERSHIP_BUILTIN_KIND_BORROW &&
        builtinKind != ZR_OWNERSHIP_BUILTIN_KIND_LOAN &&
        builtinKind != ZR_OWNERSHIP_BUILTIN_KIND_WEAK) {
        return ZR_FALSE;
    }

    outBinding->aliasReference = ownership_region_find_reference(
            context,
            aliasNode,
            aliasReferenceKind);
    outBinding->ownerReference = ownership_region_find_reference(
            context,
            ownerNode,
            ZR_SEMANTIC_REFERENCE_READ);
    if (outBinding->aliasReference == ZR_NULL || outBinding->ownerReference == ZR_NULL) {
        memset(outBinding, 0, sizeof(*outBinding));
        return ZR_FALSE;
    }

    outBinding->constructNode = constructNode;
    if (builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_BORROW) {
        outBinding->qualifier = ZR_OWNERSHIP_QUALIFIER_BORROWED;
    } else if (builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_LOAN) {
        outBinding->qualifier = ZR_OWNERSHIP_QUALIFIER_LOANED;
    } else {
        outBinding->qualifier = ZR_OWNERSHIP_QUALIFIER_WEAK;
    }
    return ZR_TRUE;
}

static SZrAstNode *ownership_region_statement_expression(SZrAstNode *statement) {
    if (statement == ZR_NULL) {
        return ZR_NULL;
    }
    switch (statement->type) {
        case ZR_AST_VARIABLE_DECLARATION:
            return statement->data.variableDeclaration.value;
        case ZR_AST_EXPRESSION_STATEMENT:
            return statement->data.expressionStatement.expr;
        case ZR_AST_RETURN_STATEMENT:
            return statement->data.returnStatement.expr;
        case ZR_AST_THROW_STATEMENT:
            return statement->data.throwStatement.expr;
        case ZR_AST_OUT_STATEMENT:
            return statement->data.outStatement.expr;
        default:
            return ZR_NULL;
    }
}

static TZrBool ownership_region_expression_releases_read(
        SZrAstNode *expression,
        const SZrSemanticReferenceFact *fact) {
    if (expression == ZR_NULL || fact == ZR_NULL) {
        return ZR_FALSE;
    }
    if (expression->type == ZR_AST_CONSTRUCT_EXPRESSION) {
        return expression->data.constructExpression.builtinKind ==
                       ZR_OWNERSHIP_BUILTIN_KIND_RELEASE &&
               ownership_region_node_contains_reference(
                       expression->data.constructExpression.target,
                       fact);
    }
    if (expression->type == ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION) {
        return expression->data.ownershipIntrinsicExpression.operation ==
                       ZR_OWNERSHIP_INTRINSIC_DROP &&
               ownership_region_node_contains_reference(
                       expression->data.ownershipIntrinsicExpression.argument,
                       fact);
    }
    if (expression->type == ZR_AST_ASSIGNMENT_EXPRESSION &&
        expression->data.assignmentExpression.op.op != ZR_NULL &&
        strcmp(expression->data.assignmentExpression.op.op, "=") == 0) {
        return ownership_region_expression_releases_read(
                expression->data.assignmentExpression.right,
                fact);
    }
    return ZR_FALSE;
}

TZrBool ZrParser_DataflowOwnership_StatementReleasesRead(
        SZrAstNode *statement,
        const SZrSemanticReferenceFact *fact) {
    if (statement != ZR_NULL && statement->type == ZR_AST_USING_STATEMENT) {
        return ownership_region_node_contains_reference(
                statement->data.usingStatement.resource,
                fact);
    }
    return ownership_region_expression_releases_read(
            ownership_region_statement_expression(statement),
            fact);
}
