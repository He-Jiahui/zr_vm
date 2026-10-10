/**
 * @file zr_test_log_macros.h
 * ## Summary
 * 为 Unity 测试提供统一的上下文和片段耗时日志；标题、分隔线及 Pass 文本属于诊断协议。
 * 用例判定仍由 Unity 断言及汇总负责，只有 ZR_TEST_FAIL 会标记当前用例失败。
 *
 * ## Constraints and lifecycle
 * 日志同步借用调用者的字符串及计时字段，不保存它们，也不取得 VM 或文件等资源所有权。
 * 刷新 stdout 便于及时观察上下文，但输出错误没有状态返回，不能保证崩溃前所有消息都已送达。
 * 标题和分隔线可在 runner 中使用；失败宏须在活动 Unity 用例内使用，清理和退出仍由调用者安排。
 *
 * ## References
 * runtime_support.h、tests/test_support.h 向运行时测试转接本接口；
 * test_zr_test_log_failure_probe.c 与 verify_test_log_failure.cmake 验证失败后的继续执行。
 * docs/testing-and-validation/test-log-failure-contract.md 说明计时口径和结果消费边界。
 */
#ifndef ZR_VM_TESTS_ZR_TEST_LOG_MACROS_H
#define ZR_VM_TESTS_ZR_TEST_LOG_MACROS_H

#include <stdio.h>
#include <time.h>

#include "unity.h"

/**
 * ## Summary
 * 保存调用者选择的测试片段两端 clock() 样本，供 ZR_TEST_PASS 和 ZR_TEST_FAIL 输出诊断耗时。
 * 标题宏不启动计时，结果宏也不补采样；用例自行决定是否把准备、执行和清理包含在片段内。
 *
 * ## Fields
 * startTime 是片段起点，endTime 是报告前的终点；两者应来自同一进程、同一 clock() 口径。
 *
 * ## Invariants and lifecycle
 * 这是不持有资源的值记录；报告前须初始化两个字段，并保证其差可由 clock_t 表示。
 * CLOCKS_PER_SEC 只负责单位换算，不能把不同平台的 clock() 语义统一为 CPU 时间。
 * ISO C 的口径是处理器时间，MSVC CRT 则记录墙上经过时间；此值不承担性能验收或超时判定。
 * TODO: 核对实际调用方对 clock() 返回 (clock_t)-1 及不可表示区间的处理；
 * 当前字段和日志没有计时有效性状态，入口见失败探针的采样及报告宏的差值表达式。
 *
 * ## References
 * test_zr_test_log_failure_probe.c 只需两端已初始化来验证失败控制流，不验证计时精度。
 * test_system_fs_module.c 由用例采样；宏还接受具有兼容字段的其他计时记录，并不要求本 typedef。
 * 平台依据见 docs/testing-and-validation/test-log-failure-contract.md 的 Microsoft clock 官方文档。
 */
typedef struct SZrTestTimer {
    /** 由调用者采集的片段起点；标题日志不会填写本字段。 */
    clock_t startTime;
    /** 报告前由调用者填写的片段终点；同起点表示零间隔，而非未初始化状态。 */
    clock_t endTime;
} SZrTestTimer;

/**
 * ## Summary
 * 在用例开始阶段发布标题，便于把后续诊断关联到当前场景；不启动计时或注册 Unity 用例。
 *
 * ## Parameters
 * @param[in] summary 有效的以 NUL 结尾的标题，借用至本次同步输出完成；不得为 NULL。
 *
 * ## Returns
 * 无返回值；结果是 stdout 上的标题及刷新请求，不修改 Unity 的判定。
 *
 * ## Constraints and lifecycle
 * 字符串由调用者保有；没有输出错误通道或并发日志分组保证。若需耗时，另行填写计时记录。
 *
 * ## References
 * test_system_fs_module.c 在取得测试状态前输出场景标题；ZR_TEST_INFO 补充本次验证的上下文。
 */
#define ZR_TEST_START(summary)                                                                                       \
    do {                                                                                                               \
        printf("Unit Test - %s\n", summary);                                                                           \
        fflush(stdout);                                                                                                \
    } while (0)

/**
 * ## Summary
 * 在断言或被测操作之前说明验证主题与场景，给失败日志提供用例名称之外的诊断上下文。
 *
 * ## Parameters
 * @param[in] summary 验证主题的有效 NUL 结尾字符串，借用至输出完成，不得为 NULL。
 * @param[in] details 场景或预期的有效 NUL 结尾字符串，借用至输出完成，不得为 NULL。
 *
 * ## Returns
 * 无返回值；只请求输出并刷新 stdout，不执行断言或改变用例结果。
 *
 * ## Constraints and lifecycle
 * 输出不会取得字符串所有权；文本不保证跨线程或与其他输出流保持原子分组。
 *
 * ## References
 * test_compiler_features.c 在参数编译测试中说明检查目的；ZR_TEST_START 提供外层用例标题。
 */
#define ZR_TEST_INFO(summary, details)                                                                                 \
    do {                                                                                                               \
        printf("Testing %s:\n %s\n", summary, details);                                                                \
        fflush(stdout);                                                                                                \
    } while (0)

