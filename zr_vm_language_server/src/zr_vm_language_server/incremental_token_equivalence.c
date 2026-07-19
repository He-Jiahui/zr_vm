#include "incremental_token_equivalence.h"

#include "zr_vm_parser/parser.h"

#include <string.h>

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

static TZrBool incremental_token_strings_equal(
        SZrString *left,
        SZrString *right) {
    const TZrChar *leftText;
    const TZrChar *rightText;
    TZrSize leftLength;
    TZrSize rightLength;

    if (left == right) {
        return ZR_TRUE;
    }
    incremental_token_string_view(left, &leftText, &leftLength);
    incremental_token_string_view(right, &rightText, &rightLength);
    return leftText != ZR_NULL && rightText != ZR_NULL &&
           leftLength == rightLength &&
           memcmp(leftText, rightText, leftLength) == 0;
}

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
    ZrParser_State_Free(&newParser);
    ZrParser_State_Free(&oldParser);
    return equivalent;
}
