#include "cfg_internal.h"

typedef struct SZrParserCfgThrowTypeProfile {
    TZrUInt32 knownKindMask;
    TZrBool hasUnknownSource;
} SZrParserCfgThrowTypeProfile;

static void cfg_node_collect_throw_type_profile(
        SZrAstNode *node,
        SZrParserCfgThrowTypeProfile *profile,
        const SZrParserCfgThrowTypeBinding *bindings);
static TZrBool cfg_node_throw_kind_mask(SZrAstNode *node,
                                        const SZrParserCfgThrowTypeBinding *bindings,
                                        TZrUInt32 *outKnownKindMask);

static TZrBool cfg_node_literal_throw_kind(SZrAstNode *node,
                                           EZrParserCfgThrowKind *outKind) {
    TZrBool boolValue = ZR_FALSE;

    if (outKind != ZR_NULL) {
        *outKind = ZR_PARSER_CFG_THROW_KIND_UNKNOWN;
    }
    if (node == ZR_NULL || outKind == ZR_NULL) {
        return ZR_FALSE;
    }

    if (cfg_node_bool_constant(node, &boolValue)) {
        *outKind = ZR_PARSER_CFG_THROW_KIND_BOOL;
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_INTEGER_LITERAL:
            *outKind = ZR_PARSER_CFG_THROW_KIND_INTEGER;
            return ZR_TRUE;
        case ZR_AST_STRING_LITERAL:
            if (node->data.stringLiteral.hasError ||
                node->data.stringLiteral.value == ZR_NULL) {
                return ZR_FALSE;
            }
            *outKind = ZR_PARSER_CFG_THROW_KIND_STRING;
            return ZR_TRUE;
        case ZR_AST_CHAR_LITERAL:
            if (node->data.charLiteral.hasError) {
                return ZR_FALSE;
            }
            *outKind = ZR_PARSER_CFG_THROW_KIND_CHAR;
            return ZR_TRUE;
        case ZR_AST_FLOAT_LITERAL:
            *outKind = ZR_PARSER_CFG_THROW_KIND_FLOAT;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool cfg_node_throw_kind_mask(SZrAstNode *node,
                                        const SZrParserCfgThrowTypeBinding *bindings,
                                        TZrUInt32 *outKnownKindMask) {
    EZrParserCfgThrowKind kind = ZR_PARSER_CFG_THROW_KIND_UNKNOWN;
    SZrString *typeName;
    SZrString *identifierName;

    if (outKnownKindMask != ZR_NULL) {
        *outKnownKindMask = 0u;
    }
    if (node == ZR_NULL || outKnownKindMask == ZR_NULL) {
        return ZR_FALSE;
    }

    if (cfg_node_literal_throw_kind(node, &kind)) {
        *outKnownKindMask = cfg_throw_kind_mask(kind);
        return ZR_TRUE;
    }

    if (node->type == ZR_AST_TYPE_CAST_EXPRESSION) {
        typeName = cfg_type_info_simple_name(node->data.typeCastExpression.targetType);
        if (!cfg_type_name_throw_kind(typeName, &kind)) {
            return ZR_FALSE;
        }
        *outKnownKindMask = cfg_throw_kind_mask(kind);
        return ZR_TRUE;
    }

    if (cfg_node_identifier_name(node, &identifierName)) {
        return cfg_throw_type_binding_lookup(bindings, identifierName, outKnownKindMask);
    }

    return ZR_FALSE;
}

static TZrBool cfg_node_array_may_enter_catch(SZrAstNodeArray *nodes) {
    TZrSize index;

    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < nodes->count; index++) {
        if (cfg_node_may_enter_catch(nodes->nodes[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool cfg_primary_expression_single_direct_lambda_iife(
        SZrAstNode *node,
        SZrAstNode **outLambdaNode,
        SZrAstNode **outCallNode) {
    SZrAstNodeArray *members;
    SZrAstNode *callNode;

    if (outLambdaNode != ZR_NULL) {
        *outLambdaNode = ZR_NULL;
    }
    if (outCallNode != ZR_NULL) {
        *outCallNode = ZR_NULL;
    }
    if (node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION ||
        node->data.primaryExpression.property == ZR_NULL ||
        node->data.primaryExpression.property->type != ZR_AST_LAMBDA_EXPRESSION) {
        return ZR_FALSE;
    }

    members = node->data.primaryExpression.members;
    if (members == ZR_NULL || members->nodes == ZR_NULL || members->count != 1) {
        return ZR_FALSE;
    }

    callNode = members->nodes[0];
    if (callNode == ZR_NULL || callNode->type != ZR_AST_FUNCTION_CALL) {
        return ZR_FALSE;
    }

    if (outLambdaNode != ZR_NULL) {
        *outLambdaNode = node->data.primaryExpression.property;
    }
    if (outCallNode != ZR_NULL) {
        *outCallNode = callNode;
    }
    return ZR_TRUE;
}

static TZrBool cfg_function_call_arguments_may_enter_catch(SZrAstNode *callNode) {
    if (callNode == ZR_NULL || callNode->type != ZR_AST_FUNCTION_CALL) {
        return ZR_FALSE;
    }
    return (TZrBool)(
            cfg_node_array_may_enter_catch(callNode->data.functionCall.args) ||
            cfg_node_array_may_enter_catch(
                    callNode->data.functionCall.genericArguments));
}

static TZrBool cfg_direct_lambda_iife_may_enter_catch(SZrAstNode *node) {
    SZrAstNode *lambdaNode = ZR_NULL;
    SZrAstNode *callNode = ZR_NULL;

    if (!cfg_primary_expression_single_direct_lambda_iife(
            node,
            &lambdaNode,
            &callNode)) {
        return ZR_FALSE;
    }

    return (TZrBool)(
            cfg_function_call_arguments_may_enter_catch(callNode) ||
            cfg_node_may_enter_catch(lambdaNode->data.lambdaExpression.block));
}

TZrBool cfg_node_may_enter_catch(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_THROW_STATEMENT:
            return ZR_TRUE;
        case ZR_AST_FUNCTION_CALL:
        case ZR_AST_IMPORT_EXPRESSION:
        case ZR_AST_CONSTRUCT_EXPRESSION:
            return ZR_TRUE;
        case ZR_AST_LAMBDA_EXPRESSION:
            return ZR_FALSE;
        case ZR_AST_SCRIPT:
            return cfg_node_array_may_enter_catch(node->data.script.statements);
        case ZR_AST_BLOCK:
            return cfg_node_array_may_enter_catch(node->data.block.body);
        case ZR_AST_VARIABLE_DECLARATION:
            return cfg_node_may_enter_catch(node->data.variableDeclaration.value);
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return (TZrBool)(cfg_node_may_enter_catch(node->data.assignmentExpression.left) ||
                             cfg_node_may_enter_catch(node->data.assignmentExpression.right));
        case ZR_AST_BINARY_EXPRESSION:
            return (TZrBool)(cfg_node_may_enter_catch(node->data.binaryExpression.left) ||
                             cfg_node_may_enter_catch(node->data.binaryExpression.right));
        case ZR_AST_LOGICAL_EXPRESSION:
            return (TZrBool)(cfg_node_may_enter_catch(node->data.logicalExpression.left) ||
                             cfg_node_may_enter_catch(node->data.logicalExpression.right));
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return (TZrBool)(cfg_node_may_enter_catch(node->data.conditionalExpression.test) ||
                             cfg_node_may_enter_catch(
                                     node->data.conditionalExpression.consequent) ||
                             cfg_node_may_enter_catch(
                                     node->data.conditionalExpression.alternate));
        case ZR_AST_UNARY_EXPRESSION:
            return cfg_node_may_enter_catch(node->data.unaryExpression.argument);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return cfg_node_may_enter_catch(node->data.typeCastExpression.expression);
        case ZR_AST_EXPRESSION_STATEMENT:
            return cfg_node_may_enter_catch(node->data.expressionStatement.expr);
        case ZR_AST_USING_STATEMENT:
            return (TZrBool)(cfg_node_may_enter_catch(node->data.usingStatement.resource) ||
                             cfg_node_may_enter_catch(node->data.usingStatement.body) ||
                             cfg_node_may_enter_catch(node->data.usingStatement.elseBody));
        case ZR_AST_RETURN_STATEMENT:
            return cfg_node_may_enter_catch(node->data.returnStatement.expr);
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return cfg_node_may_enter_catch(node->data.breakContinueStatement.expr);
        case ZR_AST_OUT_STATEMENT:
            return cfg_node_may_enter_catch(node->data.outStatement.expr);
        case ZR_AST_IF_EXPRESSION:
            return (TZrBool)(cfg_node_may_enter_catch(node->data.ifExpression.condition) ||
                             cfg_node_may_enter_catch(node->data.ifExpression.thenExpr) ||
                             cfg_node_may_enter_catch(node->data.ifExpression.elseExpr));
        case ZR_AST_SWITCH_EXPRESSION:
            return (TZrBool)(
                    cfg_node_may_enter_catch(node->data.switchExpression.expr) ||
                    cfg_node_array_may_enter_catch(node->data.switchExpression.cases) ||
                    cfg_node_may_enter_catch(node->data.switchExpression.defaultCase));
        case ZR_AST_SWITCH_CASE:
            return (TZrBool)(cfg_node_may_enter_catch(node->data.switchCase.value) ||
                             cfg_node_may_enter_catch(node->data.switchCase.block));
        case ZR_AST_SWITCH_DEFAULT:
            return cfg_node_may_enter_catch(node->data.switchDefault.block);
        case ZR_AST_WHILE_LOOP:
            return (TZrBool)(cfg_node_may_enter_catch(node->data.whileLoop.cond) ||
                             cfg_node_may_enter_catch(node->data.whileLoop.block));
        case ZR_AST_FOR_LOOP:
            return (TZrBool)(cfg_node_may_enter_catch(node->data.forLoop.init) ||
                             cfg_node_may_enter_catch(node->data.forLoop.cond) ||
                             cfg_node_may_enter_catch(node->data.forLoop.step) ||
                             cfg_node_may_enter_catch(node->data.forLoop.block));
        case ZR_AST_FOREACH_LOOP:
            return (TZrBool)(cfg_node_may_enter_catch(node->data.foreachLoop.expr) ||
                             cfg_node_may_enter_catch(node->data.foreachLoop.block));
        case ZR_AST_CATCH_CLAUSE:
            return cfg_node_may_enter_catch(node->data.catchClause.block);
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            return (TZrBool)(
                    cfg_node_may_enter_catch(node->data.tryCatchFinallyStatement.block) ||
                    cfg_node_array_may_enter_catch(
                            node->data.tryCatchFinallyStatement.catchClauses) ||
                    cfg_node_may_enter_catch(node->data.tryCatchFinallyStatement.finallyBlock));
        case ZR_AST_PRIMARY_EXPRESSION:
            if (cfg_primary_expression_single_direct_lambda_iife(
                    node,
                    ZR_NULL,
                    ZR_NULL)) {
                return cfg_direct_lambda_iife_may_enter_catch(node);
            }
            return (TZrBool)(cfg_node_may_enter_catch(node->data.primaryExpression.property) ||
                             cfg_node_array_may_enter_catch(
                                     node->data.primaryExpression.members));
        case ZR_AST_MEMBER_EXPRESSION:
            return cfg_node_may_enter_catch(node->data.memberExpression.property);
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            return cfg_node_may_enter_catch(node->data.typeQueryExpression.operand);
        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            return cfg_node_may_enter_catch(node->data.prototypeReferenceExpression.target);
        case ZR_AST_TEMPLATE_STRING_LITERAL:
            return cfg_node_array_may_enter_catch(node->data.templateStringLiteral.segments);
        case ZR_AST_INTERPOLATED_SEGMENT:
            return cfg_node_may_enter_catch(node->data.interpolatedSegment.expression);
        case ZR_AST_ARRAY_LITERAL:
            return cfg_node_array_may_enter_catch(node->data.arrayLiteral.elements);
        case ZR_AST_OBJECT_LITERAL:
            return cfg_node_array_may_enter_catch(node->data.objectLiteral.properties);
        case ZR_AST_KEY_VALUE_PAIR:
            return (TZrBool)(cfg_node_may_enter_catch(node->data.keyValuePair.key) ||
                             cfg_node_may_enter_catch(node->data.keyValuePair.value));
        case ZR_AST_UNPACK_LITERAL:
            return cfg_node_may_enter_catch(node->data.unpackLiteral.element);
        case ZR_AST_GENERATOR_EXPRESSION:
            return cfg_node_may_enter_catch(node->data.generatorExpression.block);
        default:
            return ZR_FALSE;
    }
}

static void cfg_throw_profile_add_known_mask(SZrParserCfgThrowTypeProfile *profile,
                                             TZrUInt32 knownKindMask) {
    if (profile == ZR_NULL) {
        return;
    }
    if (knownKindMask == 0u) {
        profile->hasUnknownSource = ZR_TRUE;
        return;
    }
    profile->knownKindMask |= knownKindMask;
}

static void cfg_node_array_collect_throw_type_profile(
        SZrAstNodeArray *nodes,
        SZrParserCfgThrowTypeProfile *profile,
        const SZrParserCfgThrowTypeBinding *bindings);

static void cfg_function_call_arguments_collect_throw_type_profile(
        SZrAstNode *callNode,
        SZrParserCfgThrowTypeProfile *profile,
        const SZrParserCfgThrowTypeBinding *bindings) {
    if (callNode == ZR_NULL || callNode->type != ZR_AST_FUNCTION_CALL) {
        return;
    }
    cfg_node_array_collect_throw_type_profile(callNode->data.functionCall.args,
                                              profile,
                                              bindings);
    cfg_node_array_collect_throw_type_profile(
            callNode->data.functionCall.genericArguments,
            profile,
            bindings);
}

static void cfg_direct_lambda_iife_collect_throw_type_profile(
        SZrAstNode *node,
        SZrParserCfgThrowTypeProfile *profile,
        const SZrParserCfgThrowTypeBinding *bindings) {
    SZrAstNode *lambdaNode = ZR_NULL;
    SZrAstNode *callNode = ZR_NULL;

    if (!cfg_primary_expression_single_direct_lambda_iife(
            node,
            &lambdaNode,
            &callNode)) {
        return;
    }

    cfg_function_call_arguments_collect_throw_type_profile(callNode,
                                                          profile,
                                                          bindings);
    cfg_node_collect_throw_type_profile(lambdaNode->data.lambdaExpression.block,
                                        profile,
                                        bindings);
}

static void cfg_node_sequence_collect_throw_type_profile(
        SZrAstNodeArray *nodes,
        TZrSize index,
        SZrParserCfgThrowTypeProfile *profile,
        const SZrParserCfgThrowTypeBinding *bindings) {
    SZrAstNode *node;
    SZrParserCfgThrowTypeBinding binding;
    SZrParserCfgThrowTypeBindingArray resultBindings;

    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL || profile == ZR_NULL) {
        return;
    }
    if (index >= nodes->count) {
        return;
    }

    node = nodes->nodes[index];
    if (node != ZR_NULL && node->type == ZR_AST_VARIABLE_DECLARATION) {
        cfg_node_collect_throw_type_profile(node->data.variableDeclaration.value,
                                            profile,
                                            bindings);
        if (cfg_variable_declaration_throw_binding(node, &binding)) {
            binding.next = bindings;
            cfg_node_sequence_collect_throw_type_profile(
                    nodes,
                    index + 1,
                    profile,
                    &binding);
            return;
        }
        cfg_node_sequence_collect_throw_type_profile(
                nodes,
                index + 1,
                profile,
                bindings);
        return;
    }

    if (cfg_assignment_throw_binding(node,
                                     bindings,
                                     cfg_node_throw_kind_mask,
                                     &binding)) {
        cfg_node_collect_throw_type_profile(node, profile, bindings);
        binding.next = bindings;
        cfg_node_sequence_collect_throw_type_profile(
                nodes,
                index + 1,
                profile,
                &binding);
        return;
    }

    if (node != ZR_NULL &&
        cfg_node_result_throw_binding_capacity(node) > 0 &&
        cfg_throw_type_binding_array_init(
                &resultBindings,
                cfg_node_result_throw_binding_capacity(node))) {
        if (cfg_node_collect_result_throw_bindings(node,
                                                  bindings,
                                                  cfg_node_throw_kind_mask,
                                                  &resultBindings) &&
            resultBindings.count > 0) {
            const SZrParserCfgThrowTypeBinding *resultChain =
                    cfg_throw_type_binding_array_chain_from(&resultBindings, bindings);

            cfg_node_collect_throw_type_profile(node, profile, bindings);
            cfg_node_sequence_collect_throw_type_profile(
                    nodes,
                    index + 1,
                    profile,
                    resultChain);
            cfg_throw_type_binding_array_free(&resultBindings);
            return;
        }
        cfg_throw_type_binding_array_free(&resultBindings);
    }

    cfg_node_collect_throw_type_profile(node, profile, bindings);
    cfg_node_sequence_collect_throw_type_profile(
            nodes,
            index + 1,
            profile,
            bindings);
}

static void cfg_node_array_collect_throw_type_profile(
        SZrAstNodeArray *nodes,
        SZrParserCfgThrowTypeProfile *profile,
        const SZrParserCfgThrowTypeBinding *bindings) {
    cfg_node_sequence_collect_throw_type_profile(nodes, 0, profile, bindings);
}

static void cfg_node_collect_throw_type_profile(
        SZrAstNode *node,
        SZrParserCfgThrowTypeProfile *profile,
        const SZrParserCfgThrowTypeBinding *bindings) {
    TZrUInt32 knownKindMask = 0u;

    if (node == ZR_NULL || profile == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_THROW_STATEMENT:
            cfg_node_collect_throw_type_profile(node->data.throwStatement.expr,
                                                profile,
                                                bindings);
            if (cfg_node_throw_kind_mask(node->data.throwStatement.expr,
                                         bindings,
                                         &knownKindMask)) {
                cfg_throw_profile_add_known_mask(profile, knownKindMask);
            } else {
                profile->hasUnknownSource = ZR_TRUE;
            }
            return;
        case ZR_AST_FUNCTION_CALL:
        case ZR_AST_IMPORT_EXPRESSION:
        case ZR_AST_CONSTRUCT_EXPRESSION:
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            profile->hasUnknownSource = ZR_TRUE;
            return;
        case ZR_AST_LAMBDA_EXPRESSION:
            return;
        case ZR_AST_SCRIPT:
            cfg_node_array_collect_throw_type_profile(node->data.script.statements,
                                                      profile,
                                                      bindings);
            return;
        case ZR_AST_BLOCK:
            cfg_node_array_collect_throw_type_profile(node->data.block.body,
                                                      profile,
                                                      bindings);
            return;
        case ZR_AST_VARIABLE_DECLARATION:
            cfg_node_collect_throw_type_profile(node->data.variableDeclaration.value,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            cfg_node_collect_throw_type_profile(node->data.assignmentExpression.left,
                                                profile,
                                                bindings);
            cfg_node_collect_throw_type_profile(node->data.assignmentExpression.right,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_BINARY_EXPRESSION:
            cfg_node_collect_throw_type_profile(node->data.binaryExpression.left,
                                                profile,
                                                bindings);
            cfg_node_collect_throw_type_profile(node->data.binaryExpression.right,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_LOGICAL_EXPRESSION:
            cfg_node_collect_throw_type_profile(node->data.logicalExpression.left,
                                                profile,
                                                bindings);
            cfg_node_collect_throw_type_profile(node->data.logicalExpression.right,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_CONDITIONAL_EXPRESSION:
            cfg_node_collect_throw_type_profile(node->data.conditionalExpression.test,
                                                profile,
                                                bindings);
            cfg_node_collect_throw_type_profile(node->data.conditionalExpression.consequent,
                                                profile,
                                                bindings);
            cfg_node_collect_throw_type_profile(node->data.conditionalExpression.alternate,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_UNARY_EXPRESSION:
            cfg_node_collect_throw_type_profile(node->data.unaryExpression.argument,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_TYPE_CAST_EXPRESSION:
            cfg_node_collect_throw_type_profile(node->data.typeCastExpression.expression,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_EXPRESSION_STATEMENT:
            cfg_node_collect_throw_type_profile(node->data.expressionStatement.expr,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_USING_STATEMENT:
            cfg_node_collect_throw_type_profile(node->data.usingStatement.resource,
                                                profile,
                                                bindings);
            cfg_node_collect_throw_type_profile(node->data.usingStatement.body,
                                                profile,
                                                bindings);
            cfg_node_collect_throw_type_profile(node->data.usingStatement.elseBody,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_RETURN_STATEMENT:
            cfg_node_collect_throw_type_profile(node->data.returnStatement.expr,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            cfg_node_collect_throw_type_profile(node->data.breakContinueStatement.expr,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_OUT_STATEMENT:
            cfg_node_collect_throw_type_profile(node->data.outStatement.expr,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_IF_EXPRESSION:
            cfg_node_collect_throw_type_profile(node->data.ifExpression.condition,
                                                profile,
                                                bindings);
            cfg_node_collect_throw_type_profile(node->data.ifExpression.thenExpr,
                                                profile,
                                                bindings);
            cfg_node_collect_throw_type_profile(node->data.ifExpression.elseExpr,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_SWITCH_EXPRESSION:
            cfg_node_collect_throw_type_profile(node->data.switchExpression.expr,
                                                profile,
                                                bindings);
            cfg_node_array_collect_throw_type_profile(node->data.switchExpression.cases,
                                                      profile,
                                                      bindings);
            cfg_node_collect_throw_type_profile(node->data.switchExpression.defaultCase,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_SWITCH_CASE:
            cfg_node_collect_throw_type_profile(node->data.switchCase.value,
                                                profile,
                                                bindings);
            cfg_node_collect_throw_type_profile(node->data.switchCase.block,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_SWITCH_DEFAULT:
            cfg_node_collect_throw_type_profile(node->data.switchDefault.block,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_WHILE_LOOP:
            cfg_node_collect_throw_type_profile(node->data.whileLoop.cond,
                                                profile,
                                                bindings);
            cfg_node_collect_throw_type_profile(node->data.whileLoop.block,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_FOR_LOOP:
            cfg_node_collect_throw_type_profile(node->data.forLoop.init,
                                                profile,
                                                bindings);
            cfg_node_collect_throw_type_profile(node->data.forLoop.cond,
                                                profile,
                                                bindings);
            cfg_node_collect_throw_type_profile(node->data.forLoop.step,
                                                profile,
                                                bindings);
            cfg_node_collect_throw_type_profile(node->data.forLoop.block,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_FOREACH_LOOP:
            cfg_node_collect_throw_type_profile(node->data.foreachLoop.expr,
                                                profile,
                                                bindings);
            cfg_node_collect_throw_type_profile(node->data.foreachLoop.block,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_CATCH_CLAUSE:
            cfg_node_collect_throw_type_profile(node->data.catchClause.block,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_PRIMARY_EXPRESSION:
            if (cfg_primary_expression_single_direct_lambda_iife(
                    node,
                    ZR_NULL,
                    ZR_NULL)) {
                cfg_direct_lambda_iife_collect_throw_type_profile(node,
                                                                  profile,
                                                                  bindings);
                return;
            }
            cfg_node_collect_throw_type_profile(node->data.primaryExpression.property,
                                                profile,
                                                bindings);
            cfg_node_array_collect_throw_type_profile(node->data.primaryExpression.members,
                                                      profile,
                                                      bindings);
            return;
        case ZR_AST_MEMBER_EXPRESSION:
            cfg_node_collect_throw_type_profile(node->data.memberExpression.property,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            cfg_node_collect_throw_type_profile(node->data.typeQueryExpression.operand,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            cfg_node_collect_throw_type_profile(node->data.prototypeReferenceExpression.target,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_TEMPLATE_STRING_LITERAL:
            cfg_node_array_collect_throw_type_profile(node->data.templateStringLiteral.segments,
                                                      profile,
                                                      bindings);
            return;
        case ZR_AST_INTERPOLATED_SEGMENT:
            cfg_node_collect_throw_type_profile(node->data.interpolatedSegment.expression,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_ARRAY_LITERAL:
            cfg_node_array_collect_throw_type_profile(node->data.arrayLiteral.elements,
                                                      profile,
                                                      bindings);
            return;
        case ZR_AST_OBJECT_LITERAL:
            cfg_node_array_collect_throw_type_profile(node->data.objectLiteral.properties,
                                                      profile,
                                                      bindings);
            return;
        case ZR_AST_KEY_VALUE_PAIR:
            cfg_node_collect_throw_type_profile(node->data.keyValuePair.key,
                                                profile,
                                                bindings);
            cfg_node_collect_throw_type_profile(node->data.keyValuePair.value,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_UNPACK_LITERAL:
            cfg_node_collect_throw_type_profile(node->data.unpackLiteral.element,
                                                profile,
                                                bindings);
            return;
        case ZR_AST_GENERATOR_EXPRESSION:
            cfg_node_collect_throw_type_profile(node->data.generatorExpression.block,
                                                profile,
                                                bindings);
            return;
        default:
            return;
    }
}

TZrBool cfg_try_body_has_single_known_throw_kind(
        SZrAstNode *body,
        EZrParserCfgThrowKind *outKind) {
    TZrUInt32 knownKindMask;
    TZrBool hasUnknownSource;

    if (outKind != ZR_NULL) {
        *outKind = ZR_PARSER_CFG_THROW_KIND_UNKNOWN;
    }
    if (body == ZR_NULL || outKind == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!cfg_try_body_throw_profile(body, &knownKindMask, &hasUnknownSource) ||
        hasUnknownSource) {
        return ZR_FALSE;
    }

    return cfg_throw_kind_mask_has_single_kind(knownKindMask, outKind);
}

TZrBool cfg_try_body_throw_profile(SZrAstNode *body,
                                   TZrUInt32 *outKnownKindMask,
                                   TZrBool *outHasUnknownSource) {
    SZrParserCfgThrowTypeProfile profile;

    if (outKnownKindMask != ZR_NULL) {
        *outKnownKindMask = 0u;
    }
    if (outHasUnknownSource != ZR_NULL) {
        *outHasUnknownSource = ZR_FALSE;
    }
    if (body == ZR_NULL || outKnownKindMask == ZR_NULL ||
        outHasUnknownSource == ZR_NULL) {
        return ZR_FALSE;
    }

    profile.knownKindMask = 0u;
    profile.hasUnknownSource = ZR_FALSE;
    cfg_node_collect_throw_type_profile(body, &profile, ZR_NULL);

    *outKnownKindMask = profile.knownKindMask;
    *outHasUnknownSource = profile.hasUnknownSource;
    return ZR_TRUE;
}
