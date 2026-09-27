#include "parser_internal.h"

/* 枚举成员的名称、可选值与装饰器在成功后由成员 AST 接管。 */
SZrAstNode *parse_enum_member(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNodeArray *decorators = parse_leading_decorators(ps);

    // 解析成员名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        if (decorators != ZR_NULL) {
            /* BUG: 装饰器已生成而名称解析失败时只释放容器，装饰器节点泄漏。 */
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;

    // 可选值（name = value 或 name;）
    SZrAstNode *value = ZR_NULL;
    if (consume_token(ps, ZR_TK_EQUALS)) {
        value = parse_expression(ps);
        if (value == ZR_NULL) {
            /* BUG: 成员值解析失败时 nameNode 未交接，装饰器也仅释放容器。 */
            if (decorators != ZR_NULL) {
                ZrParser_AstNodeArray_Free(ps->state, decorators);
            }
            return ZR_NULL;
        }
    }

    // 可选分隔符（逗号或分号）
    if (ps->lexer->t.token == ZR_TK_COMMA || ps->lexer->t.token == ZR_TK_SEMICOLON) {
        ZrParser_Lexer_Next(ps->lexer);
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange memberLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_ENUM_MEMBER, memberLoc);
    if (node == ZR_NULL) {
        /* BUG: 节点分配失败时名称、值和装饰器子节点均未被递归释放。 */
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        return ZR_NULL;
    }

    node->data.enumMember.name = name;
    node->data.enumMember.value = value;
    node->data.enumMember.decorators = decorators;
    return node;
}

// 解析枚举声明；枚举 AST 最终拥有名称、基类型、成员列表及装饰器。
// 语法：enum Name[: baseType] { members }

SZrAstNode *parse_enum_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrFileRange bodyOpenLoc = startLoc;
    SZrAstNodeArray *decorators = parse_leading_decorators(ps);
    TZrBool bodyOpened = ZR_FALSE;

    // 解析可见性修饰符（可选，默认 private）
    EZrAccessModifier accessModifier = parse_access_modifier(ps);

    // 期望 enum 关键字
    expect_token(ps, ZR_TK_ENUM);
    ZrParser_Lexer_Next(ps->lexer);

    // 解析枚举名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;

    // 解析基础类型（可选，继承 int, string, float, bool）
    SZrType *baseType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        baseType = parse_type(ps);
        if (baseType == ZR_NULL) {
            /* BUG: 非法基类型触发早退时，已解析名称与装饰器未释放。 */
            return ZR_NULL;
        }
    }

    // 期望左大括号
    if (ps->lexer->t.token != ZR_TK_LBRACE) {
        report_missing_declaration_body_open(ps, "enum declaration", get_current_token_location(ps));
    } else {
        bodyOpenLoc = get_current_token_location(ps);
        bodyOpened = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    // 解析成员列表
    SZrAstNodeArray *members = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_SMALL);
    if (members == ZR_NULL) {
        /* BUG: 成员数组分配失败时名称和可选基类型尚无 AST 所有者。 */
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        return ZR_NULL;
    }

    // 解析成员直到遇到右大括号
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *member = parse_enum_member(ps);
        if (member != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, members, member);
        } else {
            // 解析失败，尝试恢复
            if (ps->hasError) {
                break;
            }
            // 跳过当前 token 继续解析
            if (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
                ZrParser_Lexer_Next(ps->lexer);
            }
        }
    }

    // 期望右大括号
    if (ps->lexer->t.token != ZR_TK_RBRACE) {
        if (ps->lexer->t.token == ZR_TK_EOS) {
            if (bodyOpened) {
                report_missing_declaration_body_close(ps, "enum declaration", bodyOpenLoc);
            }
        } else {
            expect_token(ps, ZR_TK_RBRACE);
        }
    }
    consume_token(ps, ZR_TK_RBRACE);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange enumLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_ENUM_DECLARATION, enumLoc);
    if (node == ZR_NULL) {
        /* BUG: 枚举节点分配失败只释放数组容器，名称、基类型与成员子树泄漏。 */
        ZrParser_AstNodeArray_Free(ps->state, members);
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        return ZR_NULL;
    }

    node->data.enumDeclaration.name = name;
    node->data.enumDeclaration.baseType = baseType;
    node->data.enumDeclaration.members = members;
    node->data.enumDeclaration.decorators = decorators;
    node->data.enumDeclaration.accessModifier = accessModifier;
    return node;
}

