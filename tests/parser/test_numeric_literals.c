#include "unity.h"
#include "tests/harness/runtime_support.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/lexer.h"
#include "zr_vm_core/function.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static SZrState *state;
void setUp(void) { state = ZrTests_Runtime_State_Create(ZR_NULL); TEST_ASSERT_NOT_NULL(state); }
void tearDown(void) { ZrTests_Runtime_State_Destroy(state); }

static void integer_token(const char *source, TZrInt64 expected) {
    size_t length = strlen(source);
    /* An exact-sized allocation makes accidental strto* reads past sourceLength visible to ASan. */
    char *slice = malloc(length);
    TEST_ASSERT_NOT_NULL(slice);
    memcpy(slice, source, length);
    SZrLexState lexer;
    ZrParser_Lexer_Init(&lexer, state, slice, length, ZR_NULL);
    TEST_ASSERT_EQUAL(ZR_TK_INTEGER, lexer.t.token);
    TEST_ASSERT_FALSE(lexer.t.hasLexError);
    TEST_ASSERT_EQUAL_INT64(expected, lexer.t.seminfo.intValue);
    ZrParser_Lexer_Free(&lexer);
    free(slice);
}

static void test_bounded_integer_tokens(void) {
    integer_token("9223372036854775807", INT64_MAX);
    integer_token("07", 7);
    integer_token("07\r\n", 7);
    integer_token("0777777777777777777777", INT64_MAX);
    integer_token("0xffffffffffffffff", -1);
}

static void test_bounded_float_token(void) {
    char *slice = malloc(4);
    TEST_ASSERT_NOT_NULL(slice);
    memcpy(slice, "1.25", 4);
    SZrLexState lexer;
    ZrParser_Lexer_Init(&lexer, state, slice, 4, ZR_NULL);
    TEST_ASSERT_EQUAL(ZR_TK_FLOAT, lexer.t.token);
    TEST_ASSERT_FALSE(lexer.t.hasLexError);
    TEST_ASSERT_EQUAL_DOUBLE(1.25, lexer.t.seminfo.floatValue);
    ZrParser_Lexer_Free(&lexer);
    free(slice);
}

static void test_invalid_literals_reject_compilation(void) {
    const char *literals[] = {"9223372036854775808", "9999999999999999999999999",
        "0x10000000000000000", "02000000000000000000000", "078", "0x", "1e", "1e+",
        "1e999", "1e-999", "1e39f"};
    for (size_t i = 0; i < sizeof(literals)/sizeof(literals[0]); ++i) {
        char source[128];
        snprintf(source, sizeof(source), "return %s;", literals[i]);
        SZrFunction *function = ZrParser_Source_Compile(state, source, strlen(source), ZR_NULL);
        TEST_ASSERT_NULL_MESSAGE(function, literals[i]);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bounded_integer_tokens);
    RUN_TEST(test_bounded_float_token);
    RUN_TEST(test_invalid_literals_reject_compilation);
    return UNITY_END();
}
