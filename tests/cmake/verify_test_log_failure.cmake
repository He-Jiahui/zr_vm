# ## Summary
# 消费故意失败的 Unity 探针，确认日志宏报告失败后仍允许用例走到尾部和正常汇总退出。
# tests/CMakeLists.txt 的 test_log_failure_contract 通过 cmake -P 调用此文件。
#
# ## Inputs and result
# PROBE_EXE 是已构建的 zr_vm_test_log_failure_probe 路径；上游用 TARGET_FILE 选择当前配置。
# 直接调用时传 -DPROBE_EXE=<路径> -P tests/cmake/verify_test_log_failure.cmake。
# 子进程故意返回 1；本脚本只有退出码、Unity 摘要与尾部标记均满足才成功返回给 CTest。
#
# ## Constraints
# 这里只核实当前单用例的继续执行和失败记账；cleanup_reached 是赋值标记，不证明资源已释放。
# 此脚本不会构建探针，也不改变 Unity 的失败计数；执行进程和配置注册分别承担职责。
# 对挂起子进程及未来多用例摘要的限制见下方具体 TODO。
#
# ## References
# tests/CMakeLists.txt:83 创建探针；tests/CMakeLists.txt:93 传入路径，tests/CMakeLists.txt:94 调用脚本。
# tests/harness/test_zr_test_log_failure_probe.c:77 调用失败宏，tests/harness/test_zr_test_log_failure_probe.c:78 标记尾部。
# tests/harness/test_zr_test_log_failure_probe.c:102 获取 Unity 退出值，tests/harness/test_zr_test_log_failure_probe.c:103 打印标记。
# tests/harness/zr_test_log_macros.h:153 仅设置失败状态，不调用中断测试的 TEST_ABORT。
# test_log_failure_contract 验证主动失败后仍执行尾部赋值并完成 Unity 汇总，不验证资源释放。
if (NOT DEFINED PROBE_EXE OR PROBE_EXE STREQUAL "")
    message(FATAL_ERROR "PROBE_EXE is required")
endif ()

# 运行单个探针并保留结果供下面三项联合判断，故意失败不是启动脚本本身失败。
# TODO: 此处没有子进程 TIMEOUT；核对 tests/CMakeLists.txt:90 的 CTest 注册及直接
# cmake -P 调用所需期限，再确定哪一层应负责终止挂起探针。
execute_process(
        COMMAND "${PROBE_EXE}"
        RESULT_VARIABLE probe_result
        OUTPUT_VARIABLE probe_stdout
        ERROR_VARIABLE probe_stderr
)

# 拼接 stdout 与 stderr 只为取证和失败诊断，不表示两个流的事件先后顺序。
set(probe_output "${probe_stdout}\n${probe_stderr}")

# 预期的 1 来自 Unity 汇总的一次用例失败；外层 CMake 仍须继续核对摘要和尾部标记。
if (NOT probe_result EQUAL 1)
    message(FATAL_ERROR
            "ZR_TEST_FAIL probe must return exactly one Unity failure; result=${probe_result}\n${probe_output}")
endif ()

# 当前探针只登记一个用例；退出码本身不能排除额外通过或忽略的用例。
# TODO: 这里按子串匹配；扩充 tests/harness/test_zr_test_log_failure_probe.c:101 的 RUN_TEST 时，
# 核对是否应限制整行和总数边界，避免 11 Tests 中的 1 Tests 被当成单用例摘要。
if (NOT probe_output MATCHES "1 Tests 1 Failures 0 Ignored")
    message(FATAL_ERROR "ZR_TEST_FAIL probe did not report exactly one Unity failure\n${probe_output}")
endif ()

# 此标记证明失败宏后仍到达赋值和 UNITY_END 后的打印，不作为资源释放或 tearDown 证明。
if (NOT probe_output MATCHES "ZR_TEST_FAIL_PROBE cleanup_reached=1 unity_exit=1")
    message(FATAL_ERROR "ZR_TEST_FAIL skipped cleanup after marking Unity failed\n${probe_output}")
endif ()

message(STATUS "ZR_TEST_FAIL reports one failure and preserves cleanup control flow")
