#include "syntax/token_stream_internal.h"
#include "syntax/token_kind.h"
#include "syntax/lexer_internal.h"

#include <limits.h>
#include <string.h>

#include "zr_vm_core/string.h"

static TZrBool token_stream_token_is_literal(EZrToken token) {
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

static TZrBool token_stream_token_is_punct(EZrToken token) {
    return (TZrInt32)token < ZR_FIRST_RESERVED ||
           (token >= ZR_TK_PARAMS && token <= ZR_TK_AND) ||
           token == ZR_TK_THIN_ARROW || token == ZR_TK_FAT_ARROW ||
           token == ZR_TK_QUESTION_DOT;
}

static EZrTokenTriviaKind token_stream_trivia_kind(
        const TZrChar *bytes, TZrSize start, TZrSize end) {
    TZrBool hasNewline = ZR_FALSE;

    if (bytes == ZR_NULL || start >= end) {
        return ZR_TOKEN_TRIVIA_OTHER;
    }
    if (end - start >= 2u && bytes[start] == '/' && bytes[start + 1u] == '/') {
        return ZR_TOKEN_TRIVIA_LINE_COMMENT;
    }
    if (end - start >= 2u && bytes[start] == '/' && bytes[start + 1u] == '*') {
        return ZR_TOKEN_TRIVIA_BLOCK_COMMENT;
    }
    for (TZrSize index = start; index < end; index++) {
        if (bytes[index] == '\n' || bytes[index] == '\r') {
            hasNewline = ZR_TRUE;
            break;
        }
    }
    return hasNewline ? ZR_TOKEN_TRIVIA_NEWLINE : ZR_TOKEN_TRIVIA_WHITESPACE;
}

static TZrBool token_stream_is_space(TZrChar ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' ||
           ch == '\f' || ch == '\v';
}

static TZrSize token_stream_trivia_end(
        const TZrChar *bytes, TZrSize start, TZrSize end) {
    TZrSize pos = start + 1u;

    if (bytes[start] == '/' && pos < end && bytes[pos] == '/') {
        for (pos++; pos < end && bytes[pos] != '\n' && bytes[pos] != '\r'; pos++) {}
        return pos;
    }
    if (bytes[start] == '/' && pos < end && bytes[pos] == '*') {
        for (pos++; pos + 1u < end; pos++) {
            if (bytes[pos] == '*' && bytes[pos + 1u] == '/') {
                return pos + 2u;
            }
        }
        return 0u;
    }
    if (!token_stream_is_space(bytes[start])) {
        return 0u;
    }
    while (pos < end && token_stream_is_space(bytes[pos])) {
        pos++;
    }
    return pos;
}

static TZrBool token_stream_collect_trivia(
        SZrTokenStream *stream,
        TZrTokenSourceId sourceId,
        TZrSize start,
        TZrSize end,
        SZrTokenTrivia **outTrivia,
        TZrSize *outCount) {
    TZrSize count = 0u;
    TZrSize length;
    TZrSize allocationBytes;
    const TZrChar *bytes = ZrParser_TokenStream_SourceBytes(stream, sourceId, &length);
    SZrTokenTrivia *trivia = ZR_NULL;

    if (outTrivia == ZR_NULL || outCount == ZR_NULL || bytes == ZR_NULL ||
        start > end || end > length) {
        return ZR_FALSE;
    }
    for (TZrSize pos = start; pos < end; count++) {
        TZrSize next = token_stream_trivia_end(bytes, pos, end);
        if (next <= pos) {
            return ZR_FALSE;
        }
        pos = next;
    }
    if (!token_stream_size_mul(count, sizeof(*trivia), &allocationBytes)) {
        return ZR_FALSE;
    }
    if (count != 0u) {
        trivia = token_stream_alloc(stream, allocationBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (trivia == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    for (TZrSize pos = start, index = 0u; pos < end; index++) {
        TZrSize next = token_stream_trivia_end(bytes, pos, end);
        trivia[index].kind = token_stream_trivia_kind(bytes, pos, next);
        trivia[index].text.sourceId = sourceId;
        trivia[index].text.offset = pos;
        trivia[index].text.length = next - pos;
        pos = next;
    }
    *outTrivia = trivia;
    *outCount = count;
    return ZR_TRUE;
}

static TZrBool token_stream_append_scanned_token(
        SZrTokenStream *stream,
        const SZrToken *token,
        TZrTokenSourceId sourceId,
        const SZrTokenTrivia *leading,
        TZrSize leadingCount) {
    SZrTokenNativeSemInfo semInfo = {0};
    SZrTokenText rawText;
    SZrTokenSpan span = token->span;
    TZrSize index;

    span.sourceId = sourceId;
    rawText.sourceId = sourceId;
    rawText.offset = span.startOffset;
    rawText.length = span.endOffset - span.startOffset;
    semInfo.token = token->token;
    semInfo.booleanValue = token->seminfo.booleanValue;
    semInfo.intValue = token->seminfo.intValue;
    semInfo.floatValue = token->seminfo.floatValue;
    semInfo.charValue = (TZrInt32)(TZrUInt8)token->seminfo.charValue;
    if (token_stream_token_is_punct(token->token)) {
        const TZrChar *raw = ZrParser_TokenStream_TextBytes(stream, rawText);
        if (stream->rootCount != 0u) {
            SZrTokenTree *previous = stream->roots[stream->rootCount - 1u];
            if (previous->kind == ZR_TOKEN_TREE_PUNCT &&
                previous->span.sourceId == span.sourceId &&
                previous->span.endOffset == span.startOffset) {
                previous->punctSpacing = ZR_TOKEN_SPACING_JOINT;
            }
        }
        for (TZrSize offset = 0u; offset < rawText.length; offset++) {
            SZrTokenSpan piece;
            SZrTokenText text = rawText;
            TZrUInt32 codepoint = (TZrUInt8)raw[offset];
            EZrTokenSpacing spacing = offset + 1u < rawText.length
                                               ? ZR_TOKEN_SPACING_JOINT
                                               : ZR_TOKEN_SPACING_ALONE;
            if (!ZrParser_TokenSpan_Subspan(span, span.startOffset + offset,
                                            span.startOffset + offset + 1u,
                                            &piece)) {
                return ZR_FALSE;
            }
            text.offset += offset;
            text.length = 1u;
            semInfo.token = ZrParser_Token_SinglePunct(codepoint);
            if (!ZrParser_TokenStream_AppendPunct(
                        stream, semInfo, text, codepoint, spacing, piece,
                        token->syntaxContext, offset == 0u ? leading : ZR_NULL,
                        offset == 0u ? leadingCount : 0u, ZR_NULL, 0u, &index)) {
                return ZR_FALSE;
            }
        }
        return ZR_TRUE;
    }
    if (token->seminfo.stringValue != ZR_NULL &&
        !ZrParser_TokenStream_CopyText(
                stream, ZrCore_String_GetNativeString(token->seminfo.stringValue),
                ZrCore_String_GetByteLength(token->seminfo.stringValue),
                &semInfo.text)) {
        return ZR_FALSE;
    }
    if (token_stream_token_is_literal(token->token)) {
        return ZrParser_TokenStream_AppendLiteral(
                stream, semInfo, rawText, span, token->syntaxContext,
                leading, leadingCount, ZR_NULL, 0u, &index);
    }
    return ZrParser_TokenStream_AppendIdent(
            stream, semInfo, rawText, span, token->syntaxContext,
            leading, leadingCount, ZR_NULL, 0u, &index);
}

typedef struct SZrScannedGroup {
    EZrTokenDelimiter delimiter;
    SZrTokenSpan openSpan;
    TZrSize firstChild;
    SZrTokenTrivia *leading;
    TZrSize leadingCount;
    SZrTokenTrivia *innerTrivia;
    TZrSize innerTriviaCount;
} SZrScannedGroup;

static EZrTokenDelimiter token_stream_open_delimiter(EZrToken token) {
    switch (token) {
        case ZR_TK_LPAREN: return ZR_TOKEN_DELIMITER_PAREN;
        case ZR_TK_LBRACE: return ZR_TOKEN_DELIMITER_BRACE;
        case ZR_TK_LBRACKET: return ZR_TOKEN_DELIMITER_BRACKET;
        default: return ZR_TOKEN_DELIMITER_NONE;
    }
}

static EZrTokenDelimiter token_stream_close_delimiter(EZrToken token) {
    switch (token) {
        case ZR_TK_RPAREN: return ZR_TOKEN_DELIMITER_PAREN;
        case ZR_TK_RBRACE: return ZR_TOKEN_DELIMITER_BRACE;
        case ZR_TK_RBRACKET: return ZR_TOKEN_DELIMITER_BRACKET;
        default: return ZR_TOKEN_DELIMITER_NONE;
    }
}

static TZrBool token_stream_attach_final_trivia(
        SZrTokenStream *stream,
        SZrTokenTrivia *trivia,
        TZrSize count,
        TZrSize firstChild) {
    SZrTokenTree *tree;

    if (count == 0u) {
        token_stream_free(stream, trivia, count * sizeof(*trivia),
                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return ZR_TRUE;
    }
    if (stream->rootCount == firstChild) {
        stream->emptyTrivia = trivia;
        stream->emptyTriviaCount = count;
        return ZR_TRUE;
    }
    tree = stream->roots[stream->rootCount - 1u];
    tree->trailing = trivia;
    tree->trailingCount = count;
    return ZR_TRUE;
}

static TZrBool token_stream_scan(
        SZrState *state, SZrTokenStream *stream,
        TZrTokenSourceId sourceId, const TZrChar *bytes, TZrSize byteLength) {
    SZrLexState lexer;
    SZrScannedGroup *groups = ZR_NULL;
    TZrSize groupCount = 0u;
    TZrSize groupCapacity = 0u;
    TZrSize triviaCursor = 0u;
    TZrBool success = ZR_FALSE;

    ZrParser_Lexer_InitBudget(&lexer, state, bytes == ZR_NULL ? "" : bytes,
                              byteLength, ZR_NULL, token_stream_budget(stream));
    for (;;) {
        SZrTokenTrivia *leading = ZR_NULL;
        TZrSize leadingCount = 0u;
        EZrTokenDelimiter opening = token_stream_open_delimiter(lexer.t.token);
        EZrTokenDelimiter closing = token_stream_close_delimiter(lexer.t.token);
        SZrTokenSpan span = lexer.t.span;
        span.sourceId = sourceId;

        if (lexer.t.hasLexError || span.startOffset < triviaCursor ||
            span.startOffset > byteLength ||
            !ZrParser_ComptimeBudget_TryConsume(
                    token_stream_budget(stream), ZR_PARSER_COMPTIME_BUDGET_FUEL, 1u) ||
            !token_stream_collect_trivia(stream, sourceId, triviaCursor,
                                         span.startOffset, &leading, &leadingCount)) {
            goto cleanup;
        }
        if (lexer.t.token == ZR_TK_EOS) {
            token_stream_attach_final_trivia(stream, leading, leadingCount, 0u);
            stream->endSpan = span;
            success = groupCount == 0u && span.startOffset == byteLength;
            goto cleanup;
        }
        if (opening != ZR_TOKEN_DELIMITER_NONE) {
            if (groupCount == groupCapacity) {
                TZrSize nextCapacity = groupCapacity == 0u ? 16u : groupCapacity * 2u;
                TZrSize allocationBytes;
                SZrScannedGroup *next;
                if (nextCapacity < groupCapacity ||
                    !token_stream_size_mul(nextCapacity, sizeof(*next), &allocationBytes) ||
                    (next = token_stream_alloc(stream, allocationBytes,
                                               ZR_MEMORY_NATIVE_TYPE_ARRAY)) == ZR_NULL) {
                    token_stream_free(stream, leading, leadingCount * sizeof(*leading),
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
                    goto cleanup;
                }
                if (groupCount != 0u) {
                    memcpy(next, groups, groupCount * sizeof(*next));
                }
                token_stream_free(stream, groups, groupCapacity * sizeof(*groups),
                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                groups = next;
                groupCapacity = nextCapacity;
            }
            groups[groupCount].delimiter = opening;
            groups[groupCount].openSpan = span;
            groups[groupCount].firstChild = stream->rootCount;
            groups[groupCount].leading = leading;
            groups[groupCount].leadingCount = leadingCount;
            groups[groupCount].innerTrivia = ZR_NULL;
            groups[groupCount].innerTriviaCount = 0u;
            groupCount++;
        } else if (closing != ZR_TOKEN_DELIMITER_NONE) {
            SZrScannedGroup *group;
            SZrTokenSpan groupSpan;
            TZrSize childCount;
            TZrSize outIndex;
            if (groupCount == 0u || groups[groupCount - 1u].delimiter != closing) {
                token_stream_free(stream, leading, leadingCount * sizeof(*leading),
                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                goto cleanup;
            }
            group = &groups[groupCount - 1u];
            childCount = stream->rootCount - group->firstChild;
            if (childCount == 0u) {
                group->innerTrivia = leading;
                group->innerTriviaCount = leadingCount;
            } else {
                token_stream_attach_final_trivia(
                        stream, leading, leadingCount, group->firstChild);
            }
            if (!ZrParser_TokenSpan_Join(group->openSpan, span, &groupSpan)) {
                goto cleanup;
            }
            stream->rootCount = group->firstChild;
            if (!token_stream_make_group(
                        stream, closing, group->openSpan, span,
                        stream->roots == ZR_NULL ? ZR_NULL : stream->roots + group->firstChild,
                        childCount, groupSpan, lexer.t.syntaxContext,
                        group->leading, group->leadingCount, ZR_NULL, 0u, &outIndex)) {
                goto cleanup;
            }
            stream->roots[outIndex]->innerTrivia = group->innerTrivia;
            stream->roots[outIndex]->innerTriviaCount = group->innerTriviaCount;
            group->innerTrivia = ZR_NULL;
            group->innerTriviaCount = 0u;
            token_stream_free(stream, group->leading,
                              group->leadingCount * sizeof(*group->leading),
                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
            groupCount--;
        } else {
            TZrBool appended = token_stream_append_scanned_token(
                    stream, &lexer.t, sourceId, leading, leadingCount);
            token_stream_free(stream, leading, leadingCount * sizeof(*leading),
                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
            if (!appended) {
                goto cleanup;
            }
        }
        triviaCursor = span.endOffset;
        ZrParser_Lexer_Next(&lexer);
    }
cleanup:
    for (TZrSize index = 0u; index < groupCount; index++) {
        token_stream_free(stream, groups[index].leading,
                          groups[index].leadingCount * sizeof(*groups[index].leading),
                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
        token_stream_free(stream, groups[index].innerTrivia,
                          groups[index].innerTriviaCount * sizeof(*groups[index].innerTrivia),
                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    token_stream_free(stream, groups, groupCapacity * sizeof(*groups),
                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    ZrParser_Lexer_Free(&lexer);
    return success;
}

TZrBool ZrParser_TokenStream_FromText(
        SZrState *state,
        SZrSyntaxContextTable *contextTable,
        const TZrChar *sourceName,
        TZrSize sourceNameLength,
        const TZrChar *bytes,
        TZrSize byteLength,
        SZrParserComptimeBudget *budget,
        SZrTokenStream **outStream) {
    SZrParserComptimeBudget shadow;
    SZrParserComptimeBudgetUsage initialUsage;
    SZrTokenStream *stream;
    TZrTokenSourceId sourceId;
    TZrBool success;

    if (outStream == ZR_NULL) {
        return ZR_FALSE;
    }
    *outStream = ZR_NULL;
    if (state == ZR_NULL || state->global == ZR_NULL ||
        contextTable == ZR_NULL || (byteLength != 0u && bytes == ZR_NULL) ||
        (sourceNameLength != 0u && sourceName == ZR_NULL)) {
        return ZR_FALSE;
    }
    if (budget != ZR_NULL) {
        shadow = *budget;
    } else {
        token_stream_default_budget(&shadow);
    }
    initialUsage = shadow.usage;
    stream = ZrParser_TokenStream_Create(state->global, contextTable, &shadow);
    success = stream != ZR_NULL && ZrParser_TokenStream_AddSource(
            stream, sourceName, sourceNameLength, bytes, byteLength, &sourceId);
    if (success) {
        success = token_stream_scan(state, stream, sourceId, bytes, byteLength);
    }
    if (success && budget != ZR_NULL) {
        success = token_stream_commit_shadow(budget, &initialUsage, &shadow);
    }
    if (!success) {
        if (budget != ZR_NULL && shadow.exceededResource != ZR_PARSER_COMPTIME_BUDGET_NONE) {
            budget->exceededResource = shadow.exceededResource;
            budget->exceededLimit = shadow.exceededLimit;
            budget->requestedUsage = shadow.requestedUsage;
        }
        ZrParser_TokenStream_Release(stream);
        *outStream = ZR_NULL;
        return ZR_FALSE;
    }
    stream->budget = budget;
    if (budget == ZR_NULL) {
        stream->fallbackBudget = shadow;
    }
    ZrParser_TokenStream_Seal(stream);
    *outStream = stream;
    return ZR_TRUE;
}
