//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/lexer.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

#include <ctype.h>
#include <string.h>

// Token 信息结构（合并关键字表和名称表）
typedef struct {
    const TZrChar *name;
    EZrToken token;
} SZrTokenInfo;

// Token 信息表（按枚举顺序排列，确保 token == index + ZR_FIRST_RESERVED）
static const SZrTokenInfo zr_token_info[] = {
    // 关键字
    {"module", ZR_TK_MODULE},
    {"struct", ZR_TK_STRUCT},
    {"class", ZR_TK_CLASS},
    {"interface", ZR_TK_INTERFACE},
    {"enum", ZR_TK_ENUM},
    {"test", ZR_TK_TEST},
    {"intermediate", ZR_TK_INTERMEDIATE},
    {"var", ZR_TK_VAR},
    {"using", ZR_TK_USING},
    {"pub", ZR_TK_PUB},
    {"pri", ZR_TK_PRI},
    {"pro", ZR_TK_PRO},
    {"if", ZR_TK_IF},
    {"else", ZR_TK_ELSE},
    {"switch", ZR_TK_SWITCH},
    {"while", ZR_TK_WHILE},
    {"for", ZR_TK_FOR},
    {"break", ZR_TK_BREAK},
    {"continue", ZR_TK_CONTINUE},
    {"return", ZR_TK_RETURN},
    {"super", ZR_TK_SUPER},
    {"new", ZR_TK_NEW},
    {"set", ZR_TK_SET},
    {"get", ZR_TK_GET},
    {"static", ZR_TK_STATIC},
    {"const", ZR_TK_CONST},
    {"in", ZR_TK_IN},
    {"out", ZR_TK_OUT},
    {"throw", ZR_TK_THROW},
    {"try", ZR_TK_TRY},
    {"catch", ZR_TK_CATCH},
    {"finally", ZR_TK_FINALLY},
    {"Infinity", ZR_TK_INFINITY},
    {"NegativeInfinity", ZR_TK_NEG_INFINITY},
    {"NaN", ZR_TK_NAN},
    // 操作符
    {"...", ZR_TK_PARAMS},
    {"?", ZR_TK_QUESTIONMARK},
    {":", ZR_TK_COLON},
    {";", ZR_TK_SEMICOLON},
    {",", ZR_TK_COMMA},
    {".", ZR_TK_DOT},
    {"~", ZR_TK_TILDE},
    {"@", ZR_TK_AT},
    {"#", ZR_TK_SHARP},
    {"$", ZR_TK_DOLLAR},
    {"(", ZR_TK_LPAREN},
    {")", ZR_TK_RPAREN},
    {"{", ZR_TK_LBRACE},
    {"}", ZR_TK_RBRACE},
    {"[", ZR_TK_LBRACKET},
    {"]", ZR_TK_RBRACKET},
    {"=", ZR_TK_EQUALS},
    {"+=", ZR_TK_PLUS_EQUALS},
    {"-=", ZR_TK_MINUS_EQUALS},
    {"*=", ZR_TK_STAR_EQUALS},
    {"/=", ZR_TK_SLASH_EQUALS},
    {"%=", ZR_TK_PERCENT_EQUALS},
    {"==", ZR_TK_DOUBLE_EQUALS},
    {"!=", ZR_TK_BANG_EQUALS},
    {"!", ZR_TK_BANG},
    {"<", ZR_TK_LESS_THAN},
    {"<=", ZR_TK_LESS_THAN_EQUALS},
    {">", ZR_TK_GREATER_THAN},
    {">=", ZR_TK_GREATER_THAN_EQUALS},
    {"+", ZR_TK_PLUS},
    {"-", ZR_TK_MINUS},
    {"*", ZR_TK_STAR},
    {"/", ZR_TK_SLASH},
    {"%", ZR_TK_PERCENT},
    {"&&", ZR_TK_AMPERSAND_AMPERSAND},
    {"||", ZR_TK_PIPE_PIPE},
    {"=>", ZR_TK_RIGHT_ARROW},
    {"<<", ZR_TK_LEFT_SHIFT},
    {">>", ZR_TK_RIGHT_SHIFT},
    {"|", ZR_TK_OR},
    {"^", ZR_TK_XOR},
    {"&", ZR_TK_AND},
    // 字面量
    {"<boolean>", ZR_TK_BOOLEAN},
    {"<integer>", ZR_TK_INTEGER},
    {"<float>", ZR_TK_FLOAT},
    {"<string>", ZR_TK_STRING},
    {"<template-string>", ZR_TK_TEMPLATE_STRING},
    {"<char>", ZR_TK_CHAR},
    {"<null>", ZR_TK_NULL},
    {"<identifier>", ZR_TK_IDENTIFIER},
    // 特殊
    {"<eos>", ZR_TK_EOS},
};

