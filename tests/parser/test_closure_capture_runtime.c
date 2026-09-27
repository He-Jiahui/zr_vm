#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser.h"

// 每个场景自建 VM，Unity fixture 不分配共享状态。
void setUp(void) {}

// BUG: 断言失败会从测试体 longjmp 到 Unity；本地 state 的末尾 Destroy 因而被跳过。
void tearDown(void) {}

// 以独立源文件编译测试程序；返回的函数随 state 的 GC 生命周期存在。
static SZrFunction *compile_source(SZrState *state, const char *source, const char *sourceNameText) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(sourceNameText);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

// 创建函数返回的闭包必须延长捕获局部值的寿命，离开 makeRunner 后仍可读 seed。
static void test_returned_lambda_preserves_captured_local_value(void) {
    const char *source =
            "fn makeRunner() {\n"
            "    var seed = 4;\n"
            "    return fn() => {\n"
            "        return seed + 1;\n"
            "    };\n"
            "}\n"
            "var runner = makeRunner();\n"
            "return runner();";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "closure_capture_runtime_fixture.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(5, result);

    ZrTests_Runtime_State_Destroy(state);
}

// 独立 Unity 入口，确保该跨调用帧捕获场景在构建系统中执行。
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_returned_lambda_preserves_captured_local_value);
    return UNITY_END();
}
