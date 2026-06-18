#include "parser_internal.h"

static SZrAstNode *parse_switch_binding_identifier(SZrParserState *ps);

static TZrBool switch_current_token_is_move_binding_marker(SZrParserState *ps) {
    EZrToken lookahead;

    if (ps == ZR_NULL ||
        ps->lexer->t.token != ZR_TK_IDENTIFIER ||
        !current_identifier_equals(ps, "move")) {
        return ZR_FALSE;
    }

    lookahead = peek_token(ps);
    return (TZrBool)(lookahead == ZR_TK_IDENTIFIER || lookahead == ZR_TK_TEST);
}

static TZrBool switch_case_header_has_move_binding_marker(SZrParserState *ps) {
    SZrParserCursor cursor;
    TZrInt32 parenDepth = 0;
    TZrInt32 braceDepth = 0;
    TZrInt32 bracketDepth = 0;
    TZrBool found = ZR_FALSE;

    if (ps == ZR_NULL) {
        return ZR_FALSE;
    }

    save_parser_cursor(ps, &cursor);
    while (ps->lexer->t.token != ZR_TK_EOS) {
        if (parenDepth == 0 &&
            braceDepth == 0 &&
            bracketDepth == 0 &&
            ps->lexer->t.token == ZR_TK_RPAREN) {
            break;
        }

        if (switch_current_token_is_move_binding_marker(ps)) {
            found = ZR_TRUE;
            break;
        }

        switch (ps->lexer->t.token) {
            case ZR_TK_LPAREN:
                parenDepth++;
                break;
            case ZR_TK_RPAREN:
                if (parenDepth > 0) {
                    parenDepth--;
                }
                break;
            case ZR_TK_LBRACE:
                braceDepth++;
                break;
            case ZR_TK_RBRACE:
                if (braceDepth > 0) {
                    braceDepth--;
                }
                break;
            case ZR_TK_LBRACKET:
                bracketDepth++;
                break;
            case ZR_TK_RBRACKET:
                if (bracketDepth > 0) {
                    bracketDepth--;
                }
                break;
            default:
                break;
        }
        ZrParser_Lexer_Next(ps->lexer);
    }
    restore_parser_cursor(ps, &cursor);
    return found;
}

SZrAstNode *try_parse_switch_struct_variant_payload_case(SZrParserState *ps, SZrAstNode *value) {
    SZrParserCursor cursor;
    TZrBool savedSuppressErrorOutput;
    TZrParserErrorCallback savedErrorCallback;
    TZrParserStructuredErrorCallback savedStructuredErrorCallback;
    TZrPtr savedErrorUserData;
    SZrAstNode *objectNode;
    SZrAstNode *patternNode;

    if (ps == ZR_NULL ||
        value == ZR_NULL ||
        value->type != ZR_AST_IDENTIFIER_LITERAL ||
        ps->lexer->t.token != ZR_TK_LBRACE) {
        return value;
    }

    save_parser_cursor(ps, &cursor);
    savedSuppressErrorOutput = ps->suppressErrorOutput;
    savedErrorCallback = ps->errorCallback;
    savedStructuredErrorCallback = ps->structuredErrorCallback;
    savedErrorUserData = ps->errorUserData;
    ps->suppressErrorOutput = ZR_TRUE;
    ps->errorCallback = ZR_NULL;
    ps->structuredErrorCallback = ZR_NULL;
    ps->errorUserData = ZR_NULL;
    ps->hasError = ZR_FALSE;
    ps->errorMessage = ZR_NULL;

    objectNode = parse_object_literal(ps);
    if (objectNode == ZR_NULL || ps->lexer->t.token != ZR_TK_RPAREN || ps->hasError) {
        if (objectNode != ZR_NULL) {
            ZrParser_Ast_Free(ps->state, objectNode);
        }
        restore_parser_cursor(ps, &cursor);
        ps->suppressErrorOutput = savedSuppressErrorOutput;
        ps->errorCallback = savedErrorCallback;
        ps->structuredErrorCallback = savedStructuredErrorCallback;
        ps->errorUserData = savedErrorUserData;
        return value;
    }

    ps->suppressErrorOutput = savedSuppressErrorOutput;
    ps->errorCallback = savedErrorCallback;
    ps->structuredErrorCallback = savedStructuredErrorCallback;
    ps->errorUserData = savedErrorUserData;
    ps->hasError = cursor.hasError;
    ps->errorMessage = cursor.errorMessage;

    patternNode = append_primary_member(ps, value, objectNode, value->location);
    if (patternNode == value) {
        ZrParser_Ast_Free(ps->state, objectNode);
    }
    return patternNode != ZR_NULL ? patternNode : value;
}

