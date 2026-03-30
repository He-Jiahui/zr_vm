//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/parser.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/array.h"


#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stddef.h>

// 辅助函数声明
static void expect_token(SZrParserState *ps, EZrToken expected);
static TZrBool consume_token(SZrParserState *ps, EZrToken token);
static EZrToken peek_token(SZrParserState *ps);
static TZrBool consume_type_closing_angle(SZrParserState *ps);
static SZrFileRange get_current_location(SZrParserState *ps);
static SZrFilePosition get_file_position_from_offset(SZrLexState *lexer, TZrSize offset);
static TZrSize get_current_token_length(SZrParserState *ps);
static SZrFileRange get_current_token_location(SZrParserState *ps);
static void report_error(SZrParserState *ps, const TZrChar *msg);
static void report_error_with_token(SZrParserState *ps, const TZrChar *msg, EZrToken token);

// AST 节点创建辅助函数
static SZrAstNode *create_ast_node(SZrParserState *ps, EZrAstNodeType type, SZrFileRange location);
static SZrAstNode *create_identifier_node_with_location(SZrParserState *ps, SZrString *name, SZrFileRange location);
static SZrAstNode *create_identifier_node(SZrParserState *ps, SZrString *name);
static SZrAstNode *create_boolean_literal_node(SZrParserState *ps, TZrBool value);
static SZrAstNode *create_integer_literal_node(SZrParserState *ps, TZrInt64 value, SZrString *literal);
static SZrAstNode *create_float_literal_node(SZrParserState *ps, TZrDouble value, SZrString *literal, TZrBool isSingle);
static SZrAstNode *create_string_literal_node(SZrParserState *ps, SZrString *value, TZrBool hasError, SZrString *literal);
static SZrAstNode *create_char_literal_node(SZrParserState *ps, TZrChar value, TZrBool hasError, SZrString *literal);
static SZrAstNode *create_null_literal_node(SZrParserState *ps);
static SZrAstNode *create_template_string_literal_node(SZrParserState *ps, SZrAstNodeArray *segments);
static SZrAstNode *create_interpolated_segment_node(SZrParserState *ps, SZrAstNode *expression);

// 解析函数声明
static SZrAstNode *parse_script(SZrParserState *ps);
static SZrAstNode *parse_top_level_statement(SZrParserState *ps);
static SZrAstNode *parse_expression(SZrParserState *ps);
static SZrAstNode *parse_statement(SZrParserState *ps);
static SZrAstNode *parse_using_statement(SZrParserState *ps);

// 字面量解析
static SZrAstNode *parse_literal(SZrParserState *ps);
static SZrAstNode *parse_identifier(SZrParserState *ps);
static SZrAstNode *parse_template_string_literal(SZrParserState *ps, SZrString *rawValue);

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
static SZrAstNode *parse_prototype_reference_expression(SZrParserState *ps);
static SZrAstNode *parse_construct_expression(SZrParserState *ps, TZrBool hasUsingPrefix);
static SZrAstNode *parse_generator_expression(SZrParserState *ps);

// 声明和类型解析
static SZrAstNode *parse_parameter(SZrParserState *ps);
static SZrAstNodeArray *parse_parameter_list(SZrParserState *ps);
static SZrType *parse_type(SZrParserState *ps);
static SZrType *parse_type_no_generic(SZrParserState *ps);  // 不解析泛型类型的版本
static TZrBool parse_array_size_constraint(SZrParserState *ps, SZrType *type);  // 解析数组大小约束
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
static SZrAstNode *parse_compile_time_declaration(SZrParserState *ps);
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
static void free_type_info(SZrState *state, SZrType *type);

// 初始化解析器状态
void ZrParser_State_Init(SZrParserState *ps, SZrState *state, const TZrChar *source, TZrSize sourceLength, SZrString *sourceName) {
    ZR_ASSERT(ps != ZR_NULL);
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(source != ZR_NULL);

    ps->state = state;
    ps->hasError = ZR_FALSE;
    ps->errorMessage = ZR_NULL;
    ps->errorCallback = ZR_NULL;
    ps->errorUserData = ZR_NULL;
    ps->suppressErrorOutput = ZR_FALSE;

    // 初始化词法分析器
    ps->lexer = ZrCore_Memory_RawMallocWithType(state->global, sizeof(SZrLexState), ZR_MEMORY_NATIVE_TYPE_STRING);
    if (ps->lexer == ZR_NULL) {
        ps->hasError = ZR_TRUE;
        ps->errorMessage = "Failed to allocate lexer state";
        return;
    }

    ZrParser_Lexer_Init(ps->lexer, state, source, sourceLength, sourceName);

    // 初始化当前位置
    SZrFilePosition startPos = ZrParser_FilePosition_Create(0, 1, 1);
    SZrFilePosition endPos = ZrParser_FilePosition_Create(0, 1, 1);
    ps->currentLocation = ZrParser_FileRange_Create(startPos, endPos, sourceName);
}

// 清理解析器状态
void ZrParser_State_Free(SZrParserState *ps) {
    if (ps == ZR_NULL) {
        return;
    }

    if (ps->lexer != ZR_NULL) {
        // 释放词法分析器的缓冲区
        if (ps->lexer->buffer != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(ps->state->global, ps->lexer->buffer, ps->lexer->bufferSize, ZR_MEMORY_NATIVE_TYPE_STRING);
        }
        ZrCore_Memory_RawFreeWithType(ps->state->global, ps->lexer, sizeof(SZrLexState), ZR_MEMORY_NATIVE_TYPE_STRING);
    }
}

// 期望特定 token
static void expect_token(SZrParserState *ps, EZrToken expected) {
    if (ps->lexer->t.token != expected) {
        const TZrChar *expectedStr = ZrParser_Lexer_TokenToString(ps->lexer, expected);
        const TZrChar *actualStr = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);
        TZrChar errorMsg[256];
        snprintf(errorMsg, sizeof(errorMsg), "期望 '%s'，但遇到 '%s'", expectedStr, actualStr);
        report_error_with_token(ps, errorMsg, ps->lexer->t.token);
    }
}

