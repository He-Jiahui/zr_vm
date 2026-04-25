#include "lsp_editor_features_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SZrString *lsp_editor_create_string(SZrState *state, const TZrChar *text, TZrSize length) {
    if (state == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrCore_String_Create(state, (TZrNativeString)text, length);
}

SZrFileVersion *lsp_editor_get_file_version(SZrLspContext *context, SZrString *uri) {
    if (context == ZR_NULL || context->parser == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrLanguageServer_IncrementalParser_GetFileVersion(context->parser, uri);
}

SZrLspPosition lsp_editor_position_from_offset(const TZrChar *content,
                                               TZrSize contentLength,
                                               TZrSize offset) {
    SZrLspPosition position = {0, 0};
    TZrSize limit = offset < contentLength ? offset : contentLength;

    for (TZrSize index = 0; index < limit; index++) {
        if (content[index] == '\n') {
            position.line++;
            position.character = 0;
        } else {
            position.character++;
        }
    }

    return position;
}

TZrSize lsp_editor_line_start_offset(const TZrChar *content,
                                     TZrSize contentLength,
                                     TZrInt32 line) {
    TZrInt32 currentLine = 0;

    if (content == ZR_NULL || line <= 0) {
        return 0;
    }

    for (TZrSize index = 0; index < contentLength; index++) {
        if (content[index] == '\n') {
            currentLine++;
            if (currentLine == line) {
                return index + 1;
            }
        }
    }

    return contentLength;
}

TZrSize lsp_editor_line_end_offset(const TZrChar *content,
                                   TZrSize contentLength,
                                   TZrInt32 line) {
    TZrSize offset = lsp_editor_line_start_offset(content, contentLength, line);

    while (offset < contentLength && content[offset] != '\n') {
        offset++;
    }
    if (offset > 0 && content[offset - 1] == '\r') {
        offset--;
    }
    return offset;
}

static SZrLspRange lsp_editor_full_document_range(const TZrChar *content, TZrSize contentLength) {
    SZrLspRange range;
    range.start.line = 0;
    range.start.character = 0;
    range.end = lsp_editor_position_from_offset(content, contentLength, contentLength);
    return range;
}

static TZrBool lsp_code_lens_append(SZrState *state,
                                    SZrArray *result,
                                    SZrLspRange range,
                                    const TZrChar *title,
                                    const TZrChar *command,
                                    SZrString *argument,
                                    const SZrLspPosition *positionArgument) {
    SZrLspCodeLens *lens;

    if (state == ZR_NULL || result == ZR_NULL || title == ZR_NULL || command == ZR_NULL) {
        return ZR_FALSE;
    }

    lens = (SZrLspCodeLens *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspCodeLens));
    if (lens == ZR_NULL) {
        return ZR_FALSE;
    }
    lens->range = range;
    lens->commandTitle = lsp_editor_create_string(state, title, strlen(title));
    lens->command = lsp_editor_create_string(state, command, strlen(command));
    lens->argument = argument;
    lens->hasPositionArgument = positionArgument != ZR_NULL;
    if (positionArgument != ZR_NULL) {
        lens->positionArgument = *positionArgument;
    } else {
        lens->positionArgument.line = 0;
        lens->positionArgument.character = 0;
    }
    ZrCore_Array_Push(state, result, &lens);
    return ZR_TRUE;
}

static void lsp_code_lens_free_locations(SZrState *state, SZrArray *locations) {
    if (state == ZR_NULL || locations == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        if (locationPtr != ZR_NULL && *locationPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *locationPtr, sizeof(SZrLspLocation));
        }
    }
    ZrCore_Array_Free(state, locations);
}

static TZrBool lsp_code_lens_should_count_symbol(SZrSymbol *symbol, SZrString *uri) {
    if (symbol == ZR_NULL || uri == ZR_NULL || !ZrLanguageServer_Lsp_StringsEqual(symbol->location.source, uri)) {
        return ZR_FALSE;
    }

    return symbol->type == ZR_SYMBOL_FUNCTION ||
           symbol->type == ZR_SYMBOL_METHOD ||
           symbol->type == ZR_SYMBOL_CLASS ||
           symbol->type == ZR_SYMBOL_STRUCT ||
           symbol->type == ZR_SYMBOL_INTERFACE;
}

