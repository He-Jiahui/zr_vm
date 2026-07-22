#include "dataflow_ownership_moves.h"

#include <string.h>

#include "zr_vm_parser/semantic.h"

static TZrBool ownership_move_names_equal(SZrString *left, SZrString *right) {
    return left != ZR_NULL &&
           right != ZR_NULL &&
           (left == right || ZrCore_String_Equal(left, right));
}

static TZrBool ownership_move_has_offset(const SZrFilePosition *position) {
    return position != ZR_NULL && position->offset > 0;
}

static TZrBool ownership_move_same_source(SZrString *left, SZrString *right) {
    return left == ZR_NULL || right == ZR_NULL || left == right || ZrCore_String_Equal(left, right);
}

static TZrBool ownership_move_range_contains(const SZrFileRange *outer,
                                              const SZrFileRange *inner) {
    if (outer == ZR_NULL || inner == ZR_NULL ||
        !ownership_move_same_source(outer->source, inner->source)) {
        return ZR_FALSE;
    }
    if ((ownership_move_has_offset(&outer->start) || ownership_move_has_offset(&outer->end)) &&
        (ownership_move_has_offset(&inner->start) || ownership_move_has_offset(&inner->end))) {
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

static TZrBool ownership_move_node_contains_fact(SZrAstNode *node,
                                                  const SZrSemanticReferenceFact *fact) {
    return node != ZR_NULL &&
           fact != ZR_NULL &&
           (node == fact->node || ownership_move_range_contains(&node->location, &fact->range));
}

static const SZrSemanticReferenceFact *ownership_move_find_call_reference(
        const SZrSemanticContext *context,
        const SZrAstNode *callOwner) {
    TZrSize index;

    if (context == ZR_NULL || callOwner == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_NULL;
    }
    for (index = 0; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts,
                        index);
        if (fact != ZR_NULL &&
            fact->kind == ZR_SEMANTIC_REFERENCE_CALL &&
            (fact->node == callOwner ||
             (callOwner->type == ZR_AST_PRIMARY_EXPRESSION &&
              fact->node == callOwner->data.primaryExpression.property)) &&
            fact->isResolved &&
            fact->symbolId != ZR_SEMANTIC_ID_INVALID) {
            return fact;
        }
    }
    return ZR_NULL;
}

static const SZrSemanticSymbolRecord *ownership_move_find_symbol(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId) {
    TZrSize index;

    if (context == ZR_NULL || !context->symbols.isValid || symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }
    for (index = 0; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols,
                        index);
        if (symbol != ZR_NULL && symbol->id == symbolId) {
            return symbol;
        }
    }
    return ZR_NULL;
}

static SZrAstNodeArray *ownership_move_callable_parameters(SZrAstNode *node,
                                                           SZrParameter **outVariadic) {
    if (outVariadic != ZR_NULL) {
        *outVariadic = ZR_NULL;
    }
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    switch (node->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            if (outVariadic != ZR_NULL) *outVariadic = node->data.functionDeclaration.args;
            return node->data.functionDeclaration.params;
        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            if (outVariadic != ZR_NULL) *outVariadic = node->data.externFunctionDeclaration.args;
            return node->data.externFunctionDeclaration.params;
        case ZR_AST_EXTERN_DELEGATE_DECLARATION:
            if (outVariadic != ZR_NULL) *outVariadic = node->data.externDelegateDeclaration.args;
            return node->data.externDelegateDeclaration.params;
        case ZR_AST_STRUCT_METHOD:
            if (outVariadic != ZR_NULL) *outVariadic = node->data.structMethod.args;
            return node->data.structMethod.params;
        case ZR_AST_STRUCT_META_FUNCTION:
            if (outVariadic != ZR_NULL) *outVariadic = node->data.structMetaFunction.args;
            return node->data.structMetaFunction.params;
        case ZR_AST_CLASS_METHOD:
            if (outVariadic != ZR_NULL) *outVariadic = node->data.classMethod.args;
            return node->data.classMethod.params;
        case ZR_AST_CLASS_META_FUNCTION:
            if (outVariadic != ZR_NULL) *outVariadic = node->data.classMetaFunction.args;
            return node->data.classMetaFunction.params;
        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            if (outVariadic != ZR_NULL) *outVariadic = node->data.interfaceMethodSignature.args;
            return node->data.interfaceMethodSignature.params;
        case ZR_AST_INTERFACE_META_SIGNATURE:
            if (outVariadic != ZR_NULL) *outVariadic = node->data.interfaceMetaSignature.args;
            return node->data.interfaceMetaSignature.params;
        default:
            return ZR_NULL;
    }
}

