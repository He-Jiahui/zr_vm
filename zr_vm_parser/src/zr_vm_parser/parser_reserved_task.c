#include "parser_internal.h"

static SZrAstNode *zr_task_create_string_literal(SZrParserState *ps,
                                                 const TZrChar *text,
                                                 SZrFileRange location) {
    SZrString *value;

    if (ps == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }

    value = ZrCore_String_Create(ps->state, (TZrNativeString)text, strlen(text));
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return create_string_literal_node_with_location(ps, value, ZR_FALSE, value, location);
}

static SZrAstNode *zr_task_create_identifier(SZrParserState *ps,
                                             const TZrChar *name,
                                             SZrFileRange location) {
    SZrString *value;

    if (ps == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    value = ZrCore_String_Create(ps->state, (TZrNativeString)name, strlen(name));
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return create_identifier_node_with_location(ps, value, location);
}

static SZrAstNode *zr_task_create_type_node_from_info(SZrParserState *ps,
                                                      SZrType *type,
                                                      SZrFileRange location) {
    SZrAstNode *typeNode;

    if (ps == ZR_NULL || type == ZR_NULL) {
        return ZR_NULL;
    }

    typeNode = create_ast_node(ps, ZR_AST_TYPE, location);
    if (typeNode == ZR_NULL) {
        free_type_info(ps->state, type);
        ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return ZR_NULL;
    }

    typeNode->data.type = *type;
    ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    return typeNode;
}

static SZrType *zr_task_wrap_return_type_async(SZrParserState *ps, SZrType *returnType, SZrFileRange location) {
    SZrAstNode *typeArgumentNode;
    SZrAstNodeArray *genericArguments;
    SZrAstNode *genericNameNode;
    SZrAstNode *asyncIdentifierNode;
    SZrType *wrappedType;
    const TZrChar *typeName;

    if (ps == ZR_NULL || returnType == ZR_NULL) {
        return returnType;
    }

    if (returnType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_NONE && returnType->subType == ZR_NULL &&
        returnType->dimensions == 0 && !returnType->hasArraySizeConstraint && returnType->name != ZR_NULL &&
        returnType->name->type == ZR_AST_GENERIC_TYPE && returnType->name->data.genericType.name != ZR_NULL &&
        returnType->name->data.genericType.name->name != ZR_NULL) {
        typeName = ZrCore_String_GetNativeString(returnType->name->data.genericType.name->name);
        if (typeName != ZR_NULL &&
            (strcmp(typeName, "TaskRunner") == 0 || strcmp(typeName, "zr.task.TaskRunner") == 0)) {
            return returnType;
        }
    }

    typeArgumentNode = zr_task_create_type_node_from_info(ps, returnType, location);
    if (typeArgumentNode == ZR_NULL) {
        return ZR_NULL;
    }

    genericArguments = ZrParser_AstNodeArray_New(ps->state, 1);
    if (genericArguments == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, typeArgumentNode);
        return ZR_NULL;
    }
    ZrParser_AstNodeArray_Add(ps->state, genericArguments, typeArgumentNode);

    asyncIdentifierNode = zr_task_create_identifier(ps, "zr.task.TaskRunner", location);
    if (asyncIdentifierNode == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, genericArguments);
        return ZR_NULL;
    }

    genericNameNode = create_ast_node(ps, ZR_AST_GENERIC_TYPE, location);
    if (genericNameNode == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, asyncIdentifierNode);
        ZrParser_AstNodeArray_Free(ps->state, genericArguments);
        return ZR_NULL;
    }

    genericNameNode->data.genericType.name = &asyncIdentifierNode->data.identifier;
    genericNameNode->data.genericType.params = genericArguments;

    wrappedType =
            ZrCore_Memory_RawMallocWithType(ps->state->global, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (wrappedType == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, genericNameNode);
        return ZR_NULL;
    }

    wrappedType->name = genericNameNode;
    wrappedType->subType = ZR_NULL;
    wrappedType->dimensions = 0;
    wrappedType->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    wrappedType->isDecoratorPseudoType = ZR_FALSE;
    wrappedType->isImplicitBuiltinType = ZR_TRUE;
    wrappedType->arrayFixedSize = 0;
    wrappedType->arrayMinSize = 0;
    wrappedType->arrayMaxSize = 0;
    wrappedType->hasArraySizeConstraint = ZR_FALSE;
    wrappedType->arraySizeExpression = ZR_NULL;
    return wrappedType;
}

