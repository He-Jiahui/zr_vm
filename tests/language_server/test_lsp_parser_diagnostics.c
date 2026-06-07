//
// Focused LSP parser diagnostic regression tests.
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

#define TEST_DIVIDER() do { \
    printf("----------\n"); \
    fflush(stdout); \
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

static TZrBool run_parser_diagnostic_case(SZrState *state,
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

static void test_lsp_missing_index_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Index Close Parser Diagnostic";
    const TZrChar *content = "var value = [1, 2];\nreturn value[0;\n";

    TEST_START(summary);
    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_index_close.zr",
                                    content,
                                    "missing_index_close",
                                    "Missing closing ']' in index access",
                                    "Insert ']' after the index expression before continuing",
                                    "expected missing-index-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_member_name_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Member Name Parser Diagnostic";
    const TZrChar *content = "var value = 1;\nreturn value.;\n";

    TEST_START(summary);
    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_member_name.zr",
                                    content,
                                    "missing_member_name",
                                    "Missing member name after '.'",
                                    "Add a member name after '.' or remove the member access",
                                    "expected missing-member-name diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_call_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Call Close Parser Diagnostic";
    const TZrChar *content =
        "func pick(value: int): int { return value; }\n"
        "return pick(1 + 2;\n";

    TEST_START(summary);
    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_call_close.zr",
                                    content,
                                    "missing_call_close",
                                    "Missing closing ')' in function call",
                                    "Insert ')' after the last argument before continuing",
                                    "expected missing-call-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_group_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Group Close Parser Diagnostic";
    const TZrChar *content = "return (1 + 2\n";

    TEST_START(summary);
    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_group_close.zr",
                                    content,
                                    "missing_group_close",
                                    "Missing closing ')' in grouped expression",
                                    "Insert ')' after the grouped expression before continuing",
                                    "expected missing-group-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_parameter_list_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Parameter List Close Parser Diagnostic";
    const TZrChar *content = "func pick(value: int: int { return value; }\n";

    TEST_START(summary);
    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_parameter_list_close.zr",
                                    content,
                                    "missing_parameter_list_close",
                                    "Missing closing ')' in function declaration parameters",
                                    "Insert ')' after the parameter list before continuing",
                                    "expected missing-parameter-list-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_method_parameter_list_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Method Parameter List Close Parser Diagnostic";
    const TZrChar *content =
        "class Box {\n"
        "    func read(value: int: int { return value; }\n"
        "}\n";

    TEST_START(summary);
    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_method_parameter_list_close.zr",
                                    content,
                                    "missing_parameter_list_close",
                                    "Missing closing ')' in function declaration parameters",
                                    "Insert ')' after the parameter list before continuing",
                                    "expected missing-method-parameter-list-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_interface_method_parameter_list_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Interface Method Parameter List Close Parser Diagnostic";
    const TZrChar *content =
        "interface Readable {\n"
        "    read(value: int: int;\n"
        "}\n";

    TEST_START(summary);
    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_interface_method_parameter_list_close.zr",
                                    content,
                                    "missing_parameter_list_close",
                                    "Missing closing ')' in function declaration parameters",
                                    "Insert ')' after the parameter list before continuing",
                                    "expected missing-interface-method-parameter-list-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_interface_meta_parameter_list_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Interface Meta Parameter List Close Parser Diagnostic";
    const TZrChar *content =
        "interface Callable {\n"
        "    @call(value: int: int;\n"
        "}\n";

    TEST_START(summary);
    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_interface_meta_parameter_list_close.zr",
                                    content,
                                    "missing_parameter_list_close",
                                    "Missing closing ')' in function declaration parameters",
                                    "Insert ')' after the parameter list before continuing",
                                    "expected missing-interface-meta-parameter-list-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_extern_function_parameter_list_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Extern Function Parameter List Close Parser Diagnostic";
    const TZrChar *content =
        "%extern(\"fixture\") {\n"
        "    NativeAdd(value: int: int;\n"
        "}\n";

    TEST_START(summary);
    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_extern_function_parameter_list_close.zr",
                                    content,
                                    "missing_parameter_list_close",
                                    "Missing closing ')' in function declaration parameters",
                                    "Insert ')' after the parameter list before continuing",
                                    "expected missing-extern-function-parameter-list-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_extern_delegate_parameter_list_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Extern Delegate Parameter List Close Parser Diagnostic";
    const TZrChar *content =
        "%extern(\"fixture\") {\n"
        "    delegate Callback(value: int: int;\n"
        "}\n";

    TEST_START(summary);
    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_extern_delegate_parameter_list_close.zr",
                                    content,
                                    "missing_parameter_list_close",
                                    "Missing closing ')' in function declaration parameters",
                                    "Insert ')' after the parameter list before continuing",
                                    "expected missing-extern-delegate-parameter-list-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_array_assignment_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Array Assignment Parser Diagnostic";
    const TZrChar *content = "var seed = 2;\nvar values = [seed = 3];\n";

    TEST_START(summary);
    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_array_assignment.zr",
                                    content,
                                    "array_element_assignment",
                                    "Array element cannot be an assignment expression",
                                    "Move the assignment into a statement before the array literal",
                                    "expected array-element assignment diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_array_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Array Close Parser Diagnostic";
    const TZrChar *content = "return [1, 2\n";

    TEST_START(summary);
    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_array_close.zr",
                                    content,
                                    "missing_array_close",
                                    "Missing closing ']' in array literal",
                                    "Insert ']' after the last array element",
                                    "expected missing-array-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_array_element_separator_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Array Element Separator Parser Diagnostic";
    const TZrChar *content = "return [1 2];\n";

    TEST_START(summary);
    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_array_element_separator.zr",
                                    content,
                                    "missing_array_element_separator",
                                    "Missing separator between array elements",
                                    "Insert ',' or ';' between array elements",
                                    "expected missing-array-element-separator diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_object_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Object Close Parser Diagnostic";
    const TZrChar *content = "return {a: 1\n";

    TEST_START(summary);
    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_object_close.zr",
                                    content,
                                    "missing_object_close",
                                    "Missing closing '}' in object literal",
                                    "Insert '}' after the last object property",
                                    "expected missing-object-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_object_computed_key_close_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Object Computed Key Close Parser Diagnostic";
    const TZrChar *content = "var seed = 2;\nreturn {[seed: 1};\n";

    TEST_START(summary);
    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_object_computed_key_close.zr",
                                    content,
                                    "missing_object_computed_key_close",
                                    "Missing closing ']' in computed object key",
                                    "Insert ']' after the computed key expression before ':'",
                                    "expected missing-object-computed-key-close diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_object_property_colon_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Object Property Colon Parser Diagnostic";
    const TZrChar *content = "var value = {a 1};\n";

    TEST_START(summary);
    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_object_property_colon.zr",
                                    content,
                                    "missing_object_property_colon",
                                    "Missing ':' after object property key",
                                    "Insert ':' between the property key and value expression",
                                    "expected missing-object-property-colon diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_object_property_separator_parser_diagnostic(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Object Property Separator Parser Diagnostic";
    const TZrChar *content = "var value = {a: 1 b: 2};\n";

    TEST_START(summary);
    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_object_property_separator.zr",
                                    content,
                                    "missing_object_property_separator",
                                    "Missing separator between object properties",
                                    "Insert ',' or ';' between object properties",
                                    "expected missing-object-property-separator diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_conditional_branch_parser_diagnostics(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Conditional Branch Parser Diagnostics";

    TEST_START(summary);
    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_conditional_consequent.zr",
                                    "return true ? : 2;\n",
                                    "missing_conditional_consequent",
                                    "Missing expression after '?' in conditional expression",
                                    "Add the consequent expression between '?' and ':'",
                                    "expected missing-conditional-consequent diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_conditional_colon.zr",
                                    "return true ? 1 2;\n",
                                    "missing_conditional_colon",
                                    "Missing ':' in conditional expression",
                                    "Insert ':' between the consequent and alternate expressions",
                                    "expected missing-conditional-colon diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_conditional_alternate.zr",
                                    "return true ? 1 : ;\n",
                                    "missing_conditional_alternate",
                                    "Missing expression after ':' in conditional expression",
                                    "Add the alternate expression after ':'",
                                    "expected missing-conditional-alternate diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_condition_parser_diagnostics(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Condition Parser Diagnostics";

    TEST_START(summary);
    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_if_condition.zr",
                                    "if () { return 1; }",
                                    "missing_condition",
                                    "Missing condition inside 'if'",
                                    "Add a boolean expression between '(' and ')'",
                                    "expected empty if condition to publish a structured missing-condition diagnostic")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_while_condition.zr",
                                    "while () { break; }",
                                    "missing_condition",
                                    "Missing condition inside 'while'",
                                    "Add a boolean expression between '(' and ')'",
                                    "expected empty while condition to publish a structured missing-condition diagnostic")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_switch_condition.zr",
                                    "switch () { () { return 1; } }",
                                    "missing_condition",
                                    "Missing condition inside 'switch'",
                                    "Add a boolean expression between '(' and ')'",
                                    "expected empty switch condition to publish a structured missing-condition diagnostic")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_if_condition_close.zr",
                                    "var ready = true;\nif (ready { return 1; }",
                                    "missing_condition_close",
                                    "Missing ')' after 'if' condition",
                                    "Insert ')' after the condition expression before the block",
                                    "expected if condition missing-close diagnostic to name the concrete parenthesis problem")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_while_condition_close.zr",
                                    "var ready = true;\nwhile (ready { break; }",
                                    "missing_condition_close",
                                    "Missing ')' after 'while' condition",
                                    "Insert ')' after the condition expression before the block",
                                    "expected while condition missing-close diagnostic to name the concrete parenthesis problem")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_switch_condition_close.zr",
                                    "var choice = 1;\nswitch (choice { () { return 1; } }",
                                    "missing_condition_close",
                                    "Missing ')' after 'switch' condition",
                                    "Insert ')' after the condition expression before the block",
                                    "expected switch condition missing-close diagnostic to name the concrete parenthesis problem")) {
        return;
    }

    TEST_PASS(timer, summary);
}