// 消费 token（如果匹配）
static TZrBool consume_token(SZrParserState *ps, EZrToken token) {
    if (ps->lexer->t.token == token) {
        ZrParser_Lexer_Next(ps->lexer);
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

// 查看下一个 token（不消费）
static EZrToken peek_token(SZrParserState *ps) {
    return ZrParser_Lexer_Lookahead(ps->lexer);
}

static TZrBool consume_type_closing_angle(SZrParserState *ps) {
    if (ps->lexer->t.token == ZR_TK_GREATER_THAN) {
        ZrParser_Lexer_Next(ps->lexer);
        return ZR_TRUE;
    }

    if (ps->lexer->t.token == ZR_TK_RIGHT_SHIFT) {
        if (ps->lexer->currentPos > 0) {
            ps->lexer->currentPos--;
        }
        ps->lexer->currentChar = '>';
        ps->lexer->lookahead.token = ZR_TK_EOS;
        ZrParser_Lexer_Next(ps->lexer);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

// 获取当前位置信息
static SZrFileRange get_current_location(SZrParserState *ps) {
    // 计算列号（从当前行开始到当前位置的字符数）
    TZrInt32 column = 1;
    if (ps->lexer->source != ZR_NULL && ps->lexer->currentPos > 0) {
        TZrSize pos = ps->lexer->currentPos - 1;
        while (pos > 0 && ps->lexer->source[pos] != '\n' && ps->lexer->source[pos] != '\r') {
            pos--;
            column++;
        }
    }
    
    SZrFilePosition start = ZrParser_FilePosition_Create(
        ps->lexer->currentPos,
        ps->lexer->lastLine > 0 ? ps->lexer->lastLine : ps->lexer->lineNumber,
        column
    );
    SZrFilePosition end = ZrParser_FilePosition_Create(
        ps->lexer->currentPos,
        ps->lexer->lineNumber,
        column
    );
    return ZrParser_FileRange_Create(start, end, ps->lexer->sourceName);
}

static void get_string_view_for_length(SZrString *value, const TZrChar **text, TZrSize *length) {
    if (text == ZR_NULL || length == ZR_NULL) {
        return;
    }

    *text = ZR_NULL;
    *length = 0;
    if (value == ZR_NULL) {
        return;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        *text = ZrCore_String_GetNativeStringShort(value);
        *length = value->shortStringLength;
    } else {
        *text = ZrCore_String_GetNativeString(value);
        *length = value->longStringLength;
    }
}

static SZrFilePosition get_file_position_from_offset(SZrLexState *lexer, TZrSize offset) {
    TZrInt32 line = 1;
    TZrSize lineStart = 0;

    if (lexer == ZR_NULL || lexer->source == ZR_NULL) {
        return ZrParser_FilePosition_Create(offset, 1, 1);
    }

    if (offset > lexer->sourceLength) {
        offset = lexer->sourceLength;
    }

    for (TZrSize index = 0; index < offset; index++) {
        if (lexer->source[index] == '\n') {
            line++;
            lineStart = index + 1;
        }
    }

    return ZrParser_FilePosition_Create(offset, line, (TZrInt32)(offset - lineStart + 1));
}

static TZrSize get_current_token_length(SZrParserState *ps) {
    const TZrChar *text = ZR_NULL;
    TZrSize length = 0;
    const TZrChar *tokenText;

    if (ps == ZR_NULL || ps->lexer == ZR_NULL) {
        return 0;
    }

    switch (ps->lexer->t.token) {
        case ZR_TK_IDENTIFIER:
            get_string_view_for_length(ps->lexer->t.seminfo.stringValue, &text, &length);
            return length;

        case ZR_TK_INTEGER:
        case ZR_TK_FLOAT:
            get_string_view_for_length(ps->lexer->t.seminfo.stringValue, &text, &length);
            return length;

        case ZR_TK_BOOLEAN:
            return ps->lexer->t.seminfo.booleanValue ? 4 : 5;

        case ZR_TK_EOS:
            return 0;

        default:
            tokenText = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);
            return tokenText != ZR_NULL ? strlen(tokenText) : 0;
    }
}

static SZrFileRange get_current_token_location(SZrParserState *ps) {
    TZrSize tokenLength;
    TZrSize endOffset;
    TZrSize startOffset;
    SZrFilePosition start;
    SZrFilePosition end;

    if (ps == ZR_NULL || ps->lexer == ZR_NULL) {
        SZrFilePosition zero = ZrParser_FilePosition_Create(0, 1, 1);
        return ZrParser_FileRange_Create(zero, zero, ZR_NULL);
    }

    tokenLength = get_current_token_length(ps);
    endOffset = ps->lexer->currentChar == -1
                    ? ps->lexer->currentPos
                    : (ps->lexer->currentPos > 0 ? ps->lexer->currentPos - 1 : 0);
    if (endOffset > ps->lexer->sourceLength) {
        endOffset = ps->lexer->sourceLength;
    }
    startOffset = endOffset >= tokenLength ? endOffset - tokenLength : 0;

    start = get_file_position_from_offset(ps->lexer, startOffset);
    end = get_file_position_from_offset(ps->lexer, endOffset);
    return ZrParser_FileRange_Create(start, end, ps->lexer->sourceName);
}

// 获取当前行的代码片段（前后各20个字符）
static void get_line_snippet(SZrParserState *ps, TZrChar *buffer, TZrSize bufferSize, TZrInt32 *errorColumn) {
    if (ps->lexer == ZR_NULL || ps->lexer->source == ZR_NULL || bufferSize == 0) {
        buffer[0] = '\0';
        *errorColumn = 1;
        return;
    }
    
    // 计算列号并找到行首
    TZrSize pos = ps->lexer->currentPos;
    TZrInt32 column = 1;
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
    TZrInt32 displayColumn = column;
    
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
        TZrChar c = ps->lexer->source[snippetStart + i];
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
static void report_error_with_token(SZrParserState *ps, const TZrChar *msg, EZrToken token) {
    SZrFileRange errorRange;

    ps->hasError = ZR_TRUE;
    ps->errorMessage = msg;
    errorRange = get_current_token_location(ps);

    if (ps->errorCallback != ZR_NULL) {
        ps->errorCallback(ps->errorUserData, &errorRange, msg, token);
    }
    
    if (!ps->suppressErrorOutput && ps->lexer != ZR_NULL) {
        // 获取 token 字符串
        const TZrChar *tokenStr = ZrParser_Lexer_TokenToString(ps->lexer, token);
        
        // 获取文件名
        const TZrChar *fileName = "<unknown>";
        if (ps->lexer->sourceName != ZR_NULL) {
            TZrNativeString nameStr = ZrCore_String_GetNativeString(ps->lexer->sourceName);
            if (nameStr != ZR_NULL) {
                fileName = nameStr;
            }
        }
        
        // 计算列号（从当前行开始到当前位置的字符数）
        TZrInt32 column = 1;
        if (ps->lexer->source != ZR_NULL && ps->lexer->currentPos > 0) {
            TZrSize pos = ps->lexer->currentPos - 1;
            while (pos > 0 && ps->lexer->source[pos] != '\n') {
                pos--;
                column++;
            }
        }
        
        // 获取代码片段
        TZrChar snippet[128];
        TZrInt32 displayColumn = 1;
        get_line_snippet(ps, snippet, sizeof(snippet), &displayColumn);
        
        // 输出详细的错误信息
        // 使用 lastLine 而不是 lineNumber，因为 lastLine 是当前 token 的行号
        TZrInt32 errorLine = ps->lexer->lastLine > 0 ? ps->lexer->lastLine : ps->lexer->lineNumber;
        fprintf(stderr, "  [%s:%d:%d] %s (遇到 token: '%s')\n", 
               fileName, errorLine, column, msg, tokenStr);
        
        // 输出代码片段和错误位置标记
        if (snippet[0] != '\0') {
            fprintf(stderr, "    %s\n", snippet);
            // 输出错误位置标记（^）
            for (TZrInt32 i = 0; i < displayColumn - 1; i++) {
                fputc(' ', stderr);
            }
            fprintf(stderr, "^\n");
        }
    }
}

// 报告解析错误
static void report_error(SZrParserState *ps, const TZrChar *msg) {
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
    SZrAstNode *node = ZrCore_Memory_RawMallocWithType(ps->state->global, sizeof(SZrAstNode), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (node == ZR_NULL) {
        report_error(ps, "Failed to allocate AST node");
        return ZR_NULL;
    }

    node->type = type;
    node->location = location;
    return node;
}

static SZrAstNode *create_identifier_node_with_location(SZrParserState *ps, SZrString *name, SZrFileRange location) {
    SZrAstNode *node = create_ast_node(ps, ZR_AST_IDENTIFIER_LITERAL, location);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.identifier.name = name;
    return node;
}

static SZrAstNode *create_identifier_node(SZrParserState *ps, SZrString *name) {
    SZrFileRange loc = get_current_location(ps);
    return create_identifier_node_with_location(ps, name, loc);
}

static SZrAstNode *create_boolean_literal_node(SZrParserState *ps, TZrBool value) {
    SZrFileRange loc = get_current_location(ps);
    SZrAstNode *node = create_ast_node(ps, ZR_AST_BOOLEAN_LITERAL, loc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.booleanLiteral.value = value;
    return node;
}

static SZrAstNode *create_integer_literal_node(SZrParserState *ps, TZrInt64 value, SZrString *literal) {
    SZrFileRange loc = get_current_location(ps);
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTEGER_LITERAL, loc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.integerLiteral.value = value;
    node->data.integerLiteral.literal = literal;
    return node;
}

static SZrAstNode *create_float_literal_node(SZrParserState *ps, TZrDouble value, SZrString *literal, TZrBool isSingle) {
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

static SZrAstNode *create_string_literal_node(SZrParserState *ps, SZrString *value, TZrBool hasError, SZrString *literal) {
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

static SZrAstNode *create_char_literal_node(SZrParserState *ps, TZrChar value, TZrBool hasError, SZrString *literal) {
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

static SZrAstNode *create_template_string_literal_node(SZrParserState *ps, SZrAstNodeArray *segments) {
    SZrFileRange loc = get_current_location(ps);
    SZrAstNode *node = create_ast_node(ps, ZR_AST_TEMPLATE_STRING_LITERAL, loc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.templateStringLiteral.segments = segments;
    return node;
}

static SZrAstNode *create_interpolated_segment_node(SZrParserState *ps, SZrAstNode *expression) {
    SZrFileRange loc = get_current_location(ps);
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERPOLATED_SEGMENT, loc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.interpolatedSegment.expression = expression;
    return node;
}

static void get_string_native_parts(SZrString *value, TZrNativeString *nativeValue, TZrSize *length) {
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

static TZrBool zr_string_equals_literal(SZrString *value, const TZrChar *literal) {
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

static TZrBool try_get_ownership_qualifier(SZrString *name, EZrOwnershipQualifier *qualifier) {
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

    return ZR_FALSE;
}

static SZrAstNode *parse_embedded_expression(SZrParserState *ps, const TZrChar *source, TZrSize sourceLength) {
    SZrParserState nestedParser;
    SZrAstNode *expression = ZR_NULL;
    TZrChar *sourceCopy;

    if (ps == ZR_NULL || source == ZR_NULL) {
        return ZR_NULL;
    }

    sourceCopy = ZrCore_Memory_RawMallocWithType(ps->state->global,
                                           sourceLength + 1,
                                           ZR_MEMORY_NATIVE_TYPE_STRING);
    if (sourceCopy == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(sourceCopy, source, sourceLength);
    sourceCopy[sourceLength] = '\0';

    ZrParser_State_Init(&nestedParser,
                      ps->state,
                      sourceCopy,
                      sourceLength,
                      ps->lexer != ZR_NULL ? ps->lexer->sourceName : ZR_NULL);
    if (!nestedParser.hasError) {
        expression = parse_expression(&nestedParser);
        if (nestedParser.hasError || expression == ZR_NULL || nestedParser.lexer->t.token != ZR_TK_EOS) {
            expression = ZR_NULL;
        }
    }

    ZrParser_State_Free(&nestedParser);
    ZrCore_Memory_RawFreeWithType(ps->state->global,
                            sourceCopy,
                            sourceLength + 1,
                            ZR_MEMORY_NATIVE_TYPE_STRING);
    return expression;
}

static TZrBool append_template_static_segment(SZrParserState *ps,
                                            SZrAstNodeArray *segments,
                                            const TZrChar *text,
                                            TZrSize length) {
    SZrString *segmentString;
    SZrAstNode *segmentNode;

    if (ps == ZR_NULL || segments == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }

    segmentString = ZrCore_String_Create(ps->state, text, length);
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

static SZrAstNode *parse_template_string_literal(SZrParserState *ps, SZrString *rawValue) {
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

            if (!append_template_static_segment(ps,
                                                segments,
                                                rawText + segmentStart,
                                                index - segmentStart)) {
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

    if (!append_template_static_segment(ps,
                                        segments,
                                        rawText + segmentStart,
                                        rawLength - segmentStart)) {
        ZrParser_AstNodeArray_Free(ps->state, segments);
        return ZR_NULL;
    }

    return create_template_string_literal_node(ps, segments);
}

// ==================== 字面量解析 ====================

static SZrAstNode *parse_literal(SZrParserState *ps) {
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
            SZrString *literal = ps->lexer->t.seminfo.stringValue;  // 注意：这里需要从 token 中获取原始字符串
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
                    TZrSize len = (literal->shortStringLength < ZR_VM_LONG_STRING_FLAG) ?
                                  (TZrSize)literal->shortStringLength :
                                  literal->longStringLength;
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
            // 从词法分析器获取错误信息和原始字符串
            // stringValue已经包含了原始字符串（包括引号）
            // 错误信息可以通过检查字符串是否包含转义错误来判断
            TZrBool hasError = ZR_FALSE;  // TODO: 词法分析器通常会在遇到错误时报告，这里暂时设为false
            SZrString *literal = value;  // 原始字符串已经存储在stringValue中
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
            // 从词法分析器获取错误信息和原始字符串
            // char字面量的原始字符串可以通过stringValue获取（如果lexer存储了）
            TZrBool hasError = ZR_FALSE;  // TODO: 词法分析器通常会在遇到错误时报告，这里暂时设为false
            SZrString *literal = ps->lexer->t.seminfo.stringValue;  // 如果lexer存储了原始字符串，使用它
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
            return create_float_literal_node(ps, INFINITY, literal, ZR_FALSE);  // 正无穷
        }

        case ZR_TK_NEG_INFINITY: {
            ZrParser_Lexer_Next(ps->lexer);
            SZrString *literal = ZrCore_String_Create(ps->state, "NegativeInfinity", 16);
            return create_float_literal_node(ps, -INFINITY, literal, ZR_FALSE);  // 负无穷
        }

        case ZR_TK_NAN: {
            ZrParser_Lexer_Next(ps->lexer);
            SZrString *literal = ZrCore_String_Create(ps->state, "NaN", 3);
            return create_float_literal_node(ps, NAN, literal, ZR_FALSE);  // NaN
        }

        default:
            report_error(ps, "Expected literal");
            return ZR_NULL;
    }
}

static SZrAstNode *parse_identifier(SZrParserState *ps) {
    SZrFileRange identifierLoc;
    // 允许 test 关键字作为标识符（用于方法名等）
    if (ps->lexer->t.token != ZR_TK_IDENTIFIER && ps->lexer->t.token != ZR_TK_TEST) {
        report_error(ps, "Expected identifier");
        return ZR_NULL;
    }

    SZrString *name = ps->lexer->t.seminfo.stringValue;
    if (ps->lexer->t.token == ZR_TK_TEST && name == ZR_NULL) {
        name = ZrCore_String_Create(ps->state, "test", 4);
    }
    identifierLoc = get_current_token_location(ps);
    ZrParser_Lexer_Next(ps->lexer);
    return create_identifier_node_with_location(ps, name, identifierLoc);
}

// ==================== 表达式解析（按优先级从低到高）====================

// 解析数组字面量
static SZrAstNode *parse_array_literal(SZrParserState *ps) {
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
static SZrAstNode *parse_object_literal(SZrParserState *ps) {
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
static SZrAstNodeArray *parse_argument_list(SZrParserState *ps, SZrArray **argNames) {
    SZrAstNodeArray *args = ZrParser_AstNodeArray_New(ps->state, 4);
    if (args == ZR_NULL) {
        if (argNames != ZR_NULL) {
            *argNames = ZR_NULL;
        }
        return ZR_NULL;
    }

    // 初始化参数名数组
    SZrArray *names = ZR_NULL;
    if (argNames != ZR_NULL) {
        *argNames = ZR_NULL;
    }
    TZrBool hasNamedArgs = ZR_FALSE;

    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        // 检查第一个参数是否为命名参数（identifier: expression）
        TZrBool isNamed = ZR_FALSE;
        SZrString *paramName = ZR_NULL;
        
        if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
            // 保存标识符名称
            paramName = ps->lexer->t.seminfo.stringValue;
            EZrToken lookahead = peek_token(ps);
            if (lookahead == ZR_TK_COLON) {
                // 这是命名参数：identifier: expression
                isNamed = ZR_TRUE;
                ZrParser_Lexer_Next(ps->lexer);  // 跳过 identifier
                consume_token(ps, ZR_TK_COLON);  // 跳过 :
            }
        }
        
        if (isNamed) {
            hasNamedArgs = ZR_TRUE;
            // 创建参数名数组
            if (names == ZR_NULL) {
                names = ZrCore_Memory_RawMallocWithType(ps->state->global, sizeof(SZrArray), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                if (names != ZR_NULL) {
                    ZrCore_Array_Init(ps->state, names, sizeof(SZrString*), 4);
                }
            }
            if (names != ZR_NULL) {
                ZrCore_Array_Push(ps->state, names, &paramName);
            }
        } else {
            // 位置参数，参数名为 ZR_NULL
            if (names == ZR_NULL) {
                names = ZrCore_Memory_RawMallocWithType(ps->state->global, sizeof(SZrArray), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                if (names != ZR_NULL) {
                    ZrCore_Array_Init(ps->state, names, sizeof(SZrString*), 4);
                }
            }
            if (names != ZR_NULL) {
                SZrString *nullName = ZR_NULL;
                ZrCore_Array_Push(ps->state, names, &nullName);
            }
        }
        
        // 解析参数值表达式
        SZrAstNode *first = parse_expression(ps);
        if (first != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, args, first);
        } else {
            // 表达式解析失败，清理并返回
            if (names != ZR_NULL) {
                ZrCore_Array_Free(ps->state, names);
                ZrCore_Memory_RawFreeWithType(ps->state->global, names, sizeof(SZrArray), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
            if (argNames != ZR_NULL) {
                *argNames = ZR_NULL;
            }
            return args;  // 返回部分解析的结果
        }

        while (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_RPAREN) {
                break;
            }
            
            // 检查是否为命名参数
            isNamed = ZR_FALSE;
            paramName = ZR_NULL;
            
            if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
                paramName = ps->lexer->t.seminfo.stringValue;
                EZrToken lookahead = peek_token(ps);
                if (lookahead == ZR_TK_COLON) {
                    isNamed = ZR_TRUE;
                    ZrParser_Lexer_Next(ps->lexer);  // 跳过 identifier
                    consume_token(ps, ZR_TK_COLON);  // 跳过 :
                }
            }
            
            if (isNamed) {
                hasNamedArgs = ZR_TRUE;
                if (names == ZR_NULL) {
                    names = ZrCore_Memory_RawMallocWithType(ps->state->global, sizeof(SZrArray), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                    if (names != ZR_NULL) {
                        ZrCore_Array_Init(ps->state, names, sizeof(SZrString*), args->count + 1);
                        // 为之前的位置参数填充 ZR_NULL
                        for (TZrSize i = 0; i < args->count; i++) {
                            SZrString *nullName = ZR_NULL;
                            ZrCore_Array_Push(ps->state, names, &nullName);
                        }
                    }
                }
                if (names != ZR_NULL) {
                    ZrCore_Array_Push(ps->state, names, &paramName);
                }
            } else {
                if (hasNamedArgs) {
                    // 命名参数后不能再有位置参数
                    report_error(ps, "Positional arguments cannot come after named arguments");
                    break;
                }
                if (names == ZR_NULL) {
                    names = ZrCore_Memory_RawMallocWithType(ps->state->global, sizeof(SZrArray), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                    if (names != ZR_NULL) {
                        ZrCore_Array_Init(ps->state, names, sizeof(SZrString*), args->count + 1);
                        // 为之前的位置参数填充 ZR_NULL
                        for (TZrSize i = 0; i < args->count; i++) {
                            SZrString *nullName = ZR_NULL;
                            ZrCore_Array_Push(ps->state, names, &nullName);
                        }
                    }
                }
                if (names != ZR_NULL) {
                    SZrString *nullName = ZR_NULL;
                    ZrCore_Array_Push(ps->state, names, &nullName);
                }
            }
            
            SZrAstNode *arg = parse_expression(ps);
            if (arg != ZR_NULL) {
                ZrParser_AstNodeArray_Add(ps->state, args, arg);
            } else {
                break;
            }
        }
    }

    if (argNames != ZR_NULL) {
        *argNames = names;
    }
    
    return args;
}

static SZrAstNode *append_primary_member(SZrParserState *ps, SZrAstNode *base, SZrAstNode *memberNode, SZrFileRange startLoc) {
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

static TZrBool is_lambda_expression_after_lparen(SZrParserState *ps) {
    TZrSize savedPos = ps->lexer->currentPos;
    TZrInt32 savedChar = ps->lexer->currentChar;
    TZrInt32 savedLine = ps->lexer->lineNumber;
    TZrInt32 savedLastLine = ps->lexer->lastLine;
    SZrToken savedToken = ps->lexer->t;
    SZrToken savedLookahead = ps->lexer->lookahead;
    TZrSize savedLookaheadPos = ps->lexer->lookaheadPos;
    TZrInt32 savedLookaheadChar = ps->lexer->lookaheadChar;
    TZrInt32 savedLookaheadLine = ps->lexer->lookaheadLine;
    TZrInt32 savedLookaheadLastLine = ps->lexer->lookaheadLastLine;
    TZrInt32 depth = 0;
    TZrBool isLambda = ZR_FALSE;

    if (ps->lexer->t.token != ZR_TK_LPAREN) {
        return ZR_FALSE;
    }

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

    if (depth == 0 && ps->lexer->t.token == ZR_TK_RIGHT_ARROW) {
        isLambda = ZR_TRUE;
    }

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
    return isLambda;
}

static TZrBool is_expression_level_using_new(SZrParserState *ps) {
    TZrSize savedPos;
    TZrInt32 savedChar;
    TZrInt32 savedLine;
    TZrInt32 savedLastLine;
    SZrToken savedToken;
    SZrToken savedLookahead;
    TZrSize savedLookaheadPos;
    TZrInt32 savedLookaheadChar;
    TZrInt32 savedLookaheadLine;
    TZrInt32 savedLookaheadLastLine;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_USING) {
        return ZR_FALSE;
    }

    savedPos = ps->lexer->currentPos;
    savedChar = ps->lexer->currentChar;
    savedLine = ps->lexer->lineNumber;
    savedLastLine = ps->lexer->lastLine;
    savedToken = ps->lexer->t;
    savedLookahead = ps->lexer->lookahead;
    savedLookaheadPos = ps->lexer->lookaheadPos;
    savedLookaheadChar = ps->lexer->lookaheadChar;
    savedLookaheadLine = ps->lexer->lookaheadLine;
    savedLookaheadLastLine = ps->lexer->lookaheadLastLine;

    ZrParser_Lexer_Next(ps->lexer);
    if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
        SZrString *name = ps->lexer->t.seminfo.stringValue;
        if (zr_string_equals_literal(name, "unique") ||
            zr_string_equals_literal(name, "shared") ||
            zr_string_equals_literal(name, "share") ||
            zr_string_equals_literal(name, "weak")) {
            ZrParser_Lexer_Next(ps->lexer);
        }
    }

    {
        TZrBool result = ps->lexer->t.token == ZR_TK_NEW;
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
        return result;
    }
}

static SZrAstNodeArray *create_empty_argument_list(SZrParserState *ps) {
    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_AstNodeArray_New(ps->state, 0);
}

static TZrBool reject_named_construct_arguments(SZrParserState *ps, SZrArray *argNames, SZrFileRange location) {
    if (ps == ZR_NULL || argNames == ZR_NULL || argNames->length == 0) {
        return ZR_TRUE;
    }

    for (TZrSize i = 0; i < argNames->length; i++) {
        SZrString **namePtr = (SZrString **)ZrCore_Array_Get(argNames, i);
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

static SZrAstNode *create_prototype_reference_node(SZrParserState *ps, SZrAstNode *target, SZrFileRange location) {
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

static SZrAstNode *create_construct_expression_node(SZrParserState *ps,
                                                    SZrAstNode *target,
                                                    SZrAstNodeArray *args,
                                                    EZrOwnershipQualifier ownershipQualifier,
                                                    TZrBool isUsing,
                                                    TZrBool isNew,
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
    return node;
}

static SZrAstNode *parse_prototype_path_expression(SZrParserState *ps) {
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

        property = parse_identifier(ps);
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

static SZrAstNode *parse_prototype_reference_expression(SZrParserState *ps) {
    SZrFileRange startLoc;
    SZrAstNode *target;
    SZrAstNode *prototypeNode;
    SZrFileRange fullLoc;

    if (ps == ZR_NULL || ps->lexer->t.token != ZR_TK_DOLLAR) {
        return ZR_NULL;
    }

    startLoc = get_current_location(ps);
    ZrParser_Lexer_Next(ps->lexer);

    if (ps->lexer->t.token == ZR_TK_LPAREN) {
        consume_token(ps, ZR_TK_LPAREN);
        target = parse_expression(ps);
        expect_token(ps, ZR_TK_RPAREN);
        consume_token(ps, ZR_TK_RPAREN);
    } else {
        target = parse_prototype_path_expression(ps);
    }

    if (target == ZR_NULL) {
        return ZR_NULL;
    }

    fullLoc = ZrParser_FileRange_Merge(startLoc, get_current_location(ps));
    prototypeNode = create_prototype_reference_node(ps, target, fullLoc);
    if (prototypeNode == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, target);
        return ZR_NULL;
    }

    return prototypeNode;
}

static SZrAstNode *parse_construct_expression(SZrParserState *ps, TZrBool hasUsingPrefix) {
    SZrFileRange startLoc;
    TZrBool isUsing = ZR_FALSE;
    EZrOwnershipQualifier ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    SZrAstNode *target = ZR_NULL;
    SZrAstNodeArray *args = ZR_NULL;
    SZrArray *argNames = ZR_NULL;
    SZrAstNode *constructNode;
    SZrFileRange fullLoc;

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    startLoc = get_current_location(ps);
    if (hasUsingPrefix) {
        if (ps->lexer->t.token != ZR_TK_USING) {
            report_error(ps, "Expected 'using' before ownership-qualified new expression");
            return ZR_NULL;
        }
        isUsing = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
        SZrString *name = ps->lexer->t.seminfo.stringValue;
        if (zr_string_equals_literal(name, "unique")) {
            ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_UNIQUE;
            ZrParser_Lexer_Next(ps->lexer);
        } else if (zr_string_equals_literal(name, "shared")) {
            ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_SHARED;
            ZrParser_Lexer_Next(ps->lexer);
        } else if (zr_string_equals_literal(name, "share")) {
            report_error(ps, "Use 'shared' instead of 'share' in new-expression ownership prefix");
            ZrParser_Lexer_Next(ps->lexer);
        } else if (zr_string_equals_literal(name, "weak")) {
            report_error(ps, "'weak new' is not supported; use 'unique new' or 'shared new'");
            ZrParser_Lexer_Next(ps->lexer);
        }
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
    } else {
        target = parse_prototype_path_expression(ps);
    }

    if (target == ZR_NULL) {
        return ZR_NULL;
    }

    if (consume_token(ps, ZR_TK_LPAREN)) {
        args = parse_argument_list(ps, &argNames);
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

// 解析成员访问和函数调用
static SZrAstNode *parse_member_access(SZrParserState *ps, SZrAstNode *base) {
    SZrFileRange startLoc = base->location;

    while (ZR_TRUE) {
        // 点号成员访问
        if (consume_token(ps, ZR_TK_DOT)) {
            // 期望标识符
            if (ps->lexer->t.token != ZR_TK_IDENTIFIER) {
                const TZrChar *tokenStr = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);
                TZrChar errorMsg[256];
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

            base = append_primary_member(ps, base, memberNode, startLoc);
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

            base = append_primary_member(ps, base, memberNode, startLoc);
        }
        // 函数调用
        else if (consume_token(ps, ZR_TK_LPAREN)) {
            SZrArray *argNames = ZR_NULL;
            SZrAstNodeArray *args = parse_argument_list(ps, &argNames);
            expect_token(ps, ZR_TK_RPAREN);
            consume_token(ps, ZR_TK_RPAREN);

            if (base->type == ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION) {
                SZrAstNode *target = base->data.prototypeReferenceExpression.target;
                SZrAstNode *constructNode;
                SZrFileRange fullLoc;

                if (!reject_named_construct_arguments(ps, argNames, startLoc)) {
                    if (args != ZR_NULL) {
                        ZrParser_AstNodeArray_Free(ps->state, args);
                    }
                    return base;
                }

                base->data.prototypeReferenceExpression.target = ZR_NULL;
                ZrCore_Memory_RawFreeWithType(ps->state->global,
                                              base,
                                              sizeof(SZrAstNode),
                                              ZR_MEMORY_NATIVE_TYPE_ARRAY);

                fullLoc = ZrParser_FileRange_Merge(startLoc, get_current_location(ps));
                constructNode = create_construct_expression_node(ps,
                                                                 target,
                                                                 args,
                                                                 ZR_OWNERSHIP_QUALIFIER_NONE,
                                                                 ZR_FALSE,
                                                                 ZR_FALSE,
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

            SZrAstNode *callNode = create_ast_node(ps, ZR_AST_FUNCTION_CALL, startLoc);
            if (callNode == ZR_NULL) {
                if (args != ZR_NULL) {
                    ZrParser_AstNodeArray_Free(ps->state, args);
                }
                if (argNames != ZR_NULL) {
                    ZrCore_Array_Free(ps->state, argNames);
                    ZrCore_Memory_RawFreeWithType(ps->state->global, argNames, sizeof(SZrArray), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
                return base;
            }
            callNode->data.functionCall.args = args;
            callNode->data.functionCall.argNames = argNames;
            // 检查是否有命名参数
            callNode->data.functionCall.hasNamedArgs = ZR_FALSE;
            if (argNames != ZR_NULL && argNames->length > 0) {
                // 检查是否有非空的参数名
                for (TZrSize i = 0; i < argNames->length; i++) {
                    SZrString **namePtr = (SZrString**)ZrCore_Array_Get(argNames, i);
                    if (namePtr != ZR_NULL && *namePtr != ZR_NULL) {
                        callNode->data.functionCall.hasNamedArgs = ZR_TRUE;
                        break;
                    }
                }
            }

            base = append_primary_member(ps, base, callNode, startLoc);
        }
        else {
            break;
        }
    }

    return base;
}

// 解析主表达式
static SZrAstNode *parse_primary_expression(SZrParserState *ps) {
    ZR_UNUSED_PARAMETER(get_current_location(ps));
    EZrToken token = ps->lexer->t.token;
    SZrAstNode *base = ZR_NULL;

    // 字面量
    if (token == ZR_TK_BOOLEAN || token == ZR_TK_INTEGER || token == ZR_TK_FLOAT ||
        token == ZR_TK_STRING || token == ZR_TK_TEMPLATE_STRING || token == ZR_TK_CHAR || token == ZR_TK_NULL ||
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
            // 更精确的判断逻辑：
            // 1. 如果下一个token是标识符、字符串或数字，可能是对象字面量的键
            // 2. 如果下一个token是语句关键字（var, if, while等），是块表达式
            // 3. 如果下一个token是右大括号，是空对象字面量
            EZrToken lookahead = peek_token(ps);
            if (lookahead == ZR_TK_IDENTIFIER || lookahead == ZR_TK_STRING || 
                lookahead == ZR_TK_INTEGER || lookahead == ZR_TK_FLOAT ||
                lookahead == ZR_TK_RBRACE) {
                // 可能是对象字面量
                base = parse_object_literal(ps);
            } else if (lookahead == ZR_TK_VAR || lookahead == ZR_TK_IF || 
                       lookahead == ZR_TK_WHILE || lookahead == ZR_TK_FOR ||
                       lookahead == ZR_TK_RETURN || lookahead == ZR_TK_BREAK ||
                       lookahead == ZR_TK_CONTINUE || lookahead == ZR_TK_THROW ||
                       lookahead == ZR_TK_TRY || lookahead == ZR_TK_SWITCH) {
                // 是块表达式
                base = parse_block(ps);
            } else {
                // 默认尝试解析为对象字面量
                base = parse_object_literal(ps);
            }
        }
    }
    // Lambda 表达式或括号表达式
    else if (token == ZR_TK_LPAREN) {
        if (is_lambda_expression_after_lparen(ps)) {
            SZrFileRange lambdaLoc;
            SZrAstNodeArray *params = ZR_NULL;
            SZrParameter *args = ZR_NULL;

            consume_token(ps, ZR_TK_LPAREN);
            lambdaLoc = get_current_location(ps);

            if (ps->lexer->t.token == ZR_TK_PARAMS) {
                ZrParser_Lexer_Next(ps->lexer);
                {
                    SZrAstNode *argsNode = parse_parameter(ps);
                    if (argsNode != ZR_NULL) {
                        args = &argsNode->data.parameter;
                    }
                }
                params = ZrParser_AstNodeArray_New(ps->state, 0);
            } else {
                params = parse_parameter_list(ps);
                if (consume_token(ps, ZR_TK_COMMA) && ps->lexer->t.token == ZR_TK_PARAMS) {
                    ZrParser_Lexer_Next(ps->lexer);
                    {
                        SZrAstNode *argsNode = parse_parameter(ps);
                        if (argsNode != ZR_NULL) {
                            args = &argsNode->data.parameter;
                        }
                    }
                }
            }

            expect_token(ps, ZR_TK_RPAREN);
            consume_token(ps, ZR_TK_RPAREN);
            expect_token(ps, ZR_TK_RIGHT_ARROW);
            ZrParser_Lexer_Next(ps->lexer);

            {
                SZrAstNode *block = parse_block(ps);
                if (block != ZR_NULL) {
                    SZrFileRange endLoc = get_current_location(ps);
                    SZrFileRange fullLoc = ZrParser_FileRange_Merge(lambdaLoc, endLoc);
                    SZrAstNode *lambdaNode = create_ast_node(ps, ZR_AST_LAMBDA_EXPRESSION, fullLoc);
                    if (lambdaNode != ZR_NULL) {
                        lambdaNode->data.lambdaExpression.params = params;
                        lambdaNode->data.lambdaExpression.args = args;
                        lambdaNode->data.lambdaExpression.block = block;
                        return parse_member_access(ps, lambdaNode);
                    }
                }
            }
        } else {
            consume_token(ps, ZR_TK_LPAREN);
            base = parse_expression(ps);
            expect_token(ps, ZR_TK_RPAREN);
            consume_token(ps, ZR_TK_RPAREN);
        }
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

    // 检查类型转换表达式: <Type> expression
    if (token == ZR_TK_LESS_THAN) {
        // 可能是类型转换，需要向前看以区分泛型类型和类型转换
        // 类型转换: <Type> expression (后面跟着表达式)
        // 泛型类型: Type<...> (在类型解析上下文中)
        // 这里我们尝试解析类型，如果成功且后面跟着表达式，就是类型转换
        SZrFileRange startLoc = get_current_location(ps);
        
        // 保存状态以便回退
        TZrSize savedPos = ps->lexer->currentPos;
        TZrInt32 savedChar = ps->lexer->currentChar;
        TZrInt32 savedLine = ps->lexer->lineNumber;
        TZrInt32 savedLastLine = ps->lexer->lastLine;
        SZrToken savedToken = ps->lexer->t;
        
        // 尝试解析类型
        ZrParser_Lexer_Next(ps->lexer);  // 跳过 <
        SZrType *targetType = parse_type(ps);
        
        if (targetType != ZR_NULL && ps->lexer->t.token == ZR_TK_GREATER_THAN) {
            ZrParser_Lexer_Next(ps->lexer);  // 跳过 >
            
            // 检查后面是否是表达式（不是类型声明上下文）
            // 如果后面是标识符、字面量、一元操作符等，就是类型转换
            EZrToken nextToken = ps->lexer->t.token;
            if (nextToken == ZR_TK_IDENTIFIER || nextToken == ZR_TK_INTEGER || 
                nextToken == ZR_TK_FLOAT || nextToken == ZR_TK_STRING || 
                nextToken == ZR_TK_CHAR || nextToken == ZR_TK_BOOLEAN ||
                nextToken == ZR_TK_NULL || nextToken == ZR_TK_LPAREN ||
                nextToken == ZR_TK_LBRACKET || nextToken == ZR_TK_LBRACE ||
                nextToken == ZR_TK_BANG || nextToken == ZR_TK_TILDE ||
                nextToken == ZR_TK_PLUS || nextToken == ZR_TK_MINUS ||
                nextToken == ZR_TK_DOLLAR || nextToken == ZR_TK_NEW ||
                nextToken == ZR_TK_USING ||
                nextToken == ZR_TK_LESS_THAN) {
                // 是类型转换表达式
                SZrAstNode *expression = parse_unary_expression(ps);  // 递归解析表达式
                
                if (expression != ZR_NULL) {
                    SZrFileRange endLoc = get_current_location(ps);
                    SZrFileRange castLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
                    SZrAstNode *node = create_ast_node(ps, ZR_AST_TYPE_CAST_EXPRESSION, castLoc);
                    if (node != ZR_NULL) {
                        node->data.typeCastExpression.targetType = targetType;
                        node->data.typeCastExpression.expression = expression;
                        return node;
                    }
                }
                
                // 如果创建节点失败，释放类型
                if (targetType != ZR_NULL) {
                    ZrCore_Memory_RawFreeWithType(ps->state->global, targetType, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
            } else {
                // 不是类型转换，恢复状态
                ps->lexer->currentPos = savedPos;
                ps->lexer->currentChar = savedChar;
                ps->lexer->lineNumber = savedLine;
                ps->lexer->lastLine = savedLastLine;
                ps->lexer->t = savedToken;
                if (targetType != ZR_NULL) {
                    ZrCore_Memory_RawFreeWithType(ps->state->global, targetType, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
            }
        } else {
            // 解析类型失败，恢复状态
            ps->lexer->currentPos = savedPos;
            ps->lexer->currentChar = savedChar;
            ps->lexer->lineNumber = savedLine;
            ps->lexer->lastLine = savedLastLine;
            ps->lexer->t = savedToken;
            if (targetType != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(ps->state->global, targetType, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
        }
    }

    if (token == ZR_TK_USING && is_expression_level_using_new(ps)) {
        SZrAstNode *node = parse_construct_expression(ps, ZR_TRUE);
        if (node == ZR_NULL) {
            return ZR_NULL;
        }
        return parse_member_access(ps, node);
    }

    if (token == ZR_TK_NEW) {
        SZrAstNode *node = parse_construct_expression(ps, ZR_FALSE);
        if (node == ZR_NULL) {
            return ZR_NULL;
        }
        return parse_member_access(ps, node);
    }

    if (token == ZR_TK_DOLLAR) {
        SZrAstNode *node = parse_prototype_reference_expression(ps);
        if (node == ZR_NULL) {
            return ZR_NULL;
        }
        return parse_member_access(ps, node);
    }

    // 检查一元操作符
    if (token == ZR_TK_BANG || token == ZR_TK_TILDE || token == ZR_TK_PLUS ||
        token == ZR_TK_MINUS) {
        SZrFileRange startLoc = get_current_location(ps);
        SZrUnaryOperator op;
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, token);

        ZrParser_Lexer_Next(ps->lexer);
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
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
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
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
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
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
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
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
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
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
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
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
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
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
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
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);

        ZrParser_Lexer_Next(ps->lexer);
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

        ZrParser_Lexer_Next(ps->lexer);
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

        ZrParser_Lexer_Next(ps->lexer);
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
        op.op = ZrParser_Lexer_TokenToString(ps->lexer, token);

        ZrParser_Lexer_Next(ps->lexer);
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
    SZrAstNodeArray *types = ZrParser_AstNodeArray_New(ps->state, 4);
    if (types == ZR_NULL) {
        return ZR_NULL;
    }

    SZrType *first = parse_type(ps);
    if (first != ZR_NULL) {
        SZrAstNode *firstNode = create_ast_node(ps, ZR_AST_TYPE, get_current_location(ps));
        if (firstNode != ZR_NULL) {
            firstNode->data.type = *first;
            ZrCore_Memory_RawFreeWithType(ps->state->global, first, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            ZrParser_AstNodeArray_Add(ps->state, types, firstNode);
        }
    }

    while (consume_token(ps, ZR_TK_COMMA)) {
        SZrType *type = parse_type(ps);
        if (type != ZR_NULL) {
            SZrAstNode *typeNode = create_ast_node(ps, ZR_AST_TYPE, get_current_location(ps));
            if (typeNode != ZR_NULL) {
                typeNode->data.type = *type;
                ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                ZrParser_AstNodeArray_Add(ps->state, types, typeNode);
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
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNodeArray *params = parse_type_list(ps);
    if (params == ZR_NULL) {
        return ZR_NULL;
    }

    if (!consume_type_closing_angle(ps)) {
        expect_token(ps, ZR_TK_GREATER_THAN);
        ZrParser_Lexer_Next(ps->lexer);
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange genericLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_GENERIC_TYPE, genericLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, params);
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
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNodeArray *elements = parse_type_list(ps);
    if (elements == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_RBRACKET);
    ZrParser_Lexer_Next(ps->lexer);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange tupleLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_TUPLE_TYPE, tupleLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, elements);
        return ZR_NULL;
    }

    node->data.tupleType.elements = elements;
    return node;
}

// 解析类型
static SZrType *parse_type(SZrParserState *ps) {
    SZrType *type = ZrCore_Memory_RawMallocWithType(ps->state->global, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    EZrOwnershipQualifier ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    if (type == ZR_NULL) {
        return ZR_NULL;
    }

    type->dimensions = 0;
    type->name = ZR_NULL;
    type->subType = ZR_NULL;
    type->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    
    // 初始化数组大小约束
    type->arrayFixedSize = 0;
    type->arrayMinSize = 0;
    type->arrayMaxSize = 0;
    type->hasArraySizeConstraint = ZR_FALSE;
    type->arraySizeExpression = ZR_NULL;

    // 解析所有权限定符（unique<T> / shared<T> / weak<T>）
    if (ps->lexer->t.token == ZR_TK_IDENTIFIER &&
        try_get_ownership_qualifier(ps->lexer->t.seminfo.stringValue, &ownershipQualifier) &&
        peek_token(ps) == ZR_TK_LESS_THAN) {
        SZrType *innerType;

        ZrParser_Lexer_Next(ps->lexer);
        expect_token(ps, ZR_TK_LESS_THAN);
        ZrParser_Lexer_Next(ps->lexer);

        innerType = parse_type(ps);
        if (innerType == ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            return ZR_NULL;
        }

        if (!consume_type_closing_angle(ps)) {
            expect_token(ps, ZR_TK_GREATER_THAN);
            consume_token(ps, ZR_TK_GREATER_THAN);
        }

        *type = *innerType;
        type->ownershipQualifier = ownershipQualifier;
        ZrCore_Memory_RawFreeWithType(ps->state->global, innerType, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return type;
    }

    // 解析类型名称（可能是标识符、泛型类型或元组类型）
    if (ps->lexer->t.token == ZR_TK_LBRACKET) {
        // 元组类型
        SZrAstNode *tupleNode = parse_tuple_type(ps);
        if (tupleNode != ZR_NULL) {
            type->name = tupleNode;
        } else {
            ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
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
                ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                return ZR_NULL;
            }
        } else {
            // 普通标识符类型
            SZrAstNode *idNode = parse_identifier(ps);
            if (idNode != ZR_NULL) {
                type->name = idNode;
            } else {
                ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                return ZR_NULL;
            }
        }
    } else {
        report_error(ps, "Expected type name");
        ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return ZR_NULL;
    }

    // 解析子类型（可选，如 A.B）
    if (consume_token(ps, ZR_TK_DOT)) {
        type->subType = parse_type(ps);
    }

    // 解析数组维度和大小约束
    while (consume_token(ps, ZR_TK_LBRACKET)) {
        if (consume_token(ps, ZR_TK_RBRACKET)) {
            // 普通数组维度（无大小约束）
            type->dimensions++;
        } else {
            if (!parse_array_size_constraint(ps, type)) {
                ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                return ZR_NULL;
            }
            if (!consume_token(ps, ZR_TK_RBRACKET)) {
                report_error(ps, "Expected ] after array size constraint");
                ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                return ZR_NULL;
            }
            type->dimensions++;
        }
    }

    return type;
}

// 解析类型（不解析泛型类型，用于 intermediate 声明的返回类型）
static SZrType *parse_type_no_generic(SZrParserState *ps) {
    SZrType *type = ZrCore_Memory_RawMallocWithType(ps->state->global, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    EZrOwnershipQualifier ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    if (type == ZR_NULL) {
        return ZR_NULL;
    }

    type->dimensions = 0;
    type->name = ZR_NULL;
    type->subType = ZR_NULL;
    type->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    
    // 初始化数组大小约束
    type->arrayFixedSize = 0;
    type->arrayMinSize = 0;
    type->arrayMaxSize = 0;
    type->hasArraySizeConstraint = ZR_FALSE;
    type->arraySizeExpression = ZR_NULL;

    // 解析所有权限定符（unique<T> / shared<T> / weak<T>）
    if (ps->lexer->t.token == ZR_TK_IDENTIFIER &&
        try_get_ownership_qualifier(ps->lexer->t.seminfo.stringValue, &ownershipQualifier) &&
        peek_token(ps) == ZR_TK_LESS_THAN) {
        SZrType *innerType;

        ZrParser_Lexer_Next(ps->lexer);
        expect_token(ps, ZR_TK_LESS_THAN);
        ZrParser_Lexer_Next(ps->lexer);

        innerType = parse_type_no_generic(ps);
        if (innerType == ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            return ZR_NULL;
        }

        if (!consume_type_closing_angle(ps)) {
            expect_token(ps, ZR_TK_GREATER_THAN);
            consume_token(ps, ZR_TK_GREATER_THAN);
        }

        *type = *innerType;
        type->ownershipQualifier = ownershipQualifier;
        ZrCore_Memory_RawFreeWithType(ps->state->global, innerType, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return type;
    }

    // 解析类型名称（可能是标识符或元组类型，但不解析泛型类型）
    if (ps->lexer->t.token == ZR_TK_LBRACKET) {
        // 元组类型
        SZrAstNode *tupleNode = parse_tuple_type(ps);
        if (tupleNode != ZR_NULL) {
            type->name = tupleNode;
        } else {
            ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            return ZR_NULL;
        }
    } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
        // 普通标识符类型（不解析泛型类型，即使后面有 <）
        SZrAstNode *idNode = parse_identifier(ps);
        if (idNode != ZR_NULL) {
            type->name = idNode;
        } else {
            ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            return ZR_NULL;
        }
    } else {
        report_error(ps, "Expected type name");
        ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return ZR_NULL;
    }

    // 解析子类型（可选，如 A.B）
    if (consume_token(ps, ZR_TK_DOT)) {
        type->subType = parse_type_no_generic(ps);
    }

    // 解析数组维度和大小约束
    while (consume_token(ps, ZR_TK_LBRACKET)) {
        if (consume_token(ps, ZR_TK_RBRACKET)) {
            // 普通数组维度（无大小约束）
            type->dimensions++;
        } else {
            if (!parse_array_size_constraint(ps, type)) {
                ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                return ZR_NULL;
            }
            if (!consume_token(ps, ZR_TK_RBRACKET)) {
                report_error(ps, "Expected ] after array size constraint");
                ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                return ZR_NULL;
            }
            type->dimensions++;
        }
    }

    return type;
}

// 解析数组大小约束
// 支持语法：
//   [N]      - 固定大小
//   [M..N]   - 范围约束
//   [N..]    - 最小大小
static TZrBool parse_array_size_constraint(SZrParserState *ps, SZrType *type) {
    TZrSize savedPos;
    TZrInt32 savedChar;
    TZrInt32 savedLine;
    TZrInt32 savedLastLine;
    SZrToken savedToken;
    SZrToken savedLookahead;
    TZrSize savedLookaheadPos;
    TZrInt32 savedLookaheadChar;
    TZrInt32 savedLookaheadLine;
    TZrInt32 savedLookaheadLastLine;

    if (ps == ZR_NULL || type == ZR_NULL) {
        return ZR_FALSE;
    }

    savedPos = ps->lexer->currentPos;
    savedChar = ps->lexer->currentChar;
    savedLine = ps->lexer->lineNumber;
    savedLastLine = ps->lexer->lastLine;
    savedToken = ps->lexer->t;
    savedLookahead = ps->lexer->lookahead;
    savedLookaheadPos = ps->lexer->lookaheadPos;
    savedLookaheadChar = ps->lexer->lookaheadChar;
    savedLookaheadLine = ps->lexer->lookaheadLine;
    savedLookaheadLastLine = ps->lexer->lookaheadLastLine;

    if (ps->lexer->t.token == ZR_TK_INTEGER) {
        TZrInt64 firstValue = ps->lexer->t.seminfo.intValue;
        if (firstValue < 0) {
            report_error(ps, "Array size must be non-negative");
            return ZR_FALSE;
        }

        ZrParser_Lexer_Next(ps->lexer);

        if (ps->lexer->t.token == ZR_TK_RBRACKET) {
            type->arrayFixedSize = (TZrSize)firstValue;
            type->arrayMinSize = (TZrSize)firstValue;
            type->arrayMaxSize = (TZrSize)firstValue;
            type->hasArraySizeConstraint = ZR_TRUE;
            type->arraySizeExpression = ZR_NULL;
            return ZR_TRUE;
        }

        if (consume_token(ps, ZR_TK_DOT_DOT)) {
            type->arrayMinSize = (TZrSize)firstValue;
            type->arrayMaxSize = 0;
            type->hasArraySizeConstraint = ZR_TRUE;
            type->arraySizeExpression = ZR_NULL;

            if (ps->lexer->t.token == ZR_TK_INTEGER) {
                TZrInt64 secondValue = ps->lexer->t.seminfo.intValue;
                if (secondValue < 0 || secondValue < firstValue) {
                    report_error(ps, "Invalid array size range: max must be >= min");
                    return ZR_FALSE;
                }
                type->arrayMaxSize = (TZrSize)secondValue;
                ZrParser_Lexer_Next(ps->lexer);
            }
            return ZR_TRUE;
        }
    }

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

    type->arrayFixedSize = 0;
    type->arrayMinSize = 0;
    type->arrayMaxSize = 0;
    type->arraySizeExpression = parse_expression(ps);
    if (type->arraySizeExpression == ZR_NULL) {
        report_error(ps, "Expected array size constraint expression");
        return ZR_FALSE;
    }
    type->hasArraySizeConstraint = ZR_TRUE;
    return ZR_TRUE;
}

// 解析泛型声明
static SZrGenericDeclaration *parse_generic_declaration(SZrParserState *ps) {
    expect_token(ps, ZR_TK_LESS_THAN);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNodeArray *params = ZrParser_AstNodeArray_New(ps->state, 4);
    if (params == ZR_NULL) {
        return ZR_NULL;
    }

    // 至少需要一个参数
    SZrAstNode *first = parse_parameter(ps);
    if (first != ZR_NULL) {
        ZrParser_AstNodeArray_Add(ps->state, params, first);
    }

    while (consume_token(ps, ZR_TK_COMMA)) {
        if (ps->lexer->t.token == ZR_TK_GREATER_THAN) {
            break;
        }
        SZrAstNode *param = parse_parameter(ps);
        if (param != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, params, param);
        } else {
            break;
        }
    }

    expect_token(ps, ZR_TK_GREATER_THAN);
    ZrParser_Lexer_Next(ps->lexer);

    SZrGenericDeclaration *generic = ZrCore_Memory_RawMallocWithType(ps->state->global, sizeof(SZrGenericDeclaration), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (generic == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, params);
        return ZR_NULL;
    }

    generic->params = params;
    return generic;
}

// 解析元标识符
static SZrAstNode *parse_meta_identifier(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_AT);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange metaLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

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
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNode *expr = parse_expression(ps);
    if (expr == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_SHARP);
    ZrParser_Lexer_Next(ps->lexer);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange decoratorLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

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
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNodeArray *keys = ZrParser_AstNodeArray_New(ps->state, 4);
    if (keys == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_RBRACE) {
        SZrAstNode *first = parse_identifier(ps);
        if (first != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, keys, first);
        }

        while (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_RBRACE) {
                break;
            }
            SZrAstNode *key = parse_identifier(ps);
            if (key != ZR_NULL) {
                ZrParser_AstNodeArray_Add(ps->state, keys, key);
            } else {
                break;
            }
        }
    }

    expect_token(ps, ZR_TK_RBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange destructuringLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_DESTRUCTURING_OBJECT, destructuringLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, keys);
        return ZR_NULL;
    }

    node->data.destructuringObject.keys = keys;
    return node;
}

// 解析解构数组模式
static SZrAstNode *parse_destructuring_array(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_LBRACKET);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNodeArray *keys = ZrParser_AstNodeArray_New(ps->state, 4);
    if (keys == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_RBRACKET) {
        SZrAstNode *first = parse_identifier(ps);
        if (first != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, keys, first);
        }

        while (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_RBRACKET) {
                break;
            }
            SZrAstNode *key = parse_identifier(ps);
            if (key != ZR_NULL) {
                ZrParser_AstNodeArray_Add(ps->state, keys, key);
            } else {
                break;
            }
        }
    }

    expect_token(ps, ZR_TK_RBRACKET);
    ZrParser_Lexer_Next(ps->lexer);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange destructuringLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_DESTRUCTURING_ARRAY, destructuringLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, keys);
        return ZR_NULL;
    }

    node->data.destructuringArray.keys = keys;
    return node;
}

// 解析访问修饰符
static EZrAccessModifier parse_access_modifier(SZrParserState *ps) {
    EZrToken token = ps->lexer->t.token;
    if (token == ZR_TK_PUB) {
        ZrParser_Lexer_Next(ps->lexer);
        return ZR_ACCESS_PUBLIC;
    } else if (token == ZR_TK_PRI) {
        ZrParser_Lexer_Next(ps->lexer);
        return ZR_ACCESS_PRIVATE;
    } else if (token == ZR_TK_PRO) {
        ZrParser_Lexer_Next(ps->lexer);
        return ZR_ACCESS_PROTECTED;
    }
    return ZR_ACCESS_PRIVATE;  // 默认 private
}

// 解析参数
static SZrAstNode *parse_parameter(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 检查是否是可变参数 (...name: type)
    TZrBool isVariadic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        isVariadic = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }
    
    // 解析 const 关键字（可选）
    TZrBool isConst = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_CONST) {
        isConst = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
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
    node->data.parameter.isConst = isConst;
    return node;
}

// 解析参数列表
static SZrAstNodeArray *parse_parameter_list(SZrParserState *ps) {
    SZrAstNodeArray *params = ZrParser_AstNodeArray_New(ps->state, 4);
    if (params == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token != ZR_TK_RPAREN && ps->lexer->t.token != ZR_TK_PARAMS) {
        SZrAstNode *first = parse_parameter(ps);
        if (first != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, params, first);
        }

        while (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_RPAREN) {
                break;
            }
            SZrAstNode *param = parse_parameter(ps);
            if (param != ZR_NULL) {
                ZrParser_AstNodeArray_Add(ps->state, params, param);
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
    ZrParser_Lexer_Next(ps->lexer);

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
    
    // 解析可见性修饰符（可选，默认 private）
    EZrAccessModifier accessModifier = parse_access_modifier(ps);
    
    expect_token(ps, ZR_TK_VAR);
    ZrParser_Lexer_Next(ps->lexer);

    // 解析 const 关键字（可选）
    TZrBool isConst = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_CONST) {
        isConst = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

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

    // 可选初始值
    SZrAstNode *value = ZR_NULL;
    if (consume_token(ps, ZR_TK_EQUALS)) {
        value = parse_expression(ps);
        if (value == ZR_NULL) {
            // 如果解析表达式失败，尝试错误恢复
            if (ps->hasError) {
                return ZR_NULL;
            }
            // 如果没有错误但返回 NULL，可能是遇到了不支持的语法
            const TZrChar *tokenStr = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);
            TZrChar errorMsg[256];
            snprintf(errorMsg, sizeof(errorMsg), "无法解析表达式（遇到 '%s'）", tokenStr);
            report_error_with_token(ps, errorMsg, ps->lexer->t.token);
            return ZR_NULL;
        }
    }

    // 分号是可选的（在某些情况下）
    // 注意：在检查分号之前，表达式应该已经完全解析（包括成员访问和函数调用）
    if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
        consume_token(ps, ZR_TK_SEMICOLON);
    } else if (ps->lexer->t.token != ZR_TK_EOS && 
               ps->lexer->t.token != ZR_TK_VAR && 
               ps->lexer->t.token != ZR_TK_USING &&
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
        const TZrChar *tokenStr = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);
        TZrChar errorMsg[256];
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
    node->data.variableDeclaration.accessModifier = accessModifier;
    node->data.variableDeclaration.isConst = isConst;
    return node;
}

// 解析函数声明
static SZrAstNode *parse_function_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);

    // 解析装饰器（可选）
    SZrAstNodeArray *decorators = ZrParser_AstNodeArray_New(ps->state, 2);
    while (ps->lexer->t.token == ZR_TK_SHARP) {
        SZrAstNode *decorator = parse_decorator_expression(ps);
        if (decorator != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, decorators, decorator);
        } else {
            break;
        }
    }

    // 解析函数名（不需要function关键字，直接是标识符）
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    SZrFileRange nameLoc = nameNode->location;

    // 解析泛型声明（可选）
    SZrGenericDeclaration *generic = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
        generic = parse_generic_declaration(ps);
    }

    // 解析参数列表
    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);

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
            ZrParser_AstNodeArray_Free(ps->state, decorators);
            return ZR_NULL;
        }
        params = ZrParser_AstNodeArray_New(ps->state, 0);
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
                        ZrParser_AstNodeArray_Free(ps->state, params);
                    }
                    ZrParser_AstNodeArray_Free(ps->state, decorators);
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
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }

    SZrFileRange endLoc = body->location;
    SZrFileRange funcLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_FUNCTION_DECLARATION, funcLoc);
    if (node == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }

    node->data.functionDeclaration.name = name;
    node->data.functionDeclaration.nameLocation = nameLoc;
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
    SZrFileRange startLoc = get_current_token_location(ps);
    SZrFileRange endLoc;
    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNodeArray *statements = ZrParser_AstNodeArray_New(ps->state, 8);
    if (statements == ZR_NULL) {
        report_error(ps, "Failed to allocate statement array");
        return ZR_NULL;
    }

    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *stmt = parse_statement(ps);
        if (stmt != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, statements, stmt);
        } else {
            break;  // 遇到错误
        }
    }

    expect_token(ps, ZR_TK_RBRACE);
    endLoc = get_current_token_location(ps);
    consume_token(ps, ZR_TK_RBRACE);
    SZrFileRange blockLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_BLOCK, blockLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, statements);
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
    ZrParser_Lexer_Next(ps->lexer);

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
    ZrParser_Lexer_Next(ps->lexer);

    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNode *expr = parse_expression(ps);
    if (expr == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_RPAREN);
    ZrParser_Lexer_Next(ps->lexer);

    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNodeArray *cases = ZrParser_AstNodeArray_New(ps->state, 4);
    SZrAstNode *defaultCase = ZR_NULL;

    // 解析 switch cases
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        if (consume_token(ps, ZR_TK_LPAREN)) {
            if (ps->lexer->t.token == ZR_TK_RPAREN) {
                // 默认 case
                ZrParser_Lexer_Next(ps->lexer);
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
                ZrParser_Lexer_Next(ps->lexer);
                SZrAstNode *block = parse_block(ps);
                if (value != ZR_NULL && block != ZR_NULL) {
                    SZrFileRange caseLoc = get_current_location(ps);
                    SZrAstNode *caseNode = create_ast_node(ps, ZR_AST_SWITCH_CASE, caseLoc);
                    if (caseNode != ZR_NULL) {
                        caseNode->data.switchCase.value = value;
                        caseNode->data.switchCase.block = block;
                        ZrParser_AstNodeArray_Add(ps->state, cases, caseNode);
                    }
                }
            }
        } else {
            break;
        }
    }

    expect_token(ps, ZR_TK_RBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange switchLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *switchNode = create_ast_node(ps, ZR_AST_SWITCH_EXPRESSION, switchLoc);
    if (switchNode != ZR_NULL) {
    switchNode->data.switchExpression.expr = expr;
    switchNode->data.switchExpression.cases = cases;
    switchNode->data.switchExpression.defaultCase = defaultCase;
    switchNode->data.switchExpression.isStatement = ZR_TRUE;  // 默认是语句
    return switchNode;
    }
    return ZR_NULL;
}

// 解析 if 表达式/语句
static SZrAstNode *parse_if_expression(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_IF);
    ZrParser_Lexer_Next(ps->lexer);

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
    SZrFileRange ifLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_IF_EXPRESSION, ifLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.ifExpression.condition = condition;
    node->data.ifExpression.thenExpr = thenExpr;
    node->data.ifExpression.elseExpr = elseExpr;
    node->data.ifExpression.isStatement = ZR_TRUE;  // 默认是语句
    return node;
}

// 解析 while 循环
static SZrAstNode *parse_while_loop(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_WHILE);
    ZrParser_Lexer_Next(ps->lexer);

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
    SZrFileRange loopLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_WHILE_LOOP, loopLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.whileLoop.cond = cond;
    node->data.whileLoop.block = block;
    node->data.whileLoop.isStatement = ZR_TRUE;  // 默认是语句
    return node;
}

// 解析 for 循环
static SZrAstNode *parse_for_loop(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_FOR);
    ZrParser_Lexer_Next(ps->lexer);

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
    SZrFileRange loopLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_FOR_LOOP, loopLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.forLoop.init = init;
    node->data.forLoop.cond = cond;
    node->data.forLoop.step = step;
    node->data.forLoop.block = block;
    node->data.forLoop.isStatement = ZR_TRUE;  // 默认是语句
    return node;
}

// 解析 foreach 循环
static SZrAstNode *parse_foreach_loop(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    expect_token(ps, ZR_TK_FOR);
    ZrParser_Lexer_Next(ps->lexer);

    expect_token(ps, ZR_TK_LPAREN);
    consume_token(ps, ZR_TK_LPAREN);

    expect_token(ps, ZR_TK_VAR);
    consume_token(ps, ZR_TK_VAR);

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
    SZrFileRange loopLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_FOREACH_LOOP, loopLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.foreachLoop.pattern = pattern;
    node->data.foreachLoop.typeInfo = typeInfo;
    node->data.foreachLoop.expr = expr;
    node->data.foreachLoop.block = block;
    node->data.foreachLoop.isStatement = ZR_TRUE;  // 默认是语句
    return node;
}

// 解析 break/continue 语句
static SZrAstNode *parse_break_continue_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    TZrBool isBreak = (ps->lexer->t.token == ZR_TK_BREAK);
    ZrParser_Lexer_Next(ps->lexer);

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
    ZrParser_Lexer_Next(ps->lexer);  // 消费 OUT

    SZrAstNode *expr = parse_expression(ps);
    if (expr == ZR_NULL) {
        return ZR_NULL;
    }

    expect_token(ps, ZR_TK_SEMICOLON);
    consume_token(ps, ZR_TK_SEMICOLON);

    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange stmtLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

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
    ZrParser_Lexer_Next(ps->lexer);

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
    ZrParser_Lexer_Next(ps->lexer);

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
    SZrFileRange tryLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_TRY_CATCH_FINALLY_STATEMENT, tryLoc);
    if (node == ZR_NULL) {
        if (catchPattern != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, catchPattern);
        }
        return ZR_NULL;
    }

    node->data.tryCatchFinallyStatement.block = block;
    node->data.tryCatchFinallyStatement.catchPattern = catchPattern;
    node->data.tryCatchFinallyStatement.catchBlock = catchBlock;
    node->data.tryCatchFinallyStatement.finallyBlock = finallyBlock;
    return node;
}

static SZrAstNode *parse_using_statement(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrAstNode *resource = ZR_NULL;
    SZrAstNode *body = ZR_NULL;
    TZrBool isBlockScoped = ZR_FALSE;
    SZrAstNode *node;

    expect_token(ps, ZR_TK_USING);
    ZrParser_Lexer_Next(ps->lexer);

    if (consume_token(ps, ZR_TK_LPAREN)) {
        resource = parse_expression(ps);
        if (resource == ZR_NULL) {
            return ZR_NULL;
        }

        expect_token(ps, ZR_TK_RPAREN);
        consume_token(ps, ZR_TK_RPAREN);

        body = parse_block(ps);
        if (body == ZR_NULL) {
            ZrParser_Ast_Free(ps->state, resource);
            return ZR_NULL;
        }

        isBlockScoped = ZR_TRUE;
    } else {
        resource = parse_expression(ps);
        if (resource == ZR_NULL) {
            return ZR_NULL;
        }

        expect_token(ps, ZR_TK_SEMICOLON);
        consume_token(ps, ZR_TK_SEMICOLON);
    }

    node = create_ast_node(ps, ZR_AST_USING_STATEMENT, startLoc);
    if (node == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, resource);
        ZrParser_Ast_Free(ps->state, body);
        return ZR_NULL;
    }

    node->data.usingStatement.resource = resource;
    node->data.usingStatement.body = body;
    node->data.usingStatement.isBlockScoped = isBlockScoped;
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

        case ZR_TK_USING:
            if (is_expression_level_using_new(ps)) {
                return parse_expression_statement(ps);
            }
            return parse_using_statement(ps);

        case ZR_TK_IF:
            return parse_if_expression(ps);

        case ZR_TK_SWITCH: {
            SZrAstNode *switchNode = parse_switch_expression(ps);
            if (switchNode != ZR_NULL) {
                switchNode->data.switchExpression.isStatement = ZR_TRUE;
            }
            return switchNode;
        }

        case ZR_TK_WHILE: {
            SZrAstNode *whileNode = parse_while_loop(ps);
            if (whileNode != ZR_NULL) {
                whileNode->data.whileLoop.isStatement = ZR_TRUE;
            }
            return whileNode;
        }

        case ZR_TK_FOR: {
            // 判断是 for 还是 foreach
            // FOR ( VAR ... IN ... ) 是 foreach
            // FOR ( ... ; ... ; ... ) 是 for
            // 保存状态以便向前看
            TZrSize savedPos = ps->lexer->currentPos;
            TZrInt32 savedChar = ps->lexer->currentChar;
            TZrInt32 savedLine = ps->lexer->lineNumber;
            TZrInt32 savedLastLine = ps->lexer->lastLine;
            SZrToken savedToken = ps->lexer->t;
            SZrToken savedLookahead = ps->lexer->lookahead;
            TZrSize savedLookaheadPos = ps->lexer->lookaheadPos;
            TZrInt32 savedLookaheadChar = ps->lexer->lookaheadChar;
            TZrInt32 savedLookaheadLine = ps->lexer->lookaheadLine;
            TZrInt32 savedLookaheadLastLine = ps->lexer->lookaheadLastLine;
            
            // 跳过 FOR 和 LPAREN
            ZrParser_Lexer_Next(ps->lexer);  // 消费 FOR
            if (ps->lexer->t.token == ZR_TK_LPAREN) {
                ZrParser_Lexer_Next(ps->lexer);  // 消费 LPAREN
                if (ps->lexer->t.token == ZR_TK_VAR) {
                    // 可能是 foreach，继续检查
                    ZrParser_Lexer_Next(ps->lexer);  // 消费 VAR
                    // 跳过模式（标识符、类型注解等）
                    while (ps->lexer->t.token != ZR_TK_IN && 
                           ps->lexer->t.token != ZR_TK_COLON && 
                           ps->lexer->t.token != ZR_TK_RPAREN &&
                           ps->lexer->t.token != ZR_TK_EOS) {
                        ZrParser_Lexer_Next(ps->lexer);
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

        default:
            // 检查是否是函数声明（identifier(params) { statements} 风格）
            if (token == ZR_TK_IDENTIFIER) {
                // 查看下一个 token 判断是否是函数声明
                EZrToken lookahead = peek_token(ps);
                if (lookahead == ZR_TK_LPAREN || lookahead == ZR_TK_LESS_THAN) {
                    // 可能是函数声明，需要进一步检查后面是否有函数体 { }
                    // 保存状态以便向前看
                    TZrSize savedPos = ps->lexer->currentPos;
                    TZrInt32 savedChar = ps->lexer->currentChar;
                    TZrInt32 savedLine = ps->lexer->lineNumber;
                    TZrInt32 savedLastLine = ps->lexer->lastLine;
                    SZrToken savedToken = ps->lexer->t;
                    SZrToken savedLookahead = ps->lexer->lookahead;
                    TZrSize savedLookaheadPos = ps->lexer->lookaheadPos;
                    TZrInt32 savedLookaheadChar = ps->lexer->lookaheadChar;
                    TZrInt32 savedLookaheadLine = ps->lexer->lookaheadLine;
                    TZrInt32 savedLookaheadLastLine = ps->lexer->lookaheadLastLine;
                    
                    // 跳过标识符和左括号（或泛型）
                    ZrParser_Lexer_Next(ps->lexer);  // 消费标识符
                    if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
                        // 跳过泛型参数
                        while (ps->lexer->t.token != ZR_TK_GREATER_THAN && 
                               ps->lexer->t.token != ZR_TK_EOS) {
                            ZrParser_Lexer_Next(ps->lexer);
                        }
                        if (ps->lexer->t.token == ZR_TK_GREATER_THAN) {
                            ZrParser_Lexer_Next(ps->lexer);  // 消费 >
                        }
                    }
                    if (ps->lexer->t.token == ZR_TK_LPAREN) {
                        ZrParser_Lexer_Next(ps->lexer);  // 消费 (
                        // 跳过参数列表（直到遇到 )）
                        while (ps->lexer->t.token != ZR_TK_RPAREN && 
                               ps->lexer->t.token != ZR_TK_EOS) {
                            ZrParser_Lexer_Next(ps->lexer);
                        }
                        if (ps->lexer->t.token == ZR_TK_RPAREN) {
                            ZrParser_Lexer_Next(ps->lexer);  // 消费 )
                            // 跳过可选的返回类型注解
                            if (ps->lexer->t.token == ZR_TK_COLON) {
                                ZrParser_Lexer_Next(ps->lexer);  // 消费 :
                                // 跳过类型
                                while (ps->lexer->t.token != ZR_TK_LBRACE && 
                                       ps->lexer->t.token != ZR_TK_EOS &&
                                       ps->lexer->t.token != ZR_TK_SEMICOLON) {
                                    ZrParser_Lexer_Next(ps->lexer);
                                }
                            }
                            // 检查下一个 token 是否是函数体 { }
                            if (ps->lexer->t.token == ZR_TK_LBRACE) {
                                // 是函数声明，恢复状态并解析
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
                                SZrAstNode *funcDecl = parse_function_declaration(ps);
                                // 如果解析失败，直接返回 NULL（无论是否有错误）
                                // 因为如果标识符后跟括号和函数体，它应该是函数声明，不应该回退到表达式解析
                                if (funcDecl == ZR_NULL) {
                                    return ZR_NULL;
                                }
                                return funcDecl;
                            }
                        }
                    }
                    // 恢复状态
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
                }
            }
            // 尝试解析为表达式语句
            return parse_expression_statement(ps);
    }
}

// ==================== 顶层解析 ====================

// 解析顶层语句
static SZrAstNode *parse_top_level_statement(SZrParserState *ps) {
    EZrToken token = ps->lexer->t.token;

    // 检查是否是可见性修饰符（pub/pri/pro），后面应该跟 var/struct/class/interface/enum
    if (token == ZR_TK_PUB || token == ZR_TK_PRI || token == ZR_TK_PRO) {
        // 使用 peek_token 查看下一个 token，不消费当前 token
        EZrToken nextToken = peek_token(ps);
        
        // 根据下一个 token 调用相应的解析函数（它们会自己解析可见性修饰符）
        switch (nextToken) {
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
            default:
                // 如果后面不是声明类型，报告错误
                report_error(ps, "Expected declaration after access modifier");
                return ZR_NULL;
        }
    }

    switch (token) {
        case ZR_TK_MODULE:
            return parse_module_declaration(ps);

        case ZR_TK_VAR:
            return parse_variable_declaration(ps);

        case ZR_TK_USING:
            return parse_using_statement(ps);

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

        case ZR_TK_PERCENT:
            // 检查是否是 %compileTime 或 %test
            {
                EZrToken lookahead = peek_token(ps);
                // test 可能是关键字 ZR_TK_TEST 或标识符 ZR_TK_IDENTIFIER
                if (lookahead == ZR_TK_IDENTIFIER || lookahead == ZR_TK_TEST) {
                    // 保存状态以便查看标识符名称
                    TZrSize savedPos = ps->lexer->currentPos;
                    TZrInt32 savedChar = ps->lexer->currentChar;
                    TZrInt32 savedLine = ps->lexer->lineNumber;
                    TZrInt32 savedLastLine = ps->lexer->lastLine;
                    SZrToken savedToken = ps->lexer->t;
                    SZrToken savedLookahead = ps->lexer->lookahead;
                    TZrSize savedLookaheadPos = ps->lexer->lookaheadPos;
                    TZrInt32 savedLookaheadChar = ps->lexer->lookaheadChar;
                    TZrInt32 savedLookaheadLine = ps->lexer->lookaheadLine;
                    TZrInt32 savedLookaheadLastLine = ps->lexer->lookaheadLastLine;
                    
                    // 跳过 % 和查看标识符或关键字
                    ZrParser_Lexer_Next(ps->lexer);  // 跳过 %
                    
                    // 处理 test 关键字（优先处理，因为 test 是关键字）
                    if (ps->lexer->t.token == ZR_TK_TEST) {
                        // test 是关键字，直接解析测试声明
                        // 恢复状态并解析测试声明
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
                        return parse_test_declaration(ps);
                    }
                    
                    // 处理标识符
                    if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
                        SZrString *identName = ps->lexer->t.seminfo.stringValue;
                        if (identName != ZR_NULL) {
                            TZrNativeString nameStr = ZrCore_String_GetNativeString(identName);
                            if (nameStr != ZR_NULL) {
                                if (strcmp(nameStr, "compileTime") == 0) {
                                    // 恢复状态并解析编译期声明
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
                                    return parse_compile_time_declaration(ps);
                                } else if (strcmp(nameStr, "test") == 0) {
                                    // 恢复状态并解析测试声明
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
                                    return parse_test_declaration(ps);
                                }
                            }
                        }
                    }
                    
                    // 恢复状态（标识符不是 "compileTime" 或 "test"）
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
                } else {
                    // lookahead 不是标识符也不是 test 关键字，报告错误并跳过 % token
                    report_error(ps, "Expected identifier or 'test' after '%' (expected 'compileTime' or 'test')");
                    ZrParser_Lexer_Next(ps->lexer);  // 跳过 % token 以避免死循环
                    return ZR_NULL;
                }
                
                // 如果不是 %compileTime 或 %test，报告错误并跳过 % token 以避免死循环
                report_error(ps, "Unknown directive after '%' (expected 'compileTime' or 'test')");
                ZrParser_Lexer_Next(ps->lexer);  // 跳过 % token 以避免死循环
                return ZR_NULL;
            }

        case ZR_TK_INTERMEDIATE:
            return parse_intermediate_statement(ps);
        
        case ZR_TK_IF: {
            SZrAstNode *ifNode = parse_if_expression(ps);
            if (ifNode != ZR_NULL) {
                ifNode->data.ifExpression.isStatement = ZR_TRUE;
            }
            return ifNode;
        }
        
        case ZR_TK_WHILE: {
            SZrAstNode *whileNode = parse_while_loop(ps);
            if (whileNode != ZR_NULL) {
                whileNode->data.whileLoop.isStatement = ZR_TRUE;
            }
            return whileNode;
        }
        
        case ZR_TK_FOR: {
            // 判断是 for 还是 foreach
            // 保存状态以便向前看
            TZrSize savedPos = ps->lexer->currentPos;
            TZrInt32 savedChar = ps->lexer->currentChar;
            TZrInt32 savedLine = ps->lexer->lineNumber;
            TZrInt32 savedLastLine = ps->lexer->lastLine;
            SZrToken savedToken = ps->lexer->t;
            SZrToken savedLookahead = ps->lexer->lookahead;
            TZrSize savedLookaheadPos = ps->lexer->lookaheadPos;
            TZrInt32 savedLookaheadChar = ps->lexer->lookaheadChar;
            TZrInt32 savedLookaheadLine = ps->lexer->lookaheadLine;
            TZrInt32 savedLookaheadLastLine = ps->lexer->lookaheadLastLine;
            
            // 跳过 for 和 (
            ZrParser_Lexer_Next(ps->lexer);
            if (ps->lexer->t.token == ZR_TK_LPAREN) {
                ZrParser_Lexer_Next(ps->lexer);
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
                SZrAstNode *foreachNode = parse_foreach_loop(ps);
                if (foreachNode != ZR_NULL) {
                    foreachNode->data.foreachLoop.isStatement = ZR_TRUE;
                }
                return foreachNode;
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
            SZrAstNode *forNode = parse_for_loop(ps);
            if (forNode != ZR_NULL) {
                forNode->data.forLoop.isStatement = ZR_TRUE;
            }
            return forNode;
        }
        
        case ZR_TK_SWITCH: {
            SZrAstNode *switchNode = parse_switch_expression(ps);
            if (switchNode != ZR_NULL) {
                switchNode->data.switchExpression.isStatement = ZR_TRUE;
            }
            return switchNode;
        }
        
        case ZR_TK_RETURN:
            return parse_return_statement(ps);
        
        case ZR_TK_BREAK:
        case ZR_TK_CONTINUE:
            return parse_break_continue_statement(ps);
        
        case ZR_TK_OUT:
            return parse_out_statement(ps);
        
        case ZR_TK_THROW:
            return parse_throw_statement(ps);
        
        case ZR_TK_TRY:
            return parse_try_catch_finally_statement(ps);

        default:
            // 检查是否是 %compileTime 或 %test 声明
            if (token == ZR_TK_PERCENT) {
                // 查看下一个 token 判断是 compileTime 还是 test
                EZrToken lookahead = peek_token(ps);
                if (lookahead == ZR_TK_IDENTIFIER) {
                    // 保存状态以便查看标识符名称
                    TZrSize savedPos = ps->lexer->currentPos;
                    TZrInt32 savedChar = ps->lexer->currentChar;
                    TZrInt32 savedLine = ps->lexer->lineNumber;
                    TZrInt32 savedLastLine = ps->lexer->lastLine;
                    SZrToken savedToken = ps->lexer->t;
                    SZrToken savedLookahead = ps->lexer->lookahead;
                    TZrSize savedLookaheadPos = ps->lexer->lookaheadPos;
                    TZrInt32 savedLookaheadChar = ps->lexer->lookaheadChar;
                    TZrInt32 savedLookaheadLine = ps->lexer->lookaheadLine;
                    TZrInt32 savedLookaheadLastLine = ps->lexer->lookaheadLastLine;
                    
                    // 跳过 % 和查看标识符
                    ZrParser_Lexer_Next(ps->lexer);  // 跳过 %
                    if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
                        SZrString *identName = ps->lexer->t.seminfo.stringValue;
                        if (identName != ZR_NULL) {
                            TZrNativeString nameStr = ZrCore_String_GetNativeString(identName);
                            if (nameStr != ZR_NULL) {
                                if (strcmp(nameStr, "compileTime") == 0) {
                                    // 恢复状态并解析编译期声明
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
                                    return parse_compile_time_declaration(ps);
                                } else if (strcmp(nameStr, "test") == 0) {
                                    // 恢复状态并解析测试声明
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
                                    return parse_test_declaration(ps);
                                }
                            }
                        }
                    }
                    
                    // 恢复状态
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
                }
                // 如果不是 %compileTime 或 %test，继续其他解析
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
                    // TODO: 暂时先释放装饰器，让类声明解析函数重新解析
                    ZrParser_Ast_Free(ps->state, decorator);
                    return parse_class_declaration(ps);
                } else if (nextToken == ZR_TK_STRUCT) {
                    ZrParser_Ast_Free(ps->state, decorator);
                    return parse_struct_declaration(ps);
                } else if (nextToken == ZR_TK_IDENTIFIER) {
                    // 可能是函数声明
                    EZrToken lookahead = peek_token(ps);
                    if (lookahead == ZR_TK_LPAREN || lookahead == ZR_TK_LESS_THAN) {
                        ZrParser_Ast_Free(ps->state, decorator);
                        return parse_function_declaration(ps);
                    }
                }
                // 如果后面不是声明，则作为表达式语句处理
                // 但装饰器表达式通常不应该单独出现，这里可能需要错误处理
                // TODO: 暂时先返回装饰器作为表达式语句
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
                    // 如果解析失败，直接返回 NULL（无论是否有错误）
                    // 因为如果标识符后跟括号，它应该是函数声明，不应该回退到表达式解析
                    if (funcDecl == ZR_NULL) {
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
    SZrAstNodeArray *statements = ZrParser_AstNodeArray_New(ps->state, 16);
    if (statements == ZR_NULL) {
        report_error(ps, "Failed to allocate statement array");
        return ZR_NULL;
    }

    TZrSize stmtCount = 0;
    TZrSize errorCount = 0;
    const TZrSize MAX_CONSECUTIVE_ERRORS = 10;  // 最多连续错误次数
    
    while (ps->lexer->t.token != ZR_TK_EOS) {
        // 保存错误状态
        ZR_UNUSED_PARAMETER(ps->hasError);
        ZR_UNUSED_PARAMETER(ps->errorMessage);
        
        // 重置错误状态（临时）
        ps->hasError = ZR_FALSE;
        ps->errorMessage = ZR_NULL;
        
        SZrAstNode *stmt = parse_top_level_statement(ps);
        if (stmt != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, statements, stmt);
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
                    if (!ps->suppressErrorOutput) {
                        fprintf(stderr, "  Too many consecutive errors (%zu), stopping parse\n", errorCount);
                    }
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
                        ZrParser_Lexer_Next(ps->lexer);
                        break;
                    }
                    // 如果遇到可能的语句开始关键字，停止跳过
                    if (token == ZR_TK_VAR || token == ZR_TK_STRUCT || token == ZR_TK_CLASS ||
                        token == ZR_TK_USING ||
                        token == ZR_TK_INTERFACE || token == ZR_TK_ENUM ||
                        token == ZR_TK_TEST || token == ZR_TK_INTERMEDIATE ||
                        token == ZR_TK_MODULE || token == ZR_TK_IDENTIFIER) {
                        break;
                    }
                    // 如果遇到 %，需要特殊处理（可能是 %test 或 %compileTime）
                    if (token == ZR_TK_PERCENT) {
                        // 检查下一个 token 是否是标识符
                        EZrToken nextToken = peek_token(ps);
                        if (nextToken == ZR_TK_IDENTIFIER) {
                            // 可能是 %test 或 %compileTime，停止跳过
                            break;
                        } else {
                            // 不是有效的指令，跳过 % token
                            ZrParser_Lexer_Next(ps->lexer);
                            skipCount++;
                            continue;
                        }
                    }
                    // 跳过当前 token
                    ZrParser_Lexer_Next(ps->lexer);
                    skipCount++;
                }
            } else {
                // 没有错误但返回 NULL，可能是遇到了不支持的语法
                EZrToken currentToken = ps->lexer->t.token;
                if (!ps->suppressErrorOutput) {
                    fprintf(stderr, "  Warning: Failed to parse statement %zu (token: %d), skipping\n", stmtCount, currentToken);
                }
                // 尝试跳过当前 token 继续解析
                if (currentToken != ZR_TK_EOS) {
                    // 特殊处理 % token：如果后面不是有效的指令，需要跳过
                    if (currentToken == ZR_TK_PERCENT) {
                        EZrToken nextToken = peek_token(ps);
                        if (nextToken != ZR_TK_IDENTIFIER) {
                            // 不是有效的指令，跳过 % token
                            ZrParser_Lexer_Next(ps->lexer);
                        } else {
                            // 可能是有效的指令，但解析失败了，跳过 % 和标识符
                            ZrParser_Lexer_Next(ps->lexer);  // 跳过 %
                            if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
                                ZrParser_Lexer_Next(ps->lexer);  // 跳过标识符
                            }
                        }
                    } else {
                        ZrParser_Lexer_Next(ps->lexer);
                    }
                }
            }
        }
    }
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange scriptLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_SCRIPT, scriptLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, statements);
        return ZR_NULL;
    }

    node->data.script.moduleName = moduleName;
    node->data.script.statements = statements;
    return node;
}