/* extern 两类签名共用 ')' 检查；此处仅处理数组指针，不拥有调用方的名称等资源。 */
static TZrBool consume_extern_parameter_list_close_or_report(SZrParserState *ps,
                                                             SZrAstNodeArray **params,
                                                             SZrAstNodeArray **decorators) {
    if (ps->lexer->t.token == ZR_TK_RPAREN) {
        consume_token(ps, ZR_TK_RPAREN);
        return ZR_TRUE;
    }

    report_missing_parameter_list_close(ps, get_current_token_location(ps));
    if (params != ZR_NULL && *params != ZR_NULL) {
        /* BUG: 缺失 ')' 时仅释放参数数组容器，已解析参数 AST 泄漏。 */
        ZrParser_AstNodeArray_Free(ps->state, *params);
        *params = ZR_NULL;
    }
    if (decorators != ZR_NULL && *decorators != ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, *decorators);
        *decorators = ZR_NULL;
    }
    return ZR_FALSE;
}

/* native extern 的 fn 声明接管调用方传入的装饰器，结果由 extern 块持有。 */
SZrAstNode *parse_extern_function_declaration(SZrParserState *ps, SZrAstNodeArray *decorators) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNode *nameNode;
    SZrIdentifier *name;
    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL;
    SZrType *returnType = ZR_NULL;
    SZrAstNode *node;

    nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        return ZR_NULL;
    }
    name = &nameNode->data.identifier;

    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);

    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        SZrAstNode *argsNode = parse_parameter(ps);
        if (argsNode != ZR_NULL) {
            args = &argsNode->data.parameter;
        }
        params = ZrParser_AstNodeArray_New(ps->state, 0);
    } else {
        params = parse_parameter_list(ps);
        if (consume_token(ps, ZR_TK_COMMA) && ps->lexer->t.token == ZR_TK_PARAMS) {
            SZrAstNode *argsNode = parse_parameter(ps);
            if (argsNode != ZR_NULL) {
                args = &argsNode->data.parameter;
            }
        }
    }

    if (!consume_extern_parameter_list_close_or_report(ps, &params, &decorators)) {
        /* BUG: 签名缺失 ')' 时 helper 不接管名称与可变参数节点，返回后均泄漏。 */
        return ZR_NULL;
    }

    if (consume_token(ps, ZR_TK_COLON)) {
        returnType = parse_type(ps);
    }

    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    node = create_ast_node(ps, ZR_AST_EXTERN_FUNCTION_DECLARATION,
                           ZrParser_FileRange_Merge(startLoc, get_current_location(ps)));
    if (node == ZR_NULL) {
        /* BUG: 声明节点分配失败时名称、返回类型和参数子节点未释放。 */
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        return ZR_NULL;
    }

    node->data.externFunctionDeclaration.name = name;
    node->data.externFunctionDeclaration.params = params;
    node->data.externFunctionDeclaration.args = args;
    node->data.externFunctionDeclaration.returnType = returnType;
    node->data.externFunctionDeclaration.decorators = decorators;
    return node;
}