static void test_lsp_missing_statement_semicolon_parser_diagnostics(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Missing Statement Semicolon Parser Diagnostics";

    TEST_START(summary);
    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_return_semicolon.zr",
                                    "return 1\nvar next = 2;\n",
                                    "missing_statement_semicolon",
                                    "Missing ';' after return statement",
                                    "Insert ';' after the return statement",
                                    "expected return statement missing-semicolon diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_expression_statement_semicolon.zr",
                                    "1 + 2\nvar next = 3;\n",
                                    "missing_statement_semicolon",
                                    "Missing ';' after expression statement",
                                    "Insert ';' after the expression statement",
                                    "expected expression statement missing-semicolon diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_variable_declaration_semicolon.zr",
                                    "var seed = 1\n"
                                    "var next = 2;\n",
                                    "missing_statement_semicolon",
                                    "Missing ';' after variable declaration statement",
                                    "Insert ';' after the variable declaration statement",
                                    "expected variable declaration missing-semicolon diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_module_declaration_semicolon.zr",
                                    "%module \"main\"\n"
                                    "var next = 2;\n",
                                    "missing_statement_semicolon",
                                    "Missing ';' after module declaration statement",
                                    "Insert ';' after the module declaration statement",
                                    "expected module declaration missing-semicolon diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_break_semicolon.zr",
                                    "while (true) {\n"
                                    "    break\n"
                                    "    var next = 2;\n"
                                    "}\n",
                                    "missing_statement_semicolon",
                                    "Missing ';' after break statement",
                                    "Insert ';' after the break statement",
                                    "expected break statement missing-semicolon diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_continue_semicolon.zr",
                                    "while (true) {\n"
                                    "    continue\n"
                                    "    var next = 2;\n"
                                    "}\n",
                                    "missing_statement_semicolon",
                                    "Missing ';' after continue statement",
                                    "Insert ';' after the continue statement",
                                    "expected continue statement missing-semicolon diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_throw_semicolon.zr",
                                    "throw 1\n"
                                    "var next = 2;\n",
                                    "missing_statement_semicolon",
                                    "Missing ';' after throw statement",
                                    "Insert ';' after the throw statement",
                                    "expected throw statement missing-semicolon diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_out_semicolon.zr",
                                    "out 1\n"
                                    "var next = 2;\n",
                                    "missing_statement_semicolon",
                                    "Missing ';' after out statement",
                                    "Insert ';' after the out statement",
                                    "expected out statement missing-semicolon diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_using_semicolon.zr",
                                    "var resource = 1;\n"
                                    "%using resource\n"
                                    "var next = 2;\n",
                                    "missing_statement_semicolon",
                                    "Missing ';' after using statement",
                                    "Insert ';' after the using statement",
                                    "expected using statement missing-semicolon diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_interface_method_signature_semicolon.zr",
                                    "interface Readable {\n"
                                    "    read(value: int): int\n"
                                    "}\n",
                                    "missing_statement_semicolon",
                                    "Missing ';' after interface method signature statement",
                                    "Insert ';' after the interface method signature statement",
                                    "expected interface-method-signature missing-semicolon diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_interface_meta_signature_semicolon.zr",
                                    "interface Callable {\n"
                                    "    @call(value: int): int\n"
                                    "}\n",
                                    "missing_statement_semicolon",
                                    "Missing ';' after interface meta signature statement",
                                    "Insert ';' after the interface meta signature statement",
                                    "expected interface-meta-signature missing-semicolon diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_interface_property_signature_semicolon.zr",
                                    "interface Sized {\n"
                                    "    get length: int\n"
                                    "}\n",
                                    "missing_statement_semicolon",
                                    "Missing ';' after interface property signature statement",
                                    "Insert ';' after the interface property signature statement",
                                    "expected interface-property-signature missing-semicolon diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_interface_field_declaration_semicolon.zr",
                                    "interface Entity {\n"
                                    "    var id: int\n"
                                    "}\n",
                                    "missing_statement_semicolon",
                                    "Missing ';' after interface field declaration statement",
                                    "Insert ';' after the interface field declaration statement",
                                    "expected interface-field-declaration missing-semicolon diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_class_field_semicolon.zr",
                                    "class Entity {\n"
                                    "    var id: int\n"
                                    "}\n",
                                    "missing_statement_semicolon",
                                    "Missing ';' after class field declaration statement",
                                    "Insert ';' after the class field declaration statement",
                                    "expected class-field missing-semicolon diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_class_getter_semicolon.zr",
                                    "class Sized {\n"
                                    "    get length: int\n"
                                    "}\n",
                                    "missing_statement_semicolon",
                                    "Missing ';' after class getter statement",
                                    "Insert ';' after the class getter statement",
                                    "expected class-getter missing-semicolon diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_class_setter_semicolon.zr",
                                    "class Sized {\n"
                                    "    set length(value: int)\n"
                                    "}\n",
                                    "missing_statement_semicolon",
                                    "Missing ';' after class setter statement",
                                    "Insert ';' after the class setter statement",
                                    "expected class-setter missing-semicolon diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_class_method_semicolon.zr",
                                    "class Box {\n"
                                    "    func read(value: int): int\n"
                                    "}\n",
                                    "missing_statement_semicolon",
                                    "Missing ';' after class method statement",
                                    "Insert ';' after the class method statement",
                                    "expected class-method missing-semicolon diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    if (!run_parser_diagnostic_case(state,
                                    &timer,
                                    summary,
                                    "file:///parser_missing_class_meta_semicolon.zr",
                                    "class Callable {\n"
                                    "    @call(value: int): int\n"
                                    "}\n",
                                    "missing_statement_semicolon",
                                    "Missing ';' after class meta function statement",
                                    "Insert ';' after the class meta function statement",
                                    "expected class-meta-function missing-semicolon diagnostic to carry code, problem text, and suggestion")) {
        return;
    }

    TEST_PASS(timer, summary);
}