SZrAstNode *ZrParser_ParseWithState(SZrParserState *ps) {
    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL || ps->hasError) {
        return ZR_NULL;
    }

    return parse_script(ps);
}

// 解析源代码，返回 AST 根节点
SZrAstNode *ZrParser_Parse(SZrState *state, const TZrChar *source, TZrSize sourceLength, SZrString *sourceName) {
    SZrParserState ps;
    SZrAstNode *ast;

    ZrParser_State_Init(&ps, state, source, sourceLength, sourceName);
    ast = ZrParser_ParseWithState(&ps);
    ZrParser_State_Free(&ps);
    return ast;
}

static void free_type_info(SZrState *state, SZrType *type) {
    if (state == ZR_NULL || type == ZR_NULL) {
        return;
    }

    if (type->name != ZR_NULL) {
        ZrParser_Ast_Free(state, type->name);
    }
    if (type->subType != ZR_NULL) {
        free_type_info(state, type->subType);
        ZrCore_Memory_RawFreeWithType(state->global,
                                type->subType,
                                sizeof(SZrType),
                                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (type->arraySizeExpression != ZR_NULL) {
        ZrParser_Ast_Free(state, type->arraySizeExpression);
    }
}

static void free_ast_node_array_with_elements(SZrState *state, SZrAstNodeArray *array) {
    if (state == ZR_NULL || array == ZR_NULL) {
        return;
    }

    for (TZrSize i = 0; i < array->count; i++) {
        ZrParser_Ast_Free(state, array->nodes[i]);
    }

    ZrParser_AstNodeArray_Free(state, array);
}

static void free_identifier_node_from_ptr(SZrState *state, SZrIdentifier *identifier) {
    if (state == ZR_NULL || identifier == ZR_NULL) {
        return;
    }

    SZrAstNode *nameNode = (SZrAstNode *)((char *)identifier - offsetof(SZrAstNode, data.identifier));
    if (nameNode != ZR_NULL && nameNode->type == ZR_AST_IDENTIFIER_LITERAL) {
        ZrParser_Ast_Free(state, nameNode);
    }
}

static void free_parameter_node_from_ptr(SZrState *state, SZrParameter *parameter) {
    if (state == ZR_NULL || parameter == ZR_NULL) {
        return;
    }

    SZrAstNode *parameterNode = (SZrAstNode *)((char *)parameter - offsetof(SZrAstNode, data.parameter));
    if (parameterNode != ZR_NULL && parameterNode->type == ZR_AST_PARAMETER) {
        ZrParser_Ast_Free(state, parameterNode);
    }
}

static void free_owned_type(SZrState *state, SZrType *type) {
    if (state == ZR_NULL || type == ZR_NULL) {
        return;
    }

    free_type_info(state, type);
    ZrCore_Memory_RawFreeWithType(state->global,
                                  type,
                                  sizeof(SZrType),
                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
}

static void free_generic_declaration(SZrState *state, SZrGenericDeclaration *generic) {
    if (state == ZR_NULL || generic == ZR_NULL) {
        return;
    }

    free_ast_node_array_with_elements(state, generic->params);
    ZrCore_Memory_RawFreeWithType(state->global,
                                  generic,
                                  sizeof(SZrGenericDeclaration),
                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
}

// 释放 AST 节点（递归释放所有子节点）
void ZrParser_Ast_Free(SZrState *state, SZrAstNode *node) {
    if (node == ZR_NULL) {
        return;
    }

    // 递归释放所有子节点
    // 根据节点类型释放相应的资源
    switch (node->type) {
        case ZR_AST_SCRIPT: {
            SZrScript *script = &node->data.script;
            if (script->statements != ZR_NULL) {
                for (TZrSize i = 0; i < script->statements->count; i++) {
                    ZrParser_Ast_Free(state, script->statements->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, script->statements);
            }
            break;
        }
        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *func = &node->data.functionDeclaration;
            free_identifier_node_from_ptr(state, func->name);
            free_generic_declaration(state, func->generic);
            free_ast_node_array_with_elements(state, func->params);
            free_parameter_node_from_ptr(state, func->args);
            free_owned_type(state, func->returnType);
            if (func->body != ZR_NULL) {
                ZrParser_Ast_Free(state, func->body);
            }
            free_ast_node_array_with_elements(state, func->decorators);
            break;
        }
        case ZR_AST_TEST_DECLARATION: {
            SZrTestDeclaration *test = &node->data.testDeclaration;
            free_identifier_node_from_ptr(state, test->name);
            free_ast_node_array_with_elements(state, test->params);
            free_parameter_node_from_ptr(state, test->args);
            if (test->body != ZR_NULL) {
                ZrParser_Ast_Free(state, test->body);
            }
            break;
        }
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *var = &node->data.variableDeclaration;
            if (var->pattern != ZR_NULL) {
                ZrParser_Ast_Free(state, var->pattern);
            }
            if (var->value != ZR_NULL) {
                ZrParser_Ast_Free(state, var->value);
            }
            free_owned_type(state, var->typeInfo);
            break;
        }
        case ZR_AST_STRUCT_DECLARATION: {
            SZrStructDeclaration *decl = &node->data.structDeclaration;
            free_identifier_node_from_ptr(state, decl->name);
            free_generic_declaration(state, decl->generic);
            free_ast_node_array_with_elements(state, decl->inherits);
            free_ast_node_array_with_elements(state, decl->members);
            break;
        }
        case ZR_AST_CLASS_DECLARATION: {
            SZrClassDeclaration *decl = &node->data.classDeclaration;
            free_identifier_node_from_ptr(state, decl->name);
            free_generic_declaration(state, decl->generic);
            free_ast_node_array_with_elements(state, decl->inherits);
            free_ast_node_array_with_elements(state, decl->members);
            free_ast_node_array_with_elements(state, decl->decorators);
            break;
        }
        case ZR_AST_STRUCT_FIELD: {
            SZrStructField *field = &node->data.structField;
            free_identifier_node_from_ptr(state, field->name);
            free_owned_type(state, field->typeInfo);
            if (field->init != ZR_NULL) {
                ZrParser_Ast_Free(state, field->init);
            }
            break;
        }
        case ZR_AST_STRUCT_METHOD: {
            SZrStructMethod *method = &node->data.structMethod;
            free_ast_node_array_with_elements(state, method->decorators);
            free_identifier_node_from_ptr(state, method->name);
            free_generic_declaration(state, method->generic);
            free_ast_node_array_with_elements(state, method->params);
            free_parameter_node_from_ptr(state, method->args);
            free_owned_type(state, method->returnType);
            if (method->body != ZR_NULL) {
                ZrParser_Ast_Free(state, method->body);
            }
            break;
        }
        case ZR_AST_STRUCT_META_FUNCTION: {
            SZrStructMetaFunction *meta = &node->data.structMetaFunction;
            free_identifier_node_from_ptr(state, meta->meta);
            free_ast_node_array_with_elements(state, meta->params);
            free_parameter_node_from_ptr(state, meta->args);
            free_owned_type(state, meta->returnType);
            if (meta->body != ZR_NULL) {
                ZrParser_Ast_Free(state, meta->body);
            }
            break;
        }
        case ZR_AST_CLASS_FIELD: {
            SZrClassField *field = &node->data.classField;
            free_ast_node_array_with_elements(state, field->decorators);
            free_identifier_node_from_ptr(state, field->name);
            free_owned_type(state, field->typeInfo);
            if (field->init != ZR_NULL) {
                ZrParser_Ast_Free(state, field->init);
            }
            break;
        }
        case ZR_AST_CLASS_METHOD: {
            SZrClassMethod *method = &node->data.classMethod;
            free_ast_node_array_with_elements(state, method->decorators);
            free_identifier_node_from_ptr(state, method->name);
            free_generic_declaration(state, method->generic);
            free_ast_node_array_with_elements(state, method->params);
            free_parameter_node_from_ptr(state, method->args);
            free_owned_type(state, method->returnType);
            if (method->body != ZR_NULL) {
                ZrParser_Ast_Free(state, method->body);
            }
            break;
        }
        case ZR_AST_CLASS_PROPERTY: {
            SZrClassProperty *property = &node->data.classProperty;
            free_ast_node_array_with_elements(state, property->decorators);
            if (property->modifier != ZR_NULL) {
                ZrParser_Ast_Free(state, property->modifier);
            }
            break;
        }
        case ZR_AST_CLASS_META_FUNCTION: {
            SZrClassMetaFunction *meta = &node->data.classMetaFunction;
            free_identifier_node_from_ptr(state, meta->meta);
            free_ast_node_array_with_elements(state, meta->params);
            free_parameter_node_from_ptr(state, meta->args);
            free_ast_node_array_with_elements(state, meta->superArgs);
            free_owned_type(state, meta->returnType);
            if (meta->body != ZR_NULL) {
                ZrParser_Ast_Free(state, meta->body);
            }
            break;
        }
        case ZR_AST_PARAMETER: {
            SZrParameter *parameter = &node->data.parameter;
            free_identifier_node_from_ptr(state, parameter->name);
            free_owned_type(state, parameter->typeInfo);
            if (parameter->defaultValue != ZR_NULL) {
                ZrParser_Ast_Free(state, parameter->defaultValue);
            }
            break;
        }
        case ZR_AST_DECORATOR_EXPRESSION: {
            SZrDecoratorExpression *decorator = &node->data.decoratorExpression;
            if (decorator->expr != ZR_NULL) {
                ZrParser_Ast_Free(state, decorator->expr);
            }
            break;
        }
        case ZR_AST_PROPERTY_GET: {
            SZrPropertyGet *getter = &node->data.propertyGet;
            free_identifier_node_from_ptr(state, getter->name);
            free_owned_type(state, getter->targetType);
            if (getter->body != ZR_NULL) {
                ZrParser_Ast_Free(state, getter->body);
            }
            break;
        }
        case ZR_AST_PROPERTY_SET: {
            SZrPropertySet *setter = &node->data.propertySet;
            free_identifier_node_from_ptr(state, setter->name);
            free_identifier_node_from_ptr(state, setter->param);
            free_owned_type(state, setter->targetType);
            if (setter->body != ZR_NULL) {
                ZrParser_Ast_Free(state, setter->body);
            }
            break;
        }
        case ZR_AST_BINARY_EXPRESSION: {
            SZrBinaryExpression *expr = &node->data.binaryExpression;
            if (expr->left != ZR_NULL) {
                ZrParser_Ast_Free(state, expr->left);
            }
            if (expr->right != ZR_NULL) {
                ZrParser_Ast_Free(state, expr->right);
            }
            break;
        }
        case ZR_AST_UNARY_EXPRESSION: {
            SZrUnaryExpression *expr = &node->data.unaryExpression;
            if (expr->argument != ZR_NULL) {
                ZrParser_Ast_Free(state, expr->argument);
            }
            break;
        }
        case ZR_AST_FUNCTION_CALL: {
            SZrFunctionCall *call = &node->data.functionCall;
            // 注意：SZrFunctionCall没有callee成员，函数调用在primary expression中处理
            if (call->args != ZR_NULL) {
                for (TZrSize i = 0; i < call->args->count; i++) {
                    ZrParser_Ast_Free(state, call->args->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, call->args);
            }
            break;
        }
        case ZR_AST_ARRAY_LITERAL: {
            SZrArrayLiteral *arr = &node->data.arrayLiteral;
            if (arr->elements != ZR_NULL) {
                for (TZrSize i = 0; i < arr->elements->count; i++) {
                    ZrParser_Ast_Free(state, arr->elements->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, arr->elements);
            }
            break;
        }
        case ZR_AST_OBJECT_LITERAL: {
            SZrObjectLiteral *obj = &node->data.objectLiteral;
            if (obj->properties != ZR_NULL) {
                for (TZrSize i = 0; i < obj->properties->count; i++) {
                    ZrParser_Ast_Free(state, obj->properties->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, obj->properties);
            }
            break;
        }
        case ZR_AST_BLOCK: {
            SZrBlock *block = &node->data.block;
            if (block->body != ZR_NULL) {
                for (TZrSize i = 0; i < block->body->count; i++) {
                    ZrParser_Ast_Free(state, block->body->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, block->body);
            }
            break;
        }
        case ZR_AST_EXPRESSION_STATEMENT: {
            SZrExpressionStatement *exprStmt = &node->data.expressionStatement;
            if (exprStmt->expr != ZR_NULL) {
                ZrParser_Ast_Free(state, exprStmt->expr);
            }
            break;
        }
        case ZR_AST_USING_STATEMENT: {
            SZrUsingStatement *usingStmt = &node->data.usingStatement;
            if (usingStmt->resource != ZR_NULL) {
                ZrParser_Ast_Free(state, usingStmt->resource);
            }
            if (usingStmt->body != ZR_NULL) {
                ZrParser_Ast_Free(state, usingStmt->body);
            }
            break;
        }
        case ZR_AST_IF_EXPRESSION: {
            SZrIfExpression *ifExpr = &node->data.ifExpression;
            if (ifExpr->condition != ZR_NULL) {
                ZrParser_Ast_Free(state, ifExpr->condition);
            }
            if (ifExpr->thenExpr != ZR_NULL) {
                ZrParser_Ast_Free(state, ifExpr->thenExpr);
            }
            if (ifExpr->elseExpr != ZR_NULL) {
                ZrParser_Ast_Free(state, ifExpr->elseExpr);
            }
            break;
        }
        case ZR_AST_CONDITIONAL_EXPRESSION: {
            SZrConditionalExpression *condExpr = &node->data.conditionalExpression;
            if (condExpr->test != ZR_NULL) {
                ZrParser_Ast_Free(state, condExpr->test);
            }
            if (condExpr->consequent != ZR_NULL) {
                ZrParser_Ast_Free(state, condExpr->consequent);
            }
            if (condExpr->alternate != ZR_NULL) {
                ZrParser_Ast_Free(state, condExpr->alternate);
            }
            break;
        }
        case ZR_AST_SWITCH_EXPRESSION: {
            SZrSwitchExpression *switchExpr = &node->data.switchExpression;
            if (switchExpr->expr != ZR_NULL) {
                ZrParser_Ast_Free(state, switchExpr->expr);
            }
            if (switchExpr->cases != ZR_NULL) {
                for (TZrSize i = 0; i < switchExpr->cases->count; i++) {
                    ZrParser_Ast_Free(state, switchExpr->cases->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, switchExpr->cases);
            }
            if (switchExpr->defaultCase != ZR_NULL) {
                ZrParser_Ast_Free(state, switchExpr->defaultCase);
            }
            break;
        }
        case ZR_AST_LAMBDA_EXPRESSION: {
            SZrLambdaExpression *lambda = &node->data.lambdaExpression;
            if (lambda->params != ZR_NULL) {
                for (TZrSize i = 0; i < lambda->params->count; i++) {
                    ZrParser_Ast_Free(state, lambda->params->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, lambda->params);
            }
            if (lambda->block != ZR_NULL) {
                ZrParser_Ast_Free(state, lambda->block);
            }
            break;
        }
        case ZR_AST_PRIMARY_EXPRESSION: {
            SZrPrimaryExpression *primary = &node->data.primaryExpression;
            if (primary->property != ZR_NULL) {
                ZrParser_Ast_Free(state, primary->property);
            }
            if (primary->members != ZR_NULL) {
                for (TZrSize i = 0; i < primary->members->count; i++) {
                    ZrParser_Ast_Free(state, primary->members->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, primary->members);
            }
            break;
        }
        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION: {
            SZrPrototypeReferenceExpression *prototypeRef = &node->data.prototypeReferenceExpression;
            if (prototypeRef->target != ZR_NULL) {
                ZrParser_Ast_Free(state, prototypeRef->target);
            }
            break;
        }
        case ZR_AST_CONSTRUCT_EXPRESSION: {
            SZrConstructExpression *construct = &node->data.constructExpression;
            if (construct->target != ZR_NULL) {
                ZrParser_Ast_Free(state, construct->target);
            }
            if (construct->args != ZR_NULL) {
                for (TZrSize i = 0; i < construct->args->count; i++) {
                    ZrParser_Ast_Free(state, construct->args->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, construct->args);
            }
            break;
        }
        case ZR_AST_MEMBER_EXPRESSION: {
            SZrMemberExpression *member = &node->data.memberExpression;
            if (member->property != ZR_NULL) {
                ZrParser_Ast_Free(state, member->property);
            }
            break;
        }
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            SZrAssignmentExpression *assign = &node->data.assignmentExpression;
            if (assign->left != ZR_NULL) {
                ZrParser_Ast_Free(state, assign->left);
            }
            if (assign->right != ZR_NULL) {
                ZrParser_Ast_Free(state, assign->right);
            }
            break;
        }
        case ZR_AST_KEY_VALUE_PAIR: {
            SZrKeyValuePair *kv = &node->data.keyValuePair;
            if (kv->key != ZR_NULL) {
                ZrParser_Ast_Free(state, kv->key);
            }
            if (kv->value != ZR_NULL) {
                ZrParser_Ast_Free(state, kv->value);
            }
            break;
        }
        case ZR_AST_WHILE_LOOP: {
            SZrWhileLoop *loop = &node->data.whileLoop;
            if (loop->cond != ZR_NULL) {
                ZrParser_Ast_Free(state, loop->cond);
            }
            if (loop->block != ZR_NULL) {
                ZrParser_Ast_Free(state, loop->block);
            }
            break;
        }
        case ZR_AST_FOR_LOOP:
        case ZR_AST_FOREACH_LOOP: {
            // 循环语句有cond和block
            SZrWhileLoop *loop = &node->data.whileLoop;
            if (loop->cond != ZR_NULL) {
                ZrParser_Ast_Free(state, loop->cond);
            }
            if (loop->block != ZR_NULL) {
                ZrParser_Ast_Free(state, loop->block);
            }
            break;
        }
        case ZR_AST_RETURN_STATEMENT: {
            SZrReturnStatement *ret = &node->data.returnStatement;
            if (ret->expr != ZR_NULL) {
                ZrParser_Ast_Free(state, ret->expr);
            }
            break;
        }
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT: {
            SZrTryCatchFinallyStatement *tryStmt = &node->data.tryCatchFinallyStatement;
            if (tryStmt->block != ZR_NULL) {
                ZrParser_Ast_Free(state, tryStmt->block);
            }
            if (tryStmt->catchPattern != ZR_NULL) {
                // catchPattern 是 SZrAstNodeArray *，需要释放数组中的节点
                for (TZrSize i = 0; i < tryStmt->catchPattern->count; i++) {
                    if (tryStmt->catchPattern->nodes[i] != ZR_NULL) {
                        ZrParser_Ast_Free(state, tryStmt->catchPattern->nodes[i]);
                    }
                }
                ZrParser_AstNodeArray_Free(state, tryStmt->catchPattern);
            }
            if (tryStmt->catchBlock != ZR_NULL) {
                ZrParser_Ast_Free(state, tryStmt->catchBlock);
            }
            if (tryStmt->finallyBlock != ZR_NULL) {
                ZrParser_Ast_Free(state, tryStmt->finallyBlock);
            }
            break;
        }
        case ZR_AST_SWITCH_CASE: {
            SZrSwitchCase *switchCase = &node->data.switchCase;
            if (switchCase->value != ZR_NULL) {
                ZrParser_Ast_Free(state, switchCase->value);
            }
            if (switchCase->block != ZR_NULL) {
                ZrParser_Ast_Free(state, switchCase->block);
            }
            break;
        }
        case ZR_AST_SWITCH_DEFAULT: {
            SZrSwitchDefault *switchDefault = &node->data.switchDefault;
            if (switchDefault->block != ZR_NULL) {
                ZrParser_Ast_Free(state, switchDefault->block);
            }
            break;
        }
        case ZR_AST_TEMPLATE_STRING_LITERAL: {
            SZrTemplateStringLiteral *templateLiteral = &node->data.templateStringLiteral;
            if (templateLiteral->segments != ZR_NULL) {
                for (TZrSize i = 0; i < templateLiteral->segments->count; i++) {
                    ZrParser_Ast_Free(state, templateLiteral->segments->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, templateLiteral->segments);
            }
            break;
        }
        case ZR_AST_INTERPOLATED_SEGMENT: {
            SZrInterpolatedSegment *segment = &node->data.interpolatedSegment;
            if (segment->expression != ZR_NULL) {
                ZrParser_Ast_Free(state, segment->expression);
            }
            break;
        }
        case ZR_AST_TYPE: {
            free_type_info(state, &node->data.type);
            break;
        }
        case ZR_AST_GENERIC_TYPE: {
            SZrGenericType *generic = &node->data.genericType;
            if (generic->params != ZR_NULL) {
                for (TZrSize i = 0; i < generic->params->count; i++) {
                    ZrParser_Ast_Free(state, generic->params->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, generic->params);
            }
            if (generic->name != ZR_NULL) {
                SZrAstNode *nameNode =
                    (SZrAstNode *)((char *)generic->name - offsetof(SZrAstNode, data.identifier));
                if (nameNode != ZR_NULL && nameNode->type == ZR_AST_IDENTIFIER_LITERAL) {
                    ZrParser_Ast_Free(state, nameNode);
                }
            }
            break;
        }
        case ZR_AST_TUPLE_TYPE: {
            SZrTupleType *tuple = &node->data.tupleType;
            if (tuple->elements != ZR_NULL) {
                for (TZrSize i = 0; i < tuple->elements->count; i++) {
                    ZrParser_Ast_Free(state, tuple->elements->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, tuple->elements);
            }
            break;
        }
        // 其他节点类型（字面量、标识符等）通常没有子节点，不需要递归释放
        default:
            // TODO: 对于未知节点类型，暂时不释放子节点（避免错误）
            break;
    }

    // 释放节点本身
    ZrCore_Memory_RawFreeWithType(state->global, node, sizeof(SZrAstNode), ZR_MEMORY_NATIVE_TYPE_ARRAY);
}

// 解析结构体字段
static SZrAstNode *parse_struct_field(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);
    
    // 解析 static 关键字（可选）
    TZrBool isStatic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    // 解析 using 关键字（可选，field-scoped 生命周期管理）
    TZrBool isUsingManaged = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_USING) {
        isUsingManaged = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }
    
    // 解析 const 关键字（可选，可以在 var 之前或之后）
    TZrBool isConst = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_CONST) {
        isConst = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }
    
    // var 关键字是可选的（如果已经有 const，可以省略 var）
    if (ps->lexer->t.token == ZR_TK_VAR) {
        ZrParser_Lexer_Next(ps->lexer);
        
        // 如果 var 后面还有 const，也解析它（支持 var const 语法）
        if (ps->lexer->t.token == ZR_TK_CONST) {
            isConst = ZR_TRUE;
            ZrParser_Lexer_Next(ps->lexer);
        }
    } else if (!isConst) {
        // 如果没有 const 也没有 var，期望 var 关键字
        expect_token(ps, ZR_TK_VAR);
        ZrParser_Lexer_Next(ps->lexer);
    }
    
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
    SZrFileRange fieldLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_STRUCT_FIELD, fieldLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    
    node->data.structField.access = access;
    node->data.structField.isStatic = isStatic;
    node->data.structField.isUsingManaged = isUsingManaged;
    node->data.structField.isConst = isConst;
    node->data.structField.name = name;
    node->data.structField.typeInfo = typeInfo;
    node->data.structField.init = init;
    return node;
}

// 解析结构体方法
static SZrAstNode *parse_struct_method(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 解析装饰器（可选）
    SZrAstNodeArray *decorators = ZrParser_AstNodeArray_New(ps->state, 2);
    while (ps->lexer->t.token == ZR_TK_SHARP) {
        SZrAstNode *decorator = parse_decorator_expression(ps);
        if (decorator != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, decorators, decorator);
        } else {
            break;
        }
    }
    
    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);
    
    // 解析 static 关键字（可选）
    TZrBool isStatic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    // 解析方法名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
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
    ZrParser_Lexer_Next(ps->lexer);
    
    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL;
    
    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        SZrAstNode *argsNode = parse_parameter(ps);
        if (argsNode != ZR_NULL) {
            args = &argsNode->data.parameter;
        }
        params = ZrParser_AstNodeArray_New(ps->state, 0);
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
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange methodLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_STRUCT_METHOD, methodLoc);
    if (node == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        ZrParser_AstNodeArray_Free(ps->state, decorators);
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
    TZrBool isStatic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }
    
    // 期望 @ 符号
    expect_token(ps, ZR_TK_AT);
    ZrParser_Lexer_Next(ps->lexer);
    
    // 解析元标识符（@ 后面跟小写蛇形标识符）
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *meta = &nameNode->data.identifier;
    
    // 解析参数列表
    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);
    
    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL;
    
    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        SZrAstNode *argsNode = parse_parameter(ps);
        if (argsNode != ZR_NULL) {
            args = &argsNode->data.parameter;
        }
        params = ZrParser_AstNodeArray_New(ps->state, 0);
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
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        return ZR_NULL;
    }
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange metaLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_STRUCT_META_FUNCTION, metaLoc);
    if (node == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
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
    
    // 解析可见性修饰符（可选，默认 private）
    EZrAccessModifier accessModifier = parse_access_modifier(ps);
    
    // 期望 struct 关键字
    expect_token(ps, ZR_TK_STRUCT);
    ZrParser_Lexer_Next(ps->lexer);
    
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
    
    // TODO: 解析继承列表（可选，但注释说 struct 不允许继承，所以这里暂时不支持）
    SZrAstNodeArray *inherits = ZrParser_AstNodeArray_New(ps->state, 0);
    
    // 期望左大括号
    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);
    
    // 解析成员列表
    SZrAstNodeArray *members = ZrParser_AstNodeArray_New(ps->state, 8);
    if (members == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, inherits);
        return ZR_NULL;
    }
    
    // 解析成员直到遇到右大括号
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *member = ZR_NULL;
        
        // 检查是否是字段（以 var 开头，可能前面有访问修饰符、static 或 const）
        EZrToken token = ps->lexer->t.token;
        if (token == ZR_TK_PUB || token == ZR_TK_PRI || token == ZR_TK_PRO || 
            token == ZR_TK_STATIC || token == ZR_TK_CONST || token == ZR_TK_USING ||
            token == ZR_TK_VAR) {
            // 可能是字段，尝试解析
            // 需要向前看一个 token 来确定
            TZrSize savedPos = ps->lexer->currentPos;
            TZrInt32 savedChar = ps->lexer->currentChar;
            TZrInt32 savedLine = ps->lexer->lineNumber;
            TZrInt32 savedLastLine = ps->lexer->lastLine;
            SZrToken savedToken = ps->lexer->t;
            SZrToken savedLookahead = ps->lexer->lookahead;
            TZrSize savedLookaheadPos = ps->lexer->lookaheadPos;
            TZrInt32 savedLookaheadChar = ps->lexer->lookaheadChar;
            TZrInt32 savedLookaheadLine = ps->lexer->lookaheadLine;
            TZrInt32 savedLookaheadLastLine = ps->lexer->lookaheadLastLine;
            
            // 跳过访问修饰符、static、using 和 const（用于 lookahead）
            while (ps->lexer->t.token == ZR_TK_PUB || ps->lexer->t.token == ZR_TK_PRI || 
                   ps->lexer->t.token == ZR_TK_PRO || ps->lexer->t.token == ZR_TK_STATIC ||
                   ps->lexer->t.token == ZR_TK_USING || ps->lexer->t.token == ZR_TK_CONST) {
                ZrParser_Lexer_Next(ps->lexer);
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
            ZrParser_AstNodeArray_Add(ps->state, members, member);
        } else {
            // 解析失败，尝试恢复
            break;
        }
    }
    
    // 期望右大括号
    expect_token(ps, ZR_TK_RBRACE);
    consume_token(ps, ZR_TK_RBRACE);
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange structLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_STRUCT_DECLARATION, structLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, inherits);
        ZrParser_AstNodeArray_Free(ps->state, members);
        return ZR_NULL;
    }
    
    node->data.structDeclaration.name = name;
    node->data.structDeclaration.generic = generic;
    node->data.structDeclaration.inherits = inherits;
    node->data.structDeclaration.members = members;
    node->data.structDeclaration.accessModifier = accessModifier;
    return node;
}

// 解析类声明
static SZrAstNode *parse_class_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);
    SZrFileRange endLoc;
    
    // 解析可见性修饰符（可选，默认 private）
    EZrAccessModifier accessModifier = parse_access_modifier(ps);
    
    // 解析装饰器（可选）
    SZrAstNodeArray *decorators = ZrParser_AstNodeArray_New(ps->state, 2);
    while (ps->lexer->t.token == ZR_TK_SHARP) {
        SZrAstNode *decorator = parse_decorator_expression(ps);
        if (decorator != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, decorators, decorator);
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
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    SZrFileRange nameLoc = nameNode->location;
    
    // 解析泛型声明（可选）
    SZrGenericDeclaration *generic = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
        generic = parse_generic_declaration(ps);
    }
    
    // 解析继承列表（可选）
    SZrAstNodeArray *inherits = ZrParser_AstNodeArray_New(ps->state, 0);
    if (consume_token(ps, ZR_TK_COLON)) {
        // 解析类型列表
        if (ps->lexer->t.token != ZR_TK_LBRACE) {
            SZrType *firstType = parse_type(ps);
            if (firstType != ZR_NULL) {
                SZrAstNode *typeNode = create_ast_node(ps, ZR_AST_TYPE, get_current_location(ps));
                if (typeNode != ZR_NULL) {
                    typeNode->data.type = *firstType;
                    ZrCore_Memory_RawFreeWithType(ps->state->global, firstType, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                    ZrParser_AstNodeArray_Add(ps->state, inherits, typeNode);
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
                        ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                        ZrParser_AstNodeArray_Add(ps->state, inherits, typeNode);
                    }
                } else {
                    break;
                }
            }
        }
    }
    
    // 期望左大括号
    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);
    
    // 解析成员列表
    SZrAstNodeArray *members = ZrParser_AstNodeArray_New(ps->state, 8);
    if (members == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        ZrParser_AstNodeArray_Free(ps->state, inherits);
        return ZR_NULL;
    }
    
    // 解析成员直到遇到右大括号
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *member = ZR_NULL;
        
        // 检查成员类型
        EZrToken token = ps->lexer->t.token;
        
        // 检查是否是装饰器或访问修饰符（可能是字段、方法或属性）
        if (token == ZR_TK_SHARP || token == ZR_TK_PUB || token == ZR_TK_PRI || token == ZR_TK_PRO || 
            token == ZR_TK_STATIC || token == ZR_TK_CONST || token == ZR_TK_USING ||
            token == ZR_TK_VAR) {
            // 保存状态以便向前看
            TZrSize savedPos = ps->lexer->currentPos;
            TZrInt32 savedChar = ps->lexer->currentChar;
            TZrInt32 savedLine = ps->lexer->lineNumber;
            TZrInt32 savedLastLine = ps->lexer->lastLine;
            SZrToken savedToken = ps->lexer->t;
            SZrToken savedLookahead = ps->lexer->lookahead;
            TZrSize savedLookaheadPos = ps->lexer->lookaheadPos;
            TZrInt32 savedLookaheadChar = ps->lexer->lookaheadChar;
            TZrInt32 savedLookaheadLine = ps->lexer->lookaheadLine;
            TZrInt32 savedLookaheadLastLine = ps->lexer->lookaheadLastLine;
            
            // 跳过装饰器
            while (ps->lexer->t.token == ZR_TK_SHARP) {
                parse_decorator_expression(ps);
            }
            
            // 跳过访问修饰符和 static（用于 lookahead）
            // 注意：不跳过 const，因为 const 可能是字段的一部分
            while (ps->lexer->t.token == ZR_TK_PUB || ps->lexer->t.token == ZR_TK_PRI || 
                   ps->lexer->t.token == ZR_TK_PRO || ps->lexer->t.token == ZR_TK_STATIC) {
                ZrParser_Lexer_Next(ps->lexer);
            }
            
            // 检查是字段、属性还是方法
            // 如果下一个 token 是 const 或 var，则是字段
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
            
            if (nextToken == ZR_TK_VAR || nextToken == ZR_TK_CONST || nextToken == ZR_TK_USING) {
                // 字段（var 或 const 都可以表示字段）
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
            ZrParser_AstNodeArray_Add(ps->state, members, member);
        } else {
            // 解析失败，尝试恢复
            break;
        }
    }
    
    // 期望右大括号
    expect_token(ps, ZR_TK_RBRACE);
    endLoc = get_current_token_location(ps);
    consume_token(ps, ZR_TK_RBRACE);
    SZrFileRange classLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_CLASS_DECLARATION, classLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        ZrParser_AstNodeArray_Free(ps->state, inherits);
        ZrParser_AstNodeArray_Free(ps->state, members);
        return ZR_NULL;
    }
    
    node->data.classDeclaration.name = name;
    node->data.classDeclaration.nameLocation = nameLoc;
    node->data.classDeclaration.generic = generic;
    node->data.classDeclaration.inherits = inherits;
    node->data.classDeclaration.members = members;
    node->data.classDeclaration.decorators = decorators;
    node->data.classDeclaration.accessModifier = accessModifier;
    return node;
}

