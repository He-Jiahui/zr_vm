#include "lsp_code_actions_internal.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct SZrLspImportLine {
    TZrChar *text;
    TZrSize length;
} SZrLspImportLine;

static TZrBool import_action_is_identifier_start(TZrChar ch) {
    return (TZrBool)(isalpha((unsigned char)ch) || ch == '_');
}

static TZrBool import_action_is_identifier_part(TZrChar ch) {
    return (TZrBool)(isalnum((unsigned char)ch) || ch == '_');
}

static int import_action_compare_import_lines(const void *left, const void *right) {
    const SZrLspImportLine *leftLine = (const SZrLspImportLine *)left;
    const SZrLspImportLine *rightLine = (const SZrLspImportLine *)right;
    return strcmp(leftLine->text, rightLine->text);
}

static void import_action_free_import_lines(SZrLspImportLine *lines, TZrSize count) {
    if (lines == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < count; index++) {
        free(lines[index].text);
    }
    free(lines);
}

static TZrBool import_action_line_has_prefix(const TZrChar *line, TZrSize length, const TZrChar *prefix) {
    TZrSize prefixLength = prefix != ZR_NULL ? strlen(prefix) : 0;
    return line != ZR_NULL && prefix != ZR_NULL && length >= prefixLength &&
           memcmp(line, prefix, prefixLength) == 0;
}

TZrBool lsp_code_action_trimmed_line_is_import_declaration(const TZrChar *line, TZrSize length) {
    TZrSize cursor = 0;

    if (line == ZR_NULL || length == 0) {
        return ZR_FALSE;
    }
    if (import_action_line_has_prefix(line, length, "%import")) {
        return ZR_TRUE;
    }
    if (!import_action_line_has_prefix(line, length, "var ")) {
        return ZR_FALSE;
    }

    cursor = 4;
    while (cursor < length && (line[cursor] == ' ' || line[cursor] == '\t')) {
        cursor++;
    }
    if (cursor >= length || !import_action_is_identifier_start(line[cursor])) {
        return ZR_FALSE;
    }
    while (cursor < length && import_action_is_identifier_part(line[cursor])) {
        cursor++;
    }
    while (cursor < length && (line[cursor] == ' ' || line[cursor] == '\t')) {
        cursor++;
    }
    if (cursor >= length || line[cursor] != '=') {
        return ZR_FALSE;
    }
    cursor++;
    while (cursor < length && (line[cursor] == ' ' || line[cursor] == '\t')) {
        cursor++;
    }
    return length - cursor >= strlen("%import(") &&
           memcmp(line + cursor, "%import(", strlen("%import(")) == 0;
}

static TZrBool import_action_try_get_import_alias(const TZrChar *line,
                                                  TZrSize length,
                                                  const TZrChar **alias,
                                                  TZrSize *aliasLength) {
    TZrSize cursor = 0;
    TZrSize aliasStart;

    if (line == ZR_NULL || alias == ZR_NULL || aliasLength == ZR_NULL ||
        length < 4 || memcmp(line, "var ", 4) != 0) {
        return ZR_FALSE;
    }

    cursor = 4;
    while (cursor < length && (line[cursor] == ' ' || line[cursor] == '\t')) {
        cursor++;
    }
    if (cursor >= length || !import_action_is_identifier_start(line[cursor])) {
        return ZR_FALSE;
    }

    aliasStart = cursor;
    while (cursor < length && import_action_is_identifier_part(line[cursor])) {
        cursor++;
    }
    *alias = line + aliasStart;
    *aliasLength = cursor - aliasStart;
    return *aliasLength > 0;
}

