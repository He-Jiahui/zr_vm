#include "compiler_internal.h"

typedef struct ZrTaskEffectBinding {
    SZrString *name;
    TZrInt32 scopeDepth;
    TZrBool isBorrowed;
} ZrTaskEffectBinding;

typedef struct ZrTaskEffectContext {
    SZrCompilerState *cs;
    SZrArray bindings;
    TZrInt32 scopeDepth;
    TZrBool asyncAllowed;
    TZrBool awaitSeen;
} ZrTaskEffectContext;

static TZrBool task_effects_string_equals(SZrString *value, const TZrChar *literal) {
    const TZrChar *nativeValue;

    if (value == ZR_NULL || literal == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeValue = ZrCore_String_GetNativeString(value);
    return nativeValue != ZR_NULL && strcmp(nativeValue, literal) == 0;
}

static TZrBool task_effects_identifier_equals(const SZrIdentifier *identifier, const TZrChar *literal) {
    return identifier != ZR_NULL && task_effects_string_equals(identifier->name, literal);
}

static TZrBool task_effects_type_is_borrowed(const SZrType *typeInfo) {
    return typeInfo != ZR_NULL && typeInfo->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED;
}

static TZrBool task_effects_context_init(ZrTaskEffectContext *context,
                                         SZrCompilerState *cs,
                                         TZrBool asyncAllowed,
                                         const ZrTaskEffectContext *parent) {
    TZrSize capacity = 8;

    if (context == ZR_NULL || cs == ZR_NULL || cs->state == ZR_NULL) {
        return ZR_FALSE;
    }

    if (parent != ZR_NULL && parent->bindings.length > capacity) {
        capacity = parent->bindings.length + 4;
    }

    context->cs = cs;
    context->scopeDepth = 0;
    context->asyncAllowed = asyncAllowed;
    context->awaitSeen = ZR_FALSE;
    ZrCore_Array_Init(cs->state, &context->bindings, sizeof(ZrTaskEffectBinding), capacity);

    if (parent != ZR_NULL) {
        TZrSize index;
        for (index = 0; index < parent->bindings.length; index++) {
            ZrTaskEffectBinding *parentBinding =
                    (ZrTaskEffectBinding *)ZrCore_Array_Get((SZrArray *)&parent->bindings, index);
            ZrTaskEffectBinding inheritedBinding;

            if (parentBinding == ZR_NULL) {
                continue;
            }

            inheritedBinding = *parentBinding;
            inheritedBinding.scopeDepth = 0;
            ZrCore_Array_Push(cs->state, &context->bindings, &inheritedBinding);
        }
    }

    return ZR_TRUE;
}

static void task_effects_context_free(ZrTaskEffectContext *context) {
    if (context == ZR_NULL || context->cs == ZR_NULL || context->cs->state == ZR_NULL) {
        return;
    }

    ZrCore_Array_Free(context->cs->state, &context->bindings);
}

static void task_effects_enter_scope(ZrTaskEffectContext *context) {
    if (context != ZR_NULL) {
        context->scopeDepth++;
    }
}

static void task_effects_leave_scope(ZrTaskEffectContext *context) {
    if (context == ZR_NULL) {
        return;
    }

    while (context->bindings.length > 0) {
        ZrTaskEffectBinding *binding =
                (ZrTaskEffectBinding *)ZrCore_Array_Get(&context->bindings, context->bindings.length - 1);
        if (binding == ZR_NULL || binding->scopeDepth < context->scopeDepth) {
            break;
        }
        ZrCore_Array_Pop(&context->bindings);
    }

    if (context->scopeDepth > 0) {
        context->scopeDepth--;
    }
}

static void task_effects_push_binding(ZrTaskEffectContext *context, SZrString *name, TZrBool isBorrowed) {
    ZrTaskEffectBinding binding;

    if (context == ZR_NULL || context->cs == ZR_NULL || context->cs->state == ZR_NULL || name == ZR_NULL) {
        return;
    }

    binding.name = name;
    binding.scopeDepth = context->scopeDepth;
    binding.isBorrowed = isBorrowed;
    ZrCore_Array_Push(context->cs->state, &context->bindings, &binding);
}

static const ZrTaskEffectBinding *task_effects_find_binding(const ZrTaskEffectContext *context, SZrString *name) {
    TZrSize index;

    if (context == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = context->bindings.length; index > 0; index--) {
        ZrTaskEffectBinding *binding =
                (ZrTaskEffectBinding *)ZrCore_Array_Get((SZrArray *)&context->bindings, index - 1);
        if (binding != ZR_NULL && binding->name != ZR_NULL && ZrCore_String_Compare(context->cs->state, binding->name, name)) {
            return binding;
        }
    }

    return ZR_NULL;
}

static void task_effects_report_borrow_after_await(ZrTaskEffectContext *context,
                                                   SZrString *name,
                                                   SZrFileRange location) {
    TZrNativeString nativeName;
    TZrChar message[256];

    if (context == ZR_NULL || context->cs == ZR_NULL) {
        return;
    }

    nativeName = name != ZR_NULL ? ZrCore_String_GetNativeStringShort(name) : ZR_NULL;
    if (nativeName == ZR_NULL) {
        nativeName = "<borrowed>";
    }

    snprintf(message,
             sizeof(message),
             "Borrowed binding '%s' cannot be used after an await boundary",
             nativeName);
    ZrParser_Compiler_Error(context->cs, message, location);
}

static void task_effects_validate_node(ZrTaskEffectContext *context, SZrAstNode *node);

static void task_effects_validate_node_array(ZrTaskEffectContext *context, SZrAstNodeArray *nodes) {
    TZrSize index;

    if (context == ZR_NULL || nodes == ZR_NULL) {
        return;
    }

    for (index = 0; index < nodes->count && !context->cs->hasError; index++) {
        task_effects_validate_node(context, nodes->nodes[index]);
    }
}

static void task_effects_register_pattern_binding(ZrTaskEffectContext *context,
                                                  SZrAstNode *pattern,
                                                  TZrBool isBorrowed) {
    TZrSize index;

    if (context == ZR_NULL || pattern == ZR_NULL) {
        return;
    }

    switch (pattern->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            task_effects_push_binding(context, pattern->data.identifier.name, isBorrowed);
            break;
        case ZR_AST_DESTRUCTURING_OBJECT:
            if (pattern->data.destructuringObject.keys != ZR_NULL) {
                for (index = 0; index < pattern->data.destructuringObject.keys->count; index++) {
                    task_effects_register_pattern_binding(context,
                                                          pattern->data.destructuringObject.keys->nodes[index],
                                                          isBorrowed);
                }
            }
            break;
        case ZR_AST_DESTRUCTURING_ARRAY:
            if (pattern->data.destructuringArray.keys != ZR_NULL) {
                for (index = 0; index < pattern->data.destructuringArray.keys->count; index++) {
                    task_effects_register_pattern_binding(context,
                                                          pattern->data.destructuringArray.keys->nodes[index],
                                                          isBorrowed);
                }
            }
            break;
        default:
            break;
    }
}

static void task_effects_register_parameter_node(ZrTaskEffectContext *context, SZrAstNode *parameterNode) {
    if (parameterNode == ZR_NULL || parameterNode->type != ZR_AST_PARAMETER) {
        return;
    }

    task_effects_push_binding(context,
                              parameterNode->data.parameter.name != ZR_NULL ? parameterNode->data.parameter.name->name
                                                                            : ZR_NULL,
                              task_effects_type_is_borrowed(parameterNode->data.parameter.typeInfo));
}

static void task_effects_register_parameter_list(ZrTaskEffectContext *context, SZrAstNodeArray *params) {
    TZrSize index;

    if (context == ZR_NULL || params == ZR_NULL) {
        return;
    }

    for (index = 0; index < params->count; index++) {
        task_effects_register_parameter_node(context, params->nodes[index]);
    }
}

static void task_effects_register_vararg_parameter(ZrTaskEffectContext *context, SZrParameter *parameter) {
    if (context == ZR_NULL || parameter == ZR_NULL || parameter->name == ZR_NULL) {
        return;
    }

    task_effects_push_binding(context, parameter->name->name, task_effects_type_is_borrowed(parameter->typeInfo));
}

static TZrBool task_effects_is_zr_task_import(SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_IMPORT_EXPRESSION || node->data.importExpression.modulePath == ZR_NULL ||
        node->data.importExpression.modulePath->type != ZR_AST_STRING_LITERAL) {
        return ZR_FALSE;
    }

    return task_effects_string_equals(node->data.importExpression.modulePath->data.stringLiteral.value, "zr.task");
}