static SZrString *ownership_move_argument_name(const SZrFunctionCall *call, TZrSize index) {
    SZrString **name;

    if (call == ZR_NULL || call->argNames == ZR_NULL || !call->argNames->isValid ||
        index >= call->argNames->length) {
        return ZR_NULL;
    }
    name = (SZrString **)ZrCore_Array_Get(call->argNames, index);
    return name != ZR_NULL ? *name : ZR_NULL;
}

static const SZrParameter *ownership_move_parameter_at(SZrAstNodeArray *parameters,
                                                        SZrParameter *variadic,
                                                        const SZrFunctionCall *call,
                                                        TZrSize argumentIndex) {
    SZrString *argumentName = ownership_move_argument_name(call, argumentIndex);
    TZrSize index;

    if (parameters != ZR_NULL && argumentName == ZR_NULL && argumentIndex < parameters->count) {
        SZrAstNode *parameterNode = parameters->nodes[argumentIndex];
        return parameterNode != ZR_NULL && parameterNode->type == ZR_AST_PARAMETER
                       ? &parameterNode->data.parameter
                       : ZR_NULL;
    }
    for (index = 0; parameters != ZR_NULL && argumentName != ZR_NULL &&
                    index < parameters->count; index++) {
        SZrAstNode *parameterNode = parameters->nodes[index];
        if (parameterNode != ZR_NULL &&
            parameterNode->type == ZR_AST_PARAMETER &&
            parameterNode->data.parameter.name != ZR_NULL &&
            ownership_move_names_equal(parameterNode->data.parameter.name->name, argumentName)) {
            return &parameterNode->data.parameter;
        }
    }
    return argumentName == ZR_NULL ? variadic : ZR_NULL;
}

static TZrBool ownership_move_expression_is_direct_reference(
        SZrAstNode *expression,
        const SZrSemanticReferenceFact *fact) {
    if (expression == ZR_NULL || fact == ZR_NULL) {
        return ZR_FALSE;
    }
    if (expression == fact->node) {
        return ZR_TRUE;
    }
    return expression->type == ZR_AST_PRIMARY_EXPRESSION &&
           (expression->data.primaryExpression.members == ZR_NULL ||
            expression->data.primaryExpression.members->count == 0) &&
           expression->data.primaryExpression.property == fact->node;
}

static const SZrParameter *ownership_move_argument_parameter(
        const SZrSemanticContext *context,
        SZrAstNode *callOwner,
        const SZrFunctionCall *call,
        TZrSize argumentIndex) {
    const SZrSemanticReferenceFact *callReference =
            ownership_move_find_call_reference(context, callOwner);
    const SZrSemanticSymbolRecord *symbol;
    SZrAstNodeArray *parameters;
    SZrParameter *variadic;

    if (callReference == ZR_NULL) {
        return ZR_NULL;
    }
    symbol = ownership_move_find_symbol(context, callReference->symbolId);
    if (symbol == ZR_NULL || symbol->astNode == ZR_NULL) {
        return ZR_NULL;
    }
    parameters = ownership_move_callable_parameters(symbol->astNode, &variadic);
    return ownership_move_parameter_at(parameters, variadic, call, argumentIndex);
}

static TZrBool ownership_move_argument_is_by_value(const SZrSemanticContext *context,
                                                    SZrAstNode *callOwner,
                                                    const SZrFunctionCall *call,
                                                    TZrSize argumentIndex) {
    const SZrParameter *parameter = ownership_move_argument_parameter(
            context,
            callOwner,
            call,
            argumentIndex);
    return parameter != ZR_NULL && parameter->passingMode == ZR_PARAMETER_PASSING_MODE_VALUE;
}

static TZrBool ownership_move_argument_requires_weak_upgrade(
        const SZrSemanticContext *context,
        SZrAstNode *callOwner,
        const SZrFunctionCall *call,
        TZrSize argumentIndex) {
    const SZrParameter *parameter = ownership_move_argument_parameter(
            context,
            callOwner,
            call,
            argumentIndex);
    return parameter != ZR_NULL &&
           parameter->typeInfo != ZR_NULL &&
           parameter->typeInfo->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED;
}

static TZrBool ownership_move_expression_contains_call(
        const SZrSemanticContext *context,
        SZrAstNode *expression,
        const SZrSemanticReferenceFact *fact);

