#include "stdio_inline_value_scan.h"

#include <string.h>

/** inlineValue 只把 ASCII 标识符纳入轻量词法候选，实际语义仍交给分析器。 */
int ZrStdioInlineValue_IsIdentifierStart(char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_';
}

/** 与首字符规则保持一致，避免关键字前后缀被误判为独立 token。 */
int ZrStdioInlineValue_IsIdentifierPart(char ch) {
    return ZrStdioInlineValue_IsIdentifierStart(ch) || (ch >= '0' && ch <= '9');
}

/** 在行范围内识别完整关键字，供 var/return 与表达式语句筛选共用。 */
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

/** 只把含非空白字符的片段交给后续 inlineValue 候选扫描。 */
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

/** 跨过同一行的引号内容，防止形似声明的字符串文本进入候选扫描。 */
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

/**
 * 返回本行第一段代码，同时把块注释状态交给下一行；调用方只调用一次。
 * BUG: 同一行两条 var 声明之间夹块注释时，首段代码被返回后，后段声明
 * 不再送入扫描，覆盖整行的 inlineValue 请求缺少后段变量的查找项。
 * BUG: 同行代码后的闭合块注释若再跟一个未闭合块注释，提前返回还会漏掉
 * 第二个开头，下一行可能把注释正文误判为变量声明。
 */
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

/** 为表达式语句找跨行终点，忽略单/双引号和注释内的分号以维持范围归属。 */
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

/** 优先查询逻辑运算符，使短路事实能投影到表达式的 inlineValue。 */
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

/** 在缺少逻辑运算时选择算术运算符，供数值区间事实查询。 */
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

/** 在表达式尾端查找点号成员，保证查询位置落在成员标识符上。 */
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

/** 将最外层下标访问作为候选，避免数组字面量与嵌套键干扰成员事实。 */
static size_t inline_value_find_last_computed_member_operator(const char *content,
                                                              size_t start,
                                                              size_t end) {
    size_t lastOperator = end;
    int parenDepth = 0;
    int bracketDepth = 0;
    int braceDepth = 0;

    if (content == NULL || end <= start) {
        return end;
    }

    for (size_t offset = start; offset < end; offset++) {
        char current = content[offset];

        if (current == '\'' || current == '"' || current == '`') {
            offset = inline_value_skip_string_literal(content, offset, end);
            if (offset >= end) {
                break;
            }
            offset--;
            continue;
        }

        if (current == '(') {
            parenDepth++;
        } else if (current == ')' && parenDepth > 0) {
            parenDepth--;
        } else if (current == '{') {
            braceDepth++;
        } else if (current == '}' && braceDepth > 0) {
            braceDepth--;
        } else if (current == '[') {
            if (parenDepth == 0 && bracketDepth == 0 && braceDepth == 0 && offset > start) {
                size_t previous = offset;

                while (previous > start &&
                       (content[previous - 1] == ' ' || content[previous - 1] == '\t')) {
                    previous--;
                }
                if (previous > start &&
                    (ZrStdioInlineValue_IsIdentifierPart(content[previous - 1]) ||
                     content[previous - 1] == ']' ||
                     content[previous - 1] == ')' ||
                     content[previous - 1] == '}')) {
                    lastOperator = offset;
                }
            }
            bracketDepth++;
        } else if (current == ']' && bracketDepth > 0) {
            bracketDepth--;
        }
    }

    return lastOperator;
}

/**
 * 按逻辑、算术、成员、首 token 的优先级选择局部语义查询位置。
 * BUG: 逻辑/算术候选直接扫描原始字节；return 字面量后的块注释若含加号，
 * 查询位置会指向注释起始的斜杠，从而丢掉本应查询的字面量事实。
 */
size_t ZrStdioInlineValue_FindSemanticQueryOffset(const char *content,
                                                  size_t start,
                                                  size_t end) {
    size_t memberQueryOffset;
    size_t computedMemberQueryOffset;
    size_t queryOffset = inline_value_find_logical_operator(content, start, end);
    if (queryOffset < end) {
        return queryOffset;
    }

    queryOffset = inline_value_find_arithmetic_operator(content, start, end);
    if (queryOffset < end) {
        return queryOffset;
    }

    memberQueryOffset = inline_value_find_last_member_operator(content, start, end);
    computedMemberQueryOffset = inline_value_find_last_computed_member_operator(content, start, end);
    if (memberQueryOffset < end || computedMemberQueryOffset < end) {
        if (memberQueryOffset >= end) {
            return computedMemberQueryOffset;
        }
        if (computedMemberQueryOffset >= end) {
            return memberQueryOffset;
        }
        return memberQueryOffset > computedMemberQueryOffset
                   ? memberQueryOffset
                   : computedMemberQueryOffset;
    }

    return start;
}

/** 对象字面量探测允许键与冒号跨行，空白跳过必须受文档长度约束。 */
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

/** 探测对象键时忽略引号内的分隔符，避免误认冒号位置。 */
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

/** 探测计算键时越过嵌套下标及引号，使外层冒号决定对象字面量身份。 */
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

/** 区分对象字面量和普通块起始，以免把控制块当表达式查询。 */
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

/** 以轻量词法排除声明/控制语句，再让正式语义查询决定是否有可展示事实。 */
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
           !ZrStdioInlineValue_IsKeywordAt(content, lineStart, lineEnd, offset, "fn") &&
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