static TZrBool task_effects_primary_is_await_call(SZrAstNode *node, TZrSize *callMemberIndex) {
    SZrPrimaryExpression *primary;
    SZrAstNode *memberNode;

    if (callMemberIndex != ZR_NULL) {
        *callMemberIndex = 0;
    }

    if (node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION || !task_effects_is_zr_task_import(node->data.primaryExpression.property)) {
        return ZR_FALSE;
    }

    primary = &node->data.primaryExpression;
    if (primary->members == ZR_NULL || primary->members->count < 2) {
        return ZR_FALSE;
    }

    memberNode = primary->members->nodes[0];
    if (memberNode == ZR_NULL || memberNode->type != ZR_AST_MEMBER_EXPRESSION || memberNode->data.memberExpression.computed ||
        memberNode->data.memberExpression.property == ZR_NULL ||
        memberNode->data.memberExpression.property->type != ZR_AST_IDENTIFIER_LITERAL ||
        !task_effects_identifier_equals(&memberNode->data.memberExpression.property->data.identifier, "__awaitTask")) {
        return ZR_FALSE;
    }

    memberNode = primary->members->nodes[1];
    if (memberNode == ZR_NULL || memberNode->type != ZR_AST_FUNCTION_CALL) {
        return ZR_FALSE;
    }

    if (callMemberIndex != ZR_NULL) {
        *callMemberIndex = 1;
    }
    return ZR_TRUE;
}