// 关键字表（用于词法分析器查找关键字）
static const struct {
    const TZrChar *name;
    EZrToken token;
} zr_keywords[] = {{"module", ZR_TK_MODULE},
                   {"struct", ZR_TK_STRUCT},
                   {"class", ZR_TK_CLASS},
                   {"interface", ZR_TK_INTERFACE},
                   {"enum", ZR_TK_ENUM},
                   {"test", ZR_TK_TEST},
                   {"intermediate", ZR_TK_INTERMEDIATE},
                   {"var", ZR_TK_VAR},
                   {"using", ZR_TK_USING},
                   {"pub", ZR_TK_PUB},
                   {"pri", ZR_TK_PRI},
                   {"pro", ZR_TK_PRO},
                   {"if", ZR_TK_IF},
                   {"else", ZR_TK_ELSE},
                   {"switch", ZR_TK_SWITCH},
                   {"while", ZR_TK_WHILE},
                   {"for", ZR_TK_FOR},
                   {"break", ZR_TK_BREAK},
                   {"continue", ZR_TK_CONTINUE},
                   {"return", ZR_TK_RETURN},
                   {"super", ZR_TK_SUPER},
                   {"new", ZR_TK_NEW},
                   {"set", ZR_TK_SET},
                   {"get", ZR_TK_GET},
                   {"static", ZR_TK_STATIC},
                   {"const", ZR_TK_CONST},
                   {"in", ZR_TK_IN},
                   {"out", ZR_TK_OUT},
                   {"throw", ZR_TK_THROW},
                   {"try", ZR_TK_TRY},
                   {"catch", ZR_TK_CATCH},
                   {"finally", ZR_TK_FINALLY},
                   {"Infinity", ZR_TK_INFINITY},
                   {"NegativeInfinity", ZR_TK_NEG_INFINITY},
                   {"NaN", ZR_TK_NAN},
                   {"true", ZR_TK_BOOLEAN},
                   {"false", ZR_TK_BOOLEAN},
                   {"null", ZR_TK_NULL},
                   {ZR_NULL, ZR_TK_EOS}};

// 辅助函数：获取下一个字符
static TZrInt32 next_char(SZrLexState *ls) {
    if (ls->currentPos >= ls->sourceLength) {
        ls->currentChar = ZR_PARSER_LEXER_EOZ;
        return ZR_PARSER_LEXER_EOZ;
    }

    ls->currentChar = (TZrInt32) (TZrUInt8) ls->source[ls->currentPos++];
    if (ls->currentChar == '\r') {
        if (ls->currentPos < ls->sourceLength && ls->source[ls->currentPos] == '\n') {
            ls->currentPos++;
        }
        ls->currentChar = '\n';
        ls->lineNumber++;
        return ls->currentChar;
    }

    if (ls->currentChar == '\n') {
        ls->lineNumber++;
    }
    return ls->currentChar;
}

// 辅助函数：保存字符到缓冲区
static void save_char(SZrLexState *ls, TZrChar c) {
    if (ls->bufferLength + 1 >= ls->bufferSize) {
        TZrSize newSize = ls->bufferSize * ZR_PARSER_DYNAMIC_CAPACITY_GROWTH_FACTOR;
        TZrChar *newBuffer = ZrCore_Memory_RawMallocWithType(ls->state->global, newSize, ZR_MEMORY_NATIVE_TYPE_STRING);
        if (ls->buffer) {
            memcpy(newBuffer, ls->buffer, ls->bufferLength);
            ZrCore_Memory_RawFreeWithType(ls->state->global, ls->buffer, ls->bufferSize, ZR_MEMORY_NATIVE_TYPE_STRING);
        }
        ls->buffer = newBuffer;
        ls->bufferSize = newSize;
    }
    ls->buffer[ls->bufferLength++] = c;
    ls->buffer[ls->bufferLength] = '\0';
}

// 辅助函数：重置缓冲区
static void reset_buffer(SZrLexState *ls) {
    ls->bufferLength = 0;
    if (ls->buffer) {
        ls->buffer[0] = '\0';
    }
}

// 辅助函数：查找关键字
static EZrToken find_keyword(const TZrChar *name, TZrSize length) {
    for (TZrSize i = 0; zr_keywords[i].name != ZR_NULL; i++) {
        if (strlen(zr_keywords[i].name) == length && strncmp(zr_keywords[i].name, name, length) == 0) {
            return zr_keywords[i].token;
        }
    }
    return ZR_TK_IDENTIFIER;
}

