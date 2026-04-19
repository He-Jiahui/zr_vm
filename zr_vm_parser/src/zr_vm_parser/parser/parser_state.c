#include "parser_internal.h"

void expect_token(SZrParserState *ps, EZrToken expected) {
    if (ps->lexer->t.token != expected) {
        const TZrChar *expectedStr = ZrParser_Lexer_TokenToString(ps->lexer, expected);
        const TZrChar *actualStr = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);
        TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];
        snprintf(errorMsg, sizeof(errorMsg), "期望 '%s'，但遇到 '%s'", expectedStr, actualStr);
        report_error_with_token(ps, errorMsg, ps->lexer->t.token);
    }
}

// 消费 token（如果匹配）

TZrBool consume_token(SZrParserState *ps, EZrToken token) {
    if (ps->lexer->t.token == token) {
        ZrParser_Lexer_Next(ps->lexer);
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

// 查看下一个 token（不消费）

EZrToken peek_token(SZrParserState *ps) { return ZrParser_Lexer_Lookahead(ps->lexer); }

TZrBool consume_percent_keyword_token(SZrParserState *ps, EZrToken token) {
    if (ps == ZR_NULL || ps->lexer == ZR_NULL || ps->lexer->t.token != ZR_TK_PERCENT ||
        peek_token(ps) != token) {
        return ZR_FALSE;
    }

    ZrParser_Lexer_Next(ps->lexer);
    ZrParser_Lexer_Next(ps->lexer);
    return ZR_TRUE;
}

void save_parser_cursor(SZrParserState *ps, SZrParserCursor *cursor) {
    if (ps == ZR_NULL || ps->lexer == ZR_NULL || cursor == ZR_NULL) {
        return;
    }

    cursor->currentPos = ps->lexer->currentPos;
    cursor->currentChar = ps->lexer->currentChar;
    cursor->lineNumber = ps->lexer->lineNumber;
    cursor->lastLine = ps->lexer->lastLine;
    cursor->token = ps->lexer->t;
    cursor->lookahead = ps->lexer->lookahead;
    cursor->lookaheadPos = ps->lexer->lookaheadPos;
    cursor->lookaheadChar = ps->lexer->lookaheadChar;
    cursor->lookaheadLine = ps->lexer->lookaheadLine;
    cursor->lookaheadLastLine = ps->lexer->lookaheadLastLine;
    cursor->hasError = ps->hasError;
    cursor->errorMessage = ps->errorMessage;
}

void restore_parser_cursor(SZrParserState *ps, const SZrParserCursor *cursor) {
    if (ps == ZR_NULL || ps->lexer == ZR_NULL || cursor == ZR_NULL) {
        return;
    }

    ps->lexer->currentPos = cursor->currentPos;
    ps->lexer->currentChar = cursor->currentChar;
    ps->lexer->lineNumber = cursor->lineNumber;
    ps->lexer->lastLine = cursor->lastLine;
    ps->lexer->t = cursor->token;
    ps->lexer->lookahead = cursor->lookahead;
    ps->lexer->lookaheadPos = cursor->lookaheadPos;
    ps->lexer->lookaheadChar = cursor->lookaheadChar;
    ps->lexer->lookaheadLine = cursor->lookaheadLine;
    ps->lexer->lookaheadLastLine = cursor->lookaheadLastLine;
    ps->hasError = cursor->hasError;
    ps->errorMessage = cursor->errorMessage;
}

TZrBool current_identifier_equals(SZrParserState *ps, const TZrChar *text) {
    SZrString *identName;
    TZrNativeString nameStr;

    if (ps == ZR_NULL || text == ZR_NULL || ps->lexer->t.token != ZR_TK_IDENTIFIER) {
        return ZR_FALSE;
    }

    identName = ps->lexer->t.seminfo.stringValue;
    if (identName == ZR_NULL) {
        return ZR_FALSE;
    }

    nameStr = ZrCore_String_GetNativeString(identName);
    return nameStr != ZR_NULL && strcmp(nameStr, text) == 0;
}

TZrBool current_percent_directive_equals(SZrParserState *ps, const TZrChar *text) {
    TZrBool result = ZR_FALSE;
    TZrSize savedPos;
    TZrInt32 savedChar;
    TZrInt32 savedLine;
    TZrInt32 savedLastLine;
    SZrToken savedToken;
    SZrToken savedLookahead;
    TZrSize savedLookaheadPos;
    TZrInt32 savedLookaheadChar;
    TZrInt32 savedLookaheadLine;
    TZrInt32 savedLookaheadLastLine;

    if (ps == ZR_NULL || text == ZR_NULL || ps->lexer->t.token != ZR_TK_PERCENT) {
        return ZR_FALSE;
    }

    savedPos = ps->lexer->currentPos;
    savedChar = ps->lexer->currentChar;
    savedLine = ps->lexer->lineNumber;
    savedLastLine = ps->lexer->lastLine;
    savedToken = ps->lexer->t;
    savedLookahead = ps->lexer->lookahead;
    savedLookaheadPos = ps->lexer->lookaheadPos;
    savedLookaheadChar = ps->lexer->lookaheadChar;
    savedLookaheadLine = ps->lexer->lookaheadLine;
    savedLookaheadLastLine = ps->lexer->lookaheadLastLine;

    ZrParser_Lexer_Next(ps->lexer);
    if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
        result = current_identifier_equals(ps, text);
    } else if (ps->lexer->t.token == ZR_TK_MODULE) {
        result = strcmp(text, "module") == 0;
    } else if (ps->lexer->t.token == ZR_TK_TEST) {
        result = strcmp(text, "test") == 0;
    } else if (ps->lexer->t.token == ZR_TK_USING) {
        result = strcmp(text, "using") == 0;
    }

    ps->lexer->currentPos = savedPos;
    ps->lexer->currentChar = savedChar;
    ps->lexer->lineNumber = savedLine;
    ps->lexer->lastLine = savedLastLine;
    ps->lexer->t = savedToken;
    ps->lexer->lookahead = savedLookahead;
    ps->lexer->lookaheadPos = savedLookaheadPos;
    ps->lexer->lookaheadChar = savedLookaheadChar;
    ps->lexer->lookaheadLine = savedLookaheadLine;
    ps->lexer->lookaheadLastLine = savedLookaheadLastLine;
    return result;
}

TZrBool is_module_path_segment_token(EZrToken token) { return token == ZR_TK_IDENTIFIER || token == ZR_TK_TEST; }

TZrBool is_type_modifier_token(EZrToken token) {
    return token == ZR_TK_ABSTRACT || token == ZR_TK_FINAL;
}

TZrBool is_member_modifier_token(EZrToken token) {
    return token == ZR_TK_ABSTRACT || token == ZR_TK_VIRTUAL || token == ZR_TK_OVERRIDE ||
           token == ZR_TK_FINAL || token == ZR_TK_SHADOW;
}

TZrUInt32 token_to_declaration_modifier_flag(EZrToken token) {
    switch (token) {
        case ZR_TK_ABSTRACT:
            return ZR_DECLARATION_MODIFIER_ABSTRACT;
        case ZR_TK_VIRTUAL:
            return ZR_DECLARATION_MODIFIER_VIRTUAL;
        case ZR_TK_OVERRIDE:
            return ZR_DECLARATION_MODIFIER_OVERRIDE;
        case ZR_TK_FINAL:
            return ZR_DECLARATION_MODIFIER_FINAL;
        case ZR_TK_SHADOW:
            return ZR_DECLARATION_MODIFIER_SHADOW;
        default:
            return ZR_DECLARATION_MODIFIER_NONE;
    }
}

TZrUInt32 parse_declaration_modifier_flags(SZrParserState *ps, TZrUInt32 allowedFlags) {
    TZrUInt32 flags = ZR_DECLARATION_MODIFIER_NONE;

    if (ps == ZR_NULL) {
        return ZR_DECLARATION_MODIFIER_NONE;
    }

    while (ps->lexer != ZR_NULL) {
        TZrUInt32 flag = token_to_declaration_modifier_flag(ps->lexer->t.token);
        if (flag == ZR_DECLARATION_MODIFIER_NONE || (allowedFlags & flag) == 0) {
            break;
        }

        if ((flags & flag) != 0) {
            report_error(ps, "Duplicate declaration modifier");
        }

        flags |= flag;
        ZrParser_Lexer_Next(ps->lexer);
    }

    return flags;
}

void skip_balanced_after_open_paren(SZrParserState *ps) {
    TZrInt32 depth = 1;

    while (ps != ZR_NULL && ps->lexer->t.token != ZR_TK_EOS && depth > 0) {
        if (ps->lexer->t.token == ZR_TK_LPAREN) {
            depth++;
        } else if (ps->lexer->t.token == ZR_TK_RPAREN) {
            depth--;
        }
        ZrParser_Lexer_Next(ps->lexer);
    }
}

void skip_to_semicolon_or_eos(SZrParserState *ps) {
    while (ps != ZR_NULL && ps->lexer->t.token != ZR_TK_EOS) {
        if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
            ZrParser_Lexer_Next(ps->lexer);
            break;
        }
        ZrParser_Lexer_Next(ps->lexer);
    }
}

