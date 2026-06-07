#include "repl/repl_input_scan.h"

#include <string.h>

TZrBool ZrCli_ReplInput_IsSpace(TZrChar ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

static TZrBool repl_input_is_identifier_start(TZrChar ch) {
    return (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') ||
           ch == '_';
}

static TZrBool repl_input_is_identifier_part(TZrChar ch) {
    return repl_input_is_identifier_start(ch) ||
           (ch >= '0' && ch <= '9');
}

const TZrChar *ZrCli_ReplInput_SkipSpace(const TZrChar *code) {
    while (code != ZR_NULL && ZrCli_ReplInput_IsSpace(*code)) {
        ++code;
    }
    return code;
}

TZrBool ZrCli_ReplInput_StartsWithKeyword(const TZrChar *code, const TZrChar *keyword) {
    TZrSize keywordLength;

    if (code == ZR_NULL || keyword == ZR_NULL) {
        return ZR_FALSE;
    }

    keywordLength = strlen(keyword);
    return strncmp(code, keyword, keywordLength) == 0 &&
           !repl_input_is_identifier_part(code[keywordLength]);
}

static TZrBool repl_input_starts_with_statement_keyword(const TZrChar *code) {
    static const TZrChar *keywords[] = {
        "break",
        "case",
        "catch",
        "class",
        "const",
        "continue",
        "default",
        "else",
        "enum",
        "extern",
        "finally",
        "for",
        "foreach",
        "func",
        "if",
        "interface",
        "pri",
        "pro",
        "pub",
        "return",
        "struct",
        "switch",
        "throw",
        "try",
        "using",
        "var",
        "while"
    };
    TZrSize i;

    for (i = 0; i < sizeof(keywords) / sizeof(keywords[0]); ++i) {
        if (ZrCli_ReplInput_StartsWithKeyword(code, keywords[i])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool repl_input_has_statement_semicolon(const TZrChar *code) {
    TZrBool inSingleQuote = ZR_FALSE;
    TZrBool inDoubleQuote = ZR_FALSE;
    TZrBool inLineComment = ZR_FALSE;
    TZrBool inBlockComment = ZR_FALSE;
    TZrBool escaped = ZR_FALSE;
    TZrChar previous = '\0';

    if (code == ZR_NULL) {
        return ZR_FALSE;
    }

    for (; *code != '\0'; ++code) {
        TZrChar current = *code;
        TZrChar next = code[1];

        if (inLineComment) {
            if (current == '\n' || current == '\r') {
                inLineComment = ZR_FALSE;
            }
            continue;
        }

        if (inBlockComment) {
            if (previous == '*' && current == '/') {
                inBlockComment = ZR_FALSE;
                previous = '\0';
                continue;
            }
            previous = current;
            continue;
        }

        if (inSingleQuote || inDoubleQuote) {
            if (escaped) {
                escaped = ZR_FALSE;
                continue;
            }
            if (current == '\\') {
                escaped = ZR_TRUE;
                continue;
            }
            if ((inSingleQuote && current == '\'') || (inDoubleQuote && current == '"')) {
                inSingleQuote = ZR_FALSE;
                inDoubleQuote = ZR_FALSE;
            }
            continue;
        }

        if (current == '/' && next == '/') {
            inLineComment = ZR_TRUE;
            ++code;
            continue;
        }
        if (current == '/' && next == '*') {
            inBlockComment = ZR_TRUE;
            previous = '\0';
            ++code;
            continue;
        }
        if (current == '\'') {
            inSingleQuote = ZR_TRUE;
            continue;
        }
        if (current == '"') {
            inDoubleQuote = ZR_TRUE;
            continue;
        }
        if (current == ';') {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool repl_input_ends_with_incomplete_expression_marker(const TZrChar *code) {
    const TZrChar *begin;
    const TZrChar *end;
    TZrChar last;

    begin = ZrCli_ReplInput_SkipSpace(code);
    if (begin == ZR_NULL || *begin == '\0') {
        return ZR_FALSE;
    }

    end = begin + strlen(begin);
    while (end > begin && ZrCli_ReplInput_IsSpace(end[-1])) {
        --end;
    }
    if (end == begin) {
        return ZR_FALSE;
    }

    last = end[-1];
    return last == '+' || last == '-' || last == '*' || last == '/' ||
           last == '%' || last == '&' || last == '|' || last == '^' ||
           last == '!' || last == '=' || last == '<' || last == '>' ||
           last == '?' || last == ':' || last == ',' || last == '.' ||
           last == '(' || last == '[' || last == '{' || last == '~';
}

static const TZrChar *repl_input_skip_quoted(const TZrChar *code, TZrChar quote) {
    TZrBool escaped = ZR_FALSE;

    if (code == ZR_NULL || *code != quote) {
        return ZR_NULL;
    }

    for (++code; *code != '\0'; ++code) {
        if (escaped) {
            escaped = ZR_FALSE;
            continue;
        }
        if (*code == '\\') {
            escaped = ZR_TRUE;
            continue;
        }
        if (*code == quote) {
            return code + 1;
        }
    }

    return ZR_NULL;
}

static const TZrChar *repl_input_skip_computed_object_key(const TZrChar *code) {
    TZrSize depth = 0;

    if (code == ZR_NULL || *code != '[') {
        return ZR_NULL;
    }

    for (; *code != '\0'; ++code) {
        if (*code == '\'' || *code == '"') {
            code = repl_input_skip_quoted(code, *code);
            if (code == ZR_NULL) {
                return ZR_NULL;
            }
            --code;
            continue;
        }
        if (*code == '[') {
            ++depth;
            continue;
        }
        if (*code == ']') {
            if (depth == 0) {
                return ZR_NULL;
            }
            --depth;
            if (depth == 0) {
                return code + 1;
            }
        }
    }

    return ZR_NULL;
}

static TZrBool repl_input_starts_with_object_literal_expression(const TZrChar *code) {
    const TZrChar *cursor;

    cursor = ZrCli_ReplInput_SkipSpace(code);
    if (cursor == ZR_NULL || *cursor != '{') {
        return ZR_FALSE;
    }

    cursor = ZrCli_ReplInput_SkipSpace(cursor + 1);
    if (cursor == ZR_NULL) {
        return ZR_FALSE;
    }
    if (*cursor == '}') {
        return ZR_TRUE;
    }

    if (*cursor == '\'' || *cursor == '"') {
        cursor = repl_input_skip_quoted(cursor, *cursor);
        cursor = ZrCli_ReplInput_SkipSpace(cursor);
        return (TZrBool)(cursor != ZR_NULL && *cursor == ':');
    }

    if (*cursor == '[') {
        cursor = repl_input_skip_computed_object_key(cursor);
        cursor = ZrCli_ReplInput_SkipSpace(cursor);
        return (TZrBool)(cursor != ZR_NULL && *cursor == ':');
    }

    if (repl_input_is_identifier_start(*cursor)) {
        if (repl_input_starts_with_statement_keyword(cursor)) {
            return ZR_FALSE;
        }
        while (repl_input_is_identifier_part(*cursor)) {
            ++cursor;
        }
        cursor = ZrCli_ReplInput_SkipSpace(cursor);
        return (TZrBool)(cursor != ZR_NULL && *cursor == ':');
    }

    return ZR_FALSE;
}

static const TZrChar *repl_input_skip_comment(const TZrChar *code) {
    if (code == ZR_NULL || code[0] != '/') {
        return code;
    }

    if (code[1] == '/') {
        code += 2;
        while (*code != '\0' && *code != '\n' && *code != '\r') {
            ++code;
        }
        return code;
    }

    if (code[1] == '*') {
        code += 2;
        while (*code != '\0') {
            if (code[0] == '*' && code[1] == '/') {
                return code + 2;
            }
            ++code;
        }
        return ZR_NULL;
    }

    return code;
}

static const TZrChar *repl_input_skip_space_and_comments(const TZrChar *code) {
    const TZrChar *next;

    for (;;) {
        code = ZrCli_ReplInput_SkipSpace(code);
        next = repl_input_skip_comment(code);
        if (next == code) {
            return code;
        }
        if (next == ZR_NULL) {
            return ZR_NULL;
        }
        code = next;
    }
}

static TZrBool repl_input_has_single_top_level_statement_terminator(const TZrChar *code) {
    TZrBool inSingleQuote = ZR_FALSE;
    TZrBool inDoubleQuote = ZR_FALSE;
    TZrBool inLineComment = ZR_FALSE;
    TZrBool inBlockComment = ZR_FALSE;
    TZrBool escaped = ZR_FALSE;
    TZrSize parenDepth = 0;
    TZrSize bracketDepth = 0;
    TZrSize braceDepth = 0;

    if (code == ZR_NULL) {
        return ZR_FALSE;
    }

    for (; *code != '\0'; ++code) {
        TZrChar current = *code;
        TZrChar next = code[1];

        if (inLineComment) {
            if (current == '\n' || current == '\r') {
                inLineComment = ZR_FALSE;
            }
            continue;
        }

        if (inBlockComment) {
            if (current == '*' && next == '/') {
                inBlockComment = ZR_FALSE;
                ++code;
            }
            continue;
        }

        if (inSingleQuote || inDoubleQuote) {
            if (escaped) {
                escaped = ZR_FALSE;
                continue;
            }
            if (current == '\\') {
                escaped = ZR_TRUE;
                continue;
            }
            if ((inSingleQuote && current == '\'') || (inDoubleQuote && current == '"')) {
                inSingleQuote = ZR_FALSE;
                inDoubleQuote = ZR_FALSE;
            }
            continue;
        }

        if (current == '/' && next == '/') {
            inLineComment = ZR_TRUE;
            ++code;
            continue;
        }
        if (current == '/' && next == '*') {
            inBlockComment = ZR_TRUE;
            ++code;
            continue;
        }
        if (current == '\'') {
            inSingleQuote = ZR_TRUE;
            continue;
        }
        if (current == '"') {
            inDoubleQuote = ZR_TRUE;
            continue;
        }
        if (current == '(') {
            ++parenDepth;
            continue;
        }
        if (current == ')' && parenDepth > 0) {
            --parenDepth;
            continue;
        }
        if (current == '[') {
            ++bracketDepth;
            continue;
        }
        if (current == ']' && bracketDepth > 0) {
            --bracketDepth;
            continue;
        }
        if (current == '{') {
            ++braceDepth;
            continue;
        }
        if (current == '}' && braceDepth > 0) {
            --braceDepth;
            continue;
        }
        if (current == ';' && parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
            const TZrChar *tail = repl_input_skip_space_and_comments(code + 1);
            return (TZrBool)(tail != ZR_NULL && *tail == '\0');
        }
    }

    return ZR_FALSE;
}

static const TZrChar *repl_input_skip_assignment_target(const TZrChar *code) {
    const TZrChar *cursor;

    cursor = ZrCli_ReplInput_SkipSpace(code);
    if (cursor == ZR_NULL ||
        !repl_input_is_identifier_start(*cursor) ||
        repl_input_starts_with_statement_keyword(cursor)) {
        return ZR_NULL;
    }

    while (repl_input_is_identifier_part(*cursor)) {
        ++cursor;
    }

    for (;;) {
        cursor = ZrCli_ReplInput_SkipSpace(cursor);
        if (cursor == ZR_NULL) {
            return ZR_NULL;
        }

        if (*cursor == '.') {
            cursor = ZrCli_ReplInput_SkipSpace(cursor + 1);
            if (cursor == ZR_NULL || !repl_input_is_identifier_start(*cursor)) {
                return ZR_NULL;
            }
            while (repl_input_is_identifier_part(*cursor)) {
                ++cursor;
            }
            continue;
        }

        if (*cursor == '[') {
            cursor = repl_input_skip_computed_object_key(cursor);
            if (cursor == ZR_NULL) {
                return ZR_NULL;
            }
            continue;
        }

        break;
    }

    return cursor;
}

TZrBool ZrCli_ReplInput_IsSimpleAssignmentStatement(const TZrChar *code) {
    const TZrChar *cursor;

    cursor = repl_input_skip_assignment_target(code);
    cursor = ZrCli_ReplInput_SkipSpace(cursor);
    if (cursor == ZR_NULL ||
        cursor[0] != '=' ||
        cursor[1] == '=' ||
        cursor[1] == '>' ||
        cursor[1] == '\0') {
        return ZR_FALSE;
    }

    cursor = ZrCli_ReplInput_SkipSpace(cursor + 1);
    if (cursor == ZR_NULL || *cursor == '\0' || *cursor == ';') {
        return ZR_FALSE;
    }

    return repl_input_has_single_top_level_statement_terminator(cursor);
}

TZrBool ZrCli_ReplInput_ShouldWrapExpression(const TZrChar *code) {
    const TZrChar *trimmed = ZrCli_ReplInput_SkipSpace(code);

    if (trimmed == ZR_NULL || *trimmed == '\0') {
        return ZR_FALSE;
    }

    if (*trimmed == '{' && !repl_input_starts_with_object_literal_expression(trimmed)) {
        return ZR_FALSE;
    }

    if (*trimmed == ':' || *trimmed == '@' || *trimmed == '}' ||
        *trimmed == ']' || *trimmed == '/' || *trimmed == '#') {
        return ZR_FALSE;
    }

    if (repl_input_has_statement_semicolon(trimmed)) {
        return ZR_FALSE;
    }

    if (repl_input_ends_with_incomplete_expression_marker(trimmed)) {
        return ZR_FALSE;
    }

    if (repl_input_starts_with_statement_keyword(trimmed)) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}
