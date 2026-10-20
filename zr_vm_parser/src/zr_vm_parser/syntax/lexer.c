#include "syntax/lexer_internal.h"
#include "parser/parser_ownership.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

TZrInt32 ZrParser_Lexer_ReadChar(SZrLexState *ls) {
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
        ls->currentLineStartOffset = ls->currentPos;
        return ls->currentChar;
    }

    if (ls->currentChar == '\n') {
        ls->lineNumber++;
        ls->currentLineStartOffset = ls->currentPos;
    }
    return ls->currentChar;
}

// 空白与注释跳过后记录 token 起点，供 parser/LSP 的字节范围及增量 seek 使用。
static void lexer_record_token_start(SZrLexState *ls) {
    if (ls == ZR_NULL) {
        return;
    }

    ls->tokenStartLine = ls->lineNumber > 0 ? ls->lineNumber : 1;
    ls->tokenStartLineStart = ls->currentLineStartOffset;
    if (ls->currentChar == ZR_PARSER_LEXER_EOZ) {
        ls->tokenStartOffset = ls->currentPos;
        return;
    }

    ls->tokenStartOffset = ls->currentPos > 0 ? ls->currentPos - 1 : 0;
}

// 积累当前字面量文本；缓冲区归 lexer 所有，扩容后旧地址立即失效。
static void save_char(SZrLexState *ls, TZrChar c) {
    if (ls->currentTokenHadError || ls->bufferLength == ZR_MAX_SIZE) {
        return;
    }
    if (ls->bufferLength + 1 >= ls->bufferSize) {
        if (ls->bufferSize > ZR_MAX_SIZE / ZR_PARSER_DYNAMIC_CAPACITY_GROWTH_FACTOR) {
            ls->currentTokenHadError = ZR_TRUE;
            ls->currentTokenErrorMessage = "Lexer buffer capacity overflow";
            return;
        }
        TZrSize newSize = ls->bufferSize * ZR_PARSER_DYNAMIC_CAPACITY_GROWTH_FACTOR;
        TZrChar *newBuffer = ZrParser_Ownership_Allocate(
                ls->ownership, newSize, ZR_MEMORY_NATIVE_TYPE_STRING, ZR_FALSE);
        if (newBuffer == ZR_NULL) {
            ls->currentTokenHadError = ZR_TRUE;
            ls->currentTokenErrorMessage = "Lexer buffer allocation budget exceeded";
            return;
        }
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

// 每个 token 扫描前清空临时文本，保持已分配容量供后续复用。
static void reset_buffer(SZrLexState *ls) {
    ls->bufferLength = 0;
    if (ls->buffer) {
        ls->buffer[0] = '\0';
    }
}

// 用长度限制比较源片段，避免要求源文本在标识符之后有终止符。
static EZrToken read_identifier(SZrLexState *ls, TZrSemInfo *seminfo) {
    TZrSize start = ls->currentPos - 1;
    while (isalnum(ls->currentChar) || ls->currentChar == '_') {
        save_char(ls, (TZrChar) ls->currentChar);
        ZrParser_Lexer_ReadChar(ls);
    }
    TZrSize length = ls->bufferLength;
    EZrToken token = ZrParser_Lexer_FindKeyword(&ls->source[start], length);

    if (token == ZR_TK_BOOLEAN) {
        seminfo->booleanValue = (strncmp(&ls->source[start], "true", 4) == 0) ? ZR_TRUE : ZR_FALSE;
    } else if (token == ZR_TK_IDENTIFIER) {
        seminfo->stringValue = ZrParser_Lexer_CreateString(ls, &ls->source[start], length);
    }

    return token;
}

// 扫描数字拼写并同时填充值与 VM 字符串，供 AST 保留原始字面量。
static EZrToken read_number(SZrLexState *ls, TZrSemInfo *seminfo) {
    TZrBool isFloat = ZR_FALSE;
    TZrBool isHex = ZR_FALSE;
    TZrBool isOctal = ZR_FALSE;

    if (ls->currentChar == '0') {
        save_char(ls, (TZrChar)ls->currentChar);
        ZrParser_Lexer_ReadChar(ls);
        if (ls->currentChar == 'x' || ls->currentChar == 'X') {
            isHex = ZR_TRUE;
            save_char(ls, (TZrChar)ls->currentChar);
            ZrParser_Lexer_ReadChar(ls);
            if (!isxdigit(ls->currentChar)) {
                ZrParser_Lexer_SyntaxError(ls, "Hexadecimal literal requires a digit");
            }
        } else if (ls->currentChar >= '0' && ls->currentChar <= '7') {
            isOctal = ZR_TRUE;
        }
    }

    // 读取整数部分
    while (isHex ? isxdigit(ls->currentChar) : isdigit(ls->currentChar)) {
        save_char(ls, (TZrChar) ls->currentChar);
        ZrParser_Lexer_ReadChar(ls);
    }

    // 检查是否有小数点
    if (ls->currentChar == '.' && !isHex) {
        isFloat = ZR_TRUE;
        save_char(ls, (TZrChar) ls->currentChar);
        ZrParser_Lexer_ReadChar(ls);
        while (isdigit(ls->currentChar)) {
            save_char(ls, (TZrChar) ls->currentChar);
            ZrParser_Lexer_ReadChar(ls);
        }
    }

    // 检查是否有指数
    if ((ls->currentChar == 'e' || ls->currentChar == 'E') && !isHex) {
        isFloat = ZR_TRUE;
        save_char(ls, (TZrChar) ls->currentChar);
        ZrParser_Lexer_ReadChar(ls);
        if (ls->currentChar == '+' || ls->currentChar == '-') {
            save_char(ls, (TZrChar) ls->currentChar);
            ZrParser_Lexer_ReadChar(ls);
        }
        if (!isdigit(ls->currentChar)) {
            ZrParser_Lexer_SyntaxError(ls, "Numeric exponent requires a digit");
        }
        while (isdigit(ls->currentChar)) {
            save_char(ls, (TZrChar) ls->currentChar);
            ZrParser_Lexer_ReadChar(ls);
        }
    }

    // 检查是否有类型后缀
    if (!isHex && (ls->currentChar == 'f' || ls->currentChar == 'F' || ls->currentChar == 'd' || ls->currentChar == 'D')) {
        isFloat = ZR_TRUE;
        save_char(ls, (TZrChar) ls->currentChar);
        ZrParser_Lexer_ReadChar(ls);
    }

    /* Conversion must use the terminated token buffer, never the borrowed source slice. */
    seminfo->stringValue = ZrParser_Lexer_CreateString(ls, ls->buffer, ls->bufferLength);
    if (isFloat) {
        char *end = ZR_NULL;
        errno = 0;
        seminfo->floatValue = strtod(ls->buffer, &end);
        if (errno == ERANGE || !isfinite(seminfo->floatValue) ||
            ((ls->buffer[ls->bufferLength - 1] == 'f' ||
              ls->buffer[ls->bufferLength - 1] == 'F') &&
             fabs(seminfo->floatValue) > FLT_MAX)) {
            ZrParser_Lexer_SyntaxError(ls, "Floating-point literal is out of range");
        }
        if (end == ls->buffer || (*end != '\0' &&
            !((*end == 'f' || *end == 'F' || *end == 'd' || *end == 'D') && end[1] == '\0'))) {
            ZrParser_Lexer_SyntaxError(ls, "Invalid floating-point literal");
        }
        return ZR_TK_FLOAT;
    }

    /* Hex and octal retain the existing 64-bit bit-pattern spelling. Decimal is signed. */
    TZrUInt64 value = 0;
    TZrUInt64 limit = (isHex || isOctal) ? UINT64_MAX : INT64_MAX;
    unsigned base = isHex ? 16u : (isOctal ? 8u : 10u);
    for (TZrSize i = isHex ? 2 : 0; i < ls->bufferLength; ++i) {
        unsigned char c = (unsigned char)ls->buffer[i];
        unsigned digit = c >= '0' && c <= '9' ? (unsigned)(c - '0') :
                         (unsigned)(tolower(c) - 'a' + 10);
        if (digit >= base) {
            ZrParser_Lexer_SyntaxError(ls, "Invalid digit in integer literal");
            break;
        }
        if (value > (limit - digit) / base) {
            ZrParser_Lexer_SyntaxError(ls, "Integer literal is out of range");
            break;
        }
        value = value * base + digit;
    }
    seminfo->intValue = value <= INT64_MAX ? (TZrInt64)value :
                       -1 - (TZrInt64)(UINT64_MAX - value);
    return ZR_TK_INTEGER;
}

// 解码普通字符串转义；词法错误保存在当前 token 并由 parser 继续处理。
static void read_string(SZrLexState *ls, TZrSemInfo *seminfo) {
    TZrInt32 delimiter = ls->currentChar;
    ZrParser_Lexer_ReadChar(ls); // 跳过开始引号

    reset_buffer(ls);
    while (ls->currentChar != delimiter && ls->currentChar != ZR_PARSER_LEXER_EOZ) {
        if (ls->currentChar == '\\') {
            ZrParser_Lexer_ReadChar(ls); // 跳过反斜杠
            switch (ls->currentChar) {
                case 'n':
                    save_char(ls, '\n');
                    ZrParser_Lexer_ReadChar(ls); // 跳过 'n'
                    break;
                case 't':
                    save_char(ls, '\t');
                    ZrParser_Lexer_ReadChar(ls); // 跳过 't'
                    break;
                case 'r':
                    save_char(ls, '\r');
                    ZrParser_Lexer_ReadChar(ls); // 跳过 'r'
                    break;
                case 'b':
                    save_char(ls, '\b');
                    ZrParser_Lexer_ReadChar(ls); // 跳过 'b'
                    break;
                case 'f':
                    save_char(ls, '\f');
                    ZrParser_Lexer_ReadChar(ls); // 跳过 'f'
                    break;
                case '"':
                    save_char(ls, '"');
                    ZrParser_Lexer_ReadChar(ls); // 跳过 '"'
                    break;
                case '\'':
                    save_char(ls, '\'');
                    ZrParser_Lexer_ReadChar(ls); // 跳过 '\''
                    break;
                case '\\':
                    save_char(ls, '\\');
                    ZrParser_Lexer_ReadChar(ls); // 跳过 '\\'
                    break;
                case 'u': {
                    // Unicode 转义 \uXXXX
                    ZrParser_Lexer_ReadChar(ls); // 跳过 'u'
                    TZrInt32 code = 0;
                    for (int i = 0; i < 4; i++) {
                        if (!isxdigit(ls->currentChar)) {
                            ZrParser_Lexer_SyntaxError(ls, "invalid unicode escape sequence");
                            return;
                        }
                        code = code * 16 + (isdigit(ls->currentChar) ? (ls->currentChar - '0')
                                                                     : (tolower(ls->currentChar) - 'a' + 10));
                        ZrParser_Lexer_ReadChar(ls); // 跳过十六进制字符
                    }
                    // TODO: \uXXXX 目前截为一个 TZrChar；需核语言的 Unicode 契约及 VM
                    // 字符串编码，并用非 ASCII 转义样例确认应编码还是拒绝。
                    // 简化处理：直接保存为字符（实际应该转换为 UTF-8）
                    save_char(ls, (TZrChar) code);
                    // 注意：这里不需要再调用 ZrParser_Lexer_ReadChar，因为循环中已经调用了4次
                    break;
                }
                case 'x': {
                    // 十六进制转义 \xXX
                    ZrParser_Lexer_ReadChar(ls); // 跳过 'x'
                    TZrInt32 code = 0;
                    for (int i = 0; i < 2; i++) {
                        if (!isxdigit(ls->currentChar)) {
                            ZrParser_Lexer_SyntaxError(ls, "invalid hex escape sequence");
                            return;
                        }
                        code = code * 16 + (isdigit(ls->currentChar) ? (ls->currentChar - '0')
                                                                     : (tolower(ls->currentChar) - 'a' + 10));
                        ZrParser_Lexer_ReadChar(ls); // 跳过十六进制字符
                    }
                    save_char(ls, (TZrChar) code);
                    // 注意：这里不需要再调用 ZrParser_Lexer_ReadChar，因为循环中已经调用了2次
                    break;
                }
                default:
                    save_char(ls, (TZrChar) ls->currentChar);
                    ZrParser_Lexer_ReadChar(ls); // 跳过转义后的字符
                    break;
            }
            // 转义序列处理完后，继续下一次循环（不在这里调用 ZrParser_Lexer_ReadChar）
            continue;
        } else if (ls->currentChar == '\n' || ls->currentChar == '\r') {
            // 字符串中不允许未转义的换行符
            ZrParser_Lexer_SyntaxError(ls, "unfinished string");
            return;
        } else {
            save_char(ls, (TZrChar) ls->currentChar);
            ZrParser_Lexer_ReadChar(ls); // 跳过普通字符
        }
    }

    if (ls->currentChar == ZR_PARSER_LEXER_EOZ) {
        ZrParser_Lexer_SyntaxError(ls, "unfinished string");
        return;
    }

    ZrParser_Lexer_ReadChar(ls); // 跳过结束引号
    seminfo->stringValue = ZrParser_Lexer_CreateString(ls, ls->buffer, ls->bufferLength);
}

// 反引号允许跨行；这里只保留文本，插值拆分由 parser 的模板分支完成。
static void read_template_string(SZrLexState *ls, TZrSemInfo *seminfo) {
    ZrParser_Lexer_ReadChar(ls); // 跳过开始反引号

    reset_buffer(ls);
    while (ls->currentChar != '`' && ls->currentChar != ZR_PARSER_LEXER_EOZ) {
        if (ls->currentChar == '\\') {
            ZrParser_Lexer_ReadChar(ls);
            switch (ls->currentChar) {
                case 'n':
                    save_char(ls, '\n');
                    ZrParser_Lexer_ReadChar(ls);
                    break;
                case 't':
                    save_char(ls, '\t');
                    ZrParser_Lexer_ReadChar(ls);
                    break;
                case 'r':
                    save_char(ls, '\r');
                    ZrParser_Lexer_ReadChar(ls);
                    break;
                case '`':
                    save_char(ls, '`');
                    ZrParser_Lexer_ReadChar(ls);
                    break;
                case '\\':
                    save_char(ls, '\\');
                    ZrParser_Lexer_ReadChar(ls);
                    break;
                default:
                    save_char(ls, '\\');
                    if (ls->currentChar != ZR_PARSER_LEXER_EOZ) {
                        save_char(ls, (TZrChar)ls->currentChar);
                        ZrParser_Lexer_ReadChar(ls);
                    }
                    break;
            }
            continue;
        }

        save_char(ls, (TZrChar)ls->currentChar);
        ZrParser_Lexer_ReadChar(ls);
    }

    if (ls->currentChar == ZR_PARSER_LEXER_EOZ) {
        ZrParser_Lexer_SyntaxError(ls, "unfinished template string");
        return;
    }

    ZrParser_Lexer_ReadChar(ls); // 跳过结束反引号
    seminfo->stringValue = ZrParser_Lexer_CreateString(ls, ls->buffer, ls->bufferLength);
}

// 解码单字符或转义字面量；格式错误通过当前 token 标志交给 parser。
static void read_char(SZrLexState *ls, TZrSemInfo *seminfo) {
    ZrParser_Lexer_ReadChar(ls); // 跳过开始引号

    if (ls->currentChar == '\\') {
        ZrParser_Lexer_ReadChar(ls);
        switch (ls->currentChar) {
            case 'n':
                seminfo->charValue = '\n';
                ZrParser_Lexer_ReadChar(ls);
                break;
            case 't':
                seminfo->charValue = '\t';
                ZrParser_Lexer_ReadChar(ls);
                break;
            case 'r':
                seminfo->charValue = '\r';
                ZrParser_Lexer_ReadChar(ls);
                break;
            case 'b':
                seminfo->charValue = '\b';
                ZrParser_Lexer_ReadChar(ls);
                break;
            case 'f':
                seminfo->charValue = '\f';
                ZrParser_Lexer_ReadChar(ls);
                break;
            case '\'':
                seminfo->charValue = '\'';
                ZrParser_Lexer_ReadChar(ls);
                break;
            case '\\':
                seminfo->charValue = '\\';
                ZrParser_Lexer_ReadChar(ls);
                break;
            case 'u': {
                // Unicode 转义 \uXXXX
                ZrParser_Lexer_ReadChar(ls); // 跳过 'u'
                TZrInt32 code = 0;
                for (int i = 0; i < 4; i++) {
                    if (!isxdigit(ls->currentChar)) {
                        ZrParser_Lexer_SyntaxError(ls, "invalid unicode escape sequence");
                        return;
                    }
                    code = code * 16 + (isdigit(ls->currentChar) ? (ls->currentChar - '0')
                                                                 : (tolower(ls->currentChar) - 'a' + 10));
                    ZrParser_Lexer_ReadChar(ls); // 跳过十六进制字符
                }
                // TODO: 字符字面量也截断非 ASCII 码点；需核对字符值域与类型检查器的期望。
                seminfo->charValue = (TZrChar) code;
                break;
            }
            case 'x': {
                // 十六进制转义 \xXX
                ZrParser_Lexer_ReadChar(ls); // 跳过 'x'
                TZrInt32 code = 0;
                for (int i = 0; i < 2; i++) {
                    if (!isxdigit(ls->currentChar)) {
                        ZrParser_Lexer_SyntaxError(ls, "invalid hex escape sequence");
                        return;
                    }
                    code = code * 16 + (isdigit(ls->currentChar) ? (ls->currentChar - '0')
                                                                 : (tolower(ls->currentChar) - 'a' + 10));
                    ZrParser_Lexer_ReadChar(ls); // 跳过十六进制字符
                }
                seminfo->charValue = (TZrChar) code;
                break;
            }
            default:
                seminfo->charValue = (TZrChar) ls->currentChar;
                ZrParser_Lexer_ReadChar(ls);
                break;
        }
    } else {
        seminfo->charValue = (TZrChar) ls->currentChar;
        ZrParser_Lexer_ReadChar(ls);
    }

    if (ls->currentChar != '\'') {
        ZrParser_Lexer_SyntaxError(ls, "unfinished character literal");
        return;
    }
    ZrParser_Lexer_ReadChar(ls); // 跳过结束引号
}

// 在 token 边界前消耗空白与两类注释；行号由 ZrParser_Lexer_ReadChar 唯一维护。
static void skip_whitespace_and_comments(SZrLexState *ls) {
    for (;;) {
        while (isspace(ls->currentChar)) {
            // 注意：ZrParser_Lexer_ReadChar 已经处理了换行符的行号增加，这里不需要重复处理
            ZrParser_Lexer_ReadChar(ls);
        }

        if (ls->currentChar == '/' && ls->currentPos < ls->sourceLength && ls->source[ls->currentPos] == '/') {
            // 行注释
            ZrParser_Lexer_ReadChar(ls); // 跳过 '/'
            ZrParser_Lexer_ReadChar(ls); // 跳过 '/'
            while (ls->currentChar != '\n' && ls->currentChar != ZR_PARSER_LEXER_EOZ) {
                ZrParser_Lexer_ReadChar(ls);
            }
        } else if (ls->currentChar == '/' && ls->currentPos < ls->sourceLength && ls->source[ls->currentPos] == '*') {
            // 块注释
            ZrParser_Lexer_ReadChar(ls); // 跳过 '/'
            ZrParser_Lexer_ReadChar(ls); // 跳过 '*'
            while (ls->currentChar != ZR_PARSER_LEXER_EOZ) {
                if (ls->currentChar == '*' && ls->currentPos < ls->sourceLength && ls->source[ls->currentPos] == '/') {
                    ZrParser_Lexer_ReadChar(ls); // 跳过 '*'
                    ZrParser_Lexer_ReadChar(ls); // 跳过 '/'
                    break;
                }
                // 注意：ZrParser_Lexer_ReadChar 已经处理了换行符的行号增加，这里不需要重复处理
                ZrParser_Lexer_ReadChar(ls);
            }
            // BUG: `/*` 到文件末尾时不报错，ZrParser_Lexer_Scan 随后返回 EOS；Parse 可返回空脚本。
            // 语言词法文档要求未闭合块注释是源文件词法错误。
        } else {
            break;
        }
    }
}

// 以已定位的当前字符分派扫描器；复合符号在此消费完整拼写。
EZrToken ZrParser_Lexer_Scan(SZrLexState *ls, TZrSemInfo *seminfo) {
    reset_buffer(ls);
    skip_whitespace_and_comments(ls);

    ls->lastLine = ls->lineNumber;
    lexer_record_token_start(ls);

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
            ZrParser_Lexer_ReadChar(ls);
            if (ls->currentChar == '.' && ls->currentPos < ls->sourceLength && ls->source[ls->currentPos] == '.') {
                ZrParser_Lexer_ReadChar(ls); // 跳过第二个 '.'
                ZrParser_Lexer_ReadChar(ls); // 跳过第三个 '.'
                return ZR_TK_PARAMS;
            }
            if (ls->currentChar == '.') {
                ZrParser_Lexer_ReadChar(ls); // 跳过第二个 '.'
                return ZR_TK_DOT_DOT;
            }
            return ZR_TK_DOT;

        case '?':
            ZrParser_Lexer_ReadChar(ls);
            if (ls->currentChar == '.') {
                ZrParser_Lexer_ReadChar(ls);
                return ZR_TK_QUESTION_DOT;
            }
            return ZR_TK_QUESTIONMARK;

        case ':':
            ZrParser_Lexer_ReadChar(ls);
            return ZR_TK_COLON;

        case ';':
            ZrParser_Lexer_ReadChar(ls);
            return ZR_TK_SEMICOLON;

        case ',':
            ZrParser_Lexer_ReadChar(ls);
            return ZR_TK_COMMA;

        case '~':
            ZrParser_Lexer_ReadChar(ls);
            return ZR_TK_TILDE;

        case '@':
            ZrParser_Lexer_ReadChar(ls);
            return ZR_TK_AT;

        case '#':
            ZrParser_Lexer_ReadChar(ls);
            return ZR_TK_SHARP;

        case '$':
            ZrParser_Lexer_ReadChar(ls);
            return ZR_TK_DOLLAR;

        case '(':
            ZrParser_Lexer_ReadChar(ls);
            return ZR_TK_LPAREN;

        case ')':
            ZrParser_Lexer_ReadChar(ls);
            return ZR_TK_RPAREN;

        case '{':
            ZrParser_Lexer_ReadChar(ls);
            return ZR_TK_LBRACE;

        case '}':
            ZrParser_Lexer_ReadChar(ls);
            return ZR_TK_RBRACE;

        case '[':
            ZrParser_Lexer_ReadChar(ls);
            return ZR_TK_LBRACKET;

        case ']':
            ZrParser_Lexer_ReadChar(ls);
            return ZR_TK_RBRACKET;

        case '=':
            ZrParser_Lexer_ReadChar(ls);
            if (ls->currentChar == '=') {
                ZrParser_Lexer_ReadChar(ls);
                return ZR_TK_DOUBLE_EQUALS;
            } else if (ls->currentChar == '>') {
                ZrParser_Lexer_ReadChar(ls);
                return ZR_TK_FAT_ARROW;
            }
            return ZR_TK_EQUALS;

        case '+':
            ZrParser_Lexer_ReadChar(ls);
            if (ls->currentChar == '=') {
                ZrParser_Lexer_ReadChar(ls);
                return ZR_TK_PLUS_EQUALS;
            }
            return ZR_TK_PLUS;

        case '-':
            ZrParser_Lexer_ReadChar(ls);
            if (ls->currentChar == '=') {
                ZrParser_Lexer_ReadChar(ls);
                return ZR_TK_MINUS_EQUALS;
            } else if (ls->currentChar == '>') {
                ZrParser_Lexer_ReadChar(ls);
                return ZR_TK_THIN_ARROW;
            }
            return ZR_TK_MINUS;

        case '*':
            ZrParser_Lexer_ReadChar(ls);
            if (ls->currentChar == '=') {
                ZrParser_Lexer_ReadChar(ls);
                return ZR_TK_STAR_EQUALS;
            }
            return ZR_TK_STAR;

        case '/':
            ZrParser_Lexer_ReadChar(ls);
            if (ls->currentChar == '=') {
                ZrParser_Lexer_ReadChar(ls);
                return ZR_TK_SLASH_EQUALS;
            }
            return ZR_TK_SLASH;

        case '%':
            ZrParser_Lexer_ReadChar(ls);
            if (ls->currentChar == '=') {
                ZrParser_Lexer_ReadChar(ls);
                return ZR_TK_PERCENT_EQUALS;
            }
            return ZR_TK_PERCENT;

        case '!':
            ZrParser_Lexer_ReadChar(ls);
            if (ls->currentChar == '=') {
                ZrParser_Lexer_ReadChar(ls);
                return ZR_TK_BANG_EQUALS;
            }
            return ZR_TK_BANG;

        case '<':
            ZrParser_Lexer_ReadChar(ls);
            if (ls->currentChar == '=') {
                ZrParser_Lexer_ReadChar(ls);
                return ZR_TK_LESS_THAN_EQUALS;
            } else if (ls->currentChar == '<') {
                ZrParser_Lexer_ReadChar(ls);
                return ZR_TK_LEFT_SHIFT;
            }
            return ZR_TK_LESS_THAN;

        case '>':
            ZrParser_Lexer_ReadChar(ls);
            if (ls->currentChar == '=') {
                ZrParser_Lexer_ReadChar(ls);
                return ZR_TK_GREATER_THAN_EQUALS;
            } else if (ls->currentChar == '>') {
                ZrParser_Lexer_ReadChar(ls);
                return ZR_TK_RIGHT_SHIFT;
            }
            return ZR_TK_GREATER_THAN;

        case '&':
            ZrParser_Lexer_ReadChar(ls);
            if (ls->currentChar == '&') {
                ZrParser_Lexer_ReadChar(ls);
                return ZR_TK_AMPERSAND_AMPERSAND;
            }
            return ZR_TK_AND;

        case '|':
            ZrParser_Lexer_ReadChar(ls);
            if (ls->currentChar == '|') {
                ZrParser_Lexer_ReadChar(ls);
                return ZR_TK_PIPE_PIPE;
            }
            return ZR_TK_OR;

        case '^':
            ZrParser_Lexer_ReadChar(ls);
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
    ZrParser_Lexer_ReadChar(ls);
    return (EZrToken) c;
}

/* Attach lossless source provenance after ZrParser_Lexer_Scan has consumed the token. The
 * scanner pre-reads one character, so currentPos-1 is the raw end except at
 * EOF and for a CRLF lookahead. */
void ZrParser_Lexer_SetTextTokenMetadata(SZrLexState *ls) {
    TZrSize start;
    TZrSize end;
    TZrInt32 line;
    TZrInt32 column;
    TZrSize index;

    if (ls == ZR_NULL) {
        return;
    }
    start = ls->tokenStartOffset;
    end = ls->currentChar == ZR_PARSER_LEXER_EOZ
                  ? ls->currentPos
                  : (ls->currentPos > 0U ? ls->currentPos - 1U : 0U);
    if (ls->source != ZR_NULL && ls->currentChar == '\n' && end > 0U &&
        end < ls->sourceLength && ls->source[end - 1U] == '\r' &&
        ls->source[end] == '\n') {
        end--;
    }
    if (end < start) {
        end = start;
    }
    if (end > ls->sourceLength) {
        end = ls->sourceLength;
    }
    ls->t.rawText = ls->source != ZR_NULL && end > start
                            ? ls->source + start
                            : ZR_NULL;
    ls->t.rawLength = end - start;
    ls->t.punctPieceCount = 0u;
    ls->t.span.sourceId = 1U;
    ls->t.span.startOffset = start;
    ls->t.span.endOffset = end;
    ls->t.span.startLine = ls->tokenStartLine > 0 ? ls->tokenStartLine : 1;
    ls->t.span.startColumn = (TZrInt32)(start >= ls->tokenStartLineStart
                                                ? start - ls->tokenStartLineStart + 1U
                                                : 1U);
    line = ls->t.span.startLine;
    column = ls->t.span.startColumn;
    for (index = start; index < end; index++) {
        if (ls->source[index] == '\r') {
            line++;
            column = 1;
            if (index + 1U < end && ls->source[index + 1U] == '\n') {
                index++;
            }
        } else if (ls->source[index] == '\n') {
            line++;
            column = 1;
        } else {
            column++;
        }
    }
    ls->t.span.endLine = line;
    ls->t.span.endColumn = column;
    if (ls->hasInheritedSpan) {
        ls->t.span = ls->inheritedSpan;
    }
    ls->t.syntaxContext = ls->textSyntaxContext;
}