/* delegate 与 extern fn 使用同一参数关闭契约，成功后节点拥有装饰器及签名。 */
SZrAstNode *parse_extern_delegate_declaration(SZrParserState *ps, SZrAstNodeArray *decorators) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNode *nameNode;
    SZrIdentifier *name;
    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL;
    SZrType *returnType = ZR_NULL;
    SZrAstNode *node;

    if (!current_identifier_equals(ps, "delegate")) {
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        report_error(ps, "Expected 'delegate' in extern block");
        return ZR_NULL;
    }
    ZrParser_Lexer_Next(ps->lexer);

    nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        return ZR_NULL;
    }
    name = &nameNode->data.identifier;

    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);

    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        SZrAstNode *argsNode = parse_parameter(ps);
        if (argsNode != ZR_NULL) {
            args = &argsNode->data.parameter;
        }
        params = ZrParser_AstNodeArray_New(ps->state, 0);
    } else {
        params = parse_parameter_list(ps);
        if (consume_token(ps, ZR_TK_COMMA) && ps->lexer->t.token == ZR_TK_PARAMS) {
            SZrAstNode *argsNode = parse_parameter(ps);
            if (argsNode != ZR_NULL) {
                args = &argsNode->data.parameter;
            }
        }
    }

    if (!consume_extern_parameter_list_close_or_report(ps, &params, &decorators)) {
        /* BUG: 签名缺失 ')' 时名称与可变参数节点未被 helper 或调用方释放。 */
        return ZR_NULL;
    }

    if (consume_token(ps, ZR_TK_COLON)) {
        returnType = parse_type(ps);
    }

    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    node = create_ast_node(ps, ZR_AST_EXTERN_DELEGATE_DECLARATION,
                           ZrParser_FileRange_Merge(startLoc, get_current_location(ps)));
    if (node == ZR_NULL) {
        /* BUG: 声明节点分配失败时名称、返回类型和参数子节点未释放。 */
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        if (decorators != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, decorators);
        }
        return ZR_NULL;
    }

    node->data.externDelegateDeclaration.name = name;
    node->data.externDelegateDeclaration.params = params;
    node->data.externDelegateDeclaration.args = args;
    node->data.externDelegateDeclaration.returnType = returnType;
    node->data.externDelegateDeclaration.decorators = decorators;
    return node;
}

/* extern 成员分派将前置装饰器移交给函数/delegate，或替换 struct/enum 自带装饰器。 */
static SZrAstNode *parse_extern_member_declaration_impl(SZrParserState *ps) {
    SZrAstNodeArray *decorators = parse_leading_decorators(ps);
    SZrAstNode *node = ZR_NULL;
    EZrAccessModifier accessModifier = ZR_ACCESS_PRIVATE;

    if (ps->lexer->t.token == ZR_TK_PUB ||
        ps->lexer->t.token == ZR_TK_PRI ||
        ps->lexer->t.token == ZR_TK_PRO) {
        accessModifier = parse_access_modifier(ps);
    }

    if (ps->lexer->t.token == ZR_TK_FN) {
        ZrParser_Lexer_Next(ps->lexer);
        node = parse_extern_function_declaration(ps, decorators);
        decorators = ZR_NULL;
    } else if (ps->lexer->t.token == ZR_TK_STRUCT) {
        node = parse_struct_declaration(ps);
        if (node != ZR_NULL && node->type == ZR_AST_STRUCT_DECLARATION && decorators != ZR_NULL) {
            free_ast_node_array_with_elements(
                    ps->state,
                    node->data.structDeclaration.decorators);
            node->data.structDeclaration.decorators = decorators;
            decorators = ZR_NULL;
        }
    } else if (ps->lexer->t.token == ZR_TK_ENUM) {
        node = parse_enum_declaration(ps);
        if (node != ZR_NULL && node->type == ZR_AST_ENUM_DECLARATION && decorators != ZR_NULL) {
            free_ast_node_array_with_elements(
                    ps->state,
                    node->data.enumDeclaration.decorators);
            node->data.enumDeclaration.decorators = decorators;
            decorators = ZR_NULL;
        }
    } else if (current_identifier_equals(ps, "delegate")) {
        node = parse_extern_delegate_declaration(ps, decorators);
        decorators = ZR_NULL;
    } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
        report_error(ps, "Functions in a native extern block must start with 'fn'");
    } else {
        report_error(ps, "Unexpected declaration inside extern block");
    }

    if (decorators != ZR_NULL) {
        /* BUG: 非法成员或 struct/enum 解析失败时只释放容器，前置装饰器节点泄漏。 */
        ZrParser_AstNodeArray_Free(ps->state, decorators);
    }
    if (node != ZR_NULL) {
        switch (node->type) {
            case ZR_AST_EXTERN_FUNCTION_DECLARATION:
                node->data.externFunctionDeclaration.accessModifier = accessModifier;
                break;
            case ZR_AST_EXTERN_DELEGATE_DECLARATION:
                node->data.externDelegateDeclaration.accessModifier = accessModifier;
                break;
            case ZR_AST_STRUCT_DECLARATION:
                node->data.structDeclaration.accessModifier = accessModifier;
                break;
            case ZR_AST_ENUM_DECLARATION:
                node->data.enumDeclaration.accessModifier = accessModifier;
                break;
            default:
                break;
        }
    }
    return node;
}

