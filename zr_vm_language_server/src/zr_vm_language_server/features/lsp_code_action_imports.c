#include "features/lsp_code_actions_internal.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* 组织导入时暂存首个别名对应的裁剪后导入行；文本由本模块分配并在编辑生成后释放。 */
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

/* 成功与分配失败均走同一释放边界；count 只包含已拥有 text 的条目。 */
static void import_action_free_import_lines(SZrLspImportLine *lines, TZrSize count) {
    if (lines == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < count; index++) {
        free(lines[index].text);
    }
    free(lines);
}

static TZrBool import_action_skip_binding_keyword(const TZrChar *line,
                                                  TZrSize length,
                                                  TZrSize *cursor) {
    if (line == ZR_NULL || cursor == ZR_NULL) {
        return ZR_FALSE;
    }
    if (length >= 4 && memcmp(line, "let ", 4) == 0) {
        *cursor = 4;
        return ZR_TRUE;
    }
    if (length >= 4 && memcmp(line, "var ", 4) == 0) {
        *cursor = 4;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool import_action_is_import_call(const TZrChar *line, TZrSize length, TZrSize cursor) {
    return length - cursor >= strlen("import(") &&
           memcmp(line + cursor, "import(", strlen("import(")) == 0;
}

/* 识别顶部 import 声明供整理与缺失导入插入点共用；输入须已去除行首缩进。 */
TZrBool lsp_code_action_trimmed_line_is_import_declaration(const TZrChar *line, TZrSize length) {
    TZrSize cursor = 0;

    if (line == ZR_NULL || length == 0) {
        return ZR_FALSE;
    }
    if (!import_action_skip_binding_keyword(line, length, &cursor)) {
        return ZR_FALSE;
    }

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
    return import_action_is_import_call(line, length, cursor);
}

/* 别名输出借用输入行的切片；整理/清理动作只能在原内容仍有效时比较它。 */
static TZrBool import_action_try_get_import_alias(const TZrChar *line,
                                                  TZrSize length,
                                                  const TZrChar **alias,
                                                  TZrSize *aliasLength) {
    TZrSize cursor = 0;
    TZrSize aliasStart;

    if (line == ZR_NULL || alias == ZR_NULL || aliasLength == ZR_NULL ||
        !import_action_skip_binding_keyword(line, length, &cursor)) {
        return ZR_FALSE;
    }

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

/* 同一别名只保留最先出现的导入，即使后续行指向别的模块；对应既有去重回归。 */
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

/* remove-unused 只把导入块外的代码标识符当作使用；模板 ${...} 里的表达式算使用。 */
typedef struct SZrLspImportAliasUseScan {
    const TZrChar *content;
    TZrSize contentLength;
    const TZrChar *alias;
    TZrSize aliasLength;
    TZrSize ignoreStart;
    TZrSize ignoreEnd;
    TZrBool found;
} SZrLspImportAliasUseScan;

static TZrBool import_action_alias_use_callback(TZrSize offset, void *userData) {
    SZrLspImportAliasUseScan *data = (SZrLspImportAliasUseScan *)userData;

    if (data == ZR_NULL) {
        return ZR_FALSE;
    }
    if (offset >= data->ignoreStart && offset < data->ignoreEnd) {
        return ZR_TRUE;
    }
    if (import_action_identifier_at(data->content,
                                    data->contentLength,
                                    offset,
                                    data->alias,
                                    data->aliasLength)) {
        data->found = ZR_TRUE;
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
    SZrLspEditorScanState scanState = ZR_LSP_EDITOR_SCAN_STATE_CODE_INIT;
    SZrLspImportAliasUseScan data;

    if (content == ZR_NULL || alias == ZR_NULL || aliasLength == 0) {
        return ZR_FALSE;
    }

    memset(&data, 0, sizeof(data));
    data.content = content;
    data.contentLength = contentLength;
    data.alias = alias;
    data.aliasLength = aliasLength;
    data.ignoreStart = ignoreStart;
    data.ignoreEnd = ignoreEnd;
    (void)lsp_editor_visit_code_bytes(content,
                                      contentLength,
                                      0,
                                      contentLength,
                                      &scanState,
                                      import_action_alias_use_callback,
                                      &data);
    return data.found;
}

/* source 动作仅修改文件顶部、可选 module 声明后的连续导入块；正文导入不参与本动作。 */
static TZrBool import_action_find_import_block(const TZrChar *content,
                                               TZrSize contentLength,
                                               TZrSize *outStart,
                                               TZrSize *outEnd) {
    TZrSize cursor = 0;
    TZrSize firstImportStart = 0;
    TZrSize lastImportEnd = 0;
    TZrBool foundImport = ZR_FALSE;

    if (content == ZR_NULL || outStart == ZR_NULL || outEnd == ZR_NULL) {
        return ZR_FALSE;
    }

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

/* 给 source.organizeImports 收集单个替换编辑；空差异为成功且 edits 不变。
 * 编辑追加到调用方持有的 edits；内部快照、临时导入行和 builder 均在返回前释放。
 */
TZrBool lsp_code_action_collect_import_organize_edit(SZrState *state,
                                                     SZrFileVersion *fileVersion,
                                                     SZrArray *edits) {
    SZrFileVersionContentSnapshot snapshot = {0};
    const TZrChar *content;
    TZrSize contentLength;
    TZrSize importBlockStart = 0;
    TZrSize importBlockEnd = 0;
    TZrSize cursor;
    SZrLspImportLine *imports = ZR_NULL;
    TZrSize importCount = 0;
    TZrSize importCapacity = 0;
    SZrLspTextBuilder builder = {0};
    SZrLspRange editRange;

    if (state == ZR_NULL || edits == ZR_NULL ||
        !ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        return ZR_FALSE;
    }
    content = snapshot.content;
    contentLength = snapshot.contentLength;

    if (!import_action_find_import_block(content, contentLength, &importBlockStart, &importBlockEnd)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
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
                        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
                        return ZR_FALSE;
                    }
                    imports = next;
                    importCapacity = nextCapacity;
                }
                imports[importCount].text = (TZrChar *)malloc(length + 1);
                if (imports[importCount].text == ZR_NULL) {
                    import_action_free_import_lines(imports, importCount);
                    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
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
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_TRUE;
    }

    /* 排序仅改变顶部导入块，保留首个同名别名的语义选择，并沿用块内已有的 CRLF。 */
    qsort(imports, importCount, sizeof(SZrLspImportLine), import_action_compare_import_lines);
    {
        TZrBool useCrlf = ZR_FALSE;
        for (TZrSize index = importBlockStart; index + 1 < importBlockEnd; index++) {
            if (content[index] == '\r' && content[index + 1] == '\n') {
                useCrlf = ZR_TRUE;
                break;
            }
        }
        for (TZrSize index = 0; index < importCount; index++) {
            if (!lsp_text_builder_append_range(&builder, imports[index].text, imports[index].length) ||
                (useCrlf
                     ? !lsp_text_builder_append_range(&builder, "\r\n", 2)
                     : !lsp_text_builder_append_char(&builder, '\n'))) {
                import_action_free_import_lines(imports, importCount);
                free(builder.data);
                ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
                return ZR_FALSE;
            }
        }
    }

    if (builder.length == importBlockEnd - importBlockStart &&
        memcmp(builder.data, content + importBlockStart, builder.length) == 0) {
        import_action_free_import_lines(imports, importCount);
        free(builder.data);
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_TRUE;
    }

    editRange = lsp_editor_range_from_offsets(content, contentLength, importBlockStart, importBlockEnd);
    if (!lsp_editor_append_text_edit(state, edits, editRange, builder.data, builder.length)) {
        import_action_free_import_lines(imports, importCount);
        free(builder.data);
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }

    import_action_free_import_lines(imports, importCount);
    free(builder.data);
    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    return ZR_TRUE;
}

/* 给 source.removeUnused 逐行收集删除编辑；别名在正文代码中出现即保留导入。
 * 失败时调用方负责释放已追加到 edits 的部分结果。
 */
TZrBool lsp_code_action_collect_unused_import_cleanup_edit(SZrState *state,
                                                           SZrFileVersion *fileVersion,
                                                           SZrArray *edits) {
    SZrFileVersionContentSnapshot snapshot = {0};
    const TZrChar *content;
    TZrSize contentLength;
    TZrSize importBlockStart = 0;
    TZrSize importBlockEnd = 0;
    TZrSize cursor;

    if (state == ZR_NULL || edits == ZR_NULL ||
        !ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        return ZR_FALSE;
    }

    content = snapshot.content;
    contentLength = snapshot.contentLength;
    if (!import_action_find_import_block(content, contentLength, &importBlockStart, &importBlockEnd)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_TRUE;
    }

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
                ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
                return ZR_FALSE;
            }
        }

        cursor = deleteEnd;
    }

    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    return ZR_TRUE;
}
