#include "cfg_internal.h"

static TZrUInt32 cfg_catch_throw_kind_mask(EZrParserCfgThrowKind kind) {
    if (kind == ZR_PARSER_CFG_THROW_KIND_UNKNOWN) {
        return 0u;
    }
    return (TZrUInt32)(1u << (TZrUInt32)kind);
}

TZrBool cfg_catch_clause_is_catch_all(SZrAstNode *catchNode) {
    SZrAstNodeArray *pattern;
    SZrAstNode *parameterNode;

    if (catchNode == ZR_NULL || catchNode->type != ZR_AST_CATCH_CLAUSE) {
        return ZR_FALSE;
    }

    pattern = catchNode->data.catchClause.pattern;
    if (pattern == ZR_NULL || pattern->count == 0) {
        return ZR_TRUE;
    }
    if (pattern->count != 1) {
        return ZR_FALSE;
    }

    parameterNode = pattern->nodes[0];
    return (TZrBool)(parameterNode != ZR_NULL &&
                     parameterNode->type == ZR_AST_PARAMETER &&
                     parameterNode->data.parameter.typeInfo == ZR_NULL);
}

EZrParserCfgCatchMatch cfg_catch_clause_matches_known_throw_kind(
        SZrAstNode *catchNode,
        EZrParserCfgThrowKind knownThrowKind) {
    SZrAstNodeArray *pattern;
    SZrAstNode *parameterNode;
    SZrString *typeName;

    if (catchNode == ZR_NULL || catchNode->type != ZR_AST_CATCH_CLAUSE ||
        knownThrowKind == ZR_PARSER_CFG_THROW_KIND_UNKNOWN) {
        return ZR_PARSER_CFG_CATCH_MATCH_UNKNOWN;
    }

    pattern = catchNode->data.catchClause.pattern;
    if (pattern == ZR_NULL || pattern->count == 0) {
        return ZR_PARSER_CFG_CATCH_MATCH_YES;
    }
    if (pattern->count != 1) {
        return ZR_PARSER_CFG_CATCH_MATCH_UNKNOWN;
    }

    parameterNode = pattern->nodes[0];
    if (parameterNode == ZR_NULL || parameterNode->type != ZR_AST_PARAMETER) {
        return ZR_PARSER_CFG_CATCH_MATCH_UNKNOWN;
    }
    if (parameterNode->data.parameter.typeInfo == ZR_NULL) {
        return ZR_PARSER_CFG_CATCH_MATCH_YES;
    }

    typeName = cfg_type_info_simple_name(parameterNode->data.parameter.typeInfo);
    if (typeName == ZR_NULL) {
        return ZR_PARSER_CFG_CATCH_MATCH_UNKNOWN;
    }

    return cfg_throw_kind_matches_type_name(knownThrowKind, typeName)
                   ? ZR_PARSER_CFG_CATCH_MATCH_YES
                   : ZR_PARSER_CFG_CATCH_MATCH_NO;
}

TZrBool cfg_catch_clause_match_known_throw_kinds(SZrAstNode *catchNode,
                                                 TZrUInt32 knownThrowKindMask,
                                                 TZrBool *outIsPrecise,
                                                 TZrUInt32 *outMatchedMask) {
    SZrAstNodeArray *pattern;
    SZrAstNode *parameterNode;
    SZrString *typeName;
    EZrParserCfgThrowKind kind;
    TZrUInt32 matchedMask = 0u;

    if (outIsPrecise != ZR_NULL) {
        *outIsPrecise = ZR_FALSE;
    }
    if (outMatchedMask != ZR_NULL) {
        *outMatchedMask = 0u;
    }
    if (catchNode == ZR_NULL || catchNode->type != ZR_AST_CATCH_CLAUSE ||
        knownThrowKindMask == 0u || outIsPrecise == ZR_NULL ||
        outMatchedMask == ZR_NULL) {
        return ZR_FALSE;
    }

    pattern = catchNode->data.catchClause.pattern;
    if (pattern == ZR_NULL || pattern->count == 0) {
        *outIsPrecise = ZR_TRUE;
        *outMatchedMask = knownThrowKindMask;
        return ZR_TRUE;
    }
    if (pattern->count != 1) {
        return ZR_TRUE;
    }

    parameterNode = pattern->nodes[0];
    if (parameterNode == ZR_NULL || parameterNode->type != ZR_AST_PARAMETER) {
        return ZR_TRUE;
    }
    if (parameterNode->data.parameter.typeInfo == ZR_NULL) {
        *outIsPrecise = ZR_TRUE;
        *outMatchedMask = knownThrowKindMask;
        return ZR_TRUE;
    }

    typeName = cfg_type_info_simple_name(parameterNode->data.parameter.typeInfo);
    if (typeName == ZR_NULL) {
        return ZR_TRUE;
    }

    for (kind = ZR_PARSER_CFG_THROW_KIND_BOOL;
         kind <= ZR_PARSER_CFG_THROW_KIND_FLOAT;
         kind = (EZrParserCfgThrowKind)((TZrUInt32)kind + 1u)) {
        TZrUInt32 kindMask = cfg_catch_throw_kind_mask(kind);
        if ((knownThrowKindMask & kindMask) != 0u &&
            cfg_throw_kind_matches_type_name(kind, typeName)) {
            matchedMask |= kindMask;
        }
    }

    *outIsPrecise = ZR_TRUE;
    *outMatchedMask = matchedMask;
    return ZR_TRUE;
}
