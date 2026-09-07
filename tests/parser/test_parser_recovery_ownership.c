#include "unity.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/parser.h"

#include <stdlib.h>
#include <string.h>

static SZrGlobalState *g_global;
static SZrParserState g_parser;
static SZrAstNode *g_ast;
static size_t g_liveBlocks;
static size_t g_errorCount;

static TZrPtr tracking_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize,
                                TZrSize newSize, TZrInt64 flag) {
    TZrPtr result;
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);
    if (newSize == 0) {
        if (pointer != ZR_NULL) {
            g_liveBlocks--;
        }
        free(pointer);
        return ZR_NULL;
    }
    if (pointer == ZR_NULL) {
        result = malloc(newSize);
        if (result != ZR_NULL) {
            g_liveBlocks++;
        }
        return result;
    }
    return realloc(pointer, newSize);
}

static void capture_error(TZrPtr userData, const SZrStructuredDiagnostic *diagnostic, EZrToken token) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(token);
    if (diagnostic != ZR_NULL && diagnostic->severity == ZR_STRUCTURED_DIAGNOSTIC_ERROR) {
        g_errorCount++;
    }
}

static void capture_legacy_error(TZrPtr userData, const SZrFileRange *location,
                                 const TZrChar *message, EZrToken token) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(location);
    ZR_UNUSED_PARAMETER(message);
    ZR_UNUSED_PARAMETER(token);
    g_errorCount++;
}

void setUp(void) {
    SZrCallbackGlobal callbacks = {0};
    g_liveBlocks = 0;
    g_errorCount = 0;
    g_ast = ZR_NULL;
    memset(&g_parser, 0, sizeof(g_parser));
    g_global = ZrCore_GlobalState_New(tracking_allocator, ZR_NULL, 0, &callbacks);
    TEST_ASSERT_NOT_NULL(g_global);
}

void tearDown(void) {
    if (g_global != ZR_NULL) {
        ZrParser_Ast_Free(g_global->mainThreadState, g_ast);
        ZrParser_State_Free(&g_parser);
        ZrCore_GlobalState_Free(g_global);
        g_global = ZR_NULL;
    }
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0, g_liveBlocks, "parser recovery must release every allocation");
}

static void parse_source(const char *source, TZrBool expectError) {
    SZrState *state = g_global->mainThreadState;
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "parser-recovery.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ZrParser_State_Init(&g_parser, state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(g_parser.lexer);
    g_parser.structuredErrorCallback = capture_error;
    g_parser.errorCallback = capture_legacy_error;
    g_parser.suppressErrorOutput = ZR_TRUE;
    g_ast = ZrParser_ParseWithState(&g_parser);
    if (expectError) {
        TEST_ASSERT_TRUE_MESSAGE(g_errorCount > 0, "malformed fixture must report a parser error");
    } else {
        TEST_ASSERT_NOT_NULL(g_ast);
        TEST_ASSERT_EQUAL_UINT64(0, g_errorCount);
    }
}

static void test_complete_constructs_release_children(void) {
    parse_source("fn pick(value: int): int { return value; } return [1, {a: 2}, (3 + 4)];", ZR_FALSE);
}

static void test_array_missing_close_releases_elements(void) {
    parse_source("return [1, 2", ZR_TRUE);
}

static void test_array_missing_separator_releases_elements(void) {
    parse_source("return [1 2];", ZR_TRUE);
}

static void test_array_invalid_later_element_releases_previous_elements(void) {
    parse_source("return [1, +];", ZR_TRUE);
}

static void test_object_missing_close_releases_properties(void) {
    parse_source("return {a: 1", ZR_TRUE);
}

static void test_object_missing_separator_releases_properties(void) {
    parse_source("return {a: 1 \"b\": 2};", ZR_TRUE);
}

static void test_object_computed_key_missing_close_releases_key(void) {
    parse_source("return {[1: 2};", ZR_TRUE);
}

static void test_object_later_computed_key_missing_close_releases_children(void) {
    parse_source("return {a: 1, [2: 3};", ZR_TRUE);
}

static void test_object_missing_first_value_releases_key(void) {
    parse_source("return {a: };", ZR_TRUE);
}

static void test_object_missing_later_value_releases_key(void) {
    parse_source("return {a: 1, b: };", ZR_TRUE);
}

static void test_group_missing_close_releases_expression(void) {
    parse_source("return (1 + 2;", ZR_TRUE);
}

static void test_function_missing_parameter_close_releases_parameters(void) {
    parse_source("fn pick(value: int: int { return value; }", ZR_TRUE);
}

static void test_function_missing_body_releases_signature(void) {
    parse_source("fn pick<T>(value: T): T;", ZR_TRUE);
}

static void test_function_missing_variadic_close_releases_parameter(void) {
    parse_source("fn pick(params values: int: int { return 1; }", ZR_TRUE);
}

static void test_parameter_missing_name_releases_decorators(void) {
    parse_source("fn pick(,): int { return 1; }", ZR_TRUE);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_complete_constructs_release_children);
    RUN_TEST(test_array_missing_close_releases_elements);
    RUN_TEST(test_array_missing_separator_releases_elements);
    RUN_TEST(test_array_invalid_later_element_releases_previous_elements);
    RUN_TEST(test_object_missing_close_releases_properties);
    RUN_TEST(test_object_missing_separator_releases_properties);
    RUN_TEST(test_object_computed_key_missing_close_releases_key);
    RUN_TEST(test_object_later_computed_key_missing_close_releases_children);
    RUN_TEST(test_object_missing_first_value_releases_key);
    RUN_TEST(test_object_missing_later_value_releases_key);
    RUN_TEST(test_group_missing_close_releases_expression);
    RUN_TEST(test_function_missing_parameter_close_releases_parameters);
    RUN_TEST(test_function_missing_body_releases_signature);
    RUN_TEST(test_function_missing_variadic_close_releases_parameter);
    RUN_TEST(test_parameter_missing_name_releases_decorators);
    return UNITY_END();
}
