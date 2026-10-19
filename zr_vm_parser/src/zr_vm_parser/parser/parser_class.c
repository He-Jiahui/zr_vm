#include "parser_internal.h"
#include "parser_property_migration.h"
#include "parser_class_internal.h"

/* 在不提交游标的前提下选定 class 成员语法，供声明循环分派正式解析器。 */
static EZrAstNodeType classify_class_member_from_current(SZrParserState *ps) {
    SZrParserCursor cursor;
    EZrAstNodeType kind = ZR_AST_CLASS_METHOD;

    if (ps == ZR_NULL) {
        return ZR_AST_CLASS_METHOD;
    }

    save_parser_cursor(ps, &cursor);

    while (ps->lexer->t.token == ZR_TK_SHARP) {
        SZrAstNode *decorator = parse_decorator_expression(ps);
        if (decorator == ZR_NULL) {
            break;
        }
        ZrParser_Ast_Free(ps->state, decorator);
    }

    parse_access_modifier(ps);
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        ZrParser_Lexer_Next(ps->lexer);
    }

    parse_declaration_modifier_flags(ps, class_member_allowed_modifier_flags());

    if (ps->lexer->t.token == ZR_TK_IDENTIFIER &&
        current_identifier_equals(ps, "property")) {
        kind = ZR_AST_PROPERTY_DECLARATION;
    } else if (ps->lexer->t.token == ZR_TK_VAR ||
               ps->lexer->t.token == ZR_TK_LET) {
        kind = ZR_AST_CLASS_FIELD;
    } else if (ps->lexer->t.token == ZR_TK_CONST) {
        ZrParser_Lexer_Next(ps->lexer);
        if (ps->lexer->t.token == ZR_TK_AT) {
            kind = ZR_AST_CLASS_META_FUNCTION;
        } else {
            kind = ps->lexer->t.token == ZR_TK_FN
                           ? ZR_AST_CLASS_METHOD
                           : ZR_AST_CLASS_FIELD;
        }
    } else if (ps->lexer->t.token == ZR_TK_GET || ps->lexer->t.token == ZR_TK_SET) {
        kind = ZR_AST_CLASS_PROPERTY;
    } else if (ps->lexer->t.token == ZR_TK_AT) {
        kind = ZR_AST_CLASS_META_FUNCTION;
    } else {
        kind = ZR_AST_CLASS_METHOD;
    }

    restore_parser_cursor(ps, &cursor);
    return kind;
}

/* 顶层声明分派器调用此入口；返回的声明 AST 统一拥有名称、泛型、继承和成员。
 * 旧式 get/set 只在迁移解析模式暂存，发布诊断后回收，不进入正常成员树。 */