static SZrAstNode *parse_switch_binding_identifier(SZrParserState *ps) {
    TZrBool isMoveBinding = ZR_FALSE;
    SZrFileRange moveLocation;
    SZrAstNode *identifier;

    if (switch_current_token_is_move_binding_marker(ps)) {
        isMoveBinding = ZR_TRUE;
        moveLocation = get_current_location(ps);
        ZrParser_Lexer_Next(ps->lexer);
    } else {
        moveLocation = get_current_location(ps);
    }

    identifier = parse_identifier(ps);
    if (identifier != ZR_NULL && isMoveBinding) {
        identifier->data.identifier.isMoveBinding = ZR_TRUE;
        identifier->location = ZrParser_FileRange_Merge(moveLocation, identifier->location);
    }

    return identifier;
}

static SZrAstNode *parse_switch_tuple_move_payload_pattern(SZrParserState *ps, TZrBool *outHasMoveBinding) {
    SZrFileRange startLoc;
    SZrFileRange endLoc;
    SZrAstNodeArray *args;
    SZrAstNode *callNode;

    if (outHasMoveBinding != ZR_NULL) {
        *outHasMoveBinding = ZR_FALSE;
    }
    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_LPAREN) {
        return ZR_NULL;
    }

    startLoc = get_current_token_location(ps);
    consume_token(ps, ZR_TK_LPAREN);
    args = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_TINY);
    if (args == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        while (ps->lexer->t.token != ZR_TK_EOS) {
            SZrAstNode *binding = parse_switch_binding_identifier(ps);
            if (binding == ZR_NULL) {
                free_ast_node_array_with_elements(ps->state, args);
                return ZR_NULL;
            }
            if (binding->data.identifier.isMoveBinding && outHasMoveBinding != ZR_NULL) {
                *outHasMoveBinding = ZR_TRUE;
            }
            ZrParser_AstNodeArray_Add(ps->state, args, binding);

            if (ps->lexer->t.token != ZR_TK_COMMA) {
                break;
            }
            consume_token(ps, ZR_TK_COMMA);
            if (ps->lexer->t.token == ZR_TK_RPAREN) {
                break;
            }
        }
    }

    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        report_missing_call_close(ps, startLoc);
        free_ast_node_array_with_elements(ps->state, args);
        return ZR_NULL;
    }
    endLoc = get_current_token_location(ps);
    consume_token(ps, ZR_TK_RPAREN);

    callNode = create_ast_node(ps, ZR_AST_FUNCTION_CALL, ZrParser_FileRange_Merge(startLoc, endLoc));
    if (callNode == ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, args);
        return ZR_NULL;
    }
    callNode->data.functionCall.args = args;
    callNode->data.functionCall.argNames = ZR_NULL;
    callNode->data.functionCall.hasNamedArgs = ZR_FALSE;
    callNode->data.functionCall.genericArguments = ZR_NULL;
    return callNode;
}

static SZrAstNode *parse_switch_struct_move_payload_pattern(SZrParserState *ps, TZrBool *outHasMoveBinding) {
    SZrFileRange startLoc;
    SZrAstNodeArray *properties;
    SZrAstNode *objectNode;

    if (outHasMoveBinding != ZR_NULL) {
        *outHasMoveBinding = ZR_FALSE;
    }
    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_LBRACE) {
        return ZR_NULL;
    }

    startLoc = get_current_token_location(ps);
    consume_token(ps, ZR_TK_LBRACE);
    properties = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_TINY);
    if (properties == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *first = parse_switch_binding_identifier(ps);
        SZrAstNode *key;
        SZrAstNode *value;
        SZrAstNode *kvNode;
        SZrFileRange kvLoc;

        if (first == ZR_NULL) {
            free_ast_node_array_with_elements(ps->state, properties);
            return ZR_NULL;
        }

        if (consume_token(ps, ZR_TK_COLON)) {
            key = first;
            value = parse_switch_binding_identifier(ps);
            if (value == ZR_NULL) {
                ZrParser_Ast_Free(ps->state, key);
                free_ast_node_array_with_elements(ps->state, properties);
                return ZR_NULL;
            }
        } else {
            key = create_identifier_node_with_location(ps,
                                                       first->data.identifier.name,
                                                       first->location);
            value = first;
            if (key == ZR_NULL) {
                ZrParser_Ast_Free(ps->state, value);
                free_ast_node_array_with_elements(ps->state, properties);
                return ZR_NULL;
            }
        }

        if (value->data.identifier.isMoveBinding && outHasMoveBinding != ZR_NULL) {
            *outHasMoveBinding = ZR_TRUE;
        }

        kvLoc = ZrParser_FileRange_Merge(key->location, value->location);
        kvNode = create_ast_node(ps, ZR_AST_KEY_VALUE_PAIR, kvLoc);
        if (kvNode == ZR_NULL) {
            ZrParser_Ast_Free(ps->state, key);
            ZrParser_Ast_Free(ps->state, value);
            free_ast_node_array_with_elements(ps->state, properties);
            return ZR_NULL;
        }
        kvNode->data.keyValuePair.key = key;
        kvNode->data.keyValuePair.value = value;
        kvNode->data.keyValuePair.keyIsComputed = ZR_FALSE;
        ZrParser_AstNodeArray_Add(ps->state, properties, kvNode);

        if (ps->lexer->t.token == ZR_TK_COMMA || ps->lexer->t.token == ZR_TK_SEMICOLON) {
            ZrParser_Lexer_Next(ps->lexer);
            continue;
        }
        if (ps->lexer->t.token != ZR_TK_RBRACE) {
            report_error(ps, "Expected ',' or '}' in switch union struct pattern");
            free_ast_node_array_with_elements(ps->state, properties);
            return ZR_NULL;
        }
    }

    if (ps->lexer->t.token != ZR_TK_RBRACE) {
        report_missing_object_close(ps, startLoc);
        free_ast_node_array_with_elements(ps->state, properties);
        return ZR_NULL;
    }
    consume_token(ps, ZR_TK_RBRACE);

    objectNode = create_ast_node(ps, ZR_AST_OBJECT_LITERAL, ZrParser_FileRange_Merge(startLoc, get_current_location(ps)));
    if (objectNode == ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, properties);
        return ZR_NULL;
    }
    objectNode->data.objectLiteral.properties = properties;
    return objectNode;
}

