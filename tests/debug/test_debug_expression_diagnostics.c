#include <string.h>

#include "unity.h"
#include "runtime_support.h"
#include "debug_internal.h"
#include "debug_breakpoint_condition.h"
#include "debug_breakpoint_logpoint.h"
#include "debug_evaluation_effect_internal.h"
#include "debug_protocol_evaluate.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/type_inference.h"

/* Unity 的单一编译单元直接收录各场景头文件；RUN_TEST 将调试表达式的诊断、
 * 规范语义事实、纯策略判断和协议投递作为独立场景注册。 */
/* 保留完整实际诊断作为失败消息，便于定位文本协议回归。 */
static void assert_text_contains(const TZrChar *text, const TZrChar *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(text, needle), text);
}

/* 对摘要中的负向事实作断言，避免折叠后的子表达式重复出现在客户端展示中。 */
static void assert_text_not_contains(const TZrChar *text, const TZrChar *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NULL_MESSAGE(strstr(text, needle), text);
}

/* 为语义绑定测试提供带来源标签的入口函数；调用方拥有返回函数并须先于 state 释放。 */
static SZrFunction *compile_debug_source(SZrState *state, const char *sourceLabel, const char *source) {
    SZrString *sourceName;

    if (state == ZR_NULL || sourceLabel == ZR_NULL || source == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceLabel, strlen(sourceLabel));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

#include "test_debug_canonical_binding_cases.h"
#include "test_debug_formal_evaluation_cases.h"

/* 未完成的二元表达式应返回含原因与修复建议的可操作诊断，供 evaluate 客户端展示。 */
static void test_debug_evaluate_reports_missing_right_operand_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "1 +", &result, error, sizeof(error)));
    assert_text_contains(error, "Missing expression after '+'");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "Suggestion:");
    assert_text_contains(error, "right-hand expression");

    ZrTests_Runtime_State_Destroy(state);
}

/* 条件表达式缺少逻辑操作数时应指出断点场景，而不是只报告通用解析失败。 */
static void test_debug_condition_expression_reports_missing_logical_operand_with_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "true &&", &result, error, sizeof(error)));
    assert_text_contains(error, "Missing expression after '&&'");
    assert_text_contains(error, "conditional breakpoint");
    assert_text_contains(error, "Suggestion:");

    ZrTests_Runtime_State_Destroy(state);
}

/* 短路为真的或分支仍须有规范语义事实，不能绕过未知名字的校验。 */
static void test_debug_condition_rejects_unresolved_or_operand(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "true || missingLocal", &result, error, sizeof(error)));
    assert_text_contains(error, "canonical semantic facts");

    ZrTests_Runtime_State_Destroy(state);
}

/* 短路为假的与分支仍须先确认所有引用的身份，不能接受未知名字。 */
static void test_debug_condition_rejects_unresolved_and_operand(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "false && missingLocal", &result, error, sizeof(error)));
    assert_text_contains(error, "canonical semantic facts");

    ZrTests_Runtime_State_Destroy(state);
}