static SZrAstNode *zr_task_create_module_import(SZrParserState *ps, SZrFileRange location) {
    SZrAstNode *modulePath;
    SZrAstNode *node;

    modulePath = zr_task_create_string_literal(ps, "zr.task", location);
    if (modulePath == ZR_NULL) {
        return ZR_NULL;
    }

    node = create_ast_node(ps, ZR_AST_IMPORT_EXPRESSION, location);
    if (node == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, modulePath);
        return ZR_NULL;
    }

    node->data.importExpression.modulePath = modulePath;
    return node;
}

static SZrAstNode *zr_task_create_member_access(SZrParserState *ps,
                                                SZrAstNode *base,
                                                const TZrChar *memberName,
                                                SZrFileRange location) {
    SZrAstNode *property;
    SZrAstNode *memberNode;

    if (ps == ZR_NULL || base == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    property = zr_task_create_identifier(ps, memberName, location);
    if (property == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, base);
        return ZR_NULL;
    }

    memberNode = create_ast_node(ps, ZR_AST_MEMBER_EXPRESSION, location);
    if (memberNode == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, property);
        ZrParser_Ast_Free(ps->state, base);
        return ZR_NULL;
    }

    memberNode->data.memberExpression.property = property;
    memberNode->data.memberExpression.computed = ZR_FALSE;
    return append_primary_member(ps, base, memberNode, location);
}

static SZrAstNode *zr_task_create_function_call(SZrParserState *ps,
                                                SZrAstNode *base,
                                                SZrAstNodeArray *args,
                                                SZrFileRange location) {
    SZrAstNode *callNode;

    if (ps == ZR_NULL || base == ZR_NULL) {
        return ZR_NULL;
    }

    callNode = create_ast_node(ps, ZR_AST_FUNCTION_CALL, location);
    if (callNode == ZR_NULL) {
        if (args != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, args);
        }
        ZrParser_Ast_Free(ps->state, base);
        return ZR_NULL;
    }

    callNode->data.functionCall.args = args;
    callNode->data.functionCall.argNames = ZR_NULL;
    callNode->data.functionCall.genericArguments = ZR_NULL;
    callNode->data.functionCall.hasNamedArgs = ZR_FALSE;
    return append_primary_member(ps, base, callNode, location);
}

static SZrAstNode *zr_task_create_module_member_call(SZrParserState *ps,
                                                     const TZrChar *memberName,
                                                     SZrAstNodeArray *args,
                                                     SZrFileRange location) {
    SZrAstNode *importNode = zr_task_create_module_import(ps, location);
    SZrAstNode *memberAccess;

    if (importNode == ZR_NULL) {
        if (args != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, args);
        }
        return ZR_NULL;
    }

    memberAccess = zr_task_create_member_access(ps, importNode, memberName, location);
    if (memberAccess == ZR_NULL) {
        if (args != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, args);
        }
        return ZR_NULL;
    }

    return zr_task_create_function_call(ps, memberAccess, args, location);
}

