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

// Unity 每个用例经 setUp/tearDown 独立创建并销毁 VM；辅助断言借此共享同一状态。
static SZrState *g_state;

// 检查动态调用展开是否真的进入 VM 指令，而非只在 AST 层被接受。
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

// 与字节码断言配对，约束前端 SemIR 到执行路径的交接。
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

// Unity 在每个 RUN_TEST 前调用；所有解析、编译与执行资源都属于本次 VM 状态。
void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

// Unity 失败中止后仍运行 tearDown，避免单个用例留下跨用例状态。
void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

// 锁定展开实参为独立 AST，防止普通数组表达式吞掉省略号语义。
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
    // BUG: 此后任一断言失败会 longjmp 跳过末尾 Ast_Free，泄漏非 GC 分配的 AST 树。
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

// 返回语句中的嵌套调用也必须保留展开实参节点供后续编译。
static void test_return_call_spread_preserves_argument_ast(void) {
    const char *source =
            "fn sum(a: int, b: int, c: int): int { return a + b + c; }\n"
            "return sum(...[1, 2, 3]);\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "call_spread_return_ast.zr");
    SZrAstNode *script = ZrParser_Parse(
            g_state, source, strlen(source), sourceName);
    SZrAstNode *statement;
    SZrAstNode *expression;
    SZrAstNode *call;

    TEST_ASSERT_NOT_NULL(script);
    // BUG: 此后任一断言失败会 longjmp 跳过末尾 Ast_Free，泄漏非 GC 分配的 AST 树。
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

// 同时检查 SemIR、VM 指令和执行结果，贯通动态数组展开的编译/运行边界。
static void test_call_spread_executes_dynamic_array_arguments(void) {
    const char *source =
            "fn sum(a: int, b: int, c: int): int {\n"
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

// 展开引用对象数组时压低堆上限并请求 full GC，检查调用结果仍可读取实参。
// TODO: SetHeapLimitBytes/ScheduleCollection 仅安排收集；需观测展开窗口的收集计数以证明根保护。
static void test_call_spread_struct_values_survive_gc_during_expansion(void) {
    const char *source =
            "class Pair {\n"
            "    pub var value: int;\n"
            "    pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "fn sum(a: Pair, b: Pair, c: Pair): int {\n"
            "    return a.value + b.value + c.value;\n"
            "}\n"
            "var values = [new Pair(10), new Pair(20), new Pair(12)];\n"
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

// 成功场景共用的编译与执行断言；调用者提供独立源文本和期望值。
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

// 固定前缀实参与尾随展开共存时仍应按源顺序传参。
static void test_call_spread_supports_fixed_prefix(void) {
    assert_source_executes_to_int64(
            "fn sum(a: int, b: int, c: int): int { return a + b + c; }\n"
            "return sum(10, ...[20, 12]);\n",
            "call_spread_prefix.zr",
            42);
}

// 空数组展开不能额外产生一个实参。
static void test_call_spread_supports_empty_array(void) {
    assert_source_executes_to_int64(
            "fn answer(): int { return 42; }\n"
            "return answer(...[]);\n",
            "call_spread_empty.zr",
            42);
}

// 用有副作用的元素表达式约束展开阶段不能重复求值。
static void test_call_spread_evaluates_elements_once(void) {
    assert_source_executes_to_int64(
            "var count = 0;\n"
            "fn bump(value: int): int {\n"
            "    count = count + 1;\n"
            "    return value;\n"
            "}\n"
            "fn sum(a: int, b: int, c: int): int { return a + b + c; }\n"
            "var result = sum(...[bump(10), bump(20), bump(12)]);\n"
            "return result + count * 100;\n",
            "call_spread_once.zr",
            342);
}

// AOT C/LLVM 共用动态展开 fixture；两个用例各自编译并释放函数。
static SZrFunction *compile_aot_call_spread_fixture(void) {
    const char *source =
            "fn sum(a: int, b: int, c: int): int { return a + b + c; }\n"
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

// 要求 AOT C 生成器通过共享运行时入口调用动态展开，而非丢失该操作。
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
    // BUG: 读取成功后若后续断言失败，Unity 中止会跳过 free(generatedText)。
    TEST_ASSERT_NOT_NULL(generatedText);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, generatedLength);
    TEST_ASSERT_NOT_NULL(strstr(
            generatedText, "/* zr_aot_spread_function_call */"));
    TEST_ASSERT_NOT_NULL(strstr(
            generatedText, "ZrLibrary_AotRuntime_CallSpread(state, &frame,"));

    free(generatedText);
    ZrCore_Function_Free(g_state, function);
}

// LLVM 路径须保留运行时声明、调用及失败边，以匹配 C 路径语义。
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
    // BUG: 后续产物断言失败会跳过本地 malloc 缓冲区的 free。
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

// 负向用例只在编译阶段拒绝；运行时参数个数不足另由执行用例覆盖。
static void assert_source_rejects(const char *source, const char *sourceNameText) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, (TZrNativeString)sourceNameText);
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);

    TEST_ASSERT_NULL(function);
}

// 展开操作数的静态类型必须是数组。
static void test_call_spread_rejects_non_array_operand(void) {
    assert_source_rejects(
            "fn identity(value: int): int { return value; }\n"
            "return identity(...42);\n",
            "call_spread_non_array.zr");
}

// 展开实参只能位于参数列表末端。
static void test_call_spread_rejects_non_trailing_operand(void) {
    assert_source_rejects(
            "fn sum(a: int, b: int): int { return a + b; }\n"
            "return sum(...[1], 2);\n",
            "call_spread_non_trailing.zr");
}

// 同一调用不允许多个展开位置。
static void test_call_spread_rejects_multiple_spreads(void) {
    assert_source_rejects(
            "fn sum(a: int, b: int): int { return a + b; }\n"
            "return sum(...[1], ...[2]);\n",
            "call_spread_multiple.zr");
}

// 命名实参与展开实参的混用目前被编译契约拒绝。
static void test_call_spread_rejects_named_arguments(void) {
    assert_source_rejects(
            "fn sum(a: int, b: int): int { return a + b; }\n"
            "return sum(a: 1, ...[2]);\n",
            "call_spread_named.zr");
}

// 元素类型不能依赖展开时的隐式转换去适配形参。
static void test_call_spread_rejects_element_conversion(void) {
    assert_source_rejects(
            "fn identity(value: float): float { return value; }\n"
            "return identity(...[42]);\n",
            "call_spread_element_conversion.zr");
}

// 编译期可知数组长度时，实参数量错误应提前拒绝。
static void test_call_spread_rejects_known_argument_count_mismatch(void) {
    assert_source_rejects(
            "fn sum(a: int, b: int, c: int): int { return a + b + c; }\n"
            "return sum(...[1, 2]);\n",
            "call_spread_known_arity.zr");
}

// TODO: 动态数组实参数量预期由运行时拒绝；当前只断言执行失败，需核验故障类别。
static void test_call_spread_rejects_dynamic_argument_count_mismatch_at_runtime(void) {
    const char *source =
            "fn sum(a: int, b: int, c: int): int { return a + b + c; }\n"
            "fn invoke(values: int[]): int { return sum(...values); }\n"
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

// 独立 Unity 可执行入口；按 AST、执行、AOT 和拒绝路径注册全部场景。
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
