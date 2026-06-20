//
// Focused LSP line/character mapping regressions.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server.h"

static int g_failures = 0;

static TZrPtr test_allocator(TZrPtr userData,
                             TZrPtr pointer,
                             TZrSize originalSize,
                             TZrSize newSize,
                             TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        free(pointer);
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }

    return realloc(pointer, newSize);
}

static SZrLspContext *test_open_document(SZrState *state,
                                         const TZrChar *uriText,
                                         const TZrChar *content,
                                         SZrString **outUri) {
    SZrLspContext *context;
    SZrString *uri;

    if (state == ZR_NULL || uriText == ZR_NULL || content == ZR_NULL || outUri == ZR_NULL) {
        return ZR_NULL;
    }

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        return ZR_NULL;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        return ZR_NULL;
    }

    *outUri = uri;
    return context;
}

static void test_hover_free(SZrState *state, SZrLspHover *hover) {
    if (state == ZR_NULL || hover == ZR_NULL) {
        return;
    }

    ZrCore_Array_Free(state, &hover->contents);
    ZrCore_Memory_RawFree(state->global, hover, sizeof(SZrLspHover));
}

static const TZrChar *test_string_ptr(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(value);
}

static TZrBool test_lsp_range_equals(SZrLspRange range,
                                     TZrInt32 startLine,
                                     TZrInt32 startCharacter,
                                     TZrInt32 endLine,
                                     TZrInt32 endCharacter) {
    return range.start.line == startLine &&
           range.start.character == startCharacter &&
           range.end.line == endLine &&
           range.end.character == endCharacter;
}