int main(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    SZrState *state;

    printf("==========\n");
    printf("Language Server - Parser Diagnostic Tests\n");
    printf("==========\n\n");

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("Fail - failed to create VM state\n");
        return 1;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    test_lsp_missing_index_close_parser_diagnostic(state);
    TEST_DIVIDER();
    test_lsp_missing_member_name_parser_diagnostic(state);
    TEST_DIVIDER();
    test_lsp_missing_call_close_parser_diagnostic(state);
    TEST_DIVIDER();
    test_lsp_missing_group_close_parser_diagnostic(state);
    TEST_DIVIDER();
    test_lsp_missing_parameter_list_close_parser_diagnostic(state);
    TEST_DIVIDER();
    test_lsp_missing_method_parameter_list_close_parser_diagnostic(state);
    TEST_DIVIDER();
    test_lsp_missing_interface_method_parameter_list_close_parser_diagnostic(state);
    TEST_DIVIDER();
    test_lsp_missing_interface_meta_parameter_list_close_parser_diagnostic(state);
    TEST_DIVIDER();
    test_lsp_missing_extern_function_parameter_list_close_parser_diagnostic(state);
    TEST_DIVIDER();
    test_lsp_missing_extern_delegate_parameter_list_close_parser_diagnostic(state);
    TEST_DIVIDER();
    test_lsp_array_assignment_parser_diagnostic(state);
    TEST_DIVIDER();
    test_lsp_missing_array_close_parser_diagnostic(state);
    TEST_DIVIDER();
    test_lsp_missing_array_element_separator_parser_diagnostic(state);
    TEST_DIVIDER();
    test_lsp_missing_object_close_parser_diagnostic(state);
    TEST_DIVIDER();
    test_lsp_missing_object_computed_key_close_parser_diagnostic(state);
    TEST_DIVIDER();
    test_lsp_missing_object_property_colon_parser_diagnostic(state);
    TEST_DIVIDER();
    test_lsp_missing_object_property_separator_parser_diagnostic(state);
    TEST_DIVIDER();
    test_lsp_missing_conditional_branch_parser_diagnostics(state);
    TEST_DIVIDER();
    test_lsp_missing_condition_parser_diagnostics(state);
    TEST_DIVIDER();
    test_lsp_missing_statement_semicolon_parser_diagnostics(state);
    TEST_DIVIDER();

    ZrCore_GlobalState_Free(global);
    printf("==========\n");
    if (g_failures == 0) {
        printf("All LSP Parser Diagnostic Tests Completed\n");
    } else {
        printf("%d LSP Parser Diagnostic Test(s) Failed\n", g_failures);
    }
    printf("==========\n");

    return g_failures == 0 ? 0 : 1;
}