/**
 * ## Summary
 * 在调用者已完成验证后输出片段耗时和成功诊断，沿用测试日志的统一格式。
 * Pass 文本不是 Unity 判定，也不会清除之前已置位的失败标记。
 *
 * ## Parameters
 * @param[in] timer 具有已初始化 startTime/endTime 兼容字段的计时记录；同步读取而不修改或保存。
 * @param[in] summary 有效的 NUL 结尾摘要，借用至输出完成，不得为 NULL。
 *
 * ## Returns
 * 无返回值；输出诊断并请求刷新 stdout，用例是否通过仍由 Unity 汇总决定。
 *
 * ## Constraints and lifecycle
 * timer 表达式会分别用于读取两端，须稳定且无副作用；避免与宏内部的 zrTestElapsed 标识符冲突。
 * 调用者先填写两端并满足 SZrTestTimer 的时钟限制；宏不调用 clock()，也不验证样本是否有效。
 * 本宏不释放资源，调用者可在其前后安排清理；诊断片段边界由采样位置决定。
 *
 * ## References
 * test_system_fs_module.c 在释放测试状态后采集终点再报告；
 * test_compiler_regressions.c 使用具有相同字段的局部计时类型。失败标记接口是 ZR_TEST_FAIL。
 */
#define ZR_TEST_PASS(timer, summary)                                                                                   \
    do {                                                                                                               \
        double zrTestElapsed = ((double) ((timer).endTime - (timer).startTime) / CLOCKS_PER_SEC) * 1000.0;           \
        printf("Pass - Cost Time:%.3fms - %s\n", zrTestElapsed, summary);                                             \
        fflush(stdout);                                                                                                \
    } while (0)

/**
 * ## Summary
 * 报告用例失败并标记当前 Unity 状态，同时保留宏之后的控制流，让调用者执行自己的清理。
 * 与触发 TEST_ABORT 的断言不同，本接口不返回调用函数，也不执行资源释放。
 *
 * ## Parameters
 * @param[in] timer 具有已初始化 startTime/endTime 兼容字段的记录，借用读取而不保存或修改。
 * @param[in] summary 有效的 NUL 结尾失败场景，借用至输出完成，不得为 NULL。
 * @param[in] reason 有效的 NUL 结尾失败原因，借用至输出完成，不得为 NULL。
 *
 * ## Returns
 * 无返回值；设置 Unity.CurrentTestFailed 并请求刷新。用例结束时由 UnityConcludeTest 记账；
 * 已忽略用例走 Unity 的忽略分支，不能仅凭此宏保证最终失败数增加。
 *
 * ## Constraints and lifecycle
 * 须在串行的活动 Unity 用例中使用；不负责初始化 Unity、注册用例或汇总退出码。
 * timer 表达式须稳定、无副作用，且不与宏内部 zrTestElapsed 名称冲突；时钟限制同 SZrTestTimer。
 * 调用者在预期失败路径填写终点，并自行释放已取得资源、选择 return 或继续；不能假设宏会代做。
 *
 * ## References
 * test_zr_test_log_failure_probe.c 验证宏之后能赋值并到达 Unity 汇总；
 * test_compiler_features.c 的失败分支在报告后自行 destroy_test_state 并返回。
 */
#define ZR_TEST_FAIL(timer, summary, reason)                                                                           \
    do {                                                                                                               \
        double zrTestElapsed = ((double) ((timer).endTime - (timer).startTime) / CLOCKS_PER_SEC) * 1000.0;           \
        printf("Fail - Cost Time:%.3fms - %s:\n %s\n", zrTestElapsed, summary, reason);                               \
        fflush(stdout);                                                                                                \
        Unity.CurrentTestFailed = 1;                                                                                    \
        UNITY_OUTPUT_FLUSH();                                                                                           \
    } while (0)

/**
 * ## Summary
 * 为相邻用例的诊断输出划出可读边界，便于 runner 与单个用例共享日志格式。
 *
 * ## Returns
 * 无返回值；只请求输出分隔线并刷新 stdout。
 *
 * ## Constraints and lifecycle
 * 不结束 Unity 用例、不释放资源，也不改变汇总；可在 RUN_TEST 返回后或用例自己的尾部调用。
 * 输出失败没有反馈，并发输出仍可能交错。
 *
 * ## References
 * test_literal_surface_main.c 在 RUN_TEST 之间使用本宏；ZR_TEST_MODULE_DIVIDER 表示外层分组。
 */
#define ZR_TEST_DIVIDER()                                                                                              \
    do {                                                                                                               \
        printf("----------\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

/**
 * ## Summary
 * 在 runner 的模块标题或用例组切换处标识外层日志边界，使多组用例的输出便于定位。
 *
 * ## Returns
 * 无返回值；只请求输出模块分隔线并刷新 stdout。
 *
 * ## Constraints and lifecycle
 * 不注册测试、不调用 UNITY_BEGIN/UNITY_END，也不启动计时；runner 自己维护测试执行和汇总时序。
 * 无资源所有权或输出错误通道，并发消息不保证按模块分组。
 *
 * ## References
 * test_compiler_call_lowering_focus_main.c 在 UNITY_BEGIN 前和 UNITY_END 前组织模块诊断；
 * ZR_TEST_DIVIDER 提供用例之间的较小边界。
 */
#define ZR_TEST_MODULE_DIVIDER()                                                                                       \
    do {                                                                                                               \
        printf("==========\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

#endif
