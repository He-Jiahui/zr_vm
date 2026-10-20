#include "syntax/lexer_internal.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

#include <ctype.h>
#include <string.h>

// token 拼写与编号的配对项；TokenToString 按编号查找，不依赖表顺序。
typedef struct {
    const TZrChar *name;
    EZrToken token;
} SZrTokenInfo;

// 诊断拼写表，覆盖关键字、符号及字面量占位名；单字符未知 token 另走静态缓冲。
static const SZrTokenInfo zr_token_info[] = {
    // 关键字
    {"module", ZR_TK_MODULE},
    {"struct", ZR_TK_STRUCT},
    {"class", ZR_TK_CLASS},
    {"abstract", ZR_TK_ABSTRACT},
    {"virtual", ZR_TK_VIRTUAL},
    {"override", ZR_TK_OVERRIDE},
    {"final", ZR_TK_FINAL},
    {"shadow", ZR_TK_SHADOW},
    {"interface", ZR_TK_INTERFACE},
    {"enum", ZR_TK_ENUM},
    {"union", ZR_TK_UNION},
    {"test", ZR_TK_TEST},
    {"intermediate", ZR_TK_INTERMEDIATE},
    {"var", ZR_TK_VAR},
    {"let", ZR_TK_LET},
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
    {"yield", ZR_TK_YIELD},
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
    {"fn", ZR_TK_FN},
    {"ref", ZR_TK_REF},
    {"typeid", ZR_TK_TYPEID},
    {"typeof", ZR_TK_TYPEOF},
    {"share", ZR_TK_SHARE},
    {"degrade", ZR_TK_DEGRADE},
    {"wake", ZR_TK_WAKE},
    {"intoGc", ZR_TK_INTO_GC},
    {"drop", ZR_TK_DROP},
    // 操作符
    {"...", ZR_TK_PARAMS},
    {"?", ZR_TK_QUESTIONMARK},
    {"?.", ZR_TK_QUESTION_DOT},
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
    {"->", ZR_TK_THIN_ARROW},
    {"=>", ZR_TK_FAT_ARROW},
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

// 标识符扫描后的保留词判定表，以空名字作为遍历终点。
static const struct {
    const TZrChar *name;
    EZrToken token;
} zr_keywords[] = {{"module", ZR_TK_MODULE},
                   {"struct", ZR_TK_STRUCT},
                   {"class", ZR_TK_CLASS},
                   {"abstract", ZR_TK_ABSTRACT},
                   {"virtual", ZR_TK_VIRTUAL},
                   {"override", ZR_TK_OVERRIDE},
                   {"final", ZR_TK_FINAL},
                   {"shadow", ZR_TK_SHADOW},
                   {"interface", ZR_TK_INTERFACE},
                   {"enum", ZR_TK_ENUM},
                   {"union", ZR_TK_UNION},
                   {"fn", ZR_TK_FN},
                   {"ref", ZR_TK_REF},
                   {"typeid", ZR_TK_TYPEID},
                   {"typeof", ZR_TK_TYPEOF},
                   {"share", ZR_TK_SHARE},
                   {"degrade", ZR_TK_DEGRADE},
                   {"wake", ZR_TK_WAKE},
                   {"intoGc", ZR_TK_INTO_GC},
                   {"drop", ZR_TK_DROP},
                   {"test", ZR_TK_TEST},
                   {"intermediate", ZR_TK_INTERMEDIATE},
                   {"var", ZR_TK_VAR},
                   {"let", ZR_TK_LET},
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
                   {"yield", ZR_TK_YIELD},
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

// 按 sourceLength 推进字节游标；CR 和 CRLF 对语法统一为换行并更新行首偏移。
EZrToken ZrParser_Lexer_FindKeyword(const TZrChar *name, TZrSize length) {
    for (TZrSize i = 0; zr_keywords[i].name != ZR_NULL; i++) {
        if (strlen(zr_keywords[i].name) == length && strncmp(zr_keywords[i].name, name, length) == 0) {
            return zr_keywords[i].token;
        }
    }
    return ZR_TK_IDENTIFIER;
}

// 扫描 ASCII 标识符；普通名字复制进 VM 字符串，关键字只返回编号。
// 保留 token 用表中常量；单字符 token 用共享缓冲，只能在下次同类调用前借用。
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
