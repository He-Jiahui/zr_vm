---
related_code:
  - tests/cmake/task-scheduler-queue-tests.cmake
  - tests/CMakeLists.txt
  - tests/task/test_task_job_scheduler.c
  - tests/task/task_scheduler_queue_reuse_cases.inc
implementation_files:
  - tests/cmake/task-scheduler-queue-tests.cmake
plan_sources:
  - docs/code-review/comment-standard.md
  - user: 全仓库调用优先的职责、使用限制与设计目的注释审查
tests:
  - tests/task/test_task_job_scheduler.c
  - tests/task/task_scheduler_queue_reuse_cases.inc
doc_type: module-detail
---

# Job/Scheduler 的 CTest 注册契约

## 入口与配置前提

CMakeLists.txt:187 通过 BUILD_TESTS 门禁进入测试目录。完整 tests 配置在 tests/CMakeLists.txt:8804 include 此片段；宏基础专用模式会在 tests/CMakeLists.txt:225 提前返回。上游 parser/core 目标门禁位于 tests/CMakeLists.txt:617、tests/CMakeLists.txt:618，测试程序在 tests/CMakeLists.txt:4752 创建。因此 BUILD_TESTS 为 ON 不能单独证明本测试被注册。

片段的 TARGET 条件（tests/cmake/task-scheduler-queue-tests.cmake:20）让目标缺失的配置省略该项。它消费上游目标，不获取 VM 对象、资源缓冲区或 Scheduler 所有权。创建和链接测试目标的阶段，与发布 CTest 元数据和后续执行程序的阶段分别承担不同职责。

## 注册输出和调用方式

tests/cmake/task-scheduler-queue-tests.cmake:21 发布名称 task_job_scheduler，COMMAND 指向已有 zr_vm_task_job_scheduler_test。构建该目标后，可从构建目录用 ctest -R '^task_job_scheduler$' --output-on-failure 选择它；名称正则选择整个测试程序。

程序入口 tests/task/test_task_job_scheduler.c:258 先进入 Unity，tests/task/test_task_job_scheduler.c:260 至 tests/task/test_task_job_scheduler.c:269 依次登记十个 RUN_TEST，tests/task/test_task_job_scheduler.c:270 将 Unity 汇总状态作为程序返回值。CTest 中一个测试项对应这次程序执行，不能把它拆称十个 CTest 项。当前注册数说明入口结构，不替代每个用例的断言审查或执行证据。

## 标签与执行时限

tests/cmake/task-scheduler-queue-tests.cmake:23 为已登记的同名项发布 ssa/task 标签和 120 秒 TIMEOUT。标签供测试筛选；不表示本程序覆盖了全部 SSA 或 task 行为。时限约束整次 CTest 程序执行，不为每个 Unity 用例分别提供 120 秒。

## 支持证据边界

tests/task/test_task_job_scheduler.c:256 将 queue reuse 用例 include 到同一翻译单元；tests/task/task_scheduler_queue_reuse_cases.inc:35 又读取生产 private queue helper。这些位置解释同一测试目标怎样接入队列回归。本片段审查不授予测试正文、生产 helper 或 tests/CMakeLists.txt 的整文件完成信用。

tests/acceptance/2026-10-02-task-scheduler-queue-reuse.md:117 记录首次加入注册的背景；其历史构建和运行结果只适用于该记录的快照。当前注释审查没有重新运行构建或 CTest，配置注册不能证明当前用例通过。
