//
// Focused LSP reaching-definition navigation regression tests.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server.h"

typedef struct SZrTestTimer {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

static int g_failures = 0;

#define TEST_START(summary) do { \
    timer.startTime = clock(); \
    printf("Unit Test - %s\n", summary); \
    fflush(stdout); \
} while (0)

#define TEST_PASS(timerValue, summary) do { \
    (timerValue).endTime = clock(); \
    double elapsed = ((double)((timerValue).endTime - (timerValue).startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary); \
    fflush(stdout); \
} while (0)

#define TEST_FAIL(timerValue, summary, reason) do { \
    (timerValue).endTime = clock(); \
    double elapsed = ((double)((timerValue).endTime - (timerValue).startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Fail - Cost Time:%.3fms - %s:\n %s\n", elapsed, summary, reason); \
    fflush(stdout); \
    g_failures++; \
} while (0)

static TZrPtr test_allocator(TZrPtr userData,
                             TZrPtr pointer,
                             TZrSize originalSize,
                             TZrSize newSize,
                             TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL &&
            (TZrPtr)pointer >= (TZrPtr)0x1000 &&
            originalSize > 0 &&
            originalSize < 1024 * 1024 * 1024) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }
    if ((TZrPtr)pointer >= (TZrPtr)0x1000 &&
        originalSize > 0 &&
        originalSize < 1024 * 1024 * 1024) {
        return realloc(pointer, newSize);
    }
    return malloc(newSize);
}

static TZrBool lsp_find_position_for_substring(const TZrChar *content,
                                               const TZrChar *needle,
                                               TZrSize occurrence,
                                               TZrInt32 extraCharacterOffset,
                                               SZrLspPosition *outPosition) {
    const TZrChar *match;
    TZrSize currentOccurrence = 0;
    TZrInt32 line = 0;
    TZrInt32 character = 0;
    const TZrChar *cursor = content;

    if (content == ZR_NULL || needle == ZR_NULL || outPosition == ZR_NULL) {
        return ZR_FALSE;
    }

    match = strstr(content, needle);
    while (match != ZR_NULL && currentOccurrence < occurrence) {
        match = strstr(match + 1, needle);
        currentOccurrence++;
    }

    if (match == ZR_NULL) {
        return ZR_FALSE;
    }

    while (cursor < match) {
        if (*cursor == '\n') {
            line++;
            character = 0;
        } else {
            character++;
        }
        cursor++;
    }

    outPosition->line = line;
    outPosition->character = character + extraCharacterOffset;
    return ZR_TRUE;
}

static TZrBool lsp_range_equals(SZrLspRange range,
                                TZrInt32 startLine,
                                TZrInt32 startCharacter,
                                TZrInt32 endLine,
                                TZrInt32 endCharacter) {
    return range.start.line == startLine &&
           range.start.character == startCharacter &&
           range.end.line == endLine &&
           range.end.character == endCharacter;
}

static TZrBool location_array_contains_range(SZrArray *locations,
                                             TZrInt32 startLine,
                                             TZrInt32 startCharacter,
                                             TZrInt32 endLine,
                                             TZrInt32 endCharacter) {
    for (TZrSize index = 0; locations != ZR_NULL && index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        if (locationPtr != ZR_NULL && *locationPtr != ZR_NULL &&
            lsp_range_equals((*locationPtr)->range, startLine, startCharacter, endLine, endCharacter)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void test_lsp_definition_prefers_reaching_write_for_read(SZrState *state) {
    const TZrChar *summary = "LSP Definition Prefers Reaching Write For Read";
    const TZrChar *uriText = "file:///reaching_definition_navigation.zr";
    const TZrChar *content =
        "func choose(): int {\n"
        "    var seed = 1;\n"
        "    seed = 3;\n"
        "    return seed;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition readPosition;
    SZrArray definitions;
    TZrChar reason[256];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "return seed", 0, 7, &readPosition)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare reaching-definition navigation fixture");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 1);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, readPosition, &definitions) ||
        !location_array_contains_range(&definitions, 2, 4, 2, 8) ||
        location_array_contains_range(&definitions, 1, 8, 1, 12)) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected definition at reaching write range 2:4-2:8 and not declaration range; definitions=%llu",
                 (unsigned long long)definitions.length);
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrCore_Array_Free(state, &definitions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_lsp_definition_returns_branch_writes_for_divergent_branch_writes(SZrState *state) {
    const TZrChar *summary = "LSP Definition Returns Branch Writes For Divergent Branch Writes";
    const TZrChar *uriText = "file:///reaching_definition_branch_join.zr";
    const TZrChar *content =
        "func choose(flag: bool): int {\n"
        "    var seed: int;\n"
        "    if (flag) {\n"
        "        seed = 1;\n"
        "    } else {\n"
        "        seed = 2;\n"
        "    }\n"
        "    return seed;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition readPosition;
    SZrArray definitions;
    TZrChar reason[256];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "return seed", 0, 7, &readPosition)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare branch-join reaching-definition navigation fixture");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 1);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, readPosition, &definitions) ||
        definitions.length != 2 ||
        location_array_contains_range(&definitions, 1, 8, 1, 12) ||
        !location_array_contains_range(&definitions, 3, 8, 3, 12) ||
        !location_array_contains_range(&definitions, 5, 8, 5, 12)) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected two branch write ranges and no declaration range; definitions=%llu",
                 (unsigned long long)definitions.length);
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrCore_Array_Free(state, &definitions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;

    printf("ZR VM LSP Reaching Definition Navigation Tests\n");
    printf("==============================================\n\n");

    memset(&callbacks, 0, sizeof(callbacks));
    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL) {
        printf("Failed to create global state\n");
        return 1;
    }

    state = global->mainThreadState;
    if (state == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        printf("Failed to get main state\n");
        return 1;
    }
    ZrCore_GlobalState_InitRegistry(state, global);

    test_lsp_definition_prefers_reaching_write_for_read(state);
    test_lsp_definition_returns_branch_writes_for_divergent_branch_writes(state);

    ZrCore_GlobalState_Free(global);

    printf("\n==========\n");
    printf("All LSP Reaching Definition Navigation Tests Completed\n");
    printf("==========\n");
    return g_failures == 0 ? 0 : 1;
}
