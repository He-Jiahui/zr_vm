/* Regression coverage for descriptor minimum-phase enforcement. */
#include "unity.h"

#include <string.h>

#include "runtime_support.h"
#include "compiler/tool/compile_tool_binding.h"
#include "compiler/tool/compile_tool_evaluator.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/compile_tool.h"
#include "zr_vm_parser/parser.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static TZrBool evaluate_warning_at(
        EZrParserCompilePhase phase,
        EZrParserComptimeContext context,
        TZrBool *outHandled,
        SZrCompilerState *outCompiler) {
    static const TZrChar source[] =
            "compile.warning(\"descriptor-warning\");\n";
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *statement;
    SZrTypeValue result;
    SZrString *alias;
    const SZrParserCompileToolModuleDescriptor *provider;
    SZrCompilerState *compiler = outCompiler;

    if (outHandled != ZR_NULL) {
        *outHandled = ZR_FALSE;
    }
    if (compiler == ZR_NULL || g_state == ZR_NULL) {
        return ZR_FALSE;
    }
    sourceName = ZrCore_String_CreateFromNative(g_state, "compile_tool_phase_gate.zr");
    if (sourceName == ZR_NULL) {
        return ZR_FALSE;
    }
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    if (ast == ZR_NULL || ast->data.script.statements == ZR_NULL ||
        ast->data.script.statements->count != 1U) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(g_state, ast);
        }
        return ZR_FALSE;
    }
    statement = ast->data.script.statements->nodes[0];
    if (statement == ZR_NULL || statement->type != ZR_AST_EXPRESSION_STATEMENT ||
        statement->data.expressionStatement.expr == ZR_NULL) {
        ZrParser_Ast_Free(g_state, ast);
        return ZR_FALSE;
    }

    ZrParser_CompilerState_Init(compiler, g_state);
    compiler->suppressErrorOutput = ZR_TRUE;
    compiler->compilePhase = phase;
    compiler->comptimeContext = context;
    alias = ZrCore_String_CreateFromNative(g_state, "compile");
    provider = ZrParser_CompileTool_FindModule(
            ZR_PARSER_COMPILE_TOOL_MODULE_BUILD);
    if (alias == ZR_NULL || provider == ZR_NULL ||
        !ZrParser_CompileToolBinding_DeclareProvider(
                compiler, alias, provider)) {
        ZrParser_CompilerState_Free(compiler);
        ZrParser_Ast_Free(g_state, ast);
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(&result);
    {
        TZrBool handled = ZR_FALSE;
        TZrBool evaluated = ZrParser_CompileToolEvaluator_TryEvaluate(
                compiler,
                statement->data.expressionStatement.expr,
                ZR_NULL,
                &result,
                &handled);
        if (outHandled != ZR_NULL) {
            *outHandled = handled;
        }
        ZrParser_Ast_Free(g_state, ast);
        return evaluated;
    }
}