SZrAstNode *parse_class_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);
    SZrFileRange bodyOpenLoc = startLoc;
    SZrFileRange endLoc;
    SZrAstNodeArray *decorators;
    TZrUInt32 modifierFlags;
    TZrBool bodyOpened = ZR_FALSE;

    // 允许 top-level decorators 出现在访问修饰符之前。
    decorators = parse_leading_decorators(ps);
    if (decorators == ZR_NULL) {
        return ZR_NULL;
    }

    // 解析可见性修饰符（可选，默认 private）
    EZrAccessModifier accessModifier = parse_access_modifier(ps);

    modifierFlags = parse_declaration_modifier_flags(ps,
                                                     ZR_DECLARATION_MODIFIER_ABSTRACT |
                                                             ZR_DECLARATION_MODIFIER_FINAL);

    // 同时继续兼容访问修饰符之后的装饰器写法。
    while (ps->lexer->t.token == ZR_TK_SHARP) {
        SZrAstNode *decorator = parse_decorator_expression(ps);
        if (decorator != ZR_NULL) {
            if (!parser_ast_array_add(ps, decorators, decorator)) {
                ZrParser_Ast_Free(ps->state, decorator);
                free_ast_node_array_with_elements(ps->state, decorators);
                return ZR_NULL;
            }
        } else {
            break;
        }
    }

    // 期望 class 关键字
    expect_token(ps, ZR_TK_CLASS);
    ZrParser_Lexer_Next(ps->lexer);

    // 解析类名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, decorators);
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    SZrFileRange nameLoc = nameNode->location;

    // 解析泛型声明（可选）
    SZrGenericDeclaration *generic = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
        generic = parse_generic_declaration(ps, ZR_FALSE);
        if (generic == ZR_NULL) {
            ZrParser_Ast_Free(ps->state, nameNode);
            free_ast_node_array_with_elements(ps->state, decorators);
            return ZR_NULL;
        }
    }

    // 解析继承列表（可选）
    SZrAstNodeArray *inherits = parser_ast_array_new(ps, 0);
    if (inherits == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, nameNode);
        free_generic_declaration(ps->state, generic);
        free_ast_node_array_with_elements(ps->state, decorators);
        return ZR_NULL;
    }
    if (consume_token(ps, ZR_TK_COLON)) {
        // 解析类型列表
        if (ps->lexer->t.token != ZR_TK_LBRACE) {
            SZrType *firstType = parse_type(ps);
            if (firstType != ZR_NULL) {
                SZrAstNode *typeNode = create_ast_node(ps, ZR_AST_TYPE, get_current_location(ps));
                if (typeNode != ZR_NULL) {
                    typeNode->data.type = *firstType;
                    ZrCore_Memory_RawFreeWithType(ps->state->global, firstType, sizeof(SZrType),
                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                    if (!parser_ast_array_add(ps, inherits, typeNode)) {
                        ZrParser_Ast_Free(ps->state, typeNode);
                    }
                } else {
                    free_owned_type(ps->state, firstType);
                }
            }

            while (consume_token(ps, ZR_TK_COMMA)) {
                if (ps->lexer->t.token == ZR_TK_LBRACE) {
                    break;
                }
                SZrType *type = parse_type(ps);
                if (type != ZR_NULL) {
                    SZrAstNode *typeNode = create_ast_node(ps, ZR_AST_TYPE, get_current_location(ps));
                    if (typeNode != ZR_NULL) {
                        typeNode->data.type = *type;
                        ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType),
                                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
                        if (!parser_ast_array_add(ps, inherits, typeNode)) {
                            ZrParser_Ast_Free(ps->state, typeNode);
                            break;
                        }
                    } else {
                        free_owned_type(ps->state, type);
                        break;
                    }
                } else {
                    break;
                }
            }
        }
    }

    if (ps->hasFatalError || !parse_optional_where_clauses(ps, generic)) {
        ZrParser_Ast_Free(ps->state, nameNode);
        free_generic_declaration(ps->state, generic);
        free_ast_node_array_with_elements(ps->state, decorators);
        free_ast_node_array_with_elements(ps->state, inherits);
        return ZR_NULL;
    }

    // 期望左大括号
    if (ps->lexer->t.token != ZR_TK_LBRACE) {
        report_missing_declaration_body_open(ps, "class declaration", get_current_token_location(ps));
    } else {
        bodyOpenLoc = get_current_token_location(ps);
        bodyOpened = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    // 解析成员列表
    SZrAstNodeArray *members = parser_ast_array_new(ps, ZR_PARSER_INITIAL_CAPACITY_SMALL);
    SZrLegacyPropertyMigrationCollection legacyProperties;
    if (members == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, nameNode);
        free_generic_declaration(ps->state, generic);
        free_ast_node_array_with_elements(ps->state, decorators);
        free_ast_node_array_with_elements(ps->state, inherits);
        return ZR_NULL;
    }
    parser_property_migration_collection_init(ps->state, &legacyProperties);

    // 解析成员直到遇到右大括号
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *member = ZR_NULL;
        EZrToken token = ps->lexer->t.token;
        TZrBool rejectedLegacyProperty = ZR_FALSE;

        if (token == ZR_TK_PERCENT) {
            report_removed_percent_syntax(ps);
            break;
        }

        if (token == ZR_TK_SHARP || token == ZR_TK_PUB || token == ZR_TK_PRI ||
            token == ZR_TK_PRO || token == ZR_TK_STATIC || token == ZR_TK_CONST || token == ZR_TK_USING ||
            token == ZR_TK_VAR || token == ZR_TK_LET || token == ZR_TK_ABSTRACT || token == ZR_TK_VIRTUAL ||
            token == ZR_TK_OVERRIDE || token == ZR_TK_FINAL || token == ZR_TK_SHADOW || token == ZR_TK_AT ||
            token == ZR_TK_GET || token == ZR_TK_SET || token == ZR_TK_IDENTIFIER || token == ZR_TK_TEST ||
            token == ZR_TK_FN) {
            switch (classify_class_member_from_current(ps)) {
                case ZR_AST_PROPERTY_DECLARATION:
                    member = parse_property_declaration(
                            ps, ZR_PROPERTY_CONTAINER_CLASS);
                    break;
                case ZR_AST_CLASS_FIELD:
                    member = parse_class_field(ps);
                    break;
                case ZR_AST_CLASS_PROPERTY:
                    if (ps->enableLegacyMigrationParsing) {
                        member = parse_class_property(ps);
                        if (member != ZR_NULL) {
                            if (parser_property_migration_collection_append(
                                        ps->state, &legacyProperties, member)) {
                                rejectedLegacyProperty = ZR_TRUE;
                            } else {
                                ZrParser_Ast_Free(ps->state, member);
                                member = ZR_NULL;
                            }
                        }
                    } else {
                        report_removed_legacy_syntax(
                                ps,
                                "split get/set property declaration",
                                "Use one `property name: Type { get ...; set ...; }` declaration.");
                    }
                    break;
                case ZR_AST_CLASS_META_FUNCTION:
                    member = parse_class_meta_function(ps);
                    break;
                case ZR_AST_CLASS_METHOD:
                default:
                    member = parse_class_method(ps);
                    break;
            }
        } else {
            // 未知的成员类型，报告错误并跳过
            report_error(ps, "Unexpected token in class declaration");
            break;
        }

        if (rejectedLegacyProperty) {
            continue;
        }
        if (member != ZR_NULL) {
            if (!parser_ast_array_add(ps, members, member)) {
                ZrParser_Ast_Free(ps->state, member);
                break;
            }
            parser_property_migration_collection_mark_current_member(
                    &legacyProperties);
        } else {
            // 解析失败，尝试恢复
            break;
        }
    }
    if (!parser_property_migration_collection_publish_and_free(
            ps,
            &legacyProperties)) {
        ZrParser_Ast_Free(ps->state, nameNode);
        free_generic_declaration(ps->state, generic);
        free_ast_node_array_with_elements(ps->state, decorators);
        free_ast_node_array_with_elements(ps->state, inherits);
        free_ast_node_array_with_elements(ps->state, members);
        return ZR_NULL;
    }

    // 期望右大括号
    if (ps->lexer->t.token != ZR_TK_RBRACE) {
        if (ps->lexer->t.token == ZR_TK_EOS) {
            if (bodyOpened) {
                report_missing_declaration_body_close(ps, "class declaration", bodyOpenLoc);
            }
        } else {
            expect_token(ps, ZR_TK_RBRACE);
        }
    }
    endLoc = get_current_token_location(ps);
    consume_token(ps, ZR_TK_RBRACE);
    SZrFileRange classLoc;

    SZrAstNode *node;
    if (!ZrParser_MergeLocations(ps, startLoc, endLoc, &classLoc) ||
        (node = create_ast_node(ps, ZR_AST_CLASS_DECLARATION, classLoc)) == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, nameNode);
        free_generic_declaration(ps->state, generic);
        free_ast_node_array_with_elements(ps->state, decorators);
        free_ast_node_array_with_elements(ps->state, inherits);
        free_ast_node_array_with_elements(ps->state, members);
        return ZR_NULL;
    }

    node->data.classDeclaration.name = name;
    node->data.classDeclaration.nameLocation = nameLoc;
    node->data.classDeclaration.generic = generic;
    node->data.classDeclaration.inherits = inherits;
    node->data.classDeclaration.members = members;
    node->data.classDeclaration.decorators = decorators;
    node->data.classDeclaration.accessModifier = accessModifier;
    node->data.classDeclaration.isOwned = ZR_FALSE;
    node->data.classDeclaration.modifierFlags = modifierFlags;
    return node;
}

