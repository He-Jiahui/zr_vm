#include "lsp_code_actions_internal.h"
#include "module/lsp_module_metadata.h"

#include "zr_vm_library/file.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define ZR_LSP_IMPORT_ALIAS_BUFFER_LENGTH 128U

typedef struct SZrLspMissingImportCandidate {
    TZrChar alias[ZR_LSP_IMPORT_ALIAS_BUFFER_LENGTH];
    TZrChar moduleName[ZR_LIBRARY_MAX_PATH_LENGTH];
} SZrLspMissingImportCandidate;

static TZrBool lsp_editor_is_identifier_start(TZrChar ch);
static TZrBool lsp_editor_is_identifier_part(TZrChar ch);

static TZrBool lsp_editor_line_contains_text(const TZrChar *line,
                                             TZrSize length,
                                             const TZrChar *needle) {
    TZrSize needleLength = needle != ZR_NULL ? strlen(needle) : 0;

    if (line == ZR_NULL || needle == ZR_NULL || needleLength == 0 || length < needleLength) {
        return ZR_FALSE;
    }
    for (TZrSize offset = 0; offset + needleLength <= length; offset++) {
        if (memcmp(line + offset, needle, needleLength) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool lsp_editor_line_has_prefix(const TZrChar *line, TZrSize length, const TZrChar *prefix) {
    TZrSize prefixLength = prefix != ZR_NULL ? strlen(prefix) : 0;
    return line != ZR_NULL && prefix != ZR_NULL && length >= prefixLength &&
           memcmp(line, prefix, prefixLength) == 0;
}

static const TZrChar *lsp_editor_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }
    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

static TZrBool lsp_editor_is_identifier_start(TZrChar ch) {
    return (TZrBool)(isalpha((unsigned char)ch) || ch == '_');
}

static TZrBool lsp_editor_is_identifier_part(TZrChar ch) {
    return (TZrBool)(isalnum((unsigned char)ch) || ch == '_');
}

static TZrBool lsp_editor_is_keyword_identifier(const TZrChar *text, TZrSize length) {
    static const TZrChar *keywords[] = {
        "if", "for", "let", "var", "new", "pub", "return", "self", "super", "while"
    };

    for (TZrSize index = 0; index < sizeof(keywords) / sizeof(keywords[0]); index++) {
        TZrSize keywordLength = strlen(keywords[index]);
        if (length == keywordLength && memcmp(text, keywords[index], length) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool lsp_editor_line_declares_import_alias(const TZrChar *line,
                                                     TZrSize length,
                                                     const TZrChar *alias,
                                                     TZrSize aliasLength) {
    TZrSize cursor = 0;

    if (line == ZR_NULL || alias == ZR_NULL || aliasLength == 0) {
        return ZR_FALSE;
    }

    while (cursor < length && (line[cursor] == ' ' || line[cursor] == '\t')) {
        cursor++;
    }
    if (length - cursor < 4 || memcmp(line + cursor, "var ", 4) != 0) {
        return ZR_FALSE;
    }
    cursor += 4;
    while (cursor < length && (line[cursor] == ' ' || line[cursor] == '\t')) {
        cursor++;
    }
    if (length - cursor < aliasLength || memcmp(line + cursor, alias, aliasLength) != 0) {
        return ZR_FALSE;
    }
    cursor += aliasLength;
    if (cursor < length && lsp_editor_is_identifier_part(line[cursor])) {
        return ZR_FALSE;
    }
    return lsp_editor_line_contains_text(line + cursor, length - cursor, "%import(");
}

static TZrSize lsp_editor_skip_non_code_span_on_line(const TZrChar *content,
                                                     TZrSize cursor,
                                                     TZrSize lineEnd) {
    TZrChar quote;
    TZrBool escaped = ZR_FALSE;

    if (content == ZR_NULL || cursor >= lineEnd) {
        return cursor;
    }

    if (content[cursor] == '/' && cursor + 1 < lineEnd && content[cursor + 1] == '/') {
        return lineEnd;
    }
    if (content[cursor] == '/' && cursor + 1 < lineEnd && content[cursor + 1] == '*') {
        cursor += 2;
        while (cursor + 1 < lineEnd) {
            if (content[cursor] == '*' && content[cursor + 1] == '/') {
                return cursor + 2;
            }
            cursor++;
        }
        return lineEnd;
    }
    if (content[cursor] != '"' && content[cursor] != '\'' && content[cursor] != '`') {
        return cursor;
    }

    quote = content[cursor++];
    while (cursor < lineEnd) {
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

static TZrSize lsp_editor_find_line_comment_start(const TZrChar *content,
                                                  TZrSize lineStart,
                                                  TZrSize lineEnd) {
    TZrSize cursor = lineStart;

    while (content != ZR_NULL && cursor + 1 < lineEnd) {
        TZrSize nextCodeOffset = lsp_editor_skip_non_code_span_on_line(content, cursor, lineEnd);
        if (nextCodeOffset != cursor) {
            if (content[cursor] == '/' && cursor + 1 < lineEnd && content[cursor + 1] == '/') {
                return cursor;
            }
            cursor = nextCodeOffset;
            continue;
        }
        cursor++;
    }

    return lineEnd;
}

static TZrBool lsp_editor_has_import_alias(SZrFileVersion *fileVersion,
                                           const TZrChar *alias,
                                           TZrSize aliasLength) {
    const TZrChar *content;
    TZrSize cursor = 0;

    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL ||
        alias == ZR_NULL || aliasLength == 0) {
        return ZR_FALSE;
    }

    content = fileVersion->content;
    while (cursor < fileVersion->contentLength) {
        TZrSize lineStart = cursor;
        TZrSize lineEnd = cursor;
        while (lineEnd < fileVersion->contentLength && content[lineEnd] != '\n') {
            lineEnd++;
        }
        if (lsp_editor_offset_is_code(content, fileVersion->contentLength, lineStart) &&
            lsp_editor_line_declares_import_alias(content + lineStart, lineEnd - lineStart, alias, aliasLength)) {
            return ZR_TRUE;
        }
        cursor = lineEnd < fileVersion->contentLength ? lineEnd + 1 : lineEnd;
    }

    return ZR_FALSE;
}

static TZrBool lsp_editor_module_last_segment_matches(SZrString *moduleName,
                                                      const TZrChar *alias,
                                                      TZrSize aliasLength) {
    const TZrChar *moduleText = lsp_editor_string_text(moduleName);
    const TZrChar *lastDot;
    const TZrChar *segment;

    if (moduleText == ZR_NULL || alias == ZR_NULL || aliasLength == 0) {
        return ZR_FALSE;
    }

    lastDot = strrchr(moduleText, '.');
    segment = lastDot != ZR_NULL ? lastDot + 1 : moduleText;
    return strlen(segment) == aliasLength && memcmp(segment, alias, aliasLength) == 0;
}

static TZrBool lsp_editor_resolve_project_source_file_candidate(SZrLspProjectIndex *projectIndex,
                                                                const TZrChar *alias,
                                                                TZrSize aliasLength,
                                                                TZrChar *moduleName,
                                                                TZrSize moduleNameSize) {
    const TZrChar *sourceRoot;
    TZrChar relativePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar sourcePath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (projectIndex == ZR_NULL || alias == ZR_NULL || aliasLength == 0 ||
        moduleName == ZR_NULL || moduleNameSize == 0 || aliasLength >= moduleNameSize ||
        aliasLength + 4 >= sizeof(relativePath)) {
        return ZR_FALSE;
    }

    sourceRoot = lsp_editor_string_text(projectIndex->sourceRootPath);
    if (sourceRoot == ZR_NULL || sourceRoot[0] == '\0') {
        return ZR_FALSE;
    }

    memcpy(relativePath, alias, aliasLength);
    memcpy(relativePath + aliasLength, ".zr", 4);
    ZrLibrary_File_PathJoin(sourceRoot, relativePath, sourcePath);
    if (sourcePath[0] == '\0' || ZrLibrary_File_Exist(sourcePath) != ZR_LIBRARY_FILE_IS_FILE) {
        return ZR_FALSE;
    }

    memcpy(moduleName, alias, aliasLength);
    moduleName[aliasLength] = '\0';
    return ZR_TRUE;
}

static TZrBool lsp_editor_resolve_project_import_candidate(SZrState *state,
                                                           SZrLspContext *context,
                                                           SZrString *uri,
                                                           const TZrChar *alias,
                                                           TZrSize aliasLength,
                                                           TZrChar *moduleName,
                                                           TZrSize moduleNameSize) {
    SZrLspProjectIndex *projectIndex;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        alias == ZR_NULL || moduleName == ZR_NULL || moduleNameSize == 0) {
        return ZR_FALSE;
    }

    projectIndex = ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
    if (projectIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize fileOffset = 0; fileOffset < projectIndex->files.length; fileOffset++) {
        SZrLspProjectFileRecord **recordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, fileOffset);
        const TZrChar *recordModule;
        if (recordPtr == ZR_NULL || *recordPtr == ZR_NULL ||
            !lsp_editor_module_last_segment_matches((*recordPtr)->moduleName, alias, aliasLength)) {
            continue;
        }
        recordModule = lsp_editor_string_text((*recordPtr)->moduleName);
        if (recordModule == ZR_NULL || strlen(recordModule) >= moduleNameSize) {
            return ZR_FALSE;
        }
        strcpy(moduleName, recordModule);
        return ZR_TRUE;
    }
    if (lsp_editor_resolve_project_source_file_candidate(projectIndex,
                                                         alias,
                                                         aliasLength,
                                                         moduleName,
                                                         moduleNameSize)) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool lsp_editor_resolve_missing_import_candidate(SZrState *state,
                                                           SZrLspContext *context,
                                                           SZrString *uri,
                                                           const TZrChar *alias,
                                                           TZrSize aliasLength,
                                                           SZrLspMissingImportCandidate *candidate) {
    TZrChar nativeModule[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        alias == ZR_NULL || aliasLength == 0 ||
        candidate == ZR_NULL || aliasLength >= sizeof(candidate->alias)) {
        return ZR_FALSE;
    }

    memcpy(candidate->alias, alias, aliasLength);
    candidate->alias[aliasLength] = '\0';
    snprintf(nativeModule, sizeof(nativeModule), "zr.%s", candidate->alias);
    if (ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleDescriptor(state, nativeModule, ZR_NULL) != ZR_NULL) {
        strcpy(candidate->moduleName, nativeModule);
        return ZR_TRUE;
    }

    if (lsp_editor_resolve_project_import_candidate(state,
                                                    context,
                                                    uri,
                                                    alias,
                                                    aliasLength,
                                                    candidate->moduleName,
                                                    sizeof(candidate->moduleName))) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool lsp_editor_requested_range_intersects_line_span(SZrLspRange range,
                                                              TZrSize lineStart,
                                                              TZrSize lineEnd,
                                                              TZrSize spanStart,
                                                              TZrSize spanEnd) {
    TZrSize lineLength;
    TZrSize requestStart;
    TZrSize requestEnd;
    TZrSize absoluteStart;
    TZrSize absoluteEnd;

    if (spanStart >= spanEnd || spanEnd > lineEnd || lineEnd < lineStart) {
        return ZR_FALSE;
    }

    lineLength = lineEnd - lineStart;
    requestStart = range.start.character > 0 ? (TZrSize)range.start.character : 0;
    if (requestStart > lineLength) {
        requestStart = lineLength;
    }
    if (range.end.line > range.start.line) {
        requestEnd = lineLength;
    } else {
        requestEnd = range.end.character > 0 ? (TZrSize)range.end.character : requestStart;
        if (requestEnd > lineLength) {
            requestEnd = lineLength;
        }
    }
    if (requestEnd < requestStart) {
        requestEnd = requestStart;
    }

    absoluteStart = lineStart + requestStart;
    absoluteEnd = lineStart + requestEnd;
    if (absoluteStart == absoluteEnd) {
        return absoluteStart >= spanStart && absoluteStart <= spanEnd;
    }
    return absoluteStart < spanEnd && absoluteEnd > spanStart;
}

static TZrBool lsp_editor_find_missing_import_candidate_on_line(SZrState *state,
                                                                SZrLspContext *context,
                                                                SZrString *uri,
                                                                SZrFileVersion *fileVersion,
                                                                SZrLspRange range,
                                                                SZrLspMissingImportCandidate *candidate) {
    TZrSize lineStart;
    TZrSize lineEnd;
    const TZrChar *content;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || fileVersion == ZR_NULL ||
        fileVersion->content == ZR_NULL || candidate == ZR_NULL) {
        return ZR_FALSE;
    }

    content = fileVersion->content;
    lineStart = lsp_editor_line_start_offset(content, fileVersion->contentLength, range.start.line);
    lineEnd = lsp_editor_line_end_offset(content, fileVersion->contentLength, range.start.line);
    for (TZrSize cursor = lineStart; cursor + 1 < lineEnd;) {
        TZrSize aliasStart;
        TZrSize aliasEnd;
        TZrSize memberEnd;
        TZrSize nextCodeOffset = lsp_editor_skip_non_code_span_on_line(content, cursor, lineEnd);
        if (nextCodeOffset != cursor) {
            cursor = nextCodeOffset;
            continue;
        }
        if (!lsp_editor_is_identifier_start(content[cursor]) ||
            (cursor > lineStart && lsp_editor_is_identifier_part(content[cursor - 1]))) {
            cursor++;
            continue;
        }

        aliasStart = cursor;
        aliasEnd = cursor + 1;
        while (aliasEnd < lineEnd && lsp_editor_is_identifier_part(content[aliasEnd])) {
            aliasEnd++;
        }
        if (!lsp_editor_offset_is_code(content, fileVersion->contentLength, aliasStart)) {
            cursor = aliasEnd + 1;
            continue;
        }
        if (aliasEnd >= lineEnd || content[aliasEnd] != '.' ||
            lsp_editor_is_keyword_identifier(content + aliasStart, aliasEnd - aliasStart) ||
            lsp_editor_has_import_alias(fileVersion, content + aliasStart, aliasEnd - aliasStart)) {
            cursor = aliasEnd + 1;
            continue;
        }
        memberEnd = aliasEnd + 1;
        while (memberEnd < lineEnd && lsp_editor_is_identifier_part(content[memberEnd])) {
            memberEnd++;
        }
        if (!lsp_editor_requested_range_intersects_line_span(range,
                                                            lineStart,
                                                            lineEnd,
                                                            aliasStart,
                                                            memberEnd)) {
            cursor = memberEnd;
            continue;
        }
        if (lsp_editor_resolve_missing_import_candidate(state,
                                                        context,
                                                        uri,
                                                        content + aliasStart,
                                                        aliasEnd - aliasStart,
                                                        candidate)) {
            return ZR_TRUE;
        }
        cursor = memberEnd;
    }

    return ZR_FALSE;
}

static TZrSize lsp_editor_missing_import_insert_offset(SZrFileVersion *fileVersion) {
    const TZrChar *content;
    TZrSize cursor = 0;
    TZrSize insertOffset = 0;

    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL) {
        return 0;
    }

    content = fileVersion->content;
    while (cursor < fileVersion->contentLength) {
        TZrSize lineStart = cursor;
        TZrSize lineEnd = cursor;
        TZrSize trimStart;
        TZrSize trimEnd;
        TZrBool isBlank;
        TZrBool isModuleDeclaration;
        TZrBool isImport;

        while (lineEnd < fileVersion->contentLength && content[lineEnd] != '\n') {
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
        isModuleDeclaration = trimEnd - trimStart >= 7 && memcmp(content + trimStart, "module ", 7) == 0;
        isImport = lsp_code_action_trimmed_line_is_import_declaration(content + trimStart, trimEnd - trimStart);
        if (!isBlank && !isModuleDeclaration && !isImport) {
            break;
        }
        insertOffset = lineEnd < fileVersion->contentLength && content[lineEnd] == '\n' ? lineEnd + 1 : lineEnd;
        cursor = insertOffset;
    }

    return insertOffset;
}

static TZrBool lsp_editor_append_missing_import_action(SZrState *state,
                                                       SZrLspContext *context,
                                                       SZrString *uri,
                                                       SZrFileVersion *fileVersion,
                                                       SZrLspRange range,
                                                       SZrArray *result) {
    SZrLspMissingImportCandidate candidate;
    TZrSize titleLength;
    TZrSize editTextLength;
    TZrChar *title;
    TZrChar *editText;
    TZrSize insertOffset;
    SZrLspRange editRange;
    SZrLspCodeAction *action;

    if (!lsp_editor_find_missing_import_candidate_on_line(state, context, uri, fileVersion, range, &candidate)) {
        return ZR_TRUE;
    }

    titleLength = strlen("Import ") + strlen(candidate.moduleName) + strlen(" as ") + strlen(candidate.alias);
    editTextLength = strlen("var ") + strlen(candidate.alias) + strlen(" = %import(\"") +
                     strlen(candidate.moduleName) + strlen("\");\n");
    title = (TZrChar *)malloc(titleLength + 1);
    editText = (TZrChar *)malloc(editTextLength + 1);
    if (title == ZR_NULL || editText == ZR_NULL) {
        free(title);
        free(editText);
        return ZR_FALSE;
    }
    snprintf(title, titleLength + 1, "Import %s as %s", candidate.moduleName, candidate.alias);
    snprintf(editText, editTextLength + 1, "var %s = %%import(\"%s\");\n", candidate.alias, candidate.moduleName);

    action = (SZrLspCodeAction *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspCodeAction));
    if (action == ZR_NULL) {
        free(title);
        free(editText);
        return ZR_FALSE;
    }
    action->title = lsp_editor_create_string(state, title, strlen(title));
    action->kind = lsp_editor_create_string(state,
                                            ZR_LSP_CODE_ACTION_KIND_QUICK_FIX,
                                            strlen(ZR_LSP_CODE_ACTION_KIND_QUICK_FIX));
    action->isPreferred = ZR_FALSE;
    ZrCore_Array_Init(state, &action->edits, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);

    insertOffset = lsp_editor_missing_import_insert_offset(fileVersion);
    editRange.start = lsp_editor_position_from_offset(fileVersion->content, fileVersion->contentLength, insertOffset);
    editRange.end = editRange.start;
    if (!lsp_editor_append_text_edit(state, &action->edits, editRange, editText, strlen(editText))) {
        free(title);
        free(editText);
        ZrLanguageServer_Lsp_FreeTextEdits(state, &action->edits);
        ZrCore_Memory_RawFree(state->global, action, sizeof(SZrLspCodeAction));
        return ZR_FALSE;
    }
    free(title);
    free(editText);

    ZrCore_Array_Push(state, result, &action);
    return ZR_TRUE;
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
    trimEnd = lsp_editor_find_line_comment_start(fileVersion->content, lineStart, lineEnd);
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

static TZrBool lsp_editor_append_unused_import_cleanup_action(SZrState *state,
                                                              SZrFileVersion *fileVersion,
                                                              SZrArray *result) {
    SZrLspCodeAction *action;

    if (state == ZR_NULL || fileVersion == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    action = (SZrLspCodeAction *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspCodeAction));
    if (action == ZR_NULL) {
        return ZR_FALSE;
    }

    action->title = lsp_editor_create_string(state,
                                             "Remove unused Zr imports",
                                             strlen("Remove unused Zr imports"));
    action->kind = lsp_editor_create_string(state,
                                            ZR_LSP_CODE_ACTION_KIND_SOURCE_REMOVE_UNUSED,
                                            strlen(ZR_LSP_CODE_ACTION_KIND_SOURCE_REMOVE_UNUSED));
    action->isPreferred = ZR_FALSE;
    ZrCore_Array_Init(state, &action->edits, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);

    if (!lsp_code_action_collect_unused_import_cleanup_edit(state, fileVersion, &action->edits)) {
        ZrLanguageServer_Lsp_FreeTextEdits(state, &action->edits);
        ZrCore_Memory_RawFree(state->global, action, sizeof(SZrLspCodeAction));
        return ZR_FALSE;
    }
    if (action->edits.length == 0) {
        ZrLanguageServer_Lsp_FreeTextEdits(state, &action->edits);
        ZrCore_Memory_RawFree(state->global, action, sizeof(SZrLspCodeAction));
        return ZR_TRUE;
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

    if (!lsp_code_action_collect_import_organize_edit(state, fileVersion, &action->edits)) {
        ZrLanguageServer_Lsp_FreeTextEdits(state, &action->edits);
        ZrCore_Memory_RawFree(state->global, action, sizeof(SZrLspCodeAction));
        return ZR_FALSE;
    }

    if (action->edits.length == 0) {
        ZrLanguageServer_Lsp_FreeTextEdits(state, &action->edits);
        ZrCore_Memory_RawFree(state->global, action, sizeof(SZrLspCodeAction));
        return lsp_editor_append_unused_import_cleanup_action(state, fileVersion, result) &&
               lsp_editor_append_missing_import_action(state, context, uri, fileVersion, range, result) &&
               lsp_editor_append_missing_semicolon_action(state, fileVersion, range, result);
    }

    ZrCore_Array_Push(state, result, &action);
    return lsp_editor_append_unused_import_cleanup_action(state, fileVersion, result) &&
           lsp_editor_append_missing_import_action(state, context, uri, fileVersion, range, result) &&
           lsp_editor_append_missing_semicolon_action(state, fileVersion, range, result);
}
