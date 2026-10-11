# ## Summary
# 把已有的 Job/Scheduler 测试程序纳入 CTest，供常规回归按名称或标签选择调度支持测试。
# tests/CMakeLists.txt 在目标创建后的尾部 include 本片段；这里交付测试注册元数据。
#
# ## Registration inputs and result
# 输入是上游配置是否已建立 zr_vm_task_job_scheduler_test。
# 目标存在时发布 task_job_scheduler，一个 CTest 项运行该程序 main 登记的十个 Unity 用例。
# 程序退出状态交给 CTest；配置阶段的 add_test 不证明程序已构建或用例已通过。
#
# ## Constraints
# BUILD_TESTS 只控制是否进入 tests 目录；宏基础专用模式会提前 return，无法到达此 include。
# 普通流程中目标还依赖 parser/core 目标门禁；缺目标时跳过注册，避免留下不可执行测试。
# LABELS 的 ssa/task 是测试筛选元数据；TIMEOUT 120 约束一次整程序执行，不逐个重置 Unity 用例。
#
# ## References
# CMakeLists.txt:187 进入测试目录；tests/CMakeLists.txt:225 是宏基础专用模式的提前返回。
# tests/CMakeLists.txt:617、tests/CMakeLists.txt:618 是目标创建门禁，tests/CMakeLists.txt:4752 选择此目标。
# tests/CMakeLists.txt:8804 实际 include；tests/task/test_task_job_scheduler.c:471 是程序入口。
# tests/task/test_task_job_scheduler.c:473、tests/task/test_task_job_scheduler.c:482 界定 main 的十个 RUN_TEST。
if (TARGET zr_vm_task_job_scheduler_test)
    add_test(NAME task_job_scheduler COMMAND zr_vm_task_job_scheduler_test)
    # 分类和整项时限随同一个 CTest 名称发布；用例仍由测试程序自己的 main 顺序登记。
    set_tests_properties(task_job_scheduler PROPERTIES
            LABELS "ssa;task"
            TIMEOUT 120)
endif ()
