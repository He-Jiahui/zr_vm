#include "parser_internal.h"

/* Valid inputs transfer ownership here; a failed append destroys both trees. */
SZrAstNode *append_primary_member(
        SZrParserState *ps, SZrAstNode *base, SZrAstNode *memberNode,
        SZrFileRange startLoc) {
    SZrAstNode *primaryNode;
    SZrFileRange mergedLocation;

    if (ps == ZR_NULL || base == ZR_NULL || memberNode == ZR_NULL) {
        return base;
    }
    if (!ZrParser_MergeLocations(
                ps, base->type == ZR_AST_PRIMARY_EXPRESSION ? base->location : startLoc,
                memberNode->location, &mergedLocation)) {
        ZrParser_Ast_Free(ps->state, base);
        ZrParser_Ast_Free(ps->state, memberNode);
        return ZR_NULL;
    }
    if (base->type == ZR_AST_PRIMARY_EXPRESSION) {
        if (base->data.primaryExpression.members == ZR_NULL) {
            base->data.primaryExpression.members = parser_ast_array_new(ps, 1u);
            if (base->data.primaryExpression.members == ZR_NULL) {
                ZrParser_Ast_Free(ps->state, base);
                ZrParser_Ast_Free(ps->state, memberNode);
                return ZR_NULL;
            }
        }
        if (!parser_ast_array_add(
                    ps, base->data.primaryExpression.members, memberNode)) {
            ZrParser_Ast_Free(ps->state, base);
            ZrParser_Ast_Free(ps->state, memberNode);
            return ZR_NULL;
        }
        base->location = mergedLocation;
        return base;
    }

    primaryNode = create_ast_node(ps, ZR_AST_PRIMARY_EXPRESSION, mergedLocation);
    if (primaryNode == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, base);
        ZrParser_Ast_Free(ps->state, memberNode);
        return ZR_NULL;
    }
    primaryNode->data.primaryExpression.property = base;
    primaryNode->data.primaryExpression.members = parser_ast_array_new(ps, 1u);
    if (primaryNode->data.primaryExpression.members == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, primaryNode);
        ZrParser_Ast_Free(ps->state, memberNode);
        return ZR_NULL;
    }
    if (!parser_ast_array_add(
                ps, primaryNode->data.primaryExpression.members, memberNode)) {
        ZrParser_Ast_Free(ps->state, memberNode);
        ZrParser_Ast_Free(ps->state, primaryNode);
        return ZR_NULL;
    }
    return primaryNode;
}
