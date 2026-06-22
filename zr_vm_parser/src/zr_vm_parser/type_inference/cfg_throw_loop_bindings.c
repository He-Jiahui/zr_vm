#include "cfg_internal.h"

static TZrBool cfg_statement_is_break(SZrAstNode *node) {
    return (TZrBool)(node != ZR_NULL &&
                     node->type == ZR_AST_BREAK_CONTINUE_STATEMENT &&
                     node->data.breakContinueStatement.isBreak);
}

static TZrBool cfg_statement_is_continue(SZrAstNode *node) {
    return (TZrBool)(node != ZR_NULL &&
                     node->type == ZR_AST_BREAK_CONTINUE_STATEMENT &&
                     !node->data.breakContinueStatement.isBreak);
}

static TZrSize cfg_node_array_result_throw_binding_capacity(
        SZrAstNodeArray *nodes) {
    TZrSize capacity = 0;
    TZrSize index;

    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return 0;
    }

    for (index = 0; index < nodes->count; index++) {
        capacity += cfg_node_result_throw_binding_capacity(nodes->nodes[index]);
    }

    return capacity;
}

typedef struct SZrParserCfgThrowTypeBindingSource {
    const SZrParserCfgThrowTypeBindingArray *bindings;
    const struct SZrParserCfgThrowTypeBindingSource *next;
} SZrParserCfgThrowTypeBindingSource;

static TZrBool cfg_throw_type_binding_array_merge_current_name(
        SZrParserCfgThrowTypeBindingArray *outBindings,
        const SZrParserCfgThrowTypeBinding *currentBindings,
        SZrString *name) {
    const SZrParserCfgThrowTypeBinding *binding;
    SZrParserCfgThrowTypeBinding mergedBinding;
    const SZrParserCfgThrowTypeBinding *outChain;

    if (outBindings == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }
    for (binding = currentBindings; binding != ZR_NULL; binding = binding->next) {
        if (cfg_string_equals(binding->name, name)) {
            mergedBinding = *binding;
            mergedBinding.next = ZR_NULL;
            outChain = cfg_throw_type_binding_array_chain_from(outBindings,
                                                               ZR_NULL);
            return cfg_throw_type_binding_array_append_or_merge_alternative(
                    outBindings,
                    &mergedBinding,
                    &outChain);
        }
    }
    return ZR_TRUE;
}