// 读取标识符或关键字
static EZrToken read_identifier(SZrLexState *ls, TZrSemInfo *seminfo) {
    TZrSize start = ls->currentPos - 1;
    while (isalnum(ls->currentChar) || ls->currentChar == '_') {
        save_char(ls, (TZrChar) ls->currentChar);
        next_char(ls);
    }
    TZrSize length = ls->bufferLength;
    EZrToken token = find_keyword(&ls->source[start], length);

    if (token == ZR_TK_BOOLEAN) {
        seminfo->booleanValue = (strncmp(&ls->source[start], "true", 4) == 0) ? ZR_TRUE : ZR_FALSE;
    } else if (token == ZR_TK_IDENTIFIER) {
        seminfo->stringValue = ZrCore_String_Create(ls->state, (TZrNativeString)&ls->source[start], length);
    }

    return token;
}

// 读取数字
static EZrToken read_number(SZrLexState *ls, TZrSemInfo *seminfo) {
    TZrBool isFloat = ZR_FALSE;
    TZrBool isHex = ZR_FALSE;
    TZrSize start = ls->currentPos - 1;

    if (ls->currentChar == '0') {
        save_char(ls, (TZrChar) ls->currentChar);
        next_char(ls);
        if (ls->currentChar == 'x' || ls->currentChar == 'X') {
            isHex = ZR_TRUE;
            save_char(ls, (TZrChar) ls->currentChar);
            next_char(ls);
        } else if (ls->currentChar >= '0' && ls->currentChar <= '7') {
            // 八进制
            while (ls->currentChar >= '0' && ls->currentChar <= '7') {
                save_char(ls, (TZrChar) ls->currentChar);
                next_char(ls);
            }
            // 解析八进制
            TZrInt64 value = 0;
            for (TZrSize i = start; i < ls->currentPos - 1; i++) {
                value = value * 8 + (ls->source[i] - '0');
            }
            seminfo->intValue = value;
            seminfo->stringValue = ZrCore_String_Create(ls->state, ls->buffer, ls->bufferLength);
            return ZR_TK_INTEGER;
        }
    }

    // 读取整数部分
    while (isHex ? isxdigit(ls->currentChar) : isdigit(ls->currentChar)) {
        save_char(ls, (TZrChar) ls->currentChar);
        next_char(ls);
    }

    // 检查是否有小数点
    if (ls->currentChar == '.' && !isHex) {
        isFloat = ZR_TRUE;
        save_char(ls, (TZrChar) ls->currentChar);
        next_char(ls);
        while (isdigit(ls->currentChar)) {
            save_char(ls, (TZrChar) ls->currentChar);
            next_char(ls);
        }
    }

    // 检查是否有指数
    if ((ls->currentChar == 'e' || ls->currentChar == 'E') && !isHex) {
        isFloat = ZR_TRUE;
        save_char(ls, (TZrChar) ls->currentChar);
        next_char(ls);
        if (ls->currentChar == '+' || ls->currentChar == '-') {
            save_char(ls, (TZrChar) ls->currentChar);
            next_char(ls);
        }
        while (isdigit(ls->currentChar)) {
            save_char(ls, (TZrChar) ls->currentChar);
            next_char(ls);
        }
    }

    // 检查是否有类型后缀
    if (ls->currentChar == 'f' || ls->currentChar == 'F' || ls->currentChar == 'd' || ls->currentChar == 'D') {
        isFloat = ZR_TRUE;
        save_char(ls, (TZrChar) ls->currentChar);
        next_char(ls);
    }

    // 解析数字
    if (isFloat) {
        seminfo->floatValue = strtod(&ls->source[start], ZR_NULL);
        seminfo->stringValue = ZrCore_String_Create(ls->state, ls->buffer, ls->bufferLength);
        return ZR_TK_FLOAT;
    } else {
        if (isHex) {
            seminfo->intValue = (TZrInt64) strtoull(&ls->source[start], ZR_NULL, 16);
        } else {
            seminfo->intValue = (TZrInt64) strtoll(&ls->source[start], ZR_NULL, 10);
        }
        seminfo->stringValue = ZrCore_String_Create(ls->state, ls->buffer, ls->bufferLength);
        return ZR_TK_INTEGER;
    }
}

