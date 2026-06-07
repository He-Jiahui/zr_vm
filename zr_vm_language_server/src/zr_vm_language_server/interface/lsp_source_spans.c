#include "interface/lsp_interface_internal.h"

static TZrBool lsp_source_span_is_quote(TZrChar value) {
    return value == '"' || value == '\'' || value == '`';
}

static TZrSize lsp_source_span_skip_string(const TZrChar *content,
                                           TZrSize contentLength,
                                           TZrSize cursor) {
    TZrChar quote;
    TZrBool escaped = ZR_FALSE;

    if (content == ZR_NULL || cursor >= contentLength || !lsp_source_span_is_quote(content[cursor])) {
        return cursor;
    }

    quote = content[cursor++];
    while (cursor < contentLength) {
        TZrChar ch = content[cursor++];
        if (escaped) {
            escaped = ZR_FALSE;
            continue;
        }
        if (ch == '\\') {
            escaped = ZR_TRUE;
            continue;
        }
        if (ch == quote) {
            break;
        }
    }

    return cursor;
}

TZrBool ZrLanguageServer_Lsp_IsOffsetInCodeSpan(const TZrChar *content,
                                                TZrSize contentLength,
                                                TZrSize offset) {
    TZrSize cursor = 0;

    if (content == ZR_NULL || contentLength == 0 || offset >= contentLength) {
        return ZR_FALSE;
    }

    while (cursor < contentLength && cursor <= offset) {
        TZrSize spanEnd;

        if (cursor + 1 < contentLength && content[cursor] == '/' && content[cursor + 1] == '/') {
            cursor += 2;
            while (cursor < contentLength && content[cursor] != '\n' && content[cursor] != '\r') {
                cursor++;
            }
            if (offset < cursor) {
                return ZR_FALSE;
            }
            continue;
        }

        if (cursor + 1 < contentLength && content[cursor] == '/' && content[cursor + 1] == '*') {
            cursor += 2;
            while (cursor + 1 < contentLength &&
                   !(content[cursor] == '*' && content[cursor + 1] == '/')) {
                cursor++;
            }
            cursor = cursor + 1 < contentLength ? cursor + 2 : contentLength;
            if (offset < cursor) {
                return ZR_FALSE;
            }
            continue;
        }

        spanEnd = lsp_source_span_skip_string(content, contentLength, cursor);
        if (spanEnd != cursor) {
            cursor = spanEnd;
            if (offset < cursor) {
                return ZR_FALSE;
            }
            continue;
        }

        if (cursor == offset) {
            return ZR_TRUE;
        }
        cursor++;
    }

    return ZR_FALSE;
}

TZrBool ZrLanguageServer_Lsp_IsCursorOffsetInCodeSpan(const TZrChar *content,
                                                      TZrSize contentLength,
                                                      TZrSize offset) {
    if (content == ZR_NULL) {
        return ZR_FALSE;
    }
    if (contentLength == 0) {
        return ZR_TRUE;
    }
    if (offset < contentLength && ZrLanguageServer_Lsp_IsOffsetInCodeSpan(content, contentLength, offset)) {
        return ZR_TRUE;
    }
    if (offset > 0 && ZrLanguageServer_Lsp_IsOffsetInCodeSpan(content, contentLength, offset - 1)) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}
