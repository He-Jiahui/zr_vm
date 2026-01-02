//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_PARSER_LEXER_H
#define ZR_VM_PARSER_LEXER_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/state.h"

#include <stddef.h>

// 单字符 token 使用其自身的数值代码
// 其他 token 从以下值开始
#define ZR_FIRST_RESERVED (256)

// Token 类型枚举
enum EZrToken {
    // 关键字
    ZR_TK_MODULE = ZR_FIRST_RESERVED,
    ZR_TK_STRUCT,
    ZR_TK_CLASS,
    ZR_TK_INTERFACE,
    ZR_TK_ENUM,
    ZR_TK_TEST,
    ZR_TK_INTERMEDIATE,
    ZR_TK_VAR,
    ZR_TK_PUB,
    ZR_TK_PRI,
    ZR_TK_PRO,
    ZR_TK_IF,
    ZR_TK_ELSE,
    ZR_TK_SWITCH,
    ZR_TK_WHILE,
    ZR_TK_FOR,
    ZR_TK_BREAK,
    ZR_TK_CONTINUE,
    ZR_TK_RETURN,
    ZR_TK_SUPER,
    ZR_TK_NEW,
    ZR_TK_SET,
    ZR_TK_GET,
    ZR_TK_STATIC,
    ZR_TK_IN,
    ZR_TK_OUT,
    ZR_TK_THROW,
    ZR_TK_TRY,
    ZR_TK_CATCH,
    ZR_TK_FINALLY,
    ZR_TK_INFINITY,
    ZR_TK_NEG_INFINITY,
    ZR_TK_NAN,
    // 操作符
    ZR_TK_PARAMS,        // "..."
    ZR_TK_QUESTIONMARK,  // "?"
    ZR_TK_COLON,         // ":"
    ZR_TK_SEMICOLON,     // ";"
    ZR_TK_COMMA,         // ","
    ZR_TK_DOT,           // "."
    ZR_TK_TILDE,         // "~"
    ZR_TK_AT,            // "@"
    ZR_TK_SHARP,         // "#"
    ZR_TK_DOLLAR,        // "$"
    ZR_TK_LPAREN,        // "("
    ZR_TK_RPAREN,        // ")"
    ZR_TK_LBRACE,        // "{"
    ZR_TK_RBRACE,        // "}"
    ZR_TK_LBRACKET,      // "["
    ZR_TK_RBRACKET,      // "]"
    ZR_TK_EQUALS,        // "="
    ZR_TK_PLUS_EQUALS,   // "+="
    ZR_TK_MINUS_EQUALS,  // "-="
    ZR_TK_STAR_EQUALS,   // "*="
    ZR_TK_SLASH_EQUALS,  // "/="
    ZR_TK_PERCENT_EQUALS, // "%="
    ZR_TK_DOUBLE_EQUALS, // "=="
    ZR_TK_BANG_EQUALS,   // "!="
    ZR_TK_BANG,          // "!"
    ZR_TK_LESS_THAN,     // "<"
    ZR_TK_LESS_THAN_EQUALS, // "<="
    ZR_TK_GREATER_THAN,  // ">"
    ZR_TK_GREATER_THAN_EQUALS, // ">="
    ZR_TK_PLUS,          // "+"
    ZR_TK_MINUS,         // "-"
    ZR_TK_STAR,          // "*"
    ZR_TK_SLASH,         // "/"
    ZR_TK_PERCENT,       // "%"
    ZR_TK_AMPERSAND_AMPERSAND, // "&&"
    ZR_TK_PIPE_PIPE,     // "||"
    ZR_TK_RIGHT_ARROW,   // "=>"
    ZR_TK_LEFT_SHIFT,    // "<<"
    ZR_TK_RIGHT_SHIFT,   // ">>"
    ZR_TK_OR,            // "|"
    ZR_TK_XOR,           // "^"
    ZR_TK_AND,           // "&"
    // 字面量
    ZR_TK_BOOLEAN,
    ZR_TK_INTEGER,
    ZR_TK_FLOAT,
    ZR_TK_STRING,
    ZR_TK_CHAR,
    ZR_TK_NULL,
    ZR_TK_IDENTIFIER,
    // 特殊
    ZR_TK_EOS,           // End of stream
};

typedef enum EZrToken EZrToken;

// Token 语义信息
typedef union TZrSemInfo {
    TBool booleanValue;
    TInt64 intValue;
    TDouble floatValue;
    SZrString *stringValue;
    TChar charValue;
} TZrSemInfo;

// Token 结构
typedef struct SZrToken {
    EZrToken token;
    TZrSemInfo seminfo;
} SZrToken;

// 词法分析器状态
typedef struct SZrLexState {
    SZrState *state;           // VM 状态
    const TChar *source;        // 源代码
    TZrSize sourceLength;      // 源代码长度
    TZrSize currentPos;         // 当前位置
    TInt32 currentChar;         // 当前字符
    TInt32 lineNumber;          // 当前行号
    TInt32 lastLine;            // 上一个 token 的行号
    SZrToken t;                 // 当前 token
    SZrToken lookahead;         // 前瞻 token
    // 保存下一个 token 的位置（当使用缓存的 lookahead 时，需要恢复到这个位置）
    TZrSize lookaheadPos;       // 下一个 token 的位置
    TInt32 lookaheadChar;       // 下一个 token 的字符
    TInt32 lookaheadLine;       // 下一个 token 的行号
    TInt32 lookaheadLastLine;   // 下一个 token 的上一个 token 的行号
    SZrString *sourceName;      // 源文件名
    // 缓冲区用于存储 token 文本
    TChar *buffer;
    TZrSize bufferSize;
    TZrSize bufferLength;
} SZrLexState;

// 初始化词法分析器
ZR_PARSER_API void ZrLexerInit(SZrLexState *ls, SZrState *state, const TChar *source, TZrSize sourceLength, SZrString *sourceName);

// 获取下一个 token
ZR_PARSER_API void ZrLexerNext(SZrLexState *ls);

// 查看下一个 token（不消费）
ZR_PARSER_API EZrToken ZrLexerLookahead(SZrLexState *ls);

// 报告语法错误
ZR_PARSER_API void ZrLexerSyntaxError(SZrLexState *ls, const TChar *msg);

// Token 转字符串（用于错误消息）
ZR_PARSER_API const TChar *ZrLexerTokenToString(SZrLexState *ls, EZrToken token);

#endif //ZR_VM_PARSER_LEXER_H

