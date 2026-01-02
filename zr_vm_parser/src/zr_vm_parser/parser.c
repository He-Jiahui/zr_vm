//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/parser.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

#include <string.h>

// 辅助函数声明
static void expect_token(SZrParserState *ps, EZrToken expected);
static TBool consume_token(SZrParserState *ps, EZrToken token);
static EZrToken peek_token(SZrParserState *ps);
static SZrFileRange get_current_location(SZrParserState *ps);
static void report_error(SZrParserState *ps, const TChar *msg);
static void report_error_with_token(SZrParserState *ps, const TChar *msg, EZrToken token);

// AST 节点创建辅助函数
static SZrAstNode *create_ast_node(SZrParserState *ps, EZrAstNodeType type, SZrFileRange location);
static SZrAstNode *create_identifier_node(SZrParserState *ps, SZrString *name);
static SZrAstNode *create_boolean_literal_node(SZrParserState *ps, TBool value);
static SZrAstNode *create_integer_literal_node(SZrParserState *ps, TInt64 value, SZrString *literal);
static SZrAstNode *create_float_literal_node(SZrParserState *ps, TDouble value, SZrString *literal, TBool isSingle);
static SZrAstNode *create_string_literal_node(SZrParserState *ps, SZrString *value, TBool hasError, SZrString *literal);
static SZrAstNode *create_char_literal_node(SZrParserState *ps, TChar value, TBool hasError, SZrString *literal);
static SZrAstNode *create_null_literal_node(SZrParserState *ps);

// 解析函数声明
static SZrAstNode *parse_script(SZrParserState *ps);
static SZrAstNode *parse_top_level_statement(SZrParserState *ps);
static SZrAstNode *parse_expression(SZrParserState *ps);
static SZrAstNode *parse_statement(SZrParserState *ps);

// 字面量解析
static SZrAstNode *parse_literal(SZrParserState *ps);
static SZrAstNode *parse_identifier(SZrParserState *ps);

// 表达式解析（按优先级从低到高）
static SZrAstNode *parse_assignment_expression(SZrParserState *ps);
static SZrAstNode *parse_conditional_expression(SZrParserState *ps);
static SZrAstNode *parse_logical_or_expression(SZrParserState *ps);
static SZrAstNode *parse_logical_and_expression(SZrParserState *ps);
static SZrAstNode *parse_binary_or_expression(SZrParserState *ps);
static SZrAstNode *parse_binary_xor_expression(SZrParserState *ps);
static SZrAstNode *parse_binary_and_expression(SZrParserState *ps);
static SZrAstNode *parse_equality_expression(SZrParserState *ps);
static SZrAstNode *parse_relational_expression(SZrParserState *ps);
static SZrAstNode *parse_shift_expression(SZrParserState *ps);
static SZrAstNode *parse_additive_expression(SZrParserState *ps);
static SZrAstNode *parse_multiplicative_expression(SZrParserState *ps);
static SZrAstNode *parse_unary_expression(SZrParserState *ps);
static SZrAstNode *parse_primary_expression(SZrParserState *ps);
static SZrAstNode *parse_generator_expression(SZrParserState *ps);

// 声明和类型解析
static SZrAstNode *parse_parameter(SZrParserState *ps);
static SZrAstNodeArray *parse_parameter_list(SZrParserState *ps);
static SZrType *parse_type(SZrParserState *ps);
static SZrType *parse_type_no_generic(SZrParserState *ps);  // 不解析泛型类型的版本
static SZrAstNode *parse_block(SZrParserState *ps);
static SZrAstNode *parse_if_expression(SZrParserState *ps);
static SZrAstNode *parse_switch_expression(SZrParserState *ps);
static SZrAstNode *parse_while_loop(SZrParserState *ps);
static SZrAstNode *parse_for_loop(SZrParserState *ps);
static SZrAstNode *parse_foreach_loop(SZrParserState *ps);
static SZrAstNode *parse_out_statement(SZrParserState *ps);

// 声明解析函数
static SZrAstNode *parse_struct_declaration(SZrParserState *ps);
static SZrAstNode *parse_class_declaration(SZrParserState *ps);
static SZrAstNode *parse_interface_declaration(SZrParserState *ps);
static SZrAstNode *parse_enum_declaration(SZrParserState *ps);
static SZrAstNode *parse_enum_member(SZrParserState *ps);
static SZrAstNode *parse_test_declaration(SZrParserState *ps);
static SZrAstNode *parse_intermediate_statement(SZrParserState *ps);

// 结构体成员解析函数
static SZrAstNode *parse_struct_field(SZrParserState *ps);
static SZrAstNode *parse_struct_method(SZrParserState *ps);
static SZrAstNode *parse_struct_meta_function(SZrParserState *ps);

// 接口成员解析函数
static SZrAstNode *parse_interface_field_declaration(SZrParserState *ps);
static SZrAstNode *parse_interface_method_signature(SZrParserState *ps);
static SZrAstNode *parse_interface_property_signature(SZrParserState *ps);
static SZrAstNode *parse_interface_meta_signature(SZrParserState *ps);

// 类成员解析函数
static SZrAstNode *parse_class_field(SZrParserState *ps);
static SZrAstNode *parse_class_method(SZrParserState *ps);
static SZrAstNode *parse_class_property(SZrParserState *ps);
static SZrAstNode *parse_class_meta_function(SZrParserState *ps);
static SZrAstNode *parse_property_get(SZrParserState *ps);
static SZrAstNode *parse_property_set(SZrParserState *ps);

// 初始化解析器状态
void ZrParserStateInit(SZrParserState *ps, SZrState *state, const TChar *source, TZrSize sourceLength, SZrString *sourceName) {
    ZR_ASSERT(ps != ZR_NULL);
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(source != ZR_NULL);

    ps->state = state;
    ps->hasError = ZR_FALSE;
    ps->errorMessage = ZR_NULL;

    // 初始化词法分析器
    ps->lexer = ZrMemoryRawMallocWithType(state->global, sizeof(SZrLexState), ZR_MEMORY_NATIVE_TYPE_STRING);
    if (ps->lexer == ZR_NULL) {
        ps->hasError = ZR_TRUE;
        ps->errorMessage = "Failed to allocate lexer state";
        return;
    }

    ZrLexerInit(ps->lexer, state, source, sourceLength, sourceName);

    // 初始化当前位置
    SZrFilePosition startPos = ZrFilePositionCreate(0, 1, 1);
    SZrFilePosition endPos = ZrFilePositionCreate(0, 1, 1);
    ps->currentLocation = ZrFileRangeCreate(startPos, endPos, sourceName);
}

// 清理解析器状态
void ZrParserStateFree(SZrParserState *ps) {
    if (ps == ZR_NULL) {
        return;
    }

    if (ps->lexer != ZR_NULL) {
        // 释放词法分析器的缓冲区
        if (ps->lexer->buffer != ZR_NULL) {
            ZrMemoryRawFreeWithType(ps->state->global, ps->lexer->buffer, ps->lexer->bufferSize, ZR_MEMORY_NATIVE_TYPE_STRING);
        }
        ZrMemoryRawFreeWithType(ps->state->global, ps->lexer, sizeof(SZrLexState), ZR_MEMORY_NATIVE_TYPE_STRING);
    }
}

// 期望特定 token
static void expect_token(SZrParserState *ps, EZrToken expected) {
    if (ps->lexer->t.token != expected) {
        const TChar *expectedStr = ZrLexerTokenToString(ps->lexer, expected);
        const TChar *actualStr = ZrLexerTokenToString(ps->lexer, ps->lexer->t.token);
        TChar errorMsg[256];
        snprintf(errorMsg, sizeof(errorMsg), "期望 '%s'，但遇到 '%s'", expectedStr, actualStr);
        report_error_with_token(ps, errorMsg, ps->lexer->t.token);
    }
}

// 消费 token（如果匹配）
static TBool consume_token(SZrParserState *ps, EZrToken token) {
    if (ps->lexer->t.token == token) {
        ZrLexerNext(ps->lexer);
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

// 查看下一个 token（不消费）
static EZrToken peek_token(SZrParserState *ps) {
    return ZrLexerLookahead(ps->lexer);
}

// 获取当前位置信息
static SZrFileRange get_current_location(SZrParserState *ps) {
    // 计算列号（从当前行开始到当前位置的字符数）
    TInt32 column = 1;
    if (ps->lexer->source != ZR_NULL && ps->lexer->currentPos > 0) {
        TZrSize pos = ps->lexer->currentPos - 1;
        while (pos > 0 && ps->lexer->source[pos] != '\n' && ps->lexer->source[pos] != '\r') {
            pos--;
            column++;
        }
    }
    
    SZrFilePosition start = ZrFilePositionCreate(
        ps->lexer->currentPos,
        ps->lexer->lastLine > 0 ? ps->lexer->lastLine : ps->lexer->lineNumber,
        column
    );
    SZrFilePosition end = ZrFilePositionCreate(
        ps->lexer->currentPos,
        ps->lexer->lineNumber,
        column
    );
    return ZrFileRangeCreate(start, end, ps->lexer->sourceName);
}

// 获取当前行的代码片段（前后各20个字符）
static void get_line_snippet(SZrParserState *ps, TChar *buffer, TZrSize bufferSize, TInt32 *errorColumn) {
    if (ps->lexer == ZR_NULL || ps->lexer->source == ZR_NULL || bufferSize == 0) {
        buffer[0] = '\0';
        *errorColumn = 1;
        return;
    }
    
    // 计算列号并找到行首
    TZrSize pos = ps->lexer->currentPos;
    TInt32 column = 1;
    TZrSize lineStart = pos;
    
    // 向前查找行首
    while (lineStart > 0 && ps->lexer->source[lineStart - 1] != '\n') {
        lineStart--;
        column++;
    }
    
    // 向后查找行尾
    TZrSize lineEnd = pos;
    while (lineEnd < ps->lexer->sourceLength && ps->lexer->source[lineEnd] != '\n') {
        lineEnd++;
    }
    
    // 计算要显示的起始和结束位置（前后各20个字符）
    TZrSize snippetStart = lineStart;
    TZrSize snippetEnd = lineEnd;
    TInt32 displayColumn = column;
    
    // 如果列号大于20，向前移动起始位置
    if (column > 20) {
        snippetStart = pos - 20;
        displayColumn = 21;  // 错误位置在显示的第21个字符
    }
    
    // 如果剩余字符少于20，向后扩展
    if (lineEnd - pos < 20) {
        TZrSize needed = 20 - (lineEnd - pos);
        if (snippetStart >= needed) {
            snippetStart -= needed;
            displayColumn += needed;
        } else {
            snippetStart = 0;
            displayColumn = column;
        }
    }
    
    // 确保不越界
    if (snippetStart > lineStart) {
        snippetStart = lineStart;
        displayColumn = column;
    }
    if (snippetEnd > ps->lexer->sourceLength) {
        snippetEnd = ps->lexer->sourceLength;
    }
    
    // 复制代码片段
    TZrSize snippetLen = snippetEnd - snippetStart;
    if (snippetLen >= bufferSize) {
        snippetLen = bufferSize - 1;
    }
    
    for (TZrSize i = 0; i < snippetLen; i++) {
        TChar c = ps->lexer->source[snippetStart + i];
        // 将制表符和换行符替换为空格以便显示
        if (c == '\t') {
            buffer[i] = ' ';
        } else if (c == '\n' || c == '\r') {
            buffer[i] = ' ';
        } else {
            buffer[i] = c;
        }
    }
    buffer[snippetLen] = '\0';
    
    *errorColumn = displayColumn;
}

// 报告解析错误（带 token 信息）
static void report_error_with_token(SZrParserState *ps, const TChar *msg, EZrToken token) {
    ps->hasError = ZR_TRUE;
    ps->errorMessage = msg;
    
    if (ps->lexer != ZR_NULL) {
        // 获取 token 字符串
        const TChar *tokenStr = ZrLexerTokenToString(ps->lexer, token);
        
        // 获取文件名
        const TChar *fileName = "<unknown>";
        if (ps->lexer->sourceName != ZR_NULL) {
            TNativeString nameStr = ZrStringGetNativeString(ps->lexer->sourceName);
            if (nameStr != ZR_NULL) {
                fileName = nameStr;
            }
        }
        
        // 计算列号（从当前行开始到当前位置的字符数）
        TInt32 column = 1;
        if (ps->lexer->source != ZR_NULL && ps->lexer->currentPos > 0) {
            TZrSize pos = ps->lexer->currentPos - 1;
            while (pos > 0 && ps->lexer->source[pos] != '\n') {
                pos--;
                column++;
            }
        }
        
        // 获取代码片段
        TChar snippet[128];
        TInt32 displayColumn = 1;
        get_line_snippet(ps, snippet, sizeof(snippet), &displayColumn);
        
        // 输出详细的错误信息
        // 使用 lastLine 而不是 lineNumber，因为 lastLine 是当前 token 的行号
        TInt32 errorLine = ps->lexer->lastLine > 0 ? ps->lexer->lastLine : ps->lexer->lineNumber;
        printf("  [%s:%d:%d] %s (遇到 token: '%s')\n", 
               fileName, errorLine, column, msg, tokenStr);
        
        // 输出代码片段和错误位置标记
        if (snippet[0] != '\0') {
            printf("    %s\n", snippet);
            // 输出错误位置标记（^）
            for (TInt32 i = 0; i < displayColumn - 1; i++) {
                printf(" ");
            }
            printf("^\n");
        }
    }
}

// 报告解析错误
static void report_error(SZrParserState *ps, const TChar *msg) {
    ps->hasError = ZR_TRUE;
    ps->errorMessage = msg;
    
    // 使用词法分析器的错误报告函数
    if (ps->lexer != ZR_NULL) {
        EZrToken currentToken = ps->lexer->t.token;
        report_error_with_token(ps, msg, currentToken);
    }
}

// ==================== AST 节点创建辅助函数 ====================

static SZrAstNode *create_ast_node(SZrParserState *ps, EZrAstNodeType type, SZrFileRange location) {
    SZrAstNode *node = ZrMemoryRawMallocWithType(ps->state->global, sizeof(SZrAstNode), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (node == ZR_NULL) {
        report_error(ps, "Failed to allocate AST node");
        return ZR_NULL;
    }

    node->type = type;
    node->location = location;
    return node;
}

static SZrAstNode *create_identifier_node(SZrParserState *ps, SZrString *name) {
    SZrFileRange loc = get_current_location(ps);
    SZrAstNode *node = create_ast_node(ps, ZR_AST_IDENTIFIER_LITERAL, loc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.identifier.name = name;
    return node;
}

static SZrAstNode *create_boolean_literal_node(SZrParserState *ps, TBool value) {
    SZrFileRange loc = get_current_location(ps);
    SZrAstNode *node = create_ast_node(ps, ZR_AST_BOOLEAN_LITERAL, loc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.booleanLiteral.value = value;
    return node;
}

static SZrAstNode *create_integer_literal_node(SZrParserState *ps, TInt64 value, SZrString *literal) {
    SZrFileRange loc = get_current_location(ps);
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTEGER_LITERAL, loc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.integerLiteral.value = value;
    node->data.integerLiteral.literal = literal;
    return node;
}

static SZrAstNode *create_float_literal_node(SZrParserState *ps, TDouble value, SZrString *literal, TBool isSingle) {
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

static SZrAstNode *create_string_literal_node(SZrParserState *ps, SZrString *value, TBool hasError, SZrString *literal) {
    SZrFileRange loc = get_current_location(ps);
    SZrAstNode *node = create_ast_node(ps, ZR_AST_STRING_LITERAL, loc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.stringLiteral.value = value;
    node->data.stringLiteral.hasError = hasError;
    node->data.stringLiteral.literal = literal;
    return node;
}

static SZrAstNode *create_char_literal_node(SZrParserState *ps, TChar value, TBool hasError, SZrString *literal) {
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

static SZrAstNode *create_null_literal_node(SZrParserState *ps) {
    SZrFileRange loc = get_current_location(ps);
    return create_ast_node(ps, ZR_AST_NULL_LITERAL, loc);
}

// ==================== 字面量解析 ====================

static SZrAstNode *parse_literal(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    EZrToken token = ps->lexer->t.token;

    switch (token) {
        case ZR_TK_BOOLEAN: {
            TBool value = ps->lexer->t.seminfo.booleanValue;
            ZrLexerNext(ps->lexer);
            return create_boolean_literal_node(ps, value);
        }

        case ZR_TK_INTEGER: {
            TInt64 value = ps->lexer->t.seminfo.intValue;
            SZrString *literal = ps->lexer->t.seminfo.stringValue;  // 注意：这里需要从 token 中获取原始字符串
            ZrLexerNext(ps->lexer);
            return create_integer_literal_node(ps, value, literal);
        }

        case ZR_TK_FLOAT: {
            TDouble value = ps->lexer->t.seminfo.floatValue;
            SZrString *literal = ps->lexer->t.seminfo.stringValue;
            // 判断是否为单精度（需要从原始字符串判断）
            TBool isSingle = ZR_FALSE;  // TODO: 从 literal 判断
            ZrLexerNext(ps->lexer);
            return create_float_literal_node(ps, value, literal, isSingle);
        }

        case ZR_TK_STRING: {
            SZrString *value = ps->lexer->t.seminfo.stringValue;
            TBool hasError = ZR_FALSE;  // TODO: 从词法分析器获取错误信息
            SZrString *literal = value;  // TODO: 获取原始字符串
            ZrLexerNext(ps->lexer);
            return create_string_literal_node(ps, value, hasError, literal);
        }

        case ZR_TK_CHAR: {
            TChar value = ps->lexer->t.seminfo.charValue;
            TBool hasError = ZR_FALSE;  // TODO: 从词法分析器获取错误信息
            SZrString *literal = ZR_NULL;  // TODO: 获取原始字符串
            ZrLexerNext(ps->lexer);
            return create_char_literal_node(ps, value, hasError, literal);
        }

        case ZR_TK_NULL: {
            ZrLexerNext(ps->lexer);
            return create_null_literal_node(ps);
        }

        case ZR_TK_INFINITY: {
            ZrLexerNext(ps->lexer);
            SZrString *literal = ZrStringCreate(ps->state, "Infinity", 8);
            return create_float_literal_node(ps, 1.0 / 0.0, literal, ZR_FALSE);  // 正无穷
        }

        case ZR_TK_NEG_INFINITY: {
            ZrLexerNext(ps->lexer);
            SZrString *literal = ZrStringCreate(ps->state, "NegativeInfinity", 16);
            return create_float_literal_node(ps, -1.0 / 0.0, literal, ZR_FALSE);  // 负无穷
        }

        case ZR_TK_NAN: {
            ZrLexerNext(ps->lexer);
            SZrString *literal = ZrStringCreate(ps->state, "NaN", 3);
            return create_float_literal_node(ps, 0.0 / 0.0, literal, ZR_FALSE);  // NaN
        }

        default:
            report_error(ps, "Expected literal");
            return ZR_NULL;
    }
}

static SZrAstNode *parse_identifier(SZrParserState *ps) {
    // 允许 test 关键字作为标识符（用于方法名等）
    if (ps->lexer->t.token != ZR_TK_IDENTIFIER && ps->lexer->t.token != ZR_TK_TEST) {
        report_error(ps, "Expected identifier");
        return ZR_NULL;
    }

    SZrString *name = ps->lexer->t.seminfo.stringValue;
    ZrLexerNext(ps->lexer);
    return create_identifier_node(ps, name);
}

// ==================== 表达式解析（按优先级从低到高）====================

// 解析数组字面量
static SZrAstNode *parse_array_literal(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_LBRACKET);
    ZrLexerNext(ps->lexer);

    SZrAstNodeArray *elements = ZrAstNodeArrayNew(ps->state, 8);
    if (elements == ZR_NULL) {
        report_error(ps, "Failed to allocate array");
        return ZR_NULL;
    }

    // 解析第一个元素
    if (ps->lexer->t.token != ZR_TK_RBRACKET) {
        // 数组元素不应该包含赋值表达式，使用 conditional_expression
        SZrAstNode *first = parse_conditional_expression(ps);
        if (first != ZR_NULL) {
            ZrAstNodeArrayAdd(ps->state, elements, first);
        }

        // 解析后续元素
        while (ps->lexer->t.token == ZR_TK_COMMA || ps->lexer->t.token == ZR_TK_SEMICOLON) {
            ZrLexerNext(ps->lexer);
            if (ps->lexer->t.token == ZR_TK_RBRACKET) {
                break;
            }
            // 数组元素不应该包含赋值表达式，使用 conditional_expression
            SZrAstNode *elem = parse_conditional_expression(ps);
            if (elem != ZR_NULL) {
                ZrAstNodeArrayAdd(ps->state, elements, elem);
            } else {
                break;
            }
        }
    }

    // 可选的尾随分隔符
    if (ps->lexer->t.token == ZR_TK_COMMA || ps->lexer->t.token == ZR_TK_SEMICOLON) {
        ZrLexerNext(ps->lexer);
    }

    expect_token(ps, ZR_TK_RBRACKET);
    consume_token(ps, ZR_TK_RBRACKET);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange arrayLoc = ZrFileRangeMerge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_ARRAY_LITERAL, arrayLoc);
    if (node == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, elements);
        return ZR_NULL;
    }

    node->data.arrayLiteral.elements = elements;
    return node;
}

// 解析对象字面量
static SZrAstNode *parse_object_literal(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_LBRACE);
    ZrLexerNext(ps->lexer);

    SZrAstNodeArray *properties = ZrAstNodeArrayNew(ps->state, 8);
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
            ZrLexerNext(ps->lexer);
            key = parse_expression(ps);
            expect_token(ps, ZR_TK_RBRACKET);
            consume_token(ps, ZR_TK_RBRACKET);
        } else {
            report_error(ps, "Expected key in object literal");
            ZrAstNodeArrayFree(ps->state, properties);
            return ZR_NULL;
        }

        expect_token(ps, ZR_TK_COLON);
        consume_token(ps, ZR_TK_COLON);

        // 解析值
        SZrAstNode *value = parse_expression(ps);
        if (value == ZR_NULL) {
            ZrAstNodeArrayFree(ps->state, properties);
            return ZR_NULL;
        }

        // 创建键值对节点
        SZrFileRange kvLoc = ZrFileRangeMerge(key->location, value->location);
        SZrAstNode *kvNode = create_ast_node(ps, ZR_AST_KEY_VALUE_PAIR, kvLoc);
        if (kvNode == ZR_NULL) {
            ZrAstNodeArrayFree(ps->state, properties);
            return ZR_NULL;
        }
        kvNode->data.keyValuePair.key = key;
        kvNode->data.keyValuePair.value = value;
        ZrAstNodeArrayAdd(ps->state, properties, kvNode);

        // 解析后续键值对
        while (ps->lexer->t.token == ZR_TK_COMMA || ps->lexer->t.token == ZR_TK_SEMICOLON) {
            ZrLexerNext(ps->lexer);
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
                ZrLexerNext(ps->lexer);
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

            kvLoc = ZrFileRangeMerge(key->location, value->location);
            kvNode = create_ast_node(ps, ZR_AST_KEY_VALUE_PAIR, kvLoc);
            if (kvNode == ZR_NULL) {
                break;
            }
            kvNode->data.keyValuePair.key = key;
            kvNode->data.keyValuePair.value = value;
            ZrAstNodeArrayAdd(ps->state, properties, kvNode);
        }
    }

    // 可选的尾随分隔符
    if (ps->lexer->t.token == ZR_TK_COMMA || ps->lexer->t.token == ZR_TK_SEMICOLON) {
        ZrLexerNext(ps->lexer);
    }

    expect_token(ps, ZR_TK_RBRACE);
    consume_token(ps, ZR_TK_RBRACE);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange objectLoc = ZrFileRangeMerge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_OBJECT_LITERAL, objectLoc);
    if (node == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, properties);
        return ZR_NULL;
    }

    node->data.objectLiteral.properties = properties;
    return node;
}