static void task_effects_validate_member_node(ZrTaskEffectContext *context, SZrAstNode *node) {
    if (context == ZR_NULL || node == ZR_NULL || context->cs->hasError) {
        return;
    }

    if (node->type == ZR_AST_MEMBER_EXPRESSION) {
        if (node->data.memberExpression.computed) {
            task_effects_validate_node(context, node->data.memberExpression.property);
        }
        return;
    }

    if (node->type == ZR_AST_FUNCTION_CALL) {
        task_effects_validate_node_array(context, node->data.functionCall.args);
        return;
    }

    task_effects_validate_node(context, node);
}

static void task_effects_validate_primary_expression(ZrTaskEffectContext *context, SZrAstNode *node) {
    SZrPrimaryExpression *primary;
    TZrSize memberIndex;
    TZrSize awaitCallIndex = 0;

    if (context == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION || context->cs->hasError) {
        return;
    }

    primary = &node->data.primaryExpression;
    if (task_effects_primary_is_await_call(node, &awaitCallIndex)) {
        SZrAstNode *callNode = primary->members->nodes[awaitCallIndex];
        task_effects_validate_node_array(context, callNode->data.functionCall.args);
        if (context->cs->hasError) {
            return;
        }
        if (!context->asyncAllowed) {
            ZrParser_Compiler_Error(context->cs,
                                    "%await is only allowed inside %async bodies or scheduler-managed top-level coroutines",
                                    node->location);
            return;
        }
        context->awaitSeen = ZR_TRUE;
        for (memberIndex = awaitCallIndex + 1; memberIndex < primary->members->count && !context->cs->hasError;
             memberIndex++) {
            task_effects_validate_member_node(context, primary->members->nodes[memberIndex]);
        }
        return;
    }

    task_effects_validate_node(context, primary->property);
    for (memberIndex = 0; memberIndex < primary->members->count && !context->cs->hasError; memberIndex++) {
        task_effects_validate_member_node(context, primary->members->nodes[memberIndex]);
    }
}

static void task_effects_validate_function_like(ZrTaskEffectContext *parentContext,
                                                TZrBool asyncAllowed,
                                                SZrAstNodeArray *params,
                                                SZrParameter *args,
                                                SZrAstNode *body) {
    ZrTaskEffectContext childContext;

    if (parentContext == ZR_NULL || body == ZR_NULL || parentContext->cs == ZR_NULL) {
        return;
    }

    if (!task_effects_context_init(&childContext, parentContext->cs, asyncAllowed, parentContext)) {
        ZrParser_Compiler_Error(parentContext->cs, "Failed to initialize task effect validation context", body->location);
        return;
    }

    task_effects_register_parameter_list(&childContext, params);
    task_effects_register_vararg_parameter(&childContext, args);
    task_effects_validate_node(&childContext, body);
    task_effects_context_free(&childContext);
}

