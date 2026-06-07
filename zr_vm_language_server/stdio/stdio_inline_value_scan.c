#include "stdio_inline_value_scan.h"

#include <string.h>

int ZrStdioInlineValue_IsIdentifierStart(char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_';
}

int ZrStdioInlineValue_IsIdentifierPart(char ch) {
    return ZrStdioInlineValue_IsIdentifierStart(ch) || (ch >= '0' && ch <= '9');
}

int ZrStdioInlineValue_IsKeywordAt(const char *content,
                                   size_t lineStart,
                                   size_t lineEnd,
                                   size_t offset,
                                   const char *keyword) {
    size_t keywordLength;

    if (content == NULL || keyword == NULL) {
        return 0;
    }

    keywordLength = strlen(keyword);
    if (offset + keywordLength > lineEnd) {
        return 0;
    }
    if (offset > lineStart && ZrStdioInlineValue_IsIdentifierPart(content[offset - 1])) {
        return 0;
    }
    if (strncmp(content + offset, keyword, keywordLength) != 0) {
        return 0;
    }
    if (offset + keywordLength < lineEnd &&
        ZrStdioInlineValue_IsIdentifierPart(content[offset + keywordLength])) {
        return 0;
    }
    return 1;
}

static int inline_value_range_has_nonspace(const char *content, size_t start, size_t end) {
    if (content == NULL || end <= start) {
        return 0;
    }

    for (size_t offset = start; offset < end; offset++) {
        if (content[offset] != ' ' && content[offset] != '\t') {
            return 1;
        }
    }

    return 0;
}

static size_t inline_value_skip_string_literal(const char *content, size_t quoteOffset, size_t lineEnd) {
    char quote;
    size_t cursor;
    int escaped = 0;

    if (content == NULL || quoteOffset >= lineEnd) {
        return quoteOffset;
    }

    quote = content[quoteOffset];
    cursor = quoteOffset + 1;
    while (cursor < lineEnd) {
        char current = content[cursor++];
        if (escaped) {
            escaped = 0;
            continue;
        }
        if (current == '\\') {
            escaped = 1;
            continue;
        }
        if (current == quote) {
            break;
        }
    }

    return cursor;
}

int ZrStdioInlineValue_FindCodeSpanOnLine(const char *content,
                                          size_t lineStart,
                                          size_t lineEnd,
                                          int *inBlockComment,
                                          size_t *outStart,
                                          size_t *outEnd) {
    size_t cursor = lineStart;

    if (outStart != NULL) {
        *outStart = lineStart;
    }
    if (outEnd != NULL) {
        *outEnd = lineStart;
    }
    if (content == NULL || inBlockComment == NULL || outStart == NULL || outEnd == NULL) {
        return 0;
    }

    while (cursor < lineEnd) {
        size_t codeStart;

        if (*inBlockComment) {
            while (cursor + 1 < lineEnd &&
                   !(content[cursor] == '*' && content[cursor + 1] == '/')) {
                cursor++;
            }
            if (cursor + 1 >= lineEnd) {
                return 0;
            }
            *inBlockComment = 0;
            cursor += 2;
            continue;
        }

        codeStart = cursor;
        while (cursor < lineEnd) {
            char current = content[cursor];
            char next = cursor + 1 < lineEnd ? content[cursor + 1] : '\0';

            if (current == '/' && next == '/') {
                if (inline_value_range_has_nonspace(content, codeStart, cursor)) {
                    *outStart = codeStart;
                    *outEnd = cursor;
                    return 1;
                }
                return 0;
            }
            if (current == '/' && next == '*') {
                size_t close = cursor + 2;
                while (close + 1 < lineEnd &&
                       !(content[close] == '*' && content[close + 1] == '/')) {
                    close++;
                }
                *inBlockComment = close + 1 >= lineEnd;
                if (inline_value_range_has_nonspace(content, codeStart, cursor)) {
                    *outStart = codeStart;
                    *outEnd = cursor;
                    return 1;
                }
                cursor = *inBlockComment ? lineEnd : close + 2;
                codeStart = cursor;
                break;
            }
            if (current == '"' || current == '\'' || current == '`') {
                if (inline_value_range_has_nonspace(content, codeStart, cursor)) {
                    *outStart = codeStart;
                    *outEnd = cursor;
                    return 1;
                }
                cursor = inline_value_skip_string_literal(content, cursor, lineEnd);
                codeStart = cursor;
                break;
            }
            cursor++;
        }

        if (cursor >= lineEnd) {
            if (inline_value_range_has_nonspace(content, codeStart, lineEnd)) {
                *outStart = codeStart;
                *outEnd = lineEnd;
                return 1;
            }
            return 0;
        }
    }

    return 0;
}

