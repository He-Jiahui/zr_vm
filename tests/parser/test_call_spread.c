#include "unity.h"

#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/writer.h"
#include "zr_vm_parser.h"

static SZrState *g_state;

static TZrBool function_contains_opcode(
        const SZrFunction *function,
        EZrInstructionCode opcode) {
    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0u;
         index < function->instructionsLength;
         index++) {
        if (function->instructionsList[index].instruction.operationCode ==
            (TZrUInt16)opcode) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool function_contains_semir_opcode(
        const SZrFunction *function,
        EZrSemIrOpcode opcode) {
    if (function == ZR_NULL || function->semIrInstructions == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0u;
         index < function->semIrInstructionLength;
         index++) {
        if ((EZrSemIrOpcode)function->semIrInstructions[index].opcode == opcode) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

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

static void test_call_spread_has_dedicated_argument_ast(void) {
    const char *source =
            "fn collect(...values: int): int { return 1; }\n"
            "collect(...[1, 2]);\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "call_spread_ast.zr");
    SZrAstNode *script = ZrParser_Parse(
            g_state, source, strlen(source), sourceName);
    SZrAstNode *statement;
    SZrAstNode *expression;
    SZrAstNode *call;
    SZrAstNode *spread;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(
            2u, (TZrUInt32)script->data.script.statements->count);

    statement = script->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(statement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, statement->type);
    expression = statement->data.expressionStatement.expr;
    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expression->type);
    TEST_ASSERT_NOT_NULL(expression->data.primaryExpression.members);
    TEST_ASSERT_EQUAL_UINT32(
            1u, (TZrUInt32)expression->data.primaryExpression.members->count);

    call = expression->data.primaryExpression.members->nodes[0];
    TEST_ASSERT_NOT_NULL(call);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, call->type);
    TEST_ASSERT_NOT_NULL(call->data.functionCall.args);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)call->data.functionCall.args->count);

    spread = call->data.functionCall.args->nodes[0];
    TEST_ASSERT_NOT_NULL(spread);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SPREAD_ARGUMENT, spread->type);
    TEST_ASSERT_NOT_NULL(spread->data.spreadArgument.expression);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_ARRAY_LITERAL, spread->data.spreadArgument.expression->type);

    ZrParser_Ast_Free(g_state, script);
}

static void test_return_call_spread_preserves_argument_ast(void) {
    const char *source =
            "func sum(a: int, b: int, c: int): int { return a + b + c; }\n"
            "return sum(...[1, 2, 3]);\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "call_spread_return_ast.zr");
    SZrAstNode *script = ZrParser_Parse(
            g_state, source, strlen(source), sourceName);
    SZrAstNode *statement;
    SZrAstNode *expression;
    SZrAstNode *call;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(
            2u, (TZrUInt32)script->data.script.statements->count);

    statement = script->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(statement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, statement->type);
    expression = statement->data.returnStatement.expr;
    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expression->type);
    TEST_ASSERT_NOT_NULL(expression->data.primaryExpression.members);
    TEST_ASSERT_EQUAL_UINT32(
            1u, (TZrUInt32)expression->data.primaryExpression.members->count);

    call = expression->data.primaryExpression.members->nodes[0];
    TEST_ASSERT_NOT_NULL(call);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, call->type);
    TEST_ASSERT_NOT_NULL(call->data.functionCall.args);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)call->data.functionCall.args->count);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_SPREAD_ARGUMENT,
            call->data.functionCall.args->nodes[0]->type);

    ZrParser_Ast_Free(g_state, script);
}

