#include "syntax/token_kind.h"

TZrBool ZrParser_Token_IsLiteral(EZrToken token) {
    switch (token) {
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
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

TZrBool ZrParser_Token_IsPunct(EZrToken token) {
    return (TZrInt32)token >= 0 && ((TZrInt32)token < ZR_FIRST_RESERVED ||
           (token >= ZR_TK_PARAMS && token <= ZR_TK_AND) ||
           token == ZR_TK_THIN_ARROW || token == ZR_TK_FAT_ARROW ||
           token == ZR_TK_QUESTION_DOT);
}

EZrToken ZrParser_Token_SinglePunct(TZrUInt32 codepoint) {
    switch (codepoint) {
        case '?': return ZR_TK_QUESTIONMARK;
        case ':': return ZR_TK_COLON;
        case ';': return ZR_TK_SEMICOLON;
        case ',': return ZR_TK_COMMA;
        case '.': return ZR_TK_DOT;
        case '~': return ZR_TK_TILDE;
        case '@': return ZR_TK_AT;
        case '#': return ZR_TK_SHARP;
        case '$': return ZR_TK_DOLLAR;
        case '=': return ZR_TK_EQUALS;
        case '!': return ZR_TK_BANG;
        case '<': return ZR_TK_LESS_THAN;
        case '>': return ZR_TK_GREATER_THAN;
        case '+': return ZR_TK_PLUS;
        case '-': return ZR_TK_MINUS;
        case '*': return ZR_TK_STAR;
        case '/': return ZR_TK_SLASH;
        case '%': return ZR_TK_PERCENT;
        case '&': return ZR_TK_AND;
        case '|': return ZR_TK_OR;
        case '^': return ZR_TK_XOR;
        default: return (EZrToken)codepoint;
    }
}

EZrToken ZrParser_Token_CompoundPunct(TZrUInt32 first, TZrUInt32 second,
                                      TZrUInt32 third, TZrBool hasThird) {
    if (hasThird && first == '.' && second == '.' && third == '.') {
        return ZR_TK_PARAMS;
    }
    if (first == '?' && second == '.') return ZR_TK_QUESTION_DOT;
    if (first == '.' && second == '.') return ZR_TK_DOT_DOT;
    if (first == '=' && second == '=') return ZR_TK_DOUBLE_EQUALS;
    if (first == '=' && second == '>') return ZR_TK_FAT_ARROW;
    if (first == '!' && second == '=') return ZR_TK_BANG_EQUALS;
    if (first == '<' && second == '=') return ZR_TK_LESS_THAN_EQUALS;
    if (first == '>' && second == '=') return ZR_TK_GREATER_THAN_EQUALS;
    if (first == '+' && second == '=') return ZR_TK_PLUS_EQUALS;
    if (first == '-' && second == '=') return ZR_TK_MINUS_EQUALS;
    if (first == '-' && second == '>') return ZR_TK_THIN_ARROW;
    if (first == '*' && second == '=') return ZR_TK_STAR_EQUALS;
    if (first == '/' && second == '=') return ZR_TK_SLASH_EQUALS;
    if (first == '%' && second == '=') return ZR_TK_PERCENT_EQUALS;
    if (first == '<' && second == '<') return ZR_TK_LEFT_SHIFT;
    if (first == '>' && second == '>') return ZR_TK_RIGHT_SHIFT;
    if (first == '&' && second == '&') return ZR_TK_AMPERSAND_AMPERSAND;
    if (first == '|' && second == '|') return ZR_TK_PIPE_PIPE;
    return ZR_TK_EOS;
}
