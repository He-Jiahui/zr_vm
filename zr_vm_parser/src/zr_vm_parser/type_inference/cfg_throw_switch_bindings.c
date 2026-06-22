#include "cfg_internal.h"

static TZrSize cfg_switch_case_array_result_throw_binding_capacity(
        SZrAstNodeArray *cases) {
    TZrSize capacity = 0;
    TZrSize index;

    if (cases == ZR_NULL || cases->nodes == ZR_NULL) {
        return 0;
    }

    for (index = 0; index < cases->count; index++) {
        SZrAstNode *caseNode = cases->nodes[index];

        if (caseNode != ZR_NULL && caseNode->type == ZR_AST_SWITCH_CASE) {
            capacity += cfg_node_result_throw_binding_capacity(
                    caseNode->data.switchCase.block);
        }
    }

    return capacity;
}

TZrSize cfg_switch_expression_result_throw_binding_capacity(SZrAstNode *node) {
    TZrSize capacity;

    if (node == ZR_NULL || node->type != ZR_AST_SWITCH_EXPRESSION) {
        return 0;
    }

    capacity = cfg_switch_case_array_result_throw_binding_capacity(
            node->data.switchExpression.cases);
    if (node->data.switchExpression.defaultCase != ZR_NULL &&
        node->data.switchExpression.defaultCase->type == ZR_AST_SWITCH_DEFAULT) {
        capacity += cfg_node_result_throw_binding_capacity(
                node->data.switchExpression.defaultCase->data.switchDefault.block);
    }
    return capacity;
}

