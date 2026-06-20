//
// Focused code-lens LSP range regressions for UTF-16 columns.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server.h"

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

static const TZrChar *test_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

static TZrBool test_lens_matches(SZrArray *lenses,
                                 const TZrChar *title,
                                 TZrInt32 line,
                                 TZrInt32 character) {
    if (lenses == ZR_NULL || title == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < lenses->length; index++) {
        SZrLspCodeLens **lensPtr = (SZrLspCodeLens **)ZrCore_Array_Get(lenses, index);
        SZrLspCodeLens *lens = lensPtr != ZR_NULL ? *lensPtr : ZR_NULL;
        const TZrChar *lensTitle = lens != ZR_NULL ? test_string_text(lens->commandTitle) : ZR_NULL;
        if (lensTitle != ZR_NULL &&
            strcmp(lensTitle, title) == 0 &&
            lens->range.start.line == line &&
            lens->range.start.character == character &&
            lens->hasPositionArgument &&
            lens->positionArgument.line == line &&
            lens->positionArgument.character == character) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void describe_first_lens(SZrArray *lenses) {
    if (lenses != ZR_NULL && lenses->length > 0) {
        SZrLspCodeLens **lensPtr = (SZrLspCodeLens **)ZrCore_Array_Get(lenses, 0);
        SZrLspCodeLens *lens = lensPtr != ZR_NULL ? *lensPtr : ZR_NULL;
        if (lens != ZR_NULL) {
            printf(" first=%d:%d pos=%d:%d title=%s",
                   lens->range.start.line,
                   lens->range.start.character,
                   lens->positionArgument.line,
                   lens->positionArgument.character,
                   test_string_text(lens->commandTitle));
        }
    }
}

static TZrBool test_code_lens_reference_count_after_utf8_prefix_uses_utf16_columns(SZrState *state) {
    const TZrChar *content =
        "/* \xCE\xBB */ func helper(value: int): int {\n"
        "    return value;\n"
        "}\n"
        "\n"
        "func first(value: int): int {\n"
        "    return helper(value);\n"
        "}\n"
        "\n"
        "func second(value: int): int {\n"
        "    return helper(value);\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrArray lenses = {0};
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, "file:///code_lens_utf16_ranges.zr", strlen("file:///code_lens_utf16_ranges.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !ZrLanguageServer_Lsp_GetCodeLens(state, context, uri, &lenses)) {
        printf("FAIL: CodeLens UTF-16 ranges could not open fixture or collect lenses\n");
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        return ZR_FALSE;
    }

    passed = test_lens_matches(&lenses, "2 references", 0, 13);
    if (!passed) {
        printf("FAIL: CodeLens reference count expected UTF-16 range/position start 0:13 but got count=%llu",
               (unsigned long long)lenses.length);
        describe_first_lens(&lenses);
        printf("\n");
    } else {
        printf("PASS: CodeLens reference count after UTF-8 prefix uses UTF-16 columns\n");
    }

    ZrLanguageServer_Lsp_FreeCodeLens(state, &lenses);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

int main(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    TZrBool passed;

    printf("==========\n");
    printf("Language Server - CodeLens UTF-16 Range Tests\n");
    printf("==========\n\n");

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: CodeLens UTF-16 range tests could not create VM state\n");
        return 1;
    }

    ZrCore_GlobalState_InitRegistry(global->mainThreadState, global);
    passed = test_code_lens_reference_count_after_utf8_prefix_uses_utf16_columns(global->mainThreadState);
    ZrCore_GlobalState_Free(global);

    if (!passed) {
        printf("\nFAILED: CodeLens UTF-16 range tests failed\n");
        return 1;
    }

    printf("\nPASSED: CodeLens UTF-16 range tests\n");
    return 0;
}
