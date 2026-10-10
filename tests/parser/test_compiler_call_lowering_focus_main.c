#include <stdio.h>

#include "unity.h"
#include "zr_test_log_macros.h"
/** @brief 从编译器回归用例借用九个入口，集中检查已知调用降级及真实基准路径。 */
extern void test_known_vm_call_results_keep_typed_arithmetic_specialization(void);
extern void test_known_native_member_calls_quicken_to_dedicated_member_call_opcode(void);
extern void test_static_native_box_member_call_executes_without_receiver_frame_rewrite(void);
extern void test_direct_child_function_calls_quicken_to_known_vm_call_family(void);
extern void test_loop_child_function_calls_quicken_to_known_vm_call_family(void);
extern void test_matrix_add_2d_benchmark_project_compile_quickens_array_add_loop_calls(void);
extern void test_map_object_access_benchmark_project_compile_quickens_labelFor_loop_call(void);
extern void test_call_chain_polymorphic_benchmark_project_compile_quickens_loop_helper_calls(void);
extern void test_call_chain_polymorphic_dispatch_callable_parameter_quickens_to_known_vm_call(void);
/* 专项目标由 CMake 独立构建并手工执行；资源仍由各回归用例管理。 */
/**
 * @brief 集中派发调用降级的九个回归用例，提供 typed 算术、native/member、子函数和真实基准调用路径的独立检查入口。
 *
 * ## Summary
 * 直接运行 zr_vm_compiler_call_lowering_focus_test；当前已查注册配置没有把本目标加入 CTest suite。
 * 套件标题、分隔线及 Completed 文本标识派发阶段；测试判据由用例断言及 Unity 计账交付。
 *
 * ## Parameters
 * 无参数；由可执行目标作为进程入口调用，不作为可重入测试 helper。
 *
 * ## Returns
 * 返回 UNITY_END() 的失败计数；零值表示没有被 Unity 计为失败，忽略用例仍单独统计。
 *
 * ## Constraints
 * 目标链接回归源及空 setUp/tearDown；每个用例管理自己的 fixture，入口不会在失败后替它释放。
 * 本入口不启动公共日志 timer；分隔线不登记测试。RUN_TEST 的保护和生命周期回调由 Unity 调度。
 *
 * ## References
 * CMake 的独立 focus 目标链接本入口及回归/编译 fixture；这里用 RUN_TEST 交给 Unity 派发，用例负责具体断言和资源。
 * UnityDefaultTestRun 派发回调，UnityConcludeTest 计账，UnityEnd 为 suite runner 提供退出结果。
 */
int main(void) {
    printf("\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("ZR-VM Compiler Call Lowering Focus\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("\n");

    UNITY_BEGIN();
    /* 用同一组 Unity 断言覆盖 typed 算术、native/member、子函数与基准调用路径。 */
    RUN_TEST(test_known_vm_call_results_keep_typed_arithmetic_specialization);
    RUN_TEST(test_known_native_member_calls_quicken_to_dedicated_member_call_opcode);
    RUN_TEST(test_static_native_box_member_call_executes_without_receiver_frame_rewrite);
    RUN_TEST(test_direct_child_function_calls_quicken_to_known_vm_call_family);
    RUN_TEST(test_loop_child_function_calls_quicken_to_known_vm_call_family);
    RUN_TEST(test_matrix_add_2d_benchmark_project_compile_quickens_array_add_loop_calls);
    RUN_TEST(test_map_object_access_benchmark_project_compile_quickens_labelFor_loop_call);
    RUN_TEST(test_call_chain_polymorphic_benchmark_project_compile_quickens_loop_helper_calls);
    RUN_TEST(test_call_chain_polymorphic_dispatch_callable_parameter_quickens_to_known_vm_call);

    printf("\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("Compiler Call Lowering Focus Completed\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("\n");

    return UNITY_END();
}