static TZrBool cfg_throw_type_binding_array_merge_current_path(
        SZrParserCfgThrowTypeBindingArray *outBindings,
        const SZrParserCfgThrowTypeBindingSource *pathSources,
        const SZrParserCfgThrowTypeBindingArray *pathBindings,
        const SZrParserCfgThrowTypeBinding *currentBindings) {
    const SZrParserCfgThrowTypeBindingSource *source;
    TZrSize index;

    for (source = pathSources; source != ZR_NULL; source = source->next) {
        for (index = 0; source->bindings != ZR_NULL &&
                        index < source->bindings->count;
             index++) {
            if (!cfg_throw_type_binding_array_merge_current_name(
                        outBindings,
                        currentBindings,
                        source->bindings->items[index].name)) {
                return ZR_FALSE;
            }
        }
    }
    for (index = 0; pathBindings != ZR_NULL && index < pathBindings->count;
         index++) {
        if (!cfg_throw_type_binding_array_merge_current_name(
                    outBindings,
                    currentBindings,
                    pathBindings->items[index].name)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool cfg_node_collect_loop_control_throw_bindings(
        SZrAstNode *node,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        const SZrParserCfgThrowTypeBindingSource *pathSources,
        SZrParserCfgThrowTypeBindingArray *breakBindings,
        SZrParserCfgThrowTypeBindingArray *continueBindings);

static TZrBool cfg_node_array_collect_loop_control_throw_bindings(
        SZrAstNodeArray *nodes,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        const SZrParserCfgThrowTypeBindingSource *pathSources,
        SZrParserCfgThrowTypeBindingArray *breakBindings,
        SZrParserCfgThrowTypeBindingArray *continueBindings) {
    SZrParserCfgThrowTypeBindingArray pathBindings;
    SZrParserCfgThrowTypeBindingSource localSource;
    const SZrParserCfgThrowTypeBinding *currentBindings = bindings;
    TZrSize index;
    TZrBool ok = ZR_FALSE;

    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!cfg_throw_type_binding_array_init(
                &pathBindings,
                cfg_node_array_result_throw_binding_capacity(nodes))) {
        return ZR_FALSE;
    }

    for (index = 0; index < nodes->count; index++) {
        SZrAstNode *node = nodes->nodes[index];
        SZrParserCfgThrowTypeBinding binding;

        if (cfg_statement_is_break(node)) {
            ok = cfg_throw_type_binding_array_merge_current_path(breakBindings,
                                                                 pathSources,
                                                                 &pathBindings,
                                                                 currentBindings);
            goto cleanup;
        }
        if (cfg_statement_is_continue(node)) {
            ok = cfg_throw_type_binding_array_merge_current_path(continueBindings,
                                                                 pathSources,
                                                                 &pathBindings,
                                                                 currentBindings);
            goto cleanup;
        }

        localSource.bindings = &pathBindings;
        localSource.next = pathSources;
        if (cfg_node_collect_loop_control_throw_bindings(node,
                                                         currentBindings,
                                                         resolveThrowKindMask,
                                                         &localSource,
                                                         breakBindings,
                                                         continueBindings) ==
            ZR_FALSE) {
            goto cleanup;
        }

        if (cfg_variable_declaration_throw_binding(node, &binding) ||
            cfg_assignment_throw_binding(node,
                                         currentBindings,
                                         resolveThrowKindMask,
                                         &binding)) {
            if (!cfg_throw_type_binding_array_append_or_replace(&pathBindings,
                                                                &binding,
                                                                &currentBindings)) {
                goto cleanup;
            }
            currentBindings =
                    cfg_throw_type_binding_array_chain_from(&pathBindings,
                                                            bindings);
        }

        if (cfg_node_stops_result_throw_binding_flow(node)) {
            break;
        }
    }

    ok = ZR_TRUE;

cleanup:
    cfg_throw_type_binding_array_free(&pathBindings);
    return ok;
}

static TZrBool cfg_node_collect_loop_control_throw_bindings(
        SZrAstNode *node,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        const SZrParserCfgThrowTypeBindingSource *pathSources,
        SZrParserCfgThrowTypeBindingArray *breakBindings,
        SZrParserCfgThrowTypeBindingArray *continueBindings) {
    if (node == ZR_NULL) {
        return ZR_TRUE;
    }

    if (node->type == ZR_AST_IF_EXPRESSION) {
        TZrBool conditionValue = ZR_FALSE;

        if (cfg_node_bool_constant(node->data.ifExpression.condition,
                                   &conditionValue)) {
            SZrAstNode *selectedBranch =
                    conditionValue ? node->data.ifExpression.thenExpr
                                   : node->data.ifExpression.elseExpr;

            return cfg_node_collect_loop_control_throw_bindings(selectedBranch,
                                                                bindings,
                                                                resolveThrowKindMask,
                                                                pathSources,
                                                                breakBindings,
                                                                continueBindings);
        }

        return (TZrBool)(
                cfg_node_collect_loop_control_throw_bindings(
                        node->data.ifExpression.thenExpr,
                        bindings,
                        resolveThrowKindMask,
                        pathSources,
                        breakBindings,
                        continueBindings) &&
                cfg_node_collect_loop_control_throw_bindings(
                        node->data.ifExpression.elseExpr,
                        bindings,
                        resolveThrowKindMask,
                        pathSources,
                        breakBindings,
                        continueBindings));
    }
    if (node->type == ZR_AST_SCRIPT) {
        return cfg_node_array_collect_loop_control_throw_bindings(
                node->data.script.statements,
                bindings,
                resolveThrowKindMask,
                pathSources,
                breakBindings,
                continueBindings);
    }
    if (node->type == ZR_AST_BLOCK) {
        return cfg_node_array_collect_loop_control_throw_bindings(
                node->data.block.body,
                bindings,
                resolveThrowKindMask,
                pathSources,
                breakBindings,
                continueBindings);
    }

    return ZR_TRUE;
}

static TZrBool cfg_loop_body_result_throw_binding_merge(
        SZrAstNode *body,
        SZrAstNode *step,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        SZrParserCfgThrowTypeBindingArray *outBindings) {
    SZrParserCfgThrowTypeBindingArray iterationBindings;
    SZrParserCfgThrowTypeBindingArray breakExitBindings;
    SZrParserCfgThrowTypeBindingArray continueExitBindings;
    const SZrParserCfgThrowTypeBinding *iterationChain;
    const SZrParserCfgThrowTypeBinding *continueChain;
    TZrSize iterationIndex;
    TZrSize breakExitIndex;
    TZrSize continueExitIndex;
    TZrBool ok = ZR_FALSE;

    if (body == ZR_NULL || outBindings == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!cfg_throw_type_binding_array_init(
                &iterationBindings,
                cfg_node_result_throw_binding_capacity(body) +
                        cfg_node_result_throw_binding_capacity(step))) {
        return ZR_FALSE;
    }
    if (!cfg_throw_type_binding_array_init(
                &breakExitBindings,
                cfg_node_result_throw_binding_capacity(body))) {
        cfg_throw_type_binding_array_free(&iterationBindings);
        return ZR_FALSE;
    }
    if (!cfg_throw_type_binding_array_init(
                &continueExitBindings,
                cfg_node_result_throw_binding_capacity(body) +
                        cfg_node_result_throw_binding_capacity(step))) {
        cfg_throw_type_binding_array_free(&breakExitBindings);
        cfg_throw_type_binding_array_free(&iterationBindings);
        return ZR_FALSE;
    }

    if (!cfg_node_collect_result_throw_bindings(body,
                                               bindings,
                                               resolveThrowKindMask,
                                               &iterationBindings)) {
        goto cleanup;
    }
    iterationChain = cfg_throw_type_binding_array_chain_from(&iterationBindings,
                                                             bindings);
    if (!cfg_node_collect_result_throw_bindings(step,
                                               iterationChain,
                                               resolveThrowKindMask,
                                               &iterationBindings)) {
        goto cleanup;
    }

    for (iterationIndex = 0; iterationIndex < iterationBindings.count;
         iterationIndex++) {
        SZrParserCfgThrowTypeBinding mergedBinding =
                iterationBindings.items[iterationIndex];
        const SZrParserCfgThrowTypeBinding *outChain =
                cfg_throw_type_binding_array_chain_from(outBindings, bindings);

        if (!cfg_throw_type_binding_merge_with_incoming(bindings,
                                                        &mergedBinding) ||
            !cfg_throw_type_binding_array_append_or_merge_alternative(
                    outBindings,
                    &mergedBinding,
                    &outChain)) {
            goto cleanup;
        }
    }

    if (!cfg_node_collect_loop_control_throw_bindings(body,
                                                      bindings,
                                                      resolveThrowKindMask,
                                                      ZR_NULL,
                                                      &breakExitBindings,
                                                      &continueExitBindings)) {
        goto cleanup;
    }
    if (continueExitBindings.count > 0 && step != ZR_NULL) {
        continueChain = cfg_throw_type_binding_array_chain_from(
                &continueExitBindings,
                bindings);
        if (!cfg_node_collect_result_throw_bindings(step,
                                                   continueChain,
                                                   resolveThrowKindMask,
                                                   &continueExitBindings)) {
            goto cleanup;
        }
    }

    for (breakExitIndex = 0; breakExitIndex < breakExitBindings.count;
         breakExitIndex++) {
        SZrParserCfgThrowTypeBinding mergedBinding =
                breakExitBindings.items[breakExitIndex];
        const SZrParserCfgThrowTypeBinding *outChain =
                cfg_throw_type_binding_array_chain_from(outBindings, bindings);

        if (!cfg_throw_type_binding_merge_with_incoming(bindings,
                                                        &mergedBinding) ||
            !cfg_throw_type_binding_array_append_or_merge_alternative(
                    outBindings,
                    &mergedBinding,
                    &outChain)) {
            goto cleanup;
        }
    }

    for (continueExitIndex = 0; continueExitIndex < continueExitBindings.count;
         continueExitIndex++) {
        SZrParserCfgThrowTypeBinding mergedBinding =
                continueExitBindings.items[continueExitIndex];
        const SZrParserCfgThrowTypeBinding *outChain =
                cfg_throw_type_binding_array_chain_from(outBindings, bindings);

        if (!cfg_throw_type_binding_merge_with_incoming(bindings,
                                                        &mergedBinding) ||
            !cfg_throw_type_binding_array_append_or_merge_alternative(
                    outBindings,
                    &mergedBinding,
                    &outChain)) {
            goto cleanup;
        }
    }

    ok = (TZrBool)(iterationBindings.count > 0 ||
                   breakExitBindings.count > 0 ||
                   continueExitBindings.count > 0);

cleanup:
    cfg_throw_type_binding_array_free(&continueExitBindings);
    cfg_throw_type_binding_array_free(&breakExitBindings);
    cfg_throw_type_binding_array_free(&iterationBindings);
    return ok;
}

static TZrBool cfg_loop_body_merged_throw_binding(
        SZrAstNode *condition,
        SZrAstNode *body,
        SZrAstNode *step,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        SZrParserCfgThrowTypeBindingArray *outBindings) {
    TZrBool conditionValue = ZR_FALSE;

    if (condition == ZR_NULL ||
        cfg_node_bool_constant(condition, &conditionValue)) {
        return ZR_FALSE;
    }

    return cfg_loop_body_result_throw_binding_merge(body,
                                                    step,
                                                    bindings,
                                                    resolveThrowKindMask,
                                                    outBindings);
}

TZrBool cfg_while_loop_merged_throw_binding(
        SZrAstNode *node,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        SZrParserCfgThrowTypeBindingArray *outBindings) {
    if (node == ZR_NULL || node->type != ZR_AST_WHILE_LOOP) {
        return ZR_FALSE;
    }
    return cfg_loop_body_merged_throw_binding(node->data.whileLoop.cond,
                                              node->data.whileLoop.block,
                                              ZR_NULL,
                                              bindings,
                                              resolveThrowKindMask,
                                              outBindings);
}

TZrBool cfg_for_loop_merged_throw_binding(
        SZrAstNode *node,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        SZrParserCfgThrowTypeBindingArray *outBindings) {
    const SZrParserCfgThrowTypeBinding *loopBindings;
    TZrSize initialCount;
    TZrBool conditionValue = ZR_FALSE;
    TZrBool mergedLoopBinding = ZR_FALSE;

    if (node == ZR_NULL || node->type != ZR_AST_FOR_LOOP) {
        return ZR_FALSE;
    }
    if (node->data.forLoop.cond == ZR_NULL) {
        return ZR_FALSE;
    }
    if (cfg_node_bool_constant(node->data.forLoop.cond, &conditionValue)) {
        if (conditionValue) {
            return ZR_FALSE;
        }
        return cfg_node_collect_result_throw_bindings(node->data.forLoop.init,
                                                      bindings,
                                                      resolveThrowKindMask,
                                                      outBindings);
    }

    initialCount = outBindings != ZR_NULL ? outBindings->count : 0;
    if (!cfg_node_collect_result_throw_bindings(node->data.forLoop.init,
                                                bindings,
                                                resolveThrowKindMask,
                                                outBindings)) {
        return ZR_FALSE;
    }

    loopBindings = cfg_throw_type_binding_array_chain_from(outBindings, bindings);
    mergedLoopBinding = cfg_loop_body_merged_throw_binding(node->data.forLoop.cond,
                                                           node->data.forLoop.block,
                                                           node->data.forLoop.step,
                                                           loopBindings,
                                                           resolveThrowKindMask,
                                                           outBindings);
    if (mergedLoopBinding) {
        return ZR_TRUE;
    }
    return (TZrBool)(outBindings != ZR_NULL && outBindings->count > initialCount);
}

TZrBool cfg_foreach_loop_merged_throw_binding(
        SZrAstNode *node,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        SZrParserCfgThrowTypeBindingArray *outBindings) {
    if (node == ZR_NULL || node->type != ZR_AST_FOREACH_LOOP) {
        return ZR_FALSE;
    }
    return cfg_loop_body_result_throw_binding_merge(node->data.foreachLoop.block,
                                                    ZR_NULL,
                                                    bindings,
                                                    resolveThrowKindMask,
                                                    outBindings);
}