static TZrBool import_action_line_identity_exists(SZrLspImportLine *lines,
                                                  TZrSize count,
                                                  const TZrChar *text,
                                                  TZrSize length) {
    const TZrChar *alias = ZR_NULL;
    TZrSize aliasLength = 0;
    TZrBool hasAlias = import_action_try_get_import_alias(text, length, &alias, &aliasLength);

    for (TZrSize index = 0; index < count; index++) {
        const TZrChar *existingAlias = ZR_NULL;
        TZrSize existingAliasLength = 0;
        TZrBool existingHasAlias =
            import_action_try_get_import_alias(lines[index].text,
                                               lines[index].length,
                                               &existingAlias,
                                               &existingAliasLength);
        if (hasAlias || existingHasAlias) {
            if (hasAlias && existingHasAlias &&
                aliasLength == existingAliasLength &&
                memcmp(alias, existingAlias, aliasLength) == 0) {
                return ZR_TRUE;
            }
            continue;
        }
        if (lines[index].length == length && memcmp(lines[index].text, text, length) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrSize import_action_skip_quoted_span(const TZrChar *content,
                                              TZrSize cursor,
                                              TZrSize contentLength) {
    TZrChar quote;
    TZrBool escaped = ZR_FALSE;

    if (content == ZR_NULL || cursor >= contentLength ||
        (content[cursor] != '"' && content[cursor] != '\'' && content[cursor] != '`')) {
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

static TZrBool import_action_identifier_at(const TZrChar *content,
                                           TZrSize contentLength,
                                           TZrSize offset,
                                           const TZrChar *alias,
                                           TZrSize aliasLength) {
    if (content == ZR_NULL || alias == ZR_NULL || aliasLength == 0 ||
        offset + aliasLength > contentLength ||
        memcmp(content + offset, alias, aliasLength) != 0) {
        return ZR_FALSE;
    }
    if (offset > 0 && import_action_is_identifier_part(content[offset - 1])) {
        return ZR_FALSE;
    }
    if (offset + aliasLength < contentLength &&
        import_action_is_identifier_part(content[offset + aliasLength])) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool import_action_content_uses_alias_outside_range(const TZrChar *content,
                                                              TZrSize contentLength,
                                                              const TZrChar *alias,
                                                              TZrSize aliasLength,
                                                              TZrSize ignoreStart,
                                                              TZrSize ignoreEnd) {
    TZrSize cursor = 0;

    if (content == ZR_NULL || alias == ZR_NULL || aliasLength == 0) {
        return ZR_FALSE;
    }

    while (cursor < contentLength) {
        TZrSize nextCursor;

        if (cursor >= ignoreStart && cursor < ignoreEnd) {
            cursor = ignoreEnd;
            continue;
        }
        if (cursor + 1 < contentLength && content[cursor] == '/' && content[cursor + 1] == '/') {
            while (cursor < contentLength && content[cursor] != '\n') {
                cursor++;
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
            continue;
        }

        nextCursor = import_action_skip_quoted_span(content, cursor, contentLength);
        if (nextCursor != cursor) {
            cursor = nextCursor;
            continue;
        }

        if (import_action_identifier_at(content, contentLength, cursor, alias, aliasLength)) {
            return ZR_TRUE;
        }
        cursor++;
    }

    return ZR_FALSE;
}

static TZrBool import_action_find_import_block(SZrFileVersion *fileVersion,
                                               TZrSize *outStart,
                                               TZrSize *outEnd) {
    const TZrChar *content;
    TZrSize contentLength;
    TZrSize cursor = 0;
    TZrSize firstImportStart = 0;
    TZrSize lastImportEnd = 0;
    TZrBool foundImport = ZR_FALSE;

    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL ||
        outStart == ZR_NULL || outEnd == ZR_NULL) {
        return ZR_FALSE;
    }

    content = fileVersion->content;
    contentLength = fileVersion->contentLength;
    while (cursor < contentLength) {
        TZrSize lineStart = cursor;
        TZrSize lineEnd = cursor;
        TZrSize trimStart;
        TZrSize trimEnd;
        TZrBool isBlank;
        TZrBool isImport;
        TZrBool isModuleDeclaration;

        while (lineEnd < contentLength && content[lineEnd] != '\n') {
            lineEnd++;
        }
        trimStart = lineStart;
        trimEnd = lineEnd;
        if (trimEnd > trimStart && content[trimEnd - 1] == '\r') {
            trimEnd--;
        }
        while (trimStart < trimEnd && (content[trimStart] == ' ' || content[trimStart] == '\t')) {
            trimStart++;
        }
        while (trimEnd > trimStart && (content[trimEnd - 1] == ' ' || content[trimEnd - 1] == '\t')) {
            trimEnd--;
        }

        isBlank = trimStart == trimEnd;
        isImport = lsp_editor_offset_is_code(content, contentLength, trimStart) &&
                   lsp_code_action_trimmed_line_is_import_declaration(content + trimStart, trimEnd - trimStart);
        isModuleDeclaration = lsp_editor_offset_is_code(content, contentLength, trimStart) &&
                              trimEnd - trimStart >= 7 && memcmp(content + trimStart, "module ", 7) == 0;
        if (!foundImport && isModuleDeclaration) {
            cursor = lineEnd < contentLength && content[lineEnd] == '\n' ? lineEnd + 1 : lineEnd;
            firstImportStart = cursor;
            continue;
        }
        if (!isBlank && !isImport) {
            break;
        }
        if (isImport) {
            if (!foundImport) {
                firstImportStart = lineStart;
            }
            foundImport = ZR_TRUE;
            lastImportEnd = lineEnd < contentLength && content[lineEnd] == '\n' ? lineEnd + 1 : lineEnd;
        }
        cursor = lineEnd < contentLength && content[lineEnd] == '\n' ? lineEnd + 1 : lineEnd;
    }

    *outStart = firstImportStart;
    *outEnd = lastImportEnd;
    return foundImport;
}

TZrBool lsp_code_action_collect_import_organize_edit(SZrState *state,
                                                     SZrFileVersion *fileVersion,
                                                     SZrArray *edits) {
    const TZrChar *content = fileVersion->content;
    TZrSize contentLength = fileVersion->contentLength;
    TZrSize importBlockStart = 0;
    TZrSize importBlockEnd = 0;
    TZrSize cursor;
    SZrLspImportLine *imports = ZR_NULL;
    TZrSize importCount = 0;
    TZrSize importCapacity = 0;
    SZrLspTextBuilder builder = {0};
    SZrLspRange editRange;

    if (!import_action_find_import_block(fileVersion, &importBlockStart, &importBlockEnd)) {
        return ZR_TRUE;
    }

    cursor = importBlockStart;
    while (cursor < importBlockEnd) {
        TZrSize lineStart = cursor;
        TZrSize lineEnd = cursor;
        TZrSize trimStart;
        TZrSize trimEnd;
        TZrBool isImport;

        while (lineEnd < importBlockEnd && content[lineEnd] != '\n') {
            lineEnd++;
        }
        trimStart = lineStart;
        trimEnd = lineEnd;
        if (trimEnd > trimStart && content[trimEnd - 1] == '\r') {
            trimEnd--;
        }
        while (trimStart < trimEnd &&
               (content[trimStart] == ' ' || content[trimStart] == '\t')) {
            trimStart++;
        }
        while (trimEnd > trimStart &&
               (content[trimEnd - 1] == ' ' || content[trimEnd - 1] == '\t')) {
            trimEnd--;
        }

        isImport = lsp_editor_offset_is_code(content, contentLength, trimStart) &&
                   lsp_code_action_trimmed_line_is_import_declaration(content + trimStart, trimEnd - trimStart);
        if (isImport) {
            TZrSize length = trimEnd - trimStart;
            if (!import_action_line_identity_exists(imports, importCount, content + trimStart, length)) {
                if (importCount == importCapacity) {
                    TZrSize nextCapacity = importCapacity == 0 ? 4 : importCapacity * 2;
                    SZrLspImportLine *next =
                        (SZrLspImportLine *)realloc(imports, sizeof(SZrLspImportLine) * nextCapacity);
                    if (next == ZR_NULL) {
                        import_action_free_import_lines(imports, importCount);
                        return ZR_FALSE;
                    }
                    imports = next;
                    importCapacity = nextCapacity;
                }
                imports[importCount].text = (TZrChar *)malloc(length + 1);
                if (imports[importCount].text == ZR_NULL) {
                    import_action_free_import_lines(imports, importCount);
                    return ZR_FALSE;
                }
                memcpy(imports[importCount].text, content + trimStart, length);
                imports[importCount].text[length] = '\0';
                imports[importCount].length = length;
                importCount++;
            }
        }

        cursor = lineEnd < contentLength && content[lineEnd] == '\n' ? lineEnd + 1 : lineEnd;
    }

    if (importCount == 0) {
        import_action_free_import_lines(imports, importCount);
        return ZR_TRUE;
    }

    qsort(imports, importCount, sizeof(SZrLspImportLine), import_action_compare_import_lines);
    for (TZrSize index = 0; index < importCount; index++) {
        if (!lsp_text_builder_append_range(&builder, imports[index].text, imports[index].length) ||
            !lsp_text_builder_append_char(&builder, '\n')) {
            import_action_free_import_lines(imports, importCount);
            free(builder.data);
            return ZR_FALSE;
        }
    }

    if (builder.length == importBlockEnd - importBlockStart &&
        memcmp(builder.data, content + importBlockStart, builder.length) == 0) {
        import_action_free_import_lines(imports, importCount);
        free(builder.data);
        return ZR_TRUE;
    }

    editRange = lsp_editor_range_from_offsets(content, contentLength, importBlockStart, importBlockEnd);
    if (!lsp_editor_append_text_edit(state, edits, editRange, builder.data, builder.length)) {
        import_action_free_import_lines(imports, importCount);
        free(builder.data);
        return ZR_FALSE;
    }

    import_action_free_import_lines(imports, importCount);
    free(builder.data);
    return ZR_TRUE;
}

TZrBool lsp_code_action_collect_unused_import_cleanup_edit(SZrState *state,
                                                           SZrFileVersion *fileVersion,
                                                           SZrArray *edits) {
    const TZrChar *content;
    TZrSize contentLength;
    TZrSize importBlockStart = 0;
    TZrSize importBlockEnd = 0;
    TZrSize cursor;

    if (state == ZR_NULL || fileVersion == ZR_NULL || fileVersion->content == ZR_NULL || edits == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!import_action_find_import_block(fileVersion, &importBlockStart, &importBlockEnd)) {
        return ZR_TRUE;
    }

    content = fileVersion->content;
    contentLength = fileVersion->contentLength;
    cursor = importBlockStart;
    while (cursor < importBlockEnd) {
        TZrSize lineStart = cursor;
        TZrSize lineEnd = cursor;
        TZrSize deleteEnd;
        TZrSize trimStart;
        TZrSize trimEnd;
        const TZrChar *alias = ZR_NULL;
        TZrSize aliasLength = 0;

        while (lineEnd < importBlockEnd && content[lineEnd] != '\n') {
            lineEnd++;
        }
        deleteEnd = lineEnd < contentLength && content[lineEnd] == '\n' ? lineEnd + 1 : lineEnd;
        trimStart = lineStart;
        trimEnd = lineEnd;
        if (trimEnd > trimStart && content[trimEnd - 1] == '\r') {
            trimEnd--;
        }
        while (trimStart < trimEnd && (content[trimStart] == ' ' || content[trimStart] == '\t')) {
            trimStart++;
        }
        while (trimEnd > trimStart && (content[trimEnd - 1] == ' ' || content[trimEnd - 1] == '\t')) {
            trimEnd--;
        }

        if (lsp_editor_offset_is_code(content, contentLength, trimStart) &&
            import_action_try_get_import_alias(content + trimStart, trimEnd - trimStart, &alias, &aliasLength) &&
            !import_action_content_uses_alias_outside_range(content,
                                                            contentLength,
                                                            alias,
                                                            aliasLength,
                                                            importBlockStart,
                                                            importBlockEnd)) {
            SZrLspRange deleteRange = lsp_editor_range_from_offsets(content, contentLength, lineStart, deleteEnd);
            if (!lsp_editor_append_text_edit(state, edits, deleteRange, "", 0)) {
                return ZR_FALSE;
            }
        }

        cursor = deleteEnd;
    }

    return ZR_TRUE;
}
