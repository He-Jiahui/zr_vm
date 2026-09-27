//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_PARSER_LEXER_H
#define ZR_VM_PARSER_LEXER_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/state.h"

#include <stddef.h>

// 单字符 token 使用其字节值；保留 token 从 256 起，新增枚举项只能追加以维持既有 ID。
#define ZR_FIRST_RESERVED (256)

/** @brief 词法单元编号；单字节未知字符在保留区间以下，已定义 token 保持编号稳定。 */
enum EZrToken {
    // 关键字
    ZR_TK_MODULE = ZR_FIRST_RESERVED,
    ZR_TK_STRUCT,
    ZR_TK_CLASS,
    ZR_TK_ABSTRACT,
    ZR_TK_VIRTUAL,
    ZR_TK_OVERRIDE,
    ZR_TK_FINAL,
    ZR_TK_SHADOW,
    ZR_TK_INTERFACE,
    ZR_TK_ENUM,
    ZR_TK_TEST,
    ZR_TK_INTERMEDIATE,
    ZR_TK_VAR,
    ZR_TK_USING,
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
    ZR_TK_CONST,
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
    ZR_TK_DOT_DOT,       // ".."
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
    ZR_TK_RIGHT_ARROW,   // "=>" or "->"
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
    ZR_TK_TEMPLATE_STRING,
    ZR_TK_CHAR,
    ZR_TK_NULL,
    ZR_TK_IDENTIFIER,
    // 特殊
    ZR_TK_EOS,           // End of stream
    ZR_TK_UNION,         // union keyword (appended to avoid existing token id drift)
    ZR_TK_FN,            // fn keyword
    ZR_TK_REF,           // ref keyword
    ZR_TK_THIN_ARROW,    // -> function type delimiter
    ZR_TK_FAT_ARROW,     // => expression body delimiter
    ZR_TK_LET,           // immutable binding keyword (appended for token id stability)
    ZR_TK_YIELD,         // yield statement keyword (appended for token id stability)
    ZR_TK_TYPEID,        // typeid(TypeRef) intrinsic (appended for token id stability)
    ZR_TK_TYPEOF,        // typeof(expr) intrinsic (appended for token id stability)
    ZR_TK_QUESTION_DOT,  // "?." optional postfix operator
    ZR_TK_SHARE,         // share(owner) ownership intrinsic
    ZR_TK_DEGRADE,       // degrade(shared) ownership intrinsic
    ZR_TK_WAKE,          // wake(weak) ownership intrinsic
    ZR_TK_INTO_GC,       // intoGc(owner) ownership intrinsic
    ZR_TK_DROP,          // drop(owner) ownership intrinsic
};

typedef enum EZrToken EZrToken;

/** @brief 当前 token 的值载荷；仅与 token 种类对应的字段有效。
 * @note stringValue 由 VM 字符串系统持有，词法状态释放时不单独释放。 */
typedef struct TZrSemInfo {
    TZrBool booleanValue;
    TZrInt64 intValue;
    TZrDouble floatValue;
    SZrString *stringValue;
    TZrChar charValue;
} TZrSemInfo;

/** @brief 一次扫描的种类、载荷与词法错误；诊断文本借用报告方消息。 */
typedef struct SZrToken {
    EZrToken token;
    TZrSemInfo seminfo;
    TZrBool hasLexError;
    const TZrChar *lexErrorMessage;
} SZrToken;

/** @brief 顺序扫描与一 token 前瞻的游标。
 * @note source 和 sourceName 均借用调用方资源；buffer 由 Init 分配、Free 释放。
 *       lookahead 含扫描后的游标快照，Next 消费缓存时须恢复整组位置字段。 */
typedef struct SZrLexState {
    SZrState *state;           // VM 状态
    const TZrChar *source;        // 源代码
    TZrSize sourceLength;      // 源代码长度
    TZrSize currentPos;         // 当前位置
    TZrInt32 currentChar;         // 当前字符
    TZrInt32 lineNumber;          // 当前行号
    TZrInt32 lastLine;            // 上一个 token 的行号
    TZrSize currentLineStartOffset;
    TZrSize tokenStartOffset;
    TZrSize tokenStartLineStart;
    TZrInt32 tokenStartLine;
    SZrToken t;                 // 当前 token
    SZrToken lookahead;         // 前瞻 token
    // 保存下一个 token 的位置（当使用缓存的 lookahead 时，需要恢复到这个位置）
    TZrSize lookaheadPos;       // 下一个 token 的位置
    TZrInt32 lookaheadChar;       // 下一个 token 的字符
    TZrInt32 lookaheadLine;       // 下一个 token 的行号
    TZrInt32 lookaheadLastLine;   // 下一个 token 的上一个 token 的行号
    TZrSize lookaheadCurrentLineStartOffset;
    TZrSize lookaheadTokenStartOffset;
    TZrSize lookaheadTokenStartLineStart;
    TZrInt32 lookaheadTokenStartLine;
    TZrSize filePositionCacheOffset;
    TZrSize filePositionCacheLineStart;
    TZrInt32 filePositionCacheLine;
    SZrString *sourceName;      // 源文件名
    TZrBool currentTokenHadError;
    const TZrChar *currentTokenErrorMessage;
    // 缓冲区用于存储 token 文本
    TZrChar *buffer;
    TZrSize bufferSize;
    TZrSize bufferLength;
} SZrLexState;

/** @brief 绑定源文本、分配扫描缓冲并预读第一个 token。
 * @pre ls、state、source 非空，source 在 Free 前持续有效；sourceLength 是可读字节数。
 * @note 函数没有失败返回值，调用方不能仅靠其结果判断缓冲分配是否成功。 */
ZR_PARSER_API void ZrParser_Lexer_Init(SZrLexState *ls, SZrState *state, const TZrChar *source, TZrSize sourceLength, SZrString *sourceName);

/** @brief 释放 Init 创建的扫描缓冲；不释放源文本、源名或 VM 字符串。 */
ZR_PARSER_API void ZrParser_Lexer_Free(SZrLexState *ls);

/** @brief 消费当前 token 并更新 t；若已前瞻则消费缓存及其游标快照。 */
ZR_PARSER_API void ZrParser_Lexer_Next(SZrLexState *ls);

/** @brief 扫描并缓存下一个 token，返回其种类而保持当前 t 与游标不变。
 * @note 前瞻可能分配 VM 字符串，也可能触发词法诊断。 */
ZR_PARSER_API EZrToken ZrParser_Lexer_Lookahead(SZrLexState *ls);

/** @brief 标记当前扫描 token 的首个词法错误，并打印源位置及片段。
 * @note msg 原样借入 token，调用方须保持它到 token 不再使用；函数直接写 stdout，
 *       不经解析器的错误回调或 suppressErrorOutput。 */
ZR_PARSER_API void ZrParser_Lexer_SyntaxError(SZrLexState *ls, const TZrChar *msg);

/** @brief 返回 token 的诊断拼写。
 * @return 保留 token 返回静态常量；单字符 token 返回共享静态缓冲，下一次同类调用即覆盖，调用方不得长期保存。 */
ZR_PARSER_API const TZrChar *ZrParser_Lexer_TokenToString(SZrLexState *ls, EZrToken token);

#endif //ZR_VM_PARSER_LEXER_H
