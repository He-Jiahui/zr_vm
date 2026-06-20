#include "zr_vm_language_server_stdio_internal.h"

#include "stdio_inline_value_scan.h"
#include "stdio_inline_value_semantic_text.h"

#include <string.h>

static int inline_value_position_in_range(SZrLspRange range, TZrInt32 line, TZrInt32 character) {
    if (line < range.start.line || line > range.end.line) {
        return 0;
    }
    if (line == range.start.line && character < range.start.character) {
        return 0;
    }
    if (line == range.end.line && character > range.end.character) {
        return 0;
    }
    return 1;
}

static cJSON *inline_value_create_variable_lookup(TZrInt32 line,
                                                  TZrInt32 startCharacter,
                                                  const char *nameStart,
                                                  size_t nameLength) {
    cJSON *json;
    SZrLspRange range;
    char *name;

    if (nameStart == NULL || nameLength == 0) {
        return NULL;
    }

    json = cJSON_CreateObject();
    name = duplicate_string_range(nameStart, nameLength);
    if (json == NULL || name == NULL) {
        cJSON_Delete(json);
        free(name);
        return NULL;
    }

    range.start.line = line;
    range.start.character = startCharacter;
    range.end.line = line;
    range.end.character = startCharacter + (TZrInt32)nameLength;
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(range));
    cJSON_AddStringToObject(json, ZR_LSP_FIELD_VARIABLE_NAME, name);
    cJSON_AddBoolToObject(json, ZR_LSP_FIELD_CASE_SENSITIVE_LOOKUP, 1);

    free(name);
    return json;
}

static size_t inline_value_find_multiline_statement_end(const char *content,
                                                        size_t start,
                                                        size_t limit) {
    size_t end = start;

    if (content == NULL) {
        return start;
    }

    while (end < limit && content[end] != ';' && content[end] != '}') {
        end++;
    }

    return end;
}

static size_t inline_value_skip_multiline_whitespace(const char *content,
                                                     size_t offset,
                                                     size_t limit) {
    if (content == NULL) {
        return offset;
    }

    while (offset < limit &&
           (content[offset] == ' ' ||
            content[offset] == '\t' ||
            content[offset] == '\n' ||
            content[offset] == '\r')) {
        offset++;
    }

    return offset;
}

static size_t inline_value_trim_expression_end(const char *content,
                                               size_t start,
                                               size_t end) {
    if (content == NULL) {
        return end;
    }

    while (end > start &&
           (content[end - 1] == ' ' ||
            content[end - 1] == '\t' ||
            content[end - 1] == '\n' ||
            content[end - 1] == '\r')) {
        end--;
    }

    return end;
}

static SZrLspPosition inline_value_position_from_offset(const char *content,
                                                        size_t lineStart,
                                                        TZrInt32 line,
                                                        size_t offset) {
    SZrLspPosition position;
    size_t currentLineStart = lineStart;
    TZrInt32 currentLine = line;

    position.line = line;
    position.character = 0;
    if (content == NULL || offset < lineStart) {
        return position;
    }

    for (size_t scan = lineStart; scan < offset; scan++) {
        if (content[scan] == '\n') {
            currentLine++;
            currentLineStart = scan + 1;
        }
    }

    position.line = currentLine;
    position.character = (TZrInt32)(offset - currentLineStart);
    return position;
}

static cJSON *inline_value_create_semantic_text_for_offsets(SZrStdioServer *server,
                                                            SZrString *uri,
                                                            const char *content,
                                                            size_t lineStart,
                                                            TZrInt32 line,
                                                            size_t rangeStartOffset,
                                                            size_t rangeEndOffset,
                                                            size_t queryOffset) {
    SZrLspRange range;
    SZrLspPosition queryPosition;

    if (rangeEndOffset <= rangeStartOffset) {
        return NULL;
    }

    range.start = inline_value_position_from_offset(content, lineStart, line, rangeStartOffset);
    range.end = inline_value_position_from_offset(content, lineStart, line, rangeEndOffset);
    queryPosition = inline_value_position_from_offset(content, lineStart, line, queryOffset);
    return ZrStdioInlineValue_CreateSemanticTextForLspRange(server, uri, range, queryPosition);
}

