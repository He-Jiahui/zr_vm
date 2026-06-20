//
// Focused decorator-navigation LSP range regressions for UTF-16 columns.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server.h"

static TZrInt32 g_failures = 0;

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

static TZrUInt32 test_utf8_codepoint(const TZrChar *text, TZrSize length, TZrSize *index) {
    TZrUInt8 byte;
    TZrUInt32 codepoint;

    if (text == ZR_NULL || index == ZR_NULL || *index >= length) {
        return 0;
    }

    byte = (TZrUInt8)text[*index];
    if (byte < 0x80) {
        (*index)++;
        return byte;
    }

    if ((byte & 0xE0) == 0xC0 && *index + 1 < length) {
        codepoint = (TZrUInt32)(byte & 0x1F) << 6;
        codepoint |= (TZrUInt32)((TZrUInt8)text[*index + 1] & 0x3F);
        *index += 2;
        return codepoint;
    }

    if ((byte & 0xF0) == 0xE0 && *index + 2 < length) {
        codepoint = (TZrUInt32)(byte & 0x0F) << 12;
        codepoint |= (TZrUInt32)((TZrUInt8)text[*index + 1] & 0x3F) << 6;
        codepoint |= (TZrUInt32)((TZrUInt8)text[*index + 2] & 0x3F);
        *index += 3;
        return codepoint;
    }

    if ((byte & 0xF8) == 0xF0 && *index + 3 < length) {
        codepoint = (TZrUInt32)(byte & 0x07) << 18;
        codepoint |= (TZrUInt32)((TZrUInt8)text[*index + 1] & 0x3F) << 12;
        codepoint |= (TZrUInt32)((TZrUInt8)text[*index + 2] & 0x3F) << 6;
        codepoint |= (TZrUInt32)((TZrUInt8)text[*index + 3] & 0x3F);
        *index += 4;
        return codepoint;
    }

    (*index)++;
    return byte;
}

static TZrBool test_lsp_position_for_byte_offset(const TZrChar *content,
                                                 TZrSize byteOffset,
                                                 SZrLspPosition *outPosition) {
    TZrSize length;
    TZrSize index = 0;
    TZrInt32 line = 0;
    TZrInt32 character = 0;

    if (content == ZR_NULL || outPosition == ZR_NULL) {
        return ZR_FALSE;
    }

    length = strlen(content);
    if (byteOffset > length) {
        return ZR_FALSE;
    }

    while (index < byteOffset) {
        TZrSize previous = index;
        TZrUInt32 codepoint = test_utf8_codepoint(content, length, &index);
        if (index > byteOffset) {
            index = previous + 1;
            character++;
            continue;
        }
        if (codepoint == '\n') {
            line++;
            character = 0;
        } else {
            character += codepoint > 0xFFFF ? 2 : 1;
        }
    }

    outPosition->line = line;
    outPosition->character = character;
    return ZR_TRUE;
}

static TZrBool test_lsp_position_for_substring(const TZrChar *content,
                                               const TZrChar *substring,
                                               TZrSize extraByteOffset,
                                               SZrLspPosition *outPosition) {
    const TZrChar *match;

    if (content == ZR_NULL || substring == ZR_NULL || outPosition == ZR_NULL) {
        return ZR_FALSE;
    }

    match = strstr(content, substring);
    if (match == ZR_NULL) {
        return ZR_FALSE;
    }

    return test_lsp_position_for_byte_offset(content, (TZrSize)(match - content) + extraByteOffset, outPosition);
}

static TZrBool test_locations_contain_range_start(SZrArray *locations,
                                                  TZrInt32 line,
                                                  TZrInt32 character) {
    if (locations == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        SZrLspLocation *location = locationPtr != ZR_NULL ? *locationPtr : ZR_NULL;
        if (location != ZR_NULL &&
            location->range.start.line == line &&
            location->range.start.character == character) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void test_describe_first_location(SZrArray *locations) {
    if (locations != ZR_NULL && locations->length > 0) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, 0);
        SZrLspLocation *location = locationPtr != ZR_NULL ? *locationPtr : ZR_NULL;
        if (location != ZR_NULL) {
            printf(" first=%d:%d-%d:%d",
                   location->range.start.line,
                   location->range.start.character,
                   location->range.end.line,
                   location->range.end.character);
        }
    }
}