/* native extern 可包单成员或花括号列表；库名及声明数组移交给 extern 块 AST。 */
SZrAstNode *parse_extern_block(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrFileRange bodyOpenLoc = startLoc;
    SZrAstNode *libraryName = ZR_NULL;
    SZrAstNodeArray *declarations = ZR_NULL;
    SZrAstNode *node;

    if (ps->lexer->t.token == ZR_TK_IDENTIFIER &&
               current_identifier_equals(ps, "native")) {
        ZrParser_Lexer_Next(ps->lexer);
        if (ps->lexer->t.token != ZR_TK_IDENTIFIER ||
            !current_identifier_equals(ps, "extern")) {
            report_error(ps, "Expected 'extern' after 'native'");
            return ZR_NULL;
        }
    } else {
        report_error(ps, "Expected 'native extern' declaration");
        return ZR_NULL;
    }
    ZrParser_Lexer_Next(ps->lexer);

    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);

    if (ps->lexer->t.token != ZR_TK_STRING) {
        report_error(ps, "Expected string literal library spec for extern block");
        return ZR_NULL;
    }
    libraryName = parse_literal(ps);

    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        report_missing_extern_spec_close(ps, get_current_token_location(ps));
        ZrParser_Ast_Free(ps->state, libraryName);
        return ZR_NULL;
    }

    consume_token(ps, ZR_TK_RPAREN);

    declarations = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_TINY);
    if (declarations == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, libraryName);
        return ZR_NULL;
    }

    if (ps->lexer->t.token == ZR_TK_LBRACE) {
        bodyOpenLoc = get_current_token_location(ps);
        consume_token(ps, ZR_TK_LBRACE);
        while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
            SZrAstNode *declaration = parse_extern_member_declaration_impl(ps);
            if (declaration == ZR_NULL) {
                break;
            }
            ZrParser_AstNodeArray_Add(ps->state, declarations, declaration);
        }
        if (ps->lexer->t.token != ZR_TK_RBRACE) {
            if (ps->lexer->t.token == ZR_TK_EOS) {
                report_missing_declaration_body_close(ps, "extern block", bodyOpenLoc);
            } else {
                expect_token(ps, ZR_TK_RBRACE);
            }
        }
        consume_token(ps, ZR_TK_RBRACE);
    } else if (ps->lexer->t.token == ZR_TK_EOS || ps->lexer->t.token == ZR_TK_RBRACE) {
        report_missing_declaration_body_open(ps, "extern block", get_current_token_location(ps));
        ZrParser_AstNodeArray_Free(ps->state, declarations);
        ZrParser_Ast_Free(ps->state, libraryName);
        return ZR_NULL;
    } else {
        SZrAstNode *declaration = parse_extern_member_declaration_impl(ps);
        if (declaration == ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, declarations);
            ZrParser_Ast_Free(ps->state, libraryName);
            return ZR_NULL;
        }
        ZrParser_AstNodeArray_Add(ps->state, declarations, declaration);
    }

    node = create_ast_node(ps, ZR_AST_EXTERN_BLOCK, ZrParser_FileRange_Merge(startLoc, get_current_location(ps)));
    if (node == ZR_NULL) {
        for (TZrSize index = 0; index < declarations->count; index++) {
            ZrParser_Ast_Free(ps->state, declarations->nodes[index]);
        }
        ZrParser_AstNodeArray_Free(ps->state, declarations);
        ZrParser_Ast_Free(ps->state, libraryName);
        return ZR_NULL;
    }

    node->data.externBlock.libraryName = libraryName;
    node->data.externBlock.declarations = declarations;
    return node;
}

