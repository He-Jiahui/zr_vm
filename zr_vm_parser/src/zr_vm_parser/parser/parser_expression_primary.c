#include "parser_internal.h"
#include "zr_vm_parser/type_system.h"

TZrBool try_get_ownership_qualifier(SZrString *name, EZrOwnershipQualifier *qualifier) {
    if (qualifier == ZR_NULL) {
        return ZR_FALSE;
    }

    *qualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }

    if (zr_string_equals_literal(name, "unique")) {
        *qualifier = ZR_OWNERSHIP_QUALIFIER_UNIQUE;
        return ZR_TRUE;
    }
    if (zr_string_equals_literal(name, "shared")) {
        *qualifier = ZR_OWNERSHIP_QUALIFIER_SHARED;
        return ZR_TRUE;
    }
    if (zr_string_equals_literal(name, "weak")) {
        *qualifier = ZR_OWNERSHIP_QUALIFIER_WEAK;
        return ZR_TRUE;
    }
    if (zr_string_equals_literal(name, "borrowed")) {
        *qualifier = ZR_OWNERSHIP_QUALIFIER_BORROWED;
        return ZR_TRUE;
    }
    if (zr_string_equals_literal(name, "borrow")) {
        *qualifier = ZR_OWNERSHIP_QUALIFIER_BORROWED;
        return ZR_TRUE;
    }
    if (zr_string_equals_literal(name, "loaned")) {
        *qualifier = ZR_OWNERSHIP_QUALIFIER_LOANED;
        return ZR_TRUE;
    }
    if (zr_string_equals_literal(name, "loan")) {
        *qualifier = ZR_OWNERSHIP_QUALIFIER_LOANED;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

SZrAstNode *append_primary_member(SZrParserState *ps, SZrAstNode *base, SZrAstNode *memberNode,
                                         SZrFileRange startLoc) {
    if (ps == ZR_NULL || base == ZR_NULL || memberNode == ZR_NULL) {
        return base;
    }

    if (base->type == ZR_AST_PRIMARY_EXPRESSION) {
        if (base->data.primaryExpression.members == ZR_NULL) {
            base->data.primaryExpression.members = ZrParser_AstNodeArray_New(ps->state, 1);
            if (base->data.primaryExpression.members == ZR_NULL) {
                return base;
            }
        }
        ZrParser_AstNodeArray_Add(ps->state, base->data.primaryExpression.members, memberNode);
        return base;
    }

    SZrAstNode *primaryNode = create_ast_node(ps, ZR_AST_PRIMARY_EXPRESSION, startLoc);
    if (primaryNode == ZR_NULL) {
        return base;
    }
    primaryNode->data.primaryExpression.property = base;
    primaryNode->data.primaryExpression.members = ZrParser_AstNodeArray_New(ps->state, 1);
    if (primaryNode->data.primaryExpression.members == ZR_NULL) {
        return base;
    }
    ZrParser_AstNodeArray_Add(ps->state, primaryNode->data.primaryExpression.members, memberNode);
    return primaryNode;
}

TZrBool is_lambda_expression_after_lparen(SZrParserState *ps) {
    SZrParserCursor savedCursor;
    TZrInt32 depth = 0;
    TZrBool isLambda = ZR_FALSE;

    if (ps->lexer->t.token != ZR_TK_LPAREN) {
        return ZR_FALSE;
    }

    save_parser_cursor(ps, &savedCursor);
    ZrParser_Lexer_Next(ps->lexer);
    depth = 1;
    while (depth > 0 && ps->lexer->t.token != ZR_TK_EOS) {
        if (ps->lexer->t.token == ZR_TK_LPAREN) {
            depth++;
        } else if (ps->lexer->t.token == ZR_TK_RPAREN) {
            depth--;
        }
        ZrParser_Lexer_Next(ps->lexer);
    }

    /*
     * 当前这层括号只有在“匹配的右括号之后紧跟箭头”时才是 lambda 参数列表。
     * 这样可以保留 ((lambda))(call) 这类分组/立即调用写法，避免把外层分组误判成 lambda。
     */
    if (depth == 0 && (ps->lexer->t.token == ZR_TK_THIN_ARROW ||
                       ps->lexer->t.token == ZR_TK_FAT_ARROW)) {
        isLambda = ZR_TRUE;
    }

    restore_parser_cursor(ps, &savedCursor);
    return isLambda;
}

SZrAstNodeArray *create_empty_argument_list(SZrParserState *ps) {
    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_AstNodeArray_New(ps->state, 0);
}

TZrBool reject_named_construct_arguments(SZrParserState *ps, SZrArray *argNames, SZrFileRange location) {
    if (ps == ZR_NULL || argNames == ZR_NULL || argNames->length == 0) {
        return ZR_TRUE;
    }

    for (TZrSize i = 0; i < argNames->length; i++) {
        SZrString **namePtr = (SZrString **) ZrCore_Array_Get(argNames, i);
        if (namePtr != ZR_NULL && *namePtr != ZR_NULL) {
            ZrCore_Array_Free(ps->state, argNames);
            ZrCore_Memory_RawFreeWithType(ps->state->global, argNames, sizeof(SZrArray), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            report_error(ps, "Prototype construction does not support named arguments");
            ps->hasError = ZR_TRUE;
            ZR_UNUSED_PARAMETER(location);
            return ZR_FALSE;
        }
    }

    ZrCore_Array_Free(ps->state, argNames);
    ZrCore_Memory_RawFreeWithType(ps->state->global, argNames, sizeof(SZrArray), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    return ZR_TRUE;
}

SZrAstNode *create_prototype_reference_node(SZrParserState *ps, SZrAstNode *target, SZrFileRange location) {
    SZrAstNode *node;

    if (ps == ZR_NULL || target == ZR_NULL) {
        return ZR_NULL;
    }

    node = create_ast_node(ps, ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION, location);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.prototypeReferenceExpression.target = target;
    return node;
}

SZrAstNode *create_construct_expression_node(SZrParserState *ps, SZrAstNode *target, SZrAstNodeArray *args,
                                                    EZrOwnershipQualifier ownershipQualifier, TZrBool isUsing,
                                                    TZrBool isNew, EZrOwnershipBuiltinKind builtinKind,
                                                    SZrFileRange location) {
    SZrAstNode *node;

    if (ps == ZR_NULL || target == ZR_NULL) {
        return ZR_NULL;
    }

    node = create_ast_node(ps, ZR_AST_CONSTRUCT_EXPRESSION, location);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.constructExpression.target = target;
    node->data.constructExpression.args = args;
    node->data.constructExpression.ownershipQualifier = ownershipQualifier;
    node->data.constructExpression.isUsing = isUsing;
    node->data.constructExpression.isNew = isNew;
    node->data.constructExpression.isResourceSurface = ZR_FALSE;
    node->data.constructExpression.builtinKind = builtinKind;
    return node;
}

static void free_argument_name_array(SZrParserState *ps, SZrArray *argNames) {
    if (ps == ZR_NULL || argNames == ZR_NULL) {
        return;
    }

    ZrCore_Array_Free(ps->state, argNames);
    ZrCore_Memory_RawFreeWithType(ps->state->global,
                                  argNames,
                                  sizeof(SZrArray),
                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
}

static TZrBool reject_legacy_ownership_generic_call(SZrParserState *ps,
                                                    SZrAstNode *base,
                                                    SZrAstNodeArray *genericArguments,
                                                    SZrAstNodeArray *args,
                                                    SZrArray *argNames,
                                                    SZrFileRange startLoc,
                                                    SZrAstNode **outNode) {
    EZrOwnershipQualifier ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    SZrFileRange fullLoc;

    if (outNode != ZR_NULL) {
        *outNode = ZR_NULL;
    }

    if (ps == ZR_NULL || base == ZR_NULL || base->type != ZR_AST_IDENTIFIER_LITERAL ||
        (!ZrParser_OwnershipGenericNameToQualifier(base->data.identifier.name, &ownershipQualifier) &&
         !zr_string_equals_literal(base->data.identifier.name, "Borrow") &&
         !zr_string_equals_literal(base->data.identifier.name, "Loan"))) {
        return ZR_FALSE;
    }

    fullLoc = ZrParser_FileRange_Merge(startLoc, get_current_location(ps));
    report_removed_legacy_syntax_at(
            ps,
            fullLoc,
            ps->lexer->t.token,
            "ownership generic constructor",
            "Use `own Type(...)` to create a resource, then `.share()` or an explicit `ref` binding.");
    if (args != ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, args);
    }
    free_argument_name_array(ps, argNames);
    if (genericArguments != ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, genericArguments);
    }
    ZrParser_Ast_Free(ps->state, base);
    return ZR_TRUE;
}

static SZrAstNode *parse_generic_construct_target(SZrParserState *ps) {
    SZrAstNode *genericNode;
    SZrAstNode *typeNode;
    SZrFileRange startLoc;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_IDENTIFIER || peek_token(ps) != ZR_TK_LESS_THAN) {
        return ZR_NULL;
    }

    startLoc = get_current_location(ps);
    genericNode = parse_generic_type(ps);
    if (genericNode == ZR_NULL) {
        return ZR_NULL;
    }

    typeNode = create_ast_node(ps, ZR_AST_TYPE, ZrParser_FileRange_Merge(startLoc, get_current_location(ps)));
    if (typeNode == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, genericNode);
        return ZR_NULL;
    }

    typeNode->data.type.name = genericNode;
    return typeNode;
}

