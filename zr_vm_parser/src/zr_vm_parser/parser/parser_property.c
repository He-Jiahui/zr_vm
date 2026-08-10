#include "parser_internal.h"

static TZrUInt32 property_allowed_modifier_flags(void) {
    return ZR_DECLARATION_MODIFIER_ABSTRACT |
           ZR_DECLARATION_MODIFIER_VIRTUAL |
           ZR_DECLARATION_MODIFIER_OVERRIDE |
           ZR_DECLARATION_MODIFIER_FINAL |
           ZR_DECLARATION_MODIFIER_SHADOW;
}

static TZrBool property_access_modifier_starts_here(SZrParserState *ps) {
    EZrToken token;

    if (ps == ZR_NULL || ps->lexer == ZR_NULL) {
        return ZR_FALSE;
    }
    token = ps->lexer->t.token;
    return token == ZR_TK_PRI || token == ZR_TK_PRO || token == ZR_TK_PUB;
}

static TZrBool property_accessor_starts_here(SZrParserState *ps) {
    SZrParserCursor cursor;
    TZrBool result;

    if (ps == ZR_NULL || ps->lexer == ZR_NULL) {
        return ZR_FALSE;
    }

    save_parser_cursor(ps, &cursor);
    if (property_access_modifier_starts_here(ps)) {
        (void)parse_access_modifier(ps);
    }
    result = (TZrBool)(ps->lexer->t.token == ZR_TK_GET ||
                       ps->lexer->t.token == ZR_TK_SET ||
                       (ps->lexer->t.token == ZR_TK_IDENTIFIER &&
                        current_identifier_equals(ps, "init")));
    restore_parser_cursor(ps, &cursor);
    return result;
}

static void property_free_declaration_parts(SZrParserState *ps,
                                            SZrAstNodeArray *decorators,
                                            SZrType *typeInfo,
                                            SZrAstNodeArray *accessors) {
    if (ps == ZR_NULL) {
        return;
    }
    free_ast_node_array_with_elements(ps->state, decorators);
    free_owned_type(ps->state, typeInfo);
    free_ast_node_array_with_elements(ps->state, accessors);
}

TZrBool parser_property_declaration_starts_here(SZrParserState *ps) {
    SZrParserCursor cursor;
    SZrAstNodeArray *decorators;
    TZrBool result;

    if (ps == ZR_NULL || ps->lexer == ZR_NULL) {
        return ZR_FALSE;
    }
    save_parser_cursor(ps, &cursor);
    decorators = parse_leading_decorators(ps);
    parse_access_modifier(ps);
    consume_token(ps, ZR_TK_STATIC);
    parse_declaration_modifier_flags(ps, property_allowed_modifier_flags());
    result = ps->lexer->t.token == ZR_TK_IDENTIFIER &&
             current_identifier_equals(ps, "property");
    free_ast_node_array_with_elements(ps->state, decorators);
    restore_parser_cursor(ps, &cursor);
    return result;
}

static TZrBool property_consume_accessor_keyword(
        SZrParserState *ps,
        EZrPropertyAccessorKind *outKind,
        SZrFileRange *outKeywordLocation) {
    if (ps == ZR_NULL || outKind == ZR_NULL || outKeywordLocation == ZR_NULL) {
        return ZR_FALSE;
    }
    *outKeywordLocation = get_current_token_location(ps);
    if (ps->lexer->t.token == ZR_TK_GET) {
        *outKind = ZR_PROPERTY_ACCESSOR_GET;
    } else if (ps->lexer->t.token == ZR_TK_SET) {
        *outKind = ZR_PROPERTY_ACCESSOR_SET;
    } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER &&
               current_identifier_equals(ps, "init")) {
        *outKind = ZR_PROPERTY_ACCESSOR_INIT;
    } else {
        return ZR_FALSE;
    }
    ZrParser_Lexer_Next(ps->lexer);
    return ZR_TRUE;
}

