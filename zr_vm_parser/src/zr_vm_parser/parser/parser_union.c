#include "parser_internal.h"

static SZrAstNode *parse_union_field(SZrParserState *ps) {
    SZrFileRange startLoc;
    SZrAstNode *nameNode;
    SZrIdentifier *name;
    SZrType *typeInfo;
    SZrAstNode *node;
    SZrFileRange endLoc;

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    startLoc = get_current_location(ps);
    nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    name = &nameNode->data.identifier;

    if (!consume_token(ps, ZR_TK_COLON)) {
        report_error(ps, "Expected ':' after union variant field name");
        free_identifier_node_from_ptr(ps->state, name);
        return ZR_NULL;
    }

    typeInfo = parse_type(ps);
    if (typeInfo == ZR_NULL) {
        free_identifier_node_from_ptr(ps->state, name);
        return ZR_NULL;
    }

    endLoc = get_current_location(ps);
    node = create_ast_node(ps, ZR_AST_PARAMETER, ZrParser_FileRange_Merge(startLoc, endLoc));
    if (node == ZR_NULL) {
        free_identifier_node_from_ptr(ps->state, name);
        free_owned_type(ps->state, typeInfo);
        return ZR_NULL;
    }

    node->data.parameter.name = name;
    node->data.parameter.nameLocation = nameNode->location;
    node->data.parameter.typeInfo = typeInfo;
    node->data.parameter.defaultValue = ZR_NULL;
    node->data.parameter.decorators = ZR_NULL;
    node->data.parameter.passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;
    node->data.parameter.genericKind = ZR_GENERIC_PARAMETER_TYPE;
    node->data.parameter.variance = ZR_GENERIC_VARIANCE_NONE;
    node->data.parameter.genericTypeConstraints = ZR_NULL;
    node->data.parameter.genericRequiresClass = ZR_FALSE;
    node->data.parameter.genericRequiresStruct = ZR_FALSE;
    node->data.parameter.genericRequiresNew = ZR_FALSE;
    return node;
}

static SZrAstNodeArray *parse_union_tuple_fields(SZrParserState *ps) {
    SZrAstNodeArray *fields;

    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);

    fields = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_TINY);
    if (fields == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token != ZR_TK_RPAREN && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *field = parse_union_field(ps);
        if (field == ZR_NULL) {
            free_ast_node_array_with_elements(ps->state, fields);
            return ZR_NULL;
        }
        ZrParser_AstNodeArray_Add(ps->state, fields, field);

        if (!consume_token(ps, ZR_TK_COMMA)) {
            break;
        }
    }

    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        report_missing_parameter_list_close(ps, get_current_token_location(ps));
        free_ast_node_array_with_elements(ps->state, fields);
        return ZR_NULL;
    }
    consume_token(ps, ZR_TK_RPAREN);
    return fields;
}

static SZrAstNodeArray *parse_union_struct_fields(SZrParserState *ps) {
    SZrAstNodeArray *fields;

    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    fields = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_TINY);
    if (fields == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *field = parse_union_field(ps);
        if (field == ZR_NULL) {
            free_ast_node_array_with_elements(ps->state, fields);
            return ZR_NULL;
        }
        ZrParser_AstNodeArray_Add(ps->state, fields, field);

        if (ps->lexer->t.token == ZR_TK_SEMICOLON || ps->lexer->t.token == ZR_TK_COMMA) {
            ZrParser_Lexer_Next(ps->lexer);
            continue;
        }
        if (ps->lexer->t.token != ZR_TK_RBRACE) {
            report_error(ps, "Expected ';' or '}' after union struct variant field");
            free_ast_node_array_with_elements(ps->state, fields);
            return ZR_NULL;
        }
    }

    if (ps->lexer->t.token != ZR_TK_RBRACE) {
        report_missing_declaration_body_close(ps, "union variant", get_current_token_location(ps));
        free_ast_node_array_with_elements(ps->state, fields);
        return ZR_NULL;
    }
    consume_token(ps, ZR_TK_RBRACE);
    return fields;
}