static SZrAstNode *try_parse_generic_type_member_root(SZrParserState *ps) {
    SZrParserCursor cursor;
    TZrBool savedSuppressErrorOutput;
    TZrParserErrorCallback savedErrorCallback;
    TZrParserStructuredErrorCallback savedStructuredErrorCallback;
    TZrPtr savedErrorUserData;
    SZrAstNode *target;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_IDENTIFIER || peek_token(ps) != ZR_TK_LESS_THAN) {
        return ZR_NULL;
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

    target = parse_generic_construct_target(ps);
    if (target != ZR_NULL && ps->lexer->t.token == ZR_TK_DOT) {
        ps->suppressErrorOutput = savedSuppressErrorOutput;
        ps->errorCallback = savedErrorCallback;
        ps->structuredErrorCallback = savedStructuredErrorCallback;
        ps->errorUserData = savedErrorUserData;
        ps->hasError = cursor.hasError;
        ps->errorMessage = cursor.errorMessage;
        return target;
    }

    if (target != ZR_NULL) {
        ZrParser_Ast_Free(ps->state, target);
    }
    restore_parser_cursor(ps, &cursor);
    ps->suppressErrorOutput = savedSuppressErrorOutput;
    ps->errorCallback = savedErrorCallback;
    ps->structuredErrorCallback = savedStructuredErrorCallback;
    ps->errorUserData = savedErrorUserData;
    return ZR_NULL;
}

SZrAstNode *parse_prototype_path_expression(SZrParserState *ps) {
    SZrAstNode *base;
    SZrFileRange startLoc;

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_IDENTIFIER) {
        report_error(ps, "Expected identifier or member path");
        return ZR_NULL;
    }

    startLoc = get_current_location(ps);
    base = parse_identifier(ps);
    if (base == ZR_NULL) {
        return ZR_NULL;
    }

    while (consume_token(ps, ZR_TK_DOT)) {
        SZrAstNode *property;
        SZrAstNode *memberNode;

        if (ps->lexer->t.token != ZR_TK_IDENTIFIER) {
            report_error(ps, "Expected identifier after '.' in prototype path");
            return base;
        }

        if (peek_token(ps) == ZR_TK_LESS_THAN) {
            property = parse_generic_construct_target(ps);
        } else {
            property = parse_identifier(ps);
        }
        if (property == ZR_NULL) {
            return base;
        }

        memberNode = create_ast_node(ps, ZR_AST_MEMBER_EXPRESSION, startLoc);
        if (memberNode == ZR_NULL) {
            ZrParser_Ast_Free(ps->state, property);
            return base;
        }
        memberNode->data.memberExpression.property = property;
        memberNode->data.memberExpression.computed = ZR_FALSE;
        base = append_primary_member(ps, base, memberNode, startLoc);
    }

    return base;
}