static SZrAstNode *parse_property_accessor(SZrParserState *ps,
                                           EZrAccessModifier propertyAccess) {
    SZrFileRange startLocation = get_current_token_location(ps);
    SZrFileRange keywordLocation;
    SZrFileRange endLocation;
    EZrPropertyAccessorKind kind;
    EZrPropertyAccessorBodyKind bodyKind;
    EZrAccessModifier access = propertyAccess;
    TZrBool hasAccessOverride = property_access_modifier_starts_here(ps);
    TZrBool isReferenceResult = ZR_FALSE;
    SZrFileRange referenceLocation;
    SZrAstNode *body = ZR_NULL;
    SZrAstNode *node;

    memset(&referenceLocation, 0, sizeof(referenceLocation));

    if (hasAccessOverride) {
        access = parse_access_modifier(ps);
    }
    if (!property_consume_accessor_keyword(ps, &kind, &keywordLocation)) {
        report_error(ps, "Expected property accessor 'get', 'set', or 'init'");
        return ZR_NULL;
    }

    if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
        bodyKind = ZR_PROPERTY_ACCESSOR_BODY_BODYLESS;
        endLocation = get_current_token_location(ps);
        ZrParser_Lexer_Next(ps->lexer);
    } else if (ps->lexer->t.token == ZR_TK_FAT_ARROW) {
        SZrFileRange delimiterLocation = get_current_token_location(ps);

        bodyKind = ZR_PROPERTY_ACCESSOR_BODY_EXPRESSION;
        ZrParser_Lexer_Next(ps->lexer);
        if (ps->lexer->t.token == ZR_TK_REF) {
            isReferenceResult = ZR_TRUE;
            referenceLocation = get_current_token_location(ps);
            ZrParser_Lexer_Next(ps->lexer);
        }
        body = parse_expression(ps);
        if (body == ZR_NULL) {
            return ZR_NULL;
        }
        endLocation = body->location;
        if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
            endLocation = get_current_token_location(ps);
            ZrParser_Lexer_Next(ps->lexer);
        } else {
            report_missing_statement_semicolon(
                    ps,
                    "property expression accessor",
                    ZrParser_FileRange_Merge(delimiterLocation, endLocation));
        }
    } else if (ps->lexer->t.token == ZR_TK_LBRACE) {
        bodyKind = ZR_PROPERTY_ACCESSOR_BODY_BLOCK;
        body = parse_block(ps);
        if (body == ZR_NULL) {
            return ZR_NULL;
        }
        endLocation = body->location;
    } else {
        bodyKind = ZR_PROPERTY_ACCESSOR_BODY_BODYLESS;
        endLocation = get_current_token_location(ps);
        report_missing_statement_semicolon(ps, "property accessor", endLocation);
    }

    node = create_ast_node(
            ps,
            ZR_AST_PROPERTY_ACCESSOR,
            ZrParser_FileRange_Merge(startLocation, endLocation));
    if (node == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, body);
        return ZR_NULL;
    }
    node->data.propertyAccessor.kind = kind;
    node->data.propertyAccessor.access = access;
    node->data.propertyAccessor.hasAccessOverride = hasAccessOverride;
    node->data.propertyAccessor.bodyKind = bodyKind;
    node->data.propertyAccessor.body = body;
    node->data.propertyAccessor.keywordLocation = keywordLocation;
    node->data.propertyAccessor.isReferenceResult = isReferenceResult;
    node->data.propertyAccessor.referenceLocation = referenceLocation;
    return node;
}