// 解析函数调用参数列表
static SZrAstNodeArray *parse_argument_list(SZrParserState *ps) {
    SZrAstNodeArray *args = ZrAstNodeArrayNew(ps->state, 4);
    if (args == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        SZrAstNode *first = parse_expression(ps);
        if (first != ZR_NULL) {
            ZrAstNodeArrayAdd(ps->state, args, first);
        }

        while (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_RPAREN) {
                break;
            }
            SZrAstNode *arg = parse_expression(ps);
            if (arg != ZR_NULL) {
                ZrAstNodeArrayAdd(ps->state, args, arg);
            } else {
                break;
            }
        }
    }

    return args;
}

// 解析成员访问和函数调用
static SZrAstNode *parse_member_access(SZrParserState *ps, SZrAstNode *base) {
    SZrFileRange startLoc = base->location;

    while (ZR_TRUE) {
        // 点号成员访问
        if (consume_token(ps, ZR_TK_DOT)) {
            // 期望标识符
            if (ps->lexer->t.token != ZR_TK_IDENTIFIER) {
                const TChar *tokenStr = ZrLexerTokenToString(ps->lexer, ps->lexer->t.token);
                TChar errorMsg[256];
                snprintf(errorMsg, sizeof(errorMsg), "Expected identifier after '.' (遇到 '%s')", tokenStr);
                report_error_with_token(ps, errorMsg, ps->lexer->t.token);
                return base;
            }
            
            SZrAstNode *property = parse_identifier(ps);
            if (property == ZR_NULL) {
                // parse_identifier 已经报告了错误
                return base;
            }

            SZrAstNode *memberNode = create_ast_node(ps, ZR_AST_MEMBER_EXPRESSION, startLoc);
            if (memberNode == ZR_NULL) {
                return base;
            }
            memberNode->data.memberExpression.property = property;
            memberNode->data.memberExpression.computed = ZR_FALSE;

            // 创建主表达式包装
            SZrAstNode *primaryNode = create_ast_node(ps, ZR_AST_PRIMARY_EXPRESSION, startLoc);
            if (primaryNode == ZR_NULL) {
                return base;
            }
            primaryNode->data.primaryExpression.property = base;
            SZrAstNodeArray *members = ZrAstNodeArrayNew(ps->state, 1);
            if (members == ZR_NULL) {
                return base;
            }
            ZrAstNodeArrayAdd(ps->state, members, memberNode);
            primaryNode->data.primaryExpression.members = members;
            base = primaryNode;
        }
        // 方括号成员访问
        else if (consume_token(ps, ZR_TK_LBRACKET)) {
            SZrAstNode *property = parse_expression(ps);
            if (property == ZR_NULL) {
                return base;
            }
            expect_token(ps, ZR_TK_RBRACKET);
            consume_token(ps, ZR_TK_RBRACKET);

            SZrAstNode *memberNode = create_ast_node(ps, ZR_AST_MEMBER_EXPRESSION, startLoc);
            if (memberNode == ZR_NULL) {
                return base;
            }
            memberNode->data.memberExpression.property = property;
            memberNode->data.memberExpression.computed = ZR_TRUE;

            // 创建主表达式包装
            SZrAstNode *primaryNode = create_ast_node(ps, ZR_AST_PRIMARY_EXPRESSION, startLoc);
            if (primaryNode == ZR_NULL) {
                return base;
            }
            primaryNode->data.primaryExpression.property = base;
            SZrAstNodeArray *members = ZrAstNodeArrayNew(ps->state, 1);
            if (members == ZR_NULL) {
                return base;
            }
            ZrAstNodeArrayAdd(ps->state, members, memberNode);
            primaryNode->data.primaryExpression.members = members;
            base = primaryNode;
        }
        // 函数调用
        else if (consume_token(ps, ZR_TK_LPAREN)) {
            SZrAstNodeArray *args = parse_argument_list(ps);
            expect_token(ps, ZR_TK_RPAREN);
            consume_token(ps, ZR_TK_RPAREN);

            SZrAstNode *callNode = create_ast_node(ps, ZR_AST_FUNCTION_CALL, startLoc);
            if (callNode == ZR_NULL) {
                if (args != ZR_NULL) {
                    ZrAstNodeArrayFree(ps->state, args);
                }
                return base;
            }
            callNode->data.functionCall.args = args;

            // 创建主表达式包装
            SZrAstNode *primaryNode = create_ast_node(ps, ZR_AST_PRIMARY_EXPRESSION, startLoc);
            if (primaryNode == ZR_NULL) {
                return base;
            }
            primaryNode->data.primaryExpression.property = base;
            SZrAstNodeArray *members = ZrAstNodeArrayNew(ps->state, 1);
            if (members == ZR_NULL) {
                return base;
            }
            ZrAstNodeArrayAdd(ps->state, members, callNode);
            primaryNode->data.primaryExpression.members = members;
            base = primaryNode;
        }
        else {
            break;
        }
    }

    return base;
}

// 解析主表达式
static SZrAstNode *parse_primary_expression(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    EZrToken token = ps->lexer->t.token;
    SZrAstNode *base = ZR_NULL;

    // 字面量
    if (token == ZR_TK_BOOLEAN || token == ZR_TK_INTEGER || token == ZR_TK_FLOAT ||
        token == ZR_TK_STRING || token == ZR_TK_CHAR || token == ZR_TK_NULL ||
        token == ZR_TK_INFINITY || token == ZR_TK_NEG_INFINITY || token == ZR_TK_NAN) {
        base = parse_literal(ps);
    }
    // 标识符
    else if (token == ZR_TK_IDENTIFIER) {
        base = parse_identifier(ps);
        // 标识符解析后，需要继续解析可能的成员访问和函数调用
        // 这将在函数末尾统一处理
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
            // 是生成器表达式 {{}}
            base = parse_generator_expression(ps);
        } else {
            // 检查是否是对象字面量（有键值对）还是块表达式
            // 简化处理：先尝试解析为对象字面量
            // TODO: 更精确的判断逻辑
            base = parse_object_literal(ps);
        }
    }
    // Lambda 表达式或括号表达式
    else if (consume_token(ps, ZR_TK_LPAREN)) {
        // 检查是否是 lambda 表达式（参数列表后跟 =>）
        EZrToken lookahead = peek_token(ps);
        if (lookahead == ZR_TK_RIGHT_ARROW || lookahead == ZR_TK_PARAMS || 
            ps->lexer->t.token == ZR_TK_IDENTIFIER || ps->lexer->t.token == ZR_TK_PARAMS) {
            // 可能是 lambda，尝试解析
            SZrFileRange lambdaLoc = get_current_location(ps);
            SZrAstNodeArray *params = ZR_NULL;
            SZrParameter *args = ZR_NULL;

            if (ps->lexer->t.token == ZR_TK_PARAMS) {
                // 只有可变参数
                ZrLexerNext(ps->lexer);
                SZrAstNode *argsNode = parse_parameter(ps);
                if (argsNode != ZR_NULL) {
                    args = &argsNode->data.parameter;
                }
                params = ZrAstNodeArrayNew(ps->state, 0);
            } else {
                params = parse_parameter_list(ps);
                if (consume_token(ps, ZR_TK_COMMA)) {
                    if (ps->lexer->t.token == ZR_TK_PARAMS) {
                        ZrLexerNext(ps->lexer);
                        SZrAstNode *argsNode = parse_parameter(ps);
                        if (argsNode != ZR_NULL) {
                            args = &argsNode->data.parameter;
                        }
                    }
                }
            }

            if (consume_token(ps, ZR_TK_RPAREN) && consume_token(ps, ZR_TK_RIGHT_ARROW)) {
                SZrAstNode *block = parse_block(ps);
                if (block != ZR_NULL) {
                    SZrFileRange endLoc = get_current_location(ps);
                    SZrFileRange fullLoc = ZrFileRangeMerge(lambdaLoc, endLoc);
                    SZrAstNode *lambdaNode = create_ast_node(ps, ZR_AST_LAMBDA_EXPRESSION, fullLoc);
                    if (lambdaNode != ZR_NULL) {
                        lambdaNode->data.lambdaExpression.params = params;
                        lambdaNode->data.lambdaExpression.args = args;
                        lambdaNode->data.lambdaExpression.block = block;
                        return parse_member_access(ps, lambdaNode);
                    }
                }
            }
            // 如果不是 lambda，回退并解析为普通括号表达式
            // 这里需要更复杂的回退机制，暂时简化处理
        }
        // 普通括号表达式
        base = parse_expression(ps);
        expect_token(ps, ZR_TK_RPAREN);
        consume_token(ps, ZR_TK_RPAREN);
    }
    // If 表达式（作为表达式使用）
    else if (token == ZR_TK_IF) {
        base = parse_if_expression(ps);
        if (base != ZR_NULL) {
            base->data.ifExpression.isStatement = ZR_FALSE;
        }
    }
    // While 循环表达式（作为表达式使用）
    else if (token == ZR_TK_WHILE) {
        base = parse_while_loop(ps);
        if (base != ZR_NULL) {
            base->data.whileLoop.isStatement = ZR_FALSE;
        }
    }
    // For 循环表达式（作为表达式使用）
    else if (token == ZR_TK_FOR) {
        // 检查是否是 foreach (var x in ...) 还是 for (init; cond; step)
        // 保存状态以便向前看
        TZrSize savedPos = ps->lexer->currentPos;
        TInt32 savedChar = ps->lexer->currentChar;
        TInt32 savedLine = ps->lexer->lineNumber;
        TInt32 savedLastLine = ps->lexer->lastLine;
        SZrToken savedToken = ps->lexer->t;
        SZrToken savedLookahead = ps->lexer->lookahead;
        TZrSize savedLookaheadPos = ps->lexer->lookaheadPos;
        TInt32 savedLookaheadChar = ps->lexer->lookaheadChar;
        TInt32 savedLookaheadLine = ps->lexer->lookaheadLine;
        TInt32 savedLookaheadLastLine = ps->lexer->lookaheadLastLine;
        
        // 跳过 for 和 (
        ZrLexerNext(ps->lexer);
        if (ps->lexer->t.token == ZR_TK_LPAREN) {
            ZrLexerNext(ps->lexer);
        }
        
        // 检查是否是 foreach (var x in ...)
        if (ps->lexer->t.token == ZR_TK_VAR) {
            // 恢复状态并解析 foreach
            ps->lexer->currentPos = savedPos;
            ps->lexer->currentChar = savedChar;
            ps->lexer->lineNumber = savedLine;
            ps->lexer->lastLine = savedLastLine;
            ps->lexer->t = savedToken;
            ps->lexer->lookahead = savedLookahead;
            ps->lexer->lookaheadPos = savedLookaheadPos;
            ps->lexer->lookaheadChar = savedLookaheadChar;
            ps->lexer->lookaheadLine = savedLookaheadLine;
            ps->lexer->lookaheadLastLine = savedLookaheadLastLine;
            base = parse_foreach_loop(ps);
        } else {
            // 恢复状态并解析 for
            ps->lexer->currentPos = savedPos;
            ps->lexer->currentChar = savedChar;
            ps->lexer->lineNumber = savedLine;
            ps->lexer->lastLine = savedLastLine;
            ps->lexer->t = savedToken;
            ps->lexer->lookahead = savedLookahead;
            ps->lexer->lookaheadPos = savedLookaheadPos;
            ps->lexer->lookaheadChar = savedLookaheadChar;
            ps->lexer->lookaheadLine = savedLookaheadLine;
            ps->lexer->lookaheadLastLine = savedLookaheadLastLine;
            base = parse_for_loop(ps);
        }
        
        if (base != ZR_NULL) {
            if (base->type == ZR_AST_FOREACH_LOOP) {
                base->data.foreachLoop.isStatement = ZR_FALSE;
            } else if (base->type == ZR_AST_FOR_LOOP) {
                base->data.forLoop.isStatement = ZR_FALSE;
            }
        }
    }
    // Switch 表达式（作为表达式使用）
    else if (token == ZR_TK_SWITCH) {
        base = parse_switch_expression(ps);
    }
    else {
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
static SZrAstNode *parse_unary_expression(SZrParserState *ps) {
    EZrToken token = ps->lexer->t.token;

    // 检查一元操作符
    if (token == ZR_TK_BANG || token == ZR_TK_TILDE || token == ZR_TK_PLUS ||
        token == ZR_TK_MINUS || token == ZR_TK_DOLLAR || token == ZR_TK_NEW) {
        SZrFileRange startLoc = get_current_location(ps);
        SZrUnaryOperator op;
        op.op = ZrLexerTokenToString(ps->lexer, token);

        ZrLexerNext(ps->lexer);
        SZrAstNode *argument = parse_unary_expression(ps);  // 右结合

        SZrAstNode *node = create_ast_node(ps, ZR_AST_UNARY_EXPRESSION, startLoc);
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.unaryExpression.op = op;
        node->data.unaryExpression.argument = argument;
        return node;
    }

    return parse_primary_expression(ps);
}

// 解析乘法表达式
static SZrAstNode *parse_multiplicative_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_unary_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_STAR || ps->lexer->t.token == ZR_TK_SLASH ||
           ps->lexer->t.token == ZR_TK_PERCENT) {
        SZrFileRange startLoc = get_current_location(ps);
        SZrBinaryOperator op;
        op.op = ZrLexerTokenToString(ps->lexer, ps->lexer->t.token);

        ZrLexerNext(ps->lexer);
        SZrAstNode *right = parse_unary_expression(ps);

        SZrAstNode *node = create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, startLoc);
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.binaryExpression.left = left;
        node->data.binaryExpression.right = right;
        node->data.binaryExpression.op = op;
        left = node;
    }

    return left;
}