static void test_builtin_minimum_phases_are_ordered(void) {
    const SZrParserCompileToolModuleDescriptor *build =
            ZrParser_CompileTool_FindModule(ZR_PARSER_COMPILE_TOOL_MODULE_BUILD);
    const SZrParserCompileToolModuleDescriptor *declaration =
            ZrParser_CompileTool_FindModule(
                    ZR_PARSER_COMPILE_TOOL_MODULE_DECLARATION);
    const SZrParserCompileToolCallableDescriptor *feature;
    const SZrParserCompileToolCallableDescriptor *assertCallable;
    const SZrParserCompileToolTypeDescriptor *generatedField;

    TEST_ASSERT_NOT_NULL(build);
    TEST_ASSERT_NOT_NULL(declaration);
    feature = ZrParser_CompileTool_FindCallable(
            build, ZR_PARSER_COMPILE_TOOL_ROLE_BUILD_FEATURE);
    assertCallable = ZrParser_CompileTool_FindCallable(
            build, ZR_PARSER_COMPILE_TOOL_ROLE_ASSERT);
    TEST_ASSERT_NOT_NULL(feature);
    TEST_ASSERT_NOT_NULL(assertCallable);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_COMPILE_PHASE_BUILD_FACTS,
            feature->minimumPhase);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_COMPILE_PHASE_LATE_CHECK,
            assertCallable->minimumPhase);
    generatedField = ZrParser_CompileTool_FindType(
            declaration, "GeneratedField");
    TEST_ASSERT_NOT_NULL(generatedField);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_COMPILE_PHASE_EXPANSION,
            generatedField->minimumPhase);
    TEST_ASSERT_FALSE(ZrParser_CompilePhase_IsAtLeast(
            ZR_PARSER_COMPILE_PHASE_SYNTAX_EXPANSION,
            generatedField->minimumPhase));
    TEST_ASSERT_TRUE(ZrParser_CompilePhase_IsAtLeast(
            ZR_PARSER_COMPILE_PHASE_LAYOUT,
            generatedField->minimumPhase));
}

static void test_warning_is_rejected_before_late_check(void) {
    SZrCompilerState compiler;
    TZrBool handled = ZR_FALSE;

    TEST_ASSERT_FALSE(evaluate_warning_at(
            ZR_PARSER_COMPILE_PHASE_SYNTAX_EXPANSION,
            ZR_PARSER_COMPTIME_CONTEXT_SYNTAX_EXPANSION,
            &handled,
            &compiler));
    TEST_ASSERT_TRUE(handled);
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(compiler.errorMessage, "phase_violation"));
    TEST_ASSERT_NULL(strstr(compiler.errorMessage, "descriptor-warning"));
    ZrParser_CompilerState_Free(&compiler);

    TEST_ASSERT_FALSE(evaluate_warning_at(
            ZR_PARSER_COMPILE_PHASE_LAYOUT,
            ZR_PARSER_COMPTIME_CONTEXT_CHECK,
            &handled,
            &compiler));
    TEST_ASSERT_TRUE(handled);
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(compiler.errorMessage, "phase_violation"));
    TEST_ASSERT_NULL(strstr(compiler.errorMessage, "descriptor-warning"));
    ZrParser_CompilerState_Free(&compiler);
}

static void test_effect_permission_precedes_phase_gate(void) {
    SZrCompilerState compiler;
    TZrBool handled = ZR_FALSE;

    TEST_ASSERT_FALSE(evaluate_warning_at(
            ZR_PARSER_COMPILE_PHASE_BUILD_FACTS,
            ZR_PARSER_COMPTIME_CONTEXT_PURE_VALUE,
            &handled,
            &compiler));
    TEST_ASSERT_TRUE(handled);
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(compiler.errorMessage, "effect_violation"));
    TEST_ASSERT_NULL(strstr(compiler.errorMessage, "phase_violation"));
    TEST_ASSERT_NULL(strstr(compiler.errorMessage, "descriptor-warning"));
    ZrParser_CompilerState_Free(&compiler);
}

static void test_warning_is_allowed_at_late_check(void) {
    SZrCompilerState compiler;
    TZrBool handled = ZR_FALSE;

    TEST_ASSERT_TRUE(evaluate_warning_at(
            ZR_PARSER_COMPILE_PHASE_LATE_CHECK,
            ZR_PARSER_COMPTIME_CONTEXT_CHECK,
            &handled,
            &compiler));
    TEST_ASSERT_TRUE(handled);
    TEST_ASSERT_FALSE(compiler.hasCompileTimeError);
    ZrParser_CompilerState_Free(&compiler);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_builtin_minimum_phases_are_ordered);
    RUN_TEST(test_warning_is_rejected_before_late_check);
    RUN_TEST(test_effect_permission_precedes_phase_gate);
    RUN_TEST(test_warning_is_allowed_at_late_check);
    return UNITY_END();
}
