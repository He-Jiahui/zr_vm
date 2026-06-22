#include "cfg_internal.h"

#include <stdlib.h>
#include <string.h>

TZrBool cfg_node_identifier_name(SZrAstNode *node, SZrString **outName) {
    if (outName != ZR_NULL) {
        *outName = ZR_NULL;
    }
    if (node == ZR_NULL || outName == ZR_NULL ||
        node->type != ZR_AST_IDENTIFIER_LITERAL ||
        node->data.identifier.name == ZR_NULL) {
        return ZR_FALSE;
    }

    *outName = node->data.identifier.name;
    return ZR_TRUE;
}

TZrBool cfg_throw_type_binding_lookup(
        const SZrParserCfgThrowTypeBinding *bindings,
        SZrString *name,
        TZrUInt32 *outKnownKindMask) {
    const SZrParserCfgThrowTypeBinding *binding;

    if (outKnownKindMask != ZR_NULL) {
        *outKnownKindMask = 0u;
    }
    if (name == ZR_NULL || outKnownKindMask == ZR_NULL) {
        return ZR_FALSE;
    }

    for (binding = bindings; binding != ZR_NULL; binding = binding->next) {
        if (cfg_string_equals(binding->name, name)) {
            if (binding->knownKindMask == 0u) {
                return ZR_FALSE;
            }
            *outKnownKindMask = binding->knownKindMask;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool cfg_variable_declaration_throw_binding(
        SZrAstNode *node,
        SZrParserCfgThrowTypeBinding *outBinding) {
    SZrString *name;
    SZrString *typeName;
    EZrParserCfgThrowKind kind = ZR_PARSER_CFG_THROW_KIND_UNKNOWN;

    if (outBinding != ZR_NULL) {
        outBinding->name = ZR_NULL;
        outBinding->knownKindMask = 0u;
        outBinding->next = ZR_NULL;
    }
    if (node == ZR_NULL || outBinding == ZR_NULL ||
        node->type != ZR_AST_VARIABLE_DECLARATION ||
        !cfg_node_identifier_name(node->data.variableDeclaration.pattern, &name)) {
        return ZR_FALSE;
    }

    typeName = cfg_type_info_simple_name(node->data.variableDeclaration.typeInfo);
    if (!cfg_type_name_throw_kind(typeName, &kind)) {
        return ZR_FALSE;
    }

    outBinding->name = name;
    outBinding->knownKindMask = cfg_throw_kind_mask(kind);
    return ZR_TRUE;
}

static TZrBool cfg_assignment_operator_is_simple(SZrAssignmentOperator op) {
    return (TZrBool)(op.op != ZR_NULL && strcmp(op.op, "=") == 0);
}

static SZrAstNode *cfg_statement_assignment_expression(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    if (node->type == ZR_AST_ASSIGNMENT_EXPRESSION) {
        return node;
    }
    if (node->type == ZR_AST_EXPRESSION_STATEMENT) {
        return cfg_statement_assignment_expression(node->data.expressionStatement.expr);
    }
    return ZR_NULL;
}

static TZrBool cfg_statement_stops_result_throw_binding_flow(
        SZrAstNode *node) {
    return (TZrBool)(node != ZR_NULL &&
                     (node->type == ZR_AST_RETURN_STATEMENT ||
                      node->type == ZR_AST_THROW_STATEMENT ||
                      node->type == ZR_AST_BREAK_CONTINUE_STATEMENT));
}

static TZrBool cfg_node_array_stops_result_throw_binding_flow(
        SZrAstNodeArray *nodes) {
    TZrSize index;

    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < nodes->count; index++) {
        if (cfg_node_stops_result_throw_binding_flow(nodes->nodes[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool cfg_if_expression_stops_result_throw_binding_flow(
        SZrAstNode *node) {
    TZrBool conditionValue = ZR_FALSE;
    SZrAstNode *selectedBranch;

    if (node == ZR_NULL || node->type != ZR_AST_IF_EXPRESSION) {
        return ZR_FALSE;
    }

    if (cfg_node_bool_constant(node->data.ifExpression.condition,
                               &conditionValue)) {
        selectedBranch = conditionValue ? node->data.ifExpression.thenExpr
                                        : node->data.ifExpression.elseExpr;
        return cfg_node_stops_result_throw_binding_flow(selectedBranch);
    }

    if (node->data.ifExpression.elseExpr == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(
            cfg_node_stops_result_throw_binding_flow(
                    node->data.ifExpression.thenExpr) &&
            cfg_node_stops_result_throw_binding_flow(
                    node->data.ifExpression.elseExpr));
}

TZrBool cfg_node_stops_result_throw_binding_flow(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }
    if (cfg_statement_stops_result_throw_binding_flow(node)) {
        return ZR_TRUE;
    }
    if (node->type == ZR_AST_IF_EXPRESSION) {
        return cfg_if_expression_stops_result_throw_binding_flow(node);
    }
    if (node->type == ZR_AST_SCRIPT) {
        return cfg_node_array_stops_result_throw_binding_flow(
                node->data.script.statements);
    }
    if (node->type == ZR_AST_BLOCK) {
        return cfg_node_array_stops_result_throw_binding_flow(node->data.block.body);
    }

    return ZR_FALSE;
}

TZrBool cfg_assignment_throw_binding(
        SZrAstNode *node,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        SZrParserCfgThrowTypeBinding *outBinding) {
    SZrAstNode *assignment = cfg_statement_assignment_expression(node);
    SZrString *name;
    TZrUInt32 knownKindMask = 0u;

    if (outBinding != ZR_NULL) {
        outBinding->name = ZR_NULL;
        outBinding->knownKindMask = 0u;
        outBinding->next = ZR_NULL;
    }
    if (assignment == ZR_NULL || outBinding == ZR_NULL ||
        !cfg_node_identifier_name(assignment->data.assignmentExpression.left, &name)) {
        return ZR_FALSE;
    }

    outBinding->name = name;
    if (cfg_assignment_operator_is_simple(assignment->data.assignmentExpression.op) &&
        resolveThrowKindMask != ZR_NULL &&
        resolveThrowKindMask(assignment->data.assignmentExpression.right,
                             bindings,
                             &knownKindMask)) {
        outBinding->knownKindMask = knownKindMask;
    }
    return ZR_TRUE;
}

TZrBool cfg_throw_type_binding_array_init(
        SZrParserCfgThrowTypeBindingArray *array,
        TZrSize capacity) {
    if (array == ZR_NULL) {
        return ZR_FALSE;
    }

    array->items = ZR_NULL;
    array->count = 0;
    array->capacity = capacity;
    if (capacity == 0) {
        return ZR_TRUE;
    }

    array->items = (SZrParserCfgThrowTypeBinding *)malloc(
            sizeof(SZrParserCfgThrowTypeBinding) * capacity);
    if (array->items == ZR_NULL) {
        array->capacity = 0;
        return ZR_FALSE;
    }
    memset(array->items, 0, sizeof(SZrParserCfgThrowTypeBinding) * capacity);
    return ZR_TRUE;
}

void cfg_throw_type_binding_array_free(
        SZrParserCfgThrowTypeBindingArray *array) {
    if (array == ZR_NULL) {
        return;
    }
    free(array->items);
    array->items = ZR_NULL;
    array->count = 0;
    array->capacity = 0;
}

TZrBool cfg_throw_type_binding_array_find(
        const SZrParserCfgThrowTypeBindingArray *array,
        SZrString *name,
        TZrSize *outIndex) {
    TZrSize index;

    if (outIndex != ZR_NULL) {
        *outIndex = 0;
    }
    if (array == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < array->count; index++) {
        if (cfg_string_equals(array->items[index].name, name)) {
            if (outIndex != ZR_NULL) {
                *outIndex = index;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool cfg_throw_type_binding_array_append_or_replace(
        SZrParserCfgThrowTypeBindingArray *array,
        const SZrParserCfgThrowTypeBinding *binding,
        const SZrParserCfgThrowTypeBinding **inOutBindings) {
    TZrSize existingIndex;
    SZrParserCfgThrowTypeBinding *storedBinding;

    if (array == ZR_NULL || binding == ZR_NULL || binding->name == ZR_NULL ||
        inOutBindings == ZR_NULL) {
        return ZR_FALSE;
    }

    if (cfg_throw_type_binding_array_find(array, binding->name, &existingIndex)) {
        array->items[existingIndex].knownKindMask = binding->knownKindMask;
        return ZR_TRUE;
    }
    if (array->count >= array->capacity) {
        return ZR_FALSE;
    }

    storedBinding = &array->items[array->count++];
    storedBinding->name = binding->name;
    storedBinding->knownKindMask = binding->knownKindMask;
    storedBinding->next = *inOutBindings;
    *inOutBindings = storedBinding;
    return ZR_TRUE;
}

TZrBool cfg_throw_type_binding_array_append_or_merge_alternative(
        SZrParserCfgThrowTypeBindingArray *array,
        const SZrParserCfgThrowTypeBinding *binding,
        const SZrParserCfgThrowTypeBinding **inOutBindings) {
    TZrSize existingIndex;

    if (array == ZR_NULL || binding == ZR_NULL || binding->name == ZR_NULL ||
        inOutBindings == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!cfg_throw_type_binding_array_find(array,
                                           binding->name,
                                           &existingIndex)) {
        return cfg_throw_type_binding_array_append_or_replace(array,
                                                              binding,
                                                              inOutBindings);
    }

    array->items[existingIndex].knownKindMask =
            array->items[existingIndex].knownKindMask == 0u ||
                            binding->knownKindMask == 0u
                    ? 0u
                    : array->items[existingIndex].knownKindMask |
                              binding->knownKindMask;
    return ZR_TRUE;
}

const SZrParserCfgThrowTypeBinding *cfg_throw_type_binding_array_chain_from(
        SZrParserCfgThrowTypeBindingArray *array,
        const SZrParserCfgThrowTypeBinding *bindings) {
    TZrSize index;
    const SZrParserCfgThrowTypeBinding *chain = bindings;

    if (array == ZR_NULL) {
        return bindings;
    }

    for (index = 0; index < array->count; index++) {
        array->items[index].next = chain;
        chain = &array->items[index];
    }

    return chain;
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

TZrSize cfg_node_result_throw_binding_capacity(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return 0;
    }

    if (node->type == ZR_AST_VARIABLE_DECLARATION ||
        cfg_statement_assignment_expression(node) != ZR_NULL) {
        return 1;
    }
    if (node->type == ZR_AST_IF_EXPRESSION) {
        return cfg_node_result_throw_binding_capacity(
                       node->data.ifExpression.thenExpr) +
               cfg_node_result_throw_binding_capacity(
                       node->data.ifExpression.elseExpr);
    }
    if (node->type == ZR_AST_SWITCH_EXPRESSION) {
        return cfg_switch_expression_result_throw_binding_capacity(node);
    }
    if (node->type == ZR_AST_WHILE_LOOP) {
        TZrBool conditionValue = ZR_FALSE;

        if (cfg_node_bool_constant(node->data.whileLoop.cond, &conditionValue)) {
            return 0;
        }
        return cfg_node_result_throw_binding_capacity(node->data.whileLoop.block);
    }
    if (node->type == ZR_AST_FOR_LOOP) {
        TZrBool conditionValue = ZR_FALSE;
        TZrSize capacity = 0;

        if (node->data.forLoop.cond == ZR_NULL) {
            return 0;
        }
        if (cfg_node_bool_constant(node->data.forLoop.cond, &conditionValue)) {
            return conditionValue
                           ? 0
                           : cfg_node_result_throw_binding_capacity(
                                     node->data.forLoop.init);
        }
        capacity = cfg_node_result_throw_binding_capacity(node->data.forLoop.init);
        return capacity +
               cfg_node_result_throw_binding_capacity(node->data.forLoop.block) +
               cfg_node_result_throw_binding_capacity(node->data.forLoop.step);
    }
    if (node->type == ZR_AST_FOREACH_LOOP) {
        return cfg_node_result_throw_binding_capacity(node->data.foreachLoop.block);
    }
    if (node->type == ZR_AST_SCRIPT) {
        return cfg_node_array_result_throw_binding_capacity(
                node->data.script.statements);
    }
    if (node->type == ZR_AST_BLOCK) {
        return cfg_node_array_result_throw_binding_capacity(node->data.block.body);
    }

    return 0;
}

static TZrBool cfg_node_array_collect_result_throw_bindings(
        SZrAstNodeArray *nodes,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        SZrParserCfgThrowTypeBindingArray *outBindings) {
    const SZrParserCfgThrowTypeBinding *currentBindings = bindings;
    TZrSize index;

    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return ZR_TRUE;
    }

    for (index = 0; index < nodes->count; index++) {
        if (!cfg_node_collect_result_throw_bindings(nodes->nodes[index],
                                                   currentBindings,
                                                   resolveThrowKindMask,
                                                   outBindings)) {
            return ZR_FALSE;
        }
        currentBindings = cfg_throw_type_binding_array_chain_from(outBindings,
                                                                  bindings);
        if (cfg_node_stops_result_throw_binding_flow(nodes->nodes[index])) {
            break;
        }
    }

    return ZR_TRUE;
}

TZrBool cfg_throw_type_binding_merge_with_incoming(
        const SZrParserCfgThrowTypeBinding *bindings,
        SZrParserCfgThrowTypeBinding *binding) {
    TZrUInt32 incomingKnownKindMask = 0u;

    if (binding == ZR_NULL || binding->name == ZR_NULL) {
        return ZR_FALSE;
    }
    if (binding->knownKindMask == 0u) {
        return ZR_TRUE;
    }
    if (!cfg_throw_type_binding_lookup(bindings,
                                       binding->name,
                                       &incomingKnownKindMask)) {
        binding->knownKindMask = 0u;
        return ZR_TRUE;
    }

    binding->knownKindMask |= incomingKnownKindMask;
    return ZR_TRUE;
}

TZrBool cfg_node_collect_result_throw_bindings(
        SZrAstNode *node,
        const SZrParserCfgThrowTypeBinding *bindings,
        TZrParserCfgThrowKindMaskResolver resolveThrowKindMask,
        SZrParserCfgThrowTypeBindingArray *outBindings) {
    SZrParserCfgThrowTypeBinding binding;
    const SZrParserCfgThrowTypeBinding *currentBindings;

    if (outBindings == ZR_NULL) {
        return ZR_FALSE;
    }
    if (node == ZR_NULL) {
        return ZR_TRUE;
    }

    currentBindings = cfg_throw_type_binding_array_chain_from(outBindings,
                                                              bindings);
    if (cfg_variable_declaration_throw_binding(node, &binding) ||
        cfg_assignment_throw_binding(node,
                                     currentBindings,
                                     resolveThrowKindMask,
                                     &binding)) {
        return cfg_throw_type_binding_array_append_or_replace(outBindings,
                                                              &binding,
                                                              &currentBindings);
    }
    if (cfg_if_expression_merged_throw_binding(node,
                                               currentBindings,
                                               resolveThrowKindMask,
                                               outBindings)) {
        return ZR_TRUE;
    }
    if (cfg_switch_expression_merged_throw_binding(node,
                                                   currentBindings,
                                                   resolveThrowKindMask,
                                                   outBindings)) {
        return ZR_TRUE;
    }
    if (cfg_while_loop_merged_throw_binding(node,
                                            currentBindings,
                                            resolveThrowKindMask,
                                            outBindings)) {
        return ZR_TRUE;
    }
    if (cfg_for_loop_merged_throw_binding(node,
                                          currentBindings,
                                          resolveThrowKindMask,
                                          outBindings)) {
        return ZR_TRUE;
    }
    if (cfg_foreach_loop_merged_throw_binding(node,
                                              currentBindings,
                                              resolveThrowKindMask,
                                              outBindings)) {
        return ZR_TRUE;
    }

    if (node->type == ZR_AST_SCRIPT) {
        return cfg_node_array_collect_result_throw_bindings(
                node->data.script.statements,
                currentBindings,
                resolveThrowKindMask,
                outBindings);
    }
    if (node->type == ZR_AST_BLOCK) {
        return cfg_node_array_collect_result_throw_bindings(node->data.block.body,
                                                            currentBindings,
                                                            resolveThrowKindMask,
                                                            outBindings);
    }

    return ZR_TRUE;
}