SZrAstNode *parse_property_declaration(SZrParserState *ps,
                                       EZrPropertyContainerKind containerKind) {
    SZrFileRange startLocation = get_current_token_location(ps);
    SZrFileRange endLocation;
    SZrAstNodeArray *decorators;
    EZrAccessModifier access;
    TZrBool isStatic;
    TZrUInt32 modifierFlags;
    SZrAstNode *nameNode;
    SZrType *typeInfo;
    SZrAstNodeArray *accessors;
    SZrAstNode *node;
    TZrBool bodyOpened = ZR_FALSE;
    TZrBool bodyCloseReported = ZR_FALSE;

    ZR_UNUSED_PARAMETER(containerKind);
    if (ps == ZR_NULL || ps->lexer == ZR_NULL) {
        return ZR_NULL;
    }

    decorators = parse_leading_decorators(ps);
    access = parse_access_modifier(ps);
    isStatic = consume_token(ps, ZR_TK_STATIC);
    modifierFlags = parse_declaration_modifier_flags(
            ps, property_allowed_modifier_flags());
    if (ps->lexer->t.token != ZR_TK_IDENTIFIER ||
        !current_identifier_equals(ps, "property")) {
        report_error(ps, "Expected contextual 'property' declaration keyword");
        property_free_declaration_parts(ps, decorators, ZR_NULL, ZR_NULL);
        return ZR_NULL;
    }
    ZrParser_Lexer_Next(ps->lexer);

    nameNode = parse_member_identifier(ps);
    if (nameNode == ZR_NULL) {
        property_free_declaration_parts(ps, decorators, ZR_NULL, ZR_NULL);
        return ZR_NULL;
    }
    if (!consume_token(ps, ZR_TK_COLON)) {
        report_error(ps, "Expected ':' before property type");
    }
    typeInfo = parse_type(ps);
    if (typeInfo == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, nameNode);
        property_free_declaration_parts(ps, decorators, ZR_NULL, ZR_NULL);
        return ZR_NULL;
    }

    accessors = ZrParser_AstNodeArray_New(
            ps->state, ZR_PARSER_INITIAL_CAPACITY_TINY);
    if (accessors == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, nameNode);
        property_free_declaration_parts(ps, decorators, typeInfo, ZR_NULL);
        return ZR_NULL;
    }
    if (ps->lexer->t.token == ZR_TK_LBRACE) {
        endLocation = get_current_token_location(ps);
    }
    if (!consume_token(ps, ZR_TK_LBRACE)) {
        report_missing_declaration_body_open(
                ps, "property declaration", get_current_token_location(ps));
        endLocation = nameNode->location;
    } else {
        bodyOpened = ZR_TRUE;
    }

    while (bodyOpened && ps->lexer->t.token != ZR_TK_RBRACE &&
           ps->lexer->t.token != ZR_TK_EOS) {
        if (!property_accessor_starts_here(ps)) {
            report_missing_declaration_body_close(
                    ps, "property declaration", startLocation);
            bodyCloseReported = ZR_TRUE;
            break;
        }
        SZrAstNode *accessor = parse_property_accessor(ps, access);

        if (accessor != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, accessors, accessor);
            endLocation = accessor->location;
        } else if (ps->lexer->t.token != ZR_TK_RBRACE &&
                   ps->lexer->t.token != ZR_TK_EOS) {
            ZrParser_Lexer_Next(ps->lexer);
        }
    }

    if (bodyOpened) {
        if (ps->lexer->t.token == ZR_TK_RBRACE) {
            endLocation = get_current_token_location(ps);
            ZrParser_Lexer_Next(ps->lexer);
            consume_token(ps, ZR_TK_SEMICOLON);
        } else if (!bodyCloseReported) {
            report_missing_declaration_body_close(
                    ps, "property declaration", startLocation);
        }
    }

    node = create_ast_node(
            ps,
            ZR_AST_PROPERTY_DECLARATION,
            ZrParser_FileRange_Merge(startLocation, endLocation));
    if (node == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, nameNode);
        property_free_declaration_parts(ps, decorators, typeInfo, accessors);
        return ZR_NULL;
    }
    node->data.propertyDeclaration.decorators = decorators;
    node->data.propertyDeclaration.access = access;
    node->data.propertyDeclaration.isStatic = isStatic;
    node->data.propertyDeclaration.modifierFlags = modifierFlags;
    node->data.propertyDeclaration.name = &nameNode->data.identifier;
    node->data.propertyDeclaration.nameLocation = nameNode->location;
    node->data.propertyDeclaration.typeInfo = typeInfo;
    node->data.propertyDeclaration.accessors = accessors;
    return node;
}