/* 比较结果可参与布尔组合和取反，正式求值应向客户端返回稳定的 bool 文本。 */
static void test_debug_evaluate_composed_comparison_logical_expression_returns_bool(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "(1 < 2) && (3 < 4)", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("bool", result.type_name);
    TEST_ASSERT_EQUAL_STRING("true", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    memset(&result, 0, sizeof(result));
    error[0] = '\0';

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "!(1 < 2)", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("bool", result.type_name);
    TEST_ASSERT_EQUAL_STRING("false", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);

    ZrTests_Runtime_State_Destroy(state);
}

/* 语义摘要必须转义字符串字面量，避免调试展示破坏引号、反斜线和控制字符。 */
static void test_debug_evaluate_semantic_summary_escapes_string_constants(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "\"a\\\"b\\\\c\\n\\t\"", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("string", result.type_name);
    TEST_ASSERT_EQUAL_STRING("", error);
    assert_text_contains(result.semantic_summary, "expression literal exact");
    assert_text_contains(result.semantic_summary, "constant \"a\\\"b\\\\c\\n\\t\"");

    ZrTests_Runtime_State_Destroy(state);
}

/* 常量折叠后的摘要同时保留有符号和无符号范围，供调试客户端解释数值事实。 */
static void test_debug_evaluate_semantic_summary_reports_unsigned_numeric_range(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "1 + 2", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("int", result.type_name);
    TEST_ASSERT_EQUAL_STRING("3", result.value_text);
    TEST_ASSERT_EQUAL_STRING("", error);
    assert_text_contains(result.semantic_summary, "type int");
    assert_text_contains(result.semantic_summary, "range 3..3");
    assert_text_contains(result.semantic_summary, "unsigned range 3..3");

    ZrTests_Runtime_State_Destroy(state);
}

/* 仅在 VM 全局槽写入 zr 值不足以建立可验证身份；无暂停帧令牌时正式求值须拒绝。 */
static void test_debug_evaluate_rejects_runtime_global_without_canonical_identity(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *arrayText;
    SZrObject *arrayObject = ZR_NULL;
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    arrayText = ZrCore_String_CreateFromNative(state, "abcd");
    TEST_ASSERT_NOT_NULL(arrayText);
    TEST_ASSERT_TRUE(ZrCore_String_ToByteArray(state, arrayText, &arrayObject));
    TEST_ASSERT_NOT_NULL(arrayObject);
    ZrCore_Value_InitAsRawObject(state, &state->global->zrObject, ZR_CAST_RAW_OBJECT_AS_SUPER(arrayObject));
    state->global->zrObject.type = ZR_VALUE_TYPE_ARRAY;

    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_EvaluateWithCapabilities(
            &agent,
            1u,
            "zr[1]",
            ZR_DEBUG_EVALUATION_EFFECT_NONE,
            &result,
            error,
            sizeof(error)));
    assert_text_contains(error, "canonical semantic facts");

    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "zr[1]", &result, error, sizeof(error)));
    assert_text_contains(error, "canonical semantic facts");

    ZrTests_Runtime_State_Destroy(state);
}