SZrAstNode *parse_construct_expression(SZrParserState *ps,
                                              SZrFileRange startLoc,
                                              EZrOwnershipQualifier ownershipQualifier,
                                              TZrBool isUsing,
                                              EZrOwnershipBuiltinKind builtinKind) {
    SZrAstNode *target = ZR_NULL;
    SZrAstNodeArray *args = ZR_NULL;
    SZrArray *argNames = ZR_NULL;
    SZrAstNode *constructNode;
    SZrFileRange fullLoc;

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_NEW);
    if (ps->lexer->t.token != ZR_TK_NEW) {
        return ZR_NULL;
    }
    ZrParser_Lexer_Next(ps->lexer);

    if (ps->lexer->t.token == ZR_TK_LPAREN) {
        consume_token(ps, ZR_TK_LPAREN);
        target = parse_expression(ps);
        expect_token(ps, ZR_TK_RPAREN);
        consume_token(ps, ZR_TK_RPAREN);
    } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER && peek_token(ps) == ZR_TK_LESS_THAN) {
        target = parse_generic_construct_target(ps);
    } else {
        target = parse_prototype_path_expression(ps);
    }

    if (target == ZR_NULL) {
        return ZR_NULL;
    }

    if (consume_token(ps, ZR_TK_LPAREN)) {
        args = parse_argument_list(ps, &argNames, ZR_NULL);
        expect_token(ps, ZR_TK_RPAREN);
        consume_token(ps, ZR_TK_RPAREN);
        if (!reject_named_construct_arguments(ps, argNames, startLoc)) {
            if (args != ZR_NULL) {
                ZrParser_AstNodeArray_Free(ps->state, args);
            }
            ZrParser_Ast_Free(ps->state, target);
            return ZR_NULL;
        }
    } else {
        args = create_empty_argument_list(ps);
    }

    fullLoc = ZrParser_FileRange_Merge(startLoc, get_current_location(ps));
    constructNode = create_construct_expression_node(ps,
                                                     target,
                                                     args,
                                                     ownershipQualifier,
                                                     isUsing,
                                                     ZR_TRUE,
                                                     builtinKind,
                                                     fullLoc);
    if (constructNode == ZR_NULL) {
        if (args != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, args);
        }
        ZrParser_Ast_Free(ps->state, target);
        return ZR_NULL;
    }

    return constructNode;
}

static SZrAstNode *parse_resource_own_expression(SZrParserState *ps) {
    SZrFileRange startLoc;
    SZrAstNode *target;
    SZrAstNodeArray *args;
    SZrArray *argNames = ZR_NULL;
    SZrAstNode *node;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_IDENTIFIER ||
        !current_identifier_equals(ps, "own")) {
        return ZR_NULL;
    }

    startLoc = get_current_token_location(ps);
    ZrParser_Lexer_Next(ps->lexer);
    if (ps->lexer->t.token == ZR_TK_IDENTIFIER && peek_token(ps) == ZR_TK_LESS_THAN) {
        target = parse_generic_construct_target(ps);
    } else {
        target = parse_prototype_path_expression(ps);
    }
    if (target == ZR_NULL) {
        return ZR_NULL;
    }

    if (!consume_token(ps, ZR_TK_LPAREN)) {
        report_error(ps, "Expected '(' after resource construction target");
        ZrParser_Ast_Free(ps->state, target);
        return ZR_NULL;
    }
    args = parse_argument_list(ps, &argNames, ZR_NULL);
    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);
    if (!reject_named_construct_arguments(ps, argNames, startLoc)) {
        if (args != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, args);
        }
        ZrParser_Ast_Free(ps->state, target);
        return ZR_NULL;
    }

    node = create_construct_expression_node(
            ps,
            target,
            args,
            ZR_OWNERSHIP_QUALIFIER_UNIQUE,
            ZR_FALSE,
            ZR_TRUE,
            ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE,
            ZrParser_FileRange_Merge(startLoc, get_current_location(ps)));
    if (node != ZR_NULL) {
        node->data.constructExpression.isResourceSurface = ZR_TRUE;
    }
    return node;
}

static SZrAstNode *parse_resource_drop_expression(SZrParserState *ps) {
    SZrFileRange startLoc;
    SZrAstNode *target;
    SZrAstNodeArray *args;
    SZrAstNode *node;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_IDENTIFIER ||
        !current_identifier_equals(ps, "drop")) {
        return ZR_NULL;
    }

    startLoc = get_current_token_location(ps);
    ZrParser_Lexer_Next(ps->lexer);
    if (!consume_token(ps, ZR_TK_LPAREN)) {
        report_error(ps, "Expected '(' after 'drop'");
        return ZR_NULL;
    }
    target = parse_expression(ps);
    if (target == ZR_NULL) {
        return ZR_NULL;
    }
    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);
    args = create_empty_argument_list(ps);
    node = create_construct_expression_node(
            ps,
            target,
            args,
            ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_FALSE,
            ZR_FALSE,
            ZR_OWNERSHIP_BUILTIN_KIND_RELEASE,
            ZrParser_FileRange_Merge(startLoc, get_current_location(ps)));
    if (node != ZR_NULL) {
        node->data.constructExpression.isResourceSurface = ZR_TRUE;
    }
    return node;
}