void skip_legacy_import_call(SZrParserState *ps) {
    if (ps == ZR_NULL) {
        return;
    }

    ZrParser_Lexer_Next(ps->lexer);
    if (consume_token(ps, ZR_TK_LPAREN)) {
        skip_balanced_after_open_paren(ps);
    }
}

SZrAstNode *parse_normalized_dotted_module_path(SZrParserState *ps, const TZrChar *directiveName) {
    TZrChar buffer[ZR_PARSER_DECLARATION_BUFFER_LENGTH];
    TZrSize length = 0;
    SZrFileRange startLoc;
    SZrString *pathString;
    TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    startLoc = get_current_location(ps);
    if (!is_module_path_segment_token(ps->lexer->t.token)) {
        snprintf(errorMsg, sizeof(errorMsg), "Expected module path after %%%s", directiveName);
        report_error(ps, errorMsg);
        return ZR_NULL;
    }

    while (is_module_path_segment_token(ps->lexer->t.token)) {
        SZrString *segment = ps->lexer->t.seminfo.stringValue;
        TZrNativeString nativeSegment = segment != ZR_NULL ? ZrCore_String_GetNativeString(segment) : ZR_NULL;
        TZrSize segmentLength = nativeSegment != ZR_NULL ? ZrCore_NativeString_Length(nativeSegment) : 0;

        if (nativeSegment == ZR_NULL) {
            snprintf(errorMsg, sizeof(errorMsg), "Invalid module path segment in %%%s", directiveName);
            report_error(ps, errorMsg);
            return ZR_NULL;
        }

        if (length > 0) {
            if (length + 1 >= sizeof(buffer)) {
                report_error(ps, "Module path is too long");
                return ZR_NULL;
            }
            buffer[length++] = '.';
        }

        if (length + segmentLength >= sizeof(buffer)) {
            report_error(ps, "Module path is too long");
            return ZR_NULL;
        }

        memcpy(buffer + length, nativeSegment, segmentLength);
        length += segmentLength;
        ZrParser_Lexer_Next(ps->lexer);

        if (!consume_token(ps, ZR_TK_DOT)) {
            break;
        }
        if (!is_module_path_segment_token(ps->lexer->t.token)) {
            snprintf(errorMsg, sizeof(errorMsg), "Expected identifier after '.' in %%%s path", directiveName);
            report_error(ps, errorMsg);
            return ZR_NULL;
        }
    }

    buffer[length] = '\0';
    pathString = ZrCore_String_Create(ps->state, buffer, length);
    if (pathString == ZR_NULL) {
        report_error(ps, "Failed to allocate module path string");
        return ZR_NULL;
    }

    return create_string_literal_node_with_location(ps, pathString, ZR_FALSE, pathString,
                                                    ZrParser_FileRange_Merge(startLoc, get_current_location(ps)));
}

