#include "lsp_editor_features_internal.h"

#include <string.h>

typedef struct SZrLspLineTrim {
    TZrSize start;
    TZrSize end;
} SZrLspLineTrim;

typedef struct SZrBraceStart {
    TZrInt32 line;
    TZrInt32 character;
} SZrBraceStart;

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

static TZrBool lsp_editor_append_line_run_folding_ranges(SZrState *state,
                                                         SZrArray *result,
                                                         const TZrChar *content,
                                                         TZrSize contentLength,
                                                         const TZrChar *prefix,
                                                         TZrSize prefixLength,
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

        matches = hasLine &&
                  lsp_editor_trim_line(content, lineStart, lineEnd, &trim) &&
                  lsp_editor_line_starts_with(content, &trim, prefix, prefixLength);
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

static TZrBool lsp_editor_is_region_start_marker(const TZrChar *content, const SZrLspLineTrim *trim) {
    return lsp_editor_line_starts_with(content, trim, "// region", 9) ||
           lsp_editor_line_starts_with(content, trim, "//region", 8) ||
           lsp_editor_line_starts_with(content, trim, "//#region", 9);
}

static TZrBool lsp_editor_is_region_end_marker(const TZrChar *content, const SZrLspLineTrim *trim) {
    return lsp_editor_line_starts_with(content, trim, "// endregion", 12) ||
           lsp_editor_line_starts_with(content, trim, "//endregion", 11) ||
           lsp_editor_line_starts_with(content, trim, "//#endregion", 12);
}

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

        if (hasLine && lsp_editor_trim_line(content, lineStart, lineEnd, &trim)) {
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

static TZrBool lsp_editor_append_structural_folding_ranges(SZrState *state,
                                                           SZrArray *result,
                                                           const TZrChar *content,
                                                           TZrSize contentLength) {
    SZrBraceStart stack[ZR_LSP_AST_RECURSION_MAX_DEPTH];
    TZrSize stackLength = 0;
    TZrInt32 line = 0;
    TZrInt32 character = 0;

    for (TZrSize index = 0; index < contentLength; index++) {
        TZrChar current = content[index];
        if (current == '{' && stackLength < ZR_LSP_AST_RECURSION_MAX_DEPTH) {
            stack[stackLength].line = line;
            stack[stackLength].character = character;
            stackLength++;
        } else if (current == '}' && stackLength > 0) {
            SZrBraceStart start = stack[--stackLength];
            if (!lsp_editor_append_folding_range(state,
                                                 result,
                                                 start.line,
                                                 start.character,
                                                 line,
                                                 character,
                                                 ZR_LSP_FOLDING_RANGE_KIND_REGION)) {
                return ZR_FALSE;
            }
        }

        if (current == '\n') {
            line++;
            character = 0;
        } else {
            character++;
        }
    }

    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_GetFoldingRanges(SZrState *state,
                                              SZrLspContext *context,
                                              SZrString *uri,
                                              SZrArray *result) {
    SZrFileVersion *fileVersion;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspFoldingRange *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    fileVersion = lsp_editor_get_file_version(context, uri);
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!lsp_editor_append_line_run_folding_ranges(state,
                                                   result,
                                                   fileVersion->content,
                                                   fileVersion->contentLength,
                                                   "%import",
                                                   7,
                                                   ZR_LSP_FOLDING_RANGE_KIND_IMPORTS)) {
        return ZR_FALSE;
    }
    if (!lsp_editor_append_line_run_folding_ranges(state,
                                                   result,
                                                   fileVersion->content,
                                                   fileVersion->contentLength,
                                                   "//",
                                                   2,
                                                   ZR_LSP_FOLDING_RANGE_KIND_COMMENT)) {
        return ZR_FALSE;
    }
    if (!lsp_editor_append_marker_folding_ranges(state,
                                                 result,
                                                 fileVersion->content,
                                                 fileVersion->contentLength)) {
        return ZR_FALSE;
    }

    return lsp_editor_append_structural_folding_ranges(state,
                                                       result,
                                                       fileVersion->content,
                                                       fileVersion->contentLength);
}