size_t ZrStdioInlineValue_FindExpressionStatementEnd(const char *content,
                                                     size_t start,
                                                     size_t limit) {
    size_t offset;
    int parenDepth = 0;
    int bracketDepth = 0;
    int braceDepth = 0;
    int inSingleQuote = 0;
    int inDoubleQuote = 0;
    int inLineComment = 0;
    int inBlockComment = 0;
    int escaped = 0;

    if (content == NULL) {
        return start;
    }

    for (offset = start; offset < limit; offset++) {
        char current = content[offset];
        char next = offset + 1 < limit ? content[offset + 1] : '\0';

        if (inLineComment) {
            if (current == '\n' || current == '\r') {
                inLineComment = 0;
            }
            continue;
        }

        if (inBlockComment) {
            if (current == '*' && next == '/') {
                inBlockComment = 0;
                offset++;
            }
            continue;
        }

        if (inSingleQuote || inDoubleQuote) {
            if (escaped) {
                escaped = 0;
                continue;
            }
            if (current == '\\') {
                escaped = 1;
                continue;
            }
            if ((inSingleQuote && current == '\'') ||
                (inDoubleQuote && current == '"')) {
                inSingleQuote = 0;
                inDoubleQuote = 0;
            }
            continue;
        }

        if (current == '/' && next == '/') {
            inLineComment = 1;
            offset++;
            continue;
        }
        if (current == '/' && next == '*') {
            inBlockComment = 1;
            offset++;
            continue;
        }
        if (current == '\'') {
            inSingleQuote = 1;
            continue;
        }
        if (current == '"') {
            inDoubleQuote = 1;
            continue;
        }

        if (current == ';' &&
            parenDepth == 0 &&
            bracketDepth == 0 &&
            braceDepth == 0) {
            return offset;
        }
        if (current == '}' &&
            parenDepth == 0 &&
            bracketDepth == 0 &&
            braceDepth == 0) {
            return offset;
        }

        if (current == '(') {
            parenDepth++;
        } else if (current == ')' && parenDepth > 0) {
            parenDepth--;
        } else if (current == '[') {
            bracketDepth++;
        } else if (current == ']' && bracketDepth > 0) {
            bracketDepth--;
        } else if (current == '{') {
            braceDepth++;
        } else if (current == '}' && braceDepth > 0) {
            braceDepth--;
        }
    }

    return limit;
}

static size_t inline_value_find_logical_operator(const char *content,
                                                 size_t start,
                                                 size_t end) {
    if (content == NULL) {
        return end;
    }

    for (size_t offset = start; offset + 1 < end; offset++) {
        if ((content[offset] == '|' && content[offset + 1] == '|') ||
            (content[offset] == '&' && content[offset + 1] == '&')) {
            return offset;
        }
    }

    return end;
}

static size_t inline_value_find_arithmetic_operator(const char *content,
                                                    size_t start,
                                                    size_t end) {
    if (content == NULL) {
        return end;
    }

    for (size_t offset = start; offset < end; offset++) {
        if (content[offset] == '+' ||
            content[offset] == '-' ||
            content[offset] == '*' ||
            content[offset] == '/' ||
            content[offset] == '%') {
            return offset;
        }
    }

    return end;
}

static size_t inline_value_find_last_member_operator(const char *content,
                                                     size_t start,
                                                     size_t end) {
    size_t offset;

    if (content == NULL || end <= start) {
        return end;
    }

    offset = end;
    while (offset > start) {
        offset--;
        if (content[offset] == '.' &&
            offset + 1 < end &&
            ZrStdioInlineValue_IsIdentifierStart(content[offset + 1])) {
            return offset + 1;
        }
    }

    return end;
}

size_t ZrStdioInlineValue_FindSemanticQueryOffset(const char *content,
                                                  size_t start,
                                                  size_t end) {
    size_t queryOffset = inline_value_find_logical_operator(content, start, end);
    if (queryOffset < end) {
        return queryOffset;
    }

    queryOffset = inline_value_find_arithmetic_operator(content, start, end);
    if (queryOffset < end) {
        return queryOffset;
    }

    queryOffset = inline_value_find_last_member_operator(content, start, end);
    if (queryOffset < end) {
        return queryOffset;
    }

    return start;
}

static size_t inline_value_skip_object_literal_space(const char *content, size_t offset, size_t limit) {
    while (content != NULL &&
           offset < limit &&
           (content[offset] == ' ' ||
            content[offset] == '\t' ||
            content[offset] == '\n' ||
            content[offset] == '\r')) {
        offset++;
    }
    return offset;
}