// 读取字符串
static void read_string(SZrLexState *ls, TZrSemInfo *seminfo) {
    TZrInt32 delimiter = ls->currentChar;
    next_char(ls); // 跳过开始引号

    reset_buffer(ls);
    while (ls->currentChar != delimiter && ls->currentChar != ZR_PARSER_LEXER_EOZ) {
        if (ls->currentChar == '\\') {
            next_char(ls); // 跳过反斜杠
            switch (ls->currentChar) {
                case 'n':
                    save_char(ls, '\n');
                    next_char(ls); // 跳过 'n'
                    break;
                case 't':
                    save_char(ls, '\t');
                    next_char(ls); // 跳过 't'
                    break;
                case 'r':
                    save_char(ls, '\r');
                    next_char(ls); // 跳过 'r'
                    break;
                case 'b':
                    save_char(ls, '\b');
                    next_char(ls); // 跳过 'b'
                    break;
                case 'f':
                    save_char(ls, '\f');
                    next_char(ls); // 跳过 'f'
                    break;
                case '"':
                    save_char(ls, '"');
                    next_char(ls); // 跳过 '"'
                    break;
                case '\'':
                    save_char(ls, '\'');
                    next_char(ls); // 跳过 '\''
                    break;
                case '\\':
                    save_char(ls, '\\');
                    next_char(ls); // 跳过 '\\'
                    break;
                case 'u': {
                    // Unicode 转义 \uXXXX
                    next_char(ls); // 跳过 'u'
                    TZrInt32 code = 0;
                    for (int i = 0; i < 4; i++) {
                        if (!isxdigit(ls->currentChar)) {
                            ZrParser_Lexer_SyntaxError(ls, "invalid unicode escape sequence");
                            return;
                        }
                        code = code * 16 + (isdigit(ls->currentChar) ? (ls->currentChar - '0')
                                                                     : (tolower(ls->currentChar) - 'a' + 10));
                        next_char(ls); // 跳过十六进制字符
                    }
                    // 简化处理：直接保存为字符（实际应该转换为 UTF-8）
                    save_char(ls, (TZrChar) code);
                    // 注意：这里不需要再调用 next_char，因为循环中已经调用了4次
                    break;
                }
                case 'x': {
                    // 十六进制转义 \xXX
                    next_char(ls); // 跳过 'x'
                    TZrInt32 code = 0;
                    for (int i = 0; i < 2; i++) {
                        if (!isxdigit(ls->currentChar)) {
                            ZrParser_Lexer_SyntaxError(ls, "invalid hex escape sequence");
                            return;
                        }
                        code = code * 16 + (isdigit(ls->currentChar) ? (ls->currentChar - '0')
                                                                     : (tolower(ls->currentChar) - 'a' + 10));
                        next_char(ls); // 跳过十六进制字符
                    }
                    save_char(ls, (TZrChar) code);
                    // 注意：这里不需要再调用 next_char，因为循环中已经调用了2次
                    break;
                }
                default:
                    save_char(ls, (TZrChar) ls->currentChar);
                    next_char(ls); // 跳过转义后的字符
                    break;
            }
            // 转义序列处理完后，继续下一次循环（不在这里调用 next_char）
            continue;
        } else if (ls->currentChar == '\n' || ls->currentChar == '\r') {
            // 字符串中不允许未转义的换行符
            ZrParser_Lexer_SyntaxError(ls, "unfinished string");
            return;
        } else {
            save_char(ls, (TZrChar) ls->currentChar);
            next_char(ls); // 跳过普通字符
        }
    }

    if (ls->currentChar == ZR_PARSER_LEXER_EOZ) {
        ZrParser_Lexer_SyntaxError(ls, "unfinished string");
        return;
    }

    next_char(ls); // 跳过结束引号
    seminfo->stringValue = ZrCore_String_Create(ls->state, ls->buffer, ls->bufferLength);
}

// 读取模板字符串（使用反引号包裹，允许换行，插值在 parser 中拆分）
static void read_template_string(SZrLexState *ls, TZrSemInfo *seminfo) {
    next_char(ls); // 跳过开始反引号

    reset_buffer(ls);
    while (ls->currentChar != '`' && ls->currentChar != ZR_PARSER_LEXER_EOZ) {
        if (ls->currentChar == '\\') {
            next_char(ls);
            switch (ls->currentChar) {
                case 'n':
                    save_char(ls, '\n');
                    next_char(ls);
                    break;
                case 't':
                    save_char(ls, '\t');
                    next_char(ls);
                    break;
                case 'r':
                    save_char(ls, '\r');
                    next_char(ls);
                    break;
                case '`':
                    save_char(ls, '`');
                    next_char(ls);
                    break;
                case '\\':
                    save_char(ls, '\\');
                    next_char(ls);
                    break;
                default:
                    save_char(ls, '\\');
                    if (ls->currentChar != ZR_PARSER_LEXER_EOZ) {
                        save_char(ls, (TZrChar)ls->currentChar);
                        next_char(ls);
                    }
                    break;
            }
            continue;
        }

        save_char(ls, (TZrChar)ls->currentChar);
        next_char(ls);
    }

    if (ls->currentChar == ZR_PARSER_LEXER_EOZ) {
        ZrParser_Lexer_SyntaxError(ls, "unfinished template string");
        return;
    }

    next_char(ls); // 跳过结束反引号
    seminfo->stringValue = ZrCore_String_Create(ls->state, ls->buffer, ls->bufferLength);
}

