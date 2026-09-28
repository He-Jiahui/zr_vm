#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/type_system.h"

static SZrState *g_state;
static SZrCompilerState g_compiler;
static TZrBool g_compilerInitialized;
static SZrAstNode *g_ast;
static SZrFunction *g_runtimeFunction;

void setUp(void) {
    memset(&g_compiler, 0, sizeof(g_compiler));
    g_compilerInitialized = ZR_FALSE;
    g_ast = ZR_NULL;
    g_runtimeFunction = ZR_NULL;
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_runtimeFunction != ZR_NULL) {
        ZrCore_Function_Free(g_state, g_runtimeFunction);
    }
    if (g_compilerInitialized) {
        if (g_compiler.currentFunction != ZR_NULL) {
            ZrCore_Function_Free(g_state, g_compiler.currentFunction);
            g_compiler.currentFunction = ZR_NULL;
        }
        ZrParser_CompilerState_Free(&g_compiler);
    }
    if (g_ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_ast);
    }
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static void prepare_source(const TZrChar *source, TZrSize sourceLength,
                           TZrChar *sourceName) {
    SZrString *name = ZrCore_String_CreateFromNative(g_state, sourceName);

    TEST_ASSERT_NOT_NULL(name);
    g_ast = ZrParser_Parse(g_state, source, sourceLength, name);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_NOT_NULL(g_ast->data.script.statements);
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compilerInitialized = ZR_TRUE;
    g_compiler.currentAst = g_ast;
    g_compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(g_compiler.currentFunction);
}

static void compile_statement_at(TZrSize index) {
    TEST_ASSERT_TRUE(index < g_ast->data.script.statements->count);
    ZrParser_Statement_Compile(
            &g_compiler, g_ast->data.script.statements->nodes[index]);
    TEST_ASSERT_FALSE_MESSAGE(g_compiler.hasError, g_compiler.errorMessage);
}

static const SZrTypeBinding *find_error_binding(void) {
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "error");
    TEST_ASSERT_NOT_NULL(name);
    return ZrParser_TypeEnvironment_FindVariableBinding(
            g_compiler.typeEnv, name);
}

static void test_lambda_parameter_does_not_escape_to_root_type_environment(void) {
    static const TZrChar source[] =
            "var callback = fn(error: int) => { return error; };\n";
    static TZrChar sourceName[] = "lambda_parameter_scope.zr";
    SZrTypeEnvironment *rootEnvironment;

    prepare_source(source, sizeof(source) - 1U, sourceName);
    rootEnvironment = g_compiler.typeEnv;
    compile_statement_at(0U);

    TEST_ASSERT_EQUAL_PTR(rootEnvironment, g_compiler.typeEnv);
    TEST_ASSERT_NULL(find_error_binding());
}

static void test_lambda_parameter_keeps_outer_binding_identity_and_range(void) {
    static const TZrChar source[] =
            "var error: int = 7;\n"
            "var callback = fn(error: int) => { return error; };\n";
    static TZrChar sourceName[] = "lambda_parameter_shadow.zr";
    SZrTypeEnvironment *rootEnvironment;
    const SZrTypeBinding *binding;
    TZrSymbolId outerSymbolId;
    SZrFileRange outerRange;

    prepare_source(source, sizeof(source) - 1U, sourceName);
    rootEnvironment = g_compiler.typeEnv;
    compile_statement_at(0U);
    binding = find_error_binding();
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, binding->symbolId);
    TEST_ASSERT_TRUE(binding->hasDeclarationRange);
    outerSymbolId = binding->symbolId;
    outerRange = binding->declarationRange;

    compile_statement_at(1U);
    TEST_ASSERT_EQUAL_PTR(rootEnvironment, g_compiler.typeEnv);
    binding = find_error_binding();
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_UINT32(outerSymbolId, binding->symbolId);
    TEST_ASSERT_TRUE(binding->hasDeclarationRange);
    TEST_ASSERT_EQUAL_PTR(outerRange.source, binding->declarationRange.source);
    TEST_ASSERT_EQUAL_INT32(
            outerRange.start.line, binding->declarationRange.start.line);
    TEST_ASSERT_EQUAL_INT32(
            outerRange.start.column, binding->declarationRange.start.column);
    TEST_ASSERT_EQUAL_INT32(
            outerRange.end.line, binding->declarationRange.end.line);
    TEST_ASSERT_EQUAL_INT32(
            outerRange.end.column, binding->declarationRange.end.column);
}

static void test_lambda_parameter_scope_restores_after_foreach_error(void) {
    static const TZrChar source[] =
            "var callback = fn(error: int) => {\n"
            "  var values = [true];\n"
            "  for (var item in values) { break 1; }\n"
            "};\n";
    static TZrChar sourceName[] = "lambda_parameter_foreach_error_scope.zr";
    SZrTypeEnvironment *rootEnvironment;
    TZrSize initialTypeEnvStackLength;

    prepare_source(source, sizeof(source) - 1U, sourceName);
    rootEnvironment = g_compiler.typeEnv;
    initialTypeEnvStackLength = g_compiler.typeEnvStack.length;
    TEST_ASSERT_TRUE(g_ast->data.script.statements->count > 0U);
    ZrParser_Statement_Compile(
            &g_compiler, g_ast->data.script.statements->nodes[0]);

    TEST_ASSERT_TRUE(g_compiler.hasError);
    TEST_ASSERT_EQUAL_PTR(rootEnvironment, g_compiler.typeEnv);
    TEST_ASSERT_EQUAL_size_t(
            initialTypeEnvStackLength, g_compiler.typeEnvStack.length);
    TEST_ASSERT_NULL(find_error_binding());
}

static void test_nested_parameter_lambda_retains_parent_capture(void) {
    static const TZrChar source[] =
            "fn makeRunner() {\n"
            "  var seed: int = 4;\n"
            "  return fn(delta: int) => { return seed + delta; };\n"
            "}\n"
            "var runner = makeRunner();\n"
            "return runner(1);\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "nested_lambda_parent_capture.zr");
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(sourceName);
    g_runtimeFunction = ZrParser_Source_Compile(
            g_state, source, sizeof(source) - 1U, sourceName);
    TEST_ASSERT_NOT_NULL(g_runtimeFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, g_runtimeFunction, &result));
    TEST_ASSERT_EQUAL_INT64(5, result);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_lambda_parameter_does_not_escape_to_root_type_environment);
    RUN_TEST(test_lambda_parameter_keeps_outer_binding_identity_and_range);
    RUN_TEST(test_lambda_parameter_scope_restores_after_foreach_error);
    RUN_TEST(test_nested_parameter_lambda_retains_parent_capture);
    return UNITY_END();
}
