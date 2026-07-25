# Syntax 12 M6.2: Task Runtime Migration

## 状态与产出记录

- 状态：`completed`
- 开始时间：2026-07-25 22:05 +08:00
- 完成时间：2026-07-25 23:20 +08:00
- 完成项目：
  - 删除 configured product graph 中 public `TaskRunner`、`autoCoroutine`、
    `zr.coroutine`、source `pump`/`step` 与 default scheduler compatibility
    surface，仅保留 `zr.task` 的 `Task`、`Job`、`Scheduler` 和
    `zr.thread.ThreadScheduler.schedule(Job)`。
  - parser 拒绝 `%async`、`%await` 与 `%async T`；task effect 与
    reference-escape 分析只从 direct AST `async`/`await` facts 推导，不再
    识别 hidden await compatibility call。
  - project JSON 不再加载 `autoCoroutine`；task/thread runtime 不再将
    scheduler start、step、pump 或 legacy runner 暴露给 source。
  - task/thread 回归替换为 legacy public-surface negative coverage 与
    canonical Job/Scheduler execution coverage，并保留既有 M4/M5 transport、
    cancellation、fault 与 exactly-once drop 生命周期覆盖。
  - 更新 task、coroutine、thread、language syntax 与 parser/semantic module
    文档，明确 canonical migration contract。
- 验收：
  - GCC 11.4、Clang 14.0、MSVC 17.14 在隔离构建目录中均通过
    `zr_vm_parser_test` 75/75、`zr_vm_task_runtime_test` 16/16、
    `zr_vm_thread_runtime_test` 25/25；九个测试进程均真实 exit 0。
  - 已移除 task-runtime regression binary 中的 retired positive
    TaskRunner/coroutine compatibility cases，避免将历史可执行覆盖作为
    canonical contract 证据。
  - `zr_vm_lib_task` 保持未改动；root CMake product graph 不构建也不注册
    该历史模块，因此它不是本里程碑 public surface 的组成部分。