SZrAstNode *parse_reference_expression(SZrParserState *ps) {
    SZrFileRange startLoc;
    SZrAstNode *target;
    SZrAstNodeArray *args;
    SZrAstNode *node;

    if (ps == ZR_NULL || ps->lexer == ZR_NULL || ps->lexer->t.token != ZR_TK_REF) {
        return ZR_NULL;
    }

    startLoc = get_current_token_location(ps);
    ZrParser_Lexer_Next(ps->lexer);
    if (ps->lexer->t.token == ZR_TK_REF) {
        report_error(ps, "Nested 'ref ref' is not valid; use 'ref existingRef' to reborrow");
        return ZR_NULL;
    }

    target = parse_unary_expression(ps);
    if (target == ZR_NULL) {
        report_error(ps, "Expected an expression after 'ref'");
        return ZR_NULL;
    }

    args = create_empty_argument_list(ps);
    if (args == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, target);
        return ZR_NULL;
    }

    node = create_construct_expression_node(
            ps,
            target,
            args,
            ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_FALSE,
            ZR_FALSE,
            ZR_OWNERSHIP_BUILTIN_KIND_BORROW,
            ZrParser_FileRange_Merge(startLoc, target->location));
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, args);
        ZrParser_Ast_Free(ps->state, target);
    }
    return node;
}

SZrAstNode *parse_reserved_import_expression(SZrParserState *ps) {
    SZrFileRange startLoc;
    SZrAstNode *modulePath;
    SZrAstNode *node;

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    startLoc = get_current_location(ps);
    if (ps->lexer->t.token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "import")) {
        ZrParser_Lexer_Next(ps->lexer);
    } else {
        return ZR_NULL;
    }
    if (ps->lexer->t.token != ZR_TK_LPAREN) {
        report_removed_legacy_syntax(
                ps,
                "bare import path",
                "Use `import(\"module.path\")`; static import paths must be string literals in parentheses.");
        return ZR_NULL;
    }
    modulePath = parse_normalized_module_path(ps, "import");
    if (modulePath == ZR_NULL) {
        return ZR_NULL;
    }

    node = create_ast_node(ps, ZR_AST_IMPORT_EXPRESSION,
                           ZrParser_FileRange_Merge(startLoc, get_current_location(ps)));
    if (node == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, modulePath);
        return ZR_NULL;
    }

    node->data.importExpression.modulePath = modulePath;
    return node;
}

static TZrBool type_literal_probe_can_start(EZrToken token) {
    return token == ZR_TK_IDENTIFIER || token == ZR_TK_TEST || token == ZR_TK_LBRACKET ||
           token == ZR_TK_LPAREN;
}

static TZrBool type_literal_probe_identifier_supports_bare_array_literal(SZrString *name) {
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_string_equals_literal(name, "int") ||
           zr_string_equals_literal(name, "uint") ||
           zr_string_equals_literal(name, "float") ||
           zr_string_equals_literal(name, "bool") ||
           zr_string_equals_literal(name, "string") ||
           zr_string_equals_literal(name, "null") ||
           zr_string_equals_literal(name, "void") ||
           zr_string_equals_literal(name, "i8") ||
           zr_string_equals_literal(name, "u8") ||
           zr_string_equals_literal(name, "i16") ||
           zr_string_equals_literal(name, "u16") ||
           zr_string_equals_literal(name, "i32") ||
           zr_string_equals_literal(name, "u32") ||
           zr_string_equals_literal(name, "i64") ||
           zr_string_equals_literal(name, "u64") ||
           zr_string_equals_literal(name, "Integer") ||
           zr_string_equals_literal(name, "Float") ||
           zr_string_equals_literal(name, "Double") ||
           zr_string_equals_literal(name, "String") ||
           zr_string_equals_literal(name, "Bool") ||
           zr_string_equals_literal(name, "Byte") ||
           zr_string_equals_literal(name, "Char") ||
           zr_string_equals_literal(name, "UInt64") ||
           zr_string_equals_literal(name, "TypeInfo") ||
           zr_string_equals_literal(name, "Object") ||
           zr_string_equals_literal(name, "Module");
}