/* class 成员分派器选择字段后调用；产出的名称、类型和初值由字段 AST 持有。
 * 所有权约束写在字段类型而非旧式 field-scoped using 上。 */
SZrAstNode *parse_class_field(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);
    SZrFileRange endLoc;

    // 解析装饰器（可选）
    SZrAstNodeArray *decorators = parse_leading_decorators(ps);
    if (decorators == ZR_NULL) {
        return ZR_NULL;
    }

    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);

    // 解析 static 关键字（可选）
    TZrBool isStatic = ZR_FALSE;
    TZrUInt32 invalidModifierFlags = ZR_DECLARATION_MODIFIER_NONE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    invalidModifierFlags = parse_declaration_modifier_flags(ps, class_member_allowed_modifier_flags());
    if (invalidModifierFlags != ZR_DECLARATION_MODIFIER_NONE) {
        report_error(ps, "Field declarations do not support abstract/virtual/override/final/shadow modifiers");
    }

    if (ps->lexer->t.token == ZR_TK_USING) {
        report_error(ps, "Field-scoped 'using' is invalid; write `var field: Unique<T>` or `var field: Shared<T>` so ownership lives in the field type");
        skip_to_semicolon_or_eos(ps);
        free_ast_node_array_with_elements(ps->state, decorators);
        return ZR_NULL;
    }

    if (ps->lexer->t.token == ZR_TK_PERCENT) {
        report_removed_percent_syntax(ps);
        skip_to_semicolon_or_eos(ps);
        free_ast_node_array_with_elements(ps->state, decorators);
        return ZR_NULL;
    }

    startLoc = get_current_token_location(ps);
    // 解析 const 关键字（可选，可以在 var 之前或之后）
    TZrBool isConst = ps->lexer->t.token == ZR_TK_LET;
    if (isConst) {
        ZrParser_Lexer_Next(ps->lexer);
    }
    if (ps->lexer->t.token == ZR_TK_CONST) {
        isConst = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    // var 关键字是可选的（如果已经有 const，可以省略 var）
    if (!isConst && ps->lexer->t.token == ZR_TK_VAR) {
        ZrParser_Lexer_Next(ps->lexer);

        // `let` is the sole immutable binding spelling after the cutover.
        if (ps->lexer->t.token == ZR_TK_CONST) {
            report_removed_legacy_syntax(
                    ps,
                    "var const class field",
                    "Use `let field: Type = value;` for an immutable class field.");
            free_ast_node_array_with_elements(ps->state, decorators);
            return ZR_NULL;
        }
    } else if (!isConst) {
        // 如果没有 const 也没有 var，期望 var 关键字
        expect_token(ps, ZR_TK_VAR);
        ZrParser_Lexer_Next(ps->lexer);
    }

    // 解析字段名
    SZrAstNode *nameNode = parse_member_identifier(ps);
    if (nameNode == ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, decorators);
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    SZrFileRange nameLoc = nameNode->location;

    // 可选类型注解
    SZrType *typeInfo = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        typeInfo = parse_type(ps);
    }

    // 可选初始值
    SZrAstNode *init = ZR_NULL;
    if (consume_token(ps, ZR_TK_EQUALS)) {
        init = parse_expression(ps);
    }

    endLoc = get_current_token_location(ps);
    if (ps->lexer->t.token != ZR_TK_SEMICOLON) {
        report_missing_statement_semicolon(ps, "class field declaration", endLoc);
    } else {
        consume_token(ps, ZR_TK_SEMICOLON);
    }
    SZrFileRange fieldLoc;

    SZrAstNode *node;
    if (!ZrParser_MergeLocations(ps, startLoc, endLoc, &fieldLoc) ||
        (node = create_ast_node(ps, ZR_AST_CLASS_FIELD, fieldLoc)) == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, nameNode);
        free_owned_type(ps->state, typeInfo);
        ZrParser_Ast_Free(ps->state, init);
        free_ast_node_array_with_elements(ps->state, decorators);
        return ZR_NULL;
    }

    node->data.classField.decorators = decorators;
    node->data.classField.access = access;
    node->data.classField.isStatic = isStatic;
    node->data.classField.reservedRemovedUsingManaged = ZR_FALSE;
    node->data.classField.isConst = isConst;
    node->data.classField.name = name;
    node->data.classField.nameLocation = nameLoc;
    node->data.classField.typeInfo = typeInfo;
    node->data.classField.init = init;
    return node;
}


