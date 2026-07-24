# Syntax 12 M2: Task/Frame Runtime

## 状态与产出记录

- 状态：`completed`
- 开始时间：2026-07-25 05:48 +08:00
- 完成时间：2026-07-25 06:36 +08:00
- 完成项目：
  - 新增 layout 驱动的 core Task/frame runtime：同步完成零 frame allocation，pending
    才租用并复用 typed frame，state id、slot GC root、drop map 和 finally 都不从
    dynamic object 或名称推断。
  - 完成 sync completion、multiple suspension、fault/finally、multi-await、non-Copy
    result、completed-result root、frame GC/drop map 与 pool reuse 的 6 项直接测试。
  - 在固定 `08bcfb8 + M2 exact overlay + initialized submodules` 快照上完成 GCC、Clang、
    MSVC 每套 Task/frame 6/6 与 GC-domain 5/5；Clang ASan/UBSan Task/frame 6/6。
  - 修正测试 fixture 在 full GC 前创建而未根化第二个对象的生命周期错误，避免把平台相关
    悬挂裸指针误判为 runtime pool 通过。

## 产出位置

- 实施计划：`docs/plans/syntax/12-async-task-job-scheduler/m2-task-frame-runtime-implementation-plan.md`
- 验收记录：`tests/acceptance/2026-07-25-syntax-12-m2-task-frame-runtime.md`
- module 文档：`docs/core-runtime/task-frame-runtime.md`
