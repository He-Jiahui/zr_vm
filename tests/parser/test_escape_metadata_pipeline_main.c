#include <stdio.h>

#include "unity.h"
#include "zr_test_log_macros.h"

extern void test_compiler_escape_metadata_summarizes_capture_return_and_exports(void);
extern void test_binary_roundtrip_preserves_escape_metadata_summaries(void);
extern void test_runtime_global_binding_marks_returned_object_as_global_root(void);
extern void test_runtime_native_callback_capture_marks_returned_object_as_native_handle(void);
extern void test_binary_roundtrip_runtime_global_binding_preserves_escape_flags(void);
extern void test_binary_roundtrip_runtime_native_callback_preserves_escape_flags(void);
extern void test_runtime_returned_callable_capture_marks_closed_capture_object(void);
extern void test_runtime_global_callable_capture_marks_closed_capture_object(void);
extern void test_binary_roundtrip_runtime_returned_callable_capture_preserves_closed_capture_escape_flags(void);
extern void test_binary_roundtrip_runtime_global_callable_capture_preserves_closed_capture_escape_flags(void);
extern void test_runtime_compiled_child_functions_detach_owner_links(void);
extern void test_binary_roundtrip_runtime_child_functions_detach_owner_links(void);

// 独立 escape_pipeline 目标注册全部跨编译单元用例；compiler_integration 还复用其中两例。
/**
 * @brief 派发逃逸元数据的编译/二进制形状用例及运行时闭包用例，作为独立 escape_pipeline 的进程入口。
 *
 * ## Summary
 * 直接运行 zr_vm_escape_pipeline_test，或由 CTest escape_pipeline 的 executable suite runner 启动。
 * 套件标题、分隔线及 Completed 文本标识派发阶段；测试判据由用例断言及 Unity 计账交付。
 *
 * ## Parameters
 * 无参数；由可执行目标作为进程入口调用，不作为可重入测试 helper。
 *
 * ## Returns
 * 返回 UNITY_END() 的失败计数；零值表示没有被 Unity 计为失败，忽略用例仍单独统计。
 *
 * ## Constraints
 * 各用例自行持有并尝试清理编译、二进制和运行时资源；main 只负责注册和汇总，不拥有或保证释放。
 * TODO: 用例侧的正常路径漏释放与硬断言越过清理仍需处理；空 setUp/tearDown 不兜底。
 * 本入口不启动公共日志 timer；分隔线不登记测试。RUN_TEST 的保护和生命周期回调由 Unity 调度。
 *
 * ## References
 * 本入口注册十二例；compiler_integration 另复用其中两个形状用例。CMake 注册独立 escape_pipeline 并由 runner 检查退出码。
 * UnityDefaultTestRun 派发回调，UnityConcludeTest 计账，UnityEnd 为 suite runner 提供退出结果。
 */
int main(void) {
    printf("\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("ZR-VM Escape Metadata Pipeline\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("\n");

    UNITY_BEGIN();

    printf("==========\n");
    printf("Escape Metadata Shape Tests\n");
    printf("==========\n");
    RUN_TEST(test_compiler_escape_metadata_summarizes_capture_return_and_exports);
    RUN_TEST(test_binary_roundtrip_preserves_escape_metadata_summaries);

    printf("==========\n");
    printf("Runtime Escape Closure Tests\n");
    printf("==========\n");
    RUN_TEST(test_runtime_global_binding_marks_returned_object_as_global_root);
    RUN_TEST(test_runtime_native_callback_capture_marks_returned_object_as_native_handle);
    RUN_TEST(test_binary_roundtrip_runtime_global_binding_preserves_escape_flags);
    RUN_TEST(test_binary_roundtrip_runtime_native_callback_preserves_escape_flags);
    RUN_TEST(test_runtime_returned_callable_capture_marks_closed_capture_object);
    RUN_TEST(test_runtime_global_callable_capture_marks_closed_capture_object);
    RUN_TEST(test_binary_roundtrip_runtime_returned_callable_capture_preserves_closed_capture_escape_flags);
    RUN_TEST(test_binary_roundtrip_runtime_global_callable_capture_preserves_closed_capture_escape_flags);
    RUN_TEST(test_runtime_compiled_child_functions_detach_owner_links);
    RUN_TEST(test_binary_roundtrip_runtime_child_functions_detach_owner_links);

    printf("\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("Escape Metadata Pipeline Completed\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("\n");

    return UNITY_END();
}