// 解析加法表达式
static SZrAstNode *parse_additive_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_multiplicative_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_PLUS || ps->lexer->t.token == ZR_TK_MINUS) {
        SZrFileRange startLoc = get_current_location(ps);
        SZrBinaryOperator op;
        op.op = ZrLexerTokenToString(ps->lexer, ps->lexer->t.token);

        ZrLexerNext(ps->lexer);
        SZrAstNode *right = parse_multiplicative_expression(ps);

        SZrAstNode *node = create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, startLoc);
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.binaryExpression.left = left;
        node->data.binaryExpression.right = right;
        node->data.binaryExpression.op = op;
        left = node;
    }

    return left;
}

// 解析位移表达式
static SZrAstNode *parse_shift_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_additive_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_LEFT_SHIFT || ps->lexer->t.token == ZR_TK_RIGHT_SHIFT) {
        SZrFileRange startLoc = get_current_location(ps);
        SZrBinaryOperator op;
        op.op = ZrLexerTokenToString(ps->lexer, ps->lexer->t.token);

        ZrLexerNext(ps->lexer);
        SZrAstNode *right = parse_additive_expression(ps);

        SZrAstNode *node = create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, startLoc);
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.binaryExpression.left = left;
        node->data.binaryExpression.right = right;
        node->data.binaryExpression.op = op;
        left = node;
    }

    return left;
}

// 解析关系表达式
static SZrAstNode *parse_relational_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_shift_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_LESS_THAN || ps->lexer->t.token == ZR_TK_GREATER_THAN ||
           ps->lexer->t.token == ZR_TK_LESS_THAN_EQUALS || ps->lexer->t.token == ZR_TK_GREATER_THAN_EQUALS) {
        SZrFileRange startLoc = get_current_location(ps);
        SZrBinaryOperator op;
        op.op = ZrLexerTokenToString(ps->lexer, ps->lexer->t.token);

        ZrLexerNext(ps->lexer);
        SZrAstNode *right = parse_shift_expression(ps);

        SZrAstNode *node = create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, startLoc);
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.binaryExpression.left = left;
        node->data.binaryExpression.right = right;
        node->data.binaryExpression.op = op;
        left = node;
    }

    return left;
}

// 解析相等表达式
static SZrAstNode *parse_equality_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_relational_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_DOUBLE_EQUALS || ps->lexer->t.token == ZR_TK_BANG_EQUALS) {
        SZrFileRange startLoc = get_current_location(ps);
        SZrBinaryOperator op;
        op.op = ZrLexerTokenToString(ps->lexer, ps->lexer->t.token);

        ZrLexerNext(ps->lexer);
        SZrAstNode *right = parse_relational_expression(ps);

        SZrAstNode *node = create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, startLoc);
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.binaryExpression.left = left;
        node->data.binaryExpression.right = right;
        node->data.binaryExpression.op = op;
        left = node;
    }

    return left;
}

// 解析按位与表达式
static SZrAstNode *parse_binary_and_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_equality_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_AND) {
        SZrFileRange startLoc = get_current_location(ps);
        SZrBinaryOperator op;
        op.op = ZrLexerTokenToString(ps->lexer, ps->lexer->t.token);

        ZrLexerNext(ps->lexer);
        SZrAstNode *right = parse_equality_expression(ps);

        SZrAstNode *node = create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, startLoc);
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.binaryExpression.left = left;
        node->data.binaryExpression.right = right;
        node->data.binaryExpression.op = op;
        left = node;
    }

    return left;
}

// 解析按位异或表达式
static SZrAstNode *parse_binary_xor_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_binary_and_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_XOR) {
        SZrFileRange startLoc = get_current_location(ps);
        SZrBinaryOperator op;
        op.op = ZrLexerTokenToString(ps->lexer, ps->lexer->t.token);

        ZrLexerNext(ps->lexer);
        SZrAstNode *right = parse_binary_and_expression(ps);

        SZrAstNode *node = create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, startLoc);
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.binaryExpression.left = left;
        node->data.binaryExpression.right = right;
        node->data.binaryExpression.op = op;
        left = node;
    }

    return left;
}

// 解析按位或表达式
static SZrAstNode *parse_binary_or_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_binary_xor_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_OR) {
        SZrFileRange startLoc = get_current_location(ps);
        SZrBinaryOperator op;
        op.op = ZrLexerTokenToString(ps->lexer, ps->lexer->t.token);

        ZrLexerNext(ps->lexer);
        SZrAstNode *right = parse_binary_xor_expression(ps);

        SZrAstNode *node = create_ast_node(ps, ZR_AST_BINARY_EXPRESSION, startLoc);
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.binaryExpression.left = left;
        node->data.binaryExpression.right = right;
        node->data.binaryExpression.op = op;
        left = node;
    }

    return left;
}

// 解析逻辑与表达式
static SZrAstNode *parse_logical_and_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_binary_or_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_AMPERSAND_AMPERSAND) {
        SZrFileRange startLoc = get_current_location(ps);

        ZrLexerNext(ps->lexer);
        SZrAstNode *right = parse_binary_or_expression(ps);

        SZrAstNode *node = create_ast_node(ps, ZR_AST_LOGICAL_EXPRESSION, startLoc);
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.logicalExpression.left = left;
        node->data.logicalExpression.right = right;
        node->data.logicalExpression.op = "&&";
        left = node;
    }

    return left;
}

// 解析逻辑或表达式
static SZrAstNode *parse_logical_or_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_logical_and_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_PIPE_PIPE) {
        SZrFileRange startLoc = get_current_location(ps);

        ZrLexerNext(ps->lexer);
        SZrAstNode *right = parse_logical_and_expression(ps);

        SZrAstNode *node = create_ast_node(ps, ZR_AST_LOGICAL_EXPRESSION, startLoc);
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.logicalExpression.left = left;
        node->data.logicalExpression.right = right;
        node->data.logicalExpression.op = "||";
        left = node;
    }

    return left;
}

// 解析条件表达式（三元运算符）
static SZrAstNode *parse_conditional_expression(SZrParserState *ps) {
    SZrAstNode *test = parse_logical_or_expression(ps);
    if (test == ZR_NULL) {
        return ZR_NULL;
    }

    if (consume_token(ps, ZR_TK_QUESTIONMARK)) {
        SZrFileRange startLoc = get_current_location(ps);
        SZrAstNode *consequent = parse_expression(ps);
        expect_token(ps, ZR_TK_COLON);
        consume_token(ps, ZR_TK_COLON);
        SZrAstNode *alternate = parse_conditional_expression(ps);  // 右结合

        SZrAstNode *node = create_ast_node(ps, ZR_AST_CONDITIONAL_EXPRESSION, startLoc);
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.conditionalExpression.test = test;
        node->data.conditionalExpression.consequent = consequent;
        node->data.conditionalExpression.alternate = alternate;
        return node;
    }

    return test;
}

// 解析赋值表达式
static SZrAstNode *parse_assignment_expression(SZrParserState *ps) {
    SZrAstNode *left = parse_conditional_expression(ps);
    if (left == ZR_NULL) {
        return ZR_NULL;
    }

    // 检查赋值操作符
    EZrToken token = ps->lexer->t.token;
    if (token == ZR_TK_EQUALS || token == ZR_TK_PLUS_EQUALS || token == ZR_TK_MINUS_EQUALS ||
        token == ZR_TK_STAR_EQUALS || token == ZR_TK_SLASH_EQUALS || token == ZR_TK_PERCENT_EQUALS) {
        SZrFileRange startLoc = get_current_location(ps);
        SZrAssignmentOperator op;
        op.op = ZrLexerTokenToString(ps->lexer, token);

        ZrLexerNext(ps->lexer);
        SZrAstNode *right = parse_assignment_expression(ps);  // 右结合

        SZrAstNode *node = create_ast_node(ps, ZR_AST_ASSIGNMENT_EXPRESSION, startLoc);
        if (node == ZR_NULL) {
            return ZR_NULL;
        }

        node->data.assignmentExpression.left = left;
        node->data.assignmentExpression.right = right;
        node->data.assignmentExpression.op = op;
        return node;
    }

    return left;
}

// 解析表达式（入口函数）
static SZrAstNode *parse_expression(SZrParserState *ps) {
    return parse_assignment_expression(ps);
}

// ==================== 辅助解析函数 ====================

// 解析类型列表
static SZrAstNodeArray *parse_type_list(SZrParserState *ps);

// 解析类型
static SZrType *parse_type(SZrParserState *ps);

// 解析泛型类型
static SZrAstNode *parse_generic_type(SZrParserState *ps);

// 解析元组类型
static SZrAstNode *parse_tuple_type(SZrParserState *ps);

// 解析泛型声明
static SZrGenericDeclaration *parse_generic_declaration(SZrParserState *ps);

// 解析元标识符
static SZrAstNode *parse_meta_identifier(SZrParserState *ps);

// 解析装饰器表达式
static SZrAstNode *parse_decorator_expression(SZrParserState *ps);

// 解析解构对象模式
static SZrAstNode *parse_destructuring_object(SZrParserState *ps);

// 解析解构数组模式
static SZrAstNode *parse_destructuring_array(SZrParserState *ps);

// ==================== 类型解析函数实现 ====================

// 解析类型列表
static SZrAstNodeArray *parse_type_list(SZrParserState *ps) {
    SZrAstNodeArray *types = ZrAstNodeArrayNew(ps->state, 4);
    if (types == ZR_NULL) {
        return ZR_NULL;
    }

    SZrType *first = parse_type(ps);
    if (first != ZR_NULL) {
        SZrAstNode *firstNode = create_ast_node(ps, ZR_AST_TYPE, get_current_location(ps));
        if (firstNode != ZR_NULL) {
            firstNode->data.type = *first;
            ZrMemoryRawFreeWithType(ps->state->global, first, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            ZrAstNodeArrayAdd(ps->state, types, firstNode);
        }
    }

    while (consume_token(ps, ZR_TK_COMMA)) {
        SZrType *type = parse_type(ps);
        if (type != ZR_NULL) {
            SZrAstNode *typeNode = create_ast_node(ps, ZR_AST_TYPE, get_current_location(ps));
            if (typeNode != ZR_NULL) {
                typeNode->data.type = *type;
                ZrMemoryRawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                ZrAstNodeArrayAdd(ps->state, types, typeNode);
            }
        } else {
            break;
        }
    }

    return types;
}

// 解析泛型类型
static SZrAstNode *parse_generic_type(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_LESS_THAN);
    ZrLexerNext(ps->lexer);

    SZrAstNodeArray *params = parse_type_list(ps);
    if (params == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_GREATER_THAN);
    ZrLexerNext(ps->lexer);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange genericLoc = ZrFileRangeMerge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_GENERIC_TYPE, genericLoc);
    if (node == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, params);
        return ZR_NULL;
    }

    node->data.genericType.name = &nameNode->data.identifier;
    node->data.genericType.params = params;
    return node;
}

// 解析元组类型
static SZrAstNode *parse_tuple_type(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_LBRACKET);
    ZrLexerNext(ps->lexer);

    SZrAstNodeArray *elements = parse_type_list(ps);
    if (elements == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_RBRACKET);
    ZrLexerNext(ps->lexer);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange tupleLoc = ZrFileRangeMerge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_TUPLE_TYPE, tupleLoc);
    if (node == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, elements);
        return ZR_NULL;
    }

    node->data.tupleType.elements = elements;
    return node;
}

// 解析类型
static SZrType *parse_type(SZrParserState *ps) {
    SZrType *type = ZrMemoryRawMallocWithType(ps->state->global, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (type == ZR_NULL) {
        return ZR_NULL;
    }

    type->dimensions = 0;
    type->name = ZR_NULL;
    type->subType = ZR_NULL;

    // 解析类型名称（可能是标识符、泛型类型或元组类型）
    if (ps->lexer->t.token == ZR_TK_LBRACKET) {
        // 元组类型
        SZrAstNode *tupleNode = parse_tuple_type(ps);
        if (tupleNode != ZR_NULL) {
            type->name = tupleNode;
        } else {
            ZrMemoryRawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            return ZR_NULL;
        }
    } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
        // 检查是否是泛型类型
        EZrToken lookahead = peek_token(ps);
        if (lookahead == ZR_TK_LESS_THAN) {
            SZrAstNode *genericNode = parse_generic_type(ps);
            if (genericNode != ZR_NULL) {
                type->name = genericNode;
            } else {
                ZrMemoryRawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                return ZR_NULL;
            }
        } else {
            // 普通标识符类型
            SZrAstNode *idNode = parse_identifier(ps);
            if (idNode != ZR_NULL) {
                type->name = idNode;
            } else {
                ZrMemoryRawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                return ZR_NULL;
            }
        }
    } else {
        report_error(ps, "Expected type name");
        ZrMemoryRawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return ZR_NULL;
    }

    // 解析子类型（可选，如 A.B）
    if (consume_token(ps, ZR_TK_DOT)) {
        type->subType = parse_type(ps);
    }

    // 解析数组维度
    while (consume_token(ps, ZR_TK_LBRACKET)) {
        if (consume_token(ps, ZR_TK_RBRACKET)) {
            type->dimensions++;
        } else {
            report_error(ps, "Expected ] for array dimension");
            break;
        }
    }

    return type;
}

// 解析类型（不解析泛型类型，用于 intermediate 声明的返回类型）
static SZrType *parse_type_no_generic(SZrParserState *ps) {
    SZrType *type = ZrMemoryRawMallocWithType(ps->state->global, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (type == ZR_NULL) {
        return ZR_NULL;
    }

    type->dimensions = 0;
    type->name = ZR_NULL;
    type->subType = ZR_NULL;

    // 解析类型名称（可能是标识符或元组类型，但不解析泛型类型）
    if (ps->lexer->t.token == ZR_TK_LBRACKET) {
        // 元组类型
        SZrAstNode *tupleNode = parse_tuple_type(ps);
        if (tupleNode != ZR_NULL) {
            type->name = tupleNode;
        } else {
            ZrMemoryRawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            return ZR_NULL;
        }
    } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
        // 普通标识符类型（不解析泛型类型，即使后面有 <）
        SZrAstNode *idNode = parse_identifier(ps);
        if (idNode != ZR_NULL) {
            type->name = idNode;
        } else {
            ZrMemoryRawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            return ZR_NULL;
        }
    } else {
        report_error(ps, "Expected type name");
        ZrMemoryRawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return ZR_NULL;
    }

    // 解析子类型（可选，如 A.B）
    if (consume_token(ps, ZR_TK_DOT)) {
        type->subType = parse_type_no_generic(ps);
    }

    // 解析数组维度
    while (consume_token(ps, ZR_TK_LBRACKET)) {
        if (consume_token(ps, ZR_TK_RBRACKET)) {
            type->dimensions++;
        } else {
            report_error(ps, "Expected ] for array dimension");
            break;
        }
    }

    return type;
}

// 解析泛型声明
static SZrGenericDeclaration *parse_generic_declaration(SZrParserState *ps) {
    expect_token(ps, ZR_TK_LESS_THAN);
    ZrLexerNext(ps->lexer);

    SZrAstNodeArray *params = ZrAstNodeArrayNew(ps->state, 4);
    if (params == ZR_NULL) {
        return ZR_NULL;
    }

    // 至少需要一个参数
    SZrAstNode *first = parse_parameter(ps);
    if (first != ZR_NULL) {
        ZrAstNodeArrayAdd(ps->state, params, first);
    }

    while (consume_token(ps, ZR_TK_COMMA)) {
        if (ps->lexer->t.token == ZR_TK_GREATER_THAN) {
            break;
        }
        SZrAstNode *param = parse_parameter(ps);
        if (param != ZR_NULL) {
            ZrAstNodeArrayAdd(ps->state, params, param);
        } else {
            break;
        }
    }

    expect_token(ps, ZR_TK_GREATER_THAN);
    ZrLexerNext(ps->lexer);

    SZrGenericDeclaration *generic = ZrMemoryRawMallocWithType(ps->state->global, sizeof(SZrGenericDeclaration), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (generic == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, params);
        return ZR_NULL;
    }

    generic->params = params;
    return generic;
}

// 解析元标识符
static SZrAstNode *parse_meta_identifier(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_AT);
    ZrLexerNext(ps->lexer);

    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange metaLoc = ZrFileRangeMerge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_META_IDENTIFIER, metaLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.metaIdentifier.name = &nameNode->data.identifier;
    return node;
}

// 解析装饰器表达式
static SZrAstNode *parse_decorator_expression(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_SHARP);
    ZrLexerNext(ps->lexer);

    SZrAstNode *expr = parse_expression(ps);
    if (expr == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_SHARP);
    ZrLexerNext(ps->lexer);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange decoratorLoc = ZrFileRangeMerge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_DECORATOR_EXPRESSION, decoratorLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.decoratorExpression.expr = expr;
    return node;
}

