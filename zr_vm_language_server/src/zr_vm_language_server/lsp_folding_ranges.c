#include "lsp_editor_features_internal.h"

#include <ctype.h>
#include <string.h>

/* 行扫描只保存当前内容快照的字节边界，供 imports、注释和区域标记的匹配器复用。 */
typedef struct SZrLspLineTrim {
    TZrSize start;
    TZrSize end;
} SZrLspLineTrim;

/* 两类折叠扫描共用起点形状；值应按 LSP 列表示，当前扫描路径仍可能传入字节列。 */
typedef struct SZrBraceStart {
    TZrInt32 line;
    TZrInt32 character;
} SZrBraceStart;

/* 结构字符回调借用同一次文档快照，并在有界栈中配对源码花括号。 */
typedef struct SZrStructuralFoldingScanData {
    SZrState *state;
    SZrArray *result;
    const TZrChar *content;
    TZrSize contentLength;
    SZrBraceStart stack[ZR_LSP_AST_RECURSION_MAX_DEPTH];
    TZrSize stackLength;
} SZrStructuralFoldingScanData;

/* 只有跨行段才交给 stdio/WASM 发布；结果数组的 range 元素由 FreeFoldingRanges 归还。 */
static TZrBool lsp_editor_append_folding_range(SZrState *state,
                                               SZrArray *result,
                                               TZrInt32 startLine,
                                               TZrInt32 startCharacter,
                                               TZrInt32 endLine,
                                               TZrInt32 endCharacter,
                                               const TZrChar *kind) {
    SZrLspFoldingRange *range;
    const TZrChar *kindText = kind != ZR_NULL ? kind : ZR_LSP_FOLDING_RANGE_KIND_REGION;

    if (endLine <= startLine) {
        return ZR_TRUE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspFoldingRange *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    range = (SZrLspFoldingRange *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspFoldingRange));
    if (range == ZR_NULL) {
        return ZR_FALSE;
    }

    range->startLine = startLine;
    range->startCharacter = startCharacter;
    range->endLine = endLine;
    range->endCharacter = endCharacter;
    range->kind = lsp_editor_create_string(state, kindText, strlen(kindText));
    ZrCore_Array_Push(state, result, &range);
    return ZR_TRUE;
}

/* imports 和注释行匹配以去空白后的同一快照切片为准，不改变原文偏移。 */
static TZrBool lsp_editor_trim_line(const TZrChar *content,
                                    TZrSize lineStart,
                                    TZrSize lineEnd,
                                    SZrLspLineTrim *trim) {
    TZrSize trimStart = lineStart;
    TZrSize trimEnd = lineEnd;

    if (content == ZR_NULL || lineStart > lineEnd || trim == ZR_NULL) {
        return ZR_FALSE;
    }

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

    trim->start = trimStart;
    trim->end = trimEnd;
    return ZR_TRUE;
}

/* 行分类器共同使用字节前缀判断；调用方负责确保该前缀代表所需语法。 */
static TZrBool lsp_editor_line_starts_with(const TZrChar *content,
                                           const SZrLspLineTrim *trim,
                                           const TZrChar *prefix,
                                           TZrSize prefixLength) {
    return content != ZR_NULL &&
           trim != ZR_NULL &&
           prefix != ZR_NULL &&
           trim->end - trim->start >= prefixLength &&
           memcmp(content + trim->start, prefix, prefixLength) == 0;
}

/* 将相邻行的分类策略交给调用方，扫描和结果所有权保持一致。 */
typedef TZrBool (*TZrLspLineRunMatcher)(const TZrChar *content, const SZrLspLineTrim *trim);

/* 只识别连续的 `let/var 名称 = import(` 行，为 imports 折叠提供轻量文本入口。 */
static TZrBool lsp_editor_line_is_import_declaration(const TZrChar *content, const SZrLspLineTrim *trim) {
    /* BUG: 只按行文本识别，跨行模板或块注释里的伪 import 也能形成 imports 折叠。 */
    TZrSize cursor;
    static const TZrChar importToken[] = "import";

    if (content == ZR_NULL || trim == ZR_NULL) {
        return ZR_FALSE;
    }
    cursor = trim->start;
    if (lsp_editor_line_starts_with(content, trim, "let", 3) ||
        lsp_editor_line_starts_with(content, trim, "var", 3)) {
        cursor += 3;
    } else {
        return ZR_FALSE;
    }
    if (cursor >= trim->end || (content[cursor] != ' ' && content[cursor] != '\t')) {
        return ZR_FALSE;
    }
    while (cursor < trim->end && (content[cursor] == ' ' || content[cursor] == '\t')) {
        cursor++;
    }
    if (cursor >= trim->end ||
        !(isalpha((unsigned char)content[cursor]) || content[cursor] == '_')) {
        return ZR_FALSE;
    }
    while (cursor < trim->end &&
           (isalnum((unsigned char)content[cursor]) || content[cursor] == '_')) {
        cursor++;
    }
    while (cursor < trim->end && (content[cursor] == ' ' || content[cursor] == '\t')) {
        cursor++;
    }
    if (cursor >= trim->end || content[cursor++] != '=') {
        return ZR_FALSE;
    }
    while (cursor < trim->end && (content[cursor] == ' ' || content[cursor] == '\t')) {
        cursor++;
    }

    return cursor + sizeof(importToken) - 1 < trim->end &&
           memcmp(content + cursor, importToken, sizeof(importToken) - 1) == 0 &&
           content[cursor + sizeof(importToken) - 1] == '(';
}