static size_t inline_value_skip_quoted_key(const char *content, size_t offset, size_t limit) {
    char quote;
    int escaped = 0;

    if (content == NULL || offset >= limit ||
        (content[offset] != '\'' && content[offset] != '"')) {
        return offset;
    }

    quote = content[offset++];
    while (offset < limit) {
        char ch = content[offset++];
        if (escaped) {
            escaped = 0;
            continue;
        }
        if (ch == '\\') {
            escaped = 1;
            continue;
        }
        if (ch == quote) {
            break;
        }
    }
    return offset;
}

static size_t inline_value_skip_computed_key(const char *content, size_t offset, size_t limit) {
    int depth = 0;
    int inSingleQuote = 0;
    int inDoubleQuote = 0;
    int escaped = 0;

    if (content == NULL || offset >= limit || content[offset] != '[') {
        return offset;
    }

    while (offset < limit) {
        char current = content[offset++];

        if (inSingleQuote || inDoubleQuote) {
            if (escaped) {
                escaped = 0;
                continue;
            }
            if (current == '\\') {
                escaped = 1;
                continue;
            }
            if ((inSingleQuote && current == '\'') ||
                (inDoubleQuote && current == '"')) {
                inSingleQuote = 0;
                inDoubleQuote = 0;
            }
            continue;
        }

        if (current == '\'') {
            inSingleQuote = 1;
            continue;
        }
        if (current == '"') {
            inDoubleQuote = 1;
            continue;
        }
        if (current == '[') {
            depth++;
            continue;
        }
        if (current == ']') {
            depth--;
            if (depth <= 0) {
                break;
            }
        }
    }

    return offset;
}

static int inline_value_is_object_literal_start(const char *content,
                                                size_t lineEnd,
                                                size_t contentLength,
                                                size_t offset) {
    if (content == NULL || offset >= lineEnd || content[offset] != '{') {
        return 0;
    }

    offset = inline_value_skip_object_literal_space(content, offset + 1, contentLength);
    if (offset >= contentLength) {
        return 0;
    }
    if (content[offset] == '}') {
        return 1;
    }

    if (ZrStdioInlineValue_IsIdentifierStart(content[offset])) {
        offset++;
        while (offset < contentLength && ZrStdioInlineValue_IsIdentifierPart(content[offset])) {
            offset++;
        }
    } else if (content[offset] == '\'' || content[offset] == '"') {
        offset = inline_value_skip_quoted_key(content, offset, contentLength);
    } else if (content[offset] == '[') {
        offset = inline_value_skip_computed_key(content, offset, contentLength);
    } else {
        return 0;
    }

    offset = inline_value_skip_object_literal_space(content, offset, contentLength);
    return offset < contentLength && content[offset] == ':';
}

int ZrStdioInlineValue_IsExpressionStatementStart(const char *content,
                                                  size_t lineStart,
                                                  size_t lineEnd,
                                                  size_t contentLength,
                                                  size_t offset) {
    char ch;

    if (content == NULL || offset >= lineEnd) {
        return 0;
    }

    ch = content[offset];
    if ((ch >= '0' && ch <= '9') || ch == '(' || ch == '[' || ch == '!' || ch == '-') {
        return 1;
    }
    if (ch == '{') {
        return inline_value_is_object_literal_start(content, lineEnd, contentLength, offset);
    }

    if (ZrStdioInlineValue_IsKeywordAt(content, lineStart, lineEnd, offset, "true") ||
        ZrStdioInlineValue_IsKeywordAt(content, lineStart, lineEnd, offset, "false")) {
        return 1;
    }

    if (!ZrStdioInlineValue_IsIdentifierStart(ch)) {
        return 0;
    }

    return !ZrStdioInlineValue_IsKeywordAt(content, lineStart, lineEnd, offset, "var") &&
           !ZrStdioInlineValue_IsKeywordAt(content, lineStart, lineEnd, offset, "return") &&
           !ZrStdioInlineValue_IsKeywordAt(content, lineStart, lineEnd, offset, "func") &&
           !ZrStdioInlineValue_IsKeywordAt(content, lineStart, lineEnd, offset, "if") &&
           !ZrStdioInlineValue_IsKeywordAt(content, lineStart, lineEnd, offset, "while") &&
           !ZrStdioInlineValue_IsKeywordAt(content, lineStart, lineEnd, offset, "for") &&
           !ZrStdioInlineValue_IsKeywordAt(content, lineStart, lineEnd, offset, "switch") &&
           !ZrStdioInlineValue_IsKeywordAt(content, lineStart, lineEnd, offset, "class") &&
           !ZrStdioInlineValue_IsKeywordAt(content, lineStart, lineEnd, offset, "struct") &&
           !ZrStdioInlineValue_IsKeywordAt(content, lineStart, lineEnd, offset, "interface") &&
           !ZrStdioInlineValue_IsKeywordAt(content, lineStart, lineEnd, offset, "module") &&
           !ZrStdioInlineValue_IsKeywordAt(content, lineStart, lineEnd, offset, "import");
}