// 读取字符
static void read_char(SZrLexState *ls, TZrSemInfo *seminfo) {
    next_char(ls); // 跳过开始引号

    if (ls->currentChar == '\\') {
        next_char(ls);
        switch (ls->currentChar) {
            case 'n':
                seminfo->charValue = '\n';
                next_char(ls);
                break;
            case 't':
                seminfo->charValue = '\t';
                next_char(ls);
                break;
            case 'r':
                seminfo->charValue = '\r';
                next_char(ls);
                break;
            case 'b':
                seminfo->charValue = '\b';
                next_char(ls);
                break;
            case 'f':
                seminfo->charValue = '\f';
                next_char(ls);
                break;
            case '\'':
                seminfo->charValue = '\'';
                next_char(ls);
                break;
            case '\\':
                seminfo->charValue = '\\';
                next_char(ls);
                break;
            case 'u': {
                // Unicode 转义 \uXXXX
                next_char(ls); // 跳过 'u'
                TZrInt32 code = 0;
                for (int i = 0; i < 4; i++) {
                    if (!isxdigit(ls->currentChar)) {
                        ZrParser_Lexer_SyntaxError(ls, "invalid unicode escape sequence");
                        return;
                    }
                    code = code * 16 + (isdigit(ls->currentChar) ? (ls->currentChar - '0')
                                                                 : (tolower(ls->currentChar) - 'a' + 10));
                    next_char(ls); // 跳过十六进制字符
                }
                seminfo->charValue = (TZrChar) code;
                break;
            }
            case 'x': {
                // 十六进制转义 \xXX
                next_char(ls); // 跳过 'x'
                TZrInt32 code = 0;
                for (int i = 0; i < 2; i++) {
                    if (!isxdigit(ls->currentChar)) {
                        ZrParser_Lexer_SyntaxError(ls, "invalid hex escape sequence");
                        return;
                    }
                    code = code * 16 + (isdigit(ls->currentChar) ? (ls->currentChar - '0')
                                                                 : (tolower(ls->currentChar) - 'a' + 10));
                    next_char(ls); // 跳过十六进制字符
                }
                seminfo->charValue = (TZrChar) code;
                break;
            }
            default:
                seminfo->charValue = (TZrChar) ls->currentChar;
                next_char(ls);
                break;
        }
    } else {
        seminfo->charValue = (TZrChar) ls->currentChar;
        next_char(ls);
    }

    if (ls->currentChar != '\'') {
        ZrParser_Lexer_SyntaxError(ls, "unfinished character literal");
        return;
    }
    next_char(ls); // 跳过结束引号
}

// 跳过空白和注释
static void skip_whitespace_and_comments(SZrLexState *ls) {
    for (;;) {
        while (isspace(ls->currentChar)) {
            // 注意：next_char 已经处理了换行符的行号增加，这里不需要重复处理
            next_char(ls);
        }

        if (ls->currentChar == '/' && ls->currentPos < ls->sourceLength && ls->source[ls->currentPos] == '/') {
            // 行注释
            next_char(ls); // 跳过 '/'
            next_char(ls); // 跳过 '/'
            while (ls->currentChar != '\n' && ls->currentChar != ZR_PARSER_LEXER_EOZ) {
                next_char(ls);
            }
        } else if (ls->currentChar == '/' && ls->currentPos < ls->sourceLength && ls->source[ls->currentPos] == '*') {
            // 块注释
            next_char(ls); // 跳过 '/'
            next_char(ls); // 跳过 '*'
            while (ls->currentChar != ZR_PARSER_LEXER_EOZ) {
                if (ls->currentChar == '*' && ls->currentPos < ls->sourceLength && ls->source[ls->currentPos] == '/') {
                    next_char(ls); // 跳过 '*'
                    next_char(ls); // 跳过 '/'
                    break;
                }
                // 注意：next_char 已经处理了换行符的行号增加，这里不需要重复处理
                next_char(ls);
            }
        } else {
            break;
        }
    }
}