static SZrAstNode *zr_task_wrap_async_body(SZrParserState *ps, SZrAstNode *body) {
    SZrAstNodeArray *lambdaParams;
    SZrAstNode *lambdaNode;
    SZrAstNodeArray *runnerArgs;
    SZrAstNode *runnerCall;
    SZrAstNode *returnNode;
    SZrAstNodeArray *blockBody;
    SZrAstNode *blockNode;

    if (ps == ZR_NULL || body == ZR_NULL) {
        return ZR_NULL;
    }

    lambdaParams = ZrParser_AstNodeArray_New(ps->state, 0);
    if (lambdaParams == ZR_NULL) {
        return ZR_NULL;
    }

    lambdaNode = create_ast_node(ps, ZR_AST_LAMBDA_EXPRESSION, body->location);
    if (lambdaNode == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, lambdaParams);
        return ZR_NULL;
    }
    lambdaNode->data.lambdaExpression.params = lambdaParams;
    lambdaNode->data.lambdaExpression.args = ZR_NULL;
    lambdaNode->data.lambdaExpression.block = body;
    lambdaNode->data.lambdaExpression.isAsync = ZR_TRUE;

    runnerArgs = ZrParser_AstNodeArray_New(ps->state, 1);
    if (runnerArgs == ZR_NULL) {
        lambdaNode->data.lambdaExpression.block = ZR_NULL;
        ZrParser_Ast_Free(ps->state, lambdaNode);
        return ZR_NULL;
    }
    ZrParser_AstNodeArray_Add(ps->state, runnerArgs, lambdaNode);

    runnerCall = zr_task_create_module_member_call(ps, "__createTaskRunner", runnerArgs, body->location);
    if (runnerCall == ZR_NULL) {
        return ZR_NULL;
    }

    returnNode = create_ast_node(ps, ZR_AST_RETURN_STATEMENT, body->location);
    if (returnNode == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, runnerCall);
        return ZR_NULL;
    }
    returnNode->data.returnStatement.expr = runnerCall;

    blockBody = ZrParser_AstNodeArray_New(ps->state, 1);
    if (blockBody == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, returnNode);
        return ZR_NULL;
    }
    ZrParser_AstNodeArray_Add(ps->state, blockBody, returnNode);

    blockNode = create_ast_node(ps, ZR_AST_BLOCK, body->location);
    if (blockNode == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, blockBody);
        return ZR_NULL;
    }
    blockNode->data.block.body = blockBody;
    blockNode->data.block.isStatement = ZR_TRUE;
    return blockNode;
}

SZrAstNode *parse_reserved_await_expression(SZrParserState *ps) {
    SZrFileRange startLoc;
    SZrAstNode *operand;
    SZrAstNodeArray *args;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_PERCENT) {
        return ZR_NULL;
    }

    startLoc = get_current_location(ps);
    ZrParser_Lexer_Next(ps->lexer);
    if (ps->lexer->t.token != ZR_TK_IDENTIFIER || !current_identifier_equals(ps, "await")) {
        report_error(ps, "Expected 'await' after '%'");
        return ZR_NULL;
    }

    ZrParser_Lexer_Next(ps->lexer);
    operand = parse_unary_expression(ps);
    if (operand == ZR_NULL) {
        return ZR_NULL;
    }

    args = ZrParser_AstNodeArray_New(ps->state, 1);
    if (args == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, operand);
        return ZR_NULL;
    }
    ZrParser_AstNodeArray_Add(ps->state, args, operand);
    return zr_task_create_module_member_call(ps, "__awaitTask", args, startLoc);
}

SZrAstNode *parse_reserved_async_function_declaration(SZrParserState *ps) {
    SZrFileRange startLoc;
    SZrAstNode *functionNode;
    SZrAstNode *wrappedBody;
    SZrFunctionDeclaration *declaration;

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    startLoc = get_current_token_location(ps);
    if (ps->lexer->t.token == ZR_TK_PUB || ps->lexer->t.token == ZR_TK_PRI || ps->lexer->t.token == ZR_TK_PRO) {
        parse_access_modifier(ps);
    }

    if (ps->lexer->t.token != ZR_TK_PERCENT) {
        report_error(ps, "Expected '%' before async function declaration");
        return ZR_NULL;
    }

    ZrParser_Lexer_Next(ps->lexer);
    if (ps->lexer->t.token != ZR_TK_IDENTIFIER || !current_identifier_equals(ps, "async")) {
        report_error(ps, "Expected 'async' after '%'");
        return ZR_NULL;
    }

    ZrParser_Lexer_Next(ps->lexer);
    functionNode = parse_function_declaration(ps);
    if (functionNode == ZR_NULL || functionNode->type != ZR_AST_FUNCTION_DECLARATION) {
        return functionNode;
    }

    declaration = &functionNode->data.functionDeclaration;
    wrappedBody = zr_task_wrap_async_body(ps, declaration->body);
    if (wrappedBody == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, functionNode);
        return ZR_NULL;
    }

    declaration->body = wrappedBody;
    if (declaration->returnType != ZR_NULL) {
        SZrType *declaredReturnType = declaration->returnType;
        declaration->returnType = ZR_NULL;
        declaration->returnType = zr_task_wrap_return_type_async(ps, declaredReturnType, functionNode->location);
        if (declaration->returnType == ZR_NULL) {
            ZrParser_Ast_Free(ps->state, functionNode);
            return ZR_NULL;
        }
    }
    declaration->isAsync = ZR_TRUE;
    functionNode->location = ZrParser_FileRange_Merge(startLoc, functionNode->location);
    return functionNode;
}