static void test_call_spread_executes_dynamic_array_arguments(void) {
    const char *source =
            "func sum(a: int, b: int, c: int): int {\n"
            "    return a + b + c;\n"
            "}\n"
            "var values = [10, 20, 12];\n"
            "return sum(...values);\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "call_spread_runtime.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_contains_opcode(
            function, ZR_INSTRUCTION_ENUM(FUNCTION_CALL_SPREAD)));
    TEST_ASSERT_TRUE(function_contains_semir_opcode(
            function, ZR_SEMIR_OPCODE_DYN_CALL_SPREAD));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(42, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_call_spread_struct_values_survive_gc_during_expansion(void) {
    const char *source =
            "struct Pair {\n"
            "    pub var value: int;\n"
            "    pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "func sum(a: Pair, b: Pair, c: Pair): int {\n"
            "    return a.value + b.value + c.value;\n"
            "}\n"
            "var values = [$Pair(10), $Pair(20), $Pair(12)];\n"
            "return sum(...values);\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "call_spread_struct_gc.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    ZrCore_GarbageCollector_SetHeapLimitBytes(g_state->global, 1u);
    ZrCore_GarbageCollector_ScheduleCollection(
            g_state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_FULL);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(42, result);

    ZrCore_Function_Free(g_state, function);
}

static void assert_source_executes_to_int64(
        const char *source,
        const char *sourceNameText,
        TZrInt64 expected) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, (TZrNativeString)sourceNameText);
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_contains_opcode(
            function, ZR_INSTRUCTION_ENUM(FUNCTION_CALL_SPREAD)));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(expected, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_call_spread_supports_fixed_prefix(void) {
    assert_source_executes_to_int64(
            "func sum(a: int, b: int, c: int): int { return a + b + c; }\n"
            "return sum(10, ...[20, 12]);\n",
            "call_spread_prefix.zr",
            42);
}

static void test_call_spread_supports_empty_array(void) {
    assert_source_executes_to_int64(
            "func answer(): int { return 42; }\n"
            "return answer(...[]);\n",
            "call_spread_empty.zr",
            42);
}

static void test_call_spread_evaluates_elements_once(void) {
    assert_source_executes_to_int64(
            "var count = 0;\n"
            "func bump(value: int): int {\n"
            "    count = count + 1;\n"
            "    return value;\n"
            "}\n"
            "func sum(a: int, b: int, c: int): int { return a + b + c; }\n"
            "var result = sum(...[bump(10), bump(20), bump(12)]);\n"
            "return result + count * 100;\n",
            "call_spread_once.zr",
            342);
}

static SZrFunction *compile_aot_call_spread_fixture(void) {
    const char *source =
            "func sum(a: int, b: int, c: int): int { return a + b + c; }\n"
            "var values = [10, 20, 12];\n"
            "return sum(...values);\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "call_spread_aot.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_contains_opcode(
            function, ZR_INSTRUCTION_ENUM(FUNCTION_CALL_SPREAD)));
    return function;
}

static void test_call_spread_lowers_to_aot_c_runtime_boundary(void) {
    SZrFunction *function = compile_aot_call_spread_fixture();
    SZrAotWriterOptions options;
    TZrChar generatedPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedText;

    memset(&options, 0, sizeof(options));
    options.moduleName = "call_spread_aot_c";
    options.sourceHash = "call-spread-aot-c";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "call-spread-aot-c";
    options.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "call_spread",
            "aot_c",
            "call_spread",
            ".c",
            generatedPath,
            sizeof(generatedPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(
            g_state, function, generatedPath, &options));

    generatedText = ZrTests_ReadTextFile(generatedPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedText);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, generatedLength);
    TEST_ASSERT_NOT_NULL(strstr(
            generatedText, "/* zr_aot_spread_function_call */"));
    TEST_ASSERT_NOT_NULL(strstr(
            generatedText, "ZrLibrary_AotRuntime_CallSpread(state, &frame,"));

    free(generatedText);
    ZrCore_Function_Free(g_state, function);
}