// 解析接口字段声明
static SZrAstNode *parse_interface_field_declaration(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    
    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);
    
    // 期望 var 关键字
    expect_token(ps, ZR_TK_VAR);
    ZrParser_Lexer_Next(ps->lexer);
    
    // 解析 const 关键字（可选）
    TZrBool isConst = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_CONST) {
        isConst = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }
    
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
    SZrFileRange fieldLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERFACE_FIELD_DECLARATION, fieldLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    
    node->data.interfaceFieldDeclaration.access = access;
    node->data.interfaceFieldDeclaration.isConst = isConst;
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
    ZrParser_Lexer_Next(ps->lexer);
    
    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL;
    
    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        SZrAstNode *argsNode = parse_parameter(ps);
        if (argsNode != ZR_NULL) {
            args = &argsNode->data.parameter;
        }
        params = ZrParser_AstNodeArray_New(ps->state, 0);
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
    SZrFileRange methodLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERFACE_METHOD_SIGNATURE, methodLoc);
    if (node == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
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
    TZrBool hasGet = ZR_FALSE;
    TZrBool hasSet = ZR_FALSE;
    
    if (ps->lexer->t.token == ZR_TK_GET) {
        hasGet = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
        if (ps->lexer->t.token == ZR_TK_SET) {
            hasSet = ZR_TRUE;
            ZrParser_Lexer_Next(ps->lexer);
        }
    } else if (ps->lexer->t.token == ZR_TK_SET) {
        hasSet = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
        if (ps->lexer->t.token == ZR_TK_GET) {
            hasGet = ZR_TRUE;
            ZrParser_Lexer_Next(ps->lexer);
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
    SZrFileRange propertyLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
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
    ZrParser_Lexer_Next(ps->lexer);
    
    // 解析元标识符
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *meta = &nameNode->data.identifier;
    
    // 解析参数列表
    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);
    
    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL;
    
    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        SZrAstNode *argsNode = parse_parameter(ps);
        if (argsNode != ZR_NULL) {
            args = &argsNode->data.parameter;
        }
        params = ZrParser_AstNodeArray_New(ps->state, 0);
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
    SZrFileRange metaLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERFACE_META_SIGNATURE, metaLoc);
    if (node == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
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
    
    // 解析可见性修饰符（可选，默认 private）
    EZrAccessModifier accessModifier = parse_access_modifier(ps);
    
    // 期望 interface 关键字
    expect_token(ps, ZR_TK_INTERFACE);
    ZrParser_Lexer_Next(ps->lexer);
    
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
    SZrAstNodeArray *inherits = ZrParser_AstNodeArray_New(ps->state, 0);
    if (consume_token(ps, ZR_TK_COLON)) {
        // 解析类型列表
        if (ps->lexer->t.token != ZR_TK_LBRACE) {
            SZrType *firstType = parse_type(ps);
            if (firstType != ZR_NULL) {
                SZrAstNode *typeNode = create_ast_node(ps, ZR_AST_TYPE, get_current_location(ps));
                if (typeNode != ZR_NULL) {
                    typeNode->data.type = *firstType;
                    ZrCore_Memory_RawFreeWithType(ps->state->global, firstType, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                    ZrParser_AstNodeArray_Add(ps->state, inherits, typeNode);
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
                        ZrCore_Memory_RawFreeWithType(ps->state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                        ZrParser_AstNodeArray_Add(ps->state, inherits, typeNode);
                    }
                } else {
                    break;
                }
            }
        }
    }
    
    // 期望左大括号
    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);
    
    // 解析成员列表
    SZrAstNodeArray *members = ZrParser_AstNodeArray_New(ps->state, 8);
    if (members == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, inherits);
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
            TZrInt32 savedChar = ps->lexer->currentChar;
            TZrInt32 savedLine = ps->lexer->lineNumber;
            TZrInt32 savedLastLine = ps->lexer->lastLine;
            SZrToken savedToken = ps->lexer->t;
            SZrToken savedLookahead = ps->lexer->lookahead;
            TZrSize savedLookaheadPos = ps->lexer->lookaheadPos;
            TZrInt32 savedLookaheadChar = ps->lexer->lookaheadChar;
            TZrInt32 savedLookaheadLine = ps->lexer->lookaheadLine;
            TZrInt32 savedLookaheadLastLine = ps->lexer->lookaheadLastLine;
            
            // 跳过访问修饰符
            ZrParser_Lexer_Next(ps->lexer);
            
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
            ZrParser_AstNodeArray_Add(ps->state, members, member);
        } else {
            // 解析失败，尝试恢复
            break;
        }
    }
    
    // 期望右大括号
    expect_token(ps, ZR_TK_RBRACE);
    consume_token(ps, ZR_TK_RBRACE);
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange interfaceLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERFACE_DECLARATION, interfaceLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, inherits);
        ZrParser_AstNodeArray_Free(ps->state, members);
        return ZR_NULL;
    }
    
    node->data.interfaceDeclaration.name = name;
    node->data.interfaceDeclaration.generic = generic;
    node->data.interfaceDeclaration.inherits = inherits;
    node->data.interfaceDeclaration.members = members;
    node->data.interfaceDeclaration.accessModifier = accessModifier;
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
        ZrParser_Lexer_Next(ps->lexer);
    }
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange memberLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
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
    
    // 解析可见性修饰符（可选，默认 private）
    EZrAccessModifier accessModifier = parse_access_modifier(ps);
    
    // 期望 enum 关键字
    expect_token(ps, ZR_TK_ENUM);
    ZrParser_Lexer_Next(ps->lexer);
    
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
    ZrParser_Lexer_Next(ps->lexer);
    
    // 解析成员列表
    SZrAstNodeArray *members = ZrParser_AstNodeArray_New(ps->state, 8);
    if (members == ZR_NULL) {
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
    expect_token(ps, ZR_TK_RBRACE);
    consume_token(ps, ZR_TK_RBRACE);
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange enumLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_ENUM_DECLARATION, enumLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, members);
        return ZR_NULL;
    }
    
    node->data.enumDeclaration.name = name;
    node->data.enumDeclaration.baseType = baseType;
    node->data.enumDeclaration.members = members;
    node->data.enumDeclaration.accessModifier = accessModifier;
    return node;
}