/* 入口函数的已编译 callable 元数据应回放成调用引用事实，并保留实参折叠摘要。 */
static void test_debug_semantic_summary_replays_compiled_function_call_reference_fact(void) {
    const char *source =
            "fn pick(value: int): int {\n"
            "    return value;\n"
            "}\n"
            "return 0;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgent agent;
    TZrChar summary[ZR_DEBUG_TEXT_CAPACITY * 2u];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_function_call_semantic_summary.zr", source);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function->topLevelCallableBindingLength > 0);
    TEST_ASSERT_TRUE(function->topLevelCallableBindings[0].callableChildIndex < function->childFunctionLength);
    TEST_ASSERT_TRUE(function->childFunctionList[function->topLevelCallableBindings[0].callableChildIndex]
                             .hasCallableReturnType);
    TEST_ASSERT_TRUE(function->childFunctionList[function->topLevelCallableBindings[0].callableChildIndex]
                             .parameterMetadataCount > 0);

    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    summary[0] = '\0';

    zr_debug_append_expression_semantic_facts(&agent, 1, "pick(1 + 2)", summary, sizeof(summary));

    assert_text_contains(summary, "call pick args=1");
    assert_text_contains(summary, "reference call pick");
    assert_text_contains(summary, "expression binary exact");
    assert_text_contains(summary, "constant 3");
    assert_text_contains(summary, "range 3..3");

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

/* 入口函数的已编译本地绑定应支持独立表达式中全局名的只读引用摘要。 */
static void test_debug_semantic_summary_replays_compiled_top_level_variable_reference_fact(void) {
    const char *source =
            "var globalSeed: int = 2;\n"
            "return 0;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgent agent;
    TZrChar summary[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_global_variable_semantic_summary.zr", source);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function->typedLocalBindingLength > 0);

    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    summary[0] = '\0';

    zr_debug_append_expression_semantic_facts(&agent, 1, "globalSeed + 1", summary, sizeof(summary));

    assert_text_contains(summary, "reference read globalSeed");
    assert_text_contains(summary, "expression binary");

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

/* 借用表达式的摘要需要沿已编译绑定继承所有权和只读借用事实。 */
static void test_debug_semantic_summary_replays_compiled_ownership_fact(void) {
    const char *source =
            "var owner: Shared<int>;\n"
            "return 0;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgent agent;
    TZrChar summary[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_ownership_semantic_summary.zr", source);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function->typedLocalBindingLength > 0);

    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    summary[0] = '\0';

    zr_debug_append_expression_semantic_facts(&agent, 1, "ref owner", summary, sizeof(summary));

    assert_text_contains(summary, "reference read owner");
    assert_text_contains(summary, "expression ownership exact");
    assert_text_contains(summary, "ownership borrow ref readonly");

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

/* typeof 的操作数仍应作为变量读取进入语义事实遍历。 */
static void test_debug_semantic_summary_walks_type_query_operand_reference(void) {
    const char *source =
            "var owner: Unique<int>;\n"
            "return 0;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgent agent;
    TZrChar summary[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_type_query_operand_reference.zr", source);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function->typedLocalBindingLength > 0);

    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    summary[0] = '\0';

    zr_debug_append_expression_semantic_facts(&agent, 1, "typeof(owner)", summary, sizeof(summary));

    assert_text_contains(summary, "reference read owner");

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

/* 索引成员访问的摘要应同时保留访问形状和索引操作数的读取来源。 */
static void test_debug_semantic_summary_replays_member_expression_payload_fact(void) {
    const char *source =
            "var seed: int = 2;\n"
            "var index: int = 1;\n"
            "return 0;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgent agent;
    TZrChar summary[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_member_payload_semantic_summary.zr", source);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function->typedLocalBindingLength >= 2);

    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    summary[0] = '\0';

    zr_debug_append_expression_semantic_facts(&agent, 1, "seed[index]", summary, sizeof(summary));

    assert_text_contains(summary, "member index");
    assert_text_contains(summary, "reference member access index");
    assert_text_contains(summary, "reference read index");

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

/* 摘要即使不执行赋值，也须区分名字、字段和索引位置的写引用事实。 */
static void test_debug_semantic_summary_replays_assignment_write_reference_facts(void) {
    const char *source =
            "var globalSeed: int = 2;\n"
            "var seed: int = 0;\n"
            "var index: int = 1;\n"
            "return 0;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgent agent;
    TZrChar summary[ZR_DEBUG_TEXT_CAPACITY * 2u];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, "debug_assignment_semantic_summary.zr", source);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function->typedLocalBindingLength >= 3);

    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.entryFunction = function;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    summary[0] = '\0';
    zr_debug_append_expression_semantic_facts(&agent, 1, "globalSeed = 3", summary, sizeof(summary));
    assert_text_contains(summary, "expression assignment exact");
    assert_text_contains(summary, "reference write globalSeed");

    summary[0] = '\0';
    zr_debug_append_expression_semantic_facts(&agent, 1, "seed.value = 3", summary, sizeof(summary));
    assert_text_contains(summary, "expression assignment exact");
    assert_text_contains(summary, "member value");
    assert_text_contains(summary, "reference member write value");

    summary[0] = '\0';
    zr_debug_append_expression_semantic_facts(&agent, 1, "seed[index] = 4", summary, sizeof(summary));
    assert_text_contains(summary, "expression assignment exact");
    assert_text_contains(summary, "member index");
    assert_text_contains(summary, "reference member write index");
    assert_text_contains(summary, "reference read index");

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

/* 成员接收者内的常量折叠应保留最终事实，并避免展示已折叠的原始常量。 */
static void test_debug_semantic_summary_walks_member_receiver_facts(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    TZrChar summary[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    summary[0] = '\0';

    zr_debug_append_expression_semantic_facts(&agent, 1, "{a: 1 + 2}.a", summary, sizeof(summary));

    assert_text_contains(summary, "expression member exact");
    assert_text_contains(summary, "member a");
    assert_text_contains(summary, "reference member access a");
    assert_text_contains(summary, "expression object exact");
    assert_text_contains(summary, "expression binary exact");
    assert_text_contains(summary, "constant 3");
    assert_text_contains(summary, "range 3..3");
    assert_text_not_contains(summary, "constant 1");
    assert_text_not_contains(summary, "constant 2");

    ZrTests_Runtime_State_Destroy(state);
}

/* 常量条件的摘要应表明选中分支与未执行分支，防止错误的可达性展示。 */
static void test_debug_semantic_summary_replays_conditional_branch_facts(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    TZrChar summary[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    summary[0] = '\0';

    zr_debug_append_expression_semantic_facts(&agent, 1, "true ? 1 : 2", summary, sizeof(summary));

    assert_text_contains(summary, "expression conditional exact");
    assert_text_contains(summary, "range 1..1");
    assert_text_contains(summary, "logical true");
    assert_text_contains(summary, "unreachable because a constant branch skips evaluation");

    ZrTests_Runtime_State_Destroy(state);
}

/* lambda 内局部初始化的折叠和后续读取应纳入同一语义摘要。 */
static void test_debug_semantic_summary_walks_lambda_local_initializer_facts(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    TZrChar summary[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    summary[0] = '\0';

    zr_debug_append_expression_semantic_facts(&agent,
                                              1,
                                              "fn()=>{ var folded = 1 + 2; return folded; }",
                                              summary,
                                              sizeof(summary));

    assert_text_contains(summary, "expression lambda exact");
    assert_text_contains(summary, "expression binary exact");
    assert_text_contains(summary, "constant 3");
    assert_text_contains(summary, "range 3..3");
    assert_text_contains(summary, "reference read folded");
    assert_text_not_contains(summary, "constant 1");
    assert_text_not_contains(summary, "constant 2");

    ZrTests_Runtime_State_Destroy(state);
}

/* 比较式构成的短路条件也不能跳过非活动分支中的未知名字。 */
static void test_debug_condition_rejects_unresolved_comparison_operands(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "(1 < 2) || missingLocal", &result, error, sizeof(error)));
    assert_text_contains(error, "canonical semantic facts");

    memset(&result, 0, sizeof(result));
    error[0] = '\0';

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "(2 < 1) && missingLocal", &result, error, sizeof(error)));
    assert_text_contains(error, "canonical semantic facts");

    ZrTests_Runtime_State_Destroy(state);
}

/* 条件运算符两条分支都须预先通过规范绑定检查，不能按当前真值放行未知名。 */
static void test_debug_condition_rejects_unresolved_ternary_branch(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "true ? 1 : missingLocal", &result, error, sizeof(error)));
    assert_text_contains(error, "canonical semantic facts");

    memset(&result, 0, sizeof(result));
    error[0] = '\0';

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "false ? missingLocal : 2", &result, error, sizeof(error)));
    assert_text_contains(error, "canonical semantic facts");

    ZrTests_Runtime_State_Destroy(state);
}

/* 已有运行时全局值不能掩盖三元表达式其他分支缺少规范身份的问题。 */
static void test_debug_condition_rejects_unresolved_branch_with_runtime_globals(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *arrayText;
    SZrObject *arrayObject = ZR_NULL;
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    arrayText = ZrCore_String_CreateFromNative(state, "abcd");
    TEST_ASSERT_NOT_NULL(arrayText);
    TEST_ASSERT_TRUE(ZrCore_String_ToByteArray(state, arrayText, &arrayObject));
    TEST_ASSERT_NOT_NULL(arrayObject);
    ZrCore_Value_InitAsRawObject(state, &state->global->zrObject, ZR_CAST_RAW_OBJECT_AS_SUPER(arrayObject));
    state->global->zrObject.type = ZR_VALUE_TYPE_ARRAY;

    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "zr ? loadedModules : missingLocal", &result, error, sizeof(error)));
    assert_text_contains(error, "canonical semantic facts");

    ZrTests_Runtime_State_Destroy(state);
}

/* 缺少三元真分支时，诊断应保留断点上下文及补全建议。 */
static void test_debug_condition_reports_missing_ternary_consequent_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "true ? : 2", &result, error, sizeof(error)));
    assert_text_contains(error, "Missing consequent expression in conditional expression");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "conditional breakpoint");
    assert_text_contains(error, "Suggestion:");

    ZrTests_Runtime_State_Destroy(state);
}