/* 仅供旧式分裂属性的迁移解析；普通 class 属性走统一 property 声明。
 * getter AST 交给 classProperty，再由迁移收集器生成建议并释放。 */
SZrAstNode *parse_property_get(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);
    TZrUInt32 modifierFlags;

    // 期望 get 关键字
    expect_token(ps, ZR_TK_GET);
    ZrParser_Lexer_Next(ps->lexer);

    modifierFlags = parse_declaration_modifier_flags(ps, class_member_allowed_modifier_flags());

    // 解析属性名
    SZrAstNode *nameNode = parse_member_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    SZrFileRange nameLoc = nameNode->location;

    // 可选返回类型
    SZrType *targetType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        targetType = parse_type(ps);
    }

    // 解析方法体；抽象访问器等场景允许使用 ';'。
    SZrAstNode *body = ZR_NULL;
    SZrFileRange endLoc;
    if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
        endLoc = get_current_token_location(ps);
        consume_token(ps, ZR_TK_SEMICOLON);
    } else if (ps->lexer->t.token == ZR_TK_LBRACE) {
        body = parse_block(ps);
        if (body == ZR_NULL) {
            ZrParser_Ast_Free(ps->state, nameNode);
            free_owned_type(ps->state, targetType);
            return ZR_NULL;
        }
        endLoc = body->location;
    } else {
        endLoc = get_current_token_location(ps);
        report_missing_statement_semicolon(ps, "class getter", endLoc);
    }

    SZrFileRange getLoc;

    SZrAstNode *node;
    if (!ZrParser_MergeLocations(ps, startLoc, endLoc, &getLoc) ||
        (node = create_ast_node(ps, ZR_AST_PROPERTY_GET, getLoc)) == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, nameNode);
        free_owned_type(ps->state, targetType);
        ZrParser_Ast_Free(ps->state, body);
        return ZR_NULL;
    }

    node->data.propertyGet.modifierFlags = modifierFlags;
    node->data.propertyGet.name = name;
    node->data.propertyGet.nameLocation = nameLoc;
    node->data.propertyGet.targetType = targetType;
    node->data.propertyGet.body = body;
    return node;
}