// 主词法分析函数
static EZrToken llex(SZrLexState *ls, TZrSemInfo *seminfo) {
    reset_buffer(ls);
    skip_whitespace_and_comments(ls);

    ls->lastLine = ls->lineNumber;

    if (ls->currentChar == ZR_PARSER_LEXER_EOZ) {
        return ZR_TK_EOS;
    }

    // 先检查是否是字母或下划线（标识符）
    if ((ls->currentChar >= 'a' && ls->currentChar <= 'z') || (ls->currentChar >= 'A' && ls->currentChar <= 'Z') ||
        ls->currentChar == '_') {
        return read_identifier(ls, seminfo);
    }

    // 检查是否是数字
    if (ls->currentChar >= '0' && ls->currentChar <= '9') {
        return read_number(ls, seminfo);
    }

    switch (ls->currentChar) {
        case '"':
            // 双引号表示字符串
            read_string(ls, seminfo);
            return ZR_TK_STRING;

        case '`':
            read_template_string(ls, seminfo);
            return ZR_TK_TEMPLATE_STRING;
        
        case '\'':
            // 单引号表示字符字面量
            read_char(ls, seminfo);
            return ZR_TK_CHAR;

        case '.':
            next_char(ls);
            if (ls->currentChar == '.' && ls->currentPos < ls->sourceLength && ls->source[ls->currentPos] == '.') {
                next_char(ls); // 跳过第二个 '.'
                next_char(ls); // 跳过第三个 '.'
                return ZR_TK_PARAMS;
            }
            if (ls->currentChar == '.') {
                next_char(ls); // 跳过第二个 '.'
                return ZR_TK_DOT_DOT;
            }
            return ZR_TK_DOT;

        case '?':
            next_char(ls);
            return ZR_TK_QUESTIONMARK;

        case ':':
            next_char(ls);
            return ZR_TK_COLON;

        case ';':
            next_char(ls);
            return ZR_TK_SEMICOLON;

        case ',':
            next_char(ls);
            return ZR_TK_COMMA;

        case '~':
            next_char(ls);
            return ZR_TK_TILDE;

        case '@':
            next_char(ls);
            return ZR_TK_AT;

        case '#':
            next_char(ls);
            return ZR_TK_SHARP;

        case '$':
            next_char(ls);
            return ZR_TK_DOLLAR;

        case '(':
            next_char(ls);
            return ZR_TK_LPAREN;

        case ')':
            next_char(ls);
            return ZR_TK_RPAREN;

        case '{':
            next_char(ls);
            return ZR_TK_LBRACE;

        case '}':
            next_char(ls);
            return ZR_TK_RBRACE;

        case '[':
            next_char(ls);
            return ZR_TK_LBRACKET;

        case ']':
            next_char(ls);
            return ZR_TK_RBRACKET;

        case '=':
            next_char(ls);
            if (ls->currentChar == '=') {
                next_char(ls);
                return ZR_TK_DOUBLE_EQUALS;
            } else if (ls->currentChar == '>') {
                next_char(ls);
                return ZR_TK_RIGHT_ARROW;
            }
            return ZR_TK_EQUALS;

        case '+':
            next_char(ls);
            if (ls->currentChar == '=') {
                next_char(ls);
                return ZR_TK_PLUS_EQUALS;
            }
            return ZR_TK_PLUS;

        case '-':
            next_char(ls);
            if (ls->currentChar == '=') {
                next_char(ls);
                return ZR_TK_MINUS_EQUALS;
            }
            return ZR_TK_MINUS;

        case '*':
            next_char(ls);
            if (ls->currentChar == '=') {
                next_char(ls);
                return ZR_TK_STAR_EQUALS;
            }
            return ZR_TK_STAR;

        case '/':
            next_char(ls);
            if (ls->currentChar == '=') {
                next_char(ls);
                return ZR_TK_SLASH_EQUALS;
            }
            return ZR_TK_SLASH;

        case '%':
            next_char(ls);
            if (ls->currentChar == '=') {
                next_char(ls);
                return ZR_TK_PERCENT_EQUALS;
            }
            return ZR_TK_PERCENT;

        case '!':
            next_char(ls);
            if (ls->currentChar == '=') {
                next_char(ls);
                return ZR_TK_BANG_EQUALS;
            }
            return ZR_TK_BANG;

        case '<':
            next_char(ls);
            if (ls->currentChar == '=') {
                next_char(ls);
                return ZR_TK_LESS_THAN_EQUALS;
            } else if (ls->currentChar == '<') {
                next_char(ls);
                return ZR_TK_LEFT_SHIFT;
            }
            return ZR_TK_LESS_THAN;

        case '>':
            next_char(ls);
            if (ls->currentChar == '=') {
                next_char(ls);
                return ZR_TK_GREATER_THAN_EQUALS;
            } else if (ls->currentChar == '>') {
                next_char(ls);
                return ZR_TK_RIGHT_SHIFT;
            }
            return ZR_TK_GREATER_THAN;

        case '&':
            next_char(ls);
            if (ls->currentChar == '&') {
                next_char(ls);
                return ZR_TK_AMPERSAND_AMPERSAND;
            }
            return ZR_TK_AND;

        case '|':
            next_char(ls);
            if (ls->currentChar == '|') {
                next_char(ls);
                return ZR_TK_PIPE_PIPE;
            }
            return ZR_TK_OR;

        case '^':
            next_char(ls);
            return ZR_TK_XOR;
    }

    // 如果上面的 switch 没有匹配，检查是否是标识符或数字
    if ((ls->currentChar >= 'a' && ls->currentChar <= 'z') || (ls->currentChar >= 'A' && ls->currentChar <= 'Z') ||
        ls->currentChar == '_') {
        return read_identifier(ls, seminfo);
    }

    if (ls->currentChar >= '0' && ls->currentChar <= '9') {
        return read_number(ls, seminfo);
    }

    // 未知字符，返回字符本身
    TZrInt32 c = ls->currentChar;
    next_char(ls);
    return (EZrToken) c;
}