SZrAstNode *parse_normalized_module_path(SZrParserState *ps, const TZrChar *directiveName) {
    TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token == ZR_TK_LPAREN) {
        ZrParser_Lexer_Next(ps->lexer);
        if (ps->lexer->t.token != ZR_TK_STRING) {
            snprintf(errorMsg, sizeof(errorMsg), "%%%s(...) only accepts a string literal module path", directiveName);
            report_error(ps, errorMsg);
            skip_balanced_after_open_paren(ps);
            return ZR_NULL;
        }

        {
            SZrAstNode *stringNode = parse_literal(ps);
            expect_token(ps, ZR_TK_RPAREN);
            consume_token(ps, ZR_TK_RPAREN);
            return stringNode;
        }
    }

    if (ps->lexer->t.token == ZR_TK_STRING) {
        return parse_literal(ps);
    }

    return parse_normalized_dotted_module_path(ps, directiveName);
}

SZrAstNodeArray *parse_leading_decorators(SZrParserState *ps) {
    SZrAstNodeArray *decorators;

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    decorators = ZrParser_AstNodeArray_New(ps->state, 2);
    if (decorators == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_SHARP) {
        SZrAstNode *decorator = parse_decorator_expression(ps);
        if (decorator == ZR_NULL) {
            break;
        }
        ZrParser_AstNodeArray_Add(ps->state, decorators, decorator);
    }

    return decorators;
}

