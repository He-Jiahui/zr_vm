#include "lsp_editor_features_internal.h"

#include <stdlib.h>
#include <string.h>

typedef struct SZrLspImportLine {
    TZrChar *text;
    TZrSize length;
} SZrLspImportLine;

static int lsp_editor_compare_import_lines(const void *left, const void *right) {
    const SZrLspImportLine *leftLine = (const SZrLspImportLine *)left;
    const SZrLspImportLine *rightLine = (const SZrLspImportLine *)right;
    return strcmp(leftLine->text, rightLine->text);
}

static void lsp_editor_free_import_lines(SZrLspImportLine *lines, TZrSize count) {
    if (lines == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < count; index++) {
        free(lines[index].text);
    }
    free(lines);
}

static TZrBool lsp_editor_import_line_exists(SZrLspImportLine *lines,
                                             TZrSize count,
                                             const TZrChar *text,
                                             TZrSize length) {
    for (TZrSize index = 0; index < count; index++) {
        if (lines[index].length == length && memcmp(lines[index].text, text, length) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool lsp_editor_collect_import_organize_edit(SZrState *state,
                                                       SZrFileVersion *fileVersion,
                                                       SZrArray *edits) {
    const TZrChar *content = fileVersion->content;
    TZrSize contentLength = fileVersion->contentLength;
    TZrSize cursor = 0;
    TZrSize firstImportStart = 0;
    TZrSize lastImportEnd = 0;
    TZrBool foundImport = ZR_FALSE;
    SZrLspImportLine *imports = ZR_NULL;
    TZrSize importCount = 0;
    TZrSize importCapacity = 0;
    SZrLspTextBuilder builder = {0};
    SZrLspRange editRange;

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
        while (trimStart < trimEnd &&
               (content[trimStart] == ' ' || content[trimStart] == '\t')) {
            trimStart++;
        }
        while (trimEnd > trimStart &&
               (content[trimEnd - 1] == ' ' || content[trimEnd - 1] == '\t')) {
            trimEnd--;
        }

        isBlank = trimStart == trimEnd;
        isImport = trimEnd - trimStart >= 8 && memcmp(content + trimStart, "%import", 7) == 0;
        isModuleDeclaration = trimEnd - trimStart >= 7 && memcmp(content + trimStart, "module ", 7) == 0;
        if (!foundImport && isModuleDeclaration) {
            cursor = lineEnd < contentLength && content[lineEnd] == '\n' ? lineEnd + 1 : lineEnd;
            firstImportStart = cursor;
            continue;
        }
        if (!isBlank && !isImport) {
            break;
        }

        if (isImport) {
            TZrSize length = trimEnd - trimStart;
            if (!foundImport) {
                firstImportStart = lineStart;
            }
            foundImport = ZR_TRUE;
            lastImportEnd = lineEnd < contentLength && content[lineEnd] == '\n' ? lineEnd + 1 : lineEnd;

            if (!lsp_editor_import_line_exists(imports, importCount, content + trimStart, length)) {
                if (importCount == importCapacity) {
                    TZrSize nextCapacity = importCapacity == 0 ? 4 : importCapacity * 2;
                    SZrLspImportLine *next =
                        (SZrLspImportLine *)realloc(imports, sizeof(SZrLspImportLine) * nextCapacity);
                    if (next == ZR_NULL) {
                        lsp_editor_free_import_lines(imports, importCount);
                        return ZR_FALSE;
                    }
                    imports = next;
                    importCapacity = nextCapacity;
                }
                imports[importCount].text = (TZrChar *)malloc(length + 1);
                if (imports[importCount].text == ZR_NULL) {
                    lsp_editor_free_import_lines(imports, importCount);
                    return ZR_FALSE;
                }
                memcpy(imports[importCount].text, content + trimStart, length);
                imports[importCount].text[length] = '\0';
                imports[importCount].length = length;
                importCount++;
            }
        }

        cursor = lineEnd < contentLength && content[lineEnd] == '\n' ? lineEnd + 1 : lineEnd;
        if (!isImport && !foundImport) {
            firstImportStart = cursor;
        }
    }

    if (!foundImport || importCount == 0) {
        lsp_editor_free_import_lines(imports, importCount);
        return ZR_TRUE;
    }

    qsort(imports, importCount, sizeof(SZrLspImportLine), lsp_editor_compare_import_lines);
    for (TZrSize index = 0; index < importCount; index++) {
        if (!lsp_text_builder_append_range(&builder, imports[index].text, imports[index].length) ||
            !lsp_text_builder_append_char(&builder, '\n')) {
            lsp_editor_free_import_lines(imports, importCount);
            free(builder.data);
            return ZR_FALSE;
        }
    }

    if (builder.length == lastImportEnd - firstImportStart &&
        memcmp(builder.data, content + firstImportStart, builder.length) == 0) {
        lsp_editor_free_import_lines(imports, importCount);
        free(builder.data);
        return ZR_TRUE;
    }

    editRange = lsp_editor_range_from_offsets(content, contentLength, firstImportStart, lastImportEnd);
    if (!lsp_editor_append_text_edit(state, edits, editRange, builder.data, builder.length)) {
        lsp_editor_free_import_lines(imports, importCount);
        free(builder.data);
        return ZR_FALSE;
    }

    lsp_editor_free_import_lines(imports, importCount);
    free(builder.data);
    return ZR_TRUE;
}

static TZrBool lsp_editor_line_has_prefix(const TZrChar *line, TZrSize length, const TZrChar *prefix) {
    TZrSize prefixLength = prefix != ZR_NULL ? strlen(prefix) : 0;
    return line != ZR_NULL && prefix != ZR_NULL && length >= prefixLength &&
           memcmp(line, prefix, prefixLength) == 0;
}

static TZrBool lsp_editor_line_can_accept_semicolon(const TZrChar *content,
                                                    TZrSize trimStart,
                                                    TZrSize trimEnd) {
    TZrChar last;
    const TZrChar *line;
    TZrSize length;

    if (content == ZR_NULL || trimStart >= trimEnd) {
        return ZR_FALSE;
    }

    last = content[trimEnd - 1];
    if (last == ';' || last == '{' || last == '}' || last == ':') {
        return ZR_FALSE;
    }

    line = content + trimStart;
    length = trimEnd - trimStart;
    return lsp_editor_line_has_prefix(line, length, "var ") ||
           lsp_editor_line_has_prefix(line, length, "let ") ||
           lsp_editor_line_has_prefix(line, length, "return ") ||
           lsp_editor_line_has_prefix(line, length, "%import");
}

static TZrBool lsp_editor_append_missing_semicolon_action(SZrState *state,
                                                          SZrFileVersion *fileVersion,
                                                          SZrLspRange range,
                                                          SZrArray *result) {
    TZrSize lineStart;
    TZrSize lineEnd;
    TZrSize trimStart;
    TZrSize trimEnd;
    SZrLspRange editRange;
    SZrLspCodeAction *action;

    if (state == ZR_NULL || fileVersion == ZR_NULL || fileVersion->content == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    lineStart = lsp_editor_line_start_offset(fileVersion->content, fileVersion->contentLength, range.start.line);
    lineEnd = lsp_editor_line_end_offset(fileVersion->content, fileVersion->contentLength, range.start.line);
    trimStart = lineStart;
    trimEnd = lineEnd;
    while (trimStart < trimEnd &&
           (fileVersion->content[trimStart] == ' ' || fileVersion->content[trimStart] == '\t')) {
        trimStart++;
    }
    while (trimEnd > trimStart &&
           (fileVersion->content[trimEnd - 1] == ' ' || fileVersion->content[trimEnd - 1] == '\t')) {
        trimEnd--;
    }

    if (!lsp_editor_line_can_accept_semicolon(fileVersion->content, trimStart, trimEnd)) {
        return ZR_TRUE;
    }

    action = (SZrLspCodeAction *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspCodeAction));
    if (action == ZR_NULL) {
        return ZR_FALSE;
    }

    action->title = lsp_editor_create_string(state,
                                             "Insert missing semicolon",
                                             strlen("Insert missing semicolon"));
    action->kind = lsp_editor_create_string(state,
                                            ZR_LSP_CODE_ACTION_KIND_QUICK_FIX,
                                            strlen(ZR_LSP_CODE_ACTION_KIND_QUICK_FIX));
    action->isPreferred = ZR_TRUE;
    ZrCore_Array_Init(state, &action->edits, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);

    editRange.start = lsp_editor_position_from_offset(fileVersion->content, fileVersion->contentLength, trimEnd);
    editRange.end = editRange.start;
    if (!lsp_editor_append_text_edit(state, &action->edits, editRange, ";", 1)) {
        ZrLanguageServer_Lsp_FreeTextEdits(state, &action->edits);
        ZrCore_Memory_RawFree(state->global, action, sizeof(SZrLspCodeAction));
        return ZR_FALSE;
    }

    ZrCore_Array_Push(state, result, &action);
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_GetCodeActions(SZrState *state,
                                            SZrLspContext *context,
                                            SZrString *uri,
                                            SZrLspRange range,
                                            SZrArray *result) {
    SZrFileVersion *fileVersion;
    SZrLspCodeAction *action;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspCodeAction *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    fileVersion = lsp_editor_get_file_version(context, uri);
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL) {
        return ZR_FALSE;
    }

    action = (SZrLspCodeAction *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspCodeAction));
    if (action == ZR_NULL) {
        return ZR_FALSE;
    }
    action->title = lsp_editor_create_string(state, "Organize Zr imports", strlen("Organize Zr imports"));
    action->kind = lsp_editor_create_string(state,
                                            ZR_LSP_CODE_ACTION_KIND_SOURCE_ORGANIZE_IMPORTS,
                                            strlen(ZR_LSP_CODE_ACTION_KIND_SOURCE_ORGANIZE_IMPORTS));
    action->isPreferred = ZR_TRUE;
    ZrCore_Array_Init(state, &action->edits, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);

    if (!lsp_editor_collect_import_organize_edit(state, fileVersion, &action->edits)) {
        ZrLanguageServer_Lsp_FreeTextEdits(state, &action->edits);
        ZrCore_Memory_RawFree(state->global, action, sizeof(SZrLspCodeAction));
        return ZR_FALSE;
    }

    if (action->edits.length == 0) {
        ZrLanguageServer_Lsp_FreeTextEdits(state, &action->edits);
        ZrCore_Memory_RawFree(state->global, action, sizeof(SZrLspCodeAction));
        return lsp_editor_append_missing_semicolon_action(state, fileVersion, range, result);
    }

    ZrCore_Array_Push(state, result, &action);
    return lsp_editor_append_missing_semicolon_action(state, fileVersion, range, result);
}
