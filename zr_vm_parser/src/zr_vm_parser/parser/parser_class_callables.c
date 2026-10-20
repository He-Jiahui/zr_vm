#include "parser_class_internal.h"

/* Fixed and variadic parameters keep separate AST owners until publication. */
static TZrBool parse_class_callable_parameters(
        SZrParserState *ps, SZrAstNodeArray **params, SZrParameter **args) {
    *params = ZR_NULL;
    *args = ZR_NULL;
    if (!consume_token(ps, ZR_TK_LPAREN)) {
        expect_token(ps, ZR_TK_LPAREN);
        return ZR_FALSE;
    }
    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        SZrAstNode *argsNode = parse_parameter(ps);
        if (argsNode == ZR_NULL) {
            return ZR_FALSE;
        }
        *args = &argsNode->data.parameter;
        *params = parser_ast_array_new(ps, 0);
    } else {
        *params = parse_parameter_list(ps);
        if (*params != ZR_NULL && consume_token(ps, ZR_TK_COMMA) &&
            ps->lexer->t.token == ZR_TK_PARAMS) {
            SZrAstNode *argsNode = parse_parameter(ps);
            if (argsNode == ZR_NULL) {
                return ZR_FALSE;
            }
            *args = &argsNode->data.parameter;
        }
    }
    if (*params == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!consume_token(ps, ZR_TK_RPAREN)) {
        report_missing_parameter_list_close(ps, get_current_token_location(ps));
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

SZrAstNode *parse_class_method(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);
    SZrFileRange endLoc;
    SZrAstNodeArray *decorators = ZR_NULL;
    SZrAstNode *nameNode = ZR_NULL;
    SZrGenericDeclaration *generic = ZR_NULL;
    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL;
    SZrType *returnType = ZR_NULL;
    SZrAstNode *body = ZR_NULL;
    SZrAstNode *node;
    EZrAccessModifier access;
    TZrBool isStatic;
    TZrUInt32 modifierFlags;
    EZrOwnershipQualifier receiverQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    EZrMethodReceiverModifier receiverModifier = ZR_METHOD_RECEIVER_DEFAULT;
    TZrBool isAsync = ZR_FALSE;

    if (ps->lexer->t.token == ZR_TK_PERCENT) {
        report_removed_percent_syntax(ps);
        return ZR_NULL;
    }
    decorators = parse_leading_decorators(ps);
    if (decorators == ZR_NULL) {
        return ZR_NULL;
    }
    access = parse_access_modifier(ps);
    isStatic = consume_token(ps, ZR_TK_STATIC);
    modifierFlags = parse_declaration_modifier_flags(
            ps, class_member_allowed_modifier_flags());
    if (ps->lexer->t.token == ZR_TK_PERCENT) {
        report_removed_percent_syntax(ps);
        goto failed;
    }
    if (consume_token(ps, ZR_TK_CONST)) {
        receiverModifier = ZR_METHOD_RECEIVER_CONST;
        if (isStatic) {
            report_error(ps, "static const fn is invalid because static functions have no receiver");
        }
    }
    if (current_identifier_equals(ps, "async") && peek_token(ps) == ZR_TK_FN) {
        isAsync = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }
    if (!consume_token(ps, ZR_TK_FN)) {
        if (current_identifier_equals(ps, "func")) {
            report_removed_legacy_syntax(
                    ps, "func method", "Write a method declaration beginning with 'fn'.");
        } else {
            report_removed_legacy_syntax(
                    ps, "keywordless method", "Prefix the method declaration with 'fn'.");
        }
        goto failed;
    }
    nameNode = parse_member_identifier(ps);
    if (nameNode == ZR_NULL) {
        goto failed;
    }
    if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
        generic = parse_generic_declaration(ps, ZR_FALSE);
        if (generic == ZR_NULL) {
            goto failed;
        }
    }
    if (!parse_class_callable_parameters(ps, &params, &args)) {
        goto failed;
    }
    if (consume_token(ps, ZR_TK_COLON)) {
        returnType = parse_type(ps);
        if (returnType == ZR_NULL) {
            goto failed;
        }
    }
    if (!parse_optional_where_clauses(ps, generic)) {
        goto failed;
    }
    if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
        endLoc = get_current_token_location(ps);
        consume_token(ps, ZR_TK_SEMICOLON);
    } else if (ps->lexer->t.token == ZR_TK_LBRACE) {
        body = parse_block(ps);
        if (body == ZR_NULL) {
            goto failed;
        }
        endLoc = body->location;
    } else {
        endLoc = get_current_token_location(ps);
        report_missing_statement_semicolon(ps, "class method", endLoc);
    }
    SZrFileRange methodLocation;
    if (!ZrParser_MergeLocations(ps, startLoc, endLoc, &methodLocation) ||
        (node = create_ast_node(ps, ZR_AST_CLASS_METHOD, methodLocation)) == ZR_NULL) {
        goto failed;
    }
    node->data.classMethod.decorators = decorators;
    node->data.classMethod.access = access;
    node->data.classMethod.isStatic = isStatic;
    node->data.classMethod.modifierFlags = modifierFlags;
    node->data.classMethod.receiverModifier = receiverModifier;
    node->data.classMethod.isAsync = isAsync;
    node->data.classMethod.receiverQualifier = receiverQualifier;
    node->data.classMethod.name = &nameNode->data.identifier;
    node->data.classMethod.nameLocation = nameNode->location;
    node->data.classMethod.generic = generic;
    node->data.classMethod.params = params;
    node->data.classMethod.args = args;
    node->data.classMethod.returnType = returnType;
    node->data.classMethod.body = body;
    return node;

