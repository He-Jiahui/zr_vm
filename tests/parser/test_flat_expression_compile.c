#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "unity.h"
#include "harness/runtime_support.h"
#include "harness/windows_crash_diagnostics.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"

static SZrState *state;
static char *source;

void setUp(void) {
    source = ZR_NULL;
    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
}

void tearDown(void) {
    free(source);
    ZrTests_Runtime_State_Destroy(state);
}

static void compile_and_expect(const char *text, TZrInt64 expected) {
    TZrInt64 result = 0;
    SZrFunction *function = ZrParser_Source_Compile(state, text, strlen(text),
            ZrCore_String_CreateFromNative(state, "flat_expression.zr"));
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(expected, result);
    ZrCore_Function_Free(state, function);
}

static double time_chain(size_t terms, int identifiers) {
    const char *prefix = identifiers ? "var x: int = 1; return " : "return ";
    size_t prefixLength = strlen(prefix);
    clock_t start;
    double elapsed;
    free(source);
    source = malloc(prefixLength + terms * 2 + 1);
    TEST_ASSERT_NOT_NULL(source);
    memcpy(source, prefix, prefixLength);
    for (size_t i = 0; i < terms; ++i) {
        source[prefixLength + i * 2] = identifiers ? 'x' : '1';
        source[prefixLength + i * 2 + 1] = i + 1 == terms ? ';' : '+';
    }
    source[prefixLength + terms * 2] = '\0';
    start = clock();
    compile_and_expect(source, (TZrInt64)terms);
    elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
    printf("flat expression: %s, %zu terms, %.4f CPU seconds\n",
           identifiers ? "identifiers" : "literals", terms, elapsed);
    return elapsed;
}

static void expect_scaling(int identifiers) {
    double small = time_chain(200, identifiers);
    double large = time_chain(600, identifiers);
    /* Generous slack for sanitizer builds and timer granularity. Previously
     * 200 literal terms took ~10s and 400 exceeded 15s in Release. */
    TEST_ASSERT_TRUE_MESSAGE(large < 5.0, "flat expression compilation exceeded five CPU seconds");
    TEST_ASSERT_TRUE_MESSAGE(large < small * 32.0 + 0.25,
                             "flat expression compilation growth regressed");
}

static void test_literal_chain_compile_time(void) { expect_scaling(0); }
static void test_identifier_chain_compile_time(void) { expect_scaling(1); }

static void test_cache_does_not_cross_assignments_or_function_scopes(void) {
    compile_and_expect("var x: int = 1; var first = x+x; x = 5; return x+x+first;", 12);
    compile_and_expect("fn twice(x: int): int { return x+x; } return twice(3)+twice(5);", 16);
    compile_and_expect("fn sum(x: int): int { if (x > 0 && x < 4) { return x+x; }"
                       "return x+x; } return sum(-5)+sum(2);", -6);
}

int main(void) {
    ZrTests_WindowsCrashDiagnostics_Install();
    setvbuf(stdout, ZR_NULL, _IONBF, 0);
    UNITY_BEGIN();
    RUN_TEST(test_literal_chain_compile_time);
    RUN_TEST(test_identifier_chain_compile_time);
    RUN_TEST(test_cache_does_not_cross_assignments_or_function_scopes);
    return UNITY_END();
}
