//
// Focused LSP class-member parser diagnostic regression tests.
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

static TZrBool run_class_member_diagnostic_case(SZrState *state,
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

    if (diagnostics.length == 0 ||
        !diagnostic_array_contains_code(&diagnostics, expectedCode) ||
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

static void test_lsp_missing_class_setter_parameter_list_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Class Setter Parameter List Close Parser Diagnostic";
    const TZrChar *content =
        "class Sized {\n"
        "    set length(value: int { return value; }\n"
        "}\n";

    TEST_START(summary);
    if (!run_class_member_diagnostic_case(state,
                                          &timer,
                                          summary,
                                          "file:///parser_missing_class_setter_parameter_list_close.zr",
                                          content,
                                          "missing_parameter_list_close",
                                          "Missing closing ')' in function declaration parameters",
                                          "Insert ')' after the parameter list before continuing",
                                          "expected class-setter missing-parameter-list-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_class_meta_parameter_list_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Class Meta Parameter List Close Parser Diagnostic";
    const TZrChar *content =
        "class Callable {\n"
        "    @call(value: int: int { return value; }\n"
        "}\n";

    TEST_START(summary);
    if (!run_class_member_diagnostic_case(state,
                                          &timer,
                                          summary,
                                          "file:///parser_missing_class_meta_parameter_list_close.zr",
                                          content,
                                          "missing_parameter_list_close",
                                          "Missing closing ')' in function declaration parameters",
                                          "Insert ')' after the parameter list before continuing",
                                          "expected class-meta missing-parameter-list-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

int main(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    SZrState *state;

    printf("==========\n");
    printf("Language Server - Class Member Parser Diagnostic Tests\n");
    printf("==========\n\n");

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("Fail - failed to create VM state\n");
        return 1;
    }
    state = global->mainThreadState;

    test_lsp_missing_class_setter_parameter_list_close_parser_diagnostic(state);
    test_lsp_missing_class_meta_parameter_list_close_parser_diagnostic(state);

    ZrCore_GlobalState_Free(global);

    printf("==========\n");
    if (g_failures == 0) {
        printf("All LSP Class Member Parser Diagnostic Tests Completed\n");
        printf("==========\n");
        return 0;
    }

    printf("%d LSP Class Member Parser Diagnostic Test(s) Failed\n", g_failures);
    printf("==========\n");
    return 1;
}