/* 缺少三元假分支时，诊断应指向缺失位置并提供断点场景建议。 */
static void test_debug_condition_reports_missing_ternary_alternate_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "true ? 1 :", &result, error, sizeof(error)));
    assert_text_contains(error, "Missing alternate expression in conditional expression");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "conditional breakpoint");
    assert_text_contains(error, "Suggestion:");

    ZrTests_Runtime_State_Destroy(state);
}

/* 非数值加法拒绝时应说明类型前提及可执行的替代写法。 */
static void test_debug_evaluate_reports_numeric_operand_type_error_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "\"text\" + 1", &result, error, sizeof(error)));
    assert_text_contains(error, "numeric operands");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "Suggestion:");
    assert_text_contains(error, "Use numeric operands");

    ZrTests_Runtime_State_Destroy(state);
}

/* 常量除零应在调试求值中失败，并向调用方提供除数保护建议。 */
static void test_debug_evaluate_reports_division_by_zero_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "1 / 0", &result, error, sizeof(error)));
    assert_text_contains(error, "division by zero");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "Suggestion:");
    assert_text_contains(error, "guard the divisor");

    ZrTests_Runtime_State_Destroy(state);
}

/* 非法小数字面量应保留明确的词法原因和更正建议。 */
static void test_debug_evaluate_reports_invalid_numeric_literal_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "1..2", &result, error, sizeof(error)));
    assert_text_contains(error, "Invalid numeric literal");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "debug evaluate");
    assert_text_contains(error, "Suggestion:");
    assert_text_contains(error, "single decimal point");

    ZrTests_Runtime_State_Destroy(state);
}

