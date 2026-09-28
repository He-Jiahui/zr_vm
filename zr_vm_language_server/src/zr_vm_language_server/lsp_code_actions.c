#include "lsp_code_actions_internal.h"
#include "lsp_property_code_actions.h"
#include "module/lsp_module_metadata.h"

#include "zr_vm_library/file.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* 自动补导入先保存短别名，再从原生模块或项目索引解析实际模块名。 */
#define ZR_LSP_IMPORT_ALIAS_BUFFER_LENGTH 128U

/* 快速修复先保留源码别名，再将 native 或项目模块名写入同一栈对象。 */
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

/* 借出 VM 字符串的原生文本供同步动作构造；结果不转移所有权。 */
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

/* 为整文件导入去重扫描识别 let/var 形式；这里只判断单行形状，不识别词法作用域。 */
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
    if (length - cursor < 4 ||
        (memcmp(line + cursor, "let ", 4) != 0 && memcmp(line + cursor, "var ", 4) != 0)) {
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
    return lsp_editor_line_contains_text(line + cursor, length - cursor, "import(");
}

/* 候选搜索跳过行内注释及引号内容；跨行注释再由 offset_is_code 复核。 */
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

/* BUG: 全文件扫描不区分局部作用域；别处函数的局部同名 import 会压掉当前缺失导入建议。 */
static TZrBool lsp_editor_has_import_alias(const TZrChar *content,
                                           TZrSize contentLength,
                                           const TZrChar *alias,
                                           TZrSize aliasLength) {
    TZrSize cursor = 0;

    if (content == ZR_NULL || alias == ZR_NULL || aliasLength == 0) {
        return ZR_FALSE;
    }

    while (cursor < contentLength) {
        TZrSize lineStart = cursor;
        TZrSize lineEnd = cursor;
        while (lineEnd < contentLength && content[lineEnd] != '\n') {
            lineEnd++;
        }
        if (lsp_editor_offset_is_code(content, contentLength, lineStart) &&
            lsp_editor_line_declares_import_alias(content + lineStart, lineEnd - lineStart, alias, aliasLength)) {
            return ZR_TRUE;
        }
        cursor = lineEnd < contentLength ? lineEnd + 1 : lineEnd;
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

/* 索引无匹配时退回项目 sourceRoot 下的同名 .zr 文件，供刚创建的文件立即修复。 */
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

/* 项目索引先给出完整模块名，随后才检查 sourceRoot 中尚未入索引的文件。
 * TODO: 多个项目模块末段同名时当前取索引中的首个；需用双路径项目夹具核对是否应消歧。
 */
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

/* native zr.<alias> 优先于项目候选；返回的模块名写入请求栈上的固定缓冲区。 */
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

/* 把请求范围限制到当前行的 alias.member；stdio 与 WASM 均传入内部 UTF-16 列。
 * BUG: 下方将 UTF-16 列直接加在 UTF-8 字节行首，与别名的字节偏移比较；
 * 同行非 ASCII 文本位于别名前时，两条入口都可能漏掉缺失导入动作。
 */
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

/* 仅扫描请求起始行的可执行 alias.member，并用请求范围筛出一个可解析候选。 */
static TZrBool lsp_editor_find_missing_import_candidate_on_line(SZrState *state,
                                                                SZrLspContext *context,
                                                                SZrString *uri,
                                                                const TZrChar *content,
                                                                TZrSize contentLength,
                                                                SZrLspRange range,
                                                                SZrLspMissingImportCandidate *candidate) {
    TZrSize lineStart;
    TZrSize lineEnd;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || content == ZR_NULL || candidate == ZR_NULL) {
        return ZR_FALSE;
    }

    lineStart = lsp_editor_line_start_offset(content, contentLength, range.start.line);
    lineEnd = lsp_editor_line_end_offset(content, contentLength, range.start.line);
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
        if (!lsp_editor_offset_is_code(content, contentLength, aliasStart)) {
            cursor = aliasEnd + 1;
            continue;
        }
        if (aliasEnd >= lineEnd || content[aliasEnd] != '.' ||
            lsp_editor_is_keyword_identifier(content + aliasStart, aliasEnd - aliasStart) ||
            lsp_editor_has_import_alias(content, contentLength, content + aliasStart, aliasEnd - aliasStart)) {
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

/* 自动补导入插在 module/顶部导入段之后，保持正文首行不被拆开。 */
static TZrSize lsp_editor_missing_import_insert_offset(const TZrChar *content, TZrSize contentLength) {
    TZrSize cursor = 0;
    TZrSize insertOffset = 0;

    if (content == ZR_NULL) {
        return 0;
    }

    while (cursor < contentLength) {
        TZrSize lineStart = cursor;
        TZrSize lineEnd = cursor;
        TZrSize trimStart;
        TZrSize trimEnd;
        TZrBool isBlank;
        TZrBool isModuleDeclaration;
        TZrBool isImport;

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
        isModuleDeclaration = trimEnd - trimStart >= 7 && memcmp(content + trimStart, "module ", 7) == 0;
        isImport = lsp_code_action_trimmed_line_is_import_declaration(content + trimStart, trimEnd - trimStart);
        if (!isBlank && !isModuleDeclaration && !isImport) {
            break;
        }
        insertOffset = lineEnd < contentLength && content[lineEnd] == '\n' ? lineEnd + 1 : lineEnd;
        cursor = insertOffset;
    }

    return insertOffset;
}

/* 解析成功才构造 quickfix；标题、插入文本只在此调用期间由 malloc 持有。 */
static TZrBool lsp_editor_append_missing_import_action(SZrState *state,
                                                       SZrLspContext *context,
                                                       SZrString *uri,
                                                       const TZrChar *content,
                                                       TZrSize contentLength,
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

    if (!lsp_editor_find_missing_import_candidate_on_line(state,
                                                          context,
                                                          uri,
                                                          content,
                                                          contentLength,
                                                          range,
                                                          &candidate)) {
        return ZR_TRUE;
    }

    titleLength = strlen("Import ") + strlen(candidate.moduleName) + strlen(" as ") + strlen(candidate.alias);
    editTextLength = strlen("let ") + strlen(candidate.alias) + strlen(" = import(\"") +
                     strlen(candidate.moduleName) + strlen("\");\n");
    title = (TZrChar *)malloc(titleLength + 1);
    editText = (TZrChar *)malloc(editTextLength + 1);
    if (title == ZR_NULL || editText == ZR_NULL) {
        free(title);
        free(editText);
        return ZR_FALSE;
    }
    snprintf(title, titleLength + 1, "Import %s as %s", candidate.moduleName, candidate.alias);
    snprintf(editText, editTextLength + 1, "let %s = import(\"%s\");\n", candidate.alias, candidate.moduleName);

    action = (SZrLspCodeAction *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspCodeAction));
    if (action == ZR_NULL) {
        free(title);
        free(editText);
        return ZR_FALSE;
    }
    /* BUG: 字符串创建失败未检查；后续编辑若成功，客户端会收到空标题或缺失 kind 的动作。 */
    action->title = lsp_editor_create_string(state, title, strlen(title));
    action->kind = lsp_editor_create_string(state,
                                            ZR_LSP_CODE_ACTION_KIND_QUICK_FIX,
                                            strlen(ZR_LSP_CODE_ACTION_KIND_QUICK_FIX));
    action->isPreferred = ZR_FALSE;
    ZrCore_Array_Init(state, &action->edits, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);

    insertOffset = lsp_editor_missing_import_insert_offset(content, contentLength);
    editRange.start = lsp_editor_position_from_offset(content, contentLength, insertOffset);
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

static TZrInt32 lsp_code_action_compare_position(
        SZrLspPosition left,
        SZrLspPosition right) {
    if (left.line != right.line) {
        return left.line < right.line ? -1 : 1;
    }
    if (left.character == right.character) {
        return 0;
    }
    return left.character < right.character ? -1 : 1;
}

static TZrBool lsp_code_action_ranges_intersect(
        SZrLspRange left,
        SZrLspRange right) {
    return lsp_code_action_compare_position(left.start, right.end) <= 0 &&
           lsp_code_action_compare_position(right.start, left.end) <= 0;
}

/* 诊断修复接受精确重叠，也接受与诊断同行的请求以兼容编辑器光标触发。 */
static TZrBool lsp_code_action_range_selects(
        SZrLspRange requested,
        SZrLspRange candidate) {
    return lsp_code_action_ranges_intersect(requested, candidate) ||
           (requested.start.line <= candidate.end.line &&
            candidate.start.line <= requested.end.line);
}

/* 从当前诊断重新挑出机器可应用修复；不信任客户端随请求携带的旧诊断副本。
 * 中途失败保留已追加动作，由 GetCodeActions 调用方统一释放。
 */
static TZrBool lsp_editor_append_diagnostic_fix_actions(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrLspRange requestedRange,
        SZrArray *result) {
    SZrArray diagnostics = {0};
    TZrBool ok = ZR_TRUE;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Array_Init(
            state,
            &diagnostics,
            sizeof(SZrLspDiagnostic *),
            ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(
                state, context, uri, &diagnostics)) {
        ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
        return ZR_FALSE;
    }

    for (TZrSize diagnosticIndex = 0U;
         ok && diagnosticIndex < diagnostics.length;
         diagnosticIndex++) {
        SZrLspDiagnostic **diagnosticPtr =
                (SZrLspDiagnostic **)ZrCore_Array_Get(
                        &diagnostics, diagnosticIndex);
        if (diagnosticPtr == ZR_NULL || *diagnosticPtr == ZR_NULL ||
            !(*diagnosticPtr)->fixes.isValid) {
            continue;
        }

        for (TZrSize fixIndex = 0U;
             ok && fixIndex < (*diagnosticPtr)->fixes.length;
             fixIndex++) {
            const SZrLspDiagnosticFix *fix =
                    (const SZrLspDiagnosticFix *)ZrCore_Array_Get(
                            &(*diagnosticPtr)->fixes, fixIndex);
            const TZrChar *title;
            const TZrChar *editText;
            SZrLspCodeAction *action;

            if (fix == ZR_NULL ||
                fix->applicability !=
                        ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE ||
                (!lsp_code_action_range_selects(
                         requestedRange, (*diagnosticPtr)->range) &&
                 !lsp_code_action_range_selects(
                         requestedRange, fix->editRange))) {
                continue;
            }
            title = lsp_editor_string_text(fix->title);
            editText = lsp_editor_string_text(fix->editText);
            if (title == ZR_NULL || editText == ZR_NULL) {
                ok = ZR_FALSE;
                break;
            }

            action = (SZrLspCodeAction *)ZrCore_Memory_RawMalloc(
                    state->global, sizeof(SZrLspCodeAction));
            if (action == ZR_NULL) {
                ok = ZR_FALSE;
                break;
            }
            action->title = lsp_editor_create_string(
                    state, title, strlen(title));
            action->kind = lsp_editor_create_string(
                    state,
                    ZR_LSP_CODE_ACTION_KIND_QUICK_FIX,
                    strlen(ZR_LSP_CODE_ACTION_KIND_QUICK_FIX));
            action->isPreferred = ZR_TRUE;
            ZrCore_Array_Init(
                    state,
                    &action->edits,
                    sizeof(SZrLspTextEdit *),
                    ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
            if (action->title == ZR_NULL || action->kind == ZR_NULL ||
                !lsp_editor_append_text_edit(
                        state,
                        &action->edits,
                        fix->editRange,
                        editText,
                        strlen(editText))) {
                ZrLanguageServer_Lsp_FreeTextEdits(
                        state, &action->edits);
                ZrCore_Memory_RawFree(
                        state->global,
                        action,
                        sizeof(SZrLspCodeAction));
                ok = ZR_FALSE;
                break;
            }
            ZrCore_Array_Push(state, result, &action);
        }
    }

    ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
    return ok;
}

/* 只有实际删除编辑才暴露 source.removeUnused；无差异时释放空动作。 */
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

    /* BUG: 标题或 kind 创建失败仍可能追加含编辑的动作；序列化会产生空标题或缺失 kind。 */
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

/* 汇合 organize/removeUnused、缺失导入、当前诊断修复与 property 重构；stdio/worker 再按 context.only 筛选。
 * 调用方必须 FreeCodeActions；任一阶段失败返回假，已有部分结果仍由调用方释放。
 */
TZrBool ZrLanguageServer_Lsp_GetCodeActions(SZrState *state,
                                            SZrLspContext *context,
                                            SZrString *uri,
                                            SZrLspRange range,
                                            SZrArray *result) {
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    const TZrChar *content;
    TZrSize contentLength;
    SZrLspCodeAction *action;
    TZrBool ok;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspCodeAction *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    fileVersion = lsp_editor_get_file_version(context, uri);
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        return ZR_FALSE;
    }
    content = snapshot.content;
    contentLength = snapshot.contentLength;

    action = (SZrLspCodeAction *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspCodeAction));
    if (action == ZR_NULL) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }
    /* BUG: organize 标题或 kind 创建失败未检查；若编辑存在仍可向客户端返回无效动作。 */
    action->title = lsp_editor_create_string(state, "Organize Zr imports", strlen("Organize Zr imports"));
    action->kind = lsp_editor_create_string(state,
                                            ZR_LSP_CODE_ACTION_KIND_SOURCE_ORGANIZE_IMPORTS,
                                            strlen(ZR_LSP_CODE_ACTION_KIND_SOURCE_ORGANIZE_IMPORTS));
    action->isPreferred = ZR_TRUE;
    ZrCore_Array_Init(state, &action->edits, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);

    if (!lsp_code_action_collect_import_organize_edit(state, fileVersion, &action->edits)) {
        ZrLanguageServer_Lsp_FreeTextEdits(state, &action->edits);
        ZrCore_Memory_RawFree(state->global, action, sizeof(SZrLspCodeAction));
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }

    /* source.organizeImports 不产生空编辑动作，但其余四类动作仍需继续收集。 */
    if (action->edits.length == 0) {
        ZrLanguageServer_Lsp_FreeTextEdits(state, &action->edits);
        ZrCore_Memory_RawFree(state->global, action, sizeof(SZrLspCodeAction));
        ok = lsp_editor_append_unused_import_cleanup_action(state, fileVersion, result) &&
             lsp_editor_append_missing_import_action(state, context, uri, content, contentLength, range, result) &&
             lsp_editor_append_diagnostic_fix_actions(
                     state, context, uri, range, result) &&
             ZrLanguageServer_LspPropertyCodeActions_Append(
                     state,
                     context,
                     uri,
                     content,
                     contentLength,
                     range,
                     result);
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ok;
    }

    ZrCore_Array_Push(state, result, &action);
    ok = lsp_editor_append_unused_import_cleanup_action(state, fileVersion, result) &&
         lsp_editor_append_missing_import_action(state, context, uri, content, contentLength, range, result) &&
         lsp_editor_append_diagnostic_fix_actions(
                 state, context, uri, range, result) &&
         ZrLanguageServer_LspPropertyCodeActions_Append(
                 state,
                 context,
                 uri,
                 content,
                 contentLength,
                 range,
                 result);
    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    return ok;
}