static void test_lsp_decorator_navigation_after_utf8_prefix_uses_utf16_columns(SZrState *state) {
    const TZrChar *content =
        "/* \xCE\xBB */ #singleton#\n"
        "/* \xCE\xBB */ class SingletonClass {\n"
        "    /* \xCE\xBB */ #trace#\n"
        "    /* \xCE\xBB */ pub run(): int {\n"
        "        return 1;\n"
        "    }\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition classDecoratorPosition;
    SZrLspPosition methodDecoratorPosition;
    SZrLspPosition classNamePosition;
    SZrLspPosition methodNamePosition;
    SZrLspPosition decoratorAstRangeStart;
    SZrLspPosition decoratorAstRangeEnd;
    SZrArray definitions = {0};
    SZrLspHover *hover = ZR_NULL;
    TZrBool passed = ZR_TRUE;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state,
                               "file:///decorator_utf16_ranges.zr",
                               strlen("file:///decorator_utf16_ranges.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        printf("FAIL: Decorator UTF-16 ranges could not open fixture\n");
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        g_failures++;
        return;
    }

    if (!test_lsp_position_for_substring(content, "#singleton#", 1, &classDecoratorPosition) ||
        !test_lsp_position_for_substring(content, "#trace#", 1, &methodDecoratorPosition) ||
        !test_lsp_position_for_substring(content, "SingletonClass", 0, &classNamePosition) ||
        !test_lsp_position_for_substring(content, "run(): int", 0, &methodNamePosition) ||
        !test_lsp_position_for_substring(content, "race#", 0, &decoratorAstRangeStart) ||
        !test_lsp_position_for_substring(content, "run(): int", 0, &decoratorAstRangeEnd)) {
        printf("FAIL: Decorator UTF-16 ranges could not compute expected fixture positions\n");
        ZrLanguageServer_LspContext_Free(state, context);
        g_failures++;
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 2);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, classDecoratorPosition, &definitions) ||
        !test_locations_contain_range_start(&definitions,
                                            classNamePosition.line,
                                            classNamePosition.character)) {
        printf("FAIL: Decorator class definition expected UTF-16 start %d:%d but got count=%llu",
               classNamePosition.line,
               classNamePosition.character,
               (unsigned long long)definitions.length);
        test_describe_first_location(&definitions);
        printf("\n");
        passed = ZR_FALSE;
    }
    ZrCore_Array_Free(state, &definitions);

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 2);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, methodDecoratorPosition, &definitions) ||
        !test_locations_contain_range_start(&definitions,
                                            methodNamePosition.line,
                                            methodNamePosition.character)) {
        printf("FAIL: Decorator method definition expected UTF-16 start %d:%d but got count=%llu",
               methodNamePosition.line,
               methodNamePosition.character,
               (unsigned long long)definitions.length);
        test_describe_first_location(&definitions);
        printf("\n");
        passed = ZR_FALSE;
    }
    ZrCore_Array_Free(state, &definitions);

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, methodDecoratorPosition, &hover) ||
        hover == ZR_NULL ||
        hover->range.start.line != decoratorAstRangeStart.line ||
        hover->range.start.character != decoratorAstRangeStart.character ||
        hover->range.end.line != decoratorAstRangeEnd.line ||
        hover->range.end.character != decoratorAstRangeEnd.character) {
        printf("FAIL: Decorator hover expected UTF-16 parser range %d:%d-%d:%d",
               decoratorAstRangeStart.line,
               decoratorAstRangeStart.character,
               decoratorAstRangeEnd.line,
               decoratorAstRangeEnd.character);
        if (hover != ZR_NULL) {
            printf(" but got %d:%d-%d:%d",
                   hover->range.start.line,
                   hover->range.start.character,
                   hover->range.end.line,
                   hover->range.end.character);
        }
        printf("\n");
        passed = ZR_FALSE;
    }

    ZrLanguageServer_LspContext_Free(state, context);

    if (!passed) {
        g_failures++;
        return;
    }

    printf("PASS: Decorator navigation after UTF-8 prefix uses UTF-16 columns\n");
}

int main(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;

    printf("==========\n");
    printf("Language Server - Decorator UTF-16 Range Tests\n");
    printf("==========\n\n");

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: Decorator UTF-16 range tests could not create VM state\n");
        return 1;
    }

    ZrCore_GlobalState_InitRegistry(global->mainThreadState, global);
    test_lsp_decorator_navigation_after_utf8_prefix_uses_utf16_columns(global->mainThreadState);
    ZrCore_GlobalState_Free(global);

    if (g_failures != 0) {
        printf("\nFAILED: %d decorator UTF-16 range test(s) failed\n", g_failures);
        return 1;
    }

    printf("\nPASSED: decorator UTF-16 range tests\n");
    return 0;
}