static TZrBool test_highlights_contain_range(SZrArray *highlights,
                                             TZrInt32 startLine,
                                             TZrInt32 startCharacter,
                                             TZrInt32 endLine,
                                             TZrInt32 endCharacter) {
    if (highlights == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < highlights->length; index++) {
        SZrLspDocumentHighlight **highlightPtr =
            (SZrLspDocumentHighlight **)ZrCore_Array_Get(highlights, index);
        SZrLspDocumentHighlight *highlight = highlightPtr != ZR_NULL ? *highlightPtr : ZR_NULL;
        if (highlight != ZR_NULL &&
            test_lsp_range_equals(highlight->range, startLine, startCharacter, endLine, endCharacter)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool test_diagnostics_contain_range(SZrArray *diagnostics,
                                              TZrInt32 startLine,
                                              TZrInt32 startCharacter,
                                              TZrInt32 endLine,
                                              TZrInt32 endCharacter) {
    if (diagnostics == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < diagnostics->length; index++) {
        SZrLspDiagnostic **diagnosticPtr =
            (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        SZrLspDiagnostic *diagnostic = diagnosticPtr != ZR_NULL ? *diagnosticPtr : ZR_NULL;
        if (diagnostic != ZR_NULL &&
            test_lsp_range_equals(diagnostic->range, startLine, startCharacter, endLine, endCharacter)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool test_symbols_contain_range(SZrArray *symbols,
                                          TZrInt32 startLine,
                                          TZrInt32 startCharacter,
                                          TZrInt32 endLine,
                                          TZrInt32 endCharacter) {
    if (symbols == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < symbols->length; index++) {
        SZrLspSymbolInformation **symbolPtr =
            (SZrLspSymbolInformation **)ZrCore_Array_Get(symbols, index);
        SZrLspSymbolInformation *symbol = symbolPtr != ZR_NULL ? *symbolPtr : ZR_NULL;
        if (symbol != ZR_NULL &&
            test_lsp_range_equals(symbol->location.range, startLine, startCharacter, endLine, endCharacter)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool test_locations_contain_range(SZrArray *locations,
                                            TZrInt32 startLine,
                                            TZrInt32 startCharacter,
                                            TZrInt32 endLine,
                                            TZrInt32 endCharacter) {
    if (locations == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr =
            (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        SZrLspLocation *location = locationPtr != ZR_NULL ? *locationPtr : ZR_NULL;
        if (location != ZR_NULL &&
            test_lsp_range_equals(location->range, startLine, startCharacter, endLine, endCharacter)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool test_inlay_hints_contain_position_and_label(SZrArray *hints,
                                                           TZrInt32 line,
                                                           TZrInt32 character,
                                                           const TZrChar *labelFragment) {
    if (hints == ZR_NULL || labelFragment == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < hints->length; index++) {
        SZrLspInlayHint **hintPtr =
            (SZrLspInlayHint **)ZrCore_Array_Get(hints, index);
        SZrLspInlayHint *hint = hintPtr != ZR_NULL ? *hintPtr : ZR_NULL;
        const TZrChar *label = hint != ZR_NULL ? test_string_ptr(hint->label) : ZR_NULL;
        if (hint != ZR_NULL &&
            hint->position.line == line &&
            hint->position.character == character &&
            label != ZR_NULL &&
            strstr(label, labelFragment) != ZR_NULL) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrInt32 test_semantic_token_type_index(const TZrChar *typeName) {
    if (typeName == ZR_NULL) {
        return -1;
    }

    for (TZrSize index = 0; index < ZrLanguageServer_Lsp_SemanticTokenTypeCount(); index++) {
        const TZrChar *candidate = ZrLanguageServer_Lsp_SemanticTokenTypeName(index);
        if (candidate != ZR_NULL && strcmp(candidate, typeName) == 0) {
            return (TZrInt32)index;
        }
    }

    return -1;
}

static TZrBool test_semantic_tokens_contain(SZrArray *data,
                                            TZrInt32 line,
                                            TZrInt32 character,
                                            TZrInt32 length,
                                            const TZrChar *typeName) {
    TZrUInt32 currentLine = 0;
    TZrUInt32 currentCharacter = 0;
    TZrInt32 typeIndex = test_semantic_token_type_index(typeName);

    if (data == ZR_NULL || typeIndex < 0) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index + 4 < data->length; index += 5) {
        TZrUInt32 *deltaLinePtr = (TZrUInt32 *)ZrCore_Array_Get(data, index);
        TZrUInt32 *deltaStartPtr = (TZrUInt32 *)ZrCore_Array_Get(data, index + 1);
        TZrUInt32 *lengthPtr = (TZrUInt32 *)ZrCore_Array_Get(data, index + 2);
        TZrUInt32 *typePtr = (TZrUInt32 *)ZrCore_Array_Get(data, index + 3);

        if (deltaLinePtr == ZR_NULL || deltaStartPtr == ZR_NULL ||
            lengthPtr == ZR_NULL || typePtr == ZR_NULL) {
            continue;
        }

        currentLine += *deltaLinePtr;
        if (*deltaLinePtr == 0) {
            currentCharacter += *deltaStartPtr;
        } else {
            currentCharacter = *deltaStartPtr;
        }

        if ((TZrInt32)currentLine == line &&
            (TZrInt32)currentCharacter == character &&
            (TZrInt32)(*lengthPtr) == length &&
            (TZrInt32)(*typePtr) == typeIndex) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void check_offset(const TZrChar *summary,
                         const TZrChar *content,
                         TZrInt32 line,
                         TZrInt32 character,
                         TZrSize expectedOffset) {
    SZrLspRange lspRange;
    SZrFileRange fileRange;

    lspRange.start.line = line;
    lspRange.start.character = character;
    lspRange.end = lspRange.start;

    fileRange = ZrLanguageServer_LspRange_ToFileRangeWithContent(lspRange,
                                                                  ZR_NULL,
                                                                  content,
                                                                  strlen(content));
    if (fileRange.start.offset != expectedOffset || fileRange.end.offset != expectedOffset) {
        printf("FAIL: %s expected offset %llu but got start=%llu end=%llu\n",
               summary,
               (unsigned long long)expectedOffset,
               (unsigned long long)fileRange.start.offset,
               (unsigned long long)fileRange.end.offset);
        g_failures++;
        return;
    }

    printf("PASS: %s\n", summary);
}

static void check_file_position(const TZrChar *summary,
                                const TZrChar *content,
                                TZrInt32 line,
                                TZrInt32 character,
                                TZrSize expectedOffset,
                                TZrInt32 expectedFileLine,
                                TZrInt32 expectedFileColumn) {
    SZrLspPosition lspPosition;
    SZrFilePosition filePosition;

    lspPosition.line = line;
    lspPosition.character = character;

    filePosition = ZrLanguageServer_LspPosition_ToFilePositionWithContent(lspPosition,
                                                                          content,
                                                                          strlen(content));
    if (filePosition.offset != expectedOffset ||
        filePosition.line != expectedFileLine ||
        filePosition.column != expectedFileColumn) {
        printf("FAIL: %s expected file position %llu:%d:%d but got %llu:%d:%d\n",
               summary,
               (unsigned long long)expectedOffset,
               expectedFileLine,
               expectedFileColumn,
               (unsigned long long)filePosition.offset,
               filePosition.line,
               filePosition.column);
        g_failures++;
        return;
    }

    printf("PASS: %s\n", summary);
}

static void check_roundtrip(const TZrChar *summary,
                            const TZrChar *content,
                            TZrSize offset,
                            TZrInt32 expectedLine,
                            TZrInt32 expectedCharacter) {
    SZrLspPosition position;
    SZrFilePosition filePosition;
    SZrLspRange range;
    SZrFileRange fileRange;
    SZrFilePosition mappedFilePosition;
    TZrSize mappedOffset;

    filePosition = ZrParser_FilePosition_Create(offset, expectedLine + 1, 1);
    position = ZrLanguageServer_LspPosition_FromFilePositionWithContent(filePosition,
                                                                        content,
                                                                        strlen(content));
    if (position.line != expectedLine || position.character != expectedCharacter) {
        printf("FAIL: %s expected position %d:%d but got %d:%d\n",
               summary,
               expectedLine,
               expectedCharacter,
               position.line,
               position.character);
        g_failures++;
        return;
    }

    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, ZR_NULL);
    range = ZrLanguageServer_LspRange_FromFileRangeWithContent(fileRange,
                                                               content,
                                                               strlen(content));
    if (range.start.line != expectedLine || range.start.character != expectedCharacter ||
        range.end.line != expectedLine || range.end.character != expectedCharacter) {
        printf("FAIL: %s public file range conversion expected %d:%d but got start=%d:%d end=%d:%d\n",
               summary,
               expectedLine,
               expectedCharacter,
               range.start.line,
               range.start.character,
               range.end.line,
               range.end.character);
        g_failures++;
        return;
    }

    mappedFilePosition = ZrLanguageServer_LspPosition_ToFilePositionWithContent(position,
                                                                                content,
                                                                                strlen(content));
    mappedOffset = mappedFilePosition.offset;
    if (mappedOffset != offset) {
        printf("FAIL: %s expected roundtrip offset %llu but got %llu\n",
               summary,
               (unsigned long long)offset,
               (unsigned long long)mappedOffset);
        g_failures++;
        return;
    }

    printf("PASS: %s\n", summary);
}

static void test_bmp_utf8_before_cursor_maps_from_utf16_character(void) {
    const TZrChar *content = "var label = \"\xCE\xBB\";\nvar next = 1;\n";
    check_offset("BMP UTF-8 character before cursor maps from UTF-16 character",
                 content,
                 0,
                 15,
                 (TZrSize)16);
}

static void test_non_bmp_utf8_before_cursor_counts_as_two_utf16_units(void) {
    const TZrChar *content = "var icon = \"\xF0\x9F\x98\x80\";\nvar next = 1;\n";
    check_offset("Non-BMP UTF-8 character before cursor counts as two UTF-16 units",
                 content,
                 0,
                 16,
                 (TZrSize)18);
}

static void test_chinese_identifier_maps_from_utf16_character(void) {
    const TZrChar *content = "var \xE5\x80\xBC = 1;\n";
    check_offset("Chinese identifier maps one UTF-16 unit to a three-byte UTF-8 codepoint",
                 content,
                 0,
                 5,
                 (TZrSize)7);
}

static void test_mixed_ascii_and_emoji_maps_inside_line(void) {
    const TZrChar *content = "var mixed = a" "\xF0\x9F\x98\x80" "b;\n";
    check_offset("Mixed ASCII and emoji maps UTF-16 character after surrogate pair",
                 content,
                 0,
                 16,
                 (TZrSize)18);
}

static void test_crlf_line_start_maps_after_lf(void) {
    const TZrChar *content = "var x = 1;\r\nvar y = 2;\n";
    check_offset("CRLF line start maps after the LF byte",
                 content,
                 1,
                 0,
                 (TZrSize)12);
}

static void test_tab_counts_as_one_utf16_unit(void) {
    const TZrChar *content = "var\tname = 1;\n";
    check_offset("Tab counts as one UTF-16 character unit",
                 content,
                 0,
                 4,
                 (TZrSize)4);
}

static void test_eof_without_newline_clamps_to_content_end(void) {
    const TZrChar *content = "var tail = 1;";
    check_offset("EOF without trailing newline maps to content end",
                 content,
                 0,
                 13,
                 (TZrSize)13);
}

static void test_offset_to_position_roundtrip_counts_utf16_units(void) {
    const TZrChar *content = "var icon = \"\xF0\x9F\x98\x80\";\n";
    check_roundtrip("Offset to UTF-16 position roundtrip counts emoji as two units",
                    content,
                    (TZrSize)18,
                    0,
                    16);
}

static void test_file_position_uses_parser_byte_column_after_utf8(void) {
    const TZrChar *content = "var label = \"\xCE\xBB\";\n";
    check_file_position("File position keeps parser byte column after BMP UTF-8",
                        content,
                        0,
                        15,
                        (TZrSize)16,
                        1,
                        17);
}

static void test_lsp_position_before_negative_line_clamps_to_file_start(void) {
    const TZrChar *content = "var x = 1;\n";
    check_file_position("Negative LSP line and character clamp to file start",
                        content,
                        -1,
                        -1,
                        (TZrSize)0,
                        1,
                        1);
}

static void test_lsp_position_beyond_line_clamps_column_to_line_end(void) {
    const TZrChar *content = "var x = 1;\nvar y = 2;\n";
    check_file_position("LSP character beyond line clamps to line-end byte column",
                        content,
                        0,
                        999,
                        (TZrSize)10,
                        1,
                        11);
}

static void test_lsp_position_beyond_file_clamps_to_eof_position(void) {
    const TZrChar *content = "var x = 1;\n";
    check_file_position("LSP line beyond file clamps to EOF file position",
                        content,
                        99,
                        0,
                        (TZrSize)11,
                        2,
                        1);
}

static void test_definition_range_after_utf8_prefix_uses_utf16_columns(void) {
    const TZrChar *content = "/* \xCE\xBB */ var target = 1;\nvar use = target;\n";
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    SZrState *state;
    SZrLspContext *context;
    SZrString *uri = ZR_NULL;
    SZrLspPosition usagePosition;
    SZrArray definitions = {0};
    TZrBool resolved;
    SZrLspLocation **locationPtr;
    SZrLspLocation *location;

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: Definition range after UTF-8 prefix could not create VM state\n");
        g_failures++;
        return;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);
    context = test_open_document(state, "file:///tmp/zr_lsp_position_definition.zr", content, &uri);
    if (context == ZR_NULL) {
        printf("FAIL: Definition range after UTF-8 prefix could not open document\n");
        ZrCore_GlobalState_Free(global);
        g_failures++;
        return;
    }

    usagePosition.line = 1;
    usagePosition.character = 11;
    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 1);
    resolved = ZrLanguageServer_Lsp_GetDefinition(state, context, uri, usagePosition, &definitions);
    locationPtr = definitions.length > 0 ? (SZrLspLocation **)ZrCore_Array_Get(&definitions, 0) : ZR_NULL;
    location = locationPtr != ZR_NULL ? *locationPtr : ZR_NULL;

    if (!resolved || location == ZR_NULL ||
        location->range.start.line != 0 ||
        location->range.start.character != 12 ||
        location->range.end.line != 0 ||
        location->range.end.character != 18) {
        printf("FAIL: Definition range after UTF-8 prefix expected 0:12-0:18 but got count=%llu",
               (unsigned long long)definitions.length);
        if (location != ZR_NULL) {
            printf(" first=%d:%d-%d:%d",
                   location->range.start.line,
                   location->range.start.character,
                   location->range.end.line,
                   location->range.end.character);
        }
        printf("\n");
        g_failures++;
    } else {
        printf("PASS: Definition range after UTF-8 prefix uses UTF-16 columns\n");
    }

    ZrCore_Array_Free(state, &definitions);
    ZrLanguageServer_LspContext_Free(state, context);
    ZrCore_GlobalState_Free(global);
}

static void test_hover_range_after_utf8_prefix_uses_utf16_columns(void) {
    const TZrChar *content = "/* \xCE\xBB */ var target = 1;\nvar use = target;\n";
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    SZrState *state;
    SZrLspContext *context;
    SZrString *uri = ZR_NULL;
    SZrLspPosition hoverPosition;
    SZrLspHover *hover = ZR_NULL;
    TZrBool resolved;

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: Hover range after UTF-8 prefix could not create VM state\n");
        g_failures++;
        return;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);
    context = test_open_document(state, "file:///tmp/zr_lsp_position_hover.zr", content, &uri);
    if (context == ZR_NULL) {
        printf("FAIL: Hover range after UTF-8 prefix could not open document\n");
        ZrCore_GlobalState_Free(global);
        g_failures++;
        return;
    }

    hoverPosition.line = 0;
    hoverPosition.character = 13;
    resolved = ZrLanguageServer_Lsp_GetHover(state, context, uri, hoverPosition, &hover);
    if (!resolved || hover == ZR_NULL ||
        !test_lsp_range_equals(hover->range, 0, 12, 0, 18)) {
        printf("FAIL: Hover range after UTF-8 prefix expected 0:12-0:18 but got resolved=%d hover=%p",
               (int)resolved,
               (void *)hover);
        if (hover != ZR_NULL) {
            printf(" range=%d:%d-%d:%d",
                   hover->range.start.line,
                   hover->range.start.character,
                   hover->range.end.line,
                   hover->range.end.character);
        }
        printf("\n");
        g_failures++;
    } else {
        printf("PASS: Hover range after UTF-8 prefix uses UTF-16 columns\n");
    }

    test_hover_free(state, hover);
    ZrLanguageServer_LspContext_Free(state, context);
    ZrCore_GlobalState_Free(global);
}

static void test_document_highlight_range_after_utf8_prefix_uses_utf16_columns(void) {
    const TZrChar *content = "/* \xCE\xBB */ var target = 1;\nvar use = target;\n";
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    SZrState *state;
    SZrLspContext *context;
    SZrString *uri = ZR_NULL;
    SZrLspPosition usagePosition;
    SZrArray highlights = {0};
    TZrBool resolved;

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: Document highlight range after UTF-8 prefix could not create VM state\n");
        g_failures++;
        return;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);
    context = test_open_document(state, "file:///tmp/zr_lsp_position_highlight.zr", content, &uri);
    if (context == ZR_NULL) {
        printf("FAIL: Document highlight range after UTF-8 prefix could not open document\n");
        ZrCore_GlobalState_Free(global);
        g_failures++;
        return;
    }

    usagePosition.line = 1;
    usagePosition.character = 11;
    ZrCore_Array_Init(state, &highlights, sizeof(SZrLspDocumentHighlight *), 1);
    resolved = ZrLanguageServer_Lsp_GetDocumentHighlights(state, context, uri, usagePosition, &highlights);

    if (!resolved ||
        !test_highlights_contain_range(&highlights, 0, 12, 0, 18) ||
        !test_highlights_contain_range(&highlights, 1, 10, 1, 16)) {
        SZrLspDocumentHighlight **firstPtr =
            highlights.length > 0 ? (SZrLspDocumentHighlight **)ZrCore_Array_Get(&highlights, 0) : ZR_NULL;
        SZrLspDocumentHighlight *first = firstPtr != ZR_NULL ? *firstPtr : ZR_NULL;
        printf("FAIL: Document highlight range after UTF-8 prefix expected declaration 0:12-0:18 and use 1:10-1:16 but got resolved=%d count=%llu",
               (int)resolved,
               (unsigned long long)highlights.length);
        if (first != ZR_NULL) {
            printf(" first=%d:%d-%d:%d",
                   first->range.start.line,
                   first->range.start.character,
                   first->range.end.line,
                   first->range.end.character);
        }
        printf("\n");
        g_failures++;
    } else {
        printf("PASS: Document highlight range after UTF-8 prefix uses UTF-16 columns\n");
    }

    ZrCore_Array_Free(state, &highlights);
    ZrLanguageServer_LspContext_Free(state, context);
    ZrCore_GlobalState_Free(global);
}

static void test_prepare_rename_range_after_utf8_prefix_uses_utf16_columns(void) {
    const TZrChar *content = "/* \xCE\xBB */ var target = 1;\nvar use = target;\n";
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    SZrState *state;
    SZrLspContext *context;
    SZrString *uri = ZR_NULL;
    SZrLspPosition targetPosition;
    SZrLspRange range;
    SZrString *placeholder = ZR_NULL;
    TZrBool resolved;

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: Prepare rename range after UTF-8 prefix could not create VM state\n");
        g_failures++;
        return;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);
    context = test_open_document(state, "file:///tmp/zr_lsp_position_prepare_rename.zr", content, &uri);
    if (context == ZR_NULL) {
        printf("FAIL: Prepare rename range after UTF-8 prefix could not open document\n");
        ZrCore_GlobalState_Free(global);
        g_failures++;
        return;
    }

    targetPosition.line = 0;
    targetPosition.character = 13;
    resolved = ZrLanguageServer_Lsp_PrepareRename(state, context, uri, targetPosition, &range, &placeholder);
    if (!resolved || placeholder == ZR_NULL ||
        !test_lsp_range_equals(range, 0, 12, 0, 18)) {
        printf("FAIL: Prepare rename range after UTF-8 prefix expected 0:12-0:18 but got resolved=%d placeholder=%p range=%d:%d-%d:%d\n",
               (int)resolved,
               (void *)placeholder,
               range.start.line,
               range.start.character,
               range.end.line,
               range.end.character);
        g_failures++;
    } else {
        printf("PASS: Prepare rename range after UTF-8 prefix uses UTF-16 columns\n");
    }

    ZrLanguageServer_LspContext_Free(state, context);
    ZrCore_GlobalState_Free(global);
}

static void test_rename_locations_after_utf8_prefix_use_utf16_columns(void) {
    const TZrChar *content = "/* \xCE\xBB */ var target = 1;\nvar use = target;\n";
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    SZrState *state;
    SZrLspContext *context;
    SZrString *uri = ZR_NULL;
    SZrString *newName;
    SZrLspPosition usagePosition;
    SZrArray locations = {0};
    TZrBool resolved;

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: Rename locations after UTF-8 prefix could not create VM state\n");
        g_failures++;
        return;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);
    context = test_open_document(state, "file:///tmp/zr_lsp_position_rename.zr", content, &uri);
    newName = ZrCore_String_Create(state, "renamed", 7);
    if (context == ZR_NULL || newName == ZR_NULL) {
        printf("FAIL: Rename locations after UTF-8 prefix could not open document\n");
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        ZrCore_GlobalState_Free(global);
        g_failures++;
        return;
    }

    usagePosition.line = 1;
    usagePosition.character = 11;
    ZrCore_Array_Init(state, &locations, sizeof(SZrLspLocation *), 1);
    resolved = ZrLanguageServer_Lsp_Rename(state, context, uri, usagePosition, newName, &locations);
    if (!resolved ||
        !test_locations_contain_range(&locations, 0, 12, 0, 18) ||
        !test_locations_contain_range(&locations, 1, 10, 1, 16)) {
        SZrLspLocation **firstPtr =
            locations.length > 0 ? (SZrLspLocation **)ZrCore_Array_Get(&locations, 0) : ZR_NULL;
        SZrLspLocation *first = firstPtr != ZR_NULL ? *firstPtr : ZR_NULL;
        printf("FAIL: Rename locations after UTF-8 prefix expected declaration 0:12-0:18 and use 1:10-1:16 but got resolved=%d count=%llu",
               (int)resolved,
               (unsigned long long)locations.length);
        if (first != ZR_NULL) {
            printf(" first=%d:%d-%d:%d",
                   first->range.start.line,
                   first->range.start.character,
                   first->range.end.line,
                   first->range.end.character);
        }
        printf("\n");
        g_failures++;
    } else {
        printf("PASS: Rename locations after UTF-8 prefix use UTF-16 columns\n");
    }

    ZrCore_Array_Free(state, &locations);
    ZrLanguageServer_LspContext_Free(state, context);
    ZrCore_GlobalState_Free(global);
}

static void test_diagnostic_range_after_utf8_prefix_uses_utf16_columns(void) {
    const TZrChar *content = "/* \xCE\xBB */ var x = ;\n";
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    SZrState *state;
    SZrLspContext *context;
    SZrString *uri = ZR_NULL;
    SZrArray diagnostics = {0};
    TZrBool resolved;

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: Diagnostic range after UTF-8 prefix could not create VM state\n");
        g_failures++;
        return;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);
    context = test_open_document(state, "file:///tmp/zr_lsp_position_diagnostic.zr", content, &uri);
    if (context == ZR_NULL) {
        printf("FAIL: Diagnostic range after UTF-8 prefix could not open document\n");
        ZrCore_GlobalState_Free(global);
        g_failures++;
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 1);
    resolved = ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics);
    if (!resolved || !test_diagnostics_contain_range(&diagnostics, 0, 16, 0, 17)) {
        SZrLspDiagnostic **firstPtr =
            diagnostics.length > 0 ? (SZrLspDiagnostic **)ZrCore_Array_Get(&diagnostics, 0) : ZR_NULL;
        SZrLspDiagnostic *first = firstPtr != ZR_NULL ? *firstPtr : ZR_NULL;
        printf("FAIL: Diagnostic range after UTF-8 prefix expected 0:16-0:17 but got resolved=%d count=%llu",
               (int)resolved,
               (unsigned long long)diagnostics.length);
        if (first != ZR_NULL) {
            printf(" first=%d:%d-%d:%d",
                   first->range.start.line,
                   first->range.start.character,
                   first->range.end.line,
                   first->range.end.character);
        }
        printf("\n");
        g_failures++;
    } else {
        printf("PASS: Diagnostic range after UTF-8 prefix uses UTF-16 columns\n");
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    ZrCore_GlobalState_Free(global);
}

static void test_symbol_ranges_after_utf8_prefix_use_utf16_columns(void) {
    const TZrChar *content = "/* \xCE\xBB */ target() { return 1; }\nvar next = 2;\n";
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    SZrState *state;
    SZrLspContext *context;
    SZrString *uri = ZR_NULL;
    SZrString *query;
    SZrArray documentSymbols = {0};
    SZrArray workspaceSymbols = {0};
    TZrBool resolvedDocument;
    TZrBool resolvedWorkspace;

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: Symbol ranges after UTF-8 prefix could not create VM state\n");
        g_failures++;
        return;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);
    context = test_open_document(state, "file:///tmp/zr_lsp_position_symbol.zr", content, &uri);
    query = ZrCore_String_Create(state, "target", 6);
    if (context == ZR_NULL || query == ZR_NULL) {
        printf("FAIL: Symbol ranges after UTF-8 prefix could not open document\n");
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        ZrCore_GlobalState_Free(global);
        g_failures++;
        return;
    }

    ZrCore_Array_Init(state, &documentSymbols, sizeof(SZrLspSymbolInformation *), 1);
    ZrCore_Array_Init(state, &workspaceSymbols, sizeof(SZrLspSymbolInformation *), 1);
    resolvedDocument = ZrLanguageServer_Lsp_GetDocumentSymbols(state, context, uri, &documentSymbols);
    resolvedWorkspace = ZrLanguageServer_Lsp_GetWorkspaceSymbols(state, context, query, &workspaceSymbols);
    if (!resolvedDocument ||
        !resolvedWorkspace ||
        !test_symbols_contain_range(&documentSymbols, 0, 8, 0, 30) ||
        !test_symbols_contain_range(&workspaceSymbols, 0, 8, 0, 30)) {
        SZrLspSymbolInformation **firstDocumentPtr =
            documentSymbols.length > 0 ? (SZrLspSymbolInformation **)ZrCore_Array_Get(&documentSymbols, 0) : ZR_NULL;
        SZrLspSymbolInformation *firstDocument = firstDocumentPtr != ZR_NULL ? *firstDocumentPtr : ZR_NULL;
        SZrLspSymbolInformation **firstWorkspacePtr =
            workspaceSymbols.length > 0 ? (SZrLspSymbolInformation **)ZrCore_Array_Get(&workspaceSymbols, 0) : ZR_NULL;
        SZrLspSymbolInformation *firstWorkspace = firstWorkspacePtr != ZR_NULL ? *firstWorkspacePtr : ZR_NULL;
        printf("FAIL: Symbol ranges after UTF-8 prefix expected document/workspace 0:8-0:30 but got document=%d/%llu workspace=%d/%llu",
               (int)resolvedDocument,
               (unsigned long long)documentSymbols.length,
               (int)resolvedWorkspace,
               (unsigned long long)workspaceSymbols.length);
        if (firstDocument != ZR_NULL) {
            printf(" documentFirst=%d:%d-%d:%d",
                   firstDocument->location.range.start.line,
                   firstDocument->location.range.start.character,
                   firstDocument->location.range.end.line,
                   firstDocument->location.range.end.character);
        }
        if (firstWorkspace != ZR_NULL) {
            printf(" workspaceFirst=%d:%d-%d:%d",
                   firstWorkspace->location.range.start.line,
                   firstWorkspace->location.range.start.character,
                   firstWorkspace->location.range.end.line,
                   firstWorkspace->location.range.end.character);
        }
        printf("\n");
        g_failures++;
    } else {
        printf("PASS: Symbol ranges after UTF-8 prefix use UTF-16 columns\n");
    }

    ZrCore_Array_Free(state, &documentSymbols);
    ZrCore_Array_Free(state, &workspaceSymbols);
    ZrLanguageServer_LspContext_Free(state, context);
    ZrCore_GlobalState_Free(global);
}

static void test_inlay_hint_after_utf8_prefix_uses_utf16_columns(void) {
    const TZrChar *content = "/* \xCE\xBB */ var target = 1;\n";
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    SZrState *state;
    SZrLspContext *context;
    SZrString *uri = ZR_NULL;
    SZrLspRange range;
    SZrArray hints = {0};
    TZrBool resolved;

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: Inlay hint after UTF-8 prefix could not create VM state\n");
        g_failures++;
        return;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);
    context = test_open_document(state, "file:///tmp/zr_lsp_position_inlay.zr", content, &uri);
    if (context == ZR_NULL) {
        printf("FAIL: Inlay hint after UTF-8 prefix could not open document\n");
        ZrCore_GlobalState_Free(global);
        g_failures++;
        return;
    }

    range.start.line = 0;
    range.start.character = 0;
    range.end.line = 1;
    range.end.character = 0;

    ZrCore_Array_Init(state, &hints, sizeof(SZrLspInlayHint *), 4);
    resolved = ZrLanguageServer_Lsp_GetInlayHints(state, context, uri, range, &hints);
    if (!resolved || !test_inlay_hints_contain_position_and_label(&hints, 0, 18, ": int")) {
        SZrLspInlayHint **firstPtr =
            hints.length > 0 ? (SZrLspInlayHint **)ZrCore_Array_Get(&hints, 0) : ZR_NULL;
        SZrLspInlayHint *first = firstPtr != ZR_NULL ? *firstPtr : ZR_NULL;
        printf("FAIL: Inlay hint after UTF-8 prefix expected position 0:18 with ': int' but got resolved=%d count=%llu",
               (int)resolved,
               (unsigned long long)hints.length);
        if (first != ZR_NULL) {
            printf(" first=%d:%d label=%s",
                   first->position.line,
                   first->position.character,
                   test_string_ptr(first->label) != ZR_NULL ? test_string_ptr(first->label) : "<null>");
        }
        printf("\n");
        g_failures++;
    } else {
        printf("PASS: Inlay hint after UTF-8 prefix uses UTF-16 columns\n");
    }

    ZrLanguageServer_Lsp_FreeInlayHints(state, &hints);
    ZrLanguageServer_LspContext_Free(state, context);
    ZrCore_GlobalState_Free(global);
}

static void test_semantic_token_symbol_after_utf8_prefix_uses_utf16_columns(void) {
    const TZrChar *content = "/* \xCE\xBB */ target() { return 1; }\n";
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    SZrState *state;
    SZrLspContext *context;
    SZrString *uri = ZR_NULL;
    SZrArray tokens = {0};
    TZrBool resolved;

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: Semantic token after UTF-8 prefix could not create VM state\n");
        g_failures++;
        return;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);
    context = test_open_document(state, "file:///tmp/zr_lsp_position_semantic_tokens.zr", content, &uri);
    if (context == ZR_NULL) {
        printf("FAIL: Semantic token after UTF-8 prefix could not open document\n");
        ZrCore_GlobalState_Free(global);
        g_failures++;
        return;
    }

    ZrCore_Array_Init(state, &tokens, sizeof(TZrUInt32), 16);
    resolved = ZrLanguageServer_Lsp_GetSemanticTokens(state, context, uri, &tokens);
    if (!resolved || !test_semantic_tokens_contain(&tokens, 0, 8, 6, "function")) {
        printf("FAIL: Semantic token after UTF-8 prefix expected function token 0:8 length 6 but got resolved=%d count=%llu\n",
               (int)resolved,
               (unsigned long long)tokens.length);
        g_failures++;
    } else {
        printf("PASS: Semantic token after UTF-8 prefix uses UTF-16 columns\n");
    }

    ZrCore_Array_Free(state, &tokens);
    ZrLanguageServer_LspContext_Free(state, context);
    ZrCore_GlobalState_Free(global);
}

static void test_semantic_token_text_scan_after_utf8_prefix_uses_utf16_columns(void) {
    const TZrChar *content = "/* \xCE\xBB */ var system = %import(\"zr.system\");\n";
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    SZrState *state;
    SZrLspContext *context;
    SZrString *uri = ZR_NULL;
    SZrArray tokens = {0};
    TZrBool resolved;

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: Text-scan semantic token after UTF-8 prefix could not create VM state\n");
        g_failures++;
        return;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);
    context = test_open_document(state, "file:///tmp/zr_lsp_position_semantic_tokens_scan.zr", content, &uri);
    if (context == ZR_NULL) {
        printf("FAIL: Text-scan semantic token after UTF-8 prefix could not open document\n");
        ZrCore_GlobalState_Free(global);
        g_failures++;
        return;
    }

    ZrCore_Array_Init(state, &tokens, sizeof(TZrUInt32), 16);
    resolved = ZrLanguageServer_Lsp_GetSemanticTokens(state, context, uri, &tokens);
    if (!resolved || !test_semantic_tokens_contain(&tokens, 0, 21, 7, "keyword")) {
        printf("FAIL: Text-scan semantic token after UTF-8 prefix expected keyword token 0:21 length 7 but got resolved=%d count=%llu\n",
               (int)resolved,
               (unsigned long long)tokens.length);
        g_failures++;
    } else {
        printf("PASS: Text-scan semantic token after UTF-8 prefix uses UTF-16 columns\n");
    }

    ZrCore_Array_Free(state, &tokens);
    ZrLanguageServer_LspContext_Free(state, context);
    ZrCore_GlobalState_Free(global);
}

int main(void) {
    printf("==========\n");
    printf("Language Server - LSP Position Mapping Tests\n");
    printf("==========\n\n");

    test_bmp_utf8_before_cursor_maps_from_utf16_character();
    test_non_bmp_utf8_before_cursor_counts_as_two_utf16_units();
    test_chinese_identifier_maps_from_utf16_character();
    test_mixed_ascii_and_emoji_maps_inside_line();
    test_crlf_line_start_maps_after_lf();
    test_tab_counts_as_one_utf16_unit();
    test_eof_without_newline_clamps_to_content_end();
    test_offset_to_position_roundtrip_counts_utf16_units();
    test_file_position_uses_parser_byte_column_after_utf8();
    test_lsp_position_before_negative_line_clamps_to_file_start();
    test_lsp_position_beyond_line_clamps_column_to_line_end();
    test_lsp_position_beyond_file_clamps_to_eof_position();
    test_definition_range_after_utf8_prefix_uses_utf16_columns();
    test_hover_range_after_utf8_prefix_uses_utf16_columns();
    test_document_highlight_range_after_utf8_prefix_uses_utf16_columns();
    test_prepare_rename_range_after_utf8_prefix_uses_utf16_columns();
    test_rename_locations_after_utf8_prefix_use_utf16_columns();
    test_diagnostic_range_after_utf8_prefix_uses_utf16_columns();
    test_symbol_ranges_after_utf8_prefix_use_utf16_columns();
    test_inlay_hint_after_utf8_prefix_uses_utf16_columns();
    test_semantic_token_symbol_after_utf8_prefix_uses_utf16_columns();
    test_semantic_token_text_scan_after_utf8_prefix_uses_utf16_columns();

    printf("\n==========\n");
    if (g_failures == 0) {
        printf("All LSP Position Mapping Tests Completed\n");
        printf("==========\n");
        return 0;
    }

    printf("%d LSP Position Mapping Test(s) Failed\n", g_failures);
    printf("==========\n");
    return 1;
}
