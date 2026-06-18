#include "compiler_using_plugin_guard_escape_internal.h"

static TZrBool plugin_guard_name_array_contains_prefix(SZrArray *names,
                                                       TZrSize length,
                                                       SZrString *name) {
    if (names == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    if (length > names->length) {
        length = names->length;
    }

    for (TZrSize index = 0; index < length; index++) {
        SZrString **candidate = (SZrString **)ZrCore_Array_Get(names, index);
        if (candidate != ZR_NULL && *candidate != ZR_NULL &&
            (*candidate == name || ZrCore_String_Equal(*candidate, name))) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void plugin_guard_leave_nested_region(SZrPluginGuardEscapeScan *scan,
                                             TZrSize localLength,
                                             TZrSize pluginLength,
                                             TZrBool preserveOuterAssignments) {
    TZrSize writeLength;

    if (scan == ZR_NULL) {
        return;
    }

    if (!preserveOuterAssignments) {
        scan->localNames.length = localLength;
        scan->pluginNames.length = pluginLength;
        return;
    }

    writeLength = pluginLength;
    for (TZrSize readIndex = pluginLength; readIndex < scan->pluginNames.length; readIndex++) {
        SZrString **nameSlot = (SZrString **)ZrCore_Array_Get(&scan->pluginNames, readIndex);
        SZrString *name = nameSlot != ZR_NULL ? *nameSlot : ZR_NULL;
        if (name == ZR_NULL ||
            !plugin_guard_name_array_contains_prefix(&scan->localNames, localLength, name) ||
            plugin_guard_name_array_contains_prefix(&scan->pluginNames, writeLength, name)) {
            continue;
        }
        if (writeLength != readIndex) {
            ZrCore_Array_Set(&scan->pluginNames, writeLength, &name);
        }
        writeLength++;
    }
    scan->localNames.length = localLength;
    scan->pluginNames.length = writeLength;
}

static TZrBool plugin_guard_scan_variable_declaration(SZrPluginGuardEscapeScan *scan, SZrAstNode *node) {
    SZrVariableDeclaration *decl;
    SZrString *name;
    TZrBool valueContainsPlugin;

    if (scan == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_VARIABLE_DECLARATION) {
        return ZR_TRUE;
    }

    decl = &node->data.variableDeclaration;
    name = plugin_guard_bare_identifier_name(decl->pattern);
    if (plugin_guard_expression_contains_lambda_capture(scan, decl->value)) {
        return plugin_guard_report_escape(scan,
                                          decl->value != ZR_NULL ? decl->value->location : node->location,
                                          "closure capture");
    }

    if (!plugin_guard_scan_expression_side_effects(scan, decl->value)) {
        return ZR_FALSE;
    }

    if (plugin_guard_expression_passes_scoped_value_as_argument(scan, decl->value)) {
        return plugin_guard_report_escape(scan,
                                          decl->value != ZR_NULL ? decl->value->location : node->location,
                                          "call argument");
    }

    if (plugin_guard_expression_contains_container_escape(scan, decl->value)) {
        return plugin_guard_report_escape(scan,
                                          decl->value != ZR_NULL ? decl->value->location : node->location,
                                          "field/container");
    }

    valueContainsPlugin = plugin_guard_expression_contains_scoped_value(scan, decl->value);
    if (valueContainsPlugin && decl->accessModifier != ZR_ACCESS_PRIVATE) {
        return plugin_guard_report_escape(scan,
                                          decl->value != ZR_NULL ? decl->value->location : node->location,
                                          "exported declaration");
    }

    if (name == ZR_NULL) {
        return !valueContainsPlugin ||
               plugin_guard_report_escape(scan, node->location, "destructuring declaration");
    }

    plugin_guard_push_name_unique(scan, &scan->localNames, name);
    if (valueContainsPlugin) {
        plugin_guard_push_name_unique(scan, &scan->pluginNames, name);
    }
    return ZR_TRUE;
}

static TZrBool plugin_guard_scan_statement_array(SZrPluginGuardEscapeScan *scan, SZrAstNodeArray *nodes) {
    if (scan == ZR_NULL || nodes == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        if (!plugin_guard_scan_statement(scan, nodes->nodes[index])) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool plugin_guard_scan_expression_boundary(SZrPluginGuardEscapeScan *scan, SZrAstNode *node) {
    if (scan == ZR_NULL || node == ZR_NULL) {
        return ZR_TRUE;
    }

    if (plugin_guard_expression_contains_lambda_capture(scan, node)) {
        return plugin_guard_report_escape(scan, node->location, "closure capture");
    }

    if (plugin_guard_expression_passes_scoped_value_as_argument(scan, node)) {
        return plugin_guard_report_escape(scan, node->location, "call argument");
    }

    return plugin_guard_scan_expression_side_effects(scan, node);
}

static TZrBool plugin_guard_scan_argument_boundary(SZrPluginGuardEscapeScan *scan,
                                                   SZrAstNode *node,
                                                   const TZrChar *reason) {
    if (scan == ZR_NULL || node == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!plugin_guard_scan_expression_boundary(scan, node)) {
        return ZR_FALSE;
    }

    if (plugin_guard_expression_contains_container_escape(scan, node) ||
        plugin_guard_expression_contains_scoped_value(scan, node)) {
        return plugin_guard_report_escape(scan, node->location, reason);
    }

    return ZR_TRUE;
}

static TZrBool plugin_guard_scan_argument_array(SZrPluginGuardEscapeScan *scan,
                                                SZrAstNodeArray *nodes,
                                                const TZrChar *reason) {
    if (scan == ZR_NULL || nodes == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        if (!plugin_guard_scan_argument_boundary(scan, nodes->nodes[index], reason)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool plugin_guard_scan_flow_expression(SZrPluginGuardEscapeScan *scan,
                                                 SZrAstNode *expr,
                                                 SZrFileRange fallbackLocation,
                                                 const TZrChar *reason) {
    if (scan == ZR_NULL || expr == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!plugin_guard_scan_expression_side_effects(scan, expr)) {
        return ZR_FALSE;
    }

    if (plugin_guard_expression_passes_scoped_value_as_argument(scan, expr)) {
        return plugin_guard_report_escape(scan,
                                          expr != ZR_NULL ? expr->location : fallbackLocation,
                                          "call argument");
    }

    if (plugin_guard_expression_contains_scoped_value(scan, expr)) {
        return plugin_guard_report_escape(scan,
                                          expr != ZR_NULL ? expr->location : fallbackLocation,
                                          reason);
    }

    return ZR_TRUE;
}

static TZrBool plugin_guard_decorator_mentions_scoped_value(SZrPluginGuardEscapeScan *scan, SZrAstNode *node);

static TZrBool plugin_guard_decorator_array_mentions_scoped_value(SZrPluginGuardEscapeScan *scan,
                                                                  SZrAstNodeArray *nodes);

static TZrBool plugin_guard_scan_decorator_boundary(SZrPluginGuardEscapeScan *scan, SZrAstNode *node) {
    if (scan == ZR_NULL || node == ZR_NULL) {
        return ZR_TRUE;
    }

    if (plugin_guard_expression_contains_lambda_capture(scan, node)) {
        return plugin_guard_report_escape(scan, node->location, "decorator");
    }

    if (plugin_guard_decorator_mentions_scoped_value(scan, node) ||
        plugin_guard_expression_passes_scoped_value_as_argument(scan, node) ||
        plugin_guard_expression_contains_container_escape(scan, node)) {
        return plugin_guard_report_escape(scan, node->location, "decorator");
    }

    return plugin_guard_scan_expression_side_effects(scan, node);
}

static TZrBool plugin_guard_scan_decorator_array(SZrPluginGuardEscapeScan *scan, SZrAstNodeArray *decorators) {
    if (scan == ZR_NULL || decorators == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        if (!plugin_guard_scan_decorator_boundary(scan, decorators->nodes[index])) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool plugin_guard_scan_parameter(SZrPluginGuardEscapeScan *scan,
                                           SZrParameter *parameter,
                                           SZrFileRange fallbackLocation);

static TZrBool plugin_guard_scan_parameter_array(SZrPluginGuardEscapeScan *scan, SZrAstNodeArray *params);

static TZrBool plugin_guard_scan_type(SZrPluginGuardEscapeScan *scan,
                                      SZrType *typeInfo,
                                      SZrFileRange fallbackLocation,
                                      const TZrChar *reason);

static TZrBool plugin_guard_scan_type_node(SZrPluginGuardEscapeScan *scan,
                                           SZrAstNode *node,
                                           SZrFileRange fallbackLocation,
                                           const TZrChar *reason);

static TZrBool plugin_guard_scan_generic_declaration(SZrPluginGuardEscapeScan *scan,
                                                     SZrGenericDeclaration *generic,
                                                     SZrFileRange fallbackLocation,
                                                     const TZrChar *reason);

static TZrBool plugin_guard_scan_function_signature(SZrPluginGuardEscapeScan *scan,
                                                    SZrGenericDeclaration *generic,
                                                    SZrAstNodeArray *params,
                                                    SZrParameter *args,
                                                    SZrType *returnType,
                                                    SZrFileRange fallbackLocation);

static TZrBool plugin_guard_scan_signature_expression(SZrPluginGuardEscapeScan *scan,
                                                      SZrAstNode *node,
                                                      SZrFileRange fallbackLocation,
                                                      const TZrChar *reason) {
    if (scan == ZR_NULL || node == ZR_NULL) {
        return ZR_TRUE;
    }

    if (plugin_guard_expression_contains_lambda_capture(scan, node) ||
        plugin_guard_expression_passes_scoped_value_as_argument(scan, node) ||
        plugin_guard_expression_contains_container_escape(scan, node) ||
        plugin_guard_expression_contains_scoped_value(scan, node)) {
        return plugin_guard_report_escape(scan, node != ZR_NULL ? node->location : fallbackLocation, reason);
    }

    return plugin_guard_scan_expression_side_effects(scan, node);
}

static TZrBool plugin_guard_scan_type_node_array(SZrPluginGuardEscapeScan *scan,
                                                 SZrAstNodeArray *nodes,
                                                 SZrFileRange fallbackLocation,
                                                 const TZrChar *reason) {
    if (scan == ZR_NULL || nodes == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        if (!plugin_guard_scan_type_node(scan, nodes->nodes[index], fallbackLocation, reason)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool plugin_guard_scan_generic_argument_metadata(SZrPluginGuardEscapeScan *scan,
                                                    SZrAstNodeArray *arguments,
                                                    SZrFileRange fallbackLocation,
                                                    const TZrChar *reason) {
    return plugin_guard_scan_type_node_array(scan, arguments, fallbackLocation, reason);
}

static TZrBool plugin_guard_scan_type_name(SZrPluginGuardEscapeScan *scan,
                                           SZrIdentifier *name,
                                           SZrFileRange location,
                                           const TZrChar *reason) {
    if (scan == ZR_NULL || name == ZR_NULL || name->name == ZR_NULL) {
        return ZR_TRUE;
    }

    if (plugin_guard_name_array_contains(&scan->pluginNames, name->name)) {
        return plugin_guard_report_escape(scan, location, reason);
    }

    return ZR_TRUE;
}

static TZrBool plugin_guard_scan_type_node(SZrPluginGuardEscapeScan *scan,
                                           SZrAstNode *node,
                                           SZrFileRange fallbackLocation,
                                           const TZrChar *reason) {
    if (scan == ZR_NULL || node == ZR_NULL) {
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_TYPE:
            return plugin_guard_scan_type(scan, &node->data.type, node->location, reason);
        case ZR_AST_IDENTIFIER_LITERAL:
            return plugin_guard_scan_type_name(scan, &node->data.identifier, node->location, reason);
        case ZR_AST_GENERIC_TYPE:
            return plugin_guard_scan_type_name(scan, node->data.genericType.name, node->location, reason) &&
                   plugin_guard_scan_type_node_array(scan,
                                                     node->data.genericType.params,
                                                     node->location,
                                                     reason);
        case ZR_AST_TUPLE_TYPE:
            return plugin_guard_scan_type_node_array(scan, node->data.tupleType.elements, node->location, reason);
        case ZR_AST_FUNCTION_TYPE:
            return plugin_guard_scan_function_signature(scan,
                                                        node->data.functionType.generic,
                                                        node->data.functionType.params,
                                                        node->data.functionType.args,
                                                        node->data.functionType.returnType,
                                                        node->location);
        default:
            return plugin_guard_scan_signature_expression(scan, node, fallbackLocation, reason);
    }
}

static TZrBool plugin_guard_scan_type(SZrPluginGuardEscapeScan *scan,
                                      SZrType *typeInfo,
                                      SZrFileRange fallbackLocation,
                                      const TZrChar *reason) {
    if (scan == ZR_NULL || typeInfo == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!plugin_guard_scan_type_node(scan, typeInfo->name, fallbackLocation, reason)) {
        return ZR_FALSE;
    }

    if (!plugin_guard_scan_type(scan, typeInfo->subType, fallbackLocation, reason)) {
        return ZR_FALSE;
    }

    return plugin_guard_scan_signature_expression(scan, typeInfo->arraySizeExpression, fallbackLocation, reason);
}

static TZrBool plugin_guard_scan_generic_declaration(SZrPluginGuardEscapeScan *scan,
                                                     SZrGenericDeclaration *generic,
                                                     SZrFileRange fallbackLocation,
                                                     const TZrChar *reason) {
    if (scan == ZR_NULL || generic == ZR_NULL) {
        return ZR_TRUE;
    }

    (void)fallbackLocation;
    (void)reason;
    return plugin_guard_scan_parameter_array(scan, generic->params);
}

static TZrBool plugin_guard_scan_parameter(SZrPluginGuardEscapeScan *scan,
                                           SZrParameter *parameter,
                                           SZrFileRange fallbackLocation) {
    SZrAstNode *defaultValue;

    if (scan == ZR_NULL || parameter == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!plugin_guard_scan_decorator_array(scan, parameter->decorators)) {
        return ZR_FALSE;
    }

    if (!plugin_guard_scan_type(scan, parameter->typeInfo, fallbackLocation, "signature type")) {
        return ZR_FALSE;
    }

    if (!plugin_guard_scan_type_node_array(scan,
                                           parameter->genericTypeConstraints,
                                           fallbackLocation,
                                           "signature type")) {
        return ZR_FALSE;
    }

    defaultValue = parameter->defaultValue;
    if (defaultValue == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!plugin_guard_scan_expression_boundary(scan, defaultValue)) {
        return ZR_FALSE;
    }

    if (plugin_guard_expression_contains_container_escape(scan, defaultValue) ||
        plugin_guard_expression_contains_scoped_value(scan, defaultValue)) {
        return plugin_guard_report_escape(scan,
                                          defaultValue != ZR_NULL ? defaultValue->location : fallbackLocation,
                                          "parameter default");
    }

    return ZR_TRUE;
}

static TZrBool plugin_guard_scan_parameter_node(SZrPluginGuardEscapeScan *scan, SZrAstNode *node) {
    if (scan == ZR_NULL || node == ZR_NULL) {
        return ZR_TRUE;
    }

    if (node->type != ZR_AST_PARAMETER) {
        return plugin_guard_scan_statement(scan, node);
    }

    return plugin_guard_scan_parameter(scan, &node->data.parameter, node->location);
}

static TZrBool plugin_guard_scan_parameter_array(SZrPluginGuardEscapeScan *scan, SZrAstNodeArray *params) {
    if (scan == ZR_NULL || params == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < params->count; index++) {
        if (!plugin_guard_scan_parameter_node(scan, params->nodes[index])) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool plugin_guard_scan_function_signature(SZrPluginGuardEscapeScan *scan,
                                                    SZrGenericDeclaration *generic,
                                                    SZrAstNodeArray *params,
                                                    SZrParameter *args,
                                                    SZrType *returnType,
                                                    SZrFileRange fallbackLocation) {
    return plugin_guard_scan_generic_declaration(scan, generic, fallbackLocation, "signature type") &&
           plugin_guard_scan_parameter_array(scan, params) &&
           plugin_guard_scan_parameter(scan, args, fallbackLocation) &&
           plugin_guard_scan_type(scan, returnType, fallbackLocation, "signature type");
}

static TZrBool plugin_guard_decorator_mentions_scoped_value(SZrPluginGuardEscapeScan *scan, SZrAstNode *node) {
    if (scan == ZR_NULL || node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            return plugin_guard_name_array_contains(&scan->pluginNames, node->data.identifier.name);
        case ZR_AST_DECORATOR_EXPRESSION:
            return plugin_guard_decorator_mentions_scoped_value(scan, node->data.decoratorExpression.expr);
        case ZR_AST_PRIMARY_EXPRESSION:
            return plugin_guard_decorator_mentions_scoped_value(scan, node->data.primaryExpression.property) ||
                   plugin_guard_decorator_array_mentions_scoped_value(scan, node->data.primaryExpression.members);
        case ZR_AST_MEMBER_EXPRESSION:
            return plugin_guard_decorator_mentions_scoped_value(scan, node->data.memberExpression.property);
        case ZR_AST_FUNCTION_CALL:
            return plugin_guard_decorator_array_mentions_scoped_value(scan, node->data.functionCall.args);
        case ZR_AST_CONSTRUCT_EXPRESSION:
            return plugin_guard_decorator_mentions_scoped_value(scan, node->data.constructExpression.target) ||
                   plugin_guard_decorator_array_mentions_scoped_value(scan, node->data.constructExpression.args);
        default:
            return plugin_guard_expression_contains_scoped_value(scan, node);
    }
}

static TZrBool plugin_guard_decorator_array_mentions_scoped_value(SZrPluginGuardEscapeScan *scan,
                                                                  SZrAstNodeArray *nodes) {
    if (scan == ZR_NULL || nodes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        if (plugin_guard_decorator_mentions_scoped_value(scan, nodes->nodes[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool plugin_guard_scan_decorated_members(SZrPluginGuardEscapeScan *scan, SZrAstNodeArray *members) {
    if (scan == ZR_NULL || members == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < members->count; index++) {
        if (!plugin_guard_scan_statement(scan, members->nodes[index])) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool plugin_guard_scan_block(SZrPluginGuardEscapeScan *scan, SZrAstNode *node) {
    TZrSize localLength;
    TZrSize pluginLength;
    TZrBool ok;

    if (scan == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_BLOCK) {
        return ZR_TRUE;
    }

    localLength = scan->localNames.length;
    pluginLength = scan->pluginNames.length;
    ok = plugin_guard_scan_statement_array(scan, node->data.block.body);
    plugin_guard_leave_nested_region(scan, localLength, pluginLength, ok);
    return ok;
}

TZrBool plugin_guard_scan_statement(SZrPluginGuardEscapeScan *scan, SZrAstNode *node) {
    if (scan == ZR_NULL || node == ZR_NULL) {
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_BLOCK:
            return plugin_guard_scan_block(scan, node);
        case ZR_AST_VARIABLE_DECLARATION:
            return plugin_guard_scan_variable_declaration(scan, node);
        case ZR_AST_RETURN_STATEMENT:
            return plugin_guard_scan_flow_expression(scan, node->data.returnStatement.expr, node->location, "return");
        case ZR_AST_THROW_STATEMENT:
            return plugin_guard_scan_flow_expression(scan, node->data.throwStatement.expr, node->location, "throw");
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return plugin_guard_scan_flow_expression(scan,
                                                     node->data.breakContinueStatement.expr,
                                                     node->location,
                                                     node->data.breakContinueStatement.isBreak ? "break" : "continue");
        case ZR_AST_OUT_STATEMENT:
            return plugin_guard_scan_flow_expression(scan, node->data.outStatement.expr, node->location, "out");
        case ZR_AST_EXPRESSION_STATEMENT:
            if (node->data.expressionStatement.expr != ZR_NULL &&
                node->data.expressionStatement.expr->type == ZR_AST_ASSIGNMENT_EXPRESSION) {
                return plugin_guard_scan_expression_side_effects(scan, node->data.expressionStatement.expr);
            }
            if (plugin_guard_expression_contains_lambda_capture(scan, node->data.expressionStatement.expr)) {
                return plugin_guard_report_escape(scan,
                                                  node->data.expressionStatement.expr->location,
                                                  "closure capture");
            }
            if (plugin_guard_expression_passes_scoped_value_as_argument(scan, node->data.expressionStatement.expr)) {
                return plugin_guard_report_escape(scan,
                                                  node->data.expressionStatement.expr != ZR_NULL
                                                          ? node->data.expressionStatement.expr->location
                                                          : node->location,
                                                  "call argument");
            }
            return ZR_TRUE;
        case ZR_AST_FUNCTION_DECLARATION:
            if (!plugin_guard_scan_decorator_array(scan, node->data.functionDeclaration.decorators)) {
                return ZR_FALSE;
            }
            if (!plugin_guard_scan_function_signature(scan,
                                                      node->data.functionDeclaration.generic,
                                                      node->data.functionDeclaration.params,
                                                      node->data.functionDeclaration.args,
                                                      node->data.functionDeclaration.returnType,
                                                      node->location)) {
                return ZR_FALSE;
            }
            if (plugin_guard_callable_body_contains_scoped_value(scan,
                                                                 node->data.functionDeclaration.params,
                                                                 node->data.functionDeclaration.args,
                                                                 node->data.functionDeclaration.body)) {
                return plugin_guard_report_escape(scan, node->location, "nested function capture");
            }
            return ZR_TRUE;
        case ZR_AST_CLASS_DECLARATION:
            return plugin_guard_scan_decorator_array(scan, node->data.classDeclaration.decorators) &&
                   plugin_guard_scan_decorated_members(scan, node->data.classDeclaration.members);
        case ZR_AST_STRUCT_DECLARATION:
            return plugin_guard_scan_decorator_array(scan, node->data.structDeclaration.decorators) &&
                   plugin_guard_scan_decorated_members(scan, node->data.structDeclaration.members);
        case ZR_AST_ENUM_DECLARATION:
            return plugin_guard_scan_decorator_array(scan, node->data.enumDeclaration.decorators) &&
                   plugin_guard_scan_decorated_members(scan, node->data.enumDeclaration.members);
        case ZR_AST_UNION_DECLARATION:
            return plugin_guard_scan_decorator_array(scan, node->data.unionDeclaration.decorators) &&
                   plugin_guard_scan_decorated_members(scan, node->data.unionDeclaration.variants);
        case ZR_AST_UNION_VARIANT:
            return plugin_guard_scan_decorator_array(scan, node->data.unionVariant.decorators);
        case ZR_AST_ENUM_MEMBER:
            return plugin_guard_scan_decorator_array(scan, node->data.enumMember.decorators);
        case ZR_AST_STRUCT_FIELD:
            return plugin_guard_scan_decorator_array(scan, node->data.structField.decorators) &&
                   plugin_guard_scan_expression_side_effects(scan, node->data.structField.init);
        case ZR_AST_CLASS_FIELD:
            return plugin_guard_scan_decorator_array(scan, node->data.classField.decorators) &&
                   plugin_guard_scan_expression_side_effects(scan, node->data.classField.init);
        case ZR_AST_STRUCT_METHOD:
            if (!plugin_guard_scan_decorator_array(scan, node->data.structMethod.decorators)) {
                return ZR_FALSE;
            }
            if (!plugin_guard_scan_function_signature(scan,
                                                      node->data.structMethod.generic,
                                                      node->data.structMethod.params,
                                                      node->data.structMethod.args,
                                                      node->data.structMethod.returnType,
                                                      node->location)) {
                return ZR_FALSE;
            }
            if (plugin_guard_callable_body_contains_scoped_value(scan,
                                                                 node->data.structMethod.params,
                                                                 node->data.structMethod.args,
                                                                 node->data.structMethod.body)) {
                return plugin_guard_report_escape(scan, node->location, "nested method capture");
            }
            return ZR_TRUE;
        case ZR_AST_CLASS_METHOD:
            if (!plugin_guard_scan_decorator_array(scan, node->data.classMethod.decorators)) {
                return ZR_FALSE;
            }
            if (!plugin_guard_scan_function_signature(scan,
                                                      node->data.classMethod.generic,
                                                      node->data.classMethod.params,
                                                      node->data.classMethod.args,
                                                      node->data.classMethod.returnType,
                                                      node->location)) {
                return ZR_FALSE;
            }
            if (plugin_guard_callable_body_contains_scoped_value(scan,
                                                                 node->data.classMethod.params,
                                                                 node->data.classMethod.args,
                                                                 node->data.classMethod.body)) {
                return plugin_guard_report_escape(scan, node->location, "nested method capture");
            }
            return ZR_TRUE;
        case ZR_AST_STRUCT_META_FUNCTION:
            if (!plugin_guard_scan_function_signature(scan,
                                                      ZR_NULL,
                                                      node->data.structMetaFunction.params,
                                                      node->data.structMetaFunction.args,
                                                      node->data.structMetaFunction.returnType,
                                                      node->location)) {
                return ZR_FALSE;
            }
            if (plugin_guard_callable_body_contains_scoped_value(scan,
                                                                 node->data.structMetaFunction.params,
                                                                 node->data.structMetaFunction.args,
                                                                 node->data.structMetaFunction.body)) {
                return plugin_guard_report_escape(scan, node->location, "nested method capture");
            }
            return ZR_TRUE;
        case ZR_AST_CLASS_META_FUNCTION:
            if (!plugin_guard_scan_argument_array(scan, node->data.classMetaFunction.superArgs, "call argument")) {
                return ZR_FALSE;
            }
            if (!plugin_guard_scan_function_signature(scan,
                                                      ZR_NULL,
                                                      node->data.classMetaFunction.params,
                                                      node->data.classMetaFunction.args,
                                                      node->data.classMetaFunction.returnType,
                                                      node->location)) {
                return ZR_FALSE;
            }
            if (plugin_guard_callable_body_contains_scoped_value(scan,
                                                                 node->data.classMetaFunction.params,
                                                                 node->data.classMetaFunction.args,
                                                                 node->data.classMetaFunction.body)) {
                return plugin_guard_report_escape(scan, node->location, "nested method capture");
            }
            return ZR_TRUE;
        case ZR_AST_CLASS_PROPERTY:
            return plugin_guard_scan_decorator_array(scan, node->data.classProperty.decorators) &&
                   plugin_guard_scan_statement(scan, node->data.classProperty.modifier);
        case ZR_AST_IF_EXPRESSION:
            if (!plugin_guard_scan_expression_boundary(scan, node->data.ifExpression.condition)) {
                return ZR_FALSE;
            }
            return plugin_guard_scan_statement(scan, node->data.ifExpression.thenExpr) &&
                   plugin_guard_scan_statement(scan, node->data.ifExpression.elseExpr);
        case ZR_AST_SWITCH_EXPRESSION:
            if (!plugin_guard_scan_expression_boundary(scan, node->data.switchExpression.expr)) {
                return ZR_FALSE;
            }
            if (node->data.switchExpression.cases != ZR_NULL &&
                !plugin_guard_scan_statement_array(scan, node->data.switchExpression.cases)) {
                return ZR_FALSE;
            }
            return plugin_guard_scan_statement(scan, node->data.switchExpression.defaultCase);
        case ZR_AST_SWITCH_CASE:
            return plugin_guard_scan_expression_boundary(scan, node->data.switchCase.value) &&
                   plugin_guard_scan_statement(scan, node->data.switchCase.block);
        case ZR_AST_SWITCH_DEFAULT:
            return plugin_guard_scan_statement(scan, node->data.switchDefault.block);
        case ZR_AST_WHILE_LOOP:
            return plugin_guard_scan_expression_boundary(scan, node->data.whileLoop.cond) &&
                   plugin_guard_scan_statement(scan, node->data.whileLoop.block);
        case ZR_AST_FOR_LOOP: {
            TZrSize localLength = scan->localNames.length;
            TZrSize pluginLength = scan->pluginNames.length;
            TZrBool ok = plugin_guard_scan_statement(scan, node->data.forLoop.init) &&
                         plugin_guard_scan_expression_boundary(scan, node->data.forLoop.cond) &&
                         plugin_guard_scan_expression_boundary(scan, node->data.forLoop.step) &&
                         plugin_guard_scan_statement(scan, node->data.forLoop.block);
            plugin_guard_leave_nested_region(scan, localLength, pluginLength, ok);
            return ok;
        }
        case ZR_AST_FOREACH_LOOP: {
            TZrSize localLength = scan->localNames.length;
            TZrSize pluginLength = scan->pluginNames.length;
            TZrBool ok = plugin_guard_scan_type(scan,
                                                node->data.foreachLoop.typeInfo,
                                                node->location,
                                                "signature type") &&
                         plugin_guard_scan_expression_boundary(scan, node->data.foreachLoop.expr) &&
                         plugin_guard_scan_statement(scan, node->data.foreachLoop.block);
            plugin_guard_leave_nested_region(scan, localLength, pluginLength, ok);
            return ok;
        }
        case ZR_AST_USING_STATEMENT:
            if (node->data.usingStatement.body != ZR_NULL &&
                !plugin_guard_scan_statement(scan, node->data.usingStatement.body)) {
                return ZR_FALSE;
            }
            return plugin_guard_scan_statement(scan, node->data.usingStatement.elseBody);
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            return plugin_guard_scan_statement(scan, node->data.tryCatchFinallyStatement.block) &&
                   plugin_guard_scan_statement_array(scan, node->data.tryCatchFinallyStatement.catchClauses) &&
                   plugin_guard_scan_statement(scan, node->data.tryCatchFinallyStatement.finallyBlock);
        case ZR_AST_CATCH_CLAUSE: {
            TZrSize localLength = scan->localNames.length;
            TZrSize pluginLength = scan->pluginNames.length;
            TZrBool ok = plugin_guard_scan_statement(scan, node->data.catchClause.block);
            plugin_guard_leave_nested_region(scan, localLength, pluginLength, ok);
            return ok;
        }
        default:
            return ZR_TRUE;
    }
}
