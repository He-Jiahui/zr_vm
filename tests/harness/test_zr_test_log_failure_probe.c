/**
 * @file test_zr_test_log_failure_probe.c
 * ## Summary
 * 故意使一个 Unity 用例失败，检查公共日志宏仍允许用例尾部与 runner 汇总继续执行。
 * 该程序是 verify_test_log_failure.cmake 的负例输入：子进程失败 1 可使外层契约脚本成功。
 *
 * ## Constraints and lifecycle
 * 每次在独立进程中只登记一个用例，进程级标记初始为零；没有资源取得或释放场景。
 * cleanup_reached 只表示失败后的赋值被执行，不能证明无泄漏、tearDown 清理或计时有效性。
 *
 * ## References
 * tests/CMakeLists.txt 构建当前配置探针并注册 test_log_failure_contract；
 * zr_test_log_macros.h 的 ZR_TEST_FAIL 与 docs/testing-and-validation/test-log-failure-contract.md。
 */
#include <stdio.h>
#include <time.h>

#include "unity.h"
#include "zr_test_log_macros.h"

/* 供 main 及外层脚本观察失败后的继续执行；单进程单用例标记，不代表资源释放。 */
static int g_cleanupReached = 0;

/**
 * ## Summary
 * 为 UnityDefaultTestRun 提供本探针的前置钩子；故意保持空环境以隔离失败宏的控制流契约。
 *
 * ## Returns
 * 无返回值，也不取得资源或重置进程级继续执行标记。
 *
 * ## Constraints and lifecycle
 * 由 Unity 在每个已登记用例前调用，不作为可复用的测试资源初始化器。
 *
 * ## References
 * main 的 RUN_TEST 经 UnityDefaultTestRun 调用本钩子，随后调用故意失败用例。
 */
void setUp(void) {
}

/**
 * ## Summary
 * 为 UnityDefaultTestRun 提供本探针的后置钩子；空钩子使尾部标记只反映用例内的继续执行。
 *
 * ## Returns
 * 无返回值，无释放副作用；不能用本钩子为空来证明一般用例不需要清理。
 *
 * ## Constraints and lifecycle
 * Unity 在用例保护边界之后派发本钩子；本探针没有资源或异常清理覆盖。
 *
 * ## References
 * UnityDefaultTestRun 在用例后调用 tearDown，再由 UnityConcludeTest 汇总失败状态。
 */
void tearDown(void) {
}

/**
 * ## Summary
 * 构造一次可继续执行的故意失败，使外层脚本能区分失败记账与用例提前中断。
 * 不取得资源，用尾部赋值观察 ZR_TEST_FAIL 是否保留调用者控制流。
 *
 * ## Returns
 * 无返回值；通过 Unity.CurrentTestFailed 和 g_cleanupReached 交付两个独立观测值。
 *
 * ## Constraints and lifecycle
 * 只由 main 的单个 RUN_TEST 调用；两端时间相同仅为初始化报告字段，不是性能断言。
 * 标记只证明赋值到达，不证明任何资源已释放或后续硬断言仍能继续。
 *
 * ## References
 * ZR_TEST_FAIL 仅标记当前失败；main 的 UNITY_END 和 verify_test_log_failure.cmake 联合消费结果。
 */
static void test_failure_marks_unity_without_skipping_cleanup(void) {
    SZrTestTimer timer;

    timer.startTime = clock();
    timer.endTime = timer.startTime;
    /* 两端已初始化，零间隔即可；此负例关注失败控制流，不校验时间精度。 */
    ZR_TEST_FAIL(timer, "Intentional failure probe", "expected failure");
    g_cleanupReached = 1;
}

/**
 * ## Summary
 * 在独立进程中登记唯一的故意失败用例，完成 Unity 汇总后输出继续执行标记供 CMake 消费。
 *
 * ## Returns
 * @return 当前单用例正常失败汇总时返回 1；外层脚本同时检查摘要及标记，再决定契约是否通过。
 *
 * ## Constraints and lifecycle
 * 不接受命令行参数、不构建自身；g_cleanupReached 的零值依赖新进程的静态初始化。
 * 输出标记位于 UNITY_END 之后，只说明正常返回到此处；扩展用例数须同步脚本的计数检查。
 * 空 setUp/tearDown 没有资源清理验证，失败退出值不能单独证明用例尾部已执行。
 *
 * ## References
 * verify_test_log_failure.cmake 的 execute_process 实际启动本程序；
 * tests/CMakeLists.txt 的 TARGET_FILE 选择已构建路径，test_log_failure_contract 运行外层脚本。
 */
int main(void) {
    int unityResult;

    UNITY_BEGIN();
    RUN_TEST(test_failure_marks_unity_without_skipping_cleanup);
    unityResult = UNITY_END();
    printf("ZR_TEST_FAIL_PROBE cleanup_reached=%d unity_exit=%d\n", g_cleanupReached, unityResult);
    return unityResult;
}
