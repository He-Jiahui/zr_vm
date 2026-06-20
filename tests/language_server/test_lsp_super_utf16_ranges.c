//
// Focused super-constructor LSP range regressions for UTF-16 columns.
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

static TZrBool locations_contain_start(SZrArray *locations, TZrInt32 line, TZrInt32 character) {
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

static TZrBool highlights_contain_start(SZrArray *highlights, TZrInt32 line, TZrInt32 character) {
    if (highlights == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < highlights->length; index++) {
        SZrLspDocumentHighlight **highlightPtr =
            (SZrLspDocumentHighlight **)ZrCore_Array_Get(highlights, index);
        SZrLspDocumentHighlight *highlight = highlightPtr != ZR_NULL ? *highlightPtr : ZR_NULL;
        if (highlight != ZR_NULL &&
            highlight->range.start.line == line &&
            highlight->range.start.character == character) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void describe_first_location(SZrArray *locations) {
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

static void describe_first_highlight(SZrArray *highlights) {
    if (highlights != ZR_NULL && highlights->length > 0) {
        SZrLspDocumentHighlight **highlightPtr =
            (SZrLspDocumentHighlight **)ZrCore_Array_Get(highlights, 0);
        SZrLspDocumentHighlight *highlight = highlightPtr != ZR_NULL ? *highlightPtr : ZR_NULL;
        if (highlight != ZR_NULL) {
            printf(" first=%d:%d-%d:%d",
                   highlight->range.start.line,
                   highlight->range.start.character,
                   highlight->range.end.line,
                   highlight->range.end.character);
        }
    }
}

static TZrBool test_super_constructor_after_utf8_prefix_uses_utf16_columns(SZrState *state) {
    const TZrChar *content =
        "class BaseHero {\n"
        "    /* \xCE\xBB */ pub @constructor(origin: int) {\n"
        "    }\n"
        "}\n"
        "class BossHero: BaseHero {\n"
        "    /* \xCE\xBB */ pub @constructor(seed: int) super(seed) {\n"
        "    }\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition superCallPosition = {5, 46};
    SZrArray definitions = {0};
    SZrArray references = {0};
    SZrArray highlights = {0};
    TZrBool passed = ZR_TRUE;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, "file:///super_utf16_ranges.zr", strlen("file:///super_utf16_ranges.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        printf("FAIL: Super UTF-16 ranges could not open fixture\n");
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 1);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, superCallPosition, &definitions) ||
        !locations_contain_start(&definitions, 1, 16)) {
        printf("FAIL: Super definition expected base constructor UTF-16 start 1:16 but got count=%llu",
               (unsigned long long)definitions.length);
        describe_first_location(&definitions);
        printf("\n");
        passed = ZR_FALSE;
    }
    ZrCore_Array_Free(state, &definitions);

    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_FindReferences(state, context, uri, superCallPosition, ZR_TRUE, &references) ||
        !locations_contain_start(&references, 1, 16) ||
        !locations_contain_start(&references, 5, 40)) {
        printf("FAIL: Super references expected UTF-16 starts 1:16 and 5:40 but got count=%llu",
               (unsigned long long)references.length);
        describe_first_location(&references);
        printf("\n");
        passed = ZR_FALSE;
    }
    ZrCore_Array_Free(state, &references);

    ZrCore_Array_Init(state, &highlights, sizeof(SZrLspDocumentHighlight *), 4);
    if (!ZrLanguageServer_Lsp_GetDocumentHighlights(state, context, uri, superCallPosition, &highlights) ||
        !highlights_contain_start(&highlights, 1, 16) ||
        !highlights_contain_start(&highlights, 5, 40)) {
        printf("FAIL: Super highlights expected UTF-16 starts 1:16 and 5:40 but got count=%llu",
               (unsigned long long)highlights.length);
        describe_first_highlight(&highlights);
        printf("\n");
        passed = ZR_FALSE;
    }
    ZrCore_Array_Free(state, &highlights);

    ZrLanguageServer_LspContext_Free(state, context);
    if (passed) {
        printf("PASS: Super constructor navigation after UTF-8 prefix uses UTF-16 columns\n");
    }
    return passed;
}

int main(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    TZrBool passed;

    printf("==========\n");
    printf("Language Server - Super UTF-16 Range Tests\n");
    printf("==========\n\n");

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: Super UTF-16 range tests could not create VM state\n");
        return 1;
    }

    ZrCore_GlobalState_InitRegistry(global->mainThreadState, global);
    passed = test_super_constructor_after_utf8_prefix_uses_utf16_columns(global->mainThreadState);
    ZrCore_GlobalState_Free(global);

    if (!passed) {
        printf("\nFAILED: Super UTF-16 range tests failed\n");
        return 1;
    }

    printf("\nPASSED: Super UTF-16 range tests\n");
    return 0;
}