TZrBool consume_type_closing_angle(SZrParserState *ps) {
    if (ps->lexer->t.token == ZR_TK_GREATER_THAN) {
        ZrParser_Lexer_Next(ps->lexer);
        return ZR_TRUE;
    }

    if (ps->lexer->t.token == ZR_TK_RIGHT_SHIFT) {
        if (ps->lexer->currentPos > 0) {
            ps->lexer->currentPos--;
        }
        ps->lexer->currentChar = '>';
        ps->lexer->lookahead.token = ZR_TK_EOS;
        ZrParser_Lexer_Next(ps->lexer);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

// 获取当前位置信息

SZrFileRange get_current_location(SZrParserState *ps) {
    // 计算列号（从当前行开始到当前位置的字符数）
    TZrInt32 column = 1;
    if (ps->lexer->source != ZR_NULL && ps->lexer->currentPos > 0) {
        TZrSize pos = ps->lexer->currentPos - 1;
        while (pos > 0 && ps->lexer->source[pos] != '\n' && ps->lexer->source[pos] != '\r') {
            pos--;
            column++;
        }
    }

    SZrFilePosition start = ZrParser_FilePosition_Create(
            ps->lexer->currentPos, ps->lexer->lastLine > 0 ? ps->lexer->lastLine : ps->lexer->lineNumber, column);
    SZrFilePosition end = ZrParser_FilePosition_Create(ps->lexer->currentPos, ps->lexer->lineNumber, column);
    return ZrParser_FileRange_Create(start, end, ps->lexer->sourceName);
}

void get_string_view_for_length(SZrString *value, const TZrChar **text, TZrSize *length) {
    if (text == ZR_NULL || length == ZR_NULL) {
        return;
    }

    *text = ZR_NULL;
    *length = 0;
    if (value == ZR_NULL) {
        return;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        *text = ZrCore_String_GetNativeStringShort(value);
        *length = value->shortStringLength;
    } else {
        *text = ZrCore_String_GetNativeString(value);
        *length = value->longStringLength;
    }
}

SZrFilePosition get_file_position_from_offset(SZrLexState *lexer, TZrSize offset) {
    TZrInt32 line = 1;
    TZrSize lineStart = 0;
    TZrSize index = 0;

    if (lexer == ZR_NULL || lexer->source == ZR_NULL) {
        return ZrParser_FilePosition_Create(offset, 1, 1);
    }

    if (offset > lexer->sourceLength) {
        offset = lexer->sourceLength;
    }

    if (lexer->filePositionCacheOffset <= offset) {
        index = lexer->filePositionCacheOffset;
        line = lexer->filePositionCacheLine > 0 ? lexer->filePositionCacheLine : 1;
        lineStart = lexer->filePositionCacheLineStart;
    }

    for (; index < offset; index++) {
        if (lexer->source[index] == '\n') {
            line++;
            lineStart = index + 1;
        }
    }

    lexer->filePositionCacheOffset = offset;
    lexer->filePositionCacheLineStart = lineStart;
    lexer->filePositionCacheLine = line;

    return ZrParser_FilePosition_Create(offset, line, (TZrInt32) (offset - lineStart + 1));
}

static ZR_FORCE_INLINE SZrFilePosition file_position_create_from_cached_line(TZrSize offset,
                                                                             TZrInt32 line,
                                                                             TZrSize lineStart) {
    TZrInt32 resolvedLine = line > 0 ? line : 1;
    TZrSize resolvedLineStart = lineStart <= offset ? lineStart : offset;
    return ZrParser_FilePosition_Create(offset,
                                        resolvedLine,
                                        (TZrInt32)(offset - resolvedLineStart + 1));
}

static SZrFilePosition file_position_advance_over_span(SZrLexState *lexer,
                                                       TZrSize startOffset,
                                                       TZrSize endOffset,
                                                       TZrInt32 startLine,
                                                       TZrSize startLineStart) {
    TZrInt32 line = startLine > 0 ? startLine : 1;
    TZrSize lineStart = startLineStart <= startOffset ? startLineStart : startOffset;

    if (lexer == ZR_NULL || lexer->source == ZR_NULL) {
        return file_position_create_from_cached_line(endOffset, line, lineStart);
    }

    if (startOffset > lexer->sourceLength) {
        startOffset = lexer->sourceLength;
    }
    if (endOffset > lexer->sourceLength) {
        endOffset = lexer->sourceLength;
    }

    for (TZrSize index = startOffset; index < endOffset; index++) {
        if (lexer->source[index] == '\n') {
            line++;
            lineStart = index + 1;
        }
    }

    return file_position_create_from_cached_line(endOffset, line, lineStart);
}

TZrSize get_current_token_length(SZrParserState *ps) {
    const TZrChar *text = ZR_NULL;
    TZrSize length = 0;
    const TZrChar *tokenText;

    if (ps == ZR_NULL || ps->lexer == ZR_NULL) {
        return 0;
    }

    switch (ps->lexer->t.token) {
        case ZR_TK_IDENTIFIER:
            get_string_view_for_length(ps->lexer->t.seminfo.stringValue, &text, &length);
            return length;

        case ZR_TK_INTEGER:
        case ZR_TK_FLOAT:
            get_string_view_for_length(ps->lexer->t.seminfo.stringValue, &text, &length);
            return length;

        case ZR_TK_BOOLEAN:
            return ps->lexer->t.seminfo.booleanValue ? 4 : 5;

        case ZR_TK_EOS:
            return 0;

        default:
            tokenText = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);
            return tokenText != ZR_NULL ? strlen(tokenText) : 0;
    }
}

SZrFileRange get_current_token_location(SZrParserState *ps) {
    TZrSize endOffset;
    TZrSize startOffset;
    SZrFilePosition start;
    SZrFilePosition end;

    if (ps == ZR_NULL || ps->lexer == ZR_NULL) {
        SZrFilePosition zero = ZrParser_FilePosition_Create(0, 1, 1);
        return ZrParser_FileRange_Create(zero, zero, ZR_NULL);
    }

    endOffset = ps->lexer->currentChar == ZR_PARSER_LEXER_EOZ ? ps->lexer->currentPos
                                                              : (ps->lexer->currentPos > 0
                                                                         ? ps->lexer->currentPos - 1
                                                                         : 0);
    if (endOffset > ps->lexer->sourceLength) {
        endOffset = ps->lexer->sourceLength;
    }
    startOffset = ps->lexer->tokenStartOffset;
    if (startOffset > endOffset) {
        startOffset = endOffset;
    }

    start = file_position_create_from_cached_line(startOffset,
                                                  ps->lexer->tokenStartLine,
                                                  ps->lexer->tokenStartLineStart);
    end = file_position_advance_over_span(ps->lexer,
                                          startOffset,
                                          endOffset,
                                          ps->lexer->tokenStartLine,
                                          ps->lexer->tokenStartLineStart);
    return ZrParser_FileRange_Create(start, end, ps->lexer->sourceName);
}

// 获取当前行的代码片段（前后各20个字符）

void get_line_snippet(SZrParserState *ps, TZrChar *buffer, TZrSize bufferSize, TZrInt32 *errorColumn) {
    if (ps->lexer == ZR_NULL || ps->lexer->source == ZR_NULL || bufferSize == 0) {
        buffer[0] = '\0';
        *errorColumn = 1;
        return;
    }

    // 计算列号并找到行首
    TZrSize pos = ps->lexer->currentPos;
    TZrInt32 column = 1;
    TZrSize lineStart = pos;

    // 向前查找行首
    while (lineStart > 0 && ps->lexer->source[lineStart - 1] != '\n') {
        lineStart--;
        column++;
    }

    // 向后查找行尾
    TZrSize lineEnd = pos;
    while (lineEnd < ps->lexer->sourceLength && ps->lexer->source[lineEnd] != '\n') {
        lineEnd++;
    }

    // 计算要显示的起始和结束位置（前后各固定上下文字符）
    TZrSize snippetStart = lineStart;
    TZrSize snippetEnd = lineEnd;
    TZrInt32 displayColumn = column;

    // 如果列号大于上下文半径，向前移动起始位置
    if (column > (TZrInt32)ZR_PARSER_ERROR_SNIPPET_CONTEXT_RADIUS) {
        snippetStart = pos - ZR_PARSER_ERROR_SNIPPET_CONTEXT_RADIUS;
        displayColumn = (TZrInt32)ZR_PARSER_ERROR_SNIPPET_FOCUS_COLUMN;
    }

    // 如果剩余字符少于上下文半径，向后扩展
    if (lineEnd - pos < ZR_PARSER_ERROR_SNIPPET_CONTEXT_RADIUS) {
        TZrSize needed = ZR_PARSER_ERROR_SNIPPET_CONTEXT_RADIUS - (lineEnd - pos);
        if (snippetStart >= needed) {
            snippetStart -= needed;
            displayColumn += (TZrInt32)needed;
        } else {
            snippetStart = 0;
            displayColumn = column;
        }
    }

    // 确保不越界
    if (snippetStart > lineStart) {
        snippetStart = lineStart;
        displayColumn = column;
    }
    if (snippetEnd > ps->lexer->sourceLength) {
        snippetEnd = ps->lexer->sourceLength;
    }

    // 复制代码片段
    TZrSize snippetLen = snippetEnd - snippetStart;
    if (snippetLen >= bufferSize) {
        snippetLen = bufferSize - 1;
    }

    for (TZrSize i = 0; i < snippetLen; i++) {
        TZrChar c = ps->lexer->source[snippetStart + i];
        // 将制表符和换行符替换为空格以便显示
        if (c == '\t') {
            buffer[i] = ' ';
        } else if (c == '\n' || c == '\r') {
            buffer[i] = ' ';
        } else {
            buffer[i] = c;
        }
    }
    buffer[snippetLen] = '\0';

    *errorColumn = displayColumn;
}

// 报告解析错误（带 token 信息）

void report_error_with_token(SZrParserState *ps, const TZrChar *msg, EZrToken token) {
    SZrFileRange errorRange;

    ps->hasError = ZR_TRUE;
    ps->errorMessage = msg;
    errorRange = get_current_token_location(ps);

    if (ps->errorCallback != ZR_NULL) {
        ps->errorCallback(ps->errorUserData, &errorRange, msg, token);
    }

    if (!ps->suppressErrorOutput && ps->lexer != ZR_NULL) {
        // 获取 token 字符串
        const TZrChar *tokenStr = ZrParser_Lexer_TokenToString(ps->lexer, token);

        // 获取文件名
        const TZrChar *fileName = "<unknown>";
        if (ps->lexer->sourceName != ZR_NULL) {
            TZrNativeString nameStr = ZrCore_String_GetNativeString(ps->lexer->sourceName);
            if (nameStr != ZR_NULL) {
                fileName = nameStr;
            }
        }

        // 计算列号（从当前行开始到当前位置的字符数）
        TZrInt32 column = 1;
        if (ps->lexer->source != ZR_NULL && ps->lexer->currentPos > 0) {
            TZrSize pos = ps->lexer->currentPos - 1;
            while (pos > 0 && ps->lexer->source[pos] != '\n') {
                pos--;
                column++;
            }
        }

        // 获取代码片段
        TZrChar snippet[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
        TZrInt32 displayColumn = 1;
        get_line_snippet(ps, snippet, sizeof(snippet), &displayColumn);

        // 输出详细的错误信息
        // 使用 lastLine 而不是 lineNumber，因为 lastLine 是当前 token 的行号
        TZrInt32 errorLine = ps->lexer->lastLine > 0 ? ps->lexer->lastLine : ps->lexer->lineNumber;
        if (snippet[0] != '\0') {
            ZrCore_Log_Diagnosticf(ps->state,
                                   ZR_LOG_LEVEL_ERROR,
                                   ZR_OUTPUT_CHANNEL_STDERR,
                                   "  [%s:%d:%d] %s (遇到 token: '%s')\n"
                                   "    %s\n"
                                   "%*s^\n",
                                   fileName,
                                   errorLine,
                                   column,
                                   msg,
                                   tokenStr,
                                   snippet,
                                   displayColumn > 0 ? displayColumn - 1 : 0,
                                   "");
        } else {
            ZrCore_Log_Diagnosticf(ps->state,
                                   ZR_LOG_LEVEL_ERROR,
                                   ZR_OUTPUT_CHANNEL_STDERR,
                                   "  [%s:%d:%d] %s (遇到 token: '%s')\n",
                                   fileName,
                                   errorLine,
                                   column,
                                   msg,
                                   tokenStr);
        }
    }
}

// 报告解析错误

void report_error(SZrParserState *ps, const TZrChar *msg) {
    ps->hasError = ZR_TRUE;
    ps->errorMessage = msg;

    // 使用词法分析器的错误报告函数
    if (ps->lexer != ZR_NULL) {
        EZrToken currentToken = ps->lexer->t.token;
        report_error_with_token(ps, msg, currentToken);
    }
}

// ==================== AST 节点创建辅助函数 ====================