static int inline_value_previous_token_is_keyword(const char *content,
                                                  size_t lineStart,
                                                  const char *keyword) {
    size_t keywordLength;
    size_t offset;
    size_t tokenStart;
    size_t tokenEnd;

    if (content == NULL || keyword == NULL || lineStart == 0) {
        return 0;
    }

    offset = lineStart;
    while (offset > 0) {
        char ch = content[offset - 1];
        if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
            break;
        }
        offset--;
    }
    if (offset == 0 || !ZrStdioInlineValue_IsIdentifierPart(content[offset - 1])) {
        return 0;
    }

    tokenEnd = offset;
    tokenStart = tokenEnd;
    while (tokenStart > 0 && ZrStdioInlineValue_IsIdentifierPart(content[tokenStart - 1])) {
        tokenStart--;
    }

    keywordLength = strlen(keyword);
    return tokenEnd - tokenStart == keywordLength &&
           strncmp(content + tokenStart, keyword, keywordLength) == 0;
}

static int inline_value_line_is_continuation(const char *content, size_t lineStart) {
    size_t offset;

    if (content == NULL || lineStart == 0) {
        return 0;
    }

    offset = lineStart;
    while (offset > 0) {
        char ch = content[offset - 1];
        offset--;
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            continue;
        }

        if (ch == '+' ||
            ch == '-' ||
            ch == '*' ||
            ch == '/' ||
            ch == '%' ||
            ch == '&' ||
            ch == '|' ||
            ch == '?' ||
            ch == ':' ||
            ch == ',' ||
            ch == '=' ||
            ch == '(') {
            return 1;
        }
        return inline_value_previous_token_is_keyword(content, lineStart, "return");
    }

    return 0;
}

static int inline_value_has_only_whitespace_before(const char *content,
                                                   size_t start,
                                                   size_t end) {
    size_t offset;

    if (content == NULL || end < start) {
        return 0;
    }

    for (offset = start; offset < end; offset++) {
        if (content[offset] != ' ' && content[offset] != '\t') {
            return 0;
        }
    }

    return 1;
}

static int inline_value_find_continuation_expression_owner(const char *content,
                                                          size_t lineStart,
                                                          TZrInt32 line,
                                                          size_t *ownerLineStart,
                                                          size_t *ownerLineEnd,
                                                          TZrInt32 *ownerLine) {
    size_t currentLineStart = lineStart;
    TZrInt32 currentLine = line;

    if (content == NULL ||
        line <= 0 ||
        ownerLineStart == NULL ||
        ownerLineEnd == NULL ||
        ownerLine == NULL) {
        return 0;
    }

    while (currentLine > 0) {
        size_t previousEnd = currentLineStart;
        size_t previousStart;
        size_t suffix;
        char suffixChar;

        if (previousEnd > 0 && content[previousEnd - 1] == '\n') {
            previousEnd--;
        }
        if (previousEnd > 0 && content[previousEnd - 1] == '\r') {
            previousEnd--;
        }
        previousStart = previousEnd;
        while (previousStart > 0 &&
               content[previousStart - 1] != '\n' &&
               content[previousStart - 1] != '\r') {
            previousStart--;
        }

        suffix = previousEnd;
        while (suffix > previousStart &&
               (content[suffix - 1] == ' ' || content[suffix - 1] == '\t')) {
            suffix--;
        }
        if (suffix <= previousStart) {
            return 0;
        }

        suffixChar = content[suffix - 1];
        if (strchr("+-*/%&|?:,(", suffixChar) == NULL) {
            return 0;
        }

        currentLineStart = previousStart;
        currentLine--;
        if (!inline_value_line_is_continuation(content, currentLineStart)) {
            *ownerLineStart = previousStart;
            *ownerLineEnd = previousEnd;
            *ownerLine = currentLine;
            break;
        }
    }

    return currentLine > 0 || currentLineStart == 0;
}