// 解析解构对象模式
static SZrAstNode *parse_destructuring_object(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_LBRACE);
    ZrLexerNext(ps->lexer);

    SZrAstNodeArray *keys = ZrAstNodeArrayNew(ps->state, 4);
    if (keys == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_RBRACE) {
        SZrAstNode *first = parse_identifier(ps);
        if (first != ZR_NULL) {
            ZrAstNodeArrayAdd(ps->state, keys, first);
        }

        while (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_RBRACE) {
                break;
            }
            SZrAstNode *key = parse_identifier(ps);
            if (key != ZR_NULL) {
                ZrAstNodeArrayAdd(ps->state, keys, key);
            } else {
                break;
            }
        }
    }

    expect_token(ps, ZR_TK_RBRACE);
    ZrLexerNext(ps->lexer);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange destructuringLoc = ZrFileRangeMerge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_DESTRUCTURING_OBJECT, destructuringLoc);
    if (node == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, keys);
        return ZR_NULL;
    }

    node->data.destructuringObject.keys = keys;
    return node;
}

// 解析解构数组模式
static SZrAstNode *parse_destructuring_array(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_LBRACKET);
    ZrLexerNext(ps->lexer);

    SZrAstNodeArray *keys = ZrAstNodeArrayNew(ps->state, 4);
    if (keys == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_RBRACKET) {
        SZrAstNode *first = parse_identifier(ps);
        if (first != ZR_NULL) {
            ZrAstNodeArrayAdd(ps->state, keys, first);
        }

        while (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_RBRACKET) {
                break;
            }
            SZrAstNode *key = parse_identifier(ps);
            if (key != ZR_NULL) {
                ZrAstNodeArrayAdd(ps->state, keys, key);
            } else {
                break;
            }
        }
    }

    expect_token(ps, ZR_TK_RBRACKET);
    ZrLexerNext(ps->lexer);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange destructuringLoc = ZrFileRangeMerge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_DESTRUCTURING_ARRAY, destructuringLoc);
    if (node == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, keys);
        return ZR_NULL;
    }

    node->data.destructuringArray.keys = keys;
    return node;
}

// 解析访问修饰符
static EZrAccessModifier parse_access_modifier(SZrParserState *ps) {
    EZrToken token = ps->lexer->t.token;
    if (token == ZR_TK_PUB) {
        ZrLexerNext(ps->lexer);
        return ZR_ACCESS_PUBLIC;
    } else if (token == ZR_TK_PRI) {
        ZrLexerNext(ps->lexer);
        return ZR_ACCESS_PRIVATE;
    } else if (token == ZR_TK_PRO) {
        ZrLexerNext(ps->lexer);
        return ZR_ACCESS_PROTECTED;
    }
    return ZR_ACCESS_PUBLIC;  // 默认
}

// 解析参数
static SZrAstNode *parse_parameter(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 检查是否是可变参数 (...name: type)
    TBool isVariadic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        isVariadic = ZR_TRUE;
        ZrLexerNext(ps->lexer);
    }
    
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }

    SZrIdentifier *name = &nameNode->data.identifier;

    // 可选类型注解
    SZrType *typeInfo = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        typeInfo = parse_type(ps);
        // 如果类型解析失败，仍然创建参数节点（typeInfo 为 NULL）
        // 这样可以进行错误恢复
    }

    // 可选默认值（可变参数不能有默认值）
    SZrAstNode *defaultValue = ZR_NULL;
    if (!isVariadic && consume_token(ps, ZR_TK_EQUALS)) {
        defaultValue = parse_expression(ps);
    }

    SZrAstNode *node = create_ast_node(ps, ZR_AST_PARAMETER, startLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.parameter.name = name;
    node->data.parameter.typeInfo = typeInfo;
    node->data.parameter.defaultValue = defaultValue;
    return node;
}

// 解析参数列表
static SZrAstNodeArray *parse_parameter_list(SZrParserState *ps) {
    SZrAstNodeArray *params = ZrAstNodeArrayNew(ps->state, 4);
    if (params == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_RPAREN && ps->lexer->t.token != ZR_TK_PARAMS) {
        SZrAstNode *first = parse_parameter(ps);
        if (first != ZR_NULL) {
            ZrAstNodeArrayAdd(ps->state, params, first);
        }

        while (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_RPAREN) {
                break;
            }
            SZrAstNode *param = parse_parameter(ps);
            if (param != ZR_NULL) {
                ZrAstNodeArrayAdd(ps->state, params, param);
            } else {
                break;
            }
        }
    }

    return params;
}

// ==================== 声明解析 ====================

// 解析模块声明
static SZrAstNode *parse_module_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_MODULE);
    ZrLexerNext(ps->lexer);

    SZrAstNode *name = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
        name = parse_identifier(ps);
    } else if (ps->lexer->t.token == ZR_TK_STRING) {
        name = parse_literal(ps);
    } else {
        report_error(ps, "Expected identifier or string after module");
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_MODULE_DECLARATION, startLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.moduleDeclaration.name = name;
    return node;
}

// 解析变量声明
static SZrAstNode *parse_variable_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_VAR);
    ZrLexerNext(ps->lexer);

    // 解析模式（标识符或解构）
    SZrAstNode *pattern = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
        pattern = parse_identifier(ps);
    } else if (ps->lexer->t.token == ZR_TK_LBRACE) {
        // 对象解构模式 {key1, key2, ...}
        pattern = parse_destructuring_object(ps);
    } else if (ps->lexer->t.token == ZR_TK_LBRACKET) {
        // 数组解构模式 [elem1, elem2, ...]
        pattern = parse_destructuring_array(ps);
    } else {
        report_error(ps, "Expected identifier or destructuring pattern");
        return ZR_NULL;
    }
    
    if (pattern == ZR_NULL) {
        return ZR_NULL;
    }

    // 可选类型注解
    SZrType *typeInfo = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        typeInfo = parse_type(ps);
        if (typeInfo == ZR_NULL) {
            report_error(ps, "Failed to parse type annotation");
            return ZR_NULL;
        }
    }

    // 等号和值
    if (!consume_token(ps, ZR_TK_EQUALS)) {
        const TChar *tokenStr = ZrLexerTokenToString(ps->lexer, ps->lexer->t.token);
        TChar errorMsg[256];
        snprintf(errorMsg, sizeof(errorMsg), "期望 '='，但遇到 '%s'", tokenStr);
        report_error_with_token(ps, errorMsg, ps->lexer->t.token);
        return ZR_NULL;
    }

    SZrAstNode *value = parse_expression(ps);
    if (value == ZR_NULL) {
        // 如果解析表达式失败，尝试错误恢复
        if (ps->hasError) {
            return ZR_NULL;
        }
        // 如果没有错误但返回 NULL，可能是遇到了不支持的语法
        const TChar *tokenStr = ZrLexerTokenToString(ps->lexer, ps->lexer->t.token);
        TChar errorMsg[256];
        snprintf(errorMsg, sizeof(errorMsg), "无法解析表达式（遇到 '%s'）", tokenStr);
        report_error_with_token(ps, errorMsg, ps->lexer->t.token);
        return ZR_NULL;
    }

    // 分号是可选的（在某些情况下）
    // 注意：在检查分号之前，表达式应该已经完全解析（包括成员访问和函数调用）
    if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
        consume_token(ps, ZR_TK_SEMICOLON);
    } else if (ps->lexer->t.token != ZR_TK_EOS && 
               ps->lexer->t.token != ZR_TK_VAR && 
               ps->lexer->t.token != ZR_TK_STRUCT &&
               ps->lexer->t.token != ZR_TK_CLASS &&
               ps->lexer->t.token != ZR_TK_INTERFACE &&
               ps->lexer->t.token != ZR_TK_ENUM &&
               ps->lexer->t.token != ZR_TK_TEST &&
               ps->lexer->t.token != ZR_TK_INTERMEDIATE &&
               ps->lexer->t.token != ZR_TK_MODULE &&
               ps->lexer->t.token != ZR_TK_DOT &&  // 允许成员访问继续
               ps->lexer->t.token != ZR_TK_LBRACKET &&  // 允许计算属性访问
               ps->lexer->t.token != ZR_TK_LPAREN) {  // 允许函数调用
        // 如果下一个 token 不是语句开始关键字或表达式继续符号，期望分号
        const TChar *tokenStr = ZrLexerTokenToString(ps->lexer, ps->lexer->t.token);
        TChar errorMsg[256];
        snprintf(errorMsg, sizeof(errorMsg), "期望 ';'，但遇到 '%s'", tokenStr);
        report_error_with_token(ps, errorMsg, ps->lexer->t.token);
        // 不立即返回，允许错误恢复
    }

    SZrAstNode *node = create_ast_node(ps, ZR_AST_VARIABLE_DECLARATION, startLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.variableDeclaration.pattern = pattern;
    node->data.variableDeclaration.value = value;
    node->data.variableDeclaration.typeInfo = typeInfo;
    return node;
}

// 解析函数声明
static SZrAstNode *parse_function_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);

    // 解析装饰器（可选）
    SZrAstNodeArray *decorators = ZrAstNodeArrayNew(ps->state, 2);
    while (ps->lexer->t.token == ZR_TK_SHARP) {
        SZrAstNode *decorator = parse_decorator_expression(ps);
        if (decorator != ZR_NULL) {
            ZrAstNodeArrayAdd(ps->state, decorators, decorator);
        } else {
            break;
        }
    }

    // 解析函数名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, decorators);
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;

    // 解析泛型声明（可选）
    SZrGenericDeclaration *generic = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
        generic = parse_generic_declaration(ps);
    }

    // 解析参数列表
    expect_token(ps, ZR_TK_LPAREN);
    ZrLexerNext(ps->lexer);

    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL;  // 可变参数

    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        // 只有可变参数的情况 (...name: type)
        // parse_parameter 会处理 ZR_TK_PARAMS
        SZrAstNode *argsNode = parse_parameter(ps);
        if (argsNode != ZR_NULL) {
            args = &argsNode->data.parameter;
        } else {
            // 如果解析失败，清理并返回
            // parse_parameter 已经报告了错误（如果 token 不是标识符）
            ZrAstNodeArrayFree(ps->state, decorators);
            return ZR_NULL;
        }
        params = ZrAstNodeArrayNew(ps->state, 0);
    } else {
        params = parse_parameter_list(ps);
        if (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_PARAMS) {
                // 普通参数后跟可变参数 (param1, param2, ...name: type)
                // parse_parameter 会处理 ZR_TK_PARAMS
                SZrAstNode *argsNode = parse_parameter(ps);
                if (argsNode != ZR_NULL) {
                    args = &argsNode->data.parameter;
                } else {
                    // 如果解析失败，清理并返回
                    // parse_parameter 已经报告了错误（如果 token 不是标识符）
                    if (params != ZR_NULL) {
                        ZrAstNodeArrayFree(ps->state, params);
                    }
                    ZrAstNodeArrayFree(ps->state, decorators);
                    return ZR_NULL;
                }
            } else {
                // 逗号后不是可变参数，回退
                // 这里应该报告错误，但为了兼容性，我们继续解析
            }
        }
    }

    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);

    // 解析返回类型（可选）
    SZrType *returnType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        returnType = parse_type(ps);
    }

    // 解析函数体
    SZrAstNode *body = parse_block(ps);
    if (body == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrAstNodeArrayFree(ps->state, params);
        }
        ZrAstNodeArrayFree(ps->state, decorators);
        return ZR_NULL;
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange funcLoc = ZrFileRangeMerge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_FUNCTION_DECLARATION, funcLoc);
    if (node == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrAstNodeArrayFree(ps->state, params);
        }
        ZrAstNodeArrayFree(ps->state, decorators);
        return ZR_NULL;
    }

    node->data.functionDeclaration.name = name;
    node->data.functionDeclaration.generic = generic;
    node->data.functionDeclaration.params = params;
    node->data.functionDeclaration.args = args;
    node->data.functionDeclaration.returnType = returnType;
    node->data.functionDeclaration.body = body;
    node->data.functionDeclaration.decorators = decorators;
    return node;
}

// ==================== 语句解析 ====================

// 解析块
static SZrAstNode *parse_block(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_LBRACE);
    ZrLexerNext(ps->lexer);

    SZrAstNodeArray *statements = ZrAstNodeArrayNew(ps->state, 8);
    if (statements == ZR_NULL) {
        report_error(ps, "Failed to allocate statement array");
        return ZR_NULL;
    }

    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *stmt = parse_statement(ps);
        if (stmt != ZR_NULL) {
            ZrAstNodeArrayAdd(ps->state, statements, stmt);
        } else {
            break;  // 遇到错误
        }
    }

    expect_token(ps, ZR_TK_RBRACE);
    consume_token(ps, ZR_TK_RBRACE);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange blockLoc = ZrFileRangeMerge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_BLOCK, blockLoc);
    if (node == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, statements);
        return ZR_NULL;
    }

    node->data.block.body = statements;
    node->data.block.isStatement = ZR_TRUE;
    return node;
}

// 解析表达式语句
static SZrAstNode *parse_expression_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNode *expr = parse_expression(ps);
    if (expr == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_EXPRESSION_STATEMENT, startLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.expressionStatement.expr = expr;
    return node;
}

// 解析返回语句
static SZrAstNode *parse_return_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_RETURN);
    ZrLexerNext(ps->lexer);

    SZrAstNode *expr = ZR_NULL;
    if (ps->lexer->t.token != ZR_TK_SEMICOLON) {
        expr = parse_expression(ps);
    }

    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_RETURN_STATEMENT, startLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.returnStatement.expr = expr;
    return node;
}

// 解析 switch 表达式/语句
static SZrAstNode *parse_switch_expression(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_SWITCH);
    ZrLexerNext(ps->lexer);

    expect_token(ps, ZR_TK_LPAREN);
    ZrLexerNext(ps->lexer);

    SZrAstNode *expr = parse_expression(ps);
    if (expr == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_RPAREN);
    ZrLexerNext(ps->lexer);

    expect_token(ps, ZR_TK_LBRACE);
    ZrLexerNext(ps->lexer);

    SZrAstNodeArray *cases = ZrAstNodeArrayNew(ps->state, 4);
    SZrAstNode *defaultCase = ZR_NULL;

    // 解析 switch cases
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        if (consume_token(ps, ZR_TK_LPAREN)) {
            if (ps->lexer->t.token == ZR_TK_RPAREN) {
                // 默认 case
                ZrLexerNext(ps->lexer);
                SZrAstNode *block = parse_block(ps);
                if (block != ZR_NULL) {
                    SZrFileRange defaultLoc = get_current_location(ps);
                    defaultCase = create_ast_node(ps, ZR_AST_SWITCH_DEFAULT, defaultLoc);
                    if (defaultCase != ZR_NULL) {
                        defaultCase->data.switchDefault.block = block;
                    }
                }
            } else {
                // 普通 case
                SZrAstNode *value = parse_expression(ps);
                expect_token(ps, ZR_TK_RPAREN);
                ZrLexerNext(ps->lexer);
                SZrAstNode *block = parse_block(ps);
                if (value != ZR_NULL && block != ZR_NULL) {
                    SZrFileRange caseLoc = get_current_location(ps);
                    SZrAstNode *caseNode = create_ast_node(ps, ZR_AST_SWITCH_CASE, caseLoc);
                    if (caseNode != ZR_NULL) {
                        caseNode->data.switchCase.value = value;
                        caseNode->data.switchCase.block = block;
                        ZrAstNodeArrayAdd(ps->state, cases, caseNode);
                    }
                }
            }
        } else {
            break;
        }
    }

    expect_token(ps, ZR_TK_RBRACE);
    ZrLexerNext(ps->lexer);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange switchLoc = ZrFileRangeMerge(startLoc, endLoc);

    SZrAstNode *switchNode = create_ast_node(ps, ZR_AST_SWITCH_EXPRESSION, switchLoc);
    if (switchNode != ZR_NULL) {
        switchNode->data.switchExpression.expr = expr;
        switchNode->data.switchExpression.cases = cases;
        switchNode->data.switchExpression.defaultCase = defaultCase;
        switchNode->data.switchExpression.isStatement = ZR_FALSE;  // 默认是表达式
        return switchNode;
    }
    return ZR_NULL;
}

// 解析 if 表达式/语句
static SZrAstNode *parse_if_expression(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_IF);
    ZrLexerNext(ps->lexer);

    expect_token(ps, ZR_TK_LPAREN);
    consume_token(ps, ZR_TK_LPAREN);

    SZrAstNode *condition = parse_expression(ps);
    if (condition == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);

    SZrAstNode *thenExpr = parse_block(ps);
    if (thenExpr == ZR_NULL) {
        return ZR_NULL;
    }

    SZrAstNode *elseExpr = ZR_NULL;
    if (consume_token(ps, ZR_TK_ELSE)) {
        if (ps->lexer->t.token == ZR_TK_IF) {
            elseExpr = parse_if_expression(ps);
        } else {
            elseExpr = parse_block(ps);
        }
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange ifLoc = ZrFileRangeMerge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_IF_EXPRESSION, ifLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.ifExpression.condition = condition;
    node->data.ifExpression.thenExpr = thenExpr;
    node->data.ifExpression.elseExpr = elseExpr;
    node->data.ifExpression.isStatement = ZR_FALSE;  // 默认是表达式
    return node;
}

// 解析 while 循环
static SZrAstNode *parse_while_loop(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_WHILE);
    ZrLexerNext(ps->lexer);

    expect_token(ps, ZR_TK_LPAREN);
    consume_token(ps, ZR_TK_LPAREN);

    SZrAstNode *cond = parse_expression(ps);
    if (cond == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);

    SZrAstNode *block = parse_block(ps);
    if (block == ZR_NULL) {
        return ZR_NULL;
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange loopLoc = ZrFileRangeMerge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_WHILE_LOOP, loopLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.whileLoop.cond = cond;
    node->data.whileLoop.block = block;
    node->data.whileLoop.isStatement = ZR_FALSE;  // 默认是表达式
    return node;
}