/* 成员访问符后缺少名字时应返回专门诊断，避免吞掉客户端输入错误。 */
static void test_debug_evaluate_reports_missing_member_name_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "true.", &result, error, sizeof(error)));
    assert_text_contains(error, "Missing member name after '.'");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "Suggestion:");
    assert_text_contains(error, "member access");

    ZrTests_Runtime_State_Destroy(state);
}

/* 断点条件中的索引表达式缺失右括号，应定位结构错误并给出补全方向。 */
static void test_debug_condition_reports_missing_index_close_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "false || true[0", &result, error, sizeof(error)));
    assert_text_contains(error, "Missing closing ']' in index access");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "conditional breakpoint");
    assert_text_contains(error, "Suggestion:");

    ZrTests_Runtime_State_Destroy(state);
}

/* 断点条件中的分组未闭合，应在正式求值前给出可理解的诊断。 */
static void test_debug_condition_reports_missing_group_close_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "(true || false", &result, error, sizeof(error)));
    assert_text_contains(error, "Missing closing ')' in grouped expression");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "conditional breakpoint");
    assert_text_contains(error, "Suggestion:");

    ZrTests_Runtime_State_Destroy(state);
}

/* 未闭合字符串应向 evaluate 客户端说明闭合引号需求。 */
static void test_debug_evaluate_reports_unterminated_string_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "\"open", &result, error, sizeof(error)));
    assert_text_contains(error, "Unterminated string literal");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "debug evaluate");
    assert_text_contains(error, "Suggestion:");
    assert_text_contains(error, "closing quote");

    ZrTests_Runtime_State_Destroy(state);
}

/* 不支持的转义应保留原始序列并建议使用受支持的转义。 */
static void test_debug_evaluate_reports_unsupported_string_escape_with_cause_and_suggestion(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;

    TEST_ASSERT_FALSE(ZrDebug_Evaluate(&agent, 1, "\"bad\\q\"", &result, error, sizeof(error)));
    assert_text_contains(error, "Unsupported string escape");
    assert_text_contains(error, "Cause:");
    assert_text_contains(error, "\\q");
    assert_text_contains(error, "Suggestion:");
    assert_text_contains(error, "supported escape");

    ZrTests_Runtime_State_Destroy(state);
}

#include "test_debug_evaluation_effect_policy_cases.h"
#include "test_debug_breakpoint_condition_cases.h"
#include "test_debug_breakpoint_logpoint_cases.h"
#include "test_debug_evaluate_result_transport_cases.h"
#include "test_debug_evaluate_failure_transport_cases.h"

/* BUG: 创建 state 后若 Unity 断言失败，UnityDefaultTestRun 的失败跳转会跳过
 * 用例尾部的 hook 撤销和 state 销毁；空 tearDown 使该 VM 泄漏。
 * 证据：tests/third_party/zr_unity/Unity/src/unity.c 的 UnityDefaultTestRun；
 * 后续在失败注入用例中核对清理，再迁移到 fixture。 */
void setUp(void) {}

void tearDown(void) {}

