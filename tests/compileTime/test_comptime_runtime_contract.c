#include "unity.h"

#include <string.h>

#include "runtime_support.h"
#include "compile_time_executor_internal.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static SZrAstNode *parse_source(const TZrChar *source, const TZrChar *sourceNameText) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, (TZrNativeString)sourceNameText);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(ast);
    return ast;
}

static TZrBool prepare_and_run_late_checks(
        SZrCompilerState *compiler,
        SZrAstNode *ast) {
    if (!ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
                compiler, ast)) {
        return ZR_FALSE;
    }
    compiler->compilePhase = ZR_PARSER_COMPILE_PHASE_LATE_CHECK;
    return ZrParser_CompileTime_ExecuteLateChecksInCompilerState(
            compiler, ast);
}

static void test_typed_compile_tool_assert_and_warning_run_in_check_context(void) {
    static const TZrChar *source =
            "let compile = import(\"zr.compile\");\n"
            "comptime {\n"
            "    compile.assert(true, \"layout ok\");\n"
            "    compile.warning(\"alignment warning\");\n"
            "}\n"
            "return 42;\n";
    SZrAstNode *ast = parse_source(source, "comptime_typed_diagnostics.zr");
    SZrCompilerState compiler;

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_TRUE(prepare_and_run_late_checks(&compiler, ast));
    TEST_ASSERT_FALSE(compiler.hasCompileTimeError);
    TEST_ASSERT_EQUAL_UINT64(
            1U, compiler.comptimeBudget.usage.diagnosticCount);

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_tool_error_stops_build_with_typed_diagnostic(void) {
    static const TZrChar *source =
            "let compile = import(\"zr.compile\");\n"
            "comptime { compile.error(\"schema mismatch\"); }\n"
            "return 0;\n";
    SZrAstNode *ast = parse_source(source, "comptime_typed_error.zr");
    SZrCompilerState compiler;

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_FALSE(prepare_and_run_late_checks(&compiler, ast));
    TEST_ASSERT_TRUE(compiler.hasCompileTimeError);
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(compiler.errorMessage, "schema mismatch"));
    TEST_ASSERT_EQUAL_UINT64(
            1U, compiler.comptimeBudget.usage.diagnosticCount);

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_diagnostic_effect_is_rejected_in_pure_build_predicate(void) {
    static const TZrChar *source =
            "let compile = import(\"zr.compile\");\n"
            "comptime if (compile.assert(true, \"not pure\")) {\n"
            "    fn selected(): int { return 1; }\n"
            "}\n"
            "return 0;\n";
    SZrAstNode *ast = parse_source(source, "comptime_effect_violation.zr");
    SZrCompilerState compiler;

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_FALSE(ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
            &compiler, ast));
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(
            compiler.errorMessage, "comptime.effect_violation"));

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_fuel_budget_stops_evaluation_without_partial_overrun(void) {
    static const TZrChar *source =
            "comptime if ((((1 + 2) + 3) + 4) > 0) {\n"
            "    fn selected(): int { return 1; }\n"
            "}\n"
            "return 0;\n";
    SZrAstNode *ast = parse_source(source, "comptime_fuel_budget.zr");
    SZrCompilerState compiler;

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.comptimeBudget.limits.fuel = 4U;
    TEST_ASSERT_FALSE(ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
            &compiler, ast));
    TEST_ASSERT_EQUAL_UINT64(4U, compiler.comptimeBudget.usage.fuel);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_COMPTIME_BUDGET_FUEL,
            compiler.comptimeBudget.exceededResource);
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(
            compiler.errorMessage, "comptime.budget_exceeded"));

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_call_depth_budget_stops_recursive_comptime_function(void) {
    static const TZrChar *source =
            "comptime fn descend(value: int): int {\n"
            "    if (value == 0) { return 0; }\n"
            "    return descend(value - 1);\n"
            "}\n"
            "comptime { let result = descend(8); }\n"
            "return 0;\n";
    SZrAstNode *ast = parse_source(source, "comptime_call_depth.zr");
    SZrCompilerState compiler;

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.comptimeBudget.limits.callDepth = 3U;
    TEST_ASSERT_FALSE(prepare_and_run_late_checks(&compiler, ast));
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_COMPTIME_BUDGET_CALL_DEPTH,
            compiler.comptimeBudget.exceededResource);
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(
            compiler.errorMessage, "comptime.budget_exceeded"));

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_pure_comptime_function_results_use_deterministic_cache(void) {
    static const TZrChar *source =
            "comptime fn add(left: int, right: int): int {\n"
            "    return left + right;\n"
            "}\n"
            "comptime {\n"
            "    let first = add(20, 22);\n"
            "    let second = add(20, 22);\n"
            "    let third = add(21, 21);\n"
            "}\n"
            "return 0;\n";
    SZrAstNode *ast = parse_source(source, "comptime_deterministic_cache.zr");
    SZrCompilerState compiler;

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_TRUE(prepare_and_run_late_checks(&compiler, ast));
    TEST_ASSERT_EQUAL_UINT64(1U, compiler.comptimeCacheHitCount);
    TEST_ASSERT_EQUAL_UINT64(2U, compiler.comptimeCacheMissCount);

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_removed_global_assert_and_fatal_error_are_not_builtins(void) {
    static const TZrChar *sources[] = {
            "comptime { Assert(true, \"legacy\"); }\nreturn 0;\n",
            "comptime { FatalError(\"legacy\"); }\nreturn 0;\n"};

    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(sources); index++) {
        SZrAstNode *ast = parse_source(sources[index], "comptime_removed_builtin.zr");
        SZrCompilerState compiler;

        ZrParser_CompilerState_Init(&compiler, g_state);
        compiler.suppressErrorOutput = ZR_TRUE;
        TEST_ASSERT_FALSE(prepare_and_run_late_checks(&compiler, ast));
        TEST_ASSERT_NOT_NULL(compiler.errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(
                compiler.errorMessage, "Unknown compile-time identifier"));
        ZrParser_CompilerState_Free(&compiler);
        ZrParser_Ast_Free(g_state, ast);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_typed_compile_tool_assert_and_warning_run_in_check_context);
    RUN_TEST(test_compile_tool_error_stops_build_with_typed_diagnostic);
    RUN_TEST(test_diagnostic_effect_is_rejected_in_pure_build_predicate);
    RUN_TEST(test_fuel_budget_stops_evaluation_without_partial_overrun);
    RUN_TEST(test_call_depth_budget_stops_recursive_comptime_function);
    RUN_TEST(test_pure_comptime_function_results_use_deterministic_cache);
    RUN_TEST(test_removed_global_assert_and_fatal_error_are_not_builtins);
    return UNITY_END();
}
