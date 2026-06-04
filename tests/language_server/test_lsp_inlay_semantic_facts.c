//
// Focused LSP inlay hint semantic fact regression tests.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "interface/lsp_interface_internal.h"
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

static const TZrChar *test_string_ptr(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }
    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

static TZrBool inlay_hint_array_contains_label(SZrArray *hints, const TZrChar *label) {
    if (hints == ZR_NULL || label == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < hints->length; index++) {
        SZrLspInlayHint **hintPtr = (SZrLspInlayHint **)ZrCore_Array_Get(hints, index);
        if (hintPtr != ZR_NULL &&
            *hintPtr != ZR_NULL &&
            (*hintPtr)->label != ZR_NULL &&
            strcmp(test_string_ptr((*hintPtr)->label), label) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static SZrLspCompletionItem *completion_item_find_by_label(SZrArray *items, const TZrChar *label) {
    if (items == ZR_NULL || label == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < items->length; index++) {
        SZrLspCompletionItem **itemPtr = (SZrLspCompletionItem **)ZrCore_Array_Get(items, index);
        const TZrChar *itemLabel;
        if (itemPtr == ZR_NULL || *itemPtr == ZR_NULL || (*itemPtr)->label == ZR_NULL) {
            continue;
        }
        itemLabel = test_string_ptr((*itemPtr)->label);
        if (itemLabel != ZR_NULL && strcmp(itemLabel, label) == 0) {
            return *itemPtr;
        }
    }

    return ZR_NULL;
}

static const TZrChar *signature_parameter_documentation(SZrLspSignatureHelp *help, TZrSize parameterIndex) {
    SZrLspSignatureInformation **signaturePtr;
    SZrLspParameterInformation **parameterPtr;

    if (help == ZR_NULL || help->signatures.length == 0) {
        return ZR_NULL;
    }

    signaturePtr = (SZrLspSignatureInformation **)ZrCore_Array_Get(&help->signatures, 0);
    if (signaturePtr == ZR_NULL ||
        *signaturePtr == ZR_NULL ||
        parameterIndex >= (*signaturePtr)->parameters.length) {
        return ZR_NULL;
    }

    parameterPtr =
        (SZrLspParameterInformation **)ZrCore_Array_Get(&(*signaturePtr)->parameters, parameterIndex);
    if (parameterPtr == ZR_NULL || *parameterPtr == ZR_NULL || (*parameterPtr)->documentation == ZR_NULL) {
        return ZR_NULL;
    }

    return test_string_ptr((*parameterPtr)->documentation);
}

static TZrBool find_position_for_substring(const TZrChar *content,
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

static void test_inlay_hint_uses_initializer_numeric_fact(SZrState *state) {
    const TZrChar *summary = "LSP Inlay Hint Uses Initializer Numeric Fact";
    const TZrChar *uriText = "file:///inlay_initializer_numeric_fact.zr";
    const TZrChar *content =
        "func calc(): void {\n"
        "    var sum = 1 + 2;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspRange range;
    SZrArray hints;
    TZrChar reason[512];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare inlay hint fixture");
        return;
    }

    range.start.line = 0;
    range.start.character = 0;
    range.end.line = 4;
    range.end.character = 0;
    ZrCore_Array_Init(state, &hints, sizeof(SZrLspInlayHint *), 4);
    if (!ZrLanguageServer_Lsp_GetInlayHints(state, context, uri, range, &hints) ||
        !inlay_hint_array_contains_label(&hints, ": int, range 3..3")) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected local type inlay hint to include initializer numeric range from semantic facts; hintCount=%llu",
                 (unsigned long long)hints.length);
        ZrLanguageServer_Lsp_FreeInlayHints(state, &hints);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_Lsp_FreeInlayHints(state, &hints);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_completion_detail_uses_initializer_numeric_fact(SZrState *state) {
    const TZrChar *summary = "LSP Completion Detail Uses Initializer Numeric Fact";
    const TZrChar *uriText = "file:///completion_initializer_numeric_fact.zr";
    const TZrChar *content =
        "func calc(): int {\n"
        "    var sum = 1 + 2;\n"
        "    su\n"
        "    return sum;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrArray completions;
    SZrLspCompletionItem *sumItem;
    const TZrChar *detailText;
    TZrChar reason[768];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring(content, "su", 1, 2, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare completion semantic fact fixture");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, position, &completions)) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Completion request failed");
        return;
    }

    sumItem = completion_item_find_by_label(&completions, "sum");
    detailText = sumItem != ZR_NULL && sumItem->detail != ZR_NULL ? test_string_ptr(sumItem->detail) : ZR_NULL;
    if (sumItem == ZR_NULL ||
        detailText == ZR_NULL ||
        strstr(detailText, "int") == ZR_NULL ||
        strstr(detailText, "range 3..3") == ZR_NULL) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected sum completion detail to include inferred type and initializer numeric range; item=%p detail=%s count=%llu",
                 (void *)sumItem,
                 detailText != ZR_NULL ? detailText : "<null>",
                 (unsigned long long)completions.length);
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_completion_detail_uses_initializer_logical_fact(SZrState *state) {
    const TZrChar *summary = "LSP Completion Detail Uses Initializer Logical Fact";
    const TZrChar *uriText = "file:///completion_initializer_logical_fact.zr";
    const TZrChar *content =
        "func calc(): bool {\n"
        "    var const flag = true || false;\n"
        "    fl\n"
        "    return flag;\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrArray completions;
    SZrLspCompletionItem *flagItem;
    const TZrChar *detailText;
    TZrChar reason[768];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring(content, "fl", 1, 2, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare completion logical fact fixture");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, position, &completions)) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Completion request failed");
        return;
    }

    flagItem = completion_item_find_by_label(&completions, "flag");
    detailText = flagItem != ZR_NULL && flagItem->detail != ZR_NULL ? test_string_ptr(flagItem->detail) : ZR_NULL;
    if (flagItem == ZR_NULL ||
        detailText == ZR_NULL ||
        strstr(detailText, "bool") == ZR_NULL ||
        strstr(detailText, "logical true") == ZR_NULL ||
        strstr(detailText, "short-circuits") == ZR_NULL) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected flag completion detail to include inferred type and initializer logical fact; item=%p detail=%s count=%llu",
                 (void *)flagItem,
                 detailText != ZR_NULL ? detailText : "<null>",
                 (unsigned long long)completions.length);
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_completion_detail_uses_initializer_ownership_fact(SZrState *state) {
    const TZrChar *summary = "LSP Completion Detail Uses Initializer Ownership Fact";
    const TZrChar *uriText = "file:///completion_initializer_ownership_fact.zr";
    const TZrChar *content =
        "class Resource {\n"
        "}\n"
        "var owner: %unique Resource;\n"
        "var plainFromUnique: Resource = owner;\n"
        "pla\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrArray completions;
    SZrLspCompletionItem *plainItem;
    const TZrChar *detailText;
    TZrChar reason[1024];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring(content, "pla", 1, 3, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare completion ownership fact fixture");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, position, &completions)) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Completion request failed");
        return;
    }

    plainItem = completion_item_find_by_label(&completions, "plainFromUnique");
    detailText = plainItem != ZR_NULL && plainItem->detail != ZR_NULL ? test_string_ptr(plainItem->detail) : ZR_NULL;
    if (plainItem == ZR_NULL ||
        detailText == ZR_NULL ||
        strstr(detailText, "Resource") == ZR_NULL ||
        strstr(detailText, "ownership violation") == ZR_NULL ||
        strstr(detailText, "Owned value cannot flow into a plain GC value implicitly") == ZR_NULL) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected plainFromUnique completion detail to include ownership violation fact; item=%p detail=%s count=%llu",
                 (void *)plainItem,
                 detailText != ZR_NULL ? detailText : "<null>",
                 (unsigned long long)completions.length);
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_signature_help_parameter_docs_use_argument_semantic_facts(SZrState *state) {
    const TZrChar *summary = "LSP Signature Help Parameter Docs Use Argument Semantic Facts";
    const TZrChar *uriText = "file:///signature_argument_semantic_facts.zr";
    const TZrChar *content =
        "func pick(value: int, flag: bool): int {\n"
        "    return value;\n"
        "}\n"
        "func calc(): int {\n"
        "    return pick(1 + 2, true || false);\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition numericPosition;
    SZrLspPosition logicalPosition;
    SZrLspSignatureHelp *numericHelp = ZR_NULL;
    SZrLspSignatureHelp *logicalHelp = ZR_NULL;
    const TZrChar *numericDoc;
    const TZrChar *logicalDoc;
    TZrChar reason[1024];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring(content, "1 + 2", 0, 2, &numericPosition) ||
        !find_position_for_substring(content, "true || false", 0, 5, &logicalPosition)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare signature semantic fact fixture");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetSignatureHelp(state, context, uri, numericPosition, &numericHelp) ||
        !ZrLanguageServer_Lsp_GetSignatureHelp(state, context, uri, logicalPosition, &logicalHelp)) {
        if (numericHelp != ZR_NULL) {
            ZrLanguageServer_LspSignatureHelp_Free(state, numericHelp);
        }
        if (logicalHelp != ZR_NULL) {
            ZrLanguageServer_LspSignatureHelp_Free(state, logicalHelp);
        }
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Signature help request failed");
        return;
    }

    numericDoc = signature_parameter_documentation(numericHelp, 0);
    logicalDoc = signature_parameter_documentation(logicalHelp, 1);
    if (numericDoc == ZR_NULL ||
        strstr(numericDoc, "range 3..3") == ZR_NULL ||
        logicalDoc == ZR_NULL ||
        strstr(logicalDoc, "logical true") == ZR_NULL ||
        strstr(logicalDoc, "short-circuits") == ZR_NULL) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected signature parameter docs to include argument semantic facts; numeric=%s logical=%s",
                 numericDoc != ZR_NULL ? numericDoc : "<null>",
                 logicalDoc != ZR_NULL ? logicalDoc : "<null>");
        ZrLanguageServer_LspSignatureHelp_Free(state, numericHelp);
        ZrLanguageServer_LspSignatureHelp_Free(state, logicalHelp);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspSignatureHelp_Free(state, numericHelp);
    ZrLanguageServer_LspSignatureHelp_Free(state, logicalHelp);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

