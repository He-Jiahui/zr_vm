#include "parser_internal.h"

static TZrBool primary_base_accepts_braced_member(SZrAstNode *base) {
    SZrPrimaryExpression *primary;
    SZrAstNode *lastMember;

    if (base == ZR_NULL || base->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }

    primary = &base->data.primaryExpression;
    if (primary->members == ZR_NULL ||
        primary->members->nodes == ZR_NULL ||
        primary->members->count == 0) {
        return ZR_FALSE;
    }

    lastMember = primary->members->nodes[primary->members->count - 1];
    return lastMember != ZR_NULL &&
           lastMember->type == ZR_AST_MEMBER_EXPRESSION &&
           !lastMember->data.memberExpression.computed;
}

SZrAstNode *try_parse_braced_primary_member(SZrParserState *ps,
                                            SZrAstNode *base,
                                            SZrFileRange startLoc,
                                            TZrBool *outHandled) {
    SZrAstNode *objectNode;

    if (outHandled != ZR_NULL) {
        *outHandled = ZR_FALSE;
    }
    if (ps == ZR_NULL || base == ZR_NULL || outHandled == ZR_NULL ||
        ps->lexer->t.token != ZR_TK_LBRACE) {
        return base;
    }

    if (!primary_base_accepts_braced_member(base)) {
        return base;
    }

    *outHandled = ZR_TRUE;
    objectNode = parse_object_literal(ps);
    if (objectNode == ZR_NULL) {
        return ZR_NULL;
    }

    return append_primary_member(ps, base, objectNode, startLoc);
}