// 解析 for 循环
static SZrAstNode *parse_for_loop(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_FOR);
    ZrLexerNext(ps->lexer);

    expect_token(ps, ZR_TK_LPAREN);
    consume_token(ps, ZR_TK_LPAREN);

    // 解析初始化（可选）
    SZrAstNode *init = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_VAR) {
        init = parse_variable_declaration(ps);
        // 变量声明后面可能有分号，需要跳过
        if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
            consume_token(ps, ZR_TK_SEMICOLON);
        }
    } else if (ps->lexer->t.token != ZR_TK_SEMICOLON) {
        // 解析表达式（不是表达式语句，因为后面可能有分号）
        init = parse_expression(ps);
        // 如果后面是分号，跳过它
        if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
            consume_token(ps, ZR_TK_SEMICOLON);
        }
    } else {
        consume_token(ps, ZR_TK_SEMICOLON);
    }

    // 解析条件（可选）
    SZrAstNode *cond = ZR_NULL;
    if (ps->lexer->t.token != ZR_TK_SEMICOLON) {
        // 解析表达式（不是表达式语句）
        cond = parse_expression(ps);
        // 如果后面是分号，跳过它
        if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
            consume_token(ps, ZR_TK_SEMICOLON);
        }
    } else {
        consume_token(ps, ZR_TK_SEMICOLON);
    }

    // 解析步进（可选）
    SZrAstNode *step = ZR_NULL;
    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        step = parse_expression(ps);
    }

    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);

    SZrAstNode *block = parse_block(ps);
    if (block == ZR_NULL) {
        return ZR_NULL;
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange loopLoc = ZrFileRangeMerge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_FOR_LOOP, loopLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.forLoop.init = init;
    node->data.forLoop.cond = cond;
    node->data.forLoop.step = step;
    node->data.forLoop.block = block;
    node->data.forLoop.isStatement = ZR_FALSE;  // 默认是表达式
    return node;
}

// 解析 foreach 循环
static SZrAstNode *parse_foreach_loop(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_FOR);
    ZrLexerNext(ps->lexer);

    expect_token(ps, ZR_TK_LPAREN);
    consume_token(ps, ZR_TK_LPAREN);

    expect_token(ps, ZR_TK_VAR);
    consume_token(ps, ZR_TK_VAR);

    // 解析模式
    SZrAstNode *pattern = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
        pattern = parse_identifier(ps);
    } else {
        // TODO: 实现解构模式
        report_error(ps, "Expected identifier or destructuring pattern");
        return ZR_NULL;
    }

    // 可选类型注解
    SZrType *typeInfo = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        typeInfo = parse_type(ps);
    }

    expect_token(ps, ZR_TK_IN);
    consume_token(ps, ZR_TK_IN);

    SZrAstNode *expr = parse_expression(ps);
    if (expr == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);

    SZrAstNode *block = parse_block(ps);
    if (block == ZR_NULL) {
        return ZR_NULL;
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange loopLoc = ZrFileRangeMerge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_FOREACH_LOOP, loopLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.foreachLoop.pattern = pattern;
    node->data.foreachLoop.typeInfo = typeInfo;
    node->data.foreachLoop.expr = expr;
    node->data.foreachLoop.block = block;
    node->data.foreachLoop.isStatement = ZR_FALSE;  // 默认是表达式
    return node;
}

// 解析 break/continue 语句
static SZrAstNode *parse_break_continue_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    TBool isBreak = (ps->lexer->t.token == ZR_TK_BREAK);
    ZrLexerNext(ps->lexer);

    SZrAstNode *expr = ZR_NULL;
    if (ps->lexer->t.token != ZR_TK_SEMICOLON) {
        expr = parse_expression(ps);
    }

    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_BREAK_CONTINUE_STATEMENT, startLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.breakContinueStatement.isBreak = isBreak;
    node->data.breakContinueStatement.expr = expr;
    return node;
}

// 解析 out 语句（用于生成器表达式）
static SZrAstNode *parse_out_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_OUT);
    ZrLexerNext(ps->lexer);  // 消费 OUT

    SZrAstNode *expr = parse_expression(ps);
    if (expr == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange stmtLoc = ZrFileRangeMerge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_OUT_STATEMENT, stmtLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.outStatement.expr = expr;
    return node;
}

// 解析 throw 语句
static SZrAstNode *parse_throw_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_THROW);
    ZrLexerNext(ps->lexer);

    SZrAstNode *expr = parse_expression(ps);
    if (expr == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_THROW_STATEMENT, startLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.throwStatement.expr = expr;
    return node;
}

// 解析 try-catch-finally 语句
static SZrAstNode *parse_try_catch_finally_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_TRY);
    ZrLexerNext(ps->lexer);

    SZrAstNode *block = parse_block(ps);
    if (block == ZR_NULL) {
        return ZR_NULL;
    }

    // 解析 catch（可选）
    SZrAstNodeArray *catchPattern = ZR_NULL;
    SZrAstNode *catchBlock = ZR_NULL;
    if (consume_token(ps, ZR_TK_CATCH)) {
        expect_token(ps, ZR_TK_LPAREN);
        consume_token(ps, ZR_TK_LPAREN);

        catchPattern = parse_parameter_list(ps);

        expect_token(ps, ZR_TK_RPAREN);
        consume_token(ps, ZR_TK_RPAREN);

        catchBlock = parse_block(ps);
    }

    // 解析 finally（可选）
    SZrAstNode *finallyBlock = ZR_NULL;
    if (consume_token(ps, ZR_TK_FINALLY)) {
        finallyBlock = parse_block(ps);
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange tryLoc = ZrFileRangeMerge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_TRY_CATCH_FINALLY_STATEMENT, tryLoc);
    if (node == ZR_NULL) {
        if (catchPattern != ZR_NULL) {
            ZrAstNodeArrayFree(ps->state, catchPattern);
        }
        return ZR_NULL;
    }

    node->data.tryCatchFinallyStatement.block = block;
    node->data.tryCatchFinallyStatement.catchPattern = catchPattern;
    node->data.tryCatchFinallyStatement.catchBlock = catchBlock;
    node->data.tryCatchFinallyStatement.finallyBlock = finallyBlock;
    return node;
}

// 解析语句（入口函数）
static SZrAstNode *parse_statement(SZrParserState *ps) {
    EZrToken token = ps->lexer->t.token;

    switch (token) {
        case ZR_TK_LBRACE:
            return parse_block(ps);

        case ZR_TK_RETURN:
            return parse_return_statement(ps);

        case ZR_TK_VAR:
            return parse_variable_declaration(ps);

        case ZR_TK_IF:
            return parse_if_expression(ps);

        case ZR_TK_SWITCH: {
            SZrAstNode *switchNode = parse_switch_expression(ps);
            if (switchNode != ZR_NULL) {
                switchNode->data.switchExpression.isStatement = ZR_TRUE;
            }
            return switchNode;
        }

        case ZR_TK_WHILE:
            return parse_while_loop(ps);

        case ZR_TK_FOR: {
            // 判断是 for 还是 foreach
            // FOR ( VAR ... IN ... ) 是 foreach
            // FOR ( ... ; ... ; ... ) 是 for
            // 保存状态以便向前看
            TZrSize savedPos = ps->lexer->currentPos;
            TInt32 savedChar = ps->lexer->currentChar;
            TInt32 savedLine = ps->lexer->lineNumber;
            TInt32 savedLastLine = ps->lexer->lastLine;
            SZrToken savedToken = ps->lexer->t;
            SZrToken savedLookahead = ps->lexer->lookahead;
            TZrSize savedLookaheadPos = ps->lexer->lookaheadPos;
            TInt32 savedLookaheadChar = ps->lexer->lookaheadChar;
            TInt32 savedLookaheadLine = ps->lexer->lookaheadLine;
            TInt32 savedLookaheadLastLine = ps->lexer->lookaheadLastLine;
            
            // 跳过 FOR 和 LPAREN
            ZrLexerNext(ps->lexer);  // 消费 FOR
            if (ps->lexer->t.token == ZR_TK_LPAREN) {
                ZrLexerNext(ps->lexer);  // 消费 LPAREN
                if (ps->lexer->t.token == ZR_TK_VAR) {
                    // 可能是 foreach，继续检查
                    ZrLexerNext(ps->lexer);  // 消费 VAR
                    // 跳过模式（标识符、类型注解等）
                    while (ps->lexer->t.token != ZR_TK_IN && 
                           ps->lexer->t.token != ZR_TK_COLON && 
                           ps->lexer->t.token != ZR_TK_RPAREN &&
                           ps->lexer->t.token != ZR_TK_EOS) {
                        ZrLexerNext(ps->lexer);
                    }
                    if (ps->lexer->t.token == ZR_TK_IN) {
                        // 是 foreach，恢复状态并解析
                        ps->lexer->currentPos = savedPos;
                        ps->lexer->currentChar = savedChar;
                        ps->lexer->lineNumber = savedLine;
                        ps->lexer->lastLine = savedLastLine;
                        ps->lexer->t = savedToken;
                        ps->lexer->lookahead = savedLookahead;
                        ps->lexer->lookaheadPos = savedLookaheadPos;
                        ps->lexer->lookaheadChar = savedLookaheadChar;
                        ps->lexer->lookaheadLine = savedLookaheadLine;
                        ps->lexer->lookaheadLastLine = savedLookaheadLastLine;
                        SZrAstNode *loop = parse_foreach_loop(ps);
                        if (loop != ZR_NULL) {
                            loop->data.foreachLoop.isStatement = ZR_TRUE;
                        }
                        return loop;
                    }
                }
            }
            // 恢复状态并解析 for
            ps->lexer->currentPos = savedPos;
            ps->lexer->currentChar = savedChar;
            ps->lexer->lineNumber = savedLine;
            ps->lexer->lastLine = savedLastLine;
            ps->lexer->t = savedToken;
            ps->lexer->lookahead = savedLookahead;
            ps->lexer->lookaheadPos = savedLookaheadPos;
            ps->lexer->lookaheadChar = savedLookaheadChar;
            ps->lexer->lookaheadLine = savedLookaheadLine;
            ps->lexer->lookaheadLastLine = savedLookaheadLastLine;
            SZrAstNode *loop = parse_for_loop(ps);
            if (loop != ZR_NULL) {
                loop->data.forLoop.isStatement = ZR_TRUE;
            }
            return loop;
        }

        case ZR_TK_BREAK:
        case ZR_TK_CONTINUE:
            return parse_break_continue_statement(ps);

        case ZR_TK_OUT:
            return parse_out_statement(ps);

        case ZR_TK_THROW:
            return parse_throw_statement(ps);

        case ZR_TK_TRY:
            return parse_try_catch_finally_statement(ps);

        // TODO: 实现 switch 语句

        default:
            // 尝试解析为表达式语句
            return parse_expression_statement(ps);
    }
}

// ==================== 顶层解析 ====================

// 解析顶层语句
static SZrAstNode *parse_top_level_statement(SZrParserState *ps) {
    EZrToken token = ps->lexer->t.token;

    switch (token) {
        case ZR_TK_MODULE:
            return parse_module_declaration(ps);

        case ZR_TK_VAR:
            return parse_variable_declaration(ps);

        case ZR_TK_STRUCT:
            return parse_struct_declaration(ps);

        case ZR_TK_CLASS:
            return parse_class_declaration(ps);

        case ZR_TK_INTERFACE:
            return parse_interface_declaration(ps);

        case ZR_TK_ENUM:
            return parse_enum_declaration(ps);

        case ZR_TK_TEST:
            return parse_test_declaration(ps);

        case ZR_TK_INTERMEDIATE:
            return parse_intermediate_statement(ps);

        default:
            // 检查是否是测试声明（%test("test_name") { ... }）
            if (token == ZR_TK_PERCENT) {
                // 让 parse_test_declaration 处理 %test 的情况
                return parse_test_declaration(ps);
            }
            // 检查是否是装饰器（# ... #），后面应该跟 class/struct/function 等
            if (token == ZR_TK_SHARP) {
                // 解析装饰器，然后检查后面的声明类型
                // 先解析装饰器表达式，然后查看下一个 token
                SZrAstNode *decorator = parse_decorator_expression(ps);
                if (decorator == ZR_NULL) {
                    return ZR_NULL;
                }
                // 检查下一个 token 是否是声明类型
                EZrToken nextToken = ps->lexer->t.token;
                if (nextToken == ZR_TK_CLASS) {
                    // 类声明会处理装饰器，但我们已经解析了一个，需要回退
                    // 更好的方法是让类声明解析函数处理所有装饰器
                    // 这里我们需要回退并让类声明解析函数处理
                    // 暂时先释放装饰器，让类声明解析函数重新解析
                    ZrParserFreeAst(ps->state, decorator);
                    return parse_class_declaration(ps);
                } else if (nextToken == ZR_TK_STRUCT) {
                    ZrParserFreeAst(ps->state, decorator);
                    return parse_struct_declaration(ps);
                } else if (nextToken == ZR_TK_IDENTIFIER) {
                    // 可能是函数声明
                    EZrToken lookahead = peek_token(ps);
                    if (lookahead == ZR_TK_LPAREN || lookahead == ZR_TK_LESS_THAN) {
                        ZrParserFreeAst(ps->state, decorator);
                        return parse_function_declaration(ps);
                    }
                }
                // 如果后面不是声明，则作为表达式语句处理
                // 但装饰器表达式通常不应该单独出现，这里可能需要错误处理
                // 暂时先返回装饰器作为表达式语句
                SZrAstNode *stmt = create_ast_node(ps, ZR_AST_EXPRESSION_STATEMENT, decorator->location);
                if (stmt != ZR_NULL) {
                    stmt->data.expressionStatement.expr = decorator;
                }
                return stmt;
            }
            // 检查是否是函数声明（标识符后跟括号或泛型）
            if (token == ZR_TK_IDENTIFIER) {
                // 查看下一个 token 判断是否是函数声明
                EZrToken lookahead = peek_token(ps);
                if (lookahead == ZR_TK_LPAREN || lookahead == ZR_TK_LESS_THAN) {
                    // 可能是函数声明
                    SZrAstNode *funcDecl = parse_function_declaration(ps);
                    // 如果解析失败且已报告错误，直接返回 NULL，不要回退到表达式解析
                    if (funcDecl == ZR_NULL && ps->hasError) {
                        return ZR_NULL;
                    }
                    return funcDecl;
                }
            }
            // 尝试解析为表达式语句
            return parse_expression_statement(ps);
    }
}

// 解析脚本
static SZrAstNode *parse_script(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);

    // 解析可选的模块声明
    SZrAstNode *moduleName = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_MODULE) {
        moduleName = parse_module_declaration(ps);
    }

    // 解析语句列表
    SZrAstNodeArray *statements = ZrAstNodeArrayNew(ps->state, 16);
    if (statements == ZR_NULL) {
        report_error(ps, "Failed to allocate statement array");
        return ZR_NULL;
    }

    TZrSize stmtCount = 0;
    TZrSize errorCount = 0;
    const TZrSize MAX_CONSECUTIVE_ERRORS = 10;  // 最多连续错误次数
    
    while (ps->lexer->t.token != ZR_TK_EOS) {
        // 保存错误状态
        TBool hadError = ps->hasError;
        const TChar *prevErrorMessage = ps->errorMessage;
        
        // 重置错误状态（临时）
        ps->hasError = ZR_FALSE;
        ps->errorMessage = ZR_NULL;
        
        SZrAstNode *stmt = parse_top_level_statement(ps);
        if (stmt != ZR_NULL) {
            ZrAstNodeArrayAdd(ps->state, statements, stmt);
            stmtCount++;
            errorCount = 0;  // 重置错误计数
        } else {
            // 检查是否真的发生了错误
            if (ps->hasError) {
                errorCount++;
                // 错误信息已经在 report_error 中输出，这里只输出统计信息
                // printf("  Parser error at statement %zu (已在上方显示详细信息)\n", stmtCount);
                
                // 如果连续错误太多，停止解析
                if (errorCount >= MAX_CONSECUTIVE_ERRORS) {
                    printf("  Too many consecutive errors (%zu), stopping parse\n", errorCount);
                    break;
                }
                
                // 尝试错误恢复：跳过到下一个可能的语句开始位置
                // 跳过当前 token 直到遇到分号、换行或语句开始关键字
                TZrSize skipCount = 0;
                const TZrSize MAX_SKIP_TOKENS = 100;  // 最多跳过 100 个 token
                while (ps->lexer->t.token != ZR_TK_EOS && skipCount < MAX_SKIP_TOKENS) {
                    EZrToken token = ps->lexer->t.token;
                    // 如果遇到分号，跳过它并继续
                    if (token == ZR_TK_SEMICOLON) {
                        ZrLexerNext(ps->lexer);
                        break;
                    }
                    // 如果遇到可能的语句开始关键字，停止跳过
                    // 注意：函数声明使用标识符，不是 ZR_TK_FUNCTION
                    if (token == ZR_TK_VAR || token == ZR_TK_STRUCT || token == ZR_TK_CLASS ||
                        token == ZR_TK_INTERFACE || token == ZR_TK_ENUM ||
                        token == ZR_TK_TEST || token == ZR_TK_INTERMEDIATE ||
                        token == ZR_TK_MODULE || token == ZR_TK_IDENTIFIER) {
                        break;
                    }
                    // 跳过当前 token
                    ZrLexerNext(ps->lexer);
                    skipCount++;
                }
            } else {
                // 没有错误但返回 NULL，可能是遇到了不支持的语法
                printf("  Warning: Failed to parse statement %zu (token: %d), skipping\n", stmtCount, ps->lexer->t.token);
                // 尝试跳过当前 token 继续解析
                if (ps->lexer->t.token != ZR_TK_EOS) {
                    ZrLexerNext(ps->lexer);
                }
            }
        }
    }
    printf("  Parsed %zu top-level statements\n", stmtCount);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange scriptLoc = ZrFileRangeMerge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_SCRIPT, scriptLoc);
    if (node == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, statements);
        return ZR_NULL;
    }

    node->data.script.moduleName = moduleName;
    node->data.script.statements = statements;
    return node;
}

// 解析源代码，返回 AST 根节点
SZrAstNode *ZrParserParse(SZrState *state, const TChar *source, TZrSize sourceLength, SZrString *sourceName) {
    SZrParserState ps;
    ZrParserStateInit(&ps, state, source, sourceLength, sourceName);

    if (ps.hasError) {
        ZrParserStateFree(&ps);
        return ZR_NULL;
    }

    SZrAstNode *ast = parse_script(&ps);

    ZrParserStateFree(&ps);
    return ast;
}

// 释放 AST 节点
void ZrParserFreeAst(SZrState *state, SZrAstNode *node) {
    if (node == ZR_NULL) {
        return;
    }

    // TODO: 递归释放所有子节点
    // 根据节点类型释放相应的资源
    // 这里需要根据不同的节点类型进行不同的释放操作

    ZrMemoryRawFreeWithType(state->global, node, sizeof(SZrAstNode), ZR_MEMORY_NATIVE_TYPE_ARRAY);
}

