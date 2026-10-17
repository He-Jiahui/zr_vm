#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"

static SZrState *state;
static char *source;
static unsigned limitDiagnostics;

void setUp(void) {
    source = ZR_NULL;
    limitDiagnostics = 0u;
    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
}

void tearDown(void) {
    free(source);
    ZrTests_Runtime_State_Destroy(state);
}

static void make_source(const char *prefix, const char *open, const char *inner,
                        const char *close, const char *suffix, size_t count) {
    size_t length = strlen(prefix) + count * (strlen(open) + strlen(close)) +
                    strlen(inner) + strlen(suffix);
    char *cursor;
    free(source);
    source = malloc(length + 1u);
    TEST_ASSERT_NOT_NULL(source);
    cursor = source;
#define APPEND(text) do { size_t n = strlen(text); memcpy(cursor, text, n); cursor += n; } while (0)
    APPEND(prefix);
    for (size_t n = 0; n < count; ++n) APPEND(open);
    APPEND(inner);
    for (size_t n = 0; n < count; ++n) APPEND(close);
    APPEND(suffix);
    *cursor = '\0';
#undef APPEND
}

static void capture_error(TZrPtr unused, const SZrFileRange *range,
                           const TZrChar *message, EZrToken token) {
    (void)unused;
    (void)range;
    (void)token;
    if (strstr(message, "parser recursion limit") != ZR_NULL) ++limitDiagnostics;
}

/* Whole documents, incremental declarations, and expression fragments are all
 * entry points used by the compiler or language server. */
static void expect_limit(unsigned entry) {
    SZrParserState parser;
    SZrAstNode *ast;
    TZrBool failed, depthFailed;
    TZrUInt32 remainingDepth;
    char context[48];
    limitDiagnostics = 0u;
    ZrParser_State_Init(&parser, state, source, strlen(source),
                       ZrCore_String_CreateFromNative(state, "deep.zr"));
    parser.errorCallback = capture_error;
    parser.suppressErrorOutput = ZR_TRUE;
    if (entry == 1u) ast = ZrParser_ParseTopLevelStatementWithState(&parser);
    else if (entry == 2u) ast = ZrParser_ParseExpressionWithState(&parser);
    else ast = ZrParser_ParseWithState(&parser);
    failed = parser.hasError;
    depthFailed = parser.hasNestingError;
    remainingDepth = parser.recursionDepth;
    ZrParser_State_Free(&parser);
    TEST_ASSERT_NULL(ast);
    TEST_ASSERT_TRUE(failed);
    snprintf(context, sizeof(context), "Source starts: %.30s", source);
    TEST_ASSERT_TRUE_MESSAGE(depthFailed, context);
    TEST_ASSERT_EQUAL_UINT32(0u, remainingDepth);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, limitDiagnostics, context);
}

static void test_deep_parentheses_and_all_parser_entry_points(void) {
    make_source("return ", "(", "1", ")", ";", 10000u);
    expect_limit(0u);
    expect_limit(1u);
    make_source("", "(", "1", ")", "", 10000u);
    expect_limit(2u);
}

static void test_recursive_operators_and_blocks_are_bounded(void) {
    make_source("return ", "!", "true", "", ";", 10000u);
    expect_limit(0u);
    make_source("return ", "true ? 1 : ", "0", "", ";", 10000u);
    expect_limit(0u);
    make_source("", "x = ", "1", "", ";", 10000u);
    expect_limit(0u);
    make_source("fn deep() { ", "{ ", "return 1;", " }", " }", 10000u);
    expect_limit(0u);
    make_source("", "if (true) {} else ", "{}", "", "", 10000u);
    expect_limit(0u);
}

static void test_nested_types_are_bounded(void) {
    make_source("var value: ", "Box<", "int", ">", ";", 10000u);
    expect_limit(0u);
}

static void test_template_interpolation_shares_recursion_budget(void) {
    make_source("return `text${", "(", "1", ")", "}`;", 10000u);
    expect_limit(0u);
}

static void test_compiler_rejects_deep_source_and_accepts_ordinary_nesting(void) {
    SZrFunction *function;
    make_source("return ", "(", "1", ")", ";", 10000u);
    function = ZrParser_Source_Compile(state, source, strlen(source),
            ZrCore_String_CreateFromNative(state, "deep_compile.zr"));
    TEST_ASSERT_NULL(function);
    make_source("return ", "(", "1", ")", ";", 16u);
    function = ZrParser_Source_Compile(state, source, strlen(source),
            ZrCore_String_CreateFromNative(state, "ordinary.zr"));
    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Function_Free(state, function);
}

static void test_many_sibling_statements_do_not_exhaust_recursion_budget(void) {
    SZrParserState parser;
    SZrAstNode *ast;
    make_source("", "var x = 1; ", "return 1;", "", "", 1000u);
    ZrParser_State_Init(&parser, state, source, strlen(source), ZR_NULL);
    parser.suppressErrorOutput = ZR_TRUE;
    ast = ZrParser_ParseWithState(&parser);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE(parser.hasError);
    TEST_ASSERT_EQUAL_UINT32(0u, parser.recursionDepth);
    ZrParser_Ast_Free(state, ast);
    ZrParser_State_Free(&parser);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_deep_parentheses_and_all_parser_entry_points);
    RUN_TEST(test_recursive_operators_and_blocks_are_bounded);
    RUN_TEST(test_nested_types_are_bounded);
    RUN_TEST(test_template_interpolation_shares_recursion_budget);
    RUN_TEST(test_compiler_rejects_deep_source_and_accepts_ordinary_nesting);
    RUN_TEST(test_many_sibling_statements_do_not_exhaust_recursion_budget);
    return UNITY_END();
}