static void task_effects_validate_node(ZrTaskEffectContext *context, SZrAstNode *node) {
    if (context == ZR_NULL || node == ZR_NULL || context->cs == ZR_NULL || context->cs->hasError) {
        return;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            task_effects_validate_node_array(context, node->data.script.statements);
            break;
        case ZR_AST_BLOCK:
            task_effects_enter_scope(context);
            task_effects_validate_node_array(context, node->data.block.body);
            task_effects_leave_scope(context);
            break;
        case ZR_AST_FUNCTION_DECLARATION:
            if (node->data.functionDeclaration.name != ZR_NULL) {
                task_effects_push_binding(context, node->data.functionDeclaration.name->name, ZR_FALSE);
            }
            task_effects_validate_function_like(context,
                                                node->data.functionDeclaration.isAsync,
                                                node->data.functionDeclaration.params,
                                                node->data.functionDeclaration.args,
                                                node->data.functionDeclaration.body);
            break;
        case ZR_AST_TEST_DECLARATION:
            task_effects_validate_function_like(context, ZR_TRUE, node->data.testDeclaration.params,
                                                node->data.testDeclaration.args, node->data.testDeclaration.body);
            break;
        case ZR_AST_LAMBDA_EXPRESSION:
            task_effects_validate_function_like(context, node->data.lambdaExpression.isAsync,
                                                node->data.lambdaExpression.params, node->data.lambdaExpression.args,
                                                node->data.lambdaExpression.block);
            break;
        case ZR_AST_VARIABLE_DECLARATION:
            task_effects_validate_node(context, node->data.variableDeclaration.value);
            task_effects_register_pattern_binding(context,
                                                 node->data.variableDeclaration.pattern,
                                                 task_effects_type_is_borrowed(node->data.variableDeclaration.typeInfo));
            break;
        case ZR_AST_PRIMARY_EXPRESSION:
            task_effects_validate_primary_expression(context, node);
            break;
        case ZR_AST_IDENTIFIER_LITERAL: {
            const ZrTaskEffectBinding *binding = ZR_NULL;

            if (!context->awaitSeen) {
                break;
            }

            binding = task_effects_find_binding(context, node->data.identifier.name);
            if (binding != ZR_NULL && binding->isBorrowed) {
                task_effects_report_borrow_after_await(context, node->data.identifier.name, node->location);
            }
            break;
        }
        case ZR_AST_FUNCTION_CALL:
            task_effects_validate_node_array(context, node->data.functionCall.args);
            break;
        case ZR_AST_MEMBER_EXPRESSION:
            if (node->data.memberExpression.computed) {
                task_effects_validate_node(context, node->data.memberExpression.property);
            }
            break;
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            task_effects_validate_node(context, node->data.assignmentExpression.left);
            task_effects_validate_node(context, node->data.assignmentExpression.right);
            break;
        case ZR_AST_BINARY_EXPRESSION:
            task_effects_validate_node(context, node->data.binaryExpression.left);
            task_effects_validate_node(context, node->data.binaryExpression.right);
            break;
        case ZR_AST_LOGICAL_EXPRESSION:
            task_effects_validate_node(context, node->data.logicalExpression.left);
            task_effects_validate_node(context, node->data.logicalExpression.right);
            break;
        case ZR_AST_CONDITIONAL_EXPRESSION:
            task_effects_validate_node(context, node->data.conditionalExpression.test);
            task_effects_validate_node(context, node->data.conditionalExpression.consequent);
            task_effects_validate_node(context, node->data.conditionalExpression.alternate);
            break;
        case ZR_AST_UNARY_EXPRESSION:
            task_effects_validate_node(context, node->data.unaryExpression.argument);
            break;
        case ZR_AST_TYPE_CAST_EXPRESSION:
            task_effects_validate_node(context, node->data.typeCastExpression.expression);
            break;
        case ZR_AST_ARRAY_LITERAL:
            task_effects_validate_node_array(context, node->data.arrayLiteral.elements);
            break;
        case ZR_AST_OBJECT_LITERAL:
            task_effects_validate_node_array(context, node->data.objectLiteral.properties);
            break;
        case ZR_AST_KEY_VALUE_PAIR:
            if (node->data.keyValuePair.key != ZR_NULL && node->data.keyValuePair.key->type != ZR_AST_IDENTIFIER_LITERAL &&
                node->data.keyValuePair.key->type != ZR_AST_STRING_LITERAL) {
                task_effects_validate_node(context, node->data.keyValuePair.key);
            }
            task_effects_validate_node(context, node->data.keyValuePair.value);
            break;
        case ZR_AST_RETURN_STATEMENT:
            task_effects_validate_node(context, node->data.returnStatement.expr);
            break;
        case ZR_AST_EXPRESSION_STATEMENT:
            task_effects_validate_node(context, node->data.expressionStatement.expr);
            break;
        case ZR_AST_USING_STATEMENT:
            task_effects_validate_node(context, node->data.usingStatement.resource);
            task_effects_validate_node(context, node->data.usingStatement.body);
            break;
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            task_effects_validate_node(context, node->data.breakContinueStatement.expr);
            break;
        case ZR_AST_THROW_STATEMENT:
            task_effects_validate_node(context, node->data.throwStatement.expr);
            break;
        case ZR_AST_OUT_STATEMENT:
            task_effects_validate_node(context, node->data.outStatement.expr);
            break;
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            task_effects_validate_node(context, node->data.tryCatchFinallyStatement.block);
            task_effects_validate_node_array(context, node->data.tryCatchFinallyStatement.catchClauses);
            task_effects_validate_node(context, node->data.tryCatchFinallyStatement.finallyBlock);
            break;
        case ZR_AST_CATCH_CLAUSE:
            task_effects_enter_scope(context);
            task_effects_validate_node_array(context, node->data.catchClause.pattern);
            task_effects_validate_node(context, node->data.catchClause.block);
            task_effects_leave_scope(context);
            break;
        case ZR_AST_IF_EXPRESSION:
            task_effects_validate_node(context, node->data.ifExpression.condition);
            task_effects_validate_node(context, node->data.ifExpression.thenExpr);
            task_effects_validate_node(context, node->data.ifExpression.elseExpr);
            break;
        case ZR_AST_SWITCH_EXPRESSION:
            task_effects_validate_node(context, node->data.switchExpression.expr);
            task_effects_validate_node_array(context, node->data.switchExpression.cases);
            task_effects_validate_node(context, node->data.switchExpression.defaultCase);
            break;
        case ZR_AST_SWITCH_CASE:
            task_effects_validate_node(context, node->data.switchCase.value);
            task_effects_validate_node(context, node->data.switchCase.block);
            break;
        case ZR_AST_SWITCH_DEFAULT:
            task_effects_validate_node(context, node->data.switchDefault.block);
            break;
        case ZR_AST_WHILE_LOOP:
            task_effects_validate_node(context, node->data.whileLoop.cond);
            task_effects_validate_node(context, node->data.whileLoop.block);
            break;
        case ZR_AST_FOR_LOOP:
            task_effects_validate_node(context, node->data.forLoop.init);
            task_effects_validate_node(context, node->data.forLoop.cond);
            task_effects_validate_node(context, node->data.forLoop.step);
            task_effects_validate_node(context, node->data.forLoop.block);
            break;
        case ZR_AST_FOREACH_LOOP:
            task_effects_validate_node(context, node->data.foreachLoop.expr);
            task_effects_enter_scope(context);
            task_effects_register_pattern_binding(context,
                                                 node->data.foreachLoop.pattern,
                                                 task_effects_type_is_borrowed(node->data.foreachLoop.typeInfo));
            task_effects_validate_node(context, node->data.foreachLoop.block);
            task_effects_leave_scope(context);
            break;
        default:
            break;
    }
}

TZrBool compiler_validate_task_effects(SZrCompilerState *cs, SZrAstNode *node) {
    ZrTaskEffectContext rootContext;
    TZrBool succeeded;

    if (cs == ZR_NULL || node == ZR_NULL) {
        return ZR_FALSE;
    }

    succeeded = task_effects_context_init(&rootContext, cs, ZR_TRUE, ZR_NULL);
    if (!succeeded) {
        ZrParser_Compiler_Error(cs, "Failed to initialize task effect validation context", node->location);
        return ZR_FALSE;
    }

    task_effects_validate_node(&rootContext, node);
    task_effects_context_free(&rootContext);
    return !cs->hasError;
}
