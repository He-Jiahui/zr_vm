//
// Focused LSP statement parser diagnostic regression tests.
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

static TZrBool run_statement_diagnostic_case(SZrState *state,
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

static void test_lsp_missing_statement_body_open_parser_diagnostics(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Statement Body Open Parser Diagnostics";

    TEST_START(summary);

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_if_body_open.zr",
                                       "var ready = true;\nif (ready)\nreturn 1;\n",
                                       "missing_statement_body_open",
                                       "Missing '{' to start if statement body",
                                       "Insert '{' after the if statement header",
                                       "expected if statement missing-body-open diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_while_body_open.zr",
                                       "var ready = true;\nwhile (ready)\nreturn 1;\n",
                                       "missing_statement_body_open",
                                       "Missing '{' to start while statement body",
                                       "Insert '{' after the while statement header",
                                       "expected while statement missing-body-open diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_for_body_open.zr",
                                       "for (;;)\nreturn 1;\n",
                                       "missing_statement_body_open",
                                       "Missing '{' to start for statement body",
                                       "Insert '{' after the for statement header",
                                       "expected for statement missing-body-open diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_foreach_body_open.zr",
                                       "var items = [1, 2];\nfor (var item in items)\nreturn item;\n",
                                       "missing_statement_body_open",
                                       "Missing '{' to start foreach statement body",
                                       "Insert '{' after the foreach statement header",
                                       "expected foreach statement missing-body-open diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_switch_body_open.zr",
                                       "var choice = 1;\nswitch (choice)\nreturn 1;\n",
                                       "missing_statement_body_open",
                                       "Missing '{' to start switch statement body",
                                       "Insert '{' after the switch statement header",
                                       "expected switch statement missing-body-open diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_switch_case_body_open.zr",
                                       "var choice = 1;\nswitch (choice) { (1)\nreturn 1; }\n",
                                       "missing_statement_body_open",
                                       "Missing '{' to start switch case body",
                                       "Insert '{' after the switch case header",
                                       "expected switch case missing-body-open diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_switch_default_body_open.zr",
                                       "var choice = 1;\nswitch (choice) { ()\nreturn 1; }\n",
                                       "missing_statement_body_open",
                                       "Missing '{' to start switch default body",
                                       "Insert '{' after the switch default header",
                                       "expected switch default missing-body-open diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_else_body_open.zr",
                                       "var ready = true;\nif (ready) { return 1; } else\nreturn 2;\n",
                                       "missing_statement_body_open",
                                       "Missing '{' to start else statement body",
                                       "Insert '{' after the else statement header",
                                       "expected else statement missing-body-open diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_try_body_open.zr",
                                       "try\nreturn 1;\n",
                                       "missing_statement_body_open",
                                       "Missing '{' to start try statement body",
                                       "Insert '{' after the try statement header",
                                       "expected try statement missing-body-open diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_catch_body_open.zr",
                                       "try { throw 1; } catch (error)\nreturn 2;\n",
                                       "missing_statement_body_open",
                                       "Missing '{' to start catch statement body",
                                       "Insert '{' after the catch statement header",
                                       "expected catch statement missing-body-open diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_finally_body_open.zr",
                                       "try { return 1; } finally\nreturn 2;\n",
                                       "missing_statement_body_open",
                                       "Missing '{' to start finally statement body",
                                       "Insert '{' after the finally statement header",
                                       "expected finally statement missing-body-open diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_using_body_open.zr",
                                       "%using (resource)\nreturn resource;\n",
                                       "missing_statement_body_open",
                                       "Missing '{' to start using statement body",
                                       "Insert '{' after the using statement header",
                                       "expected using statement missing-body-open diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_block_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Block Close Parser Diagnostic";

    TEST_START(summary);

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_if_block_close.zr",
                                       "var ready = true;\nif (ready) { return 1;\n",
                                       "missing_block_close",
                                       "Missing closing '}' for block",
                                       "Insert '}' to close the block before continuing",
                                       "expected missing-block-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_catch_pattern_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Catch Pattern Close Parser Diagnostic";

    TEST_START(summary);

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_catch_pattern_close.zr",
                                       "try { throw 1; } catch (error { return 2; }\n",
                                       "missing_catch_pattern_close",
                                       "Missing closing ')' in catch pattern",
                                       "Insert ')' after the catch pattern before the catch body",
                                       "expected missing-catch-pattern-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_using_resource_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Using Resource Close Parser Diagnostic";

    TEST_START(summary);

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_using_resource_close.zr",
                                       "%using (resource { return resource; }\n",
                                       "missing_using_resource_close",
                                       "Missing closing ')' in using resource",
                                       "Insert ')' after the using resource before the using body",
                                       "expected missing-using-resource-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_for_header_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing For Header Close Parser Diagnostic";

    TEST_START(summary);

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_for_header_close.zr",
                                       "var ready = true;\nfor (; ready; ready = false { return 1; }\n",
                                       "missing_for_header_close",
                                       "Missing closing ')' in for header",
                                       "Insert ')' after the for header before the loop body",
                                       "expected missing-for-header-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_for_header_separator_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing For Header Separator Parser Diagnostic";

    TEST_START(summary);

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_for_header_separator.zr",
                                       "var i = 0;\nfor (i = 0 i < 3; i = i + 1) { out i; }\n",
                                       "missing_for_header_separator",
                                       "Missing ';' between for header clauses",
                                       "Insert ';' between the for header clauses",
                                       "expected missing-for-header-separator diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_for_header_condition_separator.zr",
                                       "var i = 0;\nfor (i = 0; i < 3 i = i + 1) { out i; }\n",
                                       "missing_for_header_separator",
                                       "Missing ';' between for header clauses",
                                       "Insert ';' between the for header clauses",
                                       "expected missing-for-header-separator diagnostic between condition and step clauses")) {
        return;
    }

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_for_var_header_separator.zr",
                                       "for (var i = 0 i < 3; i = i + 1) { out i; }\n",
                                       "missing_for_header_separator",
                                       "Missing ';' between for header clauses",
                                       "Insert ';' between the for header clauses",
                                       "expected missing-for-header-separator diagnostic after variable initializer clause")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_foreach_header_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Foreach Header Close Parser Diagnostic";

    TEST_START(summary);

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_foreach_header_close.zr",
                                       "var items = [1, 2];\nfor (var item in items { return item; }\n",
                                       "missing_foreach_header_close",
                                       "Missing closing ')' in foreach header",
                                       "Insert ')' after the foreach iterable before the loop body",
                                       "expected missing-foreach-header-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_foreach_in_keyword_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Foreach In Keyword Parser Diagnostic";

    TEST_START(summary);

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_foreach_in_keyword.zr",
                                       "var items = [1, 2];\nfor (var item items) { return item; }\n",
                                       "missing_foreach_in_keyword",
                                       "Missing 'in' in foreach header",
                                       "Insert 'in' between the foreach pattern and iterable expression",
                                       "expected missing-foreach-in-keyword diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_switch_case_header_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Switch Case Header Close Parser Diagnostic";

    TEST_START(summary);

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_switch_case_header_close.zr",
                                       "var choice = 1;\nswitch (choice) { (1 { return 1; } }\n",
                                       "missing_switch_case_header_close",
                                       "Missing closing ')' in switch case header",
                                       "Insert ')' after the switch case expression before the case body",
                                       "expected missing-switch-case-header-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_switch_body_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Switch Body Close Parser Diagnostic";

    TEST_START(summary);

    if (!run_statement_diagnostic_case(state,
                                       &timer,
                                       summary,
                                       "file:///parser_missing_switch_body_close.zr",
                                       "var choice = 1;\nswitch (choice) { (1) { return 1; }\n",
                                       "missing_switch_body_close",
                                       "Missing closing '}' for switch body",
                                       "Insert '}' to close the switch body before continuing",
                                       "expected missing-switch-body-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

int main(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    SZrState *state;

    printf("==========\n");
    printf("Language Server - Statement Parser Diagnostic Tests\n");
    printf("==========\n\n");

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("Fail - failed to create VM state\n");
        return 1;
    }
    state = global->mainThreadState;

    test_lsp_missing_statement_body_open_parser_diagnostics(state);
    test_lsp_missing_block_close_parser_diagnostic(state);
    test_lsp_missing_catch_pattern_close_parser_diagnostic(state);
    test_lsp_missing_using_resource_close_parser_diagnostic(state);
    test_lsp_missing_for_header_close_parser_diagnostic(state);
    test_lsp_missing_for_header_separator_parser_diagnostic(state);
    test_lsp_missing_foreach_header_close_parser_diagnostic(state);
    test_lsp_missing_foreach_in_keyword_parser_diagnostic(state);
    test_lsp_missing_switch_case_header_close_parser_diagnostic(state);
    test_lsp_missing_switch_body_close_parser_diagnostic(state);

    ZrCore_GlobalState_Free(global);

    printf("==========\n");
    if (g_failures == 0) {
        printf("All LSP Statement Parser Diagnostic Tests Completed\n");
        printf("==========\n");
        return 0;
    }

    printf("%d LSP Statement Parser Diagnostic Test(s) Failed\n", g_failures);
    printf("==========\n");
    return 1;
}
