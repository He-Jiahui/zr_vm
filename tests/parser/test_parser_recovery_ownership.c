#include "unity.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/parser.h"

#include <stdlib.h>
#include <string.h>

// 每个 Unity 用例独占 VM、parser 与部分 AST，供失败后的 tearDown 统一回收。
static SZrGlobalState *g_global;
static SZrParserState g_parser;
static SZrAstNode *g_ast;
// 只观察上游 allocator 的原生块；GC 对象须等 GlobalState_Free 后才能结算。
static size_t g_liveBlocks;
static size_t g_errorCount;

// GlobalState_New 的上游分配回调，记录解析恢复路径留下的所有原生分配。
// realloc 保持一个活动块，失败时原指针仍归调用方所有。
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

// 结构化 parser 回调只计 error；调用方在返回后继续拥有诊断对象。
static void capture_error(TZrPtr userData, const SZrStructuredDiagnostic *diagnostic, EZrToken token) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(token);
    if (diagnostic != ZR_NULL && diagnostic->severity == ZR_STRUCTURED_DIAGNOSTIC_ERROR) {
        g_errorCount++;
    }
}

// 兼容旧文本诊断通道；用例只要求错误可见，不依赖两种回调的计数比例。
static void capture_legacy_error(TZrPtr userData, const SZrFileRange *location,
                                 const TZrChar *message, EZrToken token) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(location);
    ZR_UNUSED_PARAMETER(message);
    ZR_UNUSED_PARAMETER(token);
    g_errorCount++;
}

// 每个 fixture 从空计数开始，以自定义 allocator 建立独立 VM。
void setUp(void) {
    SZrCallbackGlobal callbacks = {0};
    g_liveBlocks = 0;
    g_errorCount = 0;
    g_ast = ZR_NULL;
    memset(&g_parser, 0, sizeof(g_parser));
    g_global = ZrCore_GlobalState_New(tracking_allocator, ZR_NULL, 0, &callbacks);
    TEST_ASSERT_NOT_NULL(g_global);
}

// Unity 在测试体断言失败后仍调用此入口；AST 必须先于 parser 和 VM 释放。
void tearDown(void) {
    if (g_global != ZR_NULL) {
        ZrParser_Ast_Free(g_global->mainThreadState, g_ast);
        ZrParser_State_Free(&g_parser);
        ZrCore_GlobalState_Free(g_global);
        g_global = ZR_NULL;
    }
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0, g_liveBlocks, "parser recovery must release every allocation");
}

// 共用恢复用例的解析入口；保留返回的部分 AST，交由 tearDown 验证清理。
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

// 正常脚本提供嵌套 AST 的释放基线，便于和错误恢复路径比较。
static void test_complete_constructs_release_children(void) {
    parse_source("fn pick(value: int): int { return value; } return [1, {a: 2}, (3 + 4)];", ZR_FALSE);
}

// 数组结束符缺失后，先前接收的元素仍须在测试结束时全部释放。
static void test_array_missing_close_releases_elements(void) {
    parse_source("return [1, 2", ZR_TRUE);
}

// 缺少元素分隔符时检查恢复后的数组元素所有权。
static void test_array_missing_separator_releases_elements(void) {
    parse_source("return [1 2];", ZR_TRUE);
}

// 后续元素解析失败不得遗留已构建的前缀元素。
static void test_array_invalid_later_element_releases_previous_elements(void) {
    parse_source("return [1, +];", ZR_TRUE);
}

// 对象结束符缺失时检查已构建属性的释放链。
static void test_object_missing_close_releases_properties(void) {
    parse_source("return {a: 1", ZR_TRUE);
}

// 属性分隔符错误不得让先前的键和值脱离 AST 清理链。
static void test_object_missing_separator_releases_properties(void) {
    parse_source("return {a: 1 \"b\": 2};", ZR_TRUE);
}

// 计算键缺少闭合方括号时，键表达式由 parser 错误路径接管清理。
static void test_object_computed_key_missing_close_releases_key(void) {
    parse_source("return {[1: 2};", ZR_TRUE);
}

// 后续计算键失败还需保留先前属性的清理责任。
static void test_object_later_computed_key_missing_close_releases_children(void) {
    parse_source("return {a: 1, [2: 3};", ZR_TRUE);
}

// 首个属性没有值时，已解析的键不能遗留在临时所有权中。
static void test_object_missing_first_value_releases_key(void) {
    parse_source("return {a: };", ZR_TRUE);
}

// 后续属性没有值时，前缀属性和当前键都须可释放。
static void test_object_missing_later_value_releases_key(void) {
    parse_source("return {a: 1, b: };", ZR_TRUE);
}

// 分组表达式缺少右括号时仍需释放其内部表达式。
static void test_group_missing_close_releases_expression(void) {
    parse_source("return (1 + 2;", ZR_TRUE);
}

// 形参列表错误不能遗留已构建的形参及类型节点。
static void test_function_missing_parameter_close_releases_parameters(void) {
    parse_source("fn pick(value: int: int { return value; }", ZR_TRUE);
}

// 泛型签名解析完成而函数体缺失时检查声明子树释放。
static void test_function_missing_body_releases_signature(void) {
    parse_source("fn pick<T>(value: T): T;", ZR_TRUE);
}

// BUG: 可变形参 token 是 `...`，此输入写成普通标识符 `params`；
// 测试只触发普通形参缺 `)`，无法检出可变形参清理回归。
static void test_function_missing_variadic_close_releases_parameter(void) {
    parse_source("fn pick(params values: int: int { return 1; }", ZR_TRUE);
}

// BUG: 当前 fixture 没有装饰器；装饰器清理回归仍会通过此用例，需补含装饰器的非法形参。
static void test_parameter_missing_name_releases_decorators(void) {
    parse_source("fn pick(,): int { return 1; }", ZR_TRUE);
}

// CTest 的独立 Unity 入口注册所有恢复场景；每项都经 tearDown 做零块断言。
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