// 解析测试声明
// 语法：%test("test_name") { ... }
static SZrAstNode *parse_test_declaration(SZrParserState *ps) {
    SZrFileRange startLoc;
    
    // 解析 %test
    if (ps->lexer->t.token == ZR_TK_PERCENT) {
        // 保存 % token 的位置信息（在调用 ZrParser_Lexer_Next 之前）
        startLoc = get_current_location(ps);
        TZrInt32 percentLine = startLoc.start.line;
        TZrInt32 percentColumn = startLoc.start.column;
        ZrParser_Lexer_Next(ps->lexer);
        // 期望 "test" 标识符或关键字
        // 注意：test 可能是关键字 ZR_TK_TEST，也可能是标识符 ZR_TK_IDENTIFIER
        if (ps->lexer->t.token == ZR_TK_TEST) {
            // test 是关键字，直接接受
            ZrParser_Lexer_Next(ps->lexer);
        } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
            // test 是标识符，需要检查名称
            SZrString *identName = ps->lexer->t.seminfo.stringValue;
            if (identName == ZR_NULL) {
                report_error(ps, "Expected 'test' after '%'");
                return ZR_NULL;
            }
            TZrNativeString nameStr = ZrCore_String_GetNativeString(identName);
            if (nameStr == ZR_NULL || strcmp(nameStr, "test") != 0) {
                TZrChar errorMsg[256];
                snprintf(errorMsg, sizeof(errorMsg), "Expected 'test' after '%%', but got identifier '%s'", nameStr ? nameStr : "<null>");
                report_error(ps, errorMsg);
                return ZR_NULL;
            }
            ZrParser_Lexer_Next(ps->lexer);
        } else {
            // 使用保存的位置信息报告错误
            const TZrChar *fileName = "<unknown>";
            if (startLoc.source != ZR_NULL) {
                TZrNativeString nameStr = ZrCore_String_GetNativeString(startLoc.source);
                if (nameStr != ZR_NULL) {
                    fileName = nameStr;
                }
            }
            const TZrChar *tokenStr = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);
            if (!ps->suppressErrorOutput) {
                fprintf(stderr, "  [%s:%d:%d] Expected 'test' after '%%' (遇到 token: '%s')\n",
                       fileName, percentLine, percentColumn, tokenStr);
            }
            report_error(ps, "Expected 'test' after '%'");
            return ZR_NULL;
        }
    } else if (ps->lexer->t.token == ZR_TK_TEST) {
        // 兼容旧的语法：test() { ... }
        startLoc = get_current_location(ps);
        ZrParser_Lexer_Next(ps->lexer);
    } else {
        report_error(ps, "Expected '%test' or 'test'");
        return ZR_NULL;
    }

    // 解析测试名称参数：("test_name")
    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);

    // 期望字符串字面量作为测试名
    SZrIdentifier *name = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_STRING) {
        SZrString *testNameStr = ps->lexer->t.seminfo.stringValue;
        ZrParser_Lexer_Next(ps->lexer);
        
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
    SZrFileRange testLoc = ZrParser_FileRange_Merge(startLoc, endLoc);

    SZrAstNode *node = create_ast_node(ps, ZR_AST_TEST_DECLARATION, testLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    node->data.testDeclaration.name = name;
    node->data.testDeclaration.params = ZrParser_AstNodeArray_New(ps->state, 0);  // 测试没有参数列表
    node->data.testDeclaration.args = ZR_NULL;
    node->data.testDeclaration.body = body;
    return node;
}

static TZrBool is_compile_time_function_declaration(SZrParserState *ps) {
    TZrSize savedPos = ps->lexer->currentPos;
    TZrInt32 savedChar = ps->lexer->currentChar;
    TZrInt32 savedLine = ps->lexer->lineNumber;
    TZrInt32 savedLastLine = ps->lexer->lastLine;
    SZrToken savedToken = ps->lexer->t;
    SZrToken savedLookahead = ps->lexer->lookahead;
    TZrSize savedLookaheadPos = ps->lexer->lookaheadPos;
    TZrInt32 savedLookaheadChar = ps->lexer->lookaheadChar;
    TZrInt32 savedLookaheadLine = ps->lexer->lookaheadLine;
    TZrInt32 savedLookaheadLastLine = ps->lexer->lookaheadLastLine;
    TZrInt32 parenDepth = 0;
    TZrBool isFunctionDeclaration = ZR_FALSE;

    if (ps->lexer->t.token != ZR_TK_IDENTIFIER) {
        return ZR_FALSE;
    }

    ZrParser_Lexer_Next(ps->lexer);
    if (ps->lexer->t.token != ZR_TK_LPAREN) {
        goto restore;
    }

    do {
        if (ps->lexer->t.token == ZR_TK_LPAREN) {
            parenDepth++;
        } else if (ps->lexer->t.token == ZR_TK_RPAREN) {
            parenDepth--;
        }
        ZrParser_Lexer_Next(ps->lexer);
    } while (parenDepth > 0 && ps->lexer->t.token != ZR_TK_EOS);

    if (parenDepth == 0 &&
        (ps->lexer->t.token == ZR_TK_LBRACE || ps->lexer->t.token == ZR_TK_COLON)) {
        isFunctionDeclaration = ZR_TRUE;
    }

restore:
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
    return isFunctionDeclaration;
}

// 语法：%compileTime function/variable/statement/expression
static SZrAstNode *parse_compile_time_declaration(SZrParserState *ps) {
    SZrFileRange startLoc;
    
    // 解析 %compileTime
    if (ps->lexer->t.token == ZR_TK_PERCENT) {
        startLoc = get_current_location(ps);
        ZrParser_Lexer_Next(ps->lexer);
        
        // 期望 "compileTime" 标识符
        if (ps->lexer->t.token != ZR_TK_IDENTIFIER) {
            report_error(ps, "Expected 'compileTime' after '%'");
            return ZR_NULL;
        }
        
        SZrString *identName = ps->lexer->t.seminfo.stringValue;
        if (identName == ZR_NULL) {
            report_error(ps, "Expected 'compileTime' after '%'");
            return ZR_NULL;
        }
        
        TZrNativeString nameStr = ZrCore_String_GetNativeString(identName);
        if (nameStr == ZR_NULL || strcmp(nameStr, "compileTime") != 0) {
            TZrChar errorMsg[256];
            snprintf(errorMsg, sizeof(errorMsg), "Expected 'compileTime' after '%%', but got identifier '%s'", nameStr ? nameStr : "<null>");
            report_error(ps, errorMsg);
            return ZR_NULL;
        }
        ZrParser_Lexer_Next(ps->lexer);
    } else {
        report_error(ps, "Expected '%compileTime'");
        return ZR_NULL;
    }
    
    // 根据后续内容判断声明类型
    EZrCompileTimeDeclarationType declType;
    SZrAstNode *declaration = ZR_NULL;
    
    if (ps->lexer->t.token == ZR_TK_VAR) {
        // 编译期变量声明：%compileTime var name = value;
        declType = ZR_COMPILE_TIME_VARIABLE;
        declaration = parse_variable_declaration(ps);
    } else if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
        if (is_compile_time_function_declaration(ps)) {
            // 函数声明：%compileTime functionName(...) { ... }
            declType = ZR_COMPILE_TIME_FUNCTION;
            declaration = parse_function_declaration(ps);
        } else {
            // 函数调用表达式或其他编译期表达式
            declType = ZR_COMPILE_TIME_EXPRESSION;
            declaration = parse_expression(ps);
        }
    } else if (ps->lexer->t.token == ZR_TK_LBRACE) {
        // 编译期语句块：%compileTime { ... }
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
        return ZR_NULL;
    }
    
    node->data.compileTimeDeclaration.declarationType = declType;
    node->data.compileTimeDeclaration.declaration = declaration;
    return node;
}