static void test_signature_help_parameter_docs_use_argument_ownership_fact(SZrState *state) {
    const TZrChar *summary = "LSP Signature Help Parameter Docs Use Argument Ownership Fact";
    const TZrChar *uriText = "file:///signature_argument_ownership_fact.zr";
    const TZrChar *content =
        "class Resource {\n"
        "}\n"
        "func observe(resource: %borrowed Resource): void {\n"
        "}\n"
        "func run(resource: %weak Resource): void {\n"
        "    observe(resource);\n"
        "}\n";
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspSignatureHelp *help = ZR_NULL;
    const TZrChar *doc;
    TZrChar reason[1024];

    TEST_START(summary);

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring(content,
                                     "observe(resource);",
                                     0,
                                     (TZrInt32)strlen("observe("),
                                     &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, summary, "Failed to prepare signature ownership fact fixture");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetSignatureHelp(state, context, uri, position, &help)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, "Signature help request failed");
        return;
    }

    doc = signature_parameter_documentation(help, 0);
    if (doc == ZR_NULL ||
        strstr(doc, "ownership violation") == ZR_NULL ||
        strstr(doc, "Weak value must be upgraded") == ZR_NULL) {
        snprintf(reason,
                 sizeof(reason),
                 "Expected signature parameter docs to include argument ownership fact; doc=%s",
                 doc != ZR_NULL ? doc : "<null>");
        ZrLanguageServer_LspSignatureHelp_Free(state, help);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, summary, reason);
        return;
    }

    ZrLanguageServer_LspSignatureHelp_Free(state, help);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, summary);
}

int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;

    printf("ZR VM LSP Inlay Semantic Fact Tests\n");
    printf("===================================\n\n");

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

    test_inlay_hint_uses_initializer_numeric_fact(state);
    test_completion_detail_uses_initializer_numeric_fact(state);
    test_completion_detail_uses_initializer_logical_fact(state);
    test_completion_detail_uses_initializer_ownership_fact(state);
    test_signature_help_parameter_docs_use_argument_semantic_facts(state);
    test_signature_help_parameter_docs_use_argument_ownership_fact(state);

    ZrCore_GlobalState_Free(global);

    printf("\n==========\n");
    printf("All LSP Inlay Semantic Fact Tests Completed\n");
    printf("==========\n");
    return g_failures == 0 ? 0 : 1;
}
