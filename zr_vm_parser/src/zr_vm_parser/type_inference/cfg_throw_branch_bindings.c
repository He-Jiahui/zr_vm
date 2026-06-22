#include "cfg_internal.h"

static TZrBool cfg_if_branch_can_fallthrough(SZrAstNode *branch) {
    return (TZrBool)!cfg_node_stops_result_throw_binding_flow(branch);
}

static TZrBool cfg_if_collect_branch_bindings(
        SZrAstNode *branch,
        TZrBool canFallthrough,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        SZrParserCfgThrowTypeBindingArray *outBindings) {
    if (!canFallthrough) {
        return ZR_TRUE;
    }
    return cfg_node_collect_result_throw_bindings(branch,
                                                 bindings,
                                                 resolveThrowKindMask,
                                                 outBindings);
}

static TZrBool cfg_if_merge_then_binding(
        SZrParserCfgThrowTypeBindingArray *outBindings,
        const SZrParserCfgThrowTypeBindingArray *elseBindings,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrBool elseCanFallthrough,
        const SZrParserCfgThrowTypeBinding *thenBinding) {
    SZrParserCfgThrowTypeBinding mergedBinding;
    const SZrParserCfgThrowTypeBinding *outChain;
    TZrSize elseIndex;

    if (outBindings == ZR_NULL || thenBinding == ZR_NULL) {
        return ZR_FALSE;
    }

    mergedBinding = *thenBinding;
    outChain = cfg_throw_type_binding_array_chain_from(outBindings, bindings);
    if (elseCanFallthrough) {
        if (cfg_throw_type_binding_array_find(elseBindings,
                                              mergedBinding.name,
                                              &elseIndex)) {
            TZrUInt32 elseKnownKindMask =
                    elseBindings->items[elseIndex].knownKindMask;
            mergedBinding.knownKindMask =
                    mergedBinding.knownKindMask == 0u || elseKnownKindMask == 0u
                            ? 0u
                            : mergedBinding.knownKindMask | elseKnownKindMask;
        } else if (!cfg_throw_type_binding_merge_with_incoming(bindings,
                                                               &mergedBinding)) {
            return ZR_FALSE;
        }
    }

    return cfg_throw_type_binding_array_append_or_replace(outBindings,
                                                          &mergedBinding,
                                                          &outChain);
}

static TZrBool cfg_if_merge_else_binding(
        SZrParserCfgThrowTypeBindingArray *outBindings,
        const SZrParserCfgThrowTypeBindingArray *thenBindings,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrBool thenCanFallthrough,
        const SZrParserCfgThrowTypeBinding *elseBinding) {
    SZrParserCfgThrowTypeBinding mergedBinding;
    const SZrParserCfgThrowTypeBinding *outChain;

    if (outBindings == ZR_NULL || elseBinding == ZR_NULL) {
        return ZR_FALSE;
    }
    if (cfg_throw_type_binding_array_find(thenBindings,
                                          elseBinding->name,
                                          ZR_NULL)) {
        return ZR_TRUE;
    }

    mergedBinding = *elseBinding;
    outChain = cfg_throw_type_binding_array_chain_from(outBindings, bindings);
    if (thenCanFallthrough &&
        !cfg_throw_type_binding_merge_with_incoming(bindings, &mergedBinding)) {
        return ZR_FALSE;
    }

    return cfg_throw_type_binding_array_append_or_replace(outBindings,
                                                          &mergedBinding,
                                                          &outChain);
}

static TZrBool cfg_if_merge_fallthrough_bindings(
        SZrParserCfgThrowTypeBindingArray *outBindings,
        const SZrParserCfgThrowTypeBindingArray *thenBindings,
        const SZrParserCfgThrowTypeBindingArray *elseBindings,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrBool thenCanFallthrough,
        TZrBool elseCanFallthrough) {
    TZrSize index;

    if (thenCanFallthrough) {
        for (index = 0; index < thenBindings->count; index++) {
            if (!cfg_if_merge_then_binding(outBindings,
                                           elseBindings,
                                           bindings,
                                           elseCanFallthrough,
                                           &thenBindings->items[index])) {
                return ZR_FALSE;
            }
        }
    }

    if (elseCanFallthrough) {
        for (index = 0; index < elseBindings->count; index++) {
            if (!cfg_if_merge_else_binding(outBindings,
                                           thenBindings,
                                           bindings,
                                           thenCanFallthrough,
                                           &elseBindings->items[index])) {
                return ZR_FALSE;
            }
        }
    }

    return ZR_TRUE;
}

TZrBool cfg_if_expression_merged_throw_binding(
        SZrAstNode *node,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        SZrParserCfgThrowTypeBindingArray *outBindings) {
    SZrParserCfgThrowTypeBindingArray thenBindings;
    SZrParserCfgThrowTypeBindingArray elseBindings;
    TZrBool conditionValue = ZR_FALSE;
    TZrBool thenCanFallthrough;
    TZrBool elseCanFallthrough;
    TZrBool ok = ZR_FALSE;

    if (node == ZR_NULL || outBindings == ZR_NULL ||
        node->type != ZR_AST_IF_EXPRESSION) {
        return ZR_FALSE;
    }

    if (cfg_node_bool_constant(node->data.ifExpression.condition,
                               &conditionValue)) {
        SZrAstNode *selectedBranch = conditionValue
                                             ? node->data.ifExpression.thenExpr
                                             : node->data.ifExpression.elseExpr;
        TZrSize initialCount = outBindings->count;

        if (!cfg_node_collect_result_throw_bindings(selectedBranch,
                                                    bindings,
                                                    resolveThrowKindMask,
                                                    outBindings)) {
            return ZR_FALSE;
        }
        return (TZrBool)(outBindings->count > initialCount);
    }

    thenCanFallthrough =
            cfg_if_branch_can_fallthrough(node->data.ifExpression.thenExpr);
    elseCanFallthrough =
            cfg_if_branch_can_fallthrough(node->data.ifExpression.elseExpr);
    if (!thenCanFallthrough && !elseCanFallthrough) {
        return ZR_FALSE;
    }

    if (!cfg_throw_type_binding_array_init(
                &thenBindings,
                thenCanFallthrough
                        ? cfg_node_result_throw_binding_capacity(
                                  node->data.ifExpression.thenExpr)
                        : 0)) {
        return ZR_FALSE;
    }
    if (!cfg_throw_type_binding_array_init(
                &elseBindings,
                elseCanFallthrough
                        ? cfg_node_result_throw_binding_capacity(
                                  node->data.ifExpression.elseExpr)
                        : 0)) {
        cfg_throw_type_binding_array_free(&thenBindings);
        return ZR_FALSE;
    }

    if (!cfg_if_collect_branch_bindings(node->data.ifExpression.thenExpr,
                                        thenCanFallthrough,
                                        bindings,
                                        resolveThrowKindMask,
                                        &thenBindings) ||
        !cfg_if_collect_branch_bindings(node->data.ifExpression.elseExpr,
                                        elseCanFallthrough,
                                        bindings,
                                        resolveThrowKindMask,
                                        &elseBindings)) {
        goto cleanup;
    }

    if (!cfg_if_merge_fallthrough_bindings(outBindings,
                                           &thenBindings,
                                           &elseBindings,
                                           bindings,
                                           thenCanFallthrough,
                                           elseCanFallthrough)) {
        goto cleanup;
    }

    ok = (TZrBool)(thenBindings.count > 0 || elseBindings.count > 0);

cleanup:
    cfg_throw_type_binding_array_free(&thenBindings);
    cfg_throw_type_binding_array_free(&elseBindings);
    return ok;
}
