//
// Focused token-metadata LSP range regressions for UTF-16 columns.
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

static TZrBool test_meta_method_hover_after_utf8_prefix_uses_utf16_columns(SZrState *state) {
    const TZrChar *content =
        "class Box {\n"
        "    /* \xCE\xBB */ pub @constructor(value: int) {\n"
        "    }\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition constructorPosition = {1, 19};
    SZrLspHover *hover = ZR_NULL;
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state,
                               "file:///token_metadata_utf16_ranges.zr",
                               strlen("file:///token_metadata_utf16_ranges.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !ZrLanguageServer_Lsp_GetHover(state, context, uri, constructorPosition, &hover) ||
        hover == ZR_NULL) {
        printf("FAIL: Token metadata UTF-16 ranges could not open fixture or collect hover\n");
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        return ZR_FALSE;
    }

    passed = hover->range.start.line == 1 &&
             hover->range.start.character == 16 &&
             hover->range.end.line == 1 &&
             hover->range.end.character == 28;
    if (passed) {
        printf("PASS: Meta-method hover after UTF-8 prefix uses UTF-16 columns\n");
    } else {
        printf("FAIL: Meta-method hover expected UTF-16 range 1:16-1:28 but got %d:%d-%d:%d\n",
               hover->range.start.line,
               hover->range.start.character,
               hover->range.end.line,
               hover->range.end.character);
    }

    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

int main(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    TZrBool passed;

    printf("==========\n");
    printf("Language Server - Token Metadata UTF-16 Range Tests\n");
    printf("==========\n\n");

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: Token metadata UTF-16 range tests could not create VM state\n");
        return 1;
    }

    ZrCore_GlobalState_InitRegistry(global->mainThreadState, global);
    passed = test_meta_method_hover_after_utf8_prefix_uses_utf16_columns(global->mainThreadState);
    ZrCore_GlobalState_Free(global);

    if (!passed) {
        printf("\nFAILED: Token metadata UTF-16 range tests failed\n");
        return 1;
    }

    printf("\nPASSED: Token metadata UTF-16 range tests\n");
    return 0;
}