failed:
    free_ast_node_array_with_elements(ps->state, decorators);
    ZrParser_Ast_Free(ps->state, nameNode);
    free_generic_declaration(ps->state, generic);
    free_ast_node_array_with_elements(ps->state, params);
    free_parameter_node_from_ptr(ps->state, args);
    free_owned_type(ps->state, returnType);
    ZrParser_Ast_Free(ps->state, body);
    return ZR_NULL;
}

SZrAstNode *parse_class_meta_function(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrFileRange endLoc;
    SZrFileRange nameLocation;
    SZrAstNode *nameNode = ZR_NULL;
    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL;
    SZrAstNodeArray *superArgs = ZR_NULL;
    SZrType *returnType = ZR_NULL;
    SZrAstNode *body = ZR_NULL;
    SZrAstNode *node;
    EZrAccessModifier access = parse_access_modifier(ps);
    TZrBool isStatic = consume_token(ps, ZR_TK_STATIC);
    TZrUInt32 modifierFlags = parse_declaration_modifier_flags(
            ps, class_member_allowed_modifier_flags());
    EZrMethodReceiverModifier receiverModifier = ZR_METHOD_RECEIVER_DEFAULT;
    TZrBool hasSuperCall = ZR_FALSE;
    SZrFileRange superCallRange;
    memset(&superCallRange, 0, sizeof(superCallRange));

    if (consume_token(ps, ZR_TK_CONST)) {
        receiverModifier = ZR_METHOD_RECEIVER_CONST;
        if (isStatic) {
            report_error(ps, "static const meta function is invalid because static functions have no receiver");
        }
    }
    nameLocation = get_current_token_location(ps);
    if (!consume_token(ps, ZR_TK_AT)) {
        expect_token(ps, ZR_TK_AT);
        return ZR_NULL;
    }
    nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        goto failed;
    }
    if (!ZrParser_MergeLocations(ps, nameLocation, nameNode->location, &nameLocation)) {
        goto failed;
    }
    if (zr_string_equals_literal(nameNode->data.identifier.name, "decorate")) {
        report_error(ps, "@decorate was removed; use a declarationTransform comptime function");
        goto failed;
    }
    if (!parse_class_callable_parameters(ps, &params, &args)) {
        goto failed;
    }
    superArgs = parser_ast_array_new(ps, 0);
    if (superArgs == ZR_NULL) {
        goto failed;
    }
    if (ps->lexer->t.token == ZR_TK_SUPER) {
        hasSuperCall = ZR_TRUE;
        superCallRange = get_current_token_location(ps);
        ZrParser_Lexer_Next(ps->lexer);
        if (!consume_token(ps, ZR_TK_LPAREN)) {
            expect_token(ps, ZR_TK_LPAREN);
            goto failed;
        }
        while (ps->lexer->t.token != ZR_TK_RPAREN) {
            SZrAstNode *arg = parse_expression(ps);
            if (arg == ZR_NULL) {
                goto failed;
            }
            if (!parser_ast_array_add(ps, superArgs, arg)) {
                ZrParser_Ast_Free(ps->state, arg);
                goto failed;
            }
            if (!consume_token(ps, ZR_TK_COMMA)) {
                break;
            }
        }
        if (!ZrParser_MergeLocations(
                    ps, superCallRange, get_current_token_location(ps), &superCallRange)) {
            goto failed;
        }
        if (!consume_token(ps, ZR_TK_RPAREN)) {
            report_missing_parameter_list_close(ps, get_current_token_location(ps));
            goto failed;
        }
    }
    if (consume_token(ps, ZR_TK_COLON)) {
        returnType = parse_type(ps);
        if (returnType == ZR_NULL) {
            goto failed;
        }
    }
    if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
        endLoc = get_current_token_location(ps);
        consume_token(ps, ZR_TK_SEMICOLON);
    } else if (ps->lexer->t.token == ZR_TK_LBRACE) {
        body = parse_block(ps);
        if (body == ZR_NULL) {
            goto failed;
        }
        endLoc = get_current_location(ps);
    } else {
        endLoc = get_current_token_location(ps);
        report_missing_statement_semicolon(ps, "class meta function", endLoc);
    }
    SZrFileRange methodLocation;
    if (!ZrParser_MergeLocations(ps, startLoc, endLoc, &methodLocation) ||
        (node = create_ast_node(ps, ZR_AST_CLASS_META_FUNCTION, methodLocation)) == ZR_NULL) {
        goto failed;
    }
    node->data.classMetaFunction.access = access;
    node->data.classMetaFunction.isStatic = isStatic;
    node->data.classMetaFunction.receiverModifier = receiverModifier;
    node->data.classMetaFunction.modifierFlags = modifierFlags;
    node->data.classMetaFunction.meta = &nameNode->data.identifier;
    node->data.classMetaFunction.nameLocation = nameLocation;
    node->data.classMetaFunction.params = params;
    node->data.classMetaFunction.args = args;
    node->data.classMetaFunction.hasSuperCall = hasSuperCall;
    node->data.classMetaFunction.superArgs = superArgs;
    node->data.classMetaFunction.superCallRange = superCallRange;
    node->data.classMetaFunction.returnType = returnType;
    node->data.classMetaFunction.body = body;
    return node;

failed:
    ZrParser_Ast_Free(ps->state, nameNode);
    free_ast_node_array_with_elements(ps->state, params);
    free_parameter_node_from_ptr(ps->state, args);
    free_ast_node_array_with_elements(ps->state, superArgs);
    free_owned_type(ps->state, returnType);
    ZrParser_Ast_Free(ps->state, body);
    return ZR_NULL;
}