// 解析结构体字段
static SZrAstNode *parse_struct_field(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);
    
    // 解析 static 关键字（可选）
    TBool isStatic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrLexerNext(ps->lexer);
    }
    
    // 期望 var 关键字
    expect_token(ps, ZR_TK_VAR);
    ZrLexerNext(ps->lexer);
    
    // 解析字段名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    
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
    
    // 期望分号
    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange fieldLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_STRUCT_FIELD, fieldLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    
    node->data.structField.access = access;
    node->data.structField.isStatic = isStatic;
    node->data.structField.name = name;
    node->data.structField.typeInfo = typeInfo;
    node->data.structField.init = init;
    return node;
}

// 解析结构体方法
static SZrAstNode *parse_struct_method(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 解析装饰器（可选）
    SZrAstNodeArray *decorators = ZrAstNodeArrayNew(ps->state, 2);
    while (ps->lexer->t.token == ZR_TK_SHARP) {
        SZrAstNode *decorator = parse_decorator_expression(ps);
        if (decorator != ZR_NULL) {
            ZrAstNodeArrayAdd(ps->state, decorators, decorator);
        } else {
            break;
        }
    }
    
    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);
    
    // 解析 static 关键字（可选）
    TBool isStatic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrLexerNext(ps->lexer);
    }
    
    // 解析方法名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, decorators);
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    
    // 解析泛型声明（可选）
    SZrGenericDeclaration *generic = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
        generic = parse_generic_declaration(ps);
    }
    
    // 解析参数列表
    expect_token(ps, ZR_TK_LPAREN);
    ZrLexerNext(ps->lexer);
    
    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL;
    
    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        SZrAstNode *argsNode = parse_parameter(ps);
        if (argsNode != ZR_NULL) {
            args = &argsNode->data.parameter;
        }
        params = ZrAstNodeArrayNew(ps->state, 0);
    } else {
        params = parse_parameter_list(ps);
        if (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_PARAMS) {
                SZrAstNode *argsNode = parse_parameter(ps);
                if (argsNode != ZR_NULL) {
                    args = &argsNode->data.parameter;
                }
            }
        }
    }
    
    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);
    
    // 解析返回类型（可选）
    SZrType *returnType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        returnType = parse_type(ps);
    }
    
    // 解析方法体
    SZrAstNode *body = parse_block(ps);
    if (body == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrAstNodeArrayFree(ps->state, params);
        }
        ZrAstNodeArrayFree(ps->state, decorators);
        return ZR_NULL;
    }
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange methodLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_STRUCT_METHOD, methodLoc);
    if (node == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrAstNodeArrayFree(ps->state, params);
        }
        ZrAstNodeArrayFree(ps->state, decorators);
        return ZR_NULL;
    }
    
    node->data.structMethod.decorators = decorators;
    node->data.structMethod.access = access;
    node->data.structMethod.isStatic = isStatic;
    node->data.structMethod.name = name;
    node->data.structMethod.generic = generic;
    node->data.structMethod.params = params;
    node->data.structMethod.args = args;
    node->data.structMethod.returnType = returnType;
    node->data.structMethod.body = body;
    return node;
}

// 解析结构体元函数
static SZrAstNode *parse_struct_meta_function(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);
    
    // 解析 static 关键字（可选）
    TBool isStatic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrLexerNext(ps->lexer);
    }
    
    // 期望 @ 符号
    expect_token(ps, ZR_TK_AT);
    ZrLexerNext(ps->lexer);
    
    // 解析元标识符（@ 后面跟小写蛇形标识符）
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *meta = &nameNode->data.identifier;
    
    // 解析参数列表
    expect_token(ps, ZR_TK_LPAREN);
    ZrLexerNext(ps->lexer);
    
    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL;
    
    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        SZrAstNode *argsNode = parse_parameter(ps);
        if (argsNode != ZR_NULL) {
            args = &argsNode->data.parameter;
        }
        params = ZrAstNodeArrayNew(ps->state, 0);
    } else {
        params = parse_parameter_list(ps);
        if (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_PARAMS) {
                SZrAstNode *argsNode = parse_parameter(ps);
                if (argsNode != ZR_NULL) {
                    args = &argsNode->data.parameter;
                }
            }
        }
    }
    
    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);
    
    // 解析返回类型（可选）
    SZrType *returnType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        returnType = parse_type(ps);
    }
    
    // 解析函数体
    SZrAstNode *body = parse_block(ps);
    if (body == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrAstNodeArrayFree(ps->state, params);
        }
        return ZR_NULL;
    }
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange metaLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_STRUCT_META_FUNCTION, metaLoc);
    if (node == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrAstNodeArrayFree(ps->state, params);
        }
        return ZR_NULL;
    }
    
    node->data.structMetaFunction.access = access;
    node->data.structMetaFunction.isStatic = isStatic;
    node->data.structMetaFunction.meta = meta;
    node->data.structMetaFunction.params = params;
    node->data.structMetaFunction.args = args;
    node->data.structMetaFunction.returnType = returnType;
    node->data.structMetaFunction.body = body;
    return node;
}

// 解析结构体声明
static SZrAstNode *parse_struct_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 期望 struct 关键字
    expect_token(ps, ZR_TK_STRUCT);
    ZrLexerNext(ps->lexer);
    
    // 解析结构体名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    
    // 解析泛型声明（可选）
    SZrGenericDeclaration *generic = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
        generic = parse_generic_declaration(ps);
    }
    
    // 解析继承列表（可选，但注释说 struct 不允许继承，所以这里暂时不支持）
    SZrAstNodeArray *inherits = ZrAstNodeArrayNew(ps->state, 0);
    
    // 期望左大括号
    expect_token(ps, ZR_TK_LBRACE);
    ZrLexerNext(ps->lexer);
    
    // 解析成员列表
    SZrAstNodeArray *members = ZrAstNodeArrayNew(ps->state, 8);
    if (members == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, inherits);
        return ZR_NULL;
    }
    
    // 解析成员直到遇到右大括号
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *member = ZR_NULL;
        
        // 检查是否是字段（以 var 开头，可能前面有访问修饰符和 static）
        EZrToken token = ps->lexer->t.token;
        if (token == ZR_TK_PUB || token == ZR_TK_PRI || token == ZR_TK_PRO || 
            token == ZR_TK_STATIC || token == ZR_TK_VAR) {
            // 可能是字段，尝试解析
            // 需要向前看一个 token 来确定
            TZrSize savedPos = ps->lexer->currentPos;
            TInt32 savedChar = ps->lexer->currentChar;
            TInt32 savedLine = ps->lexer->lineNumber;
            TInt32 savedLastLine = ps->lexer->lastLine;
            SZrToken savedToken = ps->lexer->t;
            SZrToken savedLookahead = ps->lexer->lookahead;
            TZrSize savedLookaheadPos = ps->lexer->lookaheadPos;
            TInt32 savedLookaheadChar = ps->lexer->lookaheadChar;
            TInt32 savedLookaheadLine = ps->lexer->lookaheadLine;
            TInt32 savedLookaheadLastLine = ps->lexer->lookaheadLastLine;
            
            // 跳过访问修饰符和 static
            while (ps->lexer->t.token == ZR_TK_PUB || ps->lexer->t.token == ZR_TK_PRI || 
                   ps->lexer->t.token == ZR_TK_PRO || ps->lexer->t.token == ZR_TK_STATIC) {
                ZrLexerNext(ps->lexer);
            }
            
            if (ps->lexer->t.token == ZR_TK_VAR) {
                // 恢复状态并解析字段
                ps->lexer->currentPos = savedPos;
                ps->lexer->currentChar = savedChar;
                ps->lexer->lineNumber = savedLine;
                ps->lexer->lastLine = savedLastLine;
                ps->lexer->t = savedToken;
                ps->lexer->lookahead = savedLookahead;
                ps->lexer->lookaheadPos = savedLookaheadPos;
                ps->lexer->lookaheadChar = savedLookaheadChar;
                ps->lexer->lookaheadLine = savedLookaheadLine;
                ps->lexer->lookaheadLastLine = savedLookaheadLastLine;
                member = parse_struct_field(ps);
            } else if (ps->lexer->t.token == ZR_TK_AT) {
                // 恢复状态并解析元函数
                ps->lexer->currentPos = savedPos;
                ps->lexer->currentChar = savedChar;
                ps->lexer->lineNumber = savedLine;
                ps->lexer->lastLine = savedLastLine;
                ps->lexer->t = savedToken;
                ps->lexer->lookahead = savedLookahead;
                ps->lexer->lookaheadPos = savedLookaheadPos;
                ps->lexer->lookaheadChar = savedLookaheadChar;
                ps->lexer->lookaheadLine = savedLookaheadLine;
                ps->lexer->lookaheadLastLine = savedLookaheadLastLine;
                member = parse_struct_meta_function(ps);
            } else {
                // 恢复状态并解析方法
                ps->lexer->currentPos = savedPos;
                ps->lexer->currentChar = savedChar;
                ps->lexer->lineNumber = savedLine;
                ps->lexer->lastLine = savedLastLine;
                ps->lexer->t = savedToken;
                ps->lexer->lookahead = savedLookahead;
                ps->lexer->lookaheadPos = savedLookaheadPos;
                ps->lexer->lookaheadChar = savedLookaheadChar;
                ps->lexer->lookaheadLine = savedLookaheadLine;
                ps->lexer->lookaheadLastLine = savedLookaheadLastLine;
                member = parse_struct_method(ps);
            }
        } else if (token == ZR_TK_AT) {
            // 元函数
            member = parse_struct_meta_function(ps);
        } else if (token == ZR_TK_IDENTIFIER || token == ZR_TK_SHARP) {
            // 方法（可能有装饰器）
            member = parse_struct_method(ps);
        } else {
            // 未知的成员类型，报告错误并跳过
            report_error(ps, "Unexpected token in struct declaration");
            break;
        }
        
        if (member != ZR_NULL) {
            ZrAstNodeArrayAdd(ps->state, members, member);
        } else {
            // 解析失败，尝试恢复
            break;
        }
    }
    
    // 期望右大括号
    expect_token(ps, ZR_TK_RBRACE);
    consume_token(ps, ZR_TK_RBRACE);
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange structLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_STRUCT_DECLARATION, structLoc);
    if (node == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, inherits);
        ZrAstNodeArrayFree(ps->state, members);
        return ZR_NULL;
    }
    
    node->data.structDeclaration.name = name;
    node->data.structDeclaration.generic = generic;
    node->data.structDeclaration.inherits = inherits;
    node->data.structDeclaration.members = members;
    return node;
}

// 解析类声明
static SZrAstNode *parse_class_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 解析装饰器（可选）
    SZrAstNodeArray *decorators = ZrAstNodeArrayNew(ps->state, 2);
    while (ps->lexer->t.token == ZR_TK_SHARP) {
        SZrAstNode *decorator = parse_decorator_expression(ps);
        if (decorator != ZR_NULL) {
            ZrAstNodeArrayAdd(ps->state, decorators, decorator);
        } else {
            break;
        }
    }
    
    // 期望 class 关键字
    expect_token(ps, ZR_TK_CLASS);
    ZrLexerNext(ps->lexer);
    
    // 解析类名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, decorators);
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    
    // 解析泛型声明（可选）
    SZrGenericDeclaration *generic = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
        generic = parse_generic_declaration(ps);
    }
    
    // 解析继承列表（可选）
    SZrAstNodeArray *inherits = ZrAstNodeArrayNew(ps->state, 0);
    if (consume_token(ps, ZR_TK_COLON)) {
        // 解析类型列表
        if (ps->lexer->t.token != ZR_TK_LBRACE) {
            SZrType *firstType = parse_type(ps);
            if (firstType != ZR_NULL) {
                SZrAstNode *typeNode = create_ast_node(ps, ZR_AST_TYPE, get_current_location(ps));
                if (typeNode != ZR_NULL) {
                    typeNode->data.type = *firstType;
                    ZrMemoryRawFreeWithType(ps->state->global, firstType, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                    ZrAstNodeArrayAdd(ps->state, inherits, typeNode);
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
                        ZrMemoryRawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                        ZrAstNodeArrayAdd(ps->state, inherits, typeNode);
                    }
                } else {
                    break;
                }
            }
        }
    }
    
    // 期望左大括号
    expect_token(ps, ZR_TK_LBRACE);
    ZrLexerNext(ps->lexer);
    
    // 解析成员列表
    SZrAstNodeArray *members = ZrAstNodeArrayNew(ps->state, 8);
    if (members == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, decorators);
        ZrAstNodeArrayFree(ps->state, inherits);
        return ZR_NULL;
    }
    
    // 解析成员直到遇到右大括号
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *member = ZR_NULL;
        
        // 检查成员类型
        EZrToken token = ps->lexer->t.token;
        
        // 检查是否是装饰器或访问修饰符（可能是字段、方法或属性）
        if (token == ZR_TK_SHARP || token == ZR_TK_PUB || token == ZR_TK_PRI || token == ZR_TK_PRO || 
            token == ZR_TK_STATIC || token == ZR_TK_VAR) {
            // 保存状态以便向前看
            TZrSize savedPos = ps->lexer->currentPos;
            TInt32 savedChar = ps->lexer->currentChar;
            TInt32 savedLine = ps->lexer->lineNumber;
            TInt32 savedLastLine = ps->lexer->lastLine;
            SZrToken savedToken = ps->lexer->t;
            SZrToken savedLookahead = ps->lexer->lookahead;
            TZrSize savedLookaheadPos = ps->lexer->lookaheadPos;
            TInt32 savedLookaheadChar = ps->lexer->lookaheadChar;
            TInt32 savedLookaheadLine = ps->lexer->lookaheadLine;
            TInt32 savedLookaheadLastLine = ps->lexer->lookaheadLastLine;
            
            // 跳过装饰器
            while (ps->lexer->t.token == ZR_TK_SHARP) {
                parse_decorator_expression(ps);
            }
            
            // 跳过访问修饰符和 static
            while (ps->lexer->t.token == ZR_TK_PUB || ps->lexer->t.token == ZR_TK_PRI || 
                   ps->lexer->t.token == ZR_TK_PRO || ps->lexer->t.token == ZR_TK_STATIC) {
                ZrLexerNext(ps->lexer);
            }
            
            // 检查是字段、属性还是方法
            EZrToken nextToken = ps->lexer->t.token;
            
            // 恢复状态，让成员解析函数处理
            ps->lexer->currentPos = savedPos;
            ps->lexer->currentChar = savedChar;
            ps->lexer->lineNumber = savedLine;
            ps->lexer->lastLine = savedLastLine;
            ps->lexer->t = savedToken;
            ps->lexer->lookahead = savedLookahead;
            ps->lexer->lookaheadPos = savedLookaheadPos;
            ps->lexer->lookaheadChar = savedLookaheadChar;
            ps->lexer->lookaheadLine = savedLookaheadLine;
            ps->lexer->lookaheadLastLine = savedLookaheadLastLine;
            
            if (nextToken == ZR_TK_VAR) {
                // 字段
                member = parse_class_field(ps);
            } else if (nextToken == ZR_TK_GET || nextToken == ZR_TK_SET) {
                // 属性
                member = parse_class_property(ps);
            } else if (nextToken == ZR_TK_AT) {
                // 元函数
                member = parse_class_meta_function(ps);
            } else {
                // 方法
                member = parse_class_method(ps);
            }
        } else if (token == ZR_TK_AT) {
            // 元函数
            member = parse_class_meta_function(ps);
        } else if (token == ZR_TK_IDENTIFIER || token == ZR_TK_SHARP) {
            // 方法（可能有装饰器）
            member = parse_class_method(ps);
        } else {
            // 未知的成员类型，报告错误并跳过
            report_error(ps, "Unexpected token in class declaration");
            break;
        }
        
        if (member != ZR_NULL) {
            ZrAstNodeArrayAdd(ps->state, members, member);
        } else {
            // 解析失败，尝试恢复
            break;
        }
    }
    
    // 期望右大括号
    expect_token(ps, ZR_TK_RBRACE);
    consume_token(ps, ZR_TK_RBRACE);
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange classLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_CLASS_DECLARATION, classLoc);
    if (node == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, decorators);
        ZrAstNodeArrayFree(ps->state, inherits);
        ZrAstNodeArrayFree(ps->state, members);
        return ZR_NULL;
    }
    
    node->data.classDeclaration.name = name;
    node->data.classDeclaration.generic = generic;
    node->data.classDeclaration.inherits = inherits;
    node->data.classDeclaration.members = members;
    node->data.classDeclaration.decorators = decorators;
    return node;
}

// 解析接口字段声明
static SZrAstNode *parse_interface_field_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);
    
    // 期望 var 关键字
    expect_token(ps, ZR_TK_VAR);
    ZrLexerNext(ps->lexer);
    
    // 解析字段名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    
    // 可选类型注解
    SZrType *typeInfo = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        typeInfo = parse_type(ps);
    }
    
    // 期望分号
    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange fieldLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERFACE_FIELD_DECLARATION, fieldLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    
    node->data.interfaceFieldDeclaration.access = access;
    node->data.interfaceFieldDeclaration.name = name;
    node->data.interfaceFieldDeclaration.typeInfo = typeInfo;
    return node;
}

// 解析接口方法签名
static SZrAstNode *parse_interface_method_signature(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);
    
    // 解析方法名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    
    // 解析泛型声明（可选）
    SZrGenericDeclaration *generic = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
        generic = parse_generic_declaration(ps);
    }
    
    // 解析参数列表
    expect_token(ps, ZR_TK_LPAREN);
    ZrLexerNext(ps->lexer);
    
    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL;
    
    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        SZrAstNode *argsNode = parse_parameter(ps);
        if (argsNode != ZR_NULL) {
            args = &argsNode->data.parameter;
        }
        params = ZrAstNodeArrayNew(ps->state, 0);
    } else {
        params = parse_parameter_list(ps);
        if (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_PARAMS) {
                SZrAstNode *argsNode = parse_parameter(ps);
                if (argsNode != ZR_NULL) {
                    args = &argsNode->data.parameter;
                }
            }
        }
    }
    
    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);
    
    // 可选返回类型
    SZrType *returnType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        returnType = parse_type(ps);
    }
    
    // 期望分号
    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange methodLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERFACE_METHOD_SIGNATURE, methodLoc);
    if (node == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrAstNodeArrayFree(ps->state, params);
        }
        return ZR_NULL;
    }
    
    node->data.interfaceMethodSignature.access = access;
    node->data.interfaceMethodSignature.name = name;
    node->data.interfaceMethodSignature.generic = generic;
    node->data.interfaceMethodSignature.params = params;
    node->data.interfaceMethodSignature.args = args;
    node->data.interfaceMethodSignature.returnType = returnType;
    return node;
}