static SZrLspPosition lsp_code_lens_symbol_position(SZrSymbol *symbol) {
    SZrLspRange range = ZrLanguageServer_LspRange_FromFileRange(
        ZrLanguageServer_Lsp_GetSymbolLookupRange(symbol));
    return range.start;
}

static TZrBool lsp_code_lens_append_reference_counts(SZrState *state,
                                                     SZrLspContext *context,
                                                     SZrString *uri,
                                                     SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL || !analyzer->symbolTable->allScopes.isValid) {
        return ZR_TRUE;
    }

    for (TZrSize scopeIndex = 0; scopeIndex < analyzer->symbolTable->allScopes.length; scopeIndex++) {
        SZrSymbolScope **scopePtr =
            (SZrSymbolScope **)ZrCore_Array_Get(&analyzer->symbolTable->allScopes, scopeIndex);
        if (scopePtr == ZR_NULL || *scopePtr == ZR_NULL) {
            continue;
        }

        for (TZrSize symbolIndex = 0; symbolIndex < (*scopePtr)->symbols.length; symbolIndex++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&(*scopePtr)->symbols, symbolIndex);
            SZrArray references = {0};
            SZrLspPosition position;
            SZrLspRange range;
            TZrChar title[ZR_LSP_SHORT_TEXT_BUFFER_LENGTH];

            if (symbolPtr == ZR_NULL || !lsp_code_lens_should_count_symbol(*symbolPtr, uri)) {
                continue;
            }

            position = lsp_code_lens_symbol_position(*symbolPtr);
            ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
            if (!ZrLanguageServer_Lsp_FindReferences(state, context, uri, position, ZR_FALSE, &references) ||
                references.length == 0) {
                lsp_code_lens_free_locations(state, &references);
                continue;
            }

            snprintf(title,
                     sizeof(title),
                     "%zu reference%s",
                     (size_t)references.length,
                     references.length == 1 ? "" : "s");
            range = ZrLanguageServer_LspRange_FromFileRange(ZrLanguageServer_Lsp_GetSymbolLookupRange(*symbolPtr));
            if (!lsp_code_lens_append(state, result, range, title, "zr.showReferences", uri, &position)) {
                lsp_code_lens_free_locations(state, &references);
                return ZR_FALSE;
            }
            lsp_code_lens_free_locations(state, &references);
        }
    }

    return ZR_TRUE;
}

SZrLspRange lsp_editor_range_from_offsets(const TZrChar *content,
                                          TZrSize contentLength,
                                          TZrSize startOffset,
                                          TZrSize endOffset) {
    SZrLspRange range;
    range.start = lsp_editor_position_from_offset(content, contentLength, startOffset);
    range.end = lsp_editor_position_from_offset(content, contentLength, endOffset);
    return range;
}

static TZrBool lsp_text_builder_reserve(SZrLspTextBuilder *builder, TZrSize extra) {
    TZrSize required;
    TZrSize nextCapacity;
    TZrChar *nextData;

    if (builder == ZR_NULL) {
        return ZR_FALSE;
    }

    required = builder->length + extra + 1;
    if (required <= builder->capacity) {
        return ZR_TRUE;
    }

    nextCapacity = builder->capacity > 0 ? builder->capacity : 128;
    while (nextCapacity < required) {
        nextCapacity *= 2;
    }

    nextData = (TZrChar *)realloc(builder->data, nextCapacity);
    if (nextData == ZR_NULL) {
        return ZR_FALSE;
    }

    builder->data = nextData;
    builder->capacity = nextCapacity;
    return ZR_TRUE;
}

TZrBool lsp_text_builder_append_range(SZrLspTextBuilder *builder,
                                      const TZrChar *text,
                                      TZrSize length) {
    if (length == 0) {
        return ZR_TRUE;
    }
    if (builder == ZR_NULL || text == ZR_NULL || !lsp_text_builder_reserve(builder, length)) {
        return ZR_FALSE;
    }

    memcpy(builder->data + builder->length, text, length);
    builder->length += length;
    builder->data[builder->length] = '\0';
    return ZR_TRUE;
}

TZrBool lsp_text_builder_append_char(SZrLspTextBuilder *builder, TZrChar value) {
    return lsp_text_builder_append_range(builder, &value, 1);
}