static int inline_value_find_continuation_initializer_owner(const char *content,
                                                           size_t lineStart,
                                                           TZrInt32 line,
                                                           size_t *ownerLineStart,
                                                           size_t *ownerLineEnd,
                                                           TZrInt32 *ownerLine) {
    size_t currentLineStart = lineStart;
    TZrInt32 currentLine = line;

    if (content == NULL ||
        line <= 0 ||
        ownerLineStart == NULL ||
        ownerLineEnd == NULL ||
        ownerLine == NULL) {
        return 0;
    }

    while (currentLine > 0) {
        size_t previousEnd = currentLineStart;
        size_t previousStart;
        size_t suffix;
        char suffixChar;

        if (previousEnd > 0 && content[previousEnd - 1] == '\n') {
            previousEnd--;
        }
        if (previousEnd > 0 && content[previousEnd - 1] == '\r') {
            previousEnd--;
        }
        previousStart = previousEnd;
        while (previousStart > 0 &&
               content[previousStart - 1] != '\n' &&
               content[previousStart - 1] != '\r') {
            previousStart--;
        }

        suffix = previousEnd;
        while (suffix > previousStart &&
               (content[suffix - 1] == ' ' || content[suffix - 1] == '\t')) {
            suffix--;
        }
        if (suffix <= previousStart) {
            return 0;
        }

        suffixChar = content[suffix - 1];
        if (strchr("+-*/%&|?:,(=", suffixChar) == NULL) {
            return 0;
        }

        currentLineStart = previousStart;
        currentLine--;
        if (!inline_value_line_is_continuation(content, currentLineStart)) {
            *ownerLineStart = previousStart;
            *ownerLineEnd = previousEnd;
            *ownerLine = currentLine;
            break;
        }
    }

    return currentLine > 0 || currentLineStart == 0;
}

static void inline_value_emit_continuation_initializer(const char *content,
                                                       size_t lineStart,
                                                       size_t lineEnd,
                                                       size_t contentLength,
                                                       TZrInt32 line,
                                                       SZrLspRange requestRange,
                                                       SZrStdioServer *server,
                                                       SZrString *uri,
                                                       cJSON *result) {
    size_t visibleOffset = lineStart;
    size_t ownerLineStart;
    size_t ownerLineEnd;
    TZrInt32 ownerLine;
    size_t ownerOffset;
    size_t keywordEnd;
    size_t nameStart;
    size_t nameEnd;
    size_t initializerStart;
    size_t statementEnd;
    size_t expressionEnd;
    size_t queryOffset;
    SZrLspRange nameRange;
    SZrLspPosition queryPosition;
    cJSON *value;

    if (content == NULL || line != requestRange.start.line) {
        return;
    }

    while (visibleOffset < lineEnd &&
           (content[visibleOffset] == ' ' || content[visibleOffset] == '\t')) {
        visibleOffset++;
    }
    if (visibleOffset >= lineEnd ||
        !inline_value_position_in_range(requestRange,
                                        line,
                                        (TZrInt32)(visibleOffset - lineStart))) {
        return;
    }

    if (!inline_value_find_continuation_initializer_owner(content,
                                                         lineStart,
                                                         line,
                                                         &ownerLineStart,
                                                         &ownerLineEnd,
                                                         &ownerLine) ||
        ownerLine >= requestRange.start.line) {
        return;
    }

    ownerOffset = ownerLineStart;
    while (ownerOffset < ownerLineEnd &&
           (content[ownerOffset] == ' ' || content[ownerOffset] == '\t')) {
        ownerOffset++;
    }
    if (!ZrStdioInlineValue_IsKeywordAt(content,
                                        ownerLineStart,
                                        ownerLineEnd,
                                        ownerOffset,
                                        "var")) {
        return;
    }

    keywordEnd = ownerOffset + 3;
    if (keywordEnd >= ownerLineEnd ||
        (content[keywordEnd] != ' ' && content[keywordEnd] != '\t')) {
        return;
    }

    nameStart = keywordEnd;
    while (nameStart < ownerLineEnd &&
           (content[nameStart] == ' ' || content[nameStart] == '\t')) {
        nameStart++;
    }
    if (nameStart >= ownerLineEnd || !ZrStdioInlineValue_IsIdentifierStart(content[nameStart])) {
        return;
    }

    nameEnd = nameStart + 1;
    while (nameEnd < ownerLineEnd && ZrStdioInlineValue_IsIdentifierPart(content[nameEnd])) {
        nameEnd++;
    }

    initializerStart = nameEnd;
    while (initializerStart < ownerLineEnd &&
           content[initializerStart] != '=' &&
           content[initializerStart] != ';' &&
           content[initializerStart] != '\n' &&
           content[initializerStart] != '\r') {
        initializerStart++;
    }
    if (initializerStart >= ownerLineEnd || content[initializerStart] != '=') {
        return;
    }

    initializerStart = inline_value_skip_multiline_whitespace(content,
                                                             initializerStart + 1,
                                                             contentLength);
    statementEnd = inline_value_find_multiline_statement_end(content,
                                                            initializerStart,
                                                            contentLength);
    if (statementEnd >= contentLength || content[statementEnd] != ';') {
        return;
    }

    expressionEnd = inline_value_trim_expression_end(content, initializerStart, statementEnd);
    if (initializerStart >= expressionEnd || expressionEnd < lineStart) {
        return;
    }

    queryOffset = ZrStdioInlineValue_FindSemanticQueryOffset(content,
                                                             initializerStart,
                                                             expressionEnd);
    nameRange.start.line = ownerLine;
    nameRange.start.character = (TZrInt32)(nameStart - ownerLineStart);
    nameRange.end.line = ownerLine;
    nameRange.end.character = (TZrInt32)(nameEnd - ownerLineStart);
    queryPosition = inline_value_position_from_offset(content,
                                                      ownerLineStart,
                                                      ownerLine,
                                                      queryOffset);
    value = ZrStdioInlineValue_CreateSemanticTextForLspRange(server,
                                                             uri,
                                                             nameRange,
                                                             queryPosition);
    if (value != NULL) {
        cJSON_AddItemToArray(result, value);
    }
}