/* CTest 启动此 Unity 可执行文件；测试函数只由这里的 RUN_TEST 注册。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_debug_evaluate_reports_missing_right_operand_with_cause_and_suggestion);
    RUN_TEST(test_debug_condition_expression_reports_missing_logical_operand_with_suggestion);
    RUN_TEST(test_debug_condition_rejects_unresolved_or_operand);
    RUN_TEST(test_debug_condition_rejects_unresolved_and_operand);
    RUN_TEST(test_debug_evaluate_composed_comparison_logical_expression_returns_bool);
    RUN_TEST(test_debug_evaluate_formal_shift_expression_returns_int);
    RUN_TEST(test_debug_evaluate_formal_bitwise_expressions_return_int);
    RUN_TEST(test_debug_evaluate_formal_nested_shift_arithmetic_returns_int);
    RUN_TEST(test_debug_formal_array_literal_requires_explicit_allocation);
    RUN_TEST(test_debug_evaluate_semantic_summary_escapes_string_constants);
    RUN_TEST(test_debug_evaluate_semantic_summary_reports_unsigned_numeric_range);
    RUN_TEST(test_debug_evaluate_rejects_runtime_global_without_canonical_identity);
    RUN_TEST(test_debug_semantic_summary_replays_compiled_function_call_reference_fact);
    RUN_TEST(test_debug_semantic_summary_replays_compiled_top_level_variable_reference_fact);
    RUN_TEST(test_debug_semantic_summary_replays_compiled_ownership_fact);
    RUN_TEST(test_debug_semantic_summary_walks_type_query_operand_reference);
    RUN_TEST(test_debug_semantic_summary_replays_member_expression_payload_fact);
    RUN_TEST(test_debug_semantic_summary_replays_assignment_write_reference_facts);
    RUN_TEST(test_debug_semantic_summary_walks_member_receiver_facts);
    RUN_TEST(test_debug_semantic_summary_replays_conditional_branch_facts);
    RUN_TEST(test_debug_semantic_summary_walks_lambda_local_initializer_facts);
    RUN_TEST(test_debug_condition_rejects_unresolved_comparison_operands);
    RUN_TEST(test_debug_condition_rejects_unresolved_ternary_branch);
    RUN_TEST(test_debug_condition_rejects_unresolved_branch_with_runtime_globals);
    RUN_TEST(test_debug_condition_reports_missing_ternary_consequent_with_cause_and_suggestion);
    RUN_TEST(test_debug_condition_reports_missing_ternary_alternate_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluate_reports_numeric_operand_type_error_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluate_reports_division_by_zero_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluate_reports_invalid_numeric_literal_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluate_reports_missing_member_name_with_cause_and_suggestion);
    RUN_TEST(test_debug_condition_reports_missing_index_close_with_cause_and_suggestion);
    RUN_TEST(test_debug_condition_reports_missing_group_close_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluate_reports_unterminated_string_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluate_reports_unsupported_string_escape_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluate_rejects_unresolved_function_call_without_canonical_facts);
    RUN_TEST(test_debug_evaluate_rejects_assignment_with_cause_and_suggestion);
    RUN_TEST(test_debug_evaluation_effect_policy_classifies_canonical_expression_shapes);
    RUN_TEST(test_debug_evaluation_effect_policy_requires_explicit_capabilities);
    RUN_TEST(test_debug_evaluate_with_capabilities_enforces_effect_set);
    RUN_TEST(test_debug_evaluate_with_capabilities_preserves_formal_parse_diagnostic);
    RUN_TEST(test_debug_evaluate_rejects_unresolved_inactive_branch);
    RUN_TEST(test_debug_evaluation_effect_policy_marks_resolved_property_getter);
    RUN_TEST(test_debug_evaluation_effect_policy_marks_resolved_ownership_member);
    RUN_TEST(test_debug_breakpoint_condition_requires_pure_formal_evaluation);
    RUN_TEST(test_debug_breakpoint_logpoint_requires_pure_formal_evaluation);
    RUN_TEST(test_debug_evaluate_result_publishes_canonical_type_and_stop_state);
    RUN_TEST(test_debug_evaluate_failure_preserves_structured_diagnostic);
    RUN_TEST(test_debug_semantic_binding_preserves_paused_frame_canonical_identity);
    RUN_TEST(test_debug_semantic_binding_rejects_missing_paused_place);
    RUN_TEST(test_debug_source_binding_shadows_runtime_root_spelling);
    RUN_TEST(test_debug_formal_evaluation_rejects_paused_binding_type_drift);
    RUN_TEST(test_debug_semantic_binding_registers_canonical_receiver);
    RUN_TEST(test_debug_formal_evaluation_reads_indexed_paused_frame_binding);
    RUN_TEST(test_debug_formal_evaluation_resolves_generation_checked_runtime_root);
    RUN_TEST(test_debug_semantic_binding_publishes_canonical_closure_capture);
    RUN_TEST(test_debug_semantic_binding_rejects_entry_binding_without_identity);
    return UNITY_END();
}
