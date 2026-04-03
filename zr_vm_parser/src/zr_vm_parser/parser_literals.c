#include "parser_internal.h"

SZrAstNode *create_ast_node(SZrParserState *ps, EZrAstNodeType type, SZrFileRange location) {
    SZrAstNode *node =
            ZrCore_Memory_RawMallocWithType(ps->state->global, sizeof(SZrAstNode), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (node == ZR_NULL) {
        report_error(ps, "Failed to allocate AST node");
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(node, 0, sizeof(SZrAstNode));
    node->type = type;
    node->location = location;
    return node;
}

SZrAstNode *create_identifier_node_with_location(SZrParserState *ps, SZrString *name, SZrFileRange location) {
    SZrAstNode *node = create_ast_node(ps, ZR_AST_IDENTIFIER_LITERAL, location);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.identifier.name = name;
    return node;
}

SZrAstNode *create_identifier_node(SZrParserState *ps, SZrString *name) {
    SZrFileRange loc = get_current_location(ps);
    return create_identifier_node_with_location(ps, name, loc);
}

SZrAstNode *create_boolean_literal_node(SZrParserState *ps, TZrBool value) {
    SZrFileRange loc = get_current_location(ps);
    SZrAstNode *node = create_ast_node(ps, ZR_AST_BOOLEAN_LITERAL, loc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.booleanLiteral.value = value;
    return node;
}

SZrAstNode *create_integer_literal_node(SZrParserState *ps, TZrInt64 value, SZrString *literal) {
    SZrFileRange loc = get_current_location(ps);
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTEGER_LITERAL, loc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.integerLiteral.value = value;
    node->data.integerLiteral.literal = literal;
    return node;
}

SZrAstNode *create_float_literal_node(SZrParserState *ps, TZrDouble value, SZrString *literal,
                                             TZrBool isSingle) {
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

SZrAstNode *create_string_literal_node(SZrParserState *ps, SZrString *value, TZrBool hasError,
                                              SZrString *literal) {
    SZrFileRange loc = get_current_location(ps);
    return create_string_literal_node_with_location(ps, value, hasError, literal, loc);
}

SZrAstNode *create_string_literal_node_with_location(SZrParserState *ps,
                                                            SZrString *value,
                                                            TZrBool hasError,
                                                            SZrString *literal,
                                                            SZrFileRange location) {
    SZrAstNode *node = create_ast_node(ps, ZR_AST_STRING_LITERAL, location);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.stringLiteral.value = value;
    node->data.stringLiteral.hasError = hasError;
    node->data.stringLiteral.literal = literal;
    return node;
}

SZrAstNode *create_char_literal_node(SZrParserState *ps, TZrChar value, TZrBool hasError, SZrString *literal) {
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

SZrAstNode *create_null_literal_node(SZrParserState *ps) {
    SZrFileRange loc = get_current_location(ps);
    return create_ast_node(ps, ZR_AST_NULL_LITERAL, loc);
}

SZrAstNode *create_template_string_literal_node(SZrParserState *ps, SZrAstNodeArray *segments) {
    SZrFileRange loc = get_current_location(ps);
    SZrAstNode *node = create_ast_node(ps, ZR_AST_TEMPLATE_STRING_LITERAL, loc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.templateStringLiteral.segments = segments;
    return node;
}

SZrAstNode *create_interpolated_segment_node(SZrParserState *ps, SZrAstNode *expression) {
    SZrFileRange loc = get_current_location(ps);
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERPOLATED_SEGMENT, loc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.interpolatedSegment.expression = expression;
    return node;
}

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

SZrAstNode *parse_embedded_expression(SZrParserState *ps, const TZrChar *source, TZrSize sourceLength) {
    SZrParserState nestedParser;
    SZrAstNode *expression = ZR_NULL;
    TZrChar *sourceCopy;

    if (ps == ZR_NULL || source == ZR_NULL) {
        return ZR_NULL;
    }

    sourceCopy = ZrCore_Memory_RawMallocWithType(ps->state->global, sourceLength + 1, ZR_MEMORY_NATIVE_TYPE_STRING);
    if (sourceCopy == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(sourceCopy, source, sourceLength);
    sourceCopy[sourceLength] = '\0';

    ZrParser_State_Init(&nestedParser, ps->state, sourceCopy, sourceLength,
                        ps->lexer != ZR_NULL ? ps->lexer->sourceName : ZR_NULL);
    if (!nestedParser.hasError) {
        expression = parse_expression(&nestedParser);
        if (nestedParser.hasError || expression == ZR_NULL || nestedParser.lexer->t.token != ZR_TK_EOS) {
            expression = ZR_NULL;
        }
    }

    ZrParser_State_Free(&nestedParser);
    ZrCore_Memory_RawFreeWithType(ps->state->global, sourceCopy, sourceLength + 1, ZR_MEMORY_NATIVE_TYPE_STRING);
    return expression;
}

TZrBool append_template_static_segment(SZrParserState *ps, SZrAstNodeArray *segments, const TZrChar *text,
                                              TZrSize length) {
    SZrString *segmentString;
    SZrAstNode *segmentNode;

    if (ps == ZR_NULL || segments == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }

    segmentString = ZrCore_String_Create(ps->state, (TZrNativeString)text, length);
    if (segmentString == ZR_NULL) {
        return ZR_FALSE;
    }

    segmentNode = create_string_literal_node(ps, segmentString, ZR_FALSE, segmentString);
    if (segmentNode == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_AstNodeArray_Add(ps->state, segments, segmentNode);
    return ZR_TRUE;
}

SZrAstNode *parse_template_string_literal(SZrParserState *ps, SZrString *rawValue) {
    TZrNativeString rawText;
    TZrSize rawLength;
    SZrAstNodeArray *segments;
    TZrSize segmentStart = 0;
    TZrSize index = 0;

    if (ps == ZR_NULL || rawValue == ZR_NULL) {
        return ZR_NULL;
    }

    get_string_native_parts(rawValue, &rawText, &rawLength);
    if (rawText == ZR_NULL) {
        return ZR_NULL;
    }

    segments = ZrParser_AstNodeArray_New(ps->state, 4);
    if (segments == ZR_NULL) {
        return ZR_NULL;
    }

    while (index < rawLength) {
        if (rawText[index] == '$' && index + 1 < rawLength && rawText[index + 1] == '{') {
            TZrSize expressionStart = index + 2;
            TZrSize cursor = expressionStart;
            TZrInt32 braceDepth = 1;
            TZrInt32 stringDelimiter = 0;

            if (!append_template_static_segment(ps, segments, rawText + segmentStart, index - segmentStart)) {
                ZrParser_AstNodeArray_Free(ps->state, segments);
                return ZR_NULL;
            }

            while (cursor < rawLength) {
                TZrChar current = rawText[cursor];

                if (stringDelimiter != 0) {
                    if (current == '\\' && cursor + 1 < rawLength) {
                        cursor += 2;
                        continue;
                    }
                    if (current == stringDelimiter) {
                        stringDelimiter = 0;
                    }
                    cursor++;
                    continue;
                }

                if (current == '\'' || current == '"' || current == '`') {
                    stringDelimiter = current;
                    cursor++;
                    continue;
                }

                if (current == '{') {
                    braceDepth++;
                } else if (current == '}') {
                    braceDepth--;
                    if (braceDepth == 0) {
                        break;
                    }
                }
                cursor++;
            }

            if (cursor >= rawLength || braceDepth != 0) {
                report_error(ps, "Unterminated template string interpolation");
                ZrParser_AstNodeArray_Free(ps->state, segments);
                return ZR_NULL;
            }

            {
                SZrAstNode *expression =
                        parse_embedded_expression(ps, rawText + expressionStart, cursor - expressionStart);
                SZrAstNode *segmentNode;

                if (expression == ZR_NULL) {
                    report_error(ps, "Failed to parse template string interpolation");
                    ZrParser_AstNodeArray_Free(ps->state, segments);
                    return ZR_NULL;
                }

                segmentNode = create_interpolated_segment_node(ps, expression);
                if (segmentNode == ZR_NULL) {
                    ZrParser_Ast_Free(ps->state, expression);
                    ZrParser_AstNodeArray_Free(ps->state, segments);
                    return ZR_NULL;
                }

                ZrParser_AstNodeArray_Add(ps->state, segments, segmentNode);
            }

            index = cursor + 1;
            segmentStart = index;
            continue;
        }

        index++;
    }

    if (!append_template_static_segment(ps, segments, rawText + segmentStart, rawLength - segmentStart)) {
        ZrParser_AstNodeArray_Free(ps->state, segments);
        return ZR_NULL;
    }

    return create_template_string_literal_node(ps, segments);
}

// ==================== 字面量解析 ====================

SZrAstNode *parse_literal(SZrParserState *ps) {
    ZR_UNUSED_PARAMETER(get_current_location(ps));
    EZrToken token = ps->lexer->t.token;

    switch (token) {
        case ZR_TK_BOOLEAN: {
            TZrBool value = ps->lexer->t.seminfo.booleanValue;
            ZrParser_Lexer_Next(ps->lexer);
            return create_boolean_literal_node(ps, value);
        }

        case ZR_TK_INTEGER: {
            TZrInt64 value = ps->lexer->t.seminfo.intValue;
            SZrString *literal = ps->lexer->t.seminfo.stringValue; // 注意：这里需要从 token 中获取原始字符串
            ZrParser_Lexer_Next(ps->lexer);
            return create_integer_literal_node(ps, value, literal);
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
            ZrParser_Lexer_Next(ps->lexer);
            return create_float_literal_node(ps, value, literal, isSingle);
        }

        case ZR_TK_STRING: {
            SZrString *value = ps->lexer->t.seminfo.stringValue;
            TZrBool hasError = ps->lexer->t.hasLexError;
            SZrString *literal = value; // 原始字符串已经存储在stringValue中
            ZrParser_Lexer_Next(ps->lexer);
            return create_string_literal_node(ps, value, hasError, literal);
        }

        case ZR_TK_TEMPLATE_STRING: {
            SZrString *value = ps->lexer->t.seminfo.stringValue;
            SZrAstNode *node;
            ZrParser_Lexer_Next(ps->lexer);
            node = parse_template_string_literal(ps, value);
            return node;
        }

        case ZR_TK_CHAR: {
            TZrChar value = ps->lexer->t.seminfo.charValue;
            TZrBool hasError = ps->lexer->t.hasLexError;
            SZrString *literal = ps->lexer->t.seminfo.stringValue; // 如果lexer存储了原始字符串，使用它
            ZrParser_Lexer_Next(ps->lexer);
            return create_char_literal_node(ps, value, hasError, literal);
        }

        case ZR_TK_NULL: {
            ZrParser_Lexer_Next(ps->lexer);
            return create_null_literal_node(ps);
        }

        case ZR_TK_INFINITY: {
            ZrParser_Lexer_Next(ps->lexer);
            SZrString *literal = ZrCore_String_Create(ps->state, "Infinity", 8);
            return create_float_literal_node(ps, INFINITY, literal, ZR_FALSE); // 正无穷
        }

        case ZR_TK_NEG_INFINITY: {
            ZrParser_Lexer_Next(ps->lexer);
            SZrString *literal = ZrCore_String_Create(ps->state, "NegativeInfinity", 16);
            return create_float_literal_node(ps, -INFINITY, literal, ZR_FALSE); // 负无穷
        }

        case ZR_TK_NAN: {
            ZrParser_Lexer_Next(ps->lexer);
            SZrString *literal = ZrCore_String_Create(ps->state, "NaN", 3);
            return create_float_literal_node(ps, NAN, literal, ZR_FALSE); // NaN
        }

        default:
            report_error(ps, "Expected literal");
            return ZR_NULL;
    }
}

SZrAstNode *parse_identifier(SZrParserState *ps) {
    SZrFileRange identifierLoc;
    SZrString *name;
    // 允许 test 关键字作为标识符（用于方法名等）
    if (ps->lexer->t.token != ZR_TK_IDENTIFIER && ps->lexer->t.token != ZR_TK_TEST) {
        report_error(ps, "Expected identifier");
        return ZR_NULL;
    }

    name = ps->lexer->t.seminfo.stringValue;
    if (ps->lexer->t.token == ZR_TK_TEST && name == ZR_NULL) {
        name = ZrCore_String_Create(ps->state, "test", 4);
    }

    if (name != ZR_NULL) {
        TZrNativeString nativeName = ZrCore_String_GetNativeString(name);
        if (nativeName != ZR_NULL &&
            strcmp(nativeName, "import") == 0 &&
            peek_token(ps) == ZR_TK_LPAREN) {
            report_error(ps, "Legacy import() syntax is not supported; use %import");
            skip_legacy_import_call(ps);
            return ZR_NULL;
        }
    }

    identifierLoc = get_current_token_location(ps);
    ZrParser_Lexer_Next(ps->lexer);
    return create_identifier_node_with_location(ps, name, identifierLoc);
}

// ==================== 表达式解析（按优先级从低到高）====================

// 解析数组字面量

SZrAstNode *parse_array_literal(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_LBRACKET);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNodeArray *elements = ZrParser_AstNodeArray_New(ps->state, 8);
    if (elements == ZR_NULL) {
        report_error(ps, "Failed to allocate array");
        return ZR_NULL;
    }

    // 解析第一个元素
    if (ps->lexer->t.token != ZR_TK_RBRACKET) {
        // 数组元素不应该包含赋值表达式，使用 conditional_expression
        SZrAstNode *first = parse_conditional_expression(ps);
        if (first != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, elements, first);
        }

        // 解析后续元素
        while (ps->lexer->t.token == ZR_TK_COMMA || ps->lexer->t.token == ZR_TK_SEMICOLON) {
            ZrParser_Lexer_Next(ps->lexer);
            if (ps->lexer->t.token == ZR_TK_RBRACKET) {
                break;
            }
            // 数组元素不应该包含赋值表达式，使用 conditional_expression
            SZrAstNode *elem = parse_conditional_expression(ps);
            if (elem != ZR_NULL) {
                ZrParser_AstNodeArray_Add(ps->state, elements, elem);
            } else {
                break;
            }
        }
    }

    // 可选的尾随分隔符
    if (ps->lexer->t.token == ZR_TK_COMMA || ps->lexer->t.token == ZR_TK_SEMICOLON) {
        ZrParser_Lexer_Next(ps->lexer);
    }

    expect_token(ps, ZR_TK_RBRACKET);
    consume_token(ps, ZR_TK_RBRACKET);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange arrayLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_ARRAY_LITERAL, arrayLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, elements);
        return ZR_NULL;
    }

    node->data.arrayLiteral.elements = elements;
    return node;
}

// 解析对象字面量

SZrAstNode *parse_object_literal(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNodeArray *properties = ZrParser_AstNodeArray_New(ps->state, 8);
    if (properties == ZR_NULL) {
        report_error(ps, "Failed to allocate properties array");
        return ZR_NULL;
    }

    // 解析第一个键值对
    if (ps->lexer->t.token != ZR_TK_RBRACE) {
        // 解析键
        SZrAstNode *key = ZR_NULL;
        if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
            key = parse_identifier(ps);
        } else if (ps->lexer->t.token == ZR_TK_STRING) {
            key = parse_literal(ps);
        } else if (ps->lexer->t.token == ZR_TK_LBRACKET) {
            // 计算键
            ZrParser_Lexer_Next(ps->lexer);
            key = parse_expression(ps);
            expect_token(ps, ZR_TK_RBRACKET);
            consume_token(ps, ZR_TK_RBRACKET);
        } else {
            report_error(ps, "Expected key in object literal");
            ZrParser_AstNodeArray_Free(ps->state, properties);
            return ZR_NULL;
        }

        expect_token(ps, ZR_TK_COLON);
        consume_token(ps, ZR_TK_COLON);

        // 解析值
        SZrAstNode *value = parse_expression(ps);
        if (value == ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, properties);
            return ZR_NULL;
        }

        // 创建键值对节点
        SZrFileRange kvLoc = ZrParser_FileRange_Merge(key->location, value->location);
        SZrAstNode *kvNode = create_ast_node(ps, ZR_AST_KEY_VALUE_PAIR, kvLoc);
        if (kvNode == ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, properties);
            return ZR_NULL;
        }
        kvNode->data.keyValuePair.key = key;
        kvNode->data.keyValuePair.value = value;
        ZrParser_AstNodeArray_Add(ps->state, properties, kvNode);

        // 解析后续键值对
        while (ps->lexer->t.token == ZR_TK_COMMA || ps->lexer->t.token == ZR_TK_SEMICOLON) {
            ZrParser_Lexer_Next(ps->lexer);
            if (ps->lexer->t.token == ZR_TK_RBRACE) {
                break;
            }

            // 解析键
            key = ZR_NULL;
            if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
                key = parse_identifier(ps);
            } else if (ps->lexer->t.token == ZR_TK_STRING) {
                key = parse_literal(ps);
            } else if (ps->lexer->t.token == ZR_TK_LBRACKET) {
                ZrParser_Lexer_Next(ps->lexer);
                key = parse_expression(ps);
                expect_token(ps, ZR_TK_RBRACKET);
                consume_token(ps, ZR_TK_RBRACKET);
            } else {
                break;
            }

            expect_token(ps, ZR_TK_COLON);
            consume_token(ps, ZR_TK_COLON);

            value = parse_expression(ps);
            if (value == ZR_NULL) {
                break;
            }

            kvLoc = ZrParser_FileRange_Merge(key->location, value->location);
            kvNode = create_ast_node(ps, ZR_AST_KEY_VALUE_PAIR, kvLoc);
            if (kvNode == ZR_NULL) {
                break;
            }
            kvNode->data.keyValuePair.key = key;
            kvNode->data.keyValuePair.value = value;
            ZrParser_AstNodeArray_Add(ps->state, properties, kvNode);
        }
    }

    // 可选的尾随分隔符
    if (ps->lexer->t.token == ZR_TK_COMMA || ps->lexer->t.token == ZR_TK_SEMICOLON) {
        ZrParser_Lexer_Next(ps->lexer);
    }

    expect_token(ps, ZR_TK_RBRACE);
    consume_token(ps, ZR_TK_RBRACE);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange objectLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_OBJECT_LITERAL, objectLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, properties);
        return ZR_NULL;
    }

    node->data.objectLiteral.properties = properties;
    return node;
}

// 解析函数调用参数列表
// 解析参数列表，支持命名参数（paramName: value）
// 返回参数值数组，通过 argNames 输出参数名数组（ZR_NULL 表示位置参数）