/* comptime 按后继 token 分派函数、条件分支、块或表达式，并记录剪枝语义。 */
SZrAstNode *parse_compile_time_declaration(SZrParserState *ps) {
    SZrFileRange startLoc;
    TZrBool isConditionalPruning = ZR_FALSE;

    if (ps->lexer->t.token == ZR_TK_IDENTIFIER && current_identifier_equals(ps, "comptime")) {
        startLoc = get_current_location(ps);
        ZrParser_Lexer_Next(ps->lexer);
    } else {
        report_error(ps, "Expected 'comptime'");
        return ZR_NULL;
    }

    if (ps->lexer->t.token == ZR_TK_VAR || ps->lexer->t.token == ZR_TK_LET) {
        report_removed_legacy_syntax(
                ps,
                "comptime variable declaration",
                "Use ordinary immutable data or declare values inside a module-scope `comptime { ... }` block.");
        return ZR_NULL;
    }
    if (ps->lexer->t.token == ZR_TK_CLASS || ps->lexer->t.token == ZR_TK_STRUCT) {
        report_removed_legacy_syntax(
                ps,
                "comptime decorator type",
                "Use a role-marked ordinary `comptime fn(...): DeclarationPatch` transform.");
        return ZR_NULL;
    }

    // 根据后续内容判断声明类型
    EZrCompileTimeDeclarationType declType;
    SZrAstNode *declaration = ZR_NULL;

    if (ps->lexer->t.token == ZR_TK_FN) {
        declType = ZR_COMPILE_TIME_FUNCTION;
        declaration = parse_function_declaration(ps);
    } else if (ps->lexer->t.token == ZR_TK_IF) {
        declType = ZR_COMPILE_TIME_STATEMENT;
        declaration = parse_if_expression(ps);
        isConditionalPruning = ZR_TRUE;
        if (declaration != ZR_NULL) {
            declaration->data.ifExpression.isStatement = ZR_TRUE;
        }
    } else if (ps->lexer->t.token == ZR_TK_LBRACE) {
        declType = ZR_COMPILE_TIME_STATEMENT;
        declaration = parse_block(ps);
    } else {
        // 尝试解析为表达式
        declType = ZR_COMPILE_TIME_EXPRESSION;
        declaration = parse_expression(ps);
    }

    if (declaration == ZR_NULL) {
        return ZR_NULL;
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange compileTimeLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_COMPILE_TIME_DECLARATION, compileTimeLoc);
    if (node == ZR_NULL) {
        /* BUG: 外层节点分配失败时 declaration 已构造却未释放。 */
        return ZR_NULL;
    }

    node->data.compileTimeDeclaration.declarationType = declType;
    node->data.compileTimeDeclaration.declaration = declaration;
    node->data.compileTimeDeclaration.selectedBranch = ZR_NULL;
    node->data.compileTimeDeclaration.isConditionalPruning = isConditionalPruning;
    node->data.compileTimeDeclaration.buildFactsEvaluated = ZR_FALSE;
    return node;
}

// 解析生成器表达式（{{}}）；内部语句数组先由块接管，再由生成器 AST 接管块。

SZrAstNode *parse_generator_expression(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);

    // 期望第一个 {
    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    // 期望第二个 {
    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    // 解析块内容（语句列表），不期望 { 和 }
    SZrFileRange blockStartLoc = get_current_location(ps);
    SZrAstNodeArray *statements = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_SMALL);
    if (statements == ZR_NULL) {
        report_error(ps, "Failed to allocate statement array");
        return ZR_NULL;
    }

    // 解析语句直到遇到第一个 }
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *stmt = parse_statement(ps);
        if (stmt != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, statements, stmt);
        } else {
            break; // 遇到错误
        }
    }

    // 期望第一个 }
    expect_token(ps, ZR_TK_RBRACE);
    consume_token(ps, ZR_TK_RBRACE);

    // 期望第二个 }
    expect_token(ps, ZR_TK_RBRACE);
    consume_token(ps, ZR_TK_RBRACE);

    // 创建块节点
    SZrFileRange blockEndLoc = get_current_location(ps);
    SZrFileRange blockLoc = ZrParser_FileRange_Merge(blockStartLoc, blockEndLoc);
    SZrAstNode *block = create_ast_node(ps, ZR_AST_BLOCK, blockLoc);
    if (block == ZR_NULL) {
        /* TODO: 当前无生成器解析调用入口；若恢复该语法，块分配失败时
         * 只释放容器会遗留已解析的语句节点。 */
        ZrParser_AstNodeArray_Free(ps->state, statements);
        return ZR_NULL;
    }
    block->data.block.body = statements;
    block->data.block.isStatement = ZR_FALSE; // 生成器表达式中的块是表达式

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange generatorLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_GENERATOR_EXPRESSION, generatorLoc);
    if (node == ZR_NULL) {
        /* TODO: 若恢复调用，此时块已接管 statements；失败需释放整个块子树。 */
        ZrParser_AstNodeArray_Free(ps->state, statements);
        return ZR_NULL;
    }

    node->data.generatorExpression.block = block;
    return node;
}

// 解析类字段