/* 与旧式 getter 成对供迁移诊断使用；参数名可能在替换文本里变为局部绑定。 */
SZrAstNode *parse_property_set(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);
    TZrUInt32 modifierFlags;

    // 期望 set 关键字
    expect_token(ps, ZR_TK_SET);
    ZrParser_Lexer_Next(ps->lexer);

    modifierFlags = parse_declaration_modifier_flags(ps, class_member_allowed_modifier_flags());

    // 解析属性名
    SZrAstNode *nameNode = parse_member_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    SZrFileRange nameLoc = nameNode->location;

    // 解析参数列表
    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);

    // 解析参数名
    SZrAstNode *paramNode = parse_identifier(ps);
    if (paramNode == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, nameNode);
        return ZR_NULL;
    }
    SZrIdentifier *param = &paramNode->data.identifier;

    // 可选参数类型
    SZrType *targetType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        targetType = parse_type(ps);
    }

    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        report_missing_parameter_list_close(ps, get_current_token_location(ps));
        ZrParser_Ast_Free(ps->state, nameNode);
        ZrParser_Ast_Free(ps->state, paramNode);
        free_owned_type(ps->state, targetType);
        return ZR_NULL;
    }
    consume_token(ps, ZR_TK_RPAREN);

    // 解析方法体；抽象访问器等场景允许使用 ';'。
    SZrAstNode *body = ZR_NULL;
    SZrFileRange endLoc;
    if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
        endLoc = get_current_token_location(ps);
        consume_token(ps, ZR_TK_SEMICOLON);
    } else if (ps->lexer->t.token == ZR_TK_LBRACE) {
        body = parse_block(ps);
        if (body == ZR_NULL) {
            ZrParser_Ast_Free(ps->state, nameNode);
            ZrParser_Ast_Free(ps->state, paramNode);
            free_owned_type(ps->state, targetType);
            return ZR_NULL;
        }
        endLoc = body->location;
    } else {
        endLoc = get_current_token_location(ps);
        report_missing_statement_semicolon(ps, "class setter", endLoc);
    }

    SZrFileRange setLoc;

    SZrAstNode *node;
    if (!ZrParser_MergeLocations(ps, startLoc, endLoc, &setLoc) ||
        (node = create_ast_node(ps, ZR_AST_PROPERTY_SET, setLoc)) == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, nameNode);
        ZrParser_Ast_Free(ps->state, paramNode);
        free_owned_type(ps->state, targetType);
        ZrParser_Ast_Free(ps->state, body);
        return ZR_NULL;
    }

    node->data.propertySet.modifierFlags = modifierFlags;
    node->data.propertySet.name = name;
    node->data.propertySet.nameLocation = nameLoc;
    node->data.propertySet.param = param;
    node->data.propertySet.targetType = targetType;
    node->data.propertySet.body = body;
    return node;
}