static void inline_value_emit_continuation_expression_statement(const char *content,
                                                               size_t lineStart,
                                                               size_t lineEnd,
                                                               size_t contentLength,
                                                               TZrInt32 line,
                                                               SZrLspRange requestRange,
                                                               SZrStdioServer *server,
                                                               SZrString *uri,
                                                               cJSON *result) {
    size_t visibleOffset = lineStart;
    size_t ownerLineStart;
    size_t ownerLineEnd;
    TZrInt32 ownerLine;
    size_t expressionStart;
    size_t statementEnd;
    size_t expressionEnd;
    size_t queryOffset;
    cJSON *value;

    if (content == NULL || line != requestRange.start.line) {
        return;
    }

    while (visibleOffset < lineEnd &&
           (content[visibleOffset] == ' ' || content[visibleOffset] == '\t')) {
        visibleOffset++;
    }
    if (visibleOffset >= lineEnd ||
        !inline_value_position_in_range(requestRange,
                                        line,
                                        (TZrInt32)(visibleOffset - lineStart))) {
        return;
    }

    if (!inline_value_find_continuation_expression_owner(content,
                                                        lineStart,
                                                        line,
                                                        &ownerLineStart,
                                                        &ownerLineEnd,
                                                        &ownerLine) ||
        ownerLine >= requestRange.start.line) {
        return;
    }

    expressionStart = ownerLineStart;
    while (expressionStart < ownerLineEnd &&
           (content[expressionStart] == ' ' || content[expressionStart] == '\t')) {
        expressionStart++;
    }
    if (!ZrStdioInlineValue_IsExpressionStatementStart(content,
                                                       ownerLineStart,
                                                       ownerLineEnd,
                                                       contentLength,
                                                       expressionStart)) {
        return;
    }

    statementEnd = ZrStdioInlineValue_FindExpressionStatementEnd(content, expressionStart, contentLength);
    if (statementEnd >= contentLength || content[statementEnd] != ';' || statementEnd < lineStart) {
        return;
    }

    expressionEnd = inline_value_trim_expression_end(content, expressionStart, statementEnd);
    if (expressionStart >= expressionEnd) {
        return;
    }

    queryOffset = ZrStdioInlineValue_FindSemanticQueryOffset(content, expressionStart, expressionEnd);
    value = inline_value_create_semantic_text_for_offsets(server,
                                                          uri,
                                                          content,
                                                          ownerLineStart,
                                                          ownerLine,
                                                          expressionStart,
                                                          expressionEnd,
                                                          queryOffset);
    if (value != NULL) {
        cJSON_AddItemToArray(result, value);
    }
}

