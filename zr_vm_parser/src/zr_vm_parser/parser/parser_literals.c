#include "parser_internal.h"

// 统一分配并清零 AST 节点，分配失败设置 parser 错误；子树所有权由具体构造器赋予。
SZrAstNode *create_ast_node(SZrParserState *ps, EZrAstNodeType type, SZrFileRange location) {
    if (ps == ZR_NULL || ps->lexer == ZR_NULL || ps->hasFatalError ||
        ps->lexer->hasSourceEvidenceError) {
        return ZR_NULL;
    }
    SZrAstNode *node = ZrParser_Ownership_Allocate(
            ps->lexer->ownership, sizeof(SZrAstNode),
            ZR_MEMORY_NATIVE_TYPE_ARRAY, ZR_TRUE);
    if (node == ZR_NULL) {
        ps->hasFatalError = ZR_TRUE;
        report_error(ps, "Failed to allocate AST node");
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(node, 0, sizeof(SZrAstNode));
    node->type = type;
    node->location = location;
    node->ownership = ps->lexer->ownership;
    ZrParser_Ownership_Retain(node->ownership);
    return node;
}

// 把词法名称、原始位置和已捕获的语法上下文装入标识符节点；名称由 VM 状态持有。
SZrAstNode *create_identifier_node_with_context(SZrParserState *ps,
                                                SZrString *name,
                                                SZrFileRange location,
                                                SZrSyntaxContext syntaxContext) {
    if (!ZrParser_Lexer_HoldString(ps->lexer, name)) {
        ps->hasFatalError = ZR_TRUE;
        report_error(ps, "Failed to retain identifier string");
        return ZR_NULL;
    }
    SZrAstNode *node = create_ast_node(ps, ZR_AST_IDENTIFIER_LITERAL, location);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.identifier.name = name;
    node->data.identifier.syntaxContext =
            ZrParser_SyntaxContext_IsValid(syntaxContext)
                    ? syntaxContext
                    : ZrParser_SyntaxContext_Root();
    return node;
}

// 把词法名称与精确源位置装入标识符节点；调用者仍位于当前 token 时，
// 读取当前 token 的上下文并转交给显式构造器。
SZrAstNode *create_identifier_node_with_location(SZrParserState *ps,
                                                 SZrString *name,
                                                 SZrFileRange location) {
    SZrSyntaxContext syntaxContext =
            ps != ZR_NULL && ps->lexer != ZR_NULL
                    ? ps->lexer->t.syntaxContext
                    : ZrParser_SyntaxContext_Root();
    return create_identifier_node_with_context(ps, name, location, syntaxContext);
}

// 为当前 lexer 位置创建标识符节点，供不需要显式位置的解析分支使用。
// TODO: 当前仅见内部声明与此定义，需核对调用者是否已统一使用显式位置构造，再决定保留或清理。
SZrAstNode *create_identifier_node(SZrParserState *ps, SZrString *name) {
    SZrFileRange loc = get_current_location(ps);
    return create_identifier_node_with_location(ps, name, loc);
}

// 将布尔 token 的值放入叶子 AST，位置由调用方按 token 修正。
SZrAstNode *create_boolean_literal_node(SZrParserState *ps, TZrBool value) {
    SZrFileRange loc = get_current_location(ps);
    SZrAstNode *node = create_ast_node(ps, ZR_AST_BOOLEAN_LITERAL, loc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.booleanLiteral.value = value;
    return node;
}

// 保留整数值和原始拼写，后续诊断与类型阶段可区分源码表示。
SZrAstNode *create_integer_literal_node(SZrParserState *ps, TZrInt64 value, SZrString *literal) {
    if (!ZrParser_Lexer_HoldString(ps->lexer, literal)) {
        ps->hasFatalError = ZR_TRUE;
        report_error(ps, "Failed to retain integer spelling");
        return ZR_NULL;
    }
    SZrFileRange loc = get_current_location(ps);
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTEGER_LITERAL, loc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.integerLiteral.value = value;
    node->data.integerLiteral.literal = literal;
    return node;
}

// 保留浮点值、原始拼写与单精度后缀标志。
SZrAstNode *create_float_literal_node(SZrParserState *ps, TZrDouble value, SZrString *literal,
                                             TZrBool isSingle) {
    if (!ZrParser_Lexer_HoldString(ps->lexer, literal)) {
        ps->hasFatalError = ZR_TRUE;
        report_error(ps, "Failed to retain float spelling");
        return ZR_NULL;
    }
    SZrFileRange loc = get_current_location(ps);
    SZrAstNode *node = create_ast_node(ps, ZR_AST_FLOAT_LITERAL, loc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.floatLiteral.value = value;
    node->data.floatLiteral.literal = literal;
    node->data.floatLiteral.isSingle = isSingle;
    return node;
}

// 将解析后的字符串和词法错误位交给带位置的通用字符串构造器。
SZrAstNode *create_string_literal_node(SZrParserState *ps, SZrString *value, TZrBool hasError,
                                              SZrString *literal) {
    SZrFileRange loc = get_current_location(ps);
    return create_string_literal_node_with_location(ps, value, hasError, literal, loc);
}

// 构造字符串叶子并记录词法错误，保留调用方给出的 token 范围。
SZrAstNode *create_string_literal_node_with_location(SZrParserState *ps,
                                                            SZrString *value,
                                                            TZrBool hasError,
                                                            SZrString *literal,
                                                            SZrFileRange location) {
    if (!ZrParser_Lexer_HoldString(ps->lexer, value) ||
        !ZrParser_Lexer_HoldString(ps->lexer, literal)) {
        ps->hasFatalError = ZR_TRUE;
        report_error(ps, "Failed to retain string literal");
        return ZR_NULL;
    }
    SZrAstNode *node = create_ast_node(ps, ZR_AST_STRING_LITERAL, location);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.stringLiteral.value = value;
    node->data.stringLiteral.hasError = hasError;
    node->data.stringLiteral.literal = literal;
    return node;
}

// 构造字符叶子并保留词法错误与原始拼写。
SZrAstNode *create_char_literal_node(SZrParserState *ps, TZrChar value, TZrBool hasError, SZrString *literal) {
    if (!ZrParser_Lexer_HoldString(ps->lexer, literal)) {
        ps->hasFatalError = ZR_TRUE;
        report_error(ps, "Failed to retain character spelling");
        return ZR_NULL;
    }
    SZrFileRange loc = get_current_location(ps);
    SZrAstNode *node = create_ast_node(ps, ZR_AST_CHAR_LITERAL, loc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.charLiteral.value = value;
    node->data.charLiteral.hasError = hasError;
    node->data.charLiteral.literal = literal;
    return node;
}

// 空值字面量无子树，直接返回当前源位置的叶子节点。
SZrAstNode *create_null_literal_node(SZrParserState *ps) {
    SZrFileRange loc = get_current_location(ps);
    return create_ast_node(ps, ZR_AST_NULL_LITERAL, loc);
}

// 成功时接管静态及插值片段数组，由模板节点的递归释放处理。
SZrAstNode *create_template_string_literal_node(SZrParserState *ps, SZrAstNodeArray *segments) {
    SZrFileRange loc = get_current_location(ps);
    SZrAstNode *node = create_ast_node(ps, ZR_AST_TEMPLATE_STRING_LITERAL, loc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.templateStringLiteral.segments = segments;
    return node;
}

// 成功时接管内嵌表达式节点，作为模板段的子树。
SZrAstNode *create_interpolated_segment_node(SZrParserState *ps, SZrAstNode *expression) {
    SZrFileRange loc = get_current_location(ps);
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERPOLATED_SEGMENT, loc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.interpolatedSegment.expression = expression;
    return node;
}

// 按短/长字符串布局取得借用的字节视图与长度；不转移字符串所有权。
void get_string_native_parts(SZrString *value, TZrNativeString *nativeValue, TZrSize *length) {
    if (nativeValue == ZR_NULL || length == ZR_NULL) {
        return;
    }

    *nativeValue = ZR_NULL;
    *length = 0;
    if (value == ZR_NULL) {
        return;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        *nativeValue = ZrCore_String_GetNativeStringShort(value);
        *length = value->shortStringLength;
    } else {
        *nativeValue = ZrCore_String_GetNativeString(value);
        *length = value->longStringLength;
    }
}

// 长度先行后逐字节比较，避免把 VM 字符串当零终止文本。
TZrBool zr_string_equals_literal(SZrString *value, const TZrChar *literal) {
    TZrNativeString nativeValue;
    TZrSize length;
    TZrSize literalLength;

    if (value == ZR_NULL || literal == ZR_NULL) {
        return ZR_FALSE;
    }

    get_string_native_parts(value, &nativeValue, &length);
    literalLength = strlen(literal);
    if (nativeValue == ZR_NULL || length != literalLength) {
        return ZR_FALSE;
    }

    return memcmp(nativeValue, literal, literalLength) == 0;
}


// ==================== 字面量解析 ====================

// 按 token 创建字面量叶子并消费 token；数值保留原始文本供后续阶段使用。
SZrAstNode *parse_literal(SZrParserState *ps) {
    ZR_UNUSED_PARAMETER(get_current_location(ps));
    EZrToken token = ps->lexer->t.token;
    SZrFileRange literalLoc = get_current_token_location(ps);
    SZrAstNode *node;

    if ((token == ZR_TK_INTEGER || token == ZR_TK_FLOAT) && ps->lexer->t.hasLexError) {
        report_error(ps, ps->lexer->t.lexErrorMessage != ZR_NULL
                         ? ps->lexer->t.lexErrorMessage : "Invalid numeric literal");
        ZrParser_Lexer_Next(ps->lexer);
        return ZR_NULL;
    }

    switch (token) {
        case ZR_TK_BOOLEAN: {
            TZrBool value = ps->lexer->t.seminfo.booleanValue;
            node = create_boolean_literal_node(ps, value);
            if (node != ZR_NULL) {
                node->location = literalLoc;
            }
            ZrParser_Lexer_Next(ps->lexer);
            return node;
        }

        case ZR_TK_INTEGER: {
            TZrInt64 value = ps->lexer->t.seminfo.intValue;
            SZrString *literal = ps->lexer->t.seminfo.stringValue; // 注意：这里需要从 token 中获取原始字符串
            node = create_integer_literal_node(ps, value, literal);
            if (node != ZR_NULL) {
                node->location = literalLoc;
            }
            ZrParser_Lexer_Next(ps->lexer);
            return node;
        }

        case ZR_TK_FLOAT: {
            TZrDouble value = ps->lexer->t.seminfo.floatValue;
            SZrString *literal = ps->lexer->t.seminfo.stringValue;
            // 判断是否为单精度（从原始字符串判断）
            // 单精度float通常以'f'或'F'结尾
            TZrBool isSingle = ZR_FALSE;
            if (literal != ZR_NULL) {
                TZrNativeString literalStr = ZrCore_String_GetNativeString(literal);
                if (literalStr != ZR_NULL) {
                    TZrSize len = (literal->shortStringLength < ZR_VM_LONG_STRING_FLAG)
                                          ? (TZrSize) literal->shortStringLength
                                          : literal->longStringLength;
                    if (len > 0) {
                        // 检查最后一个字符是否为'f'或'F'
                        TZrChar lastChar = literalStr[len - 1];
                        if (lastChar == 'f' || lastChar == 'F') {
                            isSingle = ZR_TRUE;
                        }
                    }
                }
            }
            node = create_float_literal_node(ps, value, literal, isSingle);
            if (node != ZR_NULL) {
                node->location = literalLoc;
            }
            ZrParser_Lexer_Next(ps->lexer);
            return node;
        }

        case ZR_TK_STRING: {
            SZrString *value = ps->lexer->t.seminfo.stringValue;
            TZrBool hasError = ps->lexer->t.hasLexError;
            SZrString *literal = value; // 原始字符串已经存储在stringValue中
            node = create_string_literal_node_with_location(ps, value, hasError, literal, literalLoc);
            ZrParser_Lexer_Next(ps->lexer);
            return node;
        }

        case ZR_TK_TEMPLATE_STRING: {
            SZrString *value = ps->lexer->t.seminfo.stringValue;
            SZrSyntaxContext syntaxContext = ps->lexer->t.syntaxContext;
            SZrTokenSpan origin = ps->lexer->t.span;
            ZrParser_Lexer_Next(ps->lexer);
            node = parse_template_string_literal(ps, value, syntaxContext, origin, &literalLoc);
            if (node != ZR_NULL) {
                node->location = literalLoc;
            }
            return node;
        }

        case ZR_TK_CHAR: {
            TZrChar value = ps->lexer->t.seminfo.charValue;
            TZrBool hasError = ps->lexer->t.hasLexError;
            SZrString *literal = ps->lexer->t.seminfo.stringValue; // 如果lexer存储了原始字符串，使用它
            node = create_char_literal_node(ps, value, hasError, literal);
            if (node != ZR_NULL) {
                node->location = literalLoc;
            }
            ZrParser_Lexer_Next(ps->lexer);
            return node;
        }

        case ZR_TK_NULL: {
            node = create_null_literal_node(ps);
            if (node != ZR_NULL) {
                node->location = literalLoc;
            }
            ZrParser_Lexer_Next(ps->lexer);
            return node;
        }

        case ZR_TK_INFINITY: {
            SZrString *literal = ZrParser_Lexer_CreateString(ps->lexer, "Infinity", 8);
            node = create_float_literal_node(ps, INFINITY, literal, ZR_FALSE); // 正无穷
            if (node != ZR_NULL) {
                node->location = literalLoc;
            }
            ZrParser_Lexer_Next(ps->lexer);
            return node;
        }

        case ZR_TK_NEG_INFINITY: {
            SZrString *literal = ZrParser_Lexer_CreateString(ps->lexer, "NegativeInfinity", 16);
            node = create_float_literal_node(ps, -INFINITY, literal, ZR_FALSE); // 负无穷
            if (node != ZR_NULL) {
                node->location = literalLoc;
            }
            ZrParser_Lexer_Next(ps->lexer);
            return node;
        }

        case ZR_TK_NAN: {
            SZrString *literal = ZrParser_Lexer_CreateString(ps->lexer, "NaN", 3);
            node = create_float_literal_node(ps, NAN, literal, ZR_FALSE); // NaN
            if (node != ZR_NULL) {
                node->location = literalLoc;
            }
            ZrParser_Lexer_Next(ps->lexer);
            return node;
        }

        default:
            report_error(ps, "Expected literal");
            return ZR_NULL;
    }
}

// 保留 token 原始位置和 VM 字符串；所有权内建词在普通命名空间被拒绝。
SZrAstNode *parse_identifier(SZrParserState *ps) {
    SZrFileRange identifierLoc;
    SZrString *name;
    SZrSyntaxContext identifierContext;
    if (is_ownership_intrinsic_token(ps->lexer->t.token)) {
        if (!report_reserved_ownership_intrinsic_name(ps)) {
            report_error(
                    ps,
                    "Ownership intrinsic name is reserved in lexical namespaces");
        }
        return ZR_NULL;
    }
    // 允许 test 关键字作为标识符（用于方法名等）
    if (ps->lexer->t.token != ZR_TK_IDENTIFIER && ps->lexer->t.token != ZR_TK_TEST) {
        report_error(ps, "Expected identifier");
        return ZR_NULL;
    }

    name = ps->lexer->t.seminfo.stringValue;
    if (ps->lexer->t.token == ZR_TK_TEST && name == ZR_NULL) {
        name = ZrParser_Lexer_CreateString(ps->lexer, "test", 4);
    }

    identifierLoc = get_current_token_location(ps);
    identifierContext = ps->lexer->t.syntaxContext;
    ZrParser_Lexer_Next(ps->lexer);
    return create_identifier_node_with_context(
            ps, name, identifierLoc, identifierContext);
}

// ==================== 表达式解析（按优先级从低到高）====================

// 数组元素禁赋值的 token 判别与赋值表达式层保持一致。
static TZrBool parser_array_literal_token_is_assignment(EZrToken token) {
    return token == ZR_TK_EQUALS ||
           token == ZR_TK_PLUS_EQUALS ||
           token == ZR_TK_MINUS_EQUALS ||
           token == ZR_TK_STAR_EQUALS ||
           token == ZR_TK_SLASH_EQUALS ||
           token == ZR_TK_PERCENT_EQUALS;
}

// 在数组分隔符错误处判断下一个 token 是否可能开始元素。
static TZrBool parser_array_literal_token_can_start_element(EZrToken token) {
    switch (token) {
        case ZR_TK_IDENTIFIER:
        case ZR_TK_TEST:
        case ZR_TK_BOOLEAN:
        case ZR_TK_INTEGER:
        case ZR_TK_FLOAT:
        case ZR_TK_STRING:
        case ZR_TK_TEMPLATE_STRING:
        case ZR_TK_CHAR:
        case ZR_TK_NULL:
        case ZR_TK_INFINITY:
        case ZR_TK_NEG_INFINITY:
        case ZR_TK_NAN:
        case ZR_TK_LPAREN:
        case ZR_TK_LBRACKET:
        case ZR_TK_LBRACE:
        case ZR_TK_BANG:
        case ZR_TK_TILDE:
        case ZR_TK_PLUS:
        case ZR_TK_MINUS:
        case ZR_TK_DOLLAR:
        case ZR_TK_NEW:
        case ZR_TK_USING:
        case ZR_TK_PERCENT:
        case ZR_TK_SUPER:
        case ZR_TK_LESS_THAN:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

// 数组元素只允许条件表达式；若后接赋值操作符，释放节点并报错。
static SZrAstNode *parse_array_literal_value_element(SZrParserState *ps) {
    SZrAstNode *element = parse_conditional_expression(ps);

    if (element != ZR_NULL && parser_array_literal_token_is_assignment(ps->lexer->t.token)) {
        report_array_element_assignment(ps, get_current_token_location(ps));
        ZrParser_Ast_Free(ps->state, element);
        return ZR_NULL;
    }

    return element;
}

// 对象属性缺分隔符时识别合法键起始 token，包括计算键。
static TZrBool parser_object_literal_token_can_start_property_key(EZrToken token) {
    return token == ZR_TK_IDENTIFIER || is_ownership_intrinsic_token(token) ||
           token == ZR_TK_STRING || token == ZR_TK_LBRACKET;
}

// 解析数组字面量

// 解析逗号或分号分隔的数组元素；成功后数组节点接管元素列表。
SZrAstNode *parse_array_literal(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);
    expect_token(ps, ZR_TK_LBRACKET);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNodeArray *elements = parser_ast_array_new(ps, ZR_PARSER_INITIAL_CAPACITY_SMALL);
    if (elements == ZR_NULL) {
        report_error(ps, "Failed to allocate array");
        return ZR_NULL;
    }

    // 解析第一个元素
    if (ps->lexer->t.token != ZR_TK_RBRACKET) {
        // 数组元素不应该包含赋值表达式，使用 conditional_expression
        SZrAstNode *first = parse_array_literal_value_element(ps);
        if (first != ZR_NULL) {
            if (!parser_ast_array_add(ps, elements, first)) {
                ZrParser_Ast_Free(ps->state, first);
                free_ast_node_array_with_elements(ps->state, elements);
                return ZR_NULL;
            }
        } else {
            free_ast_node_array_with_elements(ps->state, elements);
            return ZR_NULL;
        }

        // 解析后续元素
        while (ps->lexer->t.token == ZR_TK_COMMA || ps->lexer->t.token == ZR_TK_SEMICOLON) {
            ZrParser_Lexer_Next(ps->lexer);
            if (ps->lexer->t.token == ZR_TK_RBRACKET) {
                break;
            }
            // 数组元素不应该包含赋值表达式，使用 conditional_expression
            SZrAstNode *elem = parse_array_literal_value_element(ps);
            if (elem != ZR_NULL) {
                if (!parser_ast_array_add(ps, elements, elem)) {
                    ZrParser_Ast_Free(ps->state, elem);
                    free_ast_node_array_with_elements(ps->state, elements);
                    return ZR_NULL;
                }
            } else {
                free_ast_node_array_with_elements(ps->state, elements);
                return ZR_NULL;
            }
        }
    }

    // 可选的尾随分隔符
    if (ps->lexer->t.token == ZR_TK_COMMA || ps->lexer->t.token == ZR_TK_SEMICOLON) {
        ZrParser_Lexer_Next(ps->lexer);
    }

    if (parser_array_literal_token_can_start_element(ps->lexer->t.token)) {
        report_missing_array_element_separator(ps, get_current_token_location(ps));
        free_ast_node_array_with_elements(ps->state, elements);
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_RBRACKET) {
        report_missing_array_close(ps, startLoc);
        free_ast_node_array_with_elements(ps->state, elements);
        return ZR_NULL;
    }
    consume_token(ps, ZR_TK_RBRACKET);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange arrayLoc;

    SZrAstNode *node;
    if (!ZrParser_MergeLocations(ps, startLoc, endLoc, &arrayLoc) ||
        (node = create_ast_node(ps, ZR_AST_ARRAY_LITERAL, arrayLoc)) == ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, elements);
        return ZR_NULL;
    }

    node->data.arrayLiteral.elements = elements;
    return node;
}

// 解析对象字面量

// 解析普通或计算键及其值；成功后对象节点接管键值对列表。
SZrAstNode *parse_object_literal(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);
    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNodeArray *properties = parser_ast_array_new(ps, ZR_PARSER_INITIAL_CAPACITY_SMALL);
    if (properties == ZR_NULL) {
        report_error(ps, "Failed to allocate properties array");
        return ZR_NULL;
    }

    // 解析第一个键值对
    if (ps->lexer->t.token != ZR_TK_RBRACE) {
        // 解析键
        SZrAstNode *key = ZR_NULL;
        TZrBool keyIsComputed = ZR_FALSE;
        if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
            key = parse_identifier(ps);
        } else if (is_ownership_intrinsic_token(ps->lexer->t.token)) {
            key = parse_member_identifier(ps);
        } else if (ps->lexer->t.token == ZR_TK_STRING) {
            key = parse_literal(ps);
        } else if (ps->lexer->t.token == ZR_TK_LBRACKET) {
            // 计算键
            SZrFileRange keyOpenLocation = get_current_token_location(ps);
            keyIsComputed = ZR_TRUE;
            ZrParser_Lexer_Next(ps->lexer);
            key = parse_expression(ps);
            if (ps->lexer->t.token != ZR_TK_RBRACKET) {
                report_missing_object_computed_key_close(ps, keyOpenLocation);
                ZrParser_Ast_Free(ps->state, key);
                free_ast_node_array_with_elements(ps->state, properties);
                return ZR_NULL;
            }
            consume_token(ps, ZR_TK_RBRACKET);
        } else {
            report_error(ps, "Expected key in object literal");
            free_ast_node_array_with_elements(ps->state, properties);
            return ZR_NULL;
        }

        if (key == ZR_NULL) {
            free_ast_node_array_with_elements(ps->state, properties);
            return ZR_NULL;
        }
        if (!consume_token(ps, ZR_TK_COLON)) {
            report_missing_object_property_colon(ps, get_current_token_location(ps));
        }

        // 解析值
        SZrAstNode *value = parse_expression(ps);
        if (value == ZR_NULL) {
            ZrParser_Ast_Free(ps->state, key);
            free_ast_node_array_with_elements(ps->state, properties);
            return ZR_NULL;
        }

        // 创建键值对节点
        SZrFileRange kvLoc;
        SZrAstNode *kvNode;
        if (!ZrParser_MergeLocations(ps, key->location, value->location, &kvLoc) ||
            (kvNode = create_ast_node(ps, ZR_AST_KEY_VALUE_PAIR, kvLoc)) == ZR_NULL) {
            ZrParser_Ast_Free(ps->state, key);
            ZrParser_Ast_Free(ps->state, value);
            free_ast_node_array_with_elements(ps->state, properties);
            return ZR_NULL;
        }
        kvNode->data.keyValuePair.key = key;
        kvNode->data.keyValuePair.value = value;
        kvNode->data.keyValuePair.keyIsComputed = keyIsComputed;
        if (!parser_ast_array_add(ps, properties, kvNode)) {
            ZrParser_Ast_Free(ps->state, kvNode);
            free_ast_node_array_with_elements(ps->state, properties);
            return ZR_NULL;
        }

        // 解析后续键值对
        while (ps->lexer->t.token == ZR_TK_COMMA || ps->lexer->t.token == ZR_TK_SEMICOLON) {
            ZrParser_Lexer_Next(ps->lexer);
            if (ps->lexer->t.token == ZR_TK_RBRACE) {
                break;
            }

            // 解析键
            key = ZR_NULL;
            keyIsComputed = ZR_FALSE;
            if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
                key = parse_identifier(ps);
            } else if (is_ownership_intrinsic_token(ps->lexer->t.token)) {
                key = parse_member_identifier(ps);
            } else if (ps->lexer->t.token == ZR_TK_STRING) {
                key = parse_literal(ps);
            } else if (ps->lexer->t.token == ZR_TK_LBRACKET) {
                SZrFileRange keyOpenLocation = get_current_token_location(ps);
                keyIsComputed = ZR_TRUE;
                ZrParser_Lexer_Next(ps->lexer);
                key = parse_expression(ps);
                if (ps->lexer->t.token != ZR_TK_RBRACKET) {
                    report_missing_object_computed_key_close(ps, keyOpenLocation);
                    ZrParser_Ast_Free(ps->state, key);
                    free_ast_node_array_with_elements(ps->state, properties);
                    return ZR_NULL;
                }
                consume_token(ps, ZR_TK_RBRACKET);
            } else {
                break;
            }

            if (key == ZR_NULL) {
                free_ast_node_array_with_elements(ps->state, properties);
                return ZR_NULL;
            }
            if (!consume_token(ps, ZR_TK_COLON)) {
                report_missing_object_property_colon(ps, get_current_token_location(ps));
            }

            value = parse_expression(ps);
            if (value == ZR_NULL) {
                ZrParser_Ast_Free(ps->state, key);
                break;
            }

            if (!ZrParser_MergeLocations(ps, key->location, value->location, &kvLoc)) {
                ZrParser_Ast_Free(ps->state, key);
                ZrParser_Ast_Free(ps->state, value);
                free_ast_node_array_with_elements(ps->state, properties);
                return ZR_NULL;
            }
            kvNode = create_ast_node(ps, ZR_AST_KEY_VALUE_PAIR, kvLoc);
            if (kvNode == ZR_NULL) {
                ZrParser_Ast_Free(ps->state, key);
                ZrParser_Ast_Free(ps->state, value);
                break;
            }
            kvNode->data.keyValuePair.key = key;
            kvNode->data.keyValuePair.value = value;
            kvNode->data.keyValuePair.keyIsComputed = keyIsComputed;
            if (!parser_ast_array_add(ps, properties, kvNode)) {
                ZrParser_Ast_Free(ps->state, kvNode);
                free_ast_node_array_with_elements(ps->state, properties);
                return ZR_NULL;
            }
        }
    }

    // 可选的尾随分隔符
    if (ps->lexer->t.token == ZR_TK_COMMA || ps->lexer->t.token == ZR_TK_SEMICOLON) {
        ZrParser_Lexer_Next(ps->lexer);
    }

    if (parser_object_literal_token_can_start_property_key(ps->lexer->t.token)) {
        report_missing_object_property_separator(ps, get_current_token_location(ps));
        free_ast_node_array_with_elements(ps->state, properties);
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_RBRACE) {
        report_missing_object_close(ps, startLoc);
        free_ast_node_array_with_elements(ps->state, properties);
        return ZR_NULL;
    }
    consume_token(ps, ZR_TK_RBRACE);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange objectLoc;

    SZrAstNode *node;
    if (!ZrParser_MergeLocations(ps, startLoc, endLoc, &objectLoc) ||
        (node = create_ast_node(ps, ZR_AST_OBJECT_LITERAL, objectLoc)) == ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, properties);
        return ZR_NULL;
    }

    node->data.objectLiteral.properties = properties;
    return node;
}

// 解析函数调用参数列表
// 解析参数列表，支持命名参数（paramName: value）
// 返回参数值数组，通过 argNames 输出参数名数组（ZR_NULL 表示位置参数）