// 解析接口属性签名
static SZrAstNode *parse_interface_property_signature(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);
    
    // 解析 get/set 修饰符
    TBool hasGet = ZR_FALSE;
    TBool hasSet = ZR_FALSE;
    
    if (ps->lexer->t.token == ZR_TK_GET) {
        hasGet = ZR_TRUE;
        ZrLexerNext(ps->lexer);
        if (ps->lexer->t.token == ZR_TK_SET) {
            hasSet = ZR_TRUE;
            ZrLexerNext(ps->lexer);
        }
    } else if (ps->lexer->t.token == ZR_TK_SET) {
        hasSet = ZR_TRUE;
        ZrLexerNext(ps->lexer);
        if (ps->lexer->t.token == ZR_TK_GET) {
            hasGet = ZR_TRUE;
            ZrLexerNext(ps->lexer);
        }
    }
    
    // 解析属性名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    
    // 可选类型注解
    SZrType *typeInfo = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        typeInfo = parse_type(ps);
    }
    
    // 期望分号
    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange propertyLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERFACE_PROPERTY_SIGNATURE, propertyLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    
    node->data.interfacePropertySignature.access = access;
    node->data.interfacePropertySignature.hasGet = hasGet;
    node->data.interfacePropertySignature.hasSet = hasSet;
    node->data.interfacePropertySignature.name = name;
    node->data.interfacePropertySignature.typeInfo = typeInfo;
    return node;
}

// 解析接口元函数签名
static SZrAstNode *parse_interface_meta_signature(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);
    
    // 期望 @ 符号
    expect_token(ps, ZR_TK_AT);
    ZrLexerNext(ps->lexer);
    
    // 解析元标识符
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *meta = &nameNode->data.identifier;
    
    // 解析参数列表
    expect_token(ps, ZR_TK_LPAREN);
    ZrLexerNext(ps->lexer);
    
    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL;
    
    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        SZrAstNode *argsNode = parse_parameter(ps);
        if (argsNode != ZR_NULL) {
            args = &argsNode->data.parameter;
        }
        params = ZrAstNodeArrayNew(ps->state, 0);
    } else {
        params = parse_parameter_list(ps);
        if (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_PARAMS) {
                SZrAstNode *argsNode = parse_parameter(ps);
                if (argsNode != ZR_NULL) {
                    args = &argsNode->data.parameter;
                }
            }
        }
    }
    
    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);
    
    // 可选返回类型
    SZrType *returnType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        returnType = parse_type(ps);
    }
    
    // 期望分号
    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange metaLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERFACE_META_SIGNATURE, metaLoc);
    if (node == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrAstNodeArrayFree(ps->state, params);
        }
        return ZR_NULL;
    }
    
    node->data.interfaceMetaSignature.access = access;
    node->data.interfaceMetaSignature.meta = meta;
    node->data.interfaceMetaSignature.params = params;
    node->data.interfaceMetaSignature.args = args;
    node->data.interfaceMetaSignature.returnType = returnType;
    return node;
}

// 解析接口声明
static SZrAstNode *parse_interface_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 期望 interface 关键字
    expect_token(ps, ZR_TK_INTERFACE);
    ZrLexerNext(ps->lexer);
    
    // 解析接口名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    
    // 解析泛型声明（可选）
    SZrGenericDeclaration *generic = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
        generic = parse_generic_declaration(ps);
    }
    
    // 解析继承列表（可选）
    SZrAstNodeArray *inherits = ZrAstNodeArrayNew(ps->state, 0);
    if (consume_token(ps, ZR_TK_COLON)) {
        // 解析类型列表
        if (ps->lexer->t.token != ZR_TK_LBRACE) {
            SZrType *firstType = parse_type(ps);
            if (firstType != ZR_NULL) {
                SZrAstNode *typeNode = create_ast_node(ps, ZR_AST_TYPE, get_current_location(ps));
                if (typeNode != ZR_NULL) {
                    typeNode->data.type = *firstType;
                    ZrMemoryRawFreeWithType(ps->state->global, firstType, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                    ZrAstNodeArrayAdd(ps->state, inherits, typeNode);
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
                        ZrMemoryRawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                        ZrAstNodeArrayAdd(ps->state, inherits, typeNode);
                    }
                } else {
                    break;
                }
            }
        }
    }
    
    // 期望左大括号
    expect_token(ps, ZR_TK_LBRACE);
    ZrLexerNext(ps->lexer);
    
    // 解析成员列表
    SZrAstNodeArray *members = ZrAstNodeArrayNew(ps->state, 8);
    if (members == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, inherits);
        return ZR_NULL;
    }
    
    // 解析成员直到遇到右大括号
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *member = ZR_NULL;
        
        // 检查成员类型
        EZrToken token = ps->lexer->t.token;
        if (token == ZR_TK_PUB || token == ZR_TK_PRI || token == ZR_TK_PRO) {
            // 向前看以确定成员类型
            TZrSize savedPos = ps->lexer->currentPos;
            TInt32 savedChar = ps->lexer->currentChar;
            TInt32 savedLine = ps->lexer->lineNumber;
            TInt32 savedLastLine = ps->lexer->lastLine;
            SZrToken savedToken = ps->lexer->t;
            SZrToken savedLookahead = ps->lexer->lookahead;
            TZrSize savedLookaheadPos = ps->lexer->lookaheadPos;
            TInt32 savedLookaheadChar = ps->lexer->lookaheadChar;
            TInt32 savedLookaheadLine = ps->lexer->lookaheadLine;
            TInt32 savedLookaheadLastLine = ps->lexer->lookaheadLastLine;
            
            // 跳过访问修饰符
            ZrLexerNext(ps->lexer);
            
            if (ps->lexer->t.token == ZR_TK_VAR) {
                // 字段声明
                ps->lexer->currentPos = savedPos;
                ps->lexer->currentChar = savedChar;
                ps->lexer->lineNumber = savedLine;
                ps->lexer->lastLine = savedLastLine;
                ps->lexer->t = savedToken;
                ps->lexer->lookahead = savedLookahead;
                ps->lexer->lookaheadPos = savedLookaheadPos;
                ps->lexer->lookaheadChar = savedLookaheadChar;
                ps->lexer->lookaheadLine = savedLookaheadLine;
                ps->lexer->lookaheadLastLine = savedLookaheadLastLine;
                member = parse_interface_field_declaration(ps);
            } else if (ps->lexer->t.token == ZR_TK_GET || ps->lexer->t.token == ZR_TK_SET) {
                // 属性签名
                ps->lexer->currentPos = savedPos;
                ps->lexer->currentChar = savedChar;
                ps->lexer->lineNumber = savedLine;
                ps->lexer->lastLine = savedLastLine;
                ps->lexer->t = savedToken;
                ps->lexer->lookahead = savedLookahead;
                ps->lexer->lookaheadPos = savedLookaheadPos;
                ps->lexer->lookaheadChar = savedLookaheadChar;
                ps->lexer->lookaheadLine = savedLookaheadLine;
                ps->lexer->lookaheadLastLine = savedLookaheadLastLine;
                member = parse_interface_property_signature(ps);
            } else if (ps->lexer->t.token == ZR_TK_AT) {
                // 元函数签名
                ps->lexer->currentPos = savedPos;
                ps->lexer->currentChar = savedChar;
                ps->lexer->lineNumber = savedLine;
                ps->lexer->lastLine = savedLastLine;
                ps->lexer->t = savedToken;
                ps->lexer->lookahead = savedLookahead;
                ps->lexer->lookaheadPos = savedLookaheadPos;
                ps->lexer->lookaheadChar = savedLookaheadChar;
                ps->lexer->lookaheadLine = savedLookaheadLine;
                ps->lexer->lookaheadLastLine = savedLookaheadLastLine;
                member = parse_interface_meta_signature(ps);
            } else {
                // 方法签名
                ps->lexer->currentPos = savedPos;
                ps->lexer->currentChar = savedChar;
                ps->lexer->lineNumber = savedLine;
                ps->lexer->lastLine = savedLastLine;
                ps->lexer->t = savedToken;
                ps->lexer->lookahead = savedLookahead;
                ps->lexer->lookaheadPos = savedLookaheadPos;
                ps->lexer->lookaheadChar = savedLookaheadChar;
                ps->lexer->lookaheadLine = savedLookaheadLine;
                ps->lexer->lookaheadLastLine = savedLookaheadLastLine;
                member = parse_interface_method_signature(ps);
            }
        } else if (token == ZR_TK_AT) {
            // 元函数签名
            member = parse_interface_meta_signature(ps);
        } else if (token == ZR_TK_IDENTIFIER) {
            // 方法签名
            member = parse_interface_method_signature(ps);
        } else {
            // 未知的成员类型，报告错误并跳过
            report_error(ps, "Unexpected token in interface declaration");
            break;
        }
        
        if (member != ZR_NULL) {
            ZrAstNodeArrayAdd(ps->state, members, member);
        } else {
            // 解析失败，尝试恢复
            break;
        }
    }
    
    // 期望右大括号
    expect_token(ps, ZR_TK_RBRACE);
    consume_token(ps, ZR_TK_RBRACE);
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange interfaceLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERFACE_DECLARATION, interfaceLoc);
    if (node == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, inherits);
        ZrAstNodeArrayFree(ps->state, members);
        return ZR_NULL;
    }
    
    node->data.interfaceDeclaration.name = name;
    node->data.interfaceDeclaration.generic = generic;
    node->data.interfaceDeclaration.inherits = inherits;
    node->data.interfaceDeclaration.members = members;
    return node;
}

// 存根实现：枚举声明解析
// 解析枚举成员
static SZrAstNode *parse_enum_member(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 解析成员名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    
    // 可选值（name = value 或 name;）
    SZrAstNode *value = ZR_NULL;
    if (consume_token(ps, ZR_TK_EQUALS)) {
        value = parse_expression(ps);
        if (value == ZR_NULL) {
            return ZR_NULL;
        }
    }
    
    // 可选分隔符（逗号或分号）
    if (ps->lexer->t.token == ZR_TK_COMMA || ps->lexer->t.token == ZR_TK_SEMICOLON) {
        ZrLexerNext(ps->lexer);
    }
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange memberLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_ENUM_MEMBER, memberLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    
    node->data.enumMember.name = name;
    node->data.enumMember.value = value;
    return node;
}

// 解析枚举声明
// 语法：enum Name[: baseType] { members }
static SZrAstNode *parse_enum_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 期望 enum 关键字
    expect_token(ps, ZR_TK_ENUM);
    ZrLexerNext(ps->lexer);
    
    // 解析枚举名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    
    // 解析基础类型（可选，继承 int, string, float, bool）
    SZrType *baseType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        baseType = parse_type(ps);
        if (baseType == ZR_NULL) {
            return ZR_NULL;
        }
    }
    
    // 期望左大括号
    expect_token(ps, ZR_TK_LBRACE);
    ZrLexerNext(ps->lexer);
    
    // 解析成员列表
    SZrAstNodeArray *members = ZrAstNodeArrayNew(ps->state, 8);
    if (members == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 解析成员直到遇到右大括号
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *member = parse_enum_member(ps);
        if (member != ZR_NULL) {
            ZrAstNodeArrayAdd(ps->state, members, member);
        } else {
            // 解析失败，尝试恢复
            if (ps->hasError) {
                break;
            }
            // 跳过当前 token 继续解析
            if (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
                ZrLexerNext(ps->lexer);
            }
        }
    }
    
    // 期望右大括号
    expect_token(ps, ZR_TK_RBRACE);
    consume_token(ps, ZR_TK_RBRACE);
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange enumLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_ENUM_DECLARATION, enumLoc);
    if (node == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, members);
        return ZR_NULL;
    }
    
    node->data.enumDeclaration.name = name;
    node->data.enumDeclaration.baseType = baseType;
    node->data.enumDeclaration.members = members;
    return node;
}

// 解析测试声明
// 语法：%test("test_name") { ... }
static SZrAstNode *parse_test_declaration(SZrParserState *ps) {
    SZrFileRange startLoc;
    
    // 解析 %test
    if (ps->lexer->t.token == ZR_TK_PERCENT) {
        // 保存 % token 的位置信息（在调用 ZrLexerNext 之前）
        startLoc = get_current_location(ps);
        TInt32 percentLine = startLoc.start.line;
        TInt32 percentColumn = startLoc.start.column;
        ZrLexerNext(ps->lexer);
        // 期望 "test" 标识符或关键字
        // 注意：test 可能是关键字 ZR_TK_TEST，也可能是标识符 ZR_TK_IDENTIFIER
        if (ps->lexer->t.token == ZR_TK_TEST) {
            // test 是关键字，直接接受
            ZrLexerNext(ps->lexer);
        } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
            // test 是标识符，需要检查名称
            SZrString *identName = ps->lexer->t.seminfo.stringValue;
            if (identName == ZR_NULL) {
                report_error(ps, "Expected 'test' after '%'");
                return ZR_NULL;
            }
            TNativeString nameStr = ZrStringGetNativeString(identName);
            if (nameStr == ZR_NULL || strcmp(nameStr, "test") != 0) {
                TChar errorMsg[256];
                snprintf(errorMsg, sizeof(errorMsg), "Expected 'test' after '%%', but got identifier '%s'", nameStr ? nameStr : "<null>");
                report_error(ps, errorMsg);
                return ZR_NULL;
            }
            ZrLexerNext(ps->lexer);
        } else {
            // 使用保存的位置信息报告错误
            const TChar *fileName = "<unknown>";
            if (startLoc.source != ZR_NULL) {
                TNativeString nameStr = ZrStringGetNativeString(startLoc.source);
                if (nameStr != ZR_NULL) {
                    fileName = nameStr;
                }
            }
            const TChar *tokenStr = ZrLexerTokenToString(ps->lexer, ps->lexer->t.token);
            printf("  [%s:%d:%d] Expected 'test' after '%%' (遇到 token: '%s')\n", 
                   fileName, percentLine, percentColumn, tokenStr);
            report_error(ps, "Expected 'test' after '%'");
            return ZR_NULL;
        }
    } else if (ps->lexer->t.token == ZR_TK_TEST) {
        // 兼容旧的语法：test() { ... }
        startLoc = get_current_location(ps);
        ZrLexerNext(ps->lexer);
    } else {
        report_error(ps, "Expected '%test' or 'test'");
        return ZR_NULL;
    }

    // 解析测试名称参数：("test_name")
    expect_token(ps, ZR_TK_LPAREN);
    ZrLexerNext(ps->lexer);

    // 期望字符串字面量作为测试名
    SZrIdentifier *name = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_STRING) {
        SZrString *testNameStr = ps->lexer->t.seminfo.stringValue;
        ZrLexerNext(ps->lexer);
        
        // 创建标识符节点来存储测试名
        SZrAstNode *nameNode = create_identifier_node(ps, testNameStr);
        if (nameNode != ZR_NULL) {
            name = &nameNode->data.identifier;
        }
    } else {
        report_error(ps, "Expected string literal as test name");
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);

    // 解析测试体
    SZrAstNode *body = parse_block(ps);
    if (body == ZR_NULL) {
        return ZR_NULL;
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange testLoc = ZrFileRangeMerge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_TEST_DECLARATION, testLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.testDeclaration.name = name;
    node->data.testDeclaration.params = ZrAstNodeArrayNew(ps->state, 0);  // 测试没有参数列表
    node->data.testDeclaration.args = ZR_NULL;
    node->data.testDeclaration.body = body;
    return node;
}

// 存根实现：中间代码声明解析
// 解析 Intermediate 指令参数
static SZrAstNode *parse_intermediate_instruction_parameter(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrString *value = ZR_NULL;
    
    if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
        value = ps->lexer->t.seminfo.stringValue;
        ZrLexerNext(ps->lexer);
    } else if (ps->lexer->t.token == ZR_TK_INTEGER) {
        // 整数（包括十进制、十六进制和八进制）都使用 ZR_TK_INTEGER
        value = ps->lexer->t.seminfo.stringValue;  // 获取原始字符串
        ZrLexerNext(ps->lexer);
    } else {
        report_error(ps, "Expected identifier or number in intermediate instruction parameter");
        return ZR_NULL;
    }
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange paramLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERMEDIATE_INSTRUCTION_PARAMETER, paramLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    
    node->data.intermediateInstructionParameter.value = value;
    return node;
}

// 解析 Intermediate 指令
static SZrAstNode *parse_intermediate_instruction(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 解析指令名
    if (ps->lexer->t.token != ZR_TK_IDENTIFIER) {
        report_error(ps, "Expected identifier for intermediate instruction name");
        return ZR_NULL;
    }
    
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    
    // 解析参数列表（参数之间用空格分隔，lexer 已经处理了空白）
    // 语法：name value1 value2 ... ;
    SZrAstNodeArray *values = ZrAstNodeArrayNew(ps->state, 4);
    if (values == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 解析指令参数，直到遇到分号
    // lexer 已经跳过了空白，所以直接解析参数即可
    while (ps->lexer->t.token != ZR_TK_SEMICOLON && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *param = parse_intermediate_instruction_parameter(ps);
        if (param != ZR_NULL) {
            ZrAstNodeArrayAdd(ps->state, values, param);
        } else {
            // 如果解析失败，可能是遇到了分号或其他结束符
            // 检查是否是分号（可能前面有注释）
            if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
                break;
            }
            // 否则报告错误并继续尝试
            break;
        }
    }
    
    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange instLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERMEDIATE_INSTRUCTION, instLoc);
    if (node == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, values);
        return ZR_NULL;
    }
    
    node->data.intermediateInstruction.name = name;
    node->data.intermediateInstruction.values = values;
    return node;
}