static void inline_value_scan_line(const char *content,
                                   size_t lineStart,
                                   size_t lineEnd,
                                   size_t contentLength,
                                   TZrInt32 line,
                                   SZrLspRange requestRange,
                                   SZrStdioServer *server,
                                   SZrString *uri,
                                   cJSON *result,
                                   int *inBlockComment) {
    size_t codeStart = lineStart;
    size_t commentStart = lineStart;
    size_t offset;
    TZrInt32 character;

    if (!ZrStdioInlineValue_FindCodeSpanOnLine(content,
                                               lineStart,
                                               lineEnd,
                                               inBlockComment,
                                               &codeStart,
                                               &commentStart)) {
        return;
    }
    offset = codeStart;
    character = (TZrInt32)(codeStart - lineStart);

    inline_value_emit_continuation_expression_statement(content,
                                                        lineStart,
                                                        commentStart,
                                                        contentLength,
                                                        line,
                                                        requestRange,
                                                        server,
                                                        uri,
                                                        result);
    inline_value_emit_continuation_initializer(content,
                                              lineStart,
                                              commentStart,
                                              contentLength,
                                              line,
                                              requestRange,
                                              server,
                                              uri,
                                              result);

    while (offset < commentStart) {
        size_t keywordEnd;
        size_t nameStart;
        size_t nameEnd;
        size_t initializerStart;
        size_t statementEnd;
        size_t queryOffset;
        TZrInt32 nameCharacter;
        cJSON *value;

        if (inline_value_position_in_range(requestRange, line, character) &&
            ZrStdioInlineValue_IsKeywordAt(content, lineStart, commentStart, offset, "return")) {
            size_t expressionStart = inline_value_skip_multiline_whitespace(content,
                                                                            offset + 6,
                                                                            contentLength);
            size_t expressionEnd;

            statementEnd = inline_value_find_multiline_statement_end(content, expressionStart, contentLength);
            expressionEnd = inline_value_trim_expression_end(content, expressionStart, statementEnd);
            if (expressionStart < expressionEnd) {
                queryOffset = ZrStdioInlineValue_FindSemanticQueryOffset(
                        content,
                        expressionStart,
                        expressionEnd);
                value = inline_value_create_semantic_text_for_offsets(
                        server,
                        uri,
                        content,
                        lineStart,
                        line,
                        expressionStart,
                        expressionEnd,
                        queryOffset);
                if (value != NULL) {
                    cJSON_AddItemToArray(result, value);
                }
            }

            if (statementEnd <= commentStart) {
                character += (TZrInt32)(statementEnd - offset);
                offset = statementEnd;
            } else {
                character = (TZrInt32)(commentStart - lineStart);
                offset = commentStart;
            }
            continue;
        }

        if (inline_value_position_in_range(requestRange, line, character) &&
            !inline_value_line_is_continuation(content, lineStart) &&
            inline_value_has_only_whitespace_before(content, lineStart, offset) &&
            ZrStdioInlineValue_IsExpressionStatementStart(content,
                                                          lineStart,
                                                          commentStart,
                                                          contentLength,
                                                          offset)) {
            size_t expressionStart = offset;
            size_t expressionEnd;

            statementEnd = ZrStdioInlineValue_FindExpressionStatementEnd(content, expressionStart, contentLength);
            if (statementEnd < contentLength && content[statementEnd] == ';') {
                expressionEnd = inline_value_trim_expression_end(content, expressionStart, statementEnd);
                if (expressionStart < expressionEnd) {
                    queryOffset = ZrStdioInlineValue_FindSemanticQueryOffset(
                            content,
                            expressionStart,
                            expressionEnd);
                    value = inline_value_create_semantic_text_for_offsets(
                            server,
                            uri,
                            content,
                            lineStart,
                            line,
                            expressionStart,
                            expressionEnd,
                            queryOffset);
                    if (value != NULL) {
                        cJSON_AddItemToArray(result, value);
                    }
                }
            }

            if (statementEnd <= commentStart) {
                character += (TZrInt32)(statementEnd - offset);
                offset = statementEnd;
            } else {
                character = (TZrInt32)(commentStart - lineStart);
                offset = commentStart;
            }
            continue;
        }

        if (!inline_value_position_in_range(requestRange, line, character) ||
            !ZrStdioInlineValue_IsKeywordAt(content, lineStart, commentStart, offset, "var")) {
            offset++;
            character++;
            continue;
        }

        keywordEnd = offset + 3;
        if (keywordEnd >= commentStart || (content[keywordEnd] != ' ' && content[keywordEnd] != '\t')) {
            offset++;
            character++;
            continue;
        }

        nameStart = keywordEnd;
        while (nameStart < commentStart && (content[nameStart] == ' ' || content[nameStart] == '\t')) {
            nameStart++;
        }
        if (nameStart >= commentStart || !ZrStdioInlineValue_IsIdentifierStart(content[nameStart])) {
            offset++;
            character++;
            continue;
        }

        nameCharacter = character + (TZrInt32)(nameStart - offset);
        nameEnd = nameStart + 1;
        while (nameEnd < commentStart && ZrStdioInlineValue_IsIdentifierPart(content[nameEnd])) {
            nameEnd++;
        }

        if (inline_value_position_in_range(requestRange, line, nameCharacter)) {
            value = inline_value_create_variable_lookup(line,
                                                        nameCharacter,
                                                        content + nameStart,
                                                        nameEnd - nameStart);
            if (value != NULL) {
                cJSON_AddItemToArray(result, value);
            }

            initializerStart = nameEnd;
            while (initializerStart < commentStart &&
                   content[initializerStart] != '=' &&
                   content[initializerStart] != ';' &&
                   content[initializerStart] != '\n' &&
                   content[initializerStart] != '\r') {
                initializerStart++;
            }
            if (initializerStart < commentStart && content[initializerStart] == '=') {
                initializerStart++;
                while (initializerStart < contentLength &&
                       (content[initializerStart] == ' ' ||
                        content[initializerStart] == '\t' ||
                        content[initializerStart] == '\n' ||
                        content[initializerStart] == '\r')) {
                    initializerStart++;
                }
                if (initializerStart < contentLength) {
                    SZrLspRange nameRange;
                    SZrLspPosition queryPosition;

                    statementEnd = inline_value_find_multiline_statement_end(content, initializerStart, contentLength);
                    queryOffset = ZrStdioInlineValue_FindSemanticQueryOffset(content,
                                                                             initializerStart,
                                                                             statementEnd);
                    nameRange.start.line = line;
                    nameRange.start.character = nameCharacter;
                    nameRange.end.line = line;
                    nameRange.end.character = nameCharacter + (TZrInt32)(nameEnd - nameStart);
                    queryPosition = inline_value_position_from_offset(content, lineStart, line, queryOffset);
                    value = ZrStdioInlineValue_CreateSemanticTextForLspRange(server,
                                                                             uri,
                                                                             nameRange,
                                                                             queryPosition);
                    if (value != NULL) {
                        cJSON_AddItemToArray(result, value);
                    }
                }
            }
        }

        character += (TZrInt32)(nameEnd - offset);
        offset = nameEnd;
    }
}