// 存根实现：中间代码声明解析
// 解析 Intermediate 指令参数
static SZrAstNode *parse_intermediate_instruction_parameter(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_location(ps);
    SZrString *value = ZR_NULL;
    
    if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
        value = ps->lexer->t.seminfo.stringValue;
        ZrParser_Lexer_Next(ps->lexer);
    } else if (ps->lexer->t.token == ZR_TK_INTEGER) {
        // 整数（包括十进制、十六进制和八进制）都使用 ZR_TK_INTEGER
        value = ps->lexer->t.seminfo.stringValue;  // 获取原始字符串
        ZrParser_Lexer_Next(ps->lexer);
    } else {
        report_error(ps, "Expected identifier or number in intermediate instruction parameter");
        return ZR_NULL;
    }
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange paramLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
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
    SZrAstNodeArray *values = ZrParser_AstNodeArray_New(ps->state, 4);
    if (values == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 解析指令参数，直到遇到分号
    // lexer 已经跳过了空白，所以直接解析参数即可
    while (ps->lexer->t.token != ZR_TK_SEMICOLON && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *param = parse_intermediate_instruction_parameter(ps);
        if (param != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, values, param);
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
    SZrFileRange instLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERMEDIATE_INSTRUCTION, instLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, values);
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
    SZrFileRange constLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
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
        params = ZrParser_AstNodeArray_New(ps->state, 0);
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
    SZrAstNodeArray *closures = ZrParser_AstNodeArray_New(ps->state, 0);
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
                    ZrParser_AstNodeArray_Add(ps->state, closures, first);
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
                        ZrParser_AstNodeArray_Add(ps->state, closures, param);
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
    SZrAstNodeArray *constants = ZrParser_AstNodeArray_New(ps->state, 0);
    if (consume_token(ps, ZR_TK_LBRACKET)) {
        while (ps->lexer->t.token != ZR_TK_RBRACKET && ps->lexer->t.token != ZR_TK_EOS) {
            SZrAstNode *constant = parse_intermediate_constant(ps);
            if (constant != ZR_NULL) {
                ZrParser_AstNodeArray_Add(ps->state, constants, constant);
            } else {
                break;
            }
        }
        expect_token(ps, ZR_TK_RBRACKET);
        consume_token(ps, ZR_TK_RBRACKET);
    }
    
    // 解析局部变量（可选）( ... )
    SZrAstNodeArray *locals = ZrParser_AstNodeArray_New(ps->state, 0);
    if (consume_token(ps, ZR_TK_LPAREN)) {
        if (ps->lexer->t.token != ZR_TK_RPAREN) {
            SZrAstNodeArray *localParams = parse_parameter_list(ps);
            if (localParams != ZR_NULL) {
                ZrParser_AstNodeArray_Free(ps->state, locals);
                locals = localParams;
            }
        }
        expect_token(ps, ZR_TK_RPAREN);
        consume_token(ps, ZR_TK_RPAREN);
    }
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange declLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERMEDIATE_DECLARATION, declLoc);
    if (node == ZR_NULL) {
        if (params != ZR_NULL) ZrParser_AstNodeArray_Free(ps->state, params);
        if (closures != ZR_NULL) ZrParser_AstNodeArray_Free(ps->state, closures);
        if (constants != ZR_NULL) ZrParser_AstNodeArray_Free(ps->state, constants);
        if (locals != ZR_NULL) ZrParser_AstNodeArray_Free(ps->state, locals);
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
    ZrParser_Lexer_Next(ps->lexer);
    
    // 解析声明
    SZrAstNode *declaration = parse_intermediate_declaration(ps);
    if (declaration == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 解析指令块
    expect_token(ps, ZR_TK_LBRACE);
    consume_token(ps, ZR_TK_LBRACE);
    
    SZrAstNodeArray *instructions = ZrParser_AstNodeArray_New(ps->state, 8);
    if (instructions == ZR_NULL) {
        return ZR_NULL;
    }
    
    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrAstNode *instruction = parse_intermediate_instruction(ps);
        if (instruction != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, instructions, instruction);
        } else {
            break;
        }
    }
    
    expect_token(ps, ZR_TK_RBRACE);
    consume_token(ps, ZR_TK_RBRACE);
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange stmtLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_INTERMEDIATE_STATEMENT, stmtLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, instructions);
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
    ZrParser_Lexer_Next(ps->lexer);
    
    // 期望第二个 {
    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);
    
    // 解析块内容（语句列表），不期望 { 和 }
    SZrFileRange blockStartLoc = get_current_location(ps);
    SZrAstNodeArray *statements = ZrParser_AstNodeArray_New(ps->state, 8);
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
    SZrFileRange blockLoc = ZrParser_FileRange_Merge(blockStartLoc, blockEndLoc);
    SZrAstNode *block = create_ast_node(ps, ZR_AST_BLOCK, blockLoc);
    if (block == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, statements);
        return ZR_NULL;
    }
    block->data.block.body = statements;
    block->data.block.isStatement = ZR_FALSE;  // 生成器表达式中的块是表达式
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange generatorLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_GENERATOR_EXPRESSION, generatorLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, statements);
        return ZR_NULL;
    }
    
    node->data.generatorExpression.block = block;
    return node;
}

