#include "type_inference_loop_assignment_nested_loop.h"

#include "type_inference_loop_assignment_syntax.h"

#include "zr_vm_core/string.h"

static TZrBool type_inference_loop_assignment_target_array_contains(SZrArray *targetNames,
                                                                    SZrString *name) {
    TZrSize index;

    if (targetNames == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < targetNames->length; index++) {
        SZrString **target = (SZrString **)ZrCore_Array_Get(targetNames, index);
        if (target != ZR_NULL && *target != ZR_NULL && ZrCore_String_Equal(*target, name)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool type_inference_loop_assignment_target_array_add(SZrState *state,
                                                               SZrArray *targetNames,
                                                               SZrString *name) {
    if (state == ZR_NULL || targetNames == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!type_inference_loop_assignment_target_array_contains(targetNames, name)) {
        ZrCore_Array_Push(state, targetNames, &name);
    }
    return ZR_TRUE;
}

static TZrBool type_inference_loop_assignment_node_is_loop(SZrAstNode *node) {
    return node != ZR_NULL &&
           (node->type == ZR_AST_WHILE_LOOP ||
            node->type == ZR_AST_FOR_LOOP ||
            node->type == ZR_AST_FOREACH_LOOP);
}

static TZrBool type_inference_loop_assignment_collect_targets_from_node(
        SZrState *state,
        SZrAstNode *node,
        SZrArray *targetNames,
        TZrBool *outHasAssignment);

static TZrBool type_inference_loop_assignment_collect_targets_from_block(
        SZrState *state,
        SZrAstNode *block,
        SZrArray *targetNames,
        TZrBool *outHasAssignment) {
    SZrAstNodeArray *body;
    TZrSize index;

    if (outHasAssignment != ZR_NULL) {
        *outHasAssignment = ZR_FALSE;
    }
    if (state == ZR_NULL || block == ZR_NULL || targetNames == ZR_NULL) {
        return ZR_FALSE;
    }
    if (block->type != ZR_AST_BLOCK) {
        return type_inference_loop_assignment_collect_targets_from_node(
                state,
                block,
                targetNames,
                outHasAssignment);
    }

    body = block->data.block.body;
    if (body == ZR_NULL || body->nodes == ZR_NULL) {
        return ZR_TRUE;
    }
    for (index = 0; index < body->count; index++) {
        TZrBool childHasAssignment = ZR_FALSE;
        if (!type_inference_loop_assignment_collect_targets_from_node(
                state,
                body->nodes[index],
                targetNames,
                &childHasAssignment)) {
            return ZR_FALSE;
        }
        if (childHasAssignment && outHasAssignment != ZR_NULL) {
            *outHasAssignment = ZR_TRUE;
        }
    }
    return ZR_TRUE;
}

static TZrBool type_inference_loop_assignment_collect_assignment_target(
        SZrState *state,
        SZrAstNode *node,
        SZrArray *targetNames,
        TZrBool *outHasAssignment) {
    SZrString *name = ZR_NULL;
    SZrAstNode *right = ZR_NULL;

    if (outHasAssignment != ZR_NULL) {
        *outHasAssignment = ZR_FALSE;
    }
    if (!ZrParser_TypeInferenceLoopAssignment_AssignmentParts(node, &name, &right)) {
        return ZR_TRUE;
    }
    if (!type_inference_loop_assignment_target_array_add(state, targetNames, name)) {
        return ZR_FALSE;
    }
    if (outHasAssignment != ZR_NULL) {
        *outHasAssignment = ZR_TRUE;
    }
    return ZR_TRUE;
}

static TZrBool type_inference_loop_assignment_collect_targets_from_if(
        SZrState *state,
        SZrAstNode *ifNode,
        SZrArray *targetNames,
        TZrBool *outHasAssignment) {
    TZrBool thenHasAssignment = ZR_FALSE;
    TZrBool elseHasAssignment = ZR_FALSE;

    if (outHasAssignment != ZR_NULL) {
        *outHasAssignment = ZR_FALSE;
    }
    if (ifNode == ZR_NULL || ifNode->type != ZR_AST_IF_EXPRESSION) {
        return ZR_FALSE;
    }

    if (!type_inference_loop_assignment_collect_targets_from_node(
                state,
                ifNode->data.ifExpression.thenExpr,
                targetNames,
                &thenHasAssignment) ||
        !type_inference_loop_assignment_collect_targets_from_node(
                state,
                ifNode->data.ifExpression.elseExpr,
                targetNames,
                &elseHasAssignment)) {
        return ZR_FALSE;
    }
    if ((thenHasAssignment || elseHasAssignment) && outHasAssignment != ZR_NULL) {
        *outHasAssignment = ZR_TRUE;
    }
    return ZR_TRUE;
}

static TZrBool type_inference_loop_assignment_collect_targets_from_loop(
        SZrState *state,
        SZrAstNode *loopNode,
        SZrArray *targetNames,
        TZrBool *outHasAssignment) {
    TZrBool bodyHasAssignment = ZR_FALSE;
    TZrBool initHasAssignment = ZR_FALSE;
    TZrBool stepHasAssignment = ZR_FALSE;

    if (outHasAssignment != ZR_NULL) {
        *outHasAssignment = ZR_FALSE;
    }
    if (!type_inference_loop_assignment_node_is_loop(loopNode)) {
        return ZR_FALSE;
    }

    if (loopNode->type == ZR_AST_WHILE_LOOP) {
        if (!type_inference_loop_assignment_collect_targets_from_block(
                    state,
                    loopNode->data.whileLoop.block,
                    targetNames,
                    &bodyHasAssignment)) {
            return ZR_FALSE;
        }
    } else if (loopNode->type == ZR_AST_FOR_LOOP) {
        if (!type_inference_loop_assignment_collect_assignment_target(
                    state,
                    loopNode->data.forLoop.init,
                    targetNames,
                    &initHasAssignment) ||
            !type_inference_loop_assignment_collect_targets_from_block(
                    state,
                    loopNode->data.forLoop.block,
                    targetNames,
                    &bodyHasAssignment) ||
            !type_inference_loop_assignment_collect_assignment_target(
                    state,
                    loopNode->data.forLoop.step,
                    targetNames,
                    &stepHasAssignment)) {
            return ZR_FALSE;
        }
    } else if (!type_inference_loop_assignment_collect_targets_from_block(
                       state,
                       loopNode->data.foreachLoop.block,
                       targetNames,
                       &bodyHasAssignment)) {
        return ZR_FALSE;
    }

    if ((bodyHasAssignment || initHasAssignment || stepHasAssignment) &&
        outHasAssignment != ZR_NULL) {
        *outHasAssignment = ZR_TRUE;
    }
    return ZR_TRUE;
}

static TZrBool type_inference_loop_assignment_collect_targets_from_node(
        SZrState *state,
        SZrAstNode *node,
        SZrArray *targetNames,
        TZrBool *outHasAssignment) {
    if (outHasAssignment != ZR_NULL) {
        *outHasAssignment = ZR_FALSE;
    }
    if (node == ZR_NULL) {
        return ZR_TRUE;
    }
    if (node->type == ZR_AST_BLOCK) {
        return type_inference_loop_assignment_collect_targets_from_block(
                state,
                node,
                targetNames,
                outHasAssignment);
    }

    node = ZrParser_TypeInferenceLoopAssignment_StatementExpression(node);
    if (node == ZR_NULL) {
        return ZR_TRUE;
    }
    if (node->type == ZR_AST_ASSIGNMENT_EXPRESSION) {
        return type_inference_loop_assignment_collect_assignment_target(
                state,
                node,
                targetNames,
                outHasAssignment);
    }
    if (node->type == ZR_AST_IF_EXPRESSION) {
        return type_inference_loop_assignment_collect_targets_from_if(
                state,
                node,
                targetNames,
                outHasAssignment);
    }
    if (type_inference_loop_assignment_node_is_loop(node)) {
        return type_inference_loop_assignment_collect_targets_from_loop(
                state,
                node,
                targetNames,
                outHasAssignment);
    }
    return ZR_TRUE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_NestedLoopAssignsName(SZrAstNode *node,
                                                                   SZrString *name) {
    SZrAstNodeArray *body;
    TZrSize index;

    if (node == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    if (node->type == ZR_AST_BLOCK) {
        body = node->data.block.body;
        if (body == ZR_NULL || body->nodes == ZR_NULL) {
            return ZR_FALSE;
        }
        for (index = 0; index < body->count; index++) {
            if (ZrParser_TypeInferenceLoopAssignment_NestedLoopAssignsName(body->nodes[index], name)) {
                return ZR_TRUE;
            }
        }
        return ZR_FALSE;
    }

    node = ZrParser_TypeInferenceLoopAssignment_StatementExpression(node);
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }
    if (node->type == ZR_AST_ASSIGNMENT_EXPRESSION) {
        SZrString *assignedName = ZR_NULL;
        SZrAstNode *right = ZR_NULL;
        return ZrParser_TypeInferenceLoopAssignment_AssignmentParts(node, &assignedName, &right) &&
               assignedName != ZR_NULL &&
               ZrCore_String_Equal(assignedName, name);
    }
    if (node->type == ZR_AST_IF_EXPRESSION) {
        return ZrParser_TypeInferenceLoopAssignment_NestedLoopAssignsName(
                       node->data.ifExpression.thenExpr,
                       name) ||
               ZrParser_TypeInferenceLoopAssignment_NestedLoopAssignsName(
                       node->data.ifExpression.elseExpr,
                       name);
    }
    if (node->type == ZR_AST_WHILE_LOOP) {
        return ZrParser_TypeInferenceLoopAssignment_NestedLoopAssignsName(
                node->data.whileLoop.block,
                name);
    }
    if (node->type == ZR_AST_FOR_LOOP) {
        return ZrParser_TypeInferenceLoopAssignment_NestedLoopAssignsName(
                       node->data.forLoop.init,
                       name) ||
               ZrParser_TypeInferenceLoopAssignment_NestedLoopAssignsName(
                       node->data.forLoop.block,
                       name) ||
               ZrParser_TypeInferenceLoopAssignment_NestedLoopAssignsName(
                       node->data.forLoop.step,
                       name);
    }
    if (node->type == ZR_AST_FOREACH_LOOP) {
        return ZrParser_TypeInferenceLoopAssignment_NestedLoopAssignsName(
                node->data.foreachLoop.block,
                name);
    }
    return ZR_FALSE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_NestedLoopCollectTargets(SZrState *state,
                                                                      SZrAstNode *node,
                                                                      SZrArray *targetNames,
                                                                      TZrBool *outHasAssignment) {
    if (outHasAssignment != ZR_NULL) {
        *outHasAssignment = ZR_FALSE;
    }
    if (state == ZR_NULL ||
        node == ZR_NULL ||
        targetNames == ZR_NULL ||
        !type_inference_loop_assignment_node_is_loop(node)) {
        return ZR_FALSE;
    }
    return type_inference_loop_assignment_collect_targets_from_loop(
            state,
            node,
            targetNames,
            outHasAssignment);
}

TZrBool ZrParser_TypeInferenceLoopAssignment_TryJoinNestedLoop(SZrCompilerState *cs,
                                                               SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }
    if (node->type == ZR_AST_WHILE_LOOP) {
        return ZrParser_TypeInference_TryJoinWhileNumericAssignments(cs, node);
    }
    if (node->type == ZR_AST_FOR_LOOP) {
        return ZrParser_TypeInference_TryJoinForNumericAssignments(cs, node);
    }
    if (node->type == ZR_AST_FOREACH_LOOP) {
        return ZrParser_TypeInference_TryJoinForeachNumericAssignments(cs, node);
    }
    return ZR_FALSE;
}