// 初始化词法分析器
void ZrParser_Lexer_Init(SZrLexState *ls, SZrState *state, const TZrChar *source, TZrSize sourceLength, SZrString *sourceName) {
    ZR_ASSERT(ls != ZR_NULL);
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(source != ZR_NULL);

    ls->state = state;
    ls->source = source;
    ls->sourceLength = sourceLength;
    ls->currentPos = 0;
    ls->lineNumber = 1;
    ls->lastLine = 1;
    ls->sourceName = sourceName;
    ls->currentTokenHadError = ZR_FALSE;
    ls->currentTokenErrorMessage = ZR_NULL;
    ls->buffer = ZR_NULL;
    ls->bufferSize = 0;
    ls->bufferLength = 0;
    
    // 初始化 lookahead 相关字段
    ls->lookahead.token = ZR_TK_EOS;
    ls->lookaheadPos = 0;
    ls->lookaheadChar = 0;
    ls->lookaheadLine = 1;
    ls->lookaheadLastLine = 1;

    // 分配初始缓冲区
    ls->bufferSize = ZR_PARSER_LEXER_BUFFER_INITIAL_SIZE;
    ls->buffer = ZrCore_Memory_RawMallocWithType(state->global, ls->bufferSize, ZR_MEMORY_NATIVE_TYPE_STRING);
    ls->buffer[0] = '\0';

    // 初始化 token
    ls->t.token = ZR_TK_EOS;
    ls->t.hasLexError = ZR_FALSE;
    ls->t.lexErrorMessage = ZR_NULL;
    ls->lookahead.token = ZR_TK_EOS;
    ls->lookahead.hasLexError = ZR_FALSE;
    ls->lookahead.lexErrorMessage = ZR_NULL;

    // 读取第一个字符
    next_char(ls);

    // 读取第一个 token
    ZrParser_Lexer_Next(ls);
}

// 获取下一个 token
void ZrParser_Lexer_Next(SZrLexState *ls) {
    // 如果 lookahead 已经被缓存，使用它而不是重新读取
    // 这样可以避免重复读取，提高性能，并且确保 lookahead 缓存被正确清除
    if (ls->lookahead.token != ZR_TK_EOS) {
        ls->t = ls->lookahead;
        // 恢复下一个 token 的位置（推进 lexer 状态）
        ls->currentPos = ls->lookaheadPos;
        ls->currentChar = ls->lookaheadChar;
        ls->lineNumber = ls->lookaheadLine;
        ls->lastLine = ls->lookaheadLastLine;
        ls->lookahead.token = ZR_TK_EOS;  // 清除 lookahead 缓存
    } else {
        TZrSemInfo seminfo;
        memset(&seminfo, 0, sizeof(seminfo));
        ls->currentTokenHadError = ZR_FALSE;
        ls->currentTokenErrorMessage = ZR_NULL;
        ls->t.token = llex(ls, &seminfo);
        ls->t.seminfo = seminfo;
        ls->t.hasLexError = ls->currentTokenHadError;
        ls->t.lexErrorMessage = ls->currentTokenErrorMessage;
    }
}

// 查看下一个 token（不消费）
EZrToken ZrParser_Lexer_Lookahead(SZrLexState *ls) {
    if (ls->lookahead.token == ZR_TK_EOS) {
        // 保存当前状态（包括所有可能被 llex 修改的字段）
        TZrSize savedPos = ls->currentPos;
        TZrInt32 savedChar = ls->currentChar;
        TZrInt32 savedLine = ls->lineNumber;
        TZrInt32 savedLastLine = ls->lastLine;
        SZrToken savedToken = ls->t;

        // 读取下一个 token
        TZrSemInfo seminfo;
        memset(&seminfo, 0, sizeof(seminfo));
        ls->currentTokenHadError = ZR_FALSE;
        ls->currentTokenErrorMessage = ZR_NULL;
        ls->lookahead.token = llex(ls, &seminfo);
        ls->lookahead.seminfo = seminfo;
        ls->lookahead.hasLexError = ls->currentTokenHadError;
        ls->lookahead.lexErrorMessage = ls->currentTokenErrorMessage;

        // 保存下一个 token 的位置（当使用缓存的 lookahead 时，需要恢复到这个位置）
        ls->lookaheadPos = ls->currentPos;
        ls->lookaheadChar = ls->currentChar;
        ls->lookaheadLine = ls->lineNumber;
        ls->lookaheadLastLine = ls->lastLine;

        // 恢复状态（必须恢复所有被修改的字段）
        ls->currentPos = savedPos;
        ls->currentChar = savedChar;
        ls->lineNumber = savedLine;
        ls->lastLine = savedLastLine;
        ls->t = savedToken;
    }
    return ls->lookahead.token;
}