// 解析 Intermediate 常量
static SZrAstNode *parse_intermediate_constant(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 解析常量名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    
    expect_token(ps, ZR_TK_EQUALS);
    consume_token(ps, ZR_TK_EQUALS);
    
    // 解析字面量值
    SZrAstNode *value = parse_literal(ps);
    if (value == ZR_NULL) {
        return ZR_NULL;
    }
    
    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange constLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERMEDIATE_CONSTANT, constLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    
    node->data.intermediateConstant.name = name;
    node->data.intermediateConstant.value = value;
    return node;
}

// 解析 Intermediate 声明
static SZrAstNode *parse_intermediate_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 解析名称
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    
    // 解析参数列表
    expect_token(ps, ZR_TK_LPAREN);
    consume_token(ps, ZR_TK_LPAREN);
    
    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL;
    
    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        SZrAstNode *argsNode = parse_parameter(ps);
        if (argsNode != ZR_NULL) {
            args = &argsNode->data.parameter;
        }
        params = ZrAstNodeArrayNew(ps->state, 0);
    } else {
        params = parse_parameter_list(ps);
        if (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_PARAMS) {
                SZrAstNode *argsNode = parse_parameter(ps);
                if (argsNode != ZR_NULL) {
                    args = &argsNode->data.parameter;
                }
            }
        }
    }
    
    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);
    
    // 解析返回类型（可选）
    SZrType *returnType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        returnType = parse_type(ps);
    }
    
    // 解析闭包（可选）< ... >
    // 注意：在 intermediate 声明中，返回类型和闭包参数列表之间使用 % 分隔符
    // 格式：:int % < ... > 或 :int %（如果没有闭包参数）
    SZrAstNodeArray *closures = ZrAstNodeArrayNew(ps->state, 0);
    if (consume_token(ps, ZR_TK_PERCENT)) {
        // 如果存在 % 分隔符，则解析闭包参数列表
        if (consume_token(ps, ZR_TK_LESS_THAN)) {
            // 解析闭包参数列表（格式与普通参数列表相同，但结束符是 >）
            // 闭包参数列表可以为空，所以先检查是否是 >
            // 使用 peek_token 检查，不要消费 token
            if (peek_token(ps) != ZR_TK_GREATER_THAN) {
                // 解析第一个参数
                SZrAstNode *first = parse_parameter(ps);
                if (first != ZR_NULL) {
                    ZrAstNodeArrayAdd(ps->state, closures, first);
                } else {
                    // 第一个参数解析失败，可能是语法错误
                    // 检查是否是空列表（直接是 >）
                    if (ps->lexer->t.token != ZR_TK_GREATER_THAN) {
                        // 不是空列表，但参数解析失败，报告错误
                        report_error(ps, "Failed to parse closure parameter");
                    }
                }
                
                // 解析后续参数（用逗号分隔）
                while (ps->lexer->t.token != ZR_TK_GREATER_THAN && ps->lexer->t.token != ZR_TK_EOS) {
                    if (!consume_token(ps, ZR_TK_COMMA)) {
                        // 没有逗号，可能是结束或错误
                        break;
                    }
                    if (ps->lexer->t.token == ZR_TK_GREATER_THAN) {
                        break;
                    }
                    SZrAstNode *param = parse_parameter(ps);
                    if (param != ZR_NULL) {
                        ZrAstNodeArrayAdd(ps->state, closures, param);
                    } else {
                        // 参数解析失败，报告错误但继续尝试恢复
                        break;
                    }
                }
            }
            expect_token(ps, ZR_TK_GREATER_THAN);
            consume_token(ps, ZR_TK_GREATER_THAN);
        }
        // 如果 % 后面没有 <，说明没有闭包参数列表，这是允许的
    }
    
    // 解析常量（可选）[ ... ]
    SZrAstNodeArray *constants = ZrAstNodeArrayNew(ps->state, 0);
    if (consume_token(ps, ZR_TK_LBRACKET)) {
        while (ps->lexer->t.token != ZR_TK_RBRACKET && ps->lexer->t.token != ZR_TK_EOS) {
            SZrAstNode *constant = parse_intermediate_constant(ps);
            if (constant != ZR_NULL) {
                ZrAstNodeArrayAdd(ps->state, constants, constant);
            } else {
                break;
            }
        }
        expect_token(ps, ZR_TK_RBRACKET);
        consume_token(ps, ZR_TK_RBRACKET);
    }
    
    // 解析局部变量（可选）( ... )
    SZrAstNodeArray *locals = ZrAstNodeArrayNew(ps->state, 0);
    if (consume_token(ps, ZR_TK_LPAREN)) {
        if (ps->lexer->t.token != ZR_TK_RPAREN) {
            SZrAstNodeArray *localParams = parse_parameter_list(ps);
            if (localParams != ZR_NULL) {
                ZrAstNodeArrayFree(ps->state, locals);
                locals = localParams;
            }
        }
        expect_token(ps, ZR_TK_RPAREN);
        consume_token(ps, ZR_TK_RPAREN);
    }
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange declLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERMEDIATE_DECLARATION, declLoc);
    if (node == ZR_NULL) {
        if (params != ZR_NULL) ZrAstNodeArrayFree(ps->state, params);
        if (closures != ZR_NULL) ZrAstNodeArrayFree(ps->state, closures);
        if (constants != ZR_NULL) ZrAstNodeArrayFree(ps->state, constants);
        if (locals != ZR_NULL) ZrAstNodeArrayFree(ps->state, locals);
        return ZR_NULL;
    }
    
    node->data.intermediateDeclaration.name = name;
    node->data.intermediateDeclaration.params = params;
    node->data.intermediateDeclaration.args = args;
    node->data.intermediateDeclaration.returnType = returnType;
    node->data.intermediateDeclaration.closures = closures;
    node->data.intermediateDeclaration.constants = constants;
    node->data.intermediateDeclaration.locals = locals;
    return node;
}

// 解析 Intermediate 语句
static SZrAstNode *parse_intermediate_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    expect_token(ps, ZR_TK_INTERMEDIATE);
    ZrLexerNext(ps->lexer);
    
    // 解析声明
    SZrAstNode *declaration = parse_intermediate_declaration(ps);
    if (declaration == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 解析指令块
    expect_token(ps, ZR_TK_LBRACE);
    consume_token(ps, ZR_TK_LBRACE);
    
    SZrAstNodeArray *instructions = ZrAstNodeArrayNew(ps->state, 8);
    if (instructions == ZR_NULL) {
        return ZR_NULL;
    }
    
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *instruction = parse_intermediate_instruction(ps);
        if (instruction != ZR_NULL) {
            ZrAstNodeArrayAdd(ps->state, instructions, instruction);
        } else {
            break;
        }
    }
    
    expect_token(ps, ZR_TK_RBRACE);
    consume_token(ps, ZR_TK_RBRACE);
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange stmtLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERMEDIATE_STATEMENT, stmtLoc);
    if (node == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, instructions);
        return ZR_NULL;
    }
    
    node->data.intermediateStatement.declaration = declaration;
    node->data.intermediateStatement.instructions = instructions;
    return node;
}

// 解析生成器表达式（{{}}）
static SZrAstNode *parse_generator_expression(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 期望第一个 {
    expect_token(ps, ZR_TK_LBRACE);
    ZrLexerNext(ps->lexer);
    
    // 期望第二个 {
    expect_token(ps, ZR_TK_LBRACE);
    ZrLexerNext(ps->lexer);
    
    // 解析块内容（语句列表），不期望 { 和 }
    SZrFileRange blockStartLoc = get_current_location(ps);
    SZrAstNodeArray *statements = ZrAstNodeArrayNew(ps->state, 8);
    if (statements == ZR_NULL) {
        report_error(ps, "Failed to allocate statement array");
        return ZR_NULL;
    }

    // 解析语句直到遇到第一个 }
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *stmt = parse_statement(ps);
        if (stmt != ZR_NULL) {
            ZrAstNodeArrayAdd(ps->state, statements, stmt);
        } else {
            break;  // 遇到错误
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
    SZrFileRange blockLoc = ZrFileRangeMerge(blockStartLoc, blockEndLoc);
    SZrAstNode *block = create_ast_node(ps, ZR_AST_BLOCK, blockLoc);
    if (block == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, statements);
        return ZR_NULL;
    }
    block->data.block.body = statements;
    block->data.block.isStatement = ZR_FALSE;  // 生成器表达式中的块是表达式
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange generatorLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_GENERATOR_EXPRESSION, generatorLoc);
    if (node == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, statements);
        return ZR_NULL;
    }
    
    node->data.generatorExpression.block = block;
    return node;
}

// 解析类字段
static SZrAstNode *parse_class_field(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 解析装饰器（可选）
    SZrAstNodeArray *decorators = ZrAstNodeArrayNew(ps->state, 2);
    while (ps->lexer->t.token == ZR_TK_SHARP) {
        SZrAstNode *decorator = parse_decorator_expression(ps);
        if (decorator != ZR_NULL) {
            ZrAstNodeArrayAdd(ps->state, decorators, decorator);
        } else {
            break;
        }
    }
    
    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);
    
    // 解析 static 关键字（可选）
    TBool isStatic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrLexerNext(ps->lexer);
    }
    
    // 期望 var 关键字
    expect_token(ps, ZR_TK_VAR);
    ZrLexerNext(ps->lexer);
    
    // 解析字段名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, decorators);
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    
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
    
    // 期望分号
    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange fieldLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_CLASS_FIELD, fieldLoc);
    if (node == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, decorators);
        return ZR_NULL;
    }
    
    node->data.classField.decorators = decorators;
    node->data.classField.access = access;
    node->data.classField.isStatic = isStatic;
    node->data.classField.name = name;
    node->data.classField.typeInfo = typeInfo;
    node->data.classField.init = init;
    return node;
}

// 解析类方法
static SZrAstNode *parse_class_method(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 解析装饰器（可选）
    SZrAstNodeArray *decorators = ZrAstNodeArrayNew(ps->state, 2);
    while (ps->lexer->t.token == ZR_TK_SHARP) {
        SZrAstNode *decorator = parse_decorator_expression(ps);
        if (decorator != ZR_NULL) {
            ZrAstNodeArrayAdd(ps->state, decorators, decorator);
        } else {
            break;
        }
    }
    
    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);
    
    // 解析 static 关键字（可选）
    TBool isStatic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrLexerNext(ps->lexer);
    }
    
    // 解析方法名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, decorators);
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    
    // 解析泛型声明（可选）
    SZrGenericDeclaration *generic = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
        generic = parse_generic_declaration(ps);
    }
    
    // 解析参数列表
    expect_token(ps, ZR_TK_LPAREN);
    ZrLexerNext(ps->lexer);
    
    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL;
    
    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        SZrAstNode *argsNode = parse_parameter(ps);
        if (argsNode != ZR_NULL) {
            args = &argsNode->data.parameter;
        }
        params = ZrAstNodeArrayNew(ps->state, 0);
    } else {
        params = parse_parameter_list(ps);
        if (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_PARAMS) {
                SZrAstNode *argsNode = parse_parameter(ps);
                if (argsNode != ZR_NULL) {
                    args = &argsNode->data.parameter;
                }
            }
        }
    }
    
    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);
    
    // 可选返回类型
    SZrType *returnType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        returnType = parse_type(ps);
    }
    
    // 解析方法体
    SZrAstNode *body = parse_block(ps);
    if (body == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, decorators);
        if (params != ZR_NULL) {
            ZrAstNodeArrayFree(ps->state, params);
        }
        return ZR_NULL;
    }
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange methodLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_CLASS_METHOD, methodLoc);
    if (node == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, decorators);
        if (params != ZR_NULL) {
            ZrAstNodeArrayFree(ps->state, params);
        }
        return ZR_NULL;
    }
    
    node->data.classMethod.decorators = decorators;
    node->data.classMethod.access = access;
    node->data.classMethod.isStatic = isStatic;
    node->data.classMethod.name = name;
    node->data.classMethod.generic = generic;
    node->data.classMethod.params = params;
    node->data.classMethod.args = args;
    node->data.classMethod.returnType = returnType;
    node->data.classMethod.body = body;
    return node;
}

// 解析属性 Getter
static SZrAstNode *parse_property_get(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 期望 get 关键字
    expect_token(ps, ZR_TK_GET);
    ZrLexerNext(ps->lexer);
    
    // 解析属性名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    
    // 可选返回类型
    SZrType *targetType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        targetType = parse_type(ps);
    }
    
    // 解析方法体
    SZrAstNode *body = parse_block(ps);
    if (body == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange getLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_PROPERTY_GET, getLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    
    node->data.propertyGet.name = name;
    node->data.propertyGet.targetType = targetType;
    node->data.propertyGet.body = body;
    return node;
}

// 解析属性 Setter
static SZrAstNode *parse_property_set(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 期望 set 关键字
    expect_token(ps, ZR_TK_SET);
    ZrLexerNext(ps->lexer);
    
    // 解析属性名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    
    // 解析参数列表
    expect_token(ps, ZR_TK_LPAREN);
    ZrLexerNext(ps->lexer);
    
    // 解析参数名
    SZrAstNode *paramNode = parse_identifier(ps);
    if (paramNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *param = &paramNode->data.identifier;
    
    // 可选参数类型
    SZrType *targetType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        targetType = parse_type(ps);
    }
    
    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);
    
    // 解析方法体
    SZrAstNode *body = parse_block(ps);
    if (body == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange setLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_PROPERTY_SET, setLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    
    node->data.propertySet.name = name;
    node->data.propertySet.param = param;
    node->data.propertySet.targetType = targetType;
    node->data.propertySet.body = body;
    return node;
}

// 解析类属性
static SZrAstNode *parse_class_property(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 解析装饰器（可选）
    SZrAstNodeArray *decorators = ZrAstNodeArrayNew(ps->state, 2);
    while (ps->lexer->t.token == ZR_TK_SHARP) {
        SZrAstNode *decorator = parse_decorator_expression(ps);
        if (decorator != ZR_NULL) {
            ZrAstNodeArrayAdd(ps->state, decorators, decorator);
        } else {
            break;
        }
    }
    
    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);
    
    // 解析 static 关键字（可选）
    TBool isStatic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrLexerNext(ps->lexer);
    }
    
    // 解析 get 或 set
    SZrAstNode *modifier = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_GET) {
        modifier = parse_property_get(ps);
    } else if (ps->lexer->t.token == ZR_TK_SET) {
        modifier = parse_property_set(ps);
    } else {
        ZrAstNodeArrayFree(ps->state, decorators);
        report_error(ps, "Expected 'get' or 'set' for property");
        return ZR_NULL;
    }
    
    if (modifier == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, decorators);
        return ZR_NULL;
    }
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange propertyLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_CLASS_PROPERTY, propertyLoc);
    if (node == ZR_NULL) {
        ZrAstNodeArrayFree(ps->state, decorators);
        return ZR_NULL;
    }
    
    node->data.classProperty.decorators = decorators;
    node->data.classProperty.access = access;
    node->data.classProperty.isStatic = isStatic;
    node->data.classProperty.modifier = modifier;
    return node;
}

// 解析类元函数
static SZrAstNode *parse_class_meta_function(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);
    
    // 解析 static 关键字（可选）
    TBool isStatic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrLexerNext(ps->lexer);
    }
    
    // 期望 @ 符号
    expect_token(ps, ZR_TK_AT);
    ZrLexerNext(ps->lexer);
    
    // 解析元标识符
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *meta = &nameNode->data.identifier;
    
    // 解析参数列表
    expect_token(ps, ZR_TK_LPAREN);
    ZrLexerNext(ps->lexer);
    
    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL;
    
    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        SZrAstNode *argsNode = parse_parameter(ps);
        if (argsNode != ZR_NULL) {
            args = &argsNode->data.parameter;
        }
        params = ZrAstNodeArrayNew(ps->state, 0);
    } else {
        params = parse_parameter_list(ps);
        if (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_PARAMS) {
                SZrAstNode *argsNode = parse_parameter(ps);
                if (argsNode != ZR_NULL) {
                    args = &argsNode->data.parameter;
                }
            }
        }
    }
    
    expect_token(ps, ZR_TK_RPAREN);
    consume_token(ps, ZR_TK_RPAREN);
    
    // 解析 super 调用参数（可选）
    SZrAstNodeArray *superArgs = ZrAstNodeArrayNew(ps->state, 0);
    if (consume_token(ps, ZR_TK_SUPER)) {
        expect_token(ps, ZR_TK_LPAREN);
        ZrLexerNext(ps->lexer);
        
        if (ps->lexer->t.token != ZR_TK_RPAREN) {
            SZrAstNode *firstArg = parse_expression(ps);
            if (firstArg != ZR_NULL) {
                ZrAstNodeArrayAdd(ps->state, superArgs, firstArg);
            }
            
            while (consume_token(ps, ZR_TK_COMMA)) {
                if (ps->lexer->t.token == ZR_TK_RPAREN) {
                    break;
                }
                SZrAstNode *arg = parse_expression(ps);
                if (arg != ZR_NULL) {
                    ZrAstNodeArrayAdd(ps->state, superArgs, arg);
                } else {
                    break;
                }
            }
        }
        
        expect_token(ps, ZR_TK_RPAREN);
        consume_token(ps, ZR_TK_RPAREN);
    }
    
    // 可选返回类型
    SZrType *returnType = ZR_NULL;
    if (consume_token(ps, ZR_TK_COLON)) {
        returnType = parse_type(ps);
    }
    
    // 解析方法体
    SZrAstNode *body = parse_block(ps);
    if (body == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrAstNodeArrayFree(ps->state, params);
        }
        ZrAstNodeArrayFree(ps->state, superArgs);
        return ZR_NULL;
    }
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange metaLoc = ZrFileRangeMerge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_CLASS_META_FUNCTION, metaLoc);
    if (node == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrAstNodeArrayFree(ps->state, params);
        }
        ZrAstNodeArrayFree(ps->state, superArgs);
        return ZR_NULL;
    }
    
    node->data.classMetaFunction.access = access;
    node->data.classMetaFunction.isStatic = isStatic;
    node->data.classMetaFunction.meta = meta;
    node->data.classMetaFunction.params = params;
    node->data.classMetaFunction.args = args;
    node->data.classMetaFunction.superArgs = superArgs;
    node->data.classMetaFunction.returnType = returnType;
    node->data.classMetaFunction.body = body;
    return node;
}