SZrAstNode *parse_union_variant(SZrParserState *ps) {
    SZrFileRange startLoc;
    SZrAstNodeArray *decorators;
    SZrAstNode *nameNode;
    SZrIdentifier *name;
    EZrUnionVariantKind kind = ZR_UNION_VARIANT_UNIT;
    TZrBool isDefaultUsingVariant = ZR_FALSE;
    SZrAstNodeArray *fields = ZR_NULL;
    SZrFileRange endLoc;
    SZrAstNode *node;

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    startLoc = get_current_location(ps);
    decorators = parse_leading_decorators(ps);
    if (consume_token(ps, ZR_TK_AT)) {
        isDefaultUsingVariant = ZR_TRUE;
    }

    nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        if (decorators != ZR_NULL) {
            free_ast_node_array_with_elements(ps->state, decorators);
        }
        return ZR_NULL;
    }
    name = &nameNode->data.identifier;

    if (ps->lexer->t.token == ZR_TK_LPAREN) {
        kind = ZR_UNION_VARIANT_TUPLE;
        fields = parse_union_tuple_fields(ps);
        if (fields == ZR_NULL) {
            free_identifier_node_from_ptr(ps->state, name);
            if (decorators != ZR_NULL) {
                free_ast_node_array_with_elements(ps->state, decorators);
            }
            return ZR_NULL;
        }
    } else if (ps->lexer->t.token == ZR_TK_LBRACE) {
        kind = ZR_UNION_VARIANT_STRUCT;
        fields = parse_union_struct_fields(ps);
        if (fields == ZR_NULL) {
            free_identifier_node_from_ptr(ps->state, name);
            if (decorators != ZR_NULL) {
                free_ast_node_array_with_elements(ps->state, decorators);
            }
            return ZR_NULL;
        }
    }

    if (ps->lexer->t.token == ZR_TK_SEMICOLON || ps->lexer->t.token == ZR_TK_COMMA) {
        ZrParser_Lexer_Next(ps->lexer);
    }

    endLoc = get_current_location(ps);
    node = create_ast_node(ps, ZR_AST_UNION_VARIANT, ZrParser_FileRange_Merge(startLoc, endLoc));
    if (node == ZR_NULL) {
        if (fields != ZR_NULL) {
            free_ast_node_array_with_elements(ps->state, fields);
        }
        if (decorators != ZR_NULL) {
            free_ast_node_array_with_elements(ps->state, decorators);
        }
        free_identifier_node_from_ptr(ps->state, name);
        return ZR_NULL;
    }

    node->data.unionVariant.name = name;
    node->data.unionVariant.kind = kind;
    node->data.unionVariant.fields = fields;
    node->data.unionVariant.decorators = decorators;
    node->data.unionVariant.isDefaultUsingVariant = isDefaultUsingVariant;
    return node;
}

SZrAstNode *parse_union_declaration(SZrParserState *ps) {
    SZrFileRange startLoc;
    SZrFileRange bodyOpenLoc;
    SZrAstNodeArray *decorators;
    EZrAccessModifier accessModifier;
    SZrAstNode *nameNode;
    SZrIdentifier *name;
    SZrGenericDeclaration *generic = ZR_NULL;
    SZrAstNodeArray *variants;
    TZrBool bodyOpened = ZR_FALSE;
    TZrUInt32 defaultUsingVariantCount = 0u;
    SZrAstNode *node;
    SZrFileRange endLoc;

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    startLoc = get_current_location(ps);
    bodyOpenLoc = startLoc;
    decorators = parse_leading_decorators(ps);
    accessModifier = parse_access_modifier(ps);

    expect_token(ps, ZR_TK_UNION);
    ZrParser_Lexer_Next(ps->lexer);

    nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        if (decorators != ZR_NULL) {
            free_ast_node_array_with_elements(ps->state, decorators);
        }
        return ZR_NULL;
    }
    name = &nameNode->data.identifier;

    if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
        generic = parse_generic_declaration(ps, ZR_FALSE);
        if (generic == ZR_NULL) {
            free_identifier_node_from_ptr(ps->state, name);
            if (decorators != ZR_NULL) {
                free_ast_node_array_with_elements(ps->state, decorators);
            }
            return ZR_NULL;
        }
    }

    if (!parse_optional_where_clauses(ps, generic)) {
        free_identifier_node_from_ptr(ps->state, name);
        free_generic_declaration(ps->state, generic);
        if (decorators != ZR_NULL) {
            free_ast_node_array_with_elements(ps->state, decorators);
        }
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_LBRACE) {
        report_missing_declaration_body_open(ps, "union declaration", get_current_token_location(ps));
    } else {
        bodyOpenLoc = get_current_token_location(ps);
        bodyOpened = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    variants = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_SMALL);
    if (variants == ZR_NULL) {
        free_identifier_node_from_ptr(ps->state, name);
        free_generic_declaration(ps->state, generic);
        if (decorators != ZR_NULL) {
            free_ast_node_array_with_elements(ps->state, decorators);
        }
        return ZR_NULL;
    }

    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *variant = parse_union_variant(ps);
        if (variant != ZR_NULL) {
            if (variant->data.unionVariant.isDefaultUsingVariant) {
                defaultUsingVariantCount++;
                if (defaultUsingVariantCount > 1u) {
                    report_error(ps,
                                 "union_default_variant_duplicate: only one union variant can be marked with '@'");
                }
            }
            ZrParser_AstNodeArray_Add(ps->state, variants, variant);
        } else {
            if (ps->hasError) {
                break;
            }
            if (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
                ZrParser_Lexer_Next(ps->lexer);
            }
        }
    }

    if (ps->lexer->t.token != ZR_TK_RBRACE) {
        if (ps->lexer->t.token == ZR_TK_EOS) {
            if (bodyOpened) {
                report_missing_declaration_body_close(ps, "union declaration", bodyOpenLoc);
            }
        } else {
            expect_token(ps, ZR_TK_RBRACE);
        }
    }
    consume_token(ps, ZR_TK_RBRACE);

    endLoc = get_current_location(ps);
    node = create_ast_node(ps, ZR_AST_UNION_DECLARATION, ZrParser_FileRange_Merge(startLoc, endLoc));
    if (node == ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, variants);
        free_identifier_node_from_ptr(ps->state, name);
        free_generic_declaration(ps->state, generic);
        if (decorators != ZR_NULL) {
            free_ast_node_array_with_elements(ps->state, decorators);
        }
        return ZR_NULL;
    }

    node->data.unionDeclaration.name = name;
    node->data.unionDeclaration.generic = generic;
    node->data.unionDeclaration.variants = variants;
    node->data.unionDeclaration.decorators = decorators;
    node->data.unionDeclaration.accessModifier = accessModifier;
    return node;
}