/* 连续的整行 // 注释形成一个 comment 折叠段；行尾注释不属于这个扫描器。 */
static TZrBool lsp_editor_line_is_comment(const TZrChar *content, const SZrLspLineTrim *trim) {
    /* BUG: 行首 // 若处在跨行模板或块注释内，仍被归类为注释并生成伪折叠段。 */
    return lsp_editor_line_starts_with(content, trim, "//", 2);
}

/* 扫描连续匹配行，供 import 和 comment 两次调用；空行会结束当前折叠段。 */
static TZrBool lsp_editor_append_line_run_folding_ranges(SZrState *state,
                                                         SZrArray *result,
                                                         const TZrChar *content,
                                                         TZrSize contentLength,
                                                         TZrLspLineRunMatcher matcher,
                                                         const TZrChar *kind) {
    TZrSize cursor = 0;
    TZrInt32 line = 0;
    TZrBool inBlock = ZR_FALSE;
    TZrInt32 blockStartLine = 0;
    TZrInt32 blockStartCharacter = 0;
    TZrInt32 lastLine = 0;
    TZrInt32 lastEndCharacter = 0;

    while (cursor <= contentLength) {
        TZrSize lineStart = cursor;
        TZrSize lineEnd = cursor;
        TZrBool hasLine = cursor < contentLength;
        SZrLspLineTrim trim = {0, 0};
        TZrBool matches;

        while (lineEnd < contentLength && content[lineEnd] != '\n') {
            lineEnd++;
        }

        /* BUG: trim 的字节偏移直接成为 LSP character；非 ASCII 前缀或内容使默认 UTF-16 客户端得到错误列。 */
        matches = hasLine && matcher != ZR_NULL &&
                  lsp_editor_trim_line(content, lineStart, lineEnd, &trim) &&
                  matcher(content, &trim);
        if (matches) {
            if (!inBlock) {
                inBlock = ZR_TRUE;
                blockStartLine = line;
                blockStartCharacter = (TZrInt32)(trim.start - lineStart);
            }
            lastLine = line;
            lastEndCharacter = (TZrInt32)(trim.end - lineStart);
        } else if (inBlock) {
            if (!lsp_editor_append_folding_range(state,
                                                 result,
                                                 blockStartLine,
                                                 blockStartCharacter,
                                                 lastLine,
                                                 lastEndCharacter,
                                                 kind)) {
                return ZR_FALSE;
            }
            inBlock = ZR_FALSE;
        }

        if (lineEnd >= contentLength) {
            break;
        }
        cursor = lineEnd + 1;
        line++;
    }

    if (inBlock &&
        !lsp_editor_append_folding_range(state,
                                         result,
                                         blockStartLine,
                                         blockStartCharacter,
                                         lastLine,
                                         lastEndCharacter,
                                         kind)) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* 显式区域标记允许三种历史写法，嵌套关系由下游 marker 扫描保存。 */
static TZrBool lsp_editor_is_region_start_marker(const TZrChar *content, const SZrLspLineTrim *trim) {
    return lsp_editor_line_starts_with(content, trim, "// region", 9) ||
           lsp_editor_line_starts_with(content, trim, "//region", 8) ||
           lsp_editor_line_starts_with(content, trim, "//#region", 9);
}

/* 与 start marker 成对识别；未匹配的结束标记由扫描器忽略。 */
static TZrBool lsp_editor_is_region_end_marker(const TZrChar *content, const SZrLspLineTrim *trim) {
    return lsp_editor_line_starts_with(content, trim, "// endregion", 12) ||
           lsp_editor_line_starts_with(content, trim, "//endregion", 11) ||
           lsp_editor_line_starts_with(content, trim, "//#endregion", 12);
}

/* 区域标记可嵌套且与源码花括号独立，返回的折叠段同样归属结果数组。 */
static TZrBool lsp_editor_append_marker_folding_ranges(SZrState *state,
                                                       SZrArray *result,
                                                       const TZrChar *content,
                                                       TZrSize contentLength) {
    SZrBraceStart stack[ZR_LSP_AST_RECURSION_MAX_DEPTH];
    TZrSize stackLength = 0;
    TZrSize cursor = 0;
    TZrInt32 line = 0;

    while (cursor <= contentLength) {
        TZrSize lineStart = cursor;
        TZrSize lineEnd = cursor;
        TZrBool hasLine = cursor < contentLength;
        SZrLspLineTrim trim = {0, 0};

        while (lineEnd < contentLength && content[lineEnd] != '\n') {
            lineEnd++;
        }

        /* BUG: 此处不继承词法状态，模板或块注释中的 //#region 也会被配成真实区域。 */
        if (hasLine && lsp_editor_trim_line(content, lineStart, lineEnd, &trim)) {
            /* BUG: 第 33 层 start 被丢弃，但其 end 仍弹出第 32 层，随后折叠范围错配。 */
            if (lsp_editor_is_region_start_marker(content, &trim) &&
                stackLength < ZR_LSP_AST_RECURSION_MAX_DEPTH) {
                stack[stackLength].line = line;
                stack[stackLength].character = (TZrInt32)(trim.start - lineStart);
                stackLength++;
            } else if (lsp_editor_is_region_end_marker(content, &trim) && stackLength > 0) {
                SZrBraceStart start = stack[--stackLength];
                if (!lsp_editor_append_folding_range(state,
                                                     result,
                                                     start.line,
                                                     start.character,
                                                     line,
                                                     (TZrInt32)(trim.end - lineStart),
                                                     ZR_LSP_FOLDING_RANGE_KIND_REGION)) {
                    return ZR_FALSE;
                }
            }
        }

        if (lineEnd >= contentLength) {
            break;
        }
        cursor = lineEnd + 1;
        line++;
    }

    return ZR_TRUE;
}

/* 共用词法扫描器只交付代码态花括号；回调将配对结果追加为结构折叠段。 */
static TZrBool lsp_editor_append_structural_folding_ranges_callback(TZrChar value,
                                                                    TZrSize offset,
                                                                    void *userData) {
    SZrStructuralFoldingScanData *data = (SZrStructuralFoldingScanData *)userData;
    SZrLspPosition position;

    if (data == ZR_NULL || data->state == ZR_NULL || data->result == ZR_NULL || data->content == ZR_NULL) {
        return ZR_FALSE;
    }

    /* BUG: position_from_offset 按 UTF-8 字节递增列号，WASM 与默认 UTF-16 的 stdio 会发布错误列。 */
    position = lsp_editor_position_from_offset(data->content, data->contentLength, offset);
    if (value == '{') {
        /* BUG: 超出第 32 层的 { 被忽略，其 } 却会弹出先前起点，破坏深嵌套折叠配对。 */
        if (data->stackLength < ZR_LSP_AST_RECURSION_MAX_DEPTH) {
            data->stack[data->stackLength].line = position.line;
            data->stack[data->stackLength].character = position.character;
            data->stackLength++;
        }
        return ZR_TRUE;
    }

    if (value == '}' && data->stackLength > 0) {
        SZrBraceStart start = data->stack[--data->stackLength];
        return lsp_editor_append_folding_range(data->state,
                                               data->result,
                                               start.line,
                                               start.character,
                                               position.line,
                                               position.character,
                                               ZR_LSP_FOLDING_RANGE_KIND_REGION);
    }

    return ZR_TRUE;
}

/* 对整份快照只扫描代码态花括号，避免字符串和注释中的符号生成伪折叠。 */
static TZrBool lsp_editor_append_structural_folding_ranges(SZrState *state,
                                                           SZrArray *result,
                                                           const TZrChar *content,
                                                           TZrSize contentLength) {
    SZrLspEditorScanState scanState = {ZR_LSP_EDITOR_SCAN_CODE, ZR_FALSE};
    SZrStructuralFoldingScanData data;

    data.state = state;
    data.result = result;
    data.content = content;
    data.contentLength = contentLength;
    data.stackLength = 0;
    return lsp_editor_scan_structural_chars(content,
                                            contentLength,
                                            0,
                                            contentLength,
                                            &scanState,
                                            lsp_editor_append_structural_folding_ranges_callback,
                                            &data);
}

/* stdio 与 WASM 的统一入口：依次合并 import、注释、标记和结构段，再释放同一文档快照。 */
TZrBool ZrLanguageServer_Lsp_GetFoldingRanges(SZrState *state,
                                              SZrLspContext *context,
                                              SZrString *uri,
                                              SZrArray *result) {
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    TZrBool success;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspFoldingRange *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    fileVersion = lsp_editor_get_file_version(context, uri);
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        return ZR_FALSE;
    }

    if (!lsp_editor_append_line_run_folding_ranges(state,
                                                   result,
                                                   snapshot.content,
                                                   snapshot.contentLength,
                                                   lsp_editor_line_is_import_declaration,
                                                   ZR_LSP_FOLDING_RANGE_KIND_IMPORTS)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }
    if (!lsp_editor_append_line_run_folding_ranges(state,
                                                   result,
                                                   snapshot.content,
                                                   snapshot.contentLength,
                                                   lsp_editor_line_is_comment,
                                                   ZR_LSP_FOLDING_RANGE_KIND_COMMENT)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }
    if (!lsp_editor_append_marker_folding_ranges(state,
                                                 result,
                                                 snapshot.content,
                                                 snapshot.contentLength)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }

    success = lsp_editor_append_structural_folding_ranges(state,
                                                          result,
                                                          snapshot.content,
                                                          snapshot.contentLength);
    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    return success;
}
