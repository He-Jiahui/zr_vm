#include <string.h>

#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_parser/compiler.h"

void setUp(void) {}
void tearDown(void) {}

/* 保留无操作的用户 panic handler，使测试由 runtime harness 的 fatal 分发继续处理。
 * 不吞没 fatal crash；runtime dispatcher 仍可调用 crash hook。 */
static void close_meta_exception_panic(SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
}

/* 为脚本用例创建 VM，登记源码编译入口和 system 模块的 Error 等原型。
 * 只检测 state 创建结果，未单独断言 system 注册返回值；最终源码执行结果提供后续场景检查。 */
static SZrState *close_meta_exception_new_state(void) {
    SZrState *state = ZrTests_Runtime_State_Create(close_meta_exception_panic);
    if (state != ZR_NULL) {
        ZrParser_ToGlobalState_Register(state);
        ZrVmLibSystem_Register(state->global);
    }
    return state;
}

/* 用诊断 sourceName 编译完整脚本，返回可由调用方执行并 Free 的函数。
 * 命名字符串分配失败返回 null；不使用 CompileTest 或先行生成 AOT entry。 */
static SZrFunction *close_meta_exception_compile(SZrState *state,
                                                 const TZrChar *source,
                                                 const TZrChar *sourceName) {
    SZrString *name = ZrCore_String_Create(
            state, (TZrNativeString)sourceName, strlen(sourceName));
    return name != ZR_NULL
                   ? ZrParser_Source_Compile(state, source, strlen(source), name)
                   : ZR_NULL;
}

/* 编译一次脚本，要求整数结果，并在执行成功或失败后释放已编译函数。
 * 编译失败不执行；函数 Free 与独立 VM Destroy 分属本 helper 和调用测试。 */
static TZrBool close_meta_exception_execute(SZrState *state,
                                            const TZrChar *source,
                                            const TZrChar *sourceName,
                                            TZrInt64 *outResult) {
    SZrFunction *function = close_meta_exception_compile(state, source, sourceName);
    TZrBool executed;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }
    executed = ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, outResult);
    ZrCore_Function_Free(state, function);
    return executed;
}

/* 无 using/@close 的 throw-catch 返回1作为基线，隔离普通 Error.message 与清理路径。
 * 只检查执行整数1；没有清理回调场景。 */
static void test_plain_throw_catch_message_control(void) {
    static const TZrChar source[] =
            "try { throw \"original\"; }\n"
            "catch (e) { if (e.message == \"original\") { return 1; } }\n"
            "return 0;\n";
    SZrState *state = close_meta_exception_new_state();
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(close_meta_exception_execute(
            state, source, "close_meta_plain_catch_control.zr", &result));
    TEST_ASSERT_EQUAL_INT64(1, result);
    ZrTests_Runtime_State_Destroy(state);
}

/* 同一编译函数执行三次，检查每次 original 被catch、call-info next链可达总数和handler深度归零。
 * 脚本虽递增 calls，本用例没有检查 calls 数量；有界链遍历以断言阻止循环失控。 */
static void test_repeated_close_meta_keeps_reusable_callinfo_chain_reachable(void) {
    static const TZrChar source[] =
            "class CloseProbe {\n"
            "    pub static var calls: int = 0;\n"
            "    pub @constructor() { }\n"
            "    pub @close(error) { CloseProbe.calls = CloseProbe.calls + 1; }\n"
            "}\n"
            "try { using (new CloseProbe()) { throw \"original\"; } }\n"
            "catch (e) { if (e.message == \"original\") { return 1; } }\n"
            "return 0;\n";
    SZrState *state = close_meta_exception_new_state();
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = close_meta_exception_compile(state, source, "close_meta_repeated.zr");
    TEST_ASSERT_NOT_NULL(function);
    /* 检查每次执行后缓存 next 链仍包含所有已分配 call-info，且没有剩余handler；calls字段不参与本用例断言。 */
    for (TZrUInt32 attempt = 0u; attempt < 3u; ++attempt) {
        TZrInt64 result = 0;
        TZrUInt32 reachableCallInfos = 0u;
        SZrCallInfo *cursor;

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
                state, function, &result));
        TEST_ASSERT_EQUAL_INT64(1, result);
        for (cursor = state->baseCallInfo.next; cursor != ZR_NULL;
             cursor = cursor->next) {
            TEST_ASSERT_LESS_OR_EQUAL_UINT32(state->callInfoListLength,
                                             reachableCallInfos + 1u);
            ++reachableCallInfos;
        }
        TEST_ASSERT_EQUAL_UINT32(state->callInfoListLength, reachableCallInfos);
        TEST_ASSERT_EQUAL_UINT32(0u, state->exceptionHandlerStackLength);
    }
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

/* 脚本 using 的 @close 返回后，catch必须读 original 且 close计数为1，才返回整数1。
 * 实际编译执行脚本，不主动 GC；没有任意层数的递归嵌套覆盖。 */
static void test_script_close_meta_preserves_outer_exception_through_nested_calls(void) {
    static const TZrChar source[] =
            "class CloseProbe {\n"
            "    pub static var calls: int = 0;\n"
            "    pub @constructor() { }\n"
            "    pub @close(error) { CloseProbe.calls = CloseProbe.calls + 1; }\n"
            "}\n"
            "try {\n"
            "    using (new CloseProbe()) { throw \"original\"; }\n"
            "} catch (e) {\n"
            "    if (e.message == \"original\") {\n"
            "        if (CloseProbe.calls == 1) { return 1; }\n"
            "    }\n"
            "}\n"
            "return 0;\n";
    SZrState *state = close_meta_exception_new_state();
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(close_meta_exception_execute(
            state, source, "close_meta_preserve_outer_exception.zr", &result));
    TEST_ASSERT_EQUAL_INT64(1, result);
    ZrTests_Runtime_State_Destroy(state);
}

/* 脚本 @close 抛 replacement，外层catch依据新 message 返回1，防止旧 original 覆盖新错误。
 * 只检查替换后的可观察message，不检查call-info布局或GC。 */
static void test_script_close_meta_new_exception_replaces_outer_exception(void) {
    static const TZrChar source[] =
            "class CloseProbe {\n"
            "    pub @constructor() { }\n"
            "    pub @close(error) { throw \"replacement\"; }\n"
            "}\n"
            "try {\n"
            "    using (new CloseProbe()) { throw \"original\"; }\n"
            "} catch (e) {\n"
            "    if (e.message == \"replacement\") { return 1; }\n"
            "}\n"
            "return 0;\n";
    SZrState *state = close_meta_exception_new_state();
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(close_meta_exception_execute(
            state, source, "close_meta_replace_outer_exception.zr", &result));
    TEST_ASSERT_EQUAL_INT64(1, result);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_plain_throw_catch_message_control);
    RUN_TEST(test_repeated_close_meta_keeps_reusable_callinfo_chain_reachable);
    RUN_TEST(test_script_close_meta_preserves_outer_exception_through_nested_calls);
    RUN_TEST(test_script_close_meta_new_exception_replaces_outer_exception);
    return UNITY_END();
}