cJSON *handle_inline_value_request(SZrStdioServer *server, const cJSON *params) {
    const char *uriText;
    SZrString *uri;
    SZrLspRange requestRange;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    const char *content;
    size_t contentLength;
    size_t lineStart = 0;
    TZrInt32 line = 0;
    cJSON *result;
    int inBlockComment = 0;

    if (!get_uri_from_text_document(server, params, &uriText, &uri) ||
        !parse_range_for_uri(server, uri, get_object_item(params, ZR_LSP_FIELD_RANGE), &requestRange)) {
        return cJSON_CreateArray();
    }
    ZR_UNUSED_PARAMETER(uriText);

    fileVersion = get_file_version_for_uri(server, uri);
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(server->state, fileVersion, &snapshot)) {
        return cJSON_CreateArray();
    }

    result = cJSON_CreateArray();
    if (result == NULL) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
        return NULL;
    }

    content = snapshot.content;
    contentLength = snapshot.contentLength;
    for (size_t offset = 0; offset <= contentLength; offset++) {
        if (offset == contentLength || content[offset] == '\n') {
            if (line >= requestRange.start.line && line <= requestRange.end.line) {
                inline_value_scan_line(content,
                                       lineStart,
                                       offset,
                                       contentLength,
                                       line,
                                       requestRange,
                                       server,
                                       uri,
                                       result,
                                       &inBlockComment);
            } else {
                size_t codeStart;
                size_t codeEnd;
                (void)ZrStdioInlineValue_FindCodeSpanOnLine(content,
                                                            lineStart,
                                                            offset,
                                                            &inBlockComment,
                                                            &codeStart,
                                                            &codeEnd);
            }
            line++;
            lineStart = offset + 1;
        }
    }

    ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
    return result;
}
