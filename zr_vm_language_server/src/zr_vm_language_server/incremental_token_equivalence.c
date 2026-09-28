#include "incremental_token_equivalence.h"

#include "zr_vm_parser/parser.h"

#include <string.h>

/** @brief 按 SZrString 的实际长度暴露 token 文本，避免短串与长串表示差异。 */
static void incremental_token_string_view(
        SZrString *value,
        const TZrChar **text,
        TZrSize *length) {
    *text = ZR_NULL;
    *length = 0;
    if (value == ZR_NULL) {
        return;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        *text = ZrCore_String_GetNativeStringShort(value);
        *length = value->shortStringLength;
        return;
    }
    *text = ZrCore_String_GetNativeString(value);
    *length = value->longStringLength;
}

/** @brief 比较 token 文本值；仅同指针可直接复用，其他情况须比较长度与字节。 */
static TZrBool incremental_token_strings_equal(
        SZrString *left,
        SZrString *right) {
    const TZrChar *leftText;
    const TZrChar *rightText;
    TZrSize leftLength;
    TZrSize rightLength;

    /* TODO: 两个 NULL 也会被视作相等；核对 lexer 在 String_Create 失败时是否总会
     *       标出词法错误，避免分配失败让不同标识符走 AST 复用。 */
    if (left == right) {
        return ZR_TRUE;
    }
    incremental_token_string_view(left, &leftText, &leftLength);
    incremental_token_string_view(right, &rightText, &rightLength);
    return leftText != ZR_NULL && rightText != ZR_NULL &&
           leftLength == rightLength &&
           memcmp(leftText, rightText, leftLength) == 0;
}

/** @brief 判断 lexer 产生的 token 语义值是否一致，词法错误禁止快速复用。 */
static TZrBool incremental_token_values_equal(
        const SZrToken *left,
        const SZrToken *right) {
    if (left->token != right->token ||
        left->hasLexError || right->hasLexError) {
        return ZR_FALSE;
    }

    switch (left->token) {
        case ZR_TK_BOOLEAN:
            return left->seminfo.booleanValue == right->seminfo.booleanValue;
        case ZR_TK_INTEGER:
            return left->seminfo.intValue == right->seminfo.intValue &&
                   incremental_token_strings_equal(
                           left->seminfo.stringValue,
                           right->seminfo.stringValue);
        case ZR_TK_FLOAT:
            return memcmp(
                           &left->seminfo.floatValue,
                           &right->seminfo.floatValue,
                           sizeof(left->seminfo.floatValue)) == 0 &&
                   incremental_token_strings_equal(
                           left->seminfo.stringValue,
                           right->seminfo.stringValue);
        case ZR_TK_STRING:
        case ZR_TK_TEMPLATE_STRING:
        case ZR_TK_IDENTIFIER:
            return incremental_token_strings_equal(
                    left->seminfo.stringValue,
                    right->seminfo.stringValue);
        case ZR_TK_CHAR:
            return left->seminfo.charValue == right->seminfo.charValue;
        default:
            return ZR_TRUE;
    }
}

/** @brief 位置也必须一致，保留旧 AST 时引用、诊断和编辑器范围才能继续有效。 */
static TZrBool incremental_token_positions_equal(
        const SZrLexState *left,
        const SZrLexState *right) {
    return left->tokenStartOffset == right->tokenStartOffset &&
           left->tokenStartLine == right->tokenStartLine &&
           left->tokenStartLineStart == right->tokenStartLineStart &&
           left->currentPos == right->currentPos &&
           left->currentChar == right->currentChar &&
           left->lineNumber == right->lineNumber &&
           left->lastLine == right->lastLine &&
           left->currentLineStartOffset == right->currentLineStartOffset;
}

/**
 * @brief 对等长编辑重新词法化两份文本，决定能否保留已有 AST 与诊断。
 * @note 由 update_file 在存在非回退 AST 时调用；任何不等或词法错误都让解析器走重解析。
 * BUG: State_Init 预读 token 时 lexer 可将未终止字符串错误直接写入 stdout；
 *      stdio didChange 可到达本路径，污染 JSON-RPC 输出。
 */
TZrBool ZrLanguageServer_IncrementalTokenStreams_AreEquivalent(
        SZrState *state,
        SZrString *uri,
        const TZrChar *oldContent,
        TZrSize oldContentLength,
        const TZrChar *newContent,
        TZrSize newContentLength) {
    SZrParserState oldParser;
    SZrParserState newParser;
    TZrBool equivalent = ZR_FALSE;
    TZrSize tokenCount = 0;

    if (state == ZR_NULL || oldContent == ZR_NULL || newContent == ZR_NULL ||
        oldContentLength != newContentLength) {
        return ZR_FALSE;
    }

    ZrParser_State_Init(
            &oldParser,
            state,
            oldContent,
            oldContentLength,
            uri);
    ZrParser_State_Init(
            &newParser,
            state,
            newContent,
            newContentLength,
            uri);
    if (oldParser.hasError || newParser.hasError ||
        oldParser.lexer == ZR_NULL || newParser.lexer == ZR_NULL) {
        goto cleanup;
    }

    while (tokenCount <= oldContentLength + 1) {
        if (!incremental_token_values_equal(
                    &oldParser.lexer->t,
                    &newParser.lexer->t) ||
            !incremental_token_positions_equal(
                    oldParser.lexer,
                    newParser.lexer)) {
            goto cleanup;
        }
        if (oldParser.lexer->t.token == ZR_TK_EOS) {
            equivalent = ZR_TRUE;
            break;
        }

        ZrParser_Lexer_Next(oldParser.lexer);
        ZrParser_Lexer_Next(newParser.lexer);
        tokenCount++;
    }

cleanup:
    /* 两个 lexer 无论词法失败还是提前发现差异，都在同一出口归还状态。 */
    ZrParser_State_Free(&newParser);
    ZrParser_State_Free(&oldParser);
    return equivalent;
}