static TZrBool lsp_text_builder_append_indent(SZrLspTextBuilder *builder, TZrInt32 indentLevel) {
    for (TZrInt32 level = 0; level < indentLevel; level++) {
        if (!lsp_text_builder_append_range(builder, "    ", 4)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool lsp_editor_append_text_edit(SZrState *state,
                                    SZrArray *result,
                                    SZrLspRange range,
                                    const TZrChar *newText,
                                    TZrSize newTextLength) {
    SZrLspTextEdit *edit;

    if (state == ZR_NULL || result == ZR_NULL || newText == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    edit = (SZrLspTextEdit *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspTextEdit));
    if (edit == ZR_NULL) {
        return ZR_FALSE;
    }

    edit->range = range;
    edit->newText = lsp_editor_create_string(state, newText, newTextLength);
    if (edit->newText == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, edit, sizeof(SZrLspTextEdit));
        return ZR_FALSE;
    }

    ZrCore_Array_Push(state, result, &edit);
    return ZR_TRUE;
}

static TZrInt32 lsp_editor_indent_before_offset(const TZrChar *content, TZrSize offset) {
    TZrInt32 indent = 0;

    for (TZrSize index = 0; content != ZR_NULL && index < offset; index++) {
        if (content[index] == '{') {
            indent++;
        } else if (content[index] == '}' && indent > 0) {
            indent--;
        }
    }

    return indent;
}

static TZrChar *lsp_editor_format_segment(const TZrChar *content,
                                          TZrSize contentLength,
                                          TZrSize startOffset,
                                          TZrSize endOffset,
                                          TZrSize *outLength) {
    SZrLspTextBuilder builder = {0};
    TZrSize cursor;
    TZrInt32 indent;

    if (outLength != ZR_NULL) {
        *outLength = 0;
    }
    if (content == ZR_NULL || outLength == ZR_NULL || startOffset > contentLength) {
        return ZR_NULL;
    }
    if (endOffset > contentLength) {
        endOffset = contentLength;
    }
    if (endOffset < startOffset) {
        endOffset = startOffset;
    }

    indent = lsp_editor_indent_before_offset(content, startOffset);
    cursor = startOffset;
    while (cursor < endOffset) {
        TZrSize lineStart = cursor;
        TZrSize lineEnd = cursor;
        TZrSize trimStart;
        TZrSize trimEnd;
        TZrBool hasNewline;
        TZrBool leadingClose = ZR_FALSE;

        while (lineEnd < endOffset && content[lineEnd] != '\n') {
            lineEnd++;
        }
        hasNewline = lineEnd < endOffset && content[lineEnd] == '\n';

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

        if (trimStart < trimEnd) {
            if (content[trimStart] == '}') {
                leadingClose = ZR_TRUE;
                if (indent > 0) {
                    indent--;
                }
            }

            if (!lsp_text_builder_append_indent(&builder, indent) ||
                !lsp_text_builder_append_range(&builder, content + trimStart, trimEnd - trimStart)) {
                free(builder.data);
                return ZR_NULL;
            }

            for (TZrSize index = trimStart; index < trimEnd; index++) {
                if (content[index] == '{') {
                    indent++;
                } else if (content[index] == '}' && !(leadingClose && index == trimStart) && indent > 0) {
                    indent--;
                }
            }
        }

        if (hasNewline && !lsp_text_builder_append_char(&builder, '\n')) {
            free(builder.data);
            return ZR_NULL;
        }
        cursor = hasNewline ? lineEnd + 1 : lineEnd;
    }

    if (builder.data == ZR_NULL) {
        builder.data = (TZrChar *)malloc(1);
        if (builder.data == ZR_NULL) {
            return ZR_NULL;
        }
        builder.data[0] = '\0';
    }

    *outLength = builder.length;
    return builder.data;
}

TZrBool ZrLanguageServer_Lsp_GetFormatting(SZrState *state,
                                           SZrLspContext *context,
                                           SZrString *uri,
                                           SZrArray *result) {
    SZrFileVersion *fileVersion;
    TZrChar *formatted;
    TZrSize formattedLength;
    SZrLspRange range;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    fileVersion = lsp_editor_get_file_version(context, uri);
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL) {
        return ZR_FALSE;
    }

    formatted = lsp_editor_format_segment(fileVersion->content,
                                          fileVersion->contentLength,
                                          0,
                                          fileVersion->contentLength,
                                          &formattedLength);
    if (formatted == ZR_NULL) {
        return ZR_FALSE;
    }

    if (formattedLength == fileVersion->contentLength &&
        memcmp(formatted, fileVersion->content, formattedLength) == 0) {
        free(formatted);
        return ZR_TRUE;
    }

    range = lsp_editor_full_document_range(fileVersion->content, fileVersion->contentLength);
    if (!lsp_editor_append_text_edit(state, result, range, formatted, formattedLength)) {
        free(formatted);
        return ZR_FALSE;
    }

    free(formatted);
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_GetRangeFormatting(SZrState *state,
                                                SZrLspContext *context,
                                                SZrString *uri,
                                                SZrLspRange range,
                                                SZrArray *result) {
    SZrFileVersion *fileVersion;
    TZrSize startOffset;
    TZrSize endOffset;
    TZrChar *formatted;
    TZrSize formattedLength;
    SZrLspRange editRange;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    fileVersion = lsp_editor_get_file_version(context, uri);
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL) {
        return ZR_FALSE;
    }

    startOffset = lsp_editor_line_start_offset(fileVersion->content, fileVersion->contentLength, range.start.line);
    endOffset = lsp_editor_line_end_offset(fileVersion->content, fileVersion->contentLength, range.end.line);
    if (endOffset < fileVersion->contentLength && fileVersion->content[endOffset] == '\n') {
        endOffset++;
    }

    formatted = lsp_editor_format_segment(fileVersion->content,
                                          fileVersion->contentLength,
                                          startOffset,
                                          endOffset,
                                          &formattedLength);
    if (formatted == ZR_NULL) {
        return ZR_FALSE;
    }

    if (formattedLength == endOffset - startOffset &&
        memcmp(formatted, fileVersion->content + startOffset, formattedLength) == 0) {
        free(formatted);
        return ZR_TRUE;
    }

    editRange = lsp_editor_range_from_offsets(fileVersion->content,
                                              fileVersion->contentLength,
                                              startOffset,
                                              endOffset);
    if (!lsp_editor_append_text_edit(state, result, editRange, formatted, formattedLength)) {
        free(formatted);
        return ZR_FALSE;
    }

    free(formatted);
    return ZR_TRUE;
}

static TZrBool lsp_editor_is_word_char(TZrChar value) {
    return (value >= 'a' && value <= 'z') ||
           (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9') ||
           value == '_';
}

static TZrBool lsp_editor_range_extends(SZrLspRange outer, SZrLspRange inner) {
    if (outer.start.line < inner.start.line || outer.end.line > inner.end.line) {
        return ZR_TRUE;
    }
    if (outer.start.line == inner.start.line && outer.start.character < inner.start.character) {
        return ZR_TRUE;
    }
    return outer.end.line == inner.end.line && outer.end.character > inner.end.character;
}

static TZrBool lsp_editor_find_selection_block_range(const TZrChar *content,
                                                     TZrSize contentLength,
                                                     TZrSize offset,
                                                     TZrSize lineStart,
                                                     TZrSize lineEnd,
                                                     SZrLspRange *outRange) {
    TZrSize stack[ZR_LSP_AST_RECURSION_MAX_DEPTH];
    TZrSize stackLength = 0;
    TZrSize bestStart = 0;
    TZrSize bestEnd = 0;
    TZrBool found = ZR_FALSE;

    if (content == ZR_NULL || outRange == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < contentLength; index++) {
        TZrChar current = content[index];
        if (current == '{' && stackLength < ZR_LSP_AST_RECURSION_MAX_DEPTH) {
            stack[stackLength++] = index;
        } else if (current == '}' && stackLength > 0) {
            TZrSize openOffset = stack[--stackLength];
            TZrBool containsOffset = openOffset <= offset && offset <= index;
            TZrBool opensOnLine = openOffset >= lineStart && openOffset <= lineEnd && index >= offset;
            if ((containsOffset || opensOnLine) && (!found || openOffset > bestStart)) {
                bestStart = openOffset;
                bestEnd = index + 1;
                found = ZR_TRUE;
            }
        }
    }

    if (!found) {
        return ZR_FALSE;
    }

    *outRange = lsp_editor_range_from_offsets(content, contentLength, bestStart, bestEnd);
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_GetSelectionRanges(SZrState *state,
                                                SZrLspContext *context,
                                                SZrString *uri,
                                                const SZrLspPosition *positions,
                                                TZrSize positionCount,
                                                SZrArray *result) {
    SZrFileVersion *fileVersion;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        positions == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspSelectionRange *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    fileVersion = lsp_editor_get_file_version(context, uri);
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < positionCount; index++) {
        SZrLspSelectionRange *selection;
        TZrSize offset = ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn(fileVersion->content,
                                                                            fileVersion->contentLength,
                                                                            positions[index].line,
                                                                            positions[index].character);
        TZrSize wordStart = offset;
        TZrSize wordEnd = offset;
        TZrSize lineStart = lsp_editor_line_start_offset(fileVersion->content,
                                                         fileVersion->contentLength,
                                                         positions[index].line);
        TZrSize lineEnd = lsp_editor_line_end_offset(fileVersion->content,
                                                     fileVersion->contentLength,
                                                     positions[index].line);
        TZrSize blockSearchLineStart = lineStart;
        TZrSize blockSearchLineEnd = lineEnd;
        SZrLspRange blockRange;
        TZrBool hasBlockRange;

        while (wordStart > lineStart && lsp_editor_is_word_char(fileVersion->content[wordStart - 1])) {
            wordStart--;
        }
        while (wordEnd < lineEnd && lsp_editor_is_word_char(fileVersion->content[wordEnd])) {
            wordEnd++;
        }
        if (wordEnd == wordStart && wordEnd < lineEnd) {
            wordEnd++;
        }

        while (lineStart < lineEnd &&
               (fileVersion->content[lineStart] == ' ' || fileVersion->content[lineStart] == '\t')) {
            lineStart++;
        }

        selection = (SZrLspSelectionRange *)ZrCore_Memory_RawMalloc(state->global,
                                                                    sizeof(SZrLspSelectionRange));
        if (selection == ZR_NULL) {
            return ZR_FALSE;
        }
        selection->range = lsp_editor_range_from_offsets(fileVersion->content,
                                                         fileVersion->contentLength,
                                                         wordStart,
                                                         wordEnd);
        selection->parentRange = lsp_editor_range_from_offsets(fileVersion->content,
                                                               fileVersion->contentLength,
                                                               lineStart,
                                                               lineEnd);
        selection->hasParent = selection->parentRange.start.line != selection->range.start.line ||
                               selection->parentRange.start.character < selection->range.start.character ||
                               selection->parentRange.end.character > selection->range.end.character;
        hasBlockRange = lsp_editor_find_selection_block_range(fileVersion->content,
                                                              fileVersion->contentLength,
                                                              offset,
                                                              blockSearchLineStart,
                                                              blockSearchLineEnd,
                                                              &blockRange);
        selection->hasGrandParent = selection->hasParent &&
                                    hasBlockRange &&
                                    lsp_editor_range_extends(blockRange, selection->parentRange);
        selection->grandParentRange = selection->hasGrandParent ? blockRange : selection->parentRange;
        ZrCore_Array_Push(state, result, &selection);
    }

    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_GetCodeLens(SZrState *state,
                                         SZrLspContext *context,
                                         SZrString *uri,
                                         SZrArray *result) {
    SZrFileVersion *fileVersion;
    const TZrChar *content;
    TZrSize cursor = 0;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspCodeLens *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    fileVersion = lsp_editor_get_file_version(context, uri);
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!lsp_code_lens_append_reference_counts(state, context, uri, result)) {
        return ZR_FALSE;
    }

    content = fileVersion->content;
    while (cursor < fileVersion->contentLength) {
        const TZrChar *match = strstr(content + cursor, "%test(");
        TZrSize matchOffset;
        TZrSize lineEnd;

        if (match == ZR_NULL) {
            break;
        }
        matchOffset = (TZrSize)(match - content);
        lineEnd = matchOffset;
        while (lineEnd < fileVersion->contentLength && content[lineEnd] != '\n') {
            lineEnd++;
        }

        if (!lsp_code_lens_append(state,
                                  result,
                                  lsp_editor_range_from_offsets(content, fileVersion->contentLength, matchOffset, lineEnd),
                                  "Run Zr test",
                                  "zr.runCurrentProject",
                                  uri,
                                  ZR_NULL)) {
            return ZR_FALSE;
        }
        cursor = lineEnd;
    }

    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_GetDeclaration(SZrState *state,
                                            SZrLspContext *context,
                                            SZrString *uri,
                                            SZrLspPosition position,
                                            SZrArray *result) {
    return ZrLanguageServer_Lsp_GetDefinition(state, context, uri, position, result);
}

TZrBool ZrLanguageServer_Lsp_GetTypeDefinition(SZrState *state,
                                               SZrLspContext *context,
                                               SZrString *uri,
                                               SZrLspPosition position,
                                               SZrArray *result) {
    return ZrLanguageServer_Lsp_GetDefinition(state, context, uri, position, result);
}

TZrBool ZrLanguageServer_Lsp_GetImplementation(SZrState *state,
                                               SZrLspContext *context,
                                               SZrString *uri,
                                               SZrLspPosition position,
                                               SZrArray *result) {
    return ZrLanguageServer_Lsp_GetDefinition(state, context, uri, position, result);
}

void ZrLanguageServer_Lsp_FreeTextEdits(SZrState *state, SZrArray *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspTextEdit **editPtr = (SZrLspTextEdit **)ZrCore_Array_Get(result, index);
        if (editPtr != ZR_NULL && *editPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *editPtr, sizeof(SZrLspTextEdit));
        }
    }
    ZrCore_Array_Free(state, result);
}

void ZrLanguageServer_Lsp_FreeCodeActions(SZrState *state, SZrArray *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(result, index);
        if (actionPtr != ZR_NULL && *actionPtr != ZR_NULL) {
            ZrLanguageServer_Lsp_FreeTextEdits(state, &(*actionPtr)->edits);
            ZrCore_Memory_RawFree(state->global, *actionPtr, sizeof(SZrLspCodeAction));
        }
    }
    ZrCore_Array_Free(state, result);
}