/* 迁移模式暂存单个旧式 getter/setter，不能作为现代属性直接进入编译器成员表。 */
SZrAstNode *parse_class_property(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);
    TZrUInt32 modifierFlags = ZR_DECLARATION_MODIFIER_NONE;

    // 解析装饰器（可选）
    SZrAstNodeArray *decorators = parse_leading_decorators(ps);
    if (decorators == ZR_NULL) {
        return ZR_NULL;
    }

    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);

    // 解析 static 关键字（可选）
    TZrBool isStatic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    modifierFlags = parse_declaration_modifier_flags(ps, class_member_allowed_modifier_flags());

    // 解析 get 或 set
    SZrAstNode *modifier = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_GET) {
        modifier = parse_property_get(ps);
    } else if (ps->lexer->t.token == ZR_TK_SET) {
        modifier = parse_property_set(ps);
    } else {
        free_ast_node_array_with_elements(ps->state, decorators);
        report_error(ps, "Expected 'get' or 'set' for property");
        return ZR_NULL;
    }

    if (modifier == ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, decorators);
        return ZR_NULL;
    }

    SZrFileRange endLoc = modifier->location;
    SZrFileRange propertyLoc;

    SZrAstNode *node;
    if (!ZrParser_MergeLocations(ps, startLoc, endLoc, &propertyLoc) ||
        (node = create_ast_node(ps, ZR_AST_CLASS_PROPERTY, propertyLoc)) == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, modifier);
        free_ast_node_array_with_elements(ps->state, decorators);
        return ZR_NULL;
    }

    node->data.classProperty.decorators = decorators;
    node->data.classProperty.access = access;
    node->data.classProperty.isStatic = isStatic;
    node->data.classProperty.modifierFlags = modifierFlags;
    node->data.classProperty.modifier = modifier;
    return node;
}
