#include "parser_internal.h"

/* class 与统一 property 声明共用修饰符词法范围；具体组合由容器语义阶段检查。 */
static TZrUInt32 property_allowed_modifier_flags(void) {
    return ZR_DECLARATION_MODIFIER_ABSTRACT |
           ZR_DECLARATION_MODIFIER_VIRTUAL |
           ZR_DECLARATION_MODIFIER_OVERRIDE |
           ZR_DECLARATION_MODIFIER_FINAL |
           ZR_DECLARATION_MODIFIER_SHADOW;
}

/* 访问器内部的可见性覆盖仅在明确出现访问关键字时生效。 */
static TZrBool property_access_modifier_starts_here(SZrParserState *ps) {
    EZrToken token;

    if (ps == ZR_NULL || ps->lexer == ZR_NULL) {
        return ZR_FALSE;
    }
    token = ps->lexer->t.token;
    return token == ZR_TK_PRI || token == ZR_TK_PRO || token == ZR_TK_PUB;
}

/* 在属性体内判别访问器边界，不消耗正式解析器的输入。 */
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

/* 属性声明尚未交付 AST 时的统一清理入口，连同装饰器和访问器子树回收。 */
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

/* class/struct/interface 成员分派器的试探入口；临时装饰器必须回收，
 * 然后恢复游标，让正式属性解析重新取得输入和所有权。 */
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

/* 将 contextual init 与 get/set 收敛到同一访问器角色；位置用于后续诊断。 */
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

/* 为统一 property AST 生成一个访问器，保留 bodyless、表达式和块体形状；
 * 访问器自己的可见性可覆盖属性级默认值，语义约束留给编译器。 */
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

/* 三种容器的正式 property 解析入口，统一产生 ZR_AST_PROPERTY_DECLARATION。
 * 容器实参只表示分派来源；编译器从父声明恢复容器并检查适用规则。
 * 成功时名称、类型、装饰器和访问器均转交 AST；失败时调用本文件的清理入口。 */
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