void ZrLanguageServer_Lsp_FreeFoldingRanges(SZrState *state, SZrArray *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspFoldingRange **rangePtr = (SZrLspFoldingRange **)ZrCore_Array_Get(result, index);
        if (rangePtr != ZR_NULL && *rangePtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *rangePtr, sizeof(SZrLspFoldingRange));
        }
    }
    ZrCore_Array_Free(state, result);
}

void ZrLanguageServer_Lsp_FreeSelectionRanges(SZrState *state, SZrArray *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspSelectionRange **rangePtr = (SZrLspSelectionRange **)ZrCore_Array_Get(result, index);
        if (rangePtr != ZR_NULL && *rangePtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *rangePtr, sizeof(SZrLspSelectionRange));
        }
    }
    ZrCore_Array_Free(state, result);
}

void ZrLanguageServer_Lsp_FreeDocumentLinks(SZrState *state, SZrArray *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspDocumentLink **linkPtr = (SZrLspDocumentLink **)ZrCore_Array_Get(result, index);
        if (linkPtr != ZR_NULL && *linkPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *linkPtr, sizeof(SZrLspDocumentLink));
        }
    }
    ZrCore_Array_Free(state, result);
}

void ZrLanguageServer_Lsp_FreeCodeLens(SZrState *state, SZrArray *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspCodeLens **lensPtr = (SZrLspCodeLens **)ZrCore_Array_Get(result, index);
        if (lensPtr != ZR_NULL && *lensPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *lensPtr, sizeof(SZrLspCodeLens));
        }
    }
    ZrCore_Array_Free(state, result);
}

void ZrLanguageServer_Lsp_FreeHierarchyItems(SZrState *state, SZrArray *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspHierarchyItem **itemPtr = (SZrLspHierarchyItem **)ZrCore_Array_Get(result, index);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *itemPtr, sizeof(SZrLspHierarchyItem));
        }
    }
    ZrCore_Array_Free(state, result);
}

void ZrLanguageServer_Lsp_FreeHierarchyCalls(SZrState *state, SZrArray *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspHierarchyCall **callPtr = (SZrLspHierarchyCall **)ZrCore_Array_Get(result, index);
        if (callPtr != ZR_NULL && *callPtr != ZR_NULL) {
            ZrCore_Array_Free(state, &(*callPtr)->fromRanges);
            if ((*callPtr)->item != ZR_NULL) {
                ZrCore_Memory_RawFree(state->global, (*callPtr)->item, sizeof(SZrLspHierarchyItem));
            }
            ZrCore_Memory_RawFree(state->global, *callPtr, sizeof(SZrLspHierarchyCall));
        }
    }
    ZrCore_Array_Free(state, result);
}
