#include "compiler_using_plugin_guard_escape_internal.h"

#include "compile_expression_internal.h"
#include "type_inference_internal.h"

static TZrBool plugin_guard_member_type_is_callable(EZrAstNodeType memberType) {
    return (TZrBool)(memberType == ZR_AST_STRUCT_METHOD ||
                     memberType == ZR_AST_CLASS_METHOD ||
                     memberType == ZR_AST_STRUCT_META_FUNCTION ||
                      memberType == ZR_AST_CLASS_META_FUNCTION);
}

static TZrBool plugin_guard_name_is_shadowed(SZrPluginGuardEscapeScan *scan, SZrString *name) {
    return scan != ZR_NULL &&
           name != ZR_NULL &&
           plugin_guard_name_array_contains(&scan->shadowNames, name);
}

static TZrBool plugin_guard_primary_member_reference_is_scoped_value(SZrPluginGuardEscapeScan *scan,
                                                                     SZrAstNode *node) {
    SZrPrimaryExpression *primary;
    SZrAstNode *root;
    SZrAstNode *memberNode;
    SZrMemberExpression *memberExpr;
    SZrString *moduleName = ZR_NULL;
    SZrString *memberName;
    SZrTypeMemberInfo *memberInfo;

    if (scan == ZR_NULL || scan->cs == ZR_NULL || node == ZR_NULL ||
        node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }

    primary = &node->data.primaryExpression;
    if (primary->members == ZR_NULL || primary->members->count != 1) {
        return ZR_FALSE;
    }

    root = primary->property;
    if (root == ZR_NULL ||
        root->type != ZR_AST_IDENTIFIER_LITERAL ||
        root->data.identifier.name == ZR_NULL ||
        plugin_guard_name_is_shadowed(scan, root->data.identifier.name) ||
        !plugin_guard_name_array_contains(&scan->pluginNames, root->data.identifier.name)) {
        return ZR_FALSE;
    }

    memberNode = primary->members->nodes[0];
    if (memberNode == ZR_NULL || memberNode->type != ZR_AST_MEMBER_EXPRESSION) {
        return ZR_FALSE;
    }

    memberExpr = &memberNode->data.memberExpression;
    if (memberExpr->computed ||
        memberExpr->property == ZR_NULL ||
        memberExpr->property->type != ZR_AST_IDENTIFIER_LITERAL ||
        memberExpr->property->data.identifier.name == ZR_NULL) {
        return ZR_FALSE;
    }

    moduleName = scan->moduleName;
    if (moduleName == ZR_NULL && scan->cs->typeEnv != ZR_NULL) {
        SZrInferredType rootType;
        ZrParser_InferredType_Init(scan->cs->state, &rootType, ZR_VALUE_TYPE_OBJECT);
        if (ZrParser_TypeEnvironment_LookupVariable(scan->cs->state,
                                                    scan->cs->typeEnv,
                                                    root->data.identifier.name,
                                                    &rootType) &&
            rootType.typeName != ZR_NULL) {
            moduleName = rootType.typeName;
        }
        ZrParser_InferredType_Free(scan->cs->state, &rootType);
    }
    if (moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    memberName = memberExpr->property->data.identifier.name;
    memberInfo = find_compiler_type_member(scan->cs, moduleName, memberName);
    if (memberInfo == ZR_NULL) {
        ensure_import_module_compile_info(scan->cs, moduleName);
        memberInfo = find_compiler_type_member(scan->cs, moduleName, memberName);
    }

    return (TZrBool)(memberInfo != ZR_NULL &&
                     plugin_guard_member_type_is_callable(memberInfo->memberType));
}

static TZrBool plugin_guard_node_array_contains_scoped_value(SZrPluginGuardEscapeScan *scan,
                                                             SZrAstNodeArray *nodes) {
    if (scan == ZR_NULL || nodes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        if (plugin_guard_expression_contains_scoped_value(scan, nodes->nodes[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool plugin_guard_node_array_contains_lambda_capture(SZrPluginGuardEscapeScan *scan,
                                                               SZrAstNodeArray *nodes) {
    if (scan == ZR_NULL || nodes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        if (plugin_guard_expression_contains_lambda_capture(scan, nodes->nodes[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool plugin_guard_node_array_contains_argument_escape(SZrPluginGuardEscapeScan *scan,
                                                                SZrAstNodeArray *nodes) {
    if (scan == ZR_NULL || nodes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        if (plugin_guard_expression_passes_scoped_value_as_argument(scan, nodes->nodes[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool plugin_guard_node_array_contains_container_escape(SZrPluginGuardEscapeScan *scan,
                                                                 SZrAstNodeArray *nodes) {
    if (scan == ZR_NULL || nodes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        if (plugin_guard_expression_contains_container_escape(scan, nodes->nodes[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void plugin_guard_push_shadow_name(SZrPluginGuardEscapeScan *scan, SZrString *name) {
    if (scan == ZR_NULL || name == ZR_NULL) {
        return;
    }

    plugin_guard_push_name_unique(scan, &scan->shadowNames, name);
}

static void plugin_guard_push_parameter_shadow(SZrPluginGuardEscapeScan *scan, SZrParameter *parameter) {
    if (parameter == ZR_NULL || parameter->name == ZR_NULL) {
        return;
    }

    plugin_guard_push_shadow_name(scan, parameter->name->name);
}

static void plugin_guard_push_parameter_array_shadow(SZrPluginGuardEscapeScan *scan, SZrAstNodeArray *params) {
    if (scan == ZR_NULL || params == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < params->count; index++) {
        SZrAstNode *paramNode = params->nodes[index];
        if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
            plugin_guard_push_parameter_shadow(scan, &paramNode->data.parameter);
        }
    }
}

static void plugin_guard_push_variable_shadow(SZrPluginGuardEscapeScan *scan, SZrAstNode *pattern) {
    TZrSize index;

    if (scan == ZR_NULL || pattern == ZR_NULL) {
        return;
    }

    switch (pattern->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            plugin_guard_push_shadow_name(scan, pattern->data.identifier.name);
            break;
        case ZR_AST_KEY_VALUE_PAIR:
            if (!pattern->data.keyValuePair.keyIsComputed) {
                plugin_guard_push_variable_shadow(scan, pattern->data.keyValuePair.key);
            }
            break;
        case ZR_AST_DESTRUCTURING_OBJECT:
            if (pattern->data.destructuringObject.keys != ZR_NULL) {
                for (index = 0; index < pattern->data.destructuringObject.keys->count; index++) {
                    plugin_guard_push_variable_shadow(scan,
                                                      pattern->data.destructuringObject.keys->nodes[index]);
                }
            }
            break;
        case ZR_AST_DESTRUCTURING_ARRAY:
            if (pattern->data.destructuringArray.keys != ZR_NULL) {
                for (index = 0; index < pattern->data.destructuringArray.keys->count; index++) {
                    plugin_guard_push_variable_shadow(scan,
                                                      pattern->data.destructuringArray.keys->nodes[index]);
                }
            }
            break;
        default:
            break;
    }
}

static TZrBool plugin_guard_call_args_contain_scoped_value(SZrPluginGuardEscapeScan *scan, SZrAstNodeArray *args) {
    if (scan == ZR_NULL || args == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < args->count; index++) {
        SZrAstNode *arg = args->nodes[index];
        if (plugin_guard_expression_contains_scoped_value(scan, arg) ||
            plugin_guard_expression_passes_scoped_value_as_argument(scan, arg)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool plugin_guard_primary_passes_scoped_value_as_argument(SZrPluginGuardEscapeScan *scan,
                                                                    SZrPrimaryExpression *primary) {
    if (scan == ZR_NULL || primary == ZR_NULL) {
        return ZR_FALSE;
    }

    if (plugin_guard_expression_passes_scoped_value_as_argument(scan, primary->property)) {
        return ZR_TRUE;
    }

    if (primary->members == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < primary->members->count; index++) {
        SZrAstNode *member = primary->members->nodes[index];
        if (member == ZR_NULL) {
            continue;
        }
        if (member->type == ZR_AST_FUNCTION_CALL &&
            plugin_guard_call_args_contain_scoped_value(scan, member->data.functionCall.args)) {
            return ZR_TRUE;
        }
        if (plugin_guard_expression_passes_scoped_value_as_argument(scan, member)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool plugin_guard_block_contains_scoped_value(SZrPluginGuardEscapeScan *scan, SZrAstNode *node) {
    TZrSize localLength;
    TZrSize pluginLength;
    TZrSize shadowLength;

    if (scan == ZR_NULL || node == ZR_NULL) {
        return ZR_FALSE;
    }

    if (node->type != ZR_AST_BLOCK || node->data.block.body == ZR_NULL) {
        return plugin_guard_expression_contains_scoped_value(scan, node);
    }

    localLength = scan->localNames.length;
    pluginLength = scan->pluginNames.length;
    shadowLength = scan->shadowNames.length;
    for (TZrSize index = 0; index < node->data.block.body->count; index++) {
        SZrAstNode *child = node->data.block.body->nodes[index];
        if (plugin_guard_expression_contains_scoped_value(scan, child)) {
            scan->localNames.length = localLength;
            scan->pluginNames.length = pluginLength;
            scan->shadowNames.length = shadowLength;
            return ZR_TRUE;
        }
        if (child != ZR_NULL && child->type == ZR_AST_VARIABLE_DECLARATION) {
            plugin_guard_push_variable_shadow(scan, child->data.variableDeclaration.pattern);
        }
    }
    scan->localNames.length = localLength;
    scan->pluginNames.length = pluginLength;
    scan->shadowNames.length = shadowLength;
    return ZR_FALSE;
}

TZrBool plugin_guard_callable_body_contains_scoped_value(SZrPluginGuardEscapeScan *scan,
                                                        SZrAstNodeArray *params,
                                                        SZrParameter *args,
                                                        SZrAstNode *body) {
    TZrSize shadowLength;
    TZrBool containsScopedValue;

    if (scan == ZR_NULL) {
        return ZR_FALSE;
    }

    shadowLength = scan->shadowNames.length;
    plugin_guard_push_parameter_array_shadow(scan, params);
    plugin_guard_push_parameter_shadow(scan, args);
    containsScopedValue = plugin_guard_block_contains_scoped_value(scan, body);
    scan->shadowNames.length = shadowLength;
    return containsScopedValue;
}

TZrBool plugin_guard_expression_passes_scoped_value_as_argument(SZrPluginGuardEscapeScan *scan,
                                                                SZrAstNode *node) {
    if (scan == ZR_NULL || node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_FUNCTION_CALL:
            return plugin_guard_call_args_contain_scoped_value(scan, node->data.functionCall.args);
        case ZR_AST_CONSTRUCT_EXPRESSION:
            return plugin_guard_expression_passes_scoped_value_as_argument(scan,
                                                                           node->data.constructExpression.target) ||
                   plugin_guard_call_args_contain_scoped_value(scan, node->data.constructExpression.args);
        case ZR_AST_PRIMARY_EXPRESSION:
            return plugin_guard_primary_passes_scoped_value_as_argument(scan, &node->data.primaryExpression);
        case ZR_AST_MEMBER_EXPRESSION:
            return plugin_guard_expression_passes_scoped_value_as_argument(scan,
                                                                           node->data.memberExpression.property);
        case ZR_AST_EXPRESSION_STATEMENT:
            return plugin_guard_expression_passes_scoped_value_as_argument(scan,
                                                                           node->data.expressionStatement.expr);
        case ZR_AST_DECORATOR_EXPRESSION:
            return plugin_guard_expression_passes_scoped_value_as_argument(scan,
                                                                           node->data.decoratorExpression.expr);
        case ZR_AST_RETURN_STATEMENT:
            return plugin_guard_expression_passes_scoped_value_as_argument(scan, node->data.returnStatement.expr);
        case ZR_AST_VARIABLE_DECLARATION:
            return plugin_guard_expression_passes_scoped_value_as_argument(scan,
                                                                           node->data.variableDeclaration.value);
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return plugin_guard_expression_passes_scoped_value_as_argument(scan,
                                                                           node->data.assignmentExpression.left) ||
                   plugin_guard_expression_passes_scoped_value_as_argument(scan,
                                                                           node->data.assignmentExpression.right);
        case ZR_AST_BINARY_EXPRESSION:
            return plugin_guard_expression_passes_scoped_value_as_argument(scan, node->data.binaryExpression.left) ||
                   plugin_guard_expression_passes_scoped_value_as_argument(scan, node->data.binaryExpression.right);
        case ZR_AST_UNARY_EXPRESSION:
            return plugin_guard_expression_passes_scoped_value_as_argument(scan, node->data.unaryExpression.argument);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return plugin_guard_expression_passes_scoped_value_as_argument(scan,
                                                                           node->data.typeCastExpression.expression);
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            return plugin_guard_expression_passes_scoped_value_as_argument(scan,
                                                                           node->data.typeQueryExpression.operand);
        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            return plugin_guard_expression_passes_scoped_value_as_argument(
                    scan,
                    node->data.prototypeReferenceExpression.target);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return plugin_guard_expression_passes_scoped_value_as_argument(scan,
                                                                           node->data.conditionalExpression.test) ||
                   plugin_guard_expression_passes_scoped_value_as_argument(scan,
                                                                           node->data.conditionalExpression.consequent) ||
                   plugin_guard_expression_passes_scoped_value_as_argument(scan,
                                                                           node->data.conditionalExpression.alternate);
        case ZR_AST_LOGICAL_EXPRESSION:
            return plugin_guard_expression_passes_scoped_value_as_argument(scan, node->data.logicalExpression.left) ||
                   plugin_guard_expression_passes_scoped_value_as_argument(scan, node->data.logicalExpression.right);
        case ZR_AST_IF_EXPRESSION:
            return plugin_guard_expression_passes_scoped_value_as_argument(scan, node->data.ifExpression.condition) ||
                   plugin_guard_expression_passes_scoped_value_as_argument(scan, node->data.ifExpression.thenExpr) ||
                   plugin_guard_expression_passes_scoped_value_as_argument(scan, node->data.ifExpression.elseExpr);
        case ZR_AST_SWITCH_EXPRESSION:
            return plugin_guard_expression_passes_scoped_value_as_argument(scan, node->data.switchExpression.expr) ||
                   plugin_guard_node_array_contains_argument_escape(scan, node->data.switchExpression.cases) ||
                   plugin_guard_expression_passes_scoped_value_as_argument(scan,
                                                                           node->data.switchExpression.defaultCase);
        case ZR_AST_SWITCH_CASE:
            return plugin_guard_expression_passes_scoped_value_as_argument(scan, node->data.switchCase.value) ||
                   plugin_guard_expression_passes_scoped_value_as_argument(scan, node->data.switchCase.block);
        case ZR_AST_SWITCH_DEFAULT:
            return plugin_guard_expression_passes_scoped_value_as_argument(scan, node->data.switchDefault.block);
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return plugin_guard_expression_passes_scoped_value_as_argument(scan,
                                                                           node->data.breakContinueStatement.expr);
        case ZR_AST_OUT_STATEMENT:
            return plugin_guard_expression_passes_scoped_value_as_argument(scan, node->data.outStatement.expr);
        case ZR_AST_ARRAY_LITERAL:
            return plugin_guard_node_array_contains_argument_escape(scan, node->data.arrayLiteral.elements);
        case ZR_AST_OBJECT_LITERAL:
            return plugin_guard_node_array_contains_argument_escape(scan, node->data.objectLiteral.properties);
        case ZR_AST_KEY_VALUE_PAIR:
            return (node->data.keyValuePair.keyIsComputed &&
                    plugin_guard_expression_passes_scoped_value_as_argument(scan, node->data.keyValuePair.key)) ||
                   plugin_guard_expression_passes_scoped_value_as_argument(scan, node->data.keyValuePair.value);
        case ZR_AST_UNPACK_LITERAL:
            return plugin_guard_expression_passes_scoped_value_as_argument(scan, node->data.unpackLiteral.element);
        case ZR_AST_TEMPLATE_STRING_LITERAL:
            return plugin_guard_node_array_contains_argument_escape(scan, node->data.templateStringLiteral.segments);
        case ZR_AST_INTERPOLATED_SEGMENT:
            return plugin_guard_expression_passes_scoped_value_as_argument(scan,
                                                                           node->data.interpolatedSegment.expression);
        case ZR_AST_BLOCK:
            return node->data.block.body != ZR_NULL &&
                   plugin_guard_node_array_contains_argument_escape(scan, node->data.block.body);
        default:
            return ZR_FALSE;
    }
}

TZrBool plugin_guard_expression_contains_lambda_capture(SZrPluginGuardEscapeScan *scan, SZrAstNode *node) {
    if (scan == ZR_NULL || node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_LAMBDA_EXPRESSION:
            return plugin_guard_callable_body_contains_scoped_value(scan,
                                                                    node->data.lambdaExpression.params,
                                                                    node->data.lambdaExpression.args,
                                                                    node->data.lambdaExpression.block);
        case ZR_AST_GENERATOR_EXPRESSION:
            return plugin_guard_callable_body_contains_scoped_value(scan,
                                                                    ZR_NULL,
                                                                    ZR_NULL,
                                                                    node->data.generatorExpression.block);
        case ZR_AST_FUNCTION_DECLARATION:
            return plugin_guard_callable_body_contains_scoped_value(scan,
                                                                    node->data.functionDeclaration.params,
                                                                    node->data.functionDeclaration.args,
                                                                    node->data.functionDeclaration.body);
        case ZR_AST_VARIABLE_DECLARATION:
            return plugin_guard_expression_contains_lambda_capture(scan, node->data.variableDeclaration.value);
        case ZR_AST_EXPRESSION_STATEMENT:
            return plugin_guard_expression_contains_lambda_capture(scan, node->data.expressionStatement.expr);
        case ZR_AST_DECORATOR_EXPRESSION:
            return plugin_guard_expression_contains_lambda_capture(scan, node->data.decoratorExpression.expr);
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return plugin_guard_expression_contains_lambda_capture(scan, node->data.assignmentExpression.left) ||
                   plugin_guard_expression_contains_lambda_capture(scan, node->data.assignmentExpression.right);
        case ZR_AST_BINARY_EXPRESSION:
            return plugin_guard_expression_contains_lambda_capture(scan, node->data.binaryExpression.left) ||
                   plugin_guard_expression_contains_lambda_capture(scan, node->data.binaryExpression.right);
        case ZR_AST_UNARY_EXPRESSION:
            return plugin_guard_expression_contains_lambda_capture(scan, node->data.unaryExpression.argument);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return plugin_guard_expression_contains_lambda_capture(scan, node->data.typeCastExpression.expression);
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            return plugin_guard_expression_contains_lambda_capture(scan, node->data.typeQueryExpression.operand);
        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            return plugin_guard_expression_contains_lambda_capture(scan,
                                                                   node->data.prototypeReferenceExpression.target);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return plugin_guard_expression_contains_lambda_capture(scan, node->data.conditionalExpression.test) ||
                   plugin_guard_expression_contains_lambda_capture(scan, node->data.conditionalExpression.consequent) ||
                   plugin_guard_expression_contains_lambda_capture(scan, node->data.conditionalExpression.alternate);
        case ZR_AST_LOGICAL_EXPRESSION:
            return plugin_guard_expression_contains_lambda_capture(scan, node->data.logicalExpression.left) ||
                   plugin_guard_expression_contains_lambda_capture(scan, node->data.logicalExpression.right);
        case ZR_AST_IF_EXPRESSION:
            return plugin_guard_expression_contains_lambda_capture(scan, node->data.ifExpression.condition) ||
                   plugin_guard_expression_contains_lambda_capture(scan, node->data.ifExpression.thenExpr) ||
                   plugin_guard_expression_contains_lambda_capture(scan, node->data.ifExpression.elseExpr);
        case ZR_AST_FUNCTION_CALL:
            return plugin_guard_node_array_contains_lambda_capture(scan, node->data.functionCall.args);
        case ZR_AST_CONSTRUCT_EXPRESSION:
            return plugin_guard_expression_contains_lambda_capture(scan, node->data.constructExpression.target) ||
                   plugin_guard_node_array_contains_lambda_capture(scan, node->data.constructExpression.args);
        case ZR_AST_MEMBER_EXPRESSION:
            return plugin_guard_expression_contains_lambda_capture(scan, node->data.memberExpression.property);
        case ZR_AST_PRIMARY_EXPRESSION:
            return plugin_guard_expression_contains_lambda_capture(scan, node->data.primaryExpression.property) ||
                   plugin_guard_node_array_contains_lambda_capture(scan, node->data.primaryExpression.members);
        case ZR_AST_ARRAY_LITERAL:
            return plugin_guard_node_array_contains_lambda_capture(scan, node->data.arrayLiteral.elements);
        case ZR_AST_OBJECT_LITERAL:
            return plugin_guard_node_array_contains_lambda_capture(scan, node->data.objectLiteral.properties);
        case ZR_AST_KEY_VALUE_PAIR:
            return (node->data.keyValuePair.keyIsComputed &&
                    plugin_guard_expression_contains_lambda_capture(scan, node->data.keyValuePair.key)) ||
                   plugin_guard_expression_contains_lambda_capture(scan, node->data.keyValuePair.value);
        case ZR_AST_UNPACK_LITERAL:
            return plugin_guard_expression_contains_lambda_capture(scan, node->data.unpackLiteral.element);
        case ZR_AST_OUT_STATEMENT:
            return plugin_guard_expression_contains_lambda_capture(scan, node->data.outStatement.expr);
        case ZR_AST_TEMPLATE_STRING_LITERAL:
            return plugin_guard_node_array_contains_lambda_capture(scan, node->data.templateStringLiteral.segments);
        case ZR_AST_INTERPOLATED_SEGMENT:
            return plugin_guard_expression_contains_lambda_capture(scan, node->data.interpolatedSegment.expression);
        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    if (plugin_guard_expression_contains_lambda_capture(scan,
                                                                        node->data.block.body->nodes[index])) {
                        return ZR_TRUE;
                    }
                }
            }
            return ZR_FALSE;
        default:
            return ZR_FALSE;
    }
}

TZrBool plugin_guard_expression_contains_scoped_value(SZrPluginGuardEscapeScan *scan, SZrAstNode *node) {
    if (scan == ZR_NULL || node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            if (plugin_guard_name_is_shadowed(scan, node->data.identifier.name)) {
                return ZR_FALSE;
            }
            return plugin_guard_name_array_contains(&scan->pluginNames, node->data.identifier.name);
        case ZR_AST_PRIMARY_EXPRESSION:
            if (node->data.primaryExpression.property != ZR_NULL &&
                node->data.primaryExpression.property->type == ZR_AST_IDENTIFIER_LITERAL &&
                !plugin_guard_name_is_shadowed(scan,
                                               node->data.primaryExpression.property->data.identifier.name) &&
                plugin_guard_name_array_contains(&scan->pluginNames,
                                                  node->data.primaryExpression.property->data.identifier.name)) {
                return (TZrBool)(node->data.primaryExpression.members == ZR_NULL ||
                                  node->data.primaryExpression.members->count == 0 ||
                                  plugin_guard_primary_member_reference_is_scoped_value(scan, node));
            }
            return plugin_guard_expression_contains_scoped_value(scan, node->data.primaryExpression.property) ||
                   plugin_guard_node_array_contains_scoped_value(scan, node->data.primaryExpression.members);
        case ZR_AST_MEMBER_EXPRESSION:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.memberExpression.property);
        case ZR_AST_FUNCTION_CALL:
            return plugin_guard_node_array_contains_scoped_value(scan, node->data.functionCall.args);
        case ZR_AST_CONSTRUCT_EXPRESSION:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.constructExpression.target) ||
                   plugin_guard_node_array_contains_scoped_value(scan, node->data.constructExpression.args);
        case ZR_AST_EXPRESSION_STATEMENT:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.expressionStatement.expr);
        case ZR_AST_DECORATOR_EXPRESSION:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.decoratorExpression.expr);
        case ZR_AST_RETURN_STATEMENT:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.returnStatement.expr);
        case ZR_AST_VARIABLE_DECLARATION:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.variableDeclaration.value);
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.assignmentExpression.left) ||
                   plugin_guard_expression_contains_scoped_value(scan, node->data.assignmentExpression.right);
        case ZR_AST_BINARY_EXPRESSION:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.binaryExpression.left) ||
                   plugin_guard_expression_contains_scoped_value(scan, node->data.binaryExpression.right);
        case ZR_AST_UNARY_EXPRESSION:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.unaryExpression.argument);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.typeCastExpression.expression);
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.typeQueryExpression.operand);
        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            return plugin_guard_expression_contains_scoped_value(scan,
                                                                 node->data.prototypeReferenceExpression.target);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.conditionalExpression.test) ||
                   plugin_guard_expression_contains_scoped_value(scan, node->data.conditionalExpression.consequent) ||
                   plugin_guard_expression_contains_scoped_value(scan, node->data.conditionalExpression.alternate);
        case ZR_AST_LOGICAL_EXPRESSION:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.logicalExpression.left) ||
                   plugin_guard_expression_contains_scoped_value(scan, node->data.logicalExpression.right);
        case ZR_AST_LAMBDA_EXPRESSION:
            return plugin_guard_block_contains_scoped_value(scan, node->data.lambdaExpression.block);
        case ZR_AST_GENERATOR_EXPRESSION:
            return plugin_guard_block_contains_scoped_value(scan, node->data.generatorExpression.block);
        case ZR_AST_IF_EXPRESSION:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.ifExpression.condition) ||
                   plugin_guard_expression_contains_scoped_value(scan, node->data.ifExpression.thenExpr) ||
                   plugin_guard_expression_contains_scoped_value(scan, node->data.ifExpression.elseExpr);
        case ZR_AST_SWITCH_EXPRESSION:
            if (plugin_guard_expression_contains_scoped_value(scan, node->data.switchExpression.expr)) {
                return ZR_TRUE;
            }
            if (node->data.switchExpression.cases != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.switchExpression.cases->count; index++) {
                    if (plugin_guard_expression_contains_scoped_value(scan,
                                                                      node->data.switchExpression.cases->nodes[index])) {
                        return ZR_TRUE;
                    }
                }
            }
            return plugin_guard_expression_contains_scoped_value(scan, node->data.switchExpression.defaultCase);
        case ZR_AST_SWITCH_CASE:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.switchCase.value) ||
                   plugin_guard_expression_contains_scoped_value(scan, node->data.switchCase.block);
        case ZR_AST_SWITCH_DEFAULT:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.switchDefault.block);
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.breakContinueStatement.expr);
        case ZR_AST_OUT_STATEMENT:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.outStatement.expr);
        case ZR_AST_ARRAY_LITERAL:
            return plugin_guard_node_array_contains_scoped_value(scan, node->data.arrayLiteral.elements);
        case ZR_AST_OBJECT_LITERAL:
            return plugin_guard_node_array_contains_scoped_value(scan, node->data.objectLiteral.properties);
        case ZR_AST_KEY_VALUE_PAIR:
            return (node->data.keyValuePair.keyIsComputed &&
                    plugin_guard_expression_contains_scoped_value(scan, node->data.keyValuePair.key)) ||
                   plugin_guard_expression_contains_scoped_value(scan, node->data.keyValuePair.value);
        case ZR_AST_UNPACK_LITERAL:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.unpackLiteral.element);
        case ZR_AST_TEMPLATE_STRING_LITERAL:
            return plugin_guard_node_array_contains_scoped_value(scan, node->data.templateStringLiteral.segments);
        case ZR_AST_INTERPOLATED_SEGMENT:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.interpolatedSegment.expression);
        case ZR_AST_BLOCK:
            return plugin_guard_block_contains_scoped_value(scan, node);
        default:
            return ZR_FALSE;
    }
}

TZrBool plugin_guard_expression_contains_container_escape(SZrPluginGuardEscapeScan *scan, SZrAstNode *node) {
    if (scan == ZR_NULL || node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_ARRAY_LITERAL:
            return plugin_guard_node_array_contains_scoped_value(scan, node->data.arrayLiteral.elements) ||
                   plugin_guard_node_array_contains_container_escape(scan, node->data.arrayLiteral.elements);
        case ZR_AST_OBJECT_LITERAL:
            return plugin_guard_node_array_contains_scoped_value(scan, node->data.objectLiteral.properties) ||
                   plugin_guard_node_array_contains_container_escape(scan, node->data.objectLiteral.properties);
        case ZR_AST_KEY_VALUE_PAIR:
            return (node->data.keyValuePair.keyIsComputed &&
                    (plugin_guard_expression_contains_scoped_value(scan, node->data.keyValuePair.key) ||
                     plugin_guard_expression_contains_container_escape(scan, node->data.keyValuePair.key))) ||
                   plugin_guard_expression_contains_scoped_value(scan, node->data.keyValuePair.value) ||
                   plugin_guard_expression_contains_container_escape(scan, node->data.keyValuePair.value);
        case ZR_AST_UNPACK_LITERAL:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.unpackLiteral.element) ||
                   plugin_guard_expression_contains_container_escape(scan, node->data.unpackLiteral.element);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return plugin_guard_expression_contains_container_escape(scan, node->data.conditionalExpression.test) ||
                   plugin_guard_expression_contains_container_escape(scan,
                                                                     node->data.conditionalExpression.consequent) ||
                   plugin_guard_expression_contains_container_escape(scan,
                                                                     node->data.conditionalExpression.alternate);
        case ZR_AST_LOGICAL_EXPRESSION:
            return plugin_guard_expression_contains_container_escape(scan, node->data.logicalExpression.left) ||
                   plugin_guard_expression_contains_container_escape(scan, node->data.logicalExpression.right);
        case ZR_AST_BINARY_EXPRESSION:
            return plugin_guard_expression_contains_container_escape(scan, node->data.binaryExpression.left) ||
                   plugin_guard_expression_contains_container_escape(scan, node->data.binaryExpression.right);
        case ZR_AST_UNARY_EXPRESSION:
            return plugin_guard_expression_contains_container_escape(scan, node->data.unaryExpression.argument);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return plugin_guard_expression_contains_container_escape(scan, node->data.typeCastExpression.expression);
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            return plugin_guard_expression_contains_container_escape(scan, node->data.typeQueryExpression.operand);
        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            return plugin_guard_expression_contains_container_escape(scan,
                                                                     node->data.prototypeReferenceExpression.target);
        case ZR_AST_PRIMARY_EXPRESSION:
            return plugin_guard_expression_contains_container_escape(scan, node->data.primaryExpression.property) ||
                   plugin_guard_node_array_contains_container_escape(scan, node->data.primaryExpression.members);
        case ZR_AST_MEMBER_EXPRESSION:
            return plugin_guard_expression_contains_container_escape(scan, node->data.memberExpression.property);
        case ZR_AST_FUNCTION_CALL:
            return plugin_guard_node_array_contains_container_escape(scan, node->data.functionCall.args);
        case ZR_AST_CONSTRUCT_EXPRESSION:
            return plugin_guard_expression_contains_container_escape(scan, node->data.constructExpression.target) ||
                   plugin_guard_node_array_contains_container_escape(scan, node->data.constructExpression.args);
        case ZR_AST_GENERATOR_EXPRESSION:
            return plugin_guard_expression_contains_container_escape(scan, node->data.generatorExpression.block);
        case ZR_AST_OUT_STATEMENT:
            return plugin_guard_expression_contains_container_escape(scan, node->data.outStatement.expr);
        case ZR_AST_TEMPLATE_STRING_LITERAL:
            return plugin_guard_node_array_contains_scoped_value(scan, node->data.templateStringLiteral.segments) ||
                   plugin_guard_node_array_contains_container_escape(scan, node->data.templateStringLiteral.segments);
        case ZR_AST_INTERPOLATED_SEGMENT:
            return plugin_guard_expression_contains_scoped_value(scan, node->data.interpolatedSegment.expression) ||
                   plugin_guard_expression_contains_container_escape(scan, node->data.interpolatedSegment.expression);
        case ZR_AST_DECORATOR_EXPRESSION:
            return plugin_guard_expression_contains_container_escape(scan, node->data.decoratorExpression.expr);
        default:
            return ZR_FALSE;
    }
}

static TZrBool plugin_guard_scan_assignment(SZrPluginGuardEscapeScan *scan, SZrAstNode *node) {
    SZrString *targetName;

    if (scan == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_ASSIGNMENT_EXPRESSION) {
        return ZR_TRUE;
    }

    if (plugin_guard_expression_contains_lambda_capture(scan, node->data.assignmentExpression.right)) {
        return plugin_guard_report_escape(scan, node->data.assignmentExpression.right->location, "closure capture");
    }

    if (plugin_guard_expression_passes_scoped_value_as_argument(scan, node->data.assignmentExpression.right)) {
        return plugin_guard_report_escape(scan,
                                          node->data.assignmentExpression.right != ZR_NULL
                                                  ? node->data.assignmentExpression.right->location
                                                  : node->location,
                                          "call argument");
    }

    if (plugin_guard_expression_contains_container_escape(scan, node->data.assignmentExpression.right)) {
        return plugin_guard_report_escape(scan,
                                          node->data.assignmentExpression.right != ZR_NULL
                                                  ? node->data.assignmentExpression.right->location
                                                  : node->location,
                                          "field/container");
    }

    if (!plugin_guard_expression_contains_scoped_value(scan, node->data.assignmentExpression.right)) {
        return ZR_TRUE;
    }

    targetName = plugin_guard_bare_identifier_name(node->data.assignmentExpression.left);
    if (targetName != ZR_NULL && plugin_guard_name_array_contains(&scan->localNames, targetName)) {
        plugin_guard_push_name_unique(scan, &scan->pluginNames, targetName);
        return ZR_TRUE;
    }

    return plugin_guard_report_escape(scan, node->location, "outer/global assignment");
}

static TZrBool plugin_guard_scan_expression_array_side_effects(SZrPluginGuardEscapeScan *scan,
                                                               SZrAstNodeArray *nodes) {
    if (scan == ZR_NULL || nodes == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        if (!plugin_guard_scan_expression_side_effects(scan, nodes->nodes[index])) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool plugin_guard_scan_expression_side_effects(SZrPluginGuardEscapeScan *scan, SZrAstNode *node) {
    if (scan == ZR_NULL || node == ZR_NULL) {
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return plugin_guard_scan_assignment(scan, node) &&
                   plugin_guard_scan_expression_side_effects(scan, node->data.assignmentExpression.left) &&
                   plugin_guard_scan_expression_side_effects(scan, node->data.assignmentExpression.right);
        case ZR_AST_EXPRESSION_STATEMENT:
            return plugin_guard_scan_expression_side_effects(scan, node->data.expressionStatement.expr);
        case ZR_AST_DECORATOR_EXPRESSION:
            return plugin_guard_scan_expression_side_effects(scan, node->data.decoratorExpression.expr);
        case ZR_AST_BINARY_EXPRESSION:
            return plugin_guard_scan_expression_side_effects(scan, node->data.binaryExpression.left) &&
                   plugin_guard_scan_expression_side_effects(scan, node->data.binaryExpression.right);
        case ZR_AST_LOGICAL_EXPRESSION:
            return plugin_guard_scan_expression_side_effects(scan, node->data.logicalExpression.left) &&
                   plugin_guard_scan_expression_side_effects(scan, node->data.logicalExpression.right);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return plugin_guard_scan_expression_side_effects(scan, node->data.conditionalExpression.test) &&
                   plugin_guard_scan_expression_side_effects(scan, node->data.conditionalExpression.consequent) &&
                   plugin_guard_scan_expression_side_effects(scan, node->data.conditionalExpression.alternate);
        case ZR_AST_UNARY_EXPRESSION:
            return plugin_guard_scan_expression_side_effects(scan, node->data.unaryExpression.argument);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return plugin_guard_scan_expression_side_effects(scan, node->data.typeCastExpression.expression);
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            return plugin_guard_scan_expression_side_effects(scan, node->data.typeQueryExpression.operand);
        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            return plugin_guard_scan_expression_side_effects(scan,
                                                             node->data.prototypeReferenceExpression.target);
        case ZR_AST_PRIMARY_EXPRESSION:
            return plugin_guard_scan_expression_side_effects(scan, node->data.primaryExpression.property) &&
                   plugin_guard_scan_expression_array_side_effects(scan, node->data.primaryExpression.members);
        case ZR_AST_MEMBER_EXPRESSION:
            return plugin_guard_scan_expression_side_effects(scan, node->data.memberExpression.property);
        case ZR_AST_FUNCTION_CALL:
            return plugin_guard_scan_generic_argument_metadata(scan,
                                                               node->data.functionCall.genericArguments,
                                                               node->location,
                                                               "signature type") &&
                   plugin_guard_scan_expression_array_side_effects(scan, node->data.functionCall.args);
        case ZR_AST_CONSTRUCT_EXPRESSION:
            return plugin_guard_scan_expression_side_effects(scan, node->data.constructExpression.target) &&
                   plugin_guard_scan_expression_array_side_effects(scan, node->data.constructExpression.args);
        case ZR_AST_IF_EXPRESSION:
            return plugin_guard_scan_expression_side_effects(scan, node->data.ifExpression.condition) &&
                   plugin_guard_scan_statement(scan, node->data.ifExpression.thenExpr) &&
                   plugin_guard_scan_statement(scan, node->data.ifExpression.elseExpr);
        case ZR_AST_SWITCH_EXPRESSION:
            return plugin_guard_scan_expression_side_effects(scan, node->data.switchExpression.expr) &&
                   plugin_guard_scan_expression_array_side_effects(scan, node->data.switchExpression.cases) &&
                   plugin_guard_scan_statement(scan, node->data.switchExpression.defaultCase);
        case ZR_AST_SWITCH_CASE:
            return plugin_guard_scan_expression_side_effects(scan, node->data.switchCase.value) &&
                   plugin_guard_scan_statement(scan, node->data.switchCase.block);
        case ZR_AST_SWITCH_DEFAULT:
            return plugin_guard_scan_statement(scan, node->data.switchDefault.block);
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return plugin_guard_scan_expression_side_effects(scan, node->data.breakContinueStatement.expr);
        case ZR_AST_OUT_STATEMENT:
            return plugin_guard_scan_expression_side_effects(scan, node->data.outStatement.expr);
        case ZR_AST_RETURN_STATEMENT:
            return plugin_guard_scan_expression_side_effects(scan, node->data.returnStatement.expr);
        case ZR_AST_ARRAY_LITERAL:
            return plugin_guard_scan_expression_array_side_effects(scan, node->data.arrayLiteral.elements);
        case ZR_AST_OBJECT_LITERAL:
            return plugin_guard_scan_expression_array_side_effects(scan, node->data.objectLiteral.properties);
        case ZR_AST_KEY_VALUE_PAIR:
            return (!node->data.keyValuePair.keyIsComputed ||
                    plugin_guard_scan_expression_side_effects(scan, node->data.keyValuePair.key)) &&
                   plugin_guard_scan_expression_side_effects(scan, node->data.keyValuePair.value);
        case ZR_AST_UNPACK_LITERAL:
            return plugin_guard_scan_expression_side_effects(scan, node->data.unpackLiteral.element);
        case ZR_AST_TEMPLATE_STRING_LITERAL:
            return plugin_guard_scan_expression_array_side_effects(scan, node->data.templateStringLiteral.segments);
        case ZR_AST_INTERPOLATED_SEGMENT:
            return plugin_guard_scan_expression_side_effects(scan, node->data.interpolatedSegment.expression);
        case ZR_AST_BLOCK:
            return plugin_guard_scan_statement(scan, node);
        default:
            return ZR_TRUE;
    }
}
