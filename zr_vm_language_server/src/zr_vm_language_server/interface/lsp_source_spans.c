#include "interface/lsp_interface_internal.h"

/** 语义查询的轻量文本门控所识别的三类字符串界符。 */
static TZrBool lsp_source_span_is_quote(TZrChar value) {
    return value == '"' || value == '\'' || value == '`';
}

/** 避免把普通字符串内容误判为标识符；此扫描器不是完整的语言 lexer。 */
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

/**
 * @brief 在 hover、定义等语义查询前屏蔽注释与普通字符串内的光标。
 * @note 这是轻量文本门控；调用方仍需语义查询确认标识符身份。
 * BUG: 反引号模板字符串内的 `${expr}` 会被整体当成字符串跳过，
 * 而 parser_literals.c 的模板解析将该片段作为表达式；插值中的语义查询和补全会被错误屏蔽。
 */
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

/** 补全和签名帮助容许光标位于代码片段末端，故同时检查光标前一个字节。 */
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