static TZrBool ownership_move_primary_consumes_receiver(
        SZrAstNode *primaryNode,
        const SZrSemanticReferenceFact *fact) {
    SZrAstNodeArray *members;

    if (primaryNode == ZR_NULL || fact == ZR_NULL ||
        primaryNode->type != ZR_AST_PRIMARY_EXPRESSION ||
        !ownership_move_expression_is_direct_reference(
                primaryNode->data.primaryExpression.property, fact)) {
        return ZR_FALSE;
    }
    members = primaryNode->data.primaryExpression.members;
    for (TZrSize index = 0u; members != ZR_NULL && index + 1u < members->count; index++) {
        SZrAstNode *member = members->nodes[index];
        SZrAstNode *call = members->nodes[index + 1u];
        EZrOwnershipBuiltinKind builtinKind = ZR_OWNERSHIP_BUILTIN_KIND_NONE;

        if (member == ZR_NULL || member->type != ZR_AST_MEMBER_EXPRESSION ||
            member->data.memberExpression.computed ||
            member->data.memberExpression.property == ZR_NULL ||
            member->data.memberExpression.property->type != ZR_AST_IDENTIFIER_LITERAL ||
            call == ZR_NULL || call->type != ZR_AST_FUNCTION_CALL ||
            !ZrParser_OwnershipMemberNameToBuiltinKind(
                    member->data.memberExpression.property->data.identifier.name,
                    &builtinKind)) {
            continue;
        }
        if (builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_INTO_GC) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool ownership_move_primary_contains_call(const SZrSemanticContext *context,
                                                     SZrAstNode *primaryNode,
                                                     const SZrSemanticReferenceFact *fact) {
    SZrAstNodeArray *members;
    TZrSize memberIndex;

    if (primaryNode == ZR_NULL || primaryNode->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }
    if (ownership_move_primary_consumes_receiver(primaryNode, fact)) {
        return ZR_TRUE;
    }
    members = primaryNode->data.primaryExpression.members;
    for (memberIndex = 0; members != ZR_NULL && memberIndex < members->count; memberIndex++) {
        SZrAstNode *member = members->nodes[memberIndex];
        SZrFunctionCall *call;
        TZrSize argumentIndex;

        if (member == ZR_NULL || member->type != ZR_AST_FUNCTION_CALL) {
            continue;
        }
        call = &member->data.functionCall;
        for (argumentIndex = 0; call->args != ZR_NULL && argumentIndex < call->args->count;
             argumentIndex++) {
            SZrAstNode *argument = call->args->nodes[argumentIndex];
            if (ownership_move_expression_is_direct_reference(argument, fact) &&
                ownership_move_argument_is_by_value(context,
                                                    primaryNode,
                                                    call,
                                                    argumentIndex)) {
                return ZR_TRUE;
            }
            if (ownership_move_expression_contains_call(context, argument, fact)) {
                return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
}

static TZrBool ownership_weak_expression_requires_upgrade(
        const SZrSemanticContext *context,
        SZrAstNode *expression,
        const SZrSemanticReferenceFact *fact);

static TZrBool ownership_weak_primary_requires_upgrade(
        const SZrSemanticContext *context,
        SZrAstNode *primaryNode,
        const SZrSemanticReferenceFact *fact) {
    SZrAstNodeArray *members;
    TZrSize memberIndex;

    if (primaryNode == ZR_NULL || primaryNode->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }
    members = primaryNode->data.primaryExpression.members;
    for (memberIndex = 0; members != ZR_NULL && memberIndex < members->count; memberIndex++) {
        SZrAstNode *member = members->nodes[memberIndex];
        SZrFunctionCall *call;
        TZrSize argumentIndex;

        if (member != ZR_NULL &&
            member->type == ZR_AST_FUNCTION_CALL &&
            ownership_move_node_contains_fact(
                    primaryNode->data.primaryExpression.property,
                    fact)) {
            return ZR_TRUE;
        }
        if (member == ZR_NULL || member->type != ZR_AST_FUNCTION_CALL) {
            continue;
        }
        call = &member->data.functionCall;
        for (argumentIndex = 0; call->args != ZR_NULL && argumentIndex < call->args->count;
             argumentIndex++) {
            SZrAstNode *argument = call->args->nodes[argumentIndex];
            if (ownership_move_expression_is_direct_reference(argument, fact) &&
                ownership_move_argument_requires_weak_upgrade(context,
                                                              primaryNode,
                                                              call,
                                                              argumentIndex)) {
                return ZR_TRUE;
            }
            if (ownership_weak_expression_requires_upgrade(context, argument, fact)) {
                return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
}

static TZrBool ownership_weak_expression_requires_upgrade(
        const SZrSemanticContext *context,
        SZrAstNode *expression,
        const SZrSemanticReferenceFact *fact) {
    if (expression == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (expression->type) {
        case ZR_AST_PRIMARY_EXPRESSION:
            return ownership_weak_primary_requires_upgrade(context, expression, fact);
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return ownership_weak_expression_requires_upgrade(
                           context, expression->data.assignmentExpression.left, fact) ||
                   ownership_weak_expression_requires_upgrade(
                           context, expression->data.assignmentExpression.right, fact);
        case ZR_AST_BINARY_EXPRESSION:
            return ownership_weak_expression_requires_upgrade(
                           context, expression->data.binaryExpression.left, fact) ||
                   ownership_weak_expression_requires_upgrade(
                           context, expression->data.binaryExpression.right, fact);
        case ZR_AST_LOGICAL_EXPRESSION:
            return ownership_weak_expression_requires_upgrade(
                           context, expression->data.logicalExpression.left, fact) ||
                   ownership_weak_expression_requires_upgrade(
                           context, expression->data.logicalExpression.right, fact);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return ownership_weak_expression_requires_upgrade(
                           context, expression->data.conditionalExpression.test, fact) ||
                   ownership_weak_expression_requires_upgrade(
                           context, expression->data.conditionalExpression.consequent, fact) ||
                   ownership_weak_expression_requires_upgrade(
                           context, expression->data.conditionalExpression.alternate, fact);
        case ZR_AST_UNARY_EXPRESSION:
            return ownership_weak_expression_requires_upgrade(
                    context, expression->data.unaryExpression.argument, fact);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return ownership_weak_expression_requires_upgrade(
                    context, expression->data.typeCastExpression.expression, fact);
        default:
            return ZR_FALSE;
    }
}

static TZrBool ownership_move_expression_contains_call(
        const SZrSemanticContext *context,
        SZrAstNode *expression,
        const SZrSemanticReferenceFact *fact) {
    if (expression == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (expression->type) {
        case ZR_AST_PRIMARY_EXPRESSION:
            return ownership_move_primary_contains_call(context, expression, fact);
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return ownership_move_expression_contains_call(
                           context, expression->data.assignmentExpression.left, fact) ||
                   ownership_move_expression_contains_call(
                           context, expression->data.assignmentExpression.right, fact);
        case ZR_AST_BINARY_EXPRESSION:
            return ownership_move_expression_contains_call(
                           context, expression->data.binaryExpression.left, fact) ||
                   ownership_move_expression_contains_call(
                           context, expression->data.binaryExpression.right, fact);
        case ZR_AST_LOGICAL_EXPRESSION:
            return ownership_move_expression_contains_call(
                           context, expression->data.logicalExpression.left, fact) ||
                   ownership_move_expression_contains_call(
                           context, expression->data.logicalExpression.right, fact);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return ownership_move_expression_contains_call(
                           context, expression->data.conditionalExpression.test, fact) ||
                   ownership_move_expression_contains_call(
                           context, expression->data.conditionalExpression.consequent, fact) ||
                   ownership_move_expression_contains_call(
                           context, expression->data.conditionalExpression.alternate, fact);
        case ZR_AST_UNARY_EXPRESSION:
            return ownership_move_expression_contains_call(
                    context, expression->data.unaryExpression.argument, fact);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return ownership_move_expression_contains_call(
                    context, expression->data.typeCastExpression.expression, fact);
        default:
            return ZR_FALSE;
    }
}

static TZrBool ownership_move_assignment_targets_source(const SZrSemanticContext *context,
                                                         SZrAstNode *target,
                                                         TZrSymbolId sourceSymbolId) {
    TZrSize index;

    if (context == ZR_NULL || target == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *candidate =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts,
                        index);
        if (candidate != ZR_NULL &&
            candidate->kind == ZR_SEMANTIC_REFERENCE_WRITE &&
            candidate->symbolId == sourceSymbolId &&
            ownership_move_node_contains_fact(target, candidate)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static SZrAstNode *ownership_move_statement_expression(SZrAstNode *statement) {
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

TZrBool ZrParser_DataflowOwnership_StatementMovesRead(
        const SZrSemanticContext *context,
        SZrAstNode *statement,
        const SZrSemanticReferenceFact *fact) {
    SZrAstNode *expression;

    if (context == ZR_NULL || statement == ZR_NULL || fact == ZR_NULL) {
        return ZR_FALSE;
    }
    if (statement->type == ZR_AST_VARIABLE_DECLARATION &&
        ownership_move_expression_is_direct_reference(
                statement->data.variableDeclaration.value,
                fact)) {
        return ZR_TRUE;
    }

    expression = ownership_move_statement_expression(statement);
    if (expression != ZR_NULL &&
        expression->type == ZR_AST_ASSIGNMENT_EXPRESSION &&
        expression->data.assignmentExpression.op.op != ZR_NULL &&
        strcmp(expression->data.assignmentExpression.op.op, "=") == 0 &&
        ownership_move_expression_is_direct_reference(
                expression->data.assignmentExpression.right,
                fact) &&
        !ownership_move_assignment_targets_source(
                context,
                expression->data.assignmentExpression.left,
                fact->symbolId)) {
        return ZR_TRUE;
    }
    return ownership_move_expression_contains_call(context, expression, fact);
}

TZrBool ZrParser_DataflowOwnership_StatementWeakReadRequiresUpgrade(
        const SZrSemanticContext *context,
        SZrAstNode *statement,
        const SZrSemanticReferenceFact *fact) {
    return ownership_weak_expression_requires_upgrade(
            context,
            ownership_move_statement_expression(statement),
            fact);
}
