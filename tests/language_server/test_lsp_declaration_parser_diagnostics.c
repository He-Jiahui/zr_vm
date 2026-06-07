//
// Focused LSP declaration parser diagnostic regression tests.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    printf("Pass - Cost Time:%.3fms - %s\n", \
           ((double)((timerValue).endTime - (timerValue).startTime) / CLOCKS_PER_SEC) * 1000.0, \
           summary); \
    fflush(stdout); \
} while (0)

#define TEST_FAIL(timerValue, summary, reason) do { \
    (timerValue).endTime = clock(); \
    printf("Fail - Cost Time:%.3fms - %s:\n %s\n", \
           ((double)((timerValue).endTime - (timerValue).startTime) / CLOCKS_PER_SEC) * 1000.0, \
           summary, \
           reason); \
    fflush(stdout); \
    g_failures++; \
} while (0)

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

static TZrBool diagnostic_array_contains_code(SZrArray *diagnostics, const TZrChar *expectedCode) {
    for (TZrSize index = 0; diagnostics != ZR_NULL && index < diagnostics->length; index++) {
        SZrLspDiagnostic **diagnosticPtr = (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        if (diagnosticPtr != ZR_NULL &&
            *diagnosticPtr != ZR_NULL &&
            (*diagnosticPtr)->code != ZR_NULL &&
            strcmp(test_string_text((*diagnosticPtr)->code), expectedCode) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool diagnostic_array_contains_message(SZrArray *diagnostics, const TZrChar *needle) {
    for (TZrSize index = 0; diagnostics != ZR_NULL && index < diagnostics->length; index++) {
        SZrLspDiagnostic **diagnosticPtr = (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        const TZrChar *message = (diagnosticPtr != ZR_NULL && *diagnosticPtr != ZR_NULL)
                                     ? test_string_text((*diagnosticPtr)->message)
                                     : ZR_NULL;
        if (message != ZR_NULL && strstr(message, needle) != ZR_NULL) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool run_declaration_diagnostic_case(SZrState *state,
                                               SZrTestTimer *timer,
                                               const TZrChar *summary,
                                               const TZrChar *uriText,
                                               const TZrChar *content,
                                               const TZrChar *expectedCode,
                                               const TZrChar *expectedProblem,
                                               const TZrChar *expectedSuggestion,
                                               const TZrChar *failureReason) {
    SZrLspContext *context;
    SZrString *uri;
    SZrArray diagnostics;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL((*timer), summary, "failed to create LSP context or URI");
        return ZR_FALSE;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL((*timer), summary, "failed to update document");
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 4);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL((*timer), summary, "diagnostics request failed");
        return ZR_FALSE;
    }

    if (!diagnostic_array_contains_code(&diagnostics, expectedCode) ||
        !diagnostic_array_contains_message(&diagnostics, expectedProblem) ||
        !diagnostic_array_contains_message(&diagnostics, expectedSuggestion)) {
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL((*timer), summary, failureReason);
        return ZR_FALSE;
    }

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    return ZR_TRUE;
}

static void test_lsp_missing_declaration_body_open_parser_diagnostics(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Declaration Body Open Parser Diagnostics";

    TEST_START(summary);

    if (!run_declaration_diagnostic_case(state,
                                         &timer,
                                         summary,
                                         "file:///parser_missing_class_body_open.zr",
                                         "class Box\n",
                                         "missing_declaration_body_open",
                                         "Missing '{' to start class declaration body",
                                         "Insert '{' after the class declaration header",
                                         "expected class declaration missing-body-open diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_declaration_diagnostic_case(state,
                                         &timer,
                                         summary,
                                         "file:///parser_missing_interface_body_open.zr",
                                         "interface Sized\n",
                                         "missing_declaration_body_open",
                                         "Missing '{' to start interface declaration body",
                                         "Insert '{' after the interface declaration header",
                                         "expected interface declaration missing-body-open diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_declaration_diagnostic_case(state,
                                         &timer,
                                         summary,
                                         "file:///parser_missing_function_body_open.zr",
                                         "func pick(): int\n",
                                         "missing_declaration_body_open",
                                         "Missing '{' to start function declaration body",
                                         "Insert '{' after the function declaration header",
                                         "expected function declaration missing-body-open diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_declaration_diagnostic_case(state,
                                         &timer,
                                         summary,
                                         "file:///parser_missing_enum_body_open.zr",
                                         "enum Tone\n",
                                         "missing_declaration_body_open",
                                         "Missing '{' to start enum declaration body",
                                         "Insert '{' after the enum declaration header",
                                         "expected enum declaration missing-body-open diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_declaration_diagnostic_case(state,
                                         &timer,
                                         summary,
                                         "file:///parser_missing_extern_block_body_open.zr",
                                         "%extern(\"fixture\")\n",
                                         "missing_declaration_body_open",
                                         "Missing '{' to start extern block body",
                                         "Insert '{' after the extern block header",
                                         "expected extern block missing-body-open diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_declaration_diagnostic_case(state,
                                         &timer,
                                         summary,
                                         "file:///parser_missing_test_declaration_body_open.zr",
                                         "%test(\"smoke\")\n",
                                         "missing_declaration_body_open",
                                         "Missing '{' to start test declaration body",
                                         "Insert '{' after the test declaration header",
                                         "expected test declaration missing-body-open diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_extern_spec_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Extern Spec Close Parser Diagnostic";

    TEST_START(summary);

    if (!run_declaration_diagnostic_case(state,
                                         &timer,
                                         summary,
                                         "file:///parser_missing_extern_spec_close.zr",
                                         "%extern(\"fixture\" { NativeAdd(value: int): int; }\n",
                                         "missing_extern_spec_close",
                                         "Missing closing ')' in extern block spec",
                                         "Insert ')' after the extern block spec before the extern block body",
                                         "expected extern spec missing-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_test_name_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Test Name Close Parser Diagnostic";

    TEST_START(summary);

    if (!run_declaration_diagnostic_case(state,
                                         &timer,
                                         summary,
                                         "file:///parser_missing_test_name_close.zr",
                                         "%test(\"smoke\" { return 1; }\n",
                                         "missing_test_name_close",
                                         "Missing closing ')' in test declaration name",
                                         "Insert ')' after the test declaration name before the test body",
                                         "expected test declaration name missing-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

int main(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    SZrState *state;

    printf("==========\n");
    printf("Language Server - Declaration Parser Diagnostic Tests\n");
    printf("==========\n\n");

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("Fail - failed to create VM state\n");
        return 1;
    }
    state = global->mainThreadState;

    test_lsp_missing_declaration_body_open_parser_diagnostics(state);
    test_lsp_missing_extern_spec_close_parser_diagnostic(state);
    test_lsp_missing_test_name_close_parser_diagnostic(state);

    ZrCore_GlobalState_Free(global);

    printf("==========\n");
    if (g_failures == 0) {
        printf("All LSP Declaration Parser Diagnostic Tests Completed\n");
        printf("==========\n");
        return 0;
    }

    printf("%d LSP Declaration Parser Diagnostic Test(s) Failed\n", g_failures);
    printf("==========\n");
    return 1;
}