// 解析类字段
static SZrAstNode *parse_class_field(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);
    SZrFileRange endLoc;
    
    // 解析装饰器（可选）
    SZrAstNodeArray *decorators = ZrParser_AstNodeArray_New(ps->state, 2);
    while (ps->lexer->t.token == ZR_TK_SHARP) {
        SZrAstNode *decorator = parse_decorator_expression(ps);
        if (decorator != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, decorators, decorator);
        } else {
            break;
        }
    }
    
    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);
    
    // 解析 static 关键字（可选）
    TZrBool isStatic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }

    // 解析 using 关键字（可选，field-scoped 生命周期管理）
    TZrBool fieldIsUsingManaged = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_USING) {
        fieldIsUsingManaged = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }
    
    // 解析 const 关键字（可选，可以在 var 之前或之后）
    TZrBool isConst = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_CONST) {
        isConst = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }
    
    // var 关键字是可选的（如果已经有 const，可以省略 var）
    if (ps->lexer->t.token == ZR_TK_VAR) {
        ZrParser_Lexer_Next(ps->lexer);
        
        // 如果 var 后面还有 const，也解析它（支持 var const 语法）
        if (ps->lexer->t.token == ZR_TK_CONST) {
            isConst = ZR_TRUE;
            ZrParser_Lexer_Next(ps->lexer);
        }
    } else if (!isConst) {
        // 如果没有 const 也没有 var，期望 var 关键字
        expect_token(ps, ZR_TK_VAR);
        ZrParser_Lexer_Next(ps->lexer);
    }
    
    // 解析字段名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
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
    
    // 期望分号
    expect_token(ps, ZR_TK_SEMICOLON);
    endLoc = get_current_token_location(ps);
    consume_token(ps, ZR_TK_SEMICOLON);
    SZrFileRange fieldLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_CLASS_FIELD, fieldLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }
    
    node->data.classField.decorators = decorators;
    node->data.classField.access = access;
    node->data.classField.isStatic = isStatic;
    node->data.classField.isUsingManaged = fieldIsUsingManaged;
    node->data.classField.isConst = isConst;
    node->data.classField.name = name;
    node->data.classField.nameLocation = nameLoc;
    node->data.classField.typeInfo = typeInfo;
    node->data.classField.init = init;
    return node;
}