static TZrBool cfg_switch_binding_array_merge_missing_with_incoming(
        SZrParserCfgThrowTypeBindingArray *switchBindings,
        const SZrParserCfgThrowTypeBindingArray *alternativeBindings,
        const SZrParserCfgThrowTypeBinding *bindings) {
    TZrSize index;

    if (switchBindings == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < switchBindings->count; index++) {
        SZrParserCfgThrowTypeBinding *binding = &switchBindings->items[index];

        if (!cfg_throw_type_binding_array_find(alternativeBindings,
                                               binding->name,
                                               ZR_NULL) &&
            !cfg_throw_type_binding_merge_with_incoming(bindings, binding)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool cfg_switch_binding_array_merge_alternative(
        SZrParserCfgThrowTypeBindingArray *switchBindings,
        const SZrParserCfgThrowTypeBindingArray *alternativeBindings,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrBool hasPriorAlternative) {
    TZrSize index;

    if (!cfg_switch_binding_array_merge_missing_with_incoming(switchBindings,
                                                              alternativeBindings,
                                                              bindings)) {
        return ZR_FALSE;
    }

    if (alternativeBindings == ZR_NULL) {
        return ZR_TRUE;
    }

    for (index = 0; index < alternativeBindings->count; index++) {
        SZrParserCfgThrowTypeBinding mergedBinding =
                alternativeBindings->items[index];
        const SZrParserCfgThrowTypeBinding *switchChain =
                cfg_throw_type_binding_array_chain_from(switchBindings, bindings);

        if (!cfg_throw_type_binding_array_find(switchBindings,
                                               mergedBinding.name,
                                               ZR_NULL) &&
            hasPriorAlternative &&
            !cfg_throw_type_binding_merge_with_incoming(bindings,
                                                        &mergedBinding)) {
            return ZR_FALSE;
        }
        if (!cfg_throw_type_binding_array_append_or_merge_alternative(
                    switchBindings,
                    &mergedBinding,
                    &switchChain)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool cfg_switch_statement_stops_result_flow(SZrAstNode *node) {
    return (TZrBool)(node != ZR_NULL &&
                     (node->type == ZR_AST_RETURN_STATEMENT ||
                      node->type == ZR_AST_THROW_STATEMENT ||
                      node->type == ZR_AST_BREAK_CONTINUE_STATEMENT));
}

static TZrBool cfg_switch_body_can_fallthrough(SZrAstNode *node);

static TZrBool cfg_switch_node_array_can_fallthrough(SZrAstNodeArray *nodes) {
    TZrSize index;

    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return ZR_TRUE;
    }

    for (index = 0; index < nodes->count; index++) {
        if (!cfg_switch_body_can_fallthrough(nodes->nodes[index])) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool cfg_switch_body_can_fallthrough(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_TRUE;
    }
    if (cfg_switch_statement_stops_result_flow(node)) {
        return ZR_FALSE;
    }
    if (node->type == ZR_AST_BLOCK) {
        return cfg_switch_node_array_can_fallthrough(node->data.block.body);
    }
    if (node->type == ZR_AST_SCRIPT) {
        return cfg_switch_node_array_can_fallthrough(node->data.script.statements);
    }
    if (node->type == ZR_AST_IF_EXPRESSION) {
        TZrBool conditionValue = ZR_FALSE;

        if (cfg_node_bool_constant(node->data.ifExpression.condition,
                                   &conditionValue)) {
            return cfg_switch_body_can_fallthrough(
                    conditionValue ? node->data.ifExpression.thenExpr
                                   : node->data.ifExpression.elseExpr);
        }

        TZrBool thenFalls = cfg_switch_body_can_fallthrough(
                node->data.ifExpression.thenExpr);
        TZrBool elseFalls = node->data.ifExpression.elseExpr == ZR_NULL
                                    ? ZR_TRUE
                                    : cfg_switch_body_can_fallthrough(
                                              node->data.ifExpression.elseExpr);
        return (TZrBool)(thenFalls || elseFalls);
    }

    return ZR_TRUE;
}

static TZrBool cfg_switch_collect_body_alternative(
        SZrAstNode *body,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        SZrParserCfgThrowTypeBindingArray *switchBindings,
        TZrBool hasPriorAlternative,
        TZrBool *outCollectedAlternative) {
    SZrParserCfgThrowTypeBindingArray alternativeBindings;
    TZrBool ok = ZR_FALSE;

    if (outCollectedAlternative != ZR_NULL) {
        *outCollectedAlternative = ZR_FALSE;
    }
    if (!cfg_switch_body_can_fallthrough(body)) {
        return ZR_TRUE;
    }
    if (!cfg_throw_type_binding_array_init(
                &alternativeBindings,
                cfg_node_result_throw_binding_capacity(body))) {
        return ZR_FALSE;
    }

    if (!cfg_node_collect_result_throw_bindings(body,
                                               bindings,
                                               resolveThrowKindMask,
                                               &alternativeBindings)) {
        goto cleanup;
    }

    ok = cfg_switch_binding_array_merge_alternative(switchBindings,
                                                    &alternativeBindings,
                                                    bindings,
                                                    hasPriorAlternative);
    if (ok && outCollectedAlternative != ZR_NULL) {
        *outCollectedAlternative = ZR_TRUE;
    }

cleanup:
    cfg_throw_type_binding_array_free(&alternativeBindings);
    return ok;
}

static TZrBool cfg_switch_apply_merged_bindings(
        SZrParserCfgThrowTypeBindingArray *outBindings,
        const SZrParserCfgThrowTypeBindingArray *switchBindings,
        const SZrParserCfgThrowTypeBinding *bindings) {
    TZrSize index;

    if (outBindings == ZR_NULL || switchBindings == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < switchBindings->count; index++) {
        const SZrParserCfgThrowTypeBinding *outChain =
                cfg_throw_type_binding_array_chain_from(outBindings, bindings);

        if (!cfg_throw_type_binding_array_append_or_replace(
                    outBindings,
                    &switchBindings->items[index],
                    &outChain)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool cfg_switch_collect_constant_selected_alternative(
        SZrAstNode *node,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        SZrParserCfgThrowTypeBindingArray *switchBindings,
        TZrBool *outHandled) {
    SZrParserCfgConstant selectorValue;
    SZrAstNodeArray *cases;
    TZrSize caseIndex;

    if (outHandled != ZR_NULL) {
        *outHandled = ZR_FALSE;
    }
    if (node == ZR_NULL || node->type != ZR_AST_SWITCH_EXPRESSION ||
        switchBindings == ZR_NULL || outHandled == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!cfg_node_constant(node->data.switchExpression.expr, &selectorValue)) {
        return ZR_TRUE;
    }

    cases = node->data.switchExpression.cases;
    if (cases != ZR_NULL && cases->nodes != ZR_NULL) {
        for (caseIndex = 0; caseIndex < cases->count; caseIndex++) {
            SZrAstNode *caseNode = cases->nodes[caseIndex];
            SZrParserCfgConstant caseValue;

            if (caseNode == ZR_NULL || caseNode->type != ZR_AST_SWITCH_CASE) {
                continue;
            }
            if (!cfg_node_constant(caseNode->data.switchCase.value, &caseValue) ||
                !cfg_constants_can_compare(&selectorValue, &caseValue)) {
                return ZR_TRUE;
            }
            if (!cfg_constants_equal(&selectorValue, &caseValue)) {
                continue;
            }

            *outHandled = ZR_TRUE;
            return cfg_switch_collect_body_alternative(
                    caseNode->data.switchCase.block,
                    bindings,
                    resolveThrowKindMask,
                    switchBindings,
                    ZR_FALSE,
                    ZR_NULL);
        }
    }

    *outHandled = ZR_TRUE;
    if (node->data.switchExpression.defaultCase != ZR_NULL &&
        node->data.switchExpression.defaultCase->type == ZR_AST_SWITCH_DEFAULT) {
        return cfg_switch_collect_body_alternative(
                node->data.switchExpression.defaultCase->data.switchDefault.block,
                bindings,
                resolveThrowKindMask,
                switchBindings,
                ZR_FALSE,
                ZR_NULL);
    }

    return ZR_TRUE;
}

TZrBool cfg_switch_expression_merged_throw_binding(
        SZrAstNode *node,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        SZrParserCfgThrowTypeBindingArray *outBindings) {
    SZrParserCfgThrowTypeBindingArray switchBindings;
    SZrAstNodeArray *cases;
    TZrSize caseIndex;
    TZrBool hasAlternative;
    TZrBool handledConstantSelection = ZR_FALSE;
    TZrBool ok = ZR_FALSE;

    if (node == ZR_NULL || node->type != ZR_AST_SWITCH_EXPRESSION ||
        outBindings == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!cfg_throw_type_binding_array_init(
                &switchBindings,
                cfg_switch_expression_result_throw_binding_capacity(node))) {
        return ZR_FALSE;
    }

    if (!cfg_switch_collect_constant_selected_alternative(
                node,
                bindings,
                resolveThrowKindMask,
                &switchBindings,
                &handledConstantSelection)) {
        goto cleanup;
    }
    if (handledConstantSelection) {
        ok = (TZrBool)(switchBindings.count > 0 &&
                       cfg_switch_apply_merged_bindings(outBindings,
                                                        &switchBindings,
                                                        bindings));
        goto cleanup;
    }

    hasAlternative = (TZrBool)(node->data.switchExpression.defaultCase == ZR_NULL);
    cases = node->data.switchExpression.cases;
    if (cases != ZR_NULL && cases->nodes != ZR_NULL) {
        for (caseIndex = 0; caseIndex < cases->count; caseIndex++) {
            SZrAstNode *caseNode = cases->nodes[caseIndex];
            TZrBool collectedAlternative = ZR_FALSE;

            if (caseNode == ZR_NULL || caseNode->type != ZR_AST_SWITCH_CASE) {
                continue;
            }
            if (!cfg_switch_collect_body_alternative(
                        caseNode->data.switchCase.block,
                        bindings,
                        resolveThrowKindMask,
                        &switchBindings,
                        hasAlternative,
                        &collectedAlternative)) {
                goto cleanup;
            }
            if (collectedAlternative) {
                hasAlternative = ZR_TRUE;
            }
        }
    }

    if (node->data.switchExpression.defaultCase != ZR_NULL &&
        node->data.switchExpression.defaultCase->type == ZR_AST_SWITCH_DEFAULT) {
        TZrBool collectedAlternative = ZR_FALSE;

        if (!cfg_switch_collect_body_alternative(
                    node->data.switchExpression.defaultCase->data.switchDefault.block,
                    bindings,
                    resolveThrowKindMask,
                    &switchBindings,
                    hasAlternative,
                    &collectedAlternative)) {
            goto cleanup;
        }
        if (collectedAlternative) {
            hasAlternative = ZR_TRUE;
        }
    } else if (hasAlternative &&
               !cfg_switch_binding_array_merge_alternative(&switchBindings,
                                                           ZR_NULL,
                                                           bindings,
                                                           ZR_TRUE)) {
        goto cleanup;
    }

    ok = (TZrBool)(hasAlternative && switchBindings.count > 0 &&
                   cfg_switch_apply_merged_bindings(outBindings,
                                                    &switchBindings,
                                                    bindings));

cleanup:
    cfg_throw_type_binding_array_free(&switchBindings);
    return ok;
}