static TZrBool type_literal_probe_has_unambiguous_marker(const SZrType *typeInfo) {
    if (typeInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    if ((typeInfo->dimensions > 0 || typeInfo->hasArraySizeConstraint) &&
        typeInfo->name != ZR_NULL &&
        typeInfo->name->type == ZR_AST_IDENTIFIER_LITERAL &&
        typeInfo->subType == ZR_NULL &&
        !type_literal_probe_identifier_supports_bare_array_literal(typeInfo->name->data.identifier.name)) {
        return ZR_FALSE;
    }

    if (typeInfo->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE ||
        typeInfo->dimensions > 0 ||
        typeInfo->hasArraySizeConstraint) {
        return ZR_TRUE;
    }

    if (typeInfo->name != ZR_NULL && typeInfo->name->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_TRUE;
    }

    return typeInfo->subType != ZR_NULL ? type_literal_probe_has_unambiguous_marker(typeInfo->subType) : ZR_FALSE;
}

static TZrBool type_literal_probe_is_terminator(EZrToken token) {
    return token == ZR_TK_SEMICOLON || token == ZR_TK_COMMA || token == ZR_TK_RPAREN ||
           token == ZR_TK_RBRACE || token == ZR_TK_RBRACKET || token == ZR_TK_DOT || token == ZR_TK_EOS;
}

static SZrAstNode *try_parse_unambiguous_type_literal_expression(SZrParserState *ps) {
    SZrParserCursor cursor;
    TZrBool savedSuppressErrorOutput;
    TZrParserErrorCallback savedErrorCallback;
    TZrParserStructuredErrorCallback savedStructuredErrorCallback;
    TZrPtr savedErrorUserData;
    SZrType *typeInfo;
    SZrAstNode *node;
    SZrFileRange startLoc;

    if (ps == ZR_NULL || !type_literal_probe_can_start(ps->lexer->t.token)) {
        return ZR_NULL;
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
    startLoc = get_current_location(ps);
    typeInfo = parse_type(ps);
    if (typeInfo == ZR_NULL ||
        !type_literal_probe_has_unambiguous_marker(typeInfo) ||
        !type_literal_probe_is_terminator(ps->lexer->t.token)) {
        if (typeInfo != ZR_NULL) {
            free_owned_type(ps->state, typeInfo);
        }
        restore_parser_cursor(ps, &cursor);
        ps->suppressErrorOutput = savedSuppressErrorOutput;
        ps->errorCallback = savedErrorCallback;
        ps->structuredErrorCallback = savedStructuredErrorCallback;
        ps->errorUserData = savedErrorUserData;
        return ZR_NULL;
    }

    ps->suppressErrorOutput = savedSuppressErrorOutput;
    ps->errorCallback = savedErrorCallback;
    ps->structuredErrorCallback = savedStructuredErrorCallback;
    ps->errorUserData = savedErrorUserData;
    ps->hasError = cursor.hasError;
    ps->errorMessage = cursor.errorMessage;
    node = create_ast_node(ps,
                           ZR_AST_TYPE_LITERAL_EXPRESSION,
                           ZrParser_FileRange_Merge(startLoc, get_current_location(ps)));
    if (node == ZR_NULL) {
        free_owned_type(ps->state, typeInfo);
        restore_parser_cursor(ps, &cursor);
        ps->suppressErrorOutput = savedSuppressErrorOutput;
        ps->errorCallback = savedErrorCallback;
        ps->structuredErrorCallback = savedStructuredErrorCallback;
        ps->errorUserData = savedErrorUserData;
        return ZR_NULL;
    }

    node->data.typeLiteralExpression.typeInfo = typeInfo;
    return node;
}

// 解析成员访问和函数调用

static TZrBool is_member_name_token(EZrToken token) {
    return token == ZR_TK_IDENTIFIER || token == ZR_TK_TEST ||
           token == ZR_TK_UNION || token == ZR_TK_FN || token == ZR_TK_REF ||
           token == ZR_TK_LET || token == ZR_TK_YIELD ||
           token == ZR_TK_TYPEID || token == ZR_TK_TYPEOF ||
           (token >= ZR_TK_MODULE && token <= ZR_TK_NAN);
}

static SZrAstNode *parse_member_name(SZrParserState *ps) {
    SZrFileRange memberLoc;
    EZrToken token;
    SZrString *name = ZR_NULL;

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    token = ps->lexer->t.token;
    if (!is_member_name_token(token)) {
        report_error(ps, "Expected identifier");
        return ZR_NULL;
    }

    memberLoc = get_current_token_location(ps);
    if (token == ZR_TK_IDENTIFIER) {
        name = ps->lexer->t.seminfo.stringValue;
    } else if (token == ZR_TK_TEST) {
        name = ps->lexer->t.seminfo.stringValue;
        if (name == ZR_NULL) {
            name = ZrCore_String_Create(ps->state, "test", 4);
        }
    } else {
        const TZrChar *tokenStr = ZrParser_Lexer_TokenToString(ps->lexer, token);
        if (tokenStr != ZR_NULL) {
            name = ZrCore_String_Create(ps->state, (TZrNativeString)tokenStr, strlen(tokenStr));
        }
    }

    if (name == ZR_NULL) {
        report_error(ps, "Failed to create member name");
        return ZR_NULL;
    }

    ZrParser_Lexer_Next(ps->lexer);
    return create_identifier_node_with_location(ps, name, memberLoc);
}

static SZrAstNodeArray *try_parse_explicit_generic_call_arguments(SZrParserState *ps) {
    SZrParserCursor cursor;
    TZrBool savedSuppressErrorOutput;
    TZrParserErrorCallback savedErrorCallback;
    TZrParserStructuredErrorCallback savedStructuredErrorCallback;
    TZrPtr savedErrorUserData;
    SZrAstNodeArray *genericArguments;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_LESS_THAN) {
        return ZR_NULL;
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

    genericArguments = parse_generic_argument_list(ps);
    if (genericArguments == ZR_NULL || ps->lexer->t.token != ZR_TK_LPAREN) {
        if (genericArguments != ZR_NULL) {
            free_ast_node_array_with_elements(ps->state, genericArguments);
        }
        restore_parser_cursor(ps, &cursor);
        ps->suppressErrorOutput = savedSuppressErrorOutput;
        ps->errorCallback = savedErrorCallback;
        ps->structuredErrorCallback = savedStructuredErrorCallback;
        ps->errorUserData = savedErrorUserData;
        return ZR_NULL;
    }

    ps->suppressErrorOutput = savedSuppressErrorOutput;
    ps->errorCallback = savedErrorCallback;
    ps->structuredErrorCallback = savedStructuredErrorCallback;
    ps->errorUserData = savedErrorUserData;
    ps->hasError = cursor.hasError;
    ps->errorMessage = cursor.errorMessage;
    return genericArguments;
}

SZrAstNode *parse_member_access(SZrParserState *ps, SZrAstNode *base) {
    SZrFileRange startLoc = base->location;

    while (ZR_TRUE) {
        // 点号成员访问
        if (ps->lexer->t.token == ZR_TK_DOT) {
            SZrFileRange dotLocation = get_current_token_location(ps);
            consume_token(ps, ZR_TK_DOT);
            // 成员名上下文允许关键字以普通名称形式出现，例如 zr.ffi.out
            if (!is_member_name_token(ps->lexer->t.token)) {
                report_missing_member_name(ps, dotLocation);
                return ZR_NULL;
            }
            TZrNativeString baseName =
                base->type == ZR_AST_IDENTIFIER_LITERAL && base->data.identifier.name != ZR_NULL
                    ? ZrCore_String_GetNativeString(base->data.identifier.name)
                    : ZR_NULL;
            if (baseName != ZR_NULL && strcmp(baseName, "zr") == 0 &&
                current_identifier_equals(ps, "import") &&
                peek_token(ps) == ZR_TK_LPAREN) {
                report_error(ps, "Internal module helper 'zr.import' is not available; use import(\"module.path\")");
                ZrParser_Lexer_Next(ps->lexer);
                if (consume_token(ps, ZR_TK_LPAREN)) {
                    skip_balanced_after_open_paren(ps);
                }
                return base;
            }

            SZrAstNode *property = parse_member_name(ps);
            if (property == ZR_NULL) {
                // parse_member_name 已经报告了错误
                return ZR_NULL;
            }

            SZrAstNode *memberNode = create_ast_node(ps, ZR_AST_MEMBER_EXPRESSION, startLoc);
            if (memberNode == ZR_NULL) {
                return base;
            }
            memberNode->data.memberExpression.property = property;
            memberNode->data.memberExpression.computed = ZR_FALSE;

            base = append_primary_member(ps, base, memberNode, startLoc);
        }
        // 方括号成员访问
        else if (ps->lexer->t.token == ZR_TK_LBRACKET) {
            SZrFileRange bracketLocation = get_current_token_location(ps);
            consume_token(ps, ZR_TK_LBRACKET);
            SZrAstNode *property = parse_expression(ps);
            if (property == ZR_NULL) {
                return base;
            }
            if (ps->lexer->t.token != ZR_TK_RBRACKET) {
                report_missing_index_close(ps, bracketLocation);
                return ZR_NULL;
            }
            consume_token(ps, ZR_TK_RBRACKET);

            SZrAstNode *memberNode = create_ast_node(ps, ZR_AST_MEMBER_EXPRESSION, startLoc);
            if (memberNode == ZR_NULL) {
                return base;
            }
            memberNode->data.memberExpression.property = property;
            memberNode->data.memberExpression.computed = ZR_TRUE;

            base = append_primary_member(ps, base, memberNode, startLoc);
        }
        // 显式泛型函数调用
        else if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
            SZrAstNodeArray *genericArguments = try_parse_explicit_generic_call_arguments(ps);
            SZrArray *argNames;
            SZrArray *argumentMarkers;
            SZrAstNodeArray *args;
            SZrAstNode *callNode;

            if (genericArguments == ZR_NULL) {
                break;
            }

            SZrFileRange callOpenLocation;
            SZrFileRange callCloseLocation;

            callOpenLocation = get_current_token_location(ps);
            consume_token(ps, ZR_TK_LPAREN);
            argNames = ZR_NULL;
            argumentMarkers = ZR_NULL;
            args = parse_argument_list(ps, &argNames, &argumentMarkers);
            if (ps->lexer->t.token != ZR_TK_RPAREN) {
                report_missing_call_close(ps, callOpenLocation);
                if (args != ZR_NULL) {
                    ZrParser_AstNodeArray_Free(ps->state, args);
                }
                if (argNames != ZR_NULL) {
                    ZrCore_Array_Free(ps->state, argNames);
                    ZrCore_Memory_RawFreeWithType(ps->state->global, argNames, sizeof(SZrArray),
                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
                if (argumentMarkers != ZR_NULL) {
                    ZrCore_Array_Free(ps->state, argumentMarkers);
                    ZrCore_Memory_RawFreeWithType(ps->state->global, argumentMarkers, sizeof(SZrArray),
                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
                free_ast_node_array_with_elements(ps->state, genericArguments);
                return ZR_NULL;
            }
            callCloseLocation = get_current_token_location(ps);
            consume_token(ps, ZR_TK_RPAREN);

            if (reject_legacy_ownership_generic_call(ps,
                                                     base,
                                                     genericArguments,
                                                     args,
                                                     argNames,
                                                     startLoc,
                                                     &callNode)) {
                if (argumentMarkers != ZR_NULL) {
                    ZrCore_Array_Free(ps->state, argumentMarkers);
                    ZrCore_Memory_RawFreeWithType(ps->state->global, argumentMarkers, sizeof(SZrArray),
                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
                if (callNode == ZR_NULL) {
                    return ZR_NULL;
                }
                base = callNode;
                continue;
            }

            callNode = create_ast_node(
                    ps,
                    ZR_AST_FUNCTION_CALL,
                    ZrParser_FileRange_Merge(callOpenLocation, callCloseLocation));
            if (callNode == ZR_NULL) {
                if (args != ZR_NULL) {
                    ZrParser_AstNodeArray_Free(ps->state, args);
                }
                if (argNames != ZR_NULL) {
                    ZrCore_Array_Free(ps->state, argNames);
                    ZrCore_Memory_RawFreeWithType(ps->state->global, argNames, sizeof(SZrArray),
                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
                if (argumentMarkers != ZR_NULL) {
                    ZrCore_Array_Free(ps->state, argumentMarkers);
                    ZrCore_Memory_RawFreeWithType(ps->state->global, argumentMarkers, sizeof(SZrArray),
                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
                free_ast_node_array_with_elements(ps->state, genericArguments);
                return base;
            }

            callNode->data.functionCall.args = args;
            callNode->data.functionCall.argNames = argNames;
            callNode->data.functionCall.genericArguments = genericArguments;
            callNode->data.functionCall.argumentMarkers = argumentMarkers;
            callNode->data.functionCall.hasNamedArgs = ZR_FALSE;
            if (argNames != ZR_NULL && argNames->length > 0) {
                for (TZrSize i = 0; i < argNames->length; i++) {
                    SZrString **namePtr = (SZrString **) ZrCore_Array_Get(argNames, i);
                    if (namePtr != ZR_NULL && *namePtr != ZR_NULL) {
                        callNode->data.functionCall.hasNamedArgs = ZR_TRUE;
                        break;
                    }
                }
            }

            base = append_primary_member(ps, base, callNode, startLoc);
        }
        // 函数调用
        else if (ps->lexer->t.token == ZR_TK_LPAREN) {
            SZrFileRange callOpenLocation = get_current_token_location(ps);
            SZrFileRange callCloseLocation;
            SZrArray *argNames = ZR_NULL;
            SZrArray *argumentMarkers = ZR_NULL;
            SZrAstNodeArray *args;

            consume_token(ps, ZR_TK_LPAREN);
            args = parse_argument_list(ps, &argNames, &argumentMarkers);
            if (ps->lexer->t.token != ZR_TK_RPAREN) {
                report_missing_call_close(ps, callOpenLocation);
                if (args != ZR_NULL) {
                    ZrParser_AstNodeArray_Free(ps->state, args);
                }
                if (argNames != ZR_NULL) {
                    ZrCore_Array_Free(ps->state, argNames);
                    ZrCore_Memory_RawFreeWithType(ps->state->global, argNames, sizeof(SZrArray),
                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
                if (argumentMarkers != ZR_NULL) {
                    ZrCore_Array_Free(ps->state, argumentMarkers);
                    ZrCore_Memory_RawFreeWithType(ps->state->global, argumentMarkers, sizeof(SZrArray),
                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
                return ZR_NULL;
            }
            callCloseLocation = get_current_token_location(ps);
            consume_token(ps, ZR_TK_RPAREN);

            if (base->type == ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION) {
                SZrAstNode *target = base->data.prototypeReferenceExpression.target;
                SZrAstNode *constructNode;
                SZrFileRange fullLoc;

                if (!reject_named_construct_arguments(ps, argNames, startLoc)) {
                    if (args != ZR_NULL) {
                        ZrParser_AstNodeArray_Free(ps->state, args);
                    }
                    if (argumentMarkers != ZR_NULL) {
                        ZrCore_Array_Free(ps->state, argumentMarkers);
                        ZrCore_Memory_RawFreeWithType(ps->state->global, argumentMarkers, sizeof(SZrArray),
                                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
                    }
                    return base;
                }

                if (argumentMarkers != ZR_NULL) {
                    ZrCore_Array_Free(ps->state, argumentMarkers);
                    ZrCore_Memory_RawFreeWithType(ps->state->global, argumentMarkers, sizeof(SZrArray),
                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                    argumentMarkers = ZR_NULL;
                }

                base->data.prototypeReferenceExpression.target = ZR_NULL;
                ZrCore_Memory_RawFreeWithType(ps->state->global, base, sizeof(SZrAstNode), ZR_MEMORY_NATIVE_TYPE_ARRAY);

                fullLoc = ZrParser_FileRange_Merge(startLoc, get_current_location(ps));
                constructNode = create_construct_expression_node(ps,
                                                                 target,
                                                                 args,
                                                                 ZR_OWNERSHIP_QUALIFIER_NONE,
                                                                 ZR_FALSE,
                                                                 ZR_FALSE,
                                                                 ZR_OWNERSHIP_BUILTIN_KIND_NONE,
                                                                 fullLoc);
                if (constructNode == ZR_NULL) {
                    if (args != ZR_NULL) {
                        ZrParser_AstNodeArray_Free(ps->state, args);
                    }
                    ZrParser_Ast_Free(ps->state, target);
                    return ZR_NULL;
                }

                base = constructNode;
                continue;
            }

            SZrAstNode *callNode = create_ast_node(
                    ps,
                    ZR_AST_FUNCTION_CALL,
                    ZrParser_FileRange_Merge(callOpenLocation, callCloseLocation));
            if (callNode == ZR_NULL) {
                if (args != ZR_NULL) {
                    ZrParser_AstNodeArray_Free(ps->state, args);
                }
                if (argNames != ZR_NULL) {
                    ZrCore_Array_Free(ps->state, argNames);
                    ZrCore_Memory_RawFreeWithType(ps->state->global, argNames, sizeof(SZrArray),
                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
                if (argumentMarkers != ZR_NULL) {
                    ZrCore_Array_Free(ps->state, argumentMarkers);
                    ZrCore_Memory_RawFreeWithType(ps->state->global, argumentMarkers, sizeof(SZrArray),
                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
                return base;
            }
            callNode->data.functionCall.args = args;
            callNode->data.functionCall.argNames = argNames;
            callNode->data.functionCall.genericArguments = ZR_NULL;
            callNode->data.functionCall.argumentMarkers = argumentMarkers;
            // 检查是否有命名参数
            callNode->data.functionCall.hasNamedArgs = ZR_FALSE;
            if (argNames != ZR_NULL && argNames->length > 0) {
                // 检查是否有非空的参数名
                for (TZrSize i = 0; i < argNames->length; i++) {
                    SZrString **namePtr = (SZrString **) ZrCore_Array_Get(argNames, i);
                    if (namePtr != ZR_NULL && *namePtr != ZR_NULL) {
                        callNode->data.functionCall.hasNamedArgs = ZR_TRUE;
                        break;
                    }
                }
            }

            base = append_primary_member(ps, base, callNode, startLoc);
        } else if (ps->lexer->t.token == ZR_TK_LBRACE) {
            TZrBool handled = ZR_FALSE;
            SZrAstNode *bracedMember = try_parse_braced_primary_member(ps, base, startLoc, &handled);

            if (!handled) {
                break;
            }
            if (bracedMember == ZR_NULL) {
                return ZR_NULL;
            }
            base = bracedMember;
        } else {
            break;
        }
    }

    return base;
}

// 解析主表达式

SZrAstNode *parse_primary_expression(SZrParserState *ps) {
    ZR_UNUSED_PARAMETER(get_current_location(ps));
    EZrToken token = ps->lexer->t.token;
    SZrAstNode *base = ZR_NULL;

    if (token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "async") &&
        peek_token(ps) == ZR_TK_FN) {
        SZrFileRange asyncLocation = get_current_token_location(ps);

        ZrParser_Lexer_Next(ps->lexer);
        base = parse_fn_expression(ps);
        if (base == ZR_NULL) {
            return ZR_NULL;
        }
        base->data.lambdaExpression.isAsync = ZR_TRUE;
        base->location = ZrParser_FileRange_Merge(asyncLocation, base->location);
        return parse_member_access(ps, base);
    }
    if (token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "import")) {
        base = parse_reserved_import_expression(ps);
        return base != ZR_NULL ? parse_member_access(ps, base) : ZR_NULL;
    }
    if (token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "own") &&
        peek_token(ps) == ZR_TK_IDENTIFIER) {
        base = parse_resource_own_expression(ps);
        return base != ZR_NULL ? parse_member_access(ps, base) : ZR_NULL;
    }
    if (token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "drop")) {
        base = parse_resource_drop_expression(ps);
        return base != ZR_NULL ? parse_member_access(ps, base) : ZR_NULL;
    }

    if (token == ZR_TK_IDENTIFIER || token == ZR_TK_TEST) {
        base = try_parse_generic_type_member_root(ps);
        if (base != ZR_NULL) {
            return parse_member_access(ps, base);
        }

        base = try_parse_unambiguous_type_literal_expression(ps);
        if (base != ZR_NULL) {
            return parse_member_access(ps, base);
        }
    }

    // 字面量
    if (token == ZR_TK_BOOLEAN || token == ZR_TK_INTEGER || token == ZR_TK_FLOAT || token == ZR_TK_STRING ||
        token == ZR_TK_TEMPLATE_STRING || token == ZR_TK_CHAR || token == ZR_TK_NULL || token == ZR_TK_INFINITY ||
        token == ZR_TK_NEG_INFINITY || token == ZR_TK_NAN) {
        base = parse_literal(ps);
    }
    // 标识符
    else if (token == ZR_TK_IDENTIFIER || token == ZR_TK_TEST) {
        base = parse_identifier(ps);
        // 标识符解析后，需要继续解析可能的成员访问和函数调用
        // 这将在函数末尾统一处理
    }
    // super 关键字在表达式上下文中作为一个显式基类接收器使用
    else if (token == ZR_TK_SUPER) {
        SZrFileRange superLoc = get_current_token_location(ps);
        SZrString *superName = ZrCore_String_Create(ps->state, "super", 5);
        if (superName == ZR_NULL) {
            report_error(ps, "Failed to allocate 'super' identifier");
            return ZR_NULL;
        }
        base = create_identifier_node_with_location(ps, superName, superLoc);
        ZrParser_Lexer_Next(ps->lexer);
    }
    // 数组字面量
    else if (token == ZR_TK_LBRACKET) {
        base = parse_array_literal(ps);
    }
    // 生成器表达式（{{}}）
    else if (token == ZR_TK_LBRACE) {
        // 检查是否是生成器表达式 {{ 还是普通对象字面量 {
        EZrToken lookahead = peek_token(ps);
        if (lookahead == ZR_TK_LBRACE) {
            report_removed_legacy_syntax(
                    ps,
                    "double-brace generator expression",
                    "Use an iterator function with `yield` instead of `{{ ... }}` and `out`.");
            return ZR_NULL;
        } else {
            // 检查是否是对象字面量（有键值对）还是块表达式
            // 更精确的判断逻辑：
            // 1. 如果下一个token是标识符、字符串或数字，可能是对象字面量的键
            // 2. 如果下一个token是语句关键字（var, if, while等），是块表达式
            // 3. 如果下一个token是右大括号，是空对象字面量
            EZrToken objectLookahead = peek_token(ps);
            if (objectLookahead == ZR_TK_IDENTIFIER || objectLookahead == ZR_TK_STRING ||
                objectLookahead == ZR_TK_INTEGER ||
                objectLookahead == ZR_TK_FLOAT || objectLookahead == ZR_TK_RBRACE) {
                // 可能是对象字面量
                base = parse_object_literal(ps);
            } else if (objectLookahead == ZR_TK_VAR || objectLookahead == ZR_TK_IF ||
                       objectLookahead == ZR_TK_WHILE ||
                       objectLookahead == ZR_TK_FOR || objectLookahead == ZR_TK_RETURN ||
                       objectLookahead == ZR_TK_BREAK ||
                       objectLookahead == ZR_TK_CONTINUE || objectLookahead == ZR_TK_THROW ||
                       objectLookahead == ZR_TK_TRY ||
                       objectLookahead == ZR_TK_SWITCH) {
                // 是块表达式
                base = parse_block(ps);
            } else {
                // 默认尝试解析为对象字面量
                base = parse_object_literal(ps);
            }
        }
    }
    else if (token == ZR_TK_FN) {
        base = parse_fn_expression(ps);
    }
    // Lambda 表达式或括号表达式
    else if (token == ZR_TK_LPAREN) {
        if (is_lambda_expression_after_lparen(ps)) {
            report_removed_legacy_syntax(
                    ps,
                    "lambda without fn",
                    "Write lambdas as `fn(parameters): ReturnType => expression` or `fn(parameters): ReturnType { ... }`.");
            return ZR_NULL;
        } else {
            SZrFileRange groupOpenLocation = get_current_token_location(ps);
            consume_token(ps, ZR_TK_LPAREN);
            base = parse_expression(ps);
            if (ps->lexer->t.token != ZR_TK_RPAREN) {
                report_missing_group_close(ps, groupOpenLocation);
                return ZR_NULL;
            }
            consume_token(ps, ZR_TK_RPAREN);
        }
    } else {
        report_error(ps, "Expected primary expression");
        return ZR_NULL;
    }

    if (base == ZR_NULL) {
        return ZR_NULL;
    }

    // 解析成员访问和函数调用
    // 注意：此时 lexer 应该指向标识符后的下一个 token（可能是 .、[、( 或其他）
    return parse_member_access(ps, base);
}

// 解析一元表达式