SZrAstNode *try_parse_switch_move_variant_pattern_case(SZrParserState *ps) {
    SZrParserCursor cursor;
    SZrFileRange startLoc;
    SZrAstNode *base;
    SZrAstNode *payloadNode;
    TZrBool hasMoveBinding = ZR_FALSE;

    if (!switch_case_header_has_move_binding_marker(ps)) {
        return ZR_NULL;
    }

    if (ps == ZR_NULL ||
        (ps->lexer->t.token != ZR_TK_IDENTIFIER && ps->lexer->t.token != ZR_TK_TEST)) {
        return ZR_NULL;
    }

    save_parser_cursor(ps, &cursor);
    startLoc = get_current_location(ps);
    base = parse_identifier(ps);
    if (base == ZR_NULL) {
        restore_parser_cursor(ps, &cursor);
        return ZR_NULL;
    }

    if (ps->lexer->t.token == ZR_TK_DOT) {
        SZrFileRange memberLoc = get_current_token_location(ps);
        SZrAstNode *memberIdentifier;
        SZrAstNode *memberNode;

        consume_token(ps, ZR_TK_DOT);
        memberIdentifier = parse_identifier(ps);
        if (memberIdentifier == ZR_NULL) {
            ZrParser_Ast_Free(ps->state, base);
            restore_parser_cursor(ps, &cursor);
            return ZR_NULL;
        }
        memberNode = create_ast_node(ps, ZR_AST_MEMBER_EXPRESSION, memberLoc);
        if (memberNode == ZR_NULL) {
            ZrParser_Ast_Free(ps->state, memberIdentifier);
            ZrParser_Ast_Free(ps->state, base);
            restore_parser_cursor(ps, &cursor);
            return ZR_NULL;
        }
        memberNode->data.memberExpression.property = memberIdentifier;
        memberNode->data.memberExpression.computed = ZR_FALSE;
        base = append_primary_member(ps, base, memberNode, startLoc);
    }

    if (ps->lexer->t.token != ZR_TK_LPAREN && ps->lexer->t.token != ZR_TK_LBRACE) {
        ZrParser_Ast_Free(ps->state, base);
        restore_parser_cursor(ps, &cursor);
        return ZR_NULL;
    }

    if (ps->lexer->t.token == ZR_TK_LPAREN) {
        payloadNode = parse_switch_tuple_move_payload_pattern(ps, &hasMoveBinding);
    } else {
        payloadNode = parse_switch_struct_move_payload_pattern(ps, &hasMoveBinding);
    }
    if (payloadNode == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, base);
        restore_parser_cursor(ps, &cursor);
        return ZR_NULL;
    }
    if (!hasMoveBinding) {
        ZrParser_Ast_Free(ps->state, payloadNode);
        ZrParser_Ast_Free(ps->state, base);
        restore_parser_cursor(ps, &cursor);
        return ZR_NULL;
    }

    return append_primary_member(ps, base, payloadNode, startLoc);
}