static void test_call_spread_lowers_to_aot_llvm_runtime_boundary(void) {
    SZrFunction *function = compile_aot_call_spread_fixture();
    SZrAotWriterOptions options;
    TZrChar generatedPath[ZR_TESTS_PATH_MAX];
    TZrSize generatedLength = 0u;
    char *generatedText;

    memset(&options, 0, sizeof(options));
    options.moduleName = "call_spread_aot_llvm";
    options.sourceHash = "call-spread-aot-llvm";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "call-spread-aot-llvm";
    options.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "call_spread",
            "aot_llvm",
            "call_spread",
            ".ll",
            generatedPath,
            sizeof(generatedPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFileWithOptions(
            g_state, function, generatedPath, &options));

    generatedText = ZrTests_ReadTextFile(generatedPath, &generatedLength);
    TEST_ASSERT_NOT_NULL(generatedText);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, generatedLength);
    TEST_ASSERT_NOT_NULL(strstr(
            generatedText,
            "declare i1 @ZrLibrary_AotRuntime_CallSpread(ptr, ptr, i32, i32, i32, ptr)"));
    TEST_ASSERT_NOT_NULL(strstr(
            generatedText, "call i1 @ZrLibrary_AotRuntime_CallSpread("));
    TEST_ASSERT_NOT_NULL(strstr(generatedText, "spread_ok"));

    free(generatedText);
    ZrCore_Function_Free(g_state, function);
}

static void assert_source_rejects(const char *source, const char *sourceNameText) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, (TZrNativeString)sourceNameText);
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);

    TEST_ASSERT_NULL(function);
}

static void test_call_spread_rejects_non_array_operand(void) {
    assert_source_rejects(
            "func identity(value: int): int { return value; }\n"
            "return identity(...42);\n",
            "call_spread_non_array.zr");
}

static void test_call_spread_rejects_non_trailing_operand(void) {
    assert_source_rejects(
            "func sum(a: int, b: int): int { return a + b; }\n"
            "return sum(...[1], 2);\n",
            "call_spread_non_trailing.zr");
}

static void test_call_spread_rejects_multiple_spreads(void) {
    assert_source_rejects(
            "func sum(a: int, b: int): int { return a + b; }\n"
            "return sum(...[1], ...[2]);\n",
            "call_spread_multiple.zr");
}

static void test_call_spread_rejects_named_arguments(void) {
    assert_source_rejects(
            "func sum(a: int, b: int): int { return a + b; }\n"
            "return sum(a: 1, ...[2]);\n",
            "call_spread_named.zr");
}

static void test_call_spread_rejects_element_conversion(void) {
    assert_source_rejects(
            "func identity(value: float): float { return value; }\n"
            "return identity(...[42]);\n",
            "call_spread_element_conversion.zr");
}

static void test_call_spread_rejects_known_argument_count_mismatch(void) {
    assert_source_rejects(
            "func sum(a: int, b: int, c: int): int { return a + b + c; }\n"
            "return sum(...[1, 2]);\n",
            "call_spread_known_arity.zr");
}

static void test_call_spread_rejects_dynamic_argument_count_mismatch_at_runtime(void) {
    const char *source =
            "func sum(a: int, b: int, c: int): int { return a + b + c; }\n"
            "func invoke(values: int[]): int { return sum(...values); }\n"
            "return invoke([1, 2]);\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "call_spread_dynamic_arity.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    ZrCore_Function_Free(g_state, function);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_call_spread_has_dedicated_argument_ast);
    RUN_TEST(test_return_call_spread_preserves_argument_ast);
    RUN_TEST(test_call_spread_executes_dynamic_array_arguments);
    RUN_TEST(test_call_spread_struct_values_survive_gc_during_expansion);
    RUN_TEST(test_call_spread_supports_fixed_prefix);
    RUN_TEST(test_call_spread_supports_empty_array);
    RUN_TEST(test_call_spread_evaluates_elements_once);
    RUN_TEST(test_call_spread_lowers_to_aot_c_runtime_boundary);
    RUN_TEST(test_call_spread_lowers_to_aot_llvm_runtime_boundary);
    RUN_TEST(test_call_spread_rejects_non_array_operand);
    RUN_TEST(test_call_spread_rejects_non_trailing_operand);
    RUN_TEST(test_call_spread_rejects_multiple_spreads);
    RUN_TEST(test_call_spread_rejects_named_arguments);
    RUN_TEST(test_call_spread_rejects_element_conversion);
    RUN_TEST(test_call_spread_rejects_known_argument_count_mismatch);
    RUN_TEST(test_call_spread_rejects_dynamic_argument_count_mismatch_at_runtime);
    return UNITY_END();
}