// 报告语法错误
// 计算当前列号（从当前行开始到当前位置的字符数）
static TZrInt32 calculate_column(SZrLexState *ls) {
    if (ls->source == ZR_NULL || ls->currentPos == 0) {
        return 1;
    }

    // 从当前位置向前查找，直到找到行首或换行符
    TZrSize pos = ls->currentPos - 1;
    TZrInt32 column = 1;
    while (pos > 0 && ls->source[pos] != '\n') {
        pos--;
        column++;
    }
    return column;
}

// 获取当前行的代码片段（前后各20个字符）
static void get_line_snippet_lexer(SZrLexState *ls, TZrChar *buffer, TZrSize bufferSize, TZrInt32 *errorColumn) {
    if (ls == ZR_NULL || ls->source == ZR_NULL || bufferSize == 0) {
        buffer[0] = '\0';
        *errorColumn = 1;
        return;
    }

    // 计算列号并找到行首
    TZrSize pos = ls->currentPos;
    TZrInt32 column = 1;
    TZrSize lineStart = pos;

    // 向前查找行首
    while (lineStart > 0 && ls->source[lineStart - 1] != '\n') {
        lineStart--;
        column++;
    }

    // 向后查找行尾
    TZrSize lineEnd = pos;
    while (lineEnd < ls->sourceLength && ls->source[lineEnd] != '\n') {
        lineEnd++;
    }

    // 计算要显示的起始和结束位置（前后各固定上下文字符）
    TZrSize snippetStart = lineStart;
    TZrSize snippetEnd = lineEnd;
    TZrInt32 displayColumn = column;

    // 如果列号大于上下文半径，向前移动起始位置
    if (column > (TZrInt32)ZR_PARSER_ERROR_SNIPPET_CONTEXT_RADIUS) {
        snippetStart = pos - ZR_PARSER_ERROR_SNIPPET_CONTEXT_RADIUS;
        displayColumn = (TZrInt32)ZR_PARSER_ERROR_SNIPPET_FOCUS_COLUMN;
    }

    // 如果剩余字符少于上下文半径，向后扩展
    if (lineEnd - pos < ZR_PARSER_ERROR_SNIPPET_CONTEXT_RADIUS) {
        TZrSize needed = ZR_PARSER_ERROR_SNIPPET_CONTEXT_RADIUS - (lineEnd - pos);
        if (snippetStart >= needed) {
            snippetStart -= needed;
            displayColumn += (TZrInt32)needed;
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
    if (snippetEnd > ls->sourceLength) {
        snippetEnd = ls->sourceLength;
    }

    // 复制代码片段
    TZrSize snippetLen = snippetEnd - snippetStart;
    if (snippetLen >= bufferSize) {
        snippetLen = bufferSize - 1;
    }

    for (TZrSize i = 0; i < snippetLen; i++) {
        TZrChar c = ls->source[snippetStart + i];
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

void ZrParser_Lexer_SyntaxError(SZrLexState *ls, const TZrChar *msg) {
    if (ls == ZR_NULL || msg == ZR_NULL) {
        return;
    }

    if (!ls->currentTokenHadError) {
        ls->currentTokenHadError = ZR_TRUE;
        ls->currentTokenErrorMessage = msg;
    }

    // 获取文件名
    const TZrChar *fileName = "<unknown>";
    if (ls->sourceName != ZR_NULL) {
        TZrNativeString nameStr = ZrCore_String_GetNativeString(ls->sourceName);
        if (nameStr != ZR_NULL) {
            fileName = nameStr;
        }
    }

    // 计算列号
    TZrInt32 column = calculate_column(ls);

    // 获取代码片段
    TZrChar snippet[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    TZrInt32 displayColumn = 1;
    get_line_snippet_lexer(ls, snippet, sizeof(snippet), &displayColumn);

    // 输出错误信息
    printf("  [%s:%d:%d] %s\n", fileName, ls->lineNumber, column, msg);

    // 输出代码片段和错误位置标记
    if (snippet[0] != '\0') {
        printf("    %s\n", snippet);
        // 输出错误位置标记（^）
        for (TZrInt32 i = 0; i < displayColumn - 1; i++) {
            printf(" ");
        }
        printf("^\n");
    }
}

// Token 转字符串（用于错误消息）
const TZrChar *ZrParser_Lexer_TokenToString(SZrLexState *ls, EZrToken token) {
    (void) ls;

    if (token < ZR_FIRST_RESERVED) {
        // 单字符 token
        static TZrChar charToken[2];
        charToken[0] = (TZrChar) token;
        charToken[1] = '\0';
        return charToken;
    }

    // 遍历数组查找匹配的 token（不依赖数组顺序）
    TZrSize tokenInfoCount = sizeof(zr_token_info) / sizeof(zr_token_info[0]);
    for (TZrSize i = 0; i < tokenInfoCount; i++) {
        if (zr_token_info[i].token == token) {
            return zr_token_info[i].name;
        }
    }

    return "<unknown>";
}