// 解析类方法
static SZrAstNode *parse_class_method(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);
    
    // 解析装饰器（可选）
    SZrAstNodeArray *decorators = ZrParser_AstNodeArray_New(ps->state, 2);
    while (ps->lexer->t.token == ZR_TK_SHARP) {
        SZrAstNode *decorator = parse_decorator_expression(ps);
        if (decorator != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, decorators, decorator);
        } else {
            break;
        }
    }
    
    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);
    
    // 解析 static 关键字（可选）
    TZrBool isStatic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }
    
    // 解析方法名
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }
    SZrIdentifier *name = &nameNode->data.identifier;
    SZrFileRange nameLoc = nameNode->location;
    
    // 解析泛型声明（可选）
    SZrGenericDeclaration *generic = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_LESS_THAN) {
        generic = parse_generic_declaration(ps);
    }
    
    // 解析参数列表
    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);
    
    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL;
    
    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        SZrAstNode *argsNode = parse_parameter(ps);
        if (argsNode != ZR_NULL) {
            args = &argsNode->data.parameter;
        }
        params = ZrParser_AstNodeArray_New(ps->state, 0);
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
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        return ZR_NULL;
    }
    
    SZrFileRange endLoc = body->location;
    SZrFileRange methodLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_CLASS_METHOD, methodLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        return ZR_NULL;
    }
    
    node->data.classMethod.decorators = decorators;
    node->data.classMethod.access = access;
    node->data.classMethod.isStatic = isStatic;
    node->data.classMethod.name = name;
    node->data.classMethod.nameLocation = nameLoc;
    node->data.classMethod.generic = generic;
    node->data.classMethod.params = params;
    node->data.classMethod.args = args;
    node->data.classMethod.returnType = returnType;
    node->data.classMethod.body = body;
    return node;
}

// 解析属性 Getter
static SZrAstNode *parse_property_get(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);
    
    // 期望 get 关键字
    expect_token(ps, ZR_TK_GET);
    ZrParser_Lexer_Next(ps->lexer);
    
    // 解析属性名
    SZrAstNode *nameNode = parse_identifier(ps);
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
    
    // 解析方法体
    SZrAstNode *body = parse_block(ps);
    if (body == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrFileRange endLoc = body->location;
    SZrFileRange getLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_PROPERTY_GET, getLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    
    node->data.propertyGet.name = name;
    node->data.propertyGet.nameLocation = nameLoc;
    node->data.propertyGet.targetType = targetType;
    node->data.propertyGet.body = body;
    return node;
}

// 解析属性 Setter
static SZrAstNode *parse_property_set(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);
    
    // 期望 set 关键字
    expect_token(ps, ZR_TK_SET);
    ZrParser_Lexer_Next(ps->lexer);
    
    // 解析属性名
    SZrAstNode *nameNode = parse_identifier(ps);
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
    
    SZrFileRange endLoc = body->location;
    SZrFileRange setLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_PROPERTY_SET, setLoc);
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    
    node->data.propertySet.name = name;
    node->data.propertySet.nameLocation = nameLoc;
    node->data.propertySet.param = param;
    node->data.propertySet.targetType = targetType;
    node->data.propertySet.body = body;
    return node;
}

// 解析类属性
static SZrAstNode *parse_class_property(SZrParserState *ps) {
    SZrFileRange startLoc = get_current_token_location(ps);
    
    // 解析装饰器（可选）
    SZrAstNodeArray *decorators = ZrParser_AstNodeArray_New(ps->state, 2);
    while (ps->lexer->t.token == ZR_TK_SHARP) {
        SZrAstNode *decorator = parse_decorator_expression(ps);
        if (decorator != ZR_NULL) {
            ZrParser_AstNodeArray_Add(ps->state, decorators, decorator);
        } else {
            break;
        }
    }
    
    // 解析访问修饰符（可选）
    EZrAccessModifier access = parse_access_modifier(ps);
    
    // 解析 static 关键字（可选）
    TZrBool isStatic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }
    
    // 解析 get 或 set
    SZrAstNode *modifier = ZR_NULL;
    if (ps->lexer->t.token == ZR_TK_GET) {
        modifier = parse_property_get(ps);
    } else if (ps->lexer->t.token == ZR_TK_SET) {
        modifier = parse_property_set(ps);
    } else {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        report_error(ps, "Expected 'get' or 'set' for property");
        return ZR_NULL;
    }
    
    if (modifier == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
        return ZR_NULL;
    }
    
    SZrFileRange endLoc = modifier->location;
    SZrFileRange propertyLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_CLASS_PROPERTY, propertyLoc);
    if (node == ZR_NULL) {
        ZrParser_AstNodeArray_Free(ps->state, decorators);
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
    TZrBool isStatic = ZR_FALSE;
    if (ps->lexer->t.token == ZR_TK_STATIC) {
        isStatic = ZR_TRUE;
        ZrParser_Lexer_Next(ps->lexer);
    }
    
    // 期望 @ 符号
    expect_token(ps, ZR_TK_AT);
    ZrParser_Lexer_Next(ps->lexer);
    
    // 解析元标识符
    SZrAstNode *nameNode = parse_identifier(ps);
    if (nameNode == ZR_NULL) {
        return ZR_NULL;
    }
    SZrIdentifier *meta = &nameNode->data.identifier;
    
    // 解析参数列表
    expect_token(ps, ZR_TK_LPAREN);
    ZrParser_Lexer_Next(ps->lexer);
    
    SZrAstNodeArray *params = ZR_NULL;
    SZrParameter *args = ZR_NULL;
    
    if (ps->lexer->t.token == ZR_TK_PARAMS) {
        SZrAstNode *argsNode = parse_parameter(ps);
        if (argsNode != ZR_NULL) {
            args = &argsNode->data.parameter;
        }
        params = ZrParser_AstNodeArray_New(ps->state, 0);
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
    TZrBool hasSuperCall = ZR_FALSE;
    SZrAstNodeArray *superArgs = ZrParser_AstNodeArray_New(ps->state, 0);
    if (consume_token(ps, ZR_TK_SUPER)) {
        hasSuperCall = ZR_TRUE;
        expect_token(ps, ZR_TK_LPAREN);
        ZrParser_Lexer_Next(ps->lexer);
        
        if (ps->lexer->t.token != ZR_TK_RPAREN) {
            SZrAstNode *firstArg = parse_expression(ps);
            if (firstArg != ZR_NULL) {
                ZrParser_AstNodeArray_Add(ps->state, superArgs, firstArg);
            }
            
            while (consume_token(ps, ZR_TK_COMMA)) {
                if (ps->lexer->t.token == ZR_TK_RPAREN) {
                    break;
                }
                SZrAstNode *arg = parse_expression(ps);
                if (arg != ZR_NULL) {
                    ZrParser_AstNodeArray_Add(ps->state, superArgs, arg);
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
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        ZrParser_AstNodeArray_Free(ps->state, superArgs);
        return ZR_NULL;
    }
    
    SZrFileRange endLoc = get_current_location(ps);
    SZrFileRange metaLoc = ZrParser_FileRange_Merge(startLoc, endLoc);
    
    SZrAstNode *node = create_ast_node(ps, ZR_AST_CLASS_META_FUNCTION, metaLoc);
    if (node == ZR_NULL) {
        if (params != ZR_NULL) {
            ZrParser_AstNodeArray_Free(ps->state, params);
        }
        ZrParser_AstNodeArray_Free(ps->state, superArgs);
        return ZR_NULL;
    }
    
    node->data.classMetaFunction.access = access;
    node->data.classMetaFunction.isStatic = isStatic;
    node->data.classMetaFunction.meta = meta;
    node->data.classMetaFunction.params = params;
    node->data.classMetaFunction.args = args;
    node->data.classMetaFunction.hasSuperCall = hasSuperCall;
    node->data.classMetaFunction.superArgs = superArgs;
    node->data.classMetaFunction.returnType = returnType;
    node->data.classMetaFunction.body = body;
    return node;
}
