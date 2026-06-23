#include "type_inference_loop_assignment_nested_if.h"

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

TZrBool ZrParser_TypeInferenceLoopAssignment_NestedIfAssignsName(SZrAstNode *node,
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
            if (ZrParser_TypeInferenceLoopAssignment_NestedIfAssignsName(body->nodes[index], name)) {
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
        return ZrParser_TypeInferenceLoopAssignment_NestedIfAssignsName(
                       node->data.ifExpression.thenExpr,
                       name) ||
               ZrParser_TypeInferenceLoopAssignment_NestedIfAssignsName(
                       node->data.ifExpression.elseExpr,
                       name);
    }
    return ZR_FALSE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_NestedIfCollectTargets(SZrState *state,
                                                                    SZrAstNode *node,
                                                                    SZrArray *targetNames,
                                                                    TZrBool *outHasAssignment) {
    SZrAstNodeArray *body;
    TZrSize index;

    if (outHasAssignment != ZR_NULL) {
        *outHasAssignment = ZR_FALSE;
    }
    if (state == ZR_NULL || targetNames == ZR_NULL) {
        return ZR_FALSE;
    }
    if (node == ZR_NULL) {
        return ZR_TRUE;
    }

    if (node->type == ZR_AST_BLOCK) {
        body = node->data.block.body;
        if (body == ZR_NULL || body->nodes == ZR_NULL) {
            return ZR_TRUE;
        }
        for (index = 0; index < body->count; index++) {
            TZrBool childHasAssignment = ZR_FALSE;
            if (!ZrParser_TypeInferenceLoopAssignment_NestedIfCollectTargets(
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

    node = ZrParser_TypeInferenceLoopAssignment_StatementExpression(node);
    if (node == ZR_NULL) {
        return ZR_TRUE;
    }
    if (node->type == ZR_AST_ASSIGNMENT_EXPRESSION) {
        SZrString *name = ZR_NULL;
        SZrAstNode *right = ZR_NULL;
        if (!ZrParser_TypeInferenceLoopAssignment_AssignmentParts(node, &name, &right) ||
            !type_inference_loop_assignment_target_array_add(state, targetNames, name)) {
            return ZR_FALSE;
        }
        if (outHasAssignment != ZR_NULL) {
            *outHasAssignment = ZR_TRUE;
        }
        return ZR_TRUE;
    }
    if (node->type == ZR_AST_IF_EXPRESSION) {
        TZrBool thenHasAssignment = ZR_FALSE;
        TZrBool elseHasAssignment = ZR_FALSE;
        if (!ZrParser_TypeInferenceLoopAssignment_NestedIfCollectTargets(
                    state,
                    node->data.ifExpression.thenExpr,
                    targetNames,
                    &thenHasAssignment) ||
            !ZrParser_TypeInferenceLoopAssignment_NestedIfCollectTargets(
                    state,
                    node->data.ifExpression.elseExpr,
                    targetNames,
                    &elseHasAssignment)) {
            return ZR_FALSE;
        }
        if ((thenHasAssignment || elseHasAssignment) && outHasAssignment != ZR_NULL) {
            *outHasAssignment = ZR_TRUE;
        }
        return ZR_TRUE;
    }
    return ZR_TRUE;
}
