#include "unity.h"

#include <string.h>

#include "runtime_support.h"
#include "compiler/compile_time/compile_time_executor_internal.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static SZrAstNode *parse_source(const TZrChar *source) {
    SZrString *name = ZrCore_String_CreateFromNative(
            g_state, "comptime_control_flow.zr");
    SZrParserState parser;
    ZrParser_State_Init(&parser, g_state, source, strlen(source), name);
    parser.suppressErrorOutput = ZR_TRUE;
    SZrAstNode *ast = ZrParser_ParseWithState(&parser);
    TZrBool hadError = parser.hasError;
    ZrParser_State_Free(&parser);
    TEST_ASSERT_FALSE_MESSAGE(hadError, "Control-flow fixture contains a syntax error");
    TEST_ASSERT_NOT_NULL_MESSAGE(ast, "Control-flow fixture must parse as ZR");
    return ast;
}

static TZrBool run_checks(SZrCompilerState *compiler, SZrAstNode *ast) {
    if (!ZrParser_CompileTime_PrepareBuildFactsInCompilerState(compiler, ast)) {
        return ZR_FALSE;
    }
    compiler->compilePhase = ZR_PARSER_COMPILE_PHASE_LATE_CHECK;
    return ZrParser_CompileTime_ExecuteLateChecksInCompilerState(compiler, ast);
}

static void assert_checks_succeed(const TZrChar *source) {
    SZrAstNode *ast = parse_source(source);
    SZrCompilerState compiler;
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TZrBool succeeded = run_checks(&compiler, ast);
    TZrBool hadError = compiler.hasCompileTimeError;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
    TEST_ASSERT_TRUE_MESSAGE(succeeded, "Compile-time assertion did not execute successfully");
    TEST_ASSERT_FALSE(hadError);
}

static void test_while_updates_outer_binding(void) {
    assert_checks_succeed(
            "let compile = import(\"zr.compile\");\n"
            "comptime fn sum(limit: int): int {\n"
            "    var total = 0; var i = 0;\n"
            "    while (i < limit) { total += i; i += 1; }\n"
            "    return total;\n"
            "}\n"
            "comptime { compile.assert(sum(4) == 6, \"while sum\"); }\n"
            "return 0;\n");
}

static void test_for_continue_runs_step_and_break_stops_loop(void) {
    assert_checks_succeed(
            "let compile = import(\"zr.compile\");\n"
            "comptime fn sum(): int {\n"
            "    var total = 0;\n"
            "    for (var i = 0; i < 8; i += 1) {\n"
            "        if (i == 2) { continue; }\n"
            "        if (i == 5) { break; }\n"
            "        total += i;\n"
            "    }\n"
            "    return total;\n"
            "}\n"
            "comptime { compile.assert(sum() == 8, \"for flow\"); }\n"
            "return 0;\n");
}

static void test_foreach_iterates_array_once_per_element(void) {
    assert_checks_succeed(
            "let compile = import(\"zr.compile\");\n"
            "comptime fn sum(): int {\n"
            "    var total = 0;\n"
            "    for (let value in [2, 3, 5]) { total += value; }\n"
            "    return total;\n"
            "}\n"
            "comptime { compile.assert(sum() == 10, \"foreach sum\"); }\n"
            "return 0;\n");
}

static void test_foreach_binds_destructured_elements(void) {
    assert_checks_succeed(
            "let compile = import(\"zr.compile\");\n"
            "comptime fn sum(): int {\n"
            "    var total = 0;\n"
            "    for (let [a, b] in [[1, 2], [3, 4]]) { total += a + b; }\n"
            "    return total;\n"
            "}\n"
            "comptime { compile.assert(sum() == 10, \"destructuring\"); }\n"
            "return 0;\n");
}

static void test_nested_loop_break_is_local(void) {
    assert_checks_succeed(
            "let compile = import(\"zr.compile\");\n"
            "comptime fn count(): int {\n"
            "    var count = 0;\n"
            "    for (var i = 0; i < 3; i += 1) {\n"
            "        for (var j = 0; j < 4; j += 1) {\n"
            "            if (j == 2) { break; } count += 1;\n"
            "        }\n"
            "    }\n"
            "    return count;\n"
            "}\n"
            "comptime { compile.assert(count() == 6, \"nested break\"); }\n"
            "return 0;\n");
}

static void test_return_unwinds_nested_loops(void) {
    assert_checks_succeed(
            "let compile = import(\"zr.compile\");\n"
            "comptime fn first(): int {\n"
            "    for (var i = 0; i < 3; i += 1) {\n"
            "        while (true) { if (i == 1) { return 42; } break; }\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "comptime { compile.assert(first() == 42, \"nested return\"); }\n"
            "return 0;\n");
}

static void test_blocks_preserve_shadowed_outer_binding(void) {
    assert_checks_succeed(
            "let compile = import(\"zr.compile\");\n"
            "comptime fn scopedValue(): int {\n"
            "    var value = 1;\n"
            "    if (true) { var value = 8; value += 1; }\n"
            "    return value;\n"
            "}\n"
            "comptime { compile.assert(scopedValue() == 1, \"block shadow\"); }\n"
            "return 0;\n");
}

static void test_switch_selects_one_case_or_default(void) {
    assert_checks_succeed(
            "let compile = import(\"zr.compile\");\n"
            "comptime fn choose(value: int): int {\n"
            "    switch (value) {\n"
            "        (1) { return 11; }\n"
            "        (2) { return 22; }\n"
            "        () { return 33; }\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "comptime {\n"
            "    compile.assert(choose(2) == 22, \"switch case\");\n"
            "    compile.assert(choose(9) == 33, \"switch default\");\n"
            "}\n"
            "return 0;\n");
}

static void test_infinite_loop_terminates_at_fuel_limit(void) {
    SZrAstNode *ast = parse_source("comptime { while (true) {} } return 0;");
    SZrCompilerState compiler;
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.comptimeBudget.limits.fuel = 64U;
    TZrBool succeeded = run_checks(&compiler, ast);
    EZrParserComptimeBudgetResource resource = compiler.comptimeBudget.exceededResource;
    TZrUInt64 used = compiler.comptimeBudget.usage.fuel;
    TZrBool budgetDiagnostic = compiler.errorMessage != ZR_NULL &&
            strstr(compiler.errorMessage, "comptime.budget_exceeded") != ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
    TEST_ASSERT_FALSE(succeeded);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_FUEL, resource);
    TEST_ASSERT_EQUAL_UINT64(64U, used);
    TEST_ASSERT_TRUE(budgetDiagnostic);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_while_updates_outer_binding);
    RUN_TEST(test_for_continue_runs_step_and_break_stops_loop);
    RUN_TEST(test_foreach_iterates_array_once_per_element);
    RUN_TEST(test_foreach_binds_destructured_elements);
    RUN_TEST(test_nested_loop_break_is_local);
    RUN_TEST(test_return_unwinds_nested_loops);
    RUN_TEST(test_blocks_preserve_shadowed_outer_binding);
    RUN_TEST(test_switch_selects_one_case_or_default);
    RUN_TEST(test_infinite_loop_terminates_at_fuel_limit);
    return UNITY_END();
}
