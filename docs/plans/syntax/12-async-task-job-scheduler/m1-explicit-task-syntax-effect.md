# Syntax 12 M1: Explicit Task Syntax And Effect

## 状态与产出记录

- 状态：`completed`
- 开始时间：2026-07-25 03:27 +08:00
- 完成时间：2026-07-25 05:22 +08:00
- 完成项目：
  - `async fn`、class/struct async member 和 `async fn` lambda 保留源码中的
    closed `zr.task.Task<T>` TypeRef；`%async` 继续被标记为 legacy lowering。
  - `await expr` 已有独立 AST、Task-handle role payload inference、effect scope
    validation、reference-escape suspension epoch，以及 expression/return/变量
    初始化的 `SUSPEND -> JOIN -> RESUME` CFG 边。
  - canonical callable effect 从 declaration 统一派生，named function 的
    `TypeId`、member callable refinement 和 lambda effect 均使用
    `ZR_CANONICAL_CALLABLE_EFFECT_ASYNC`，不以名字或文本推断。
  - 三工具链 focused 验证：GCC、Clang、MSVC 的 CFG 均 6/6、type inference
    均 119/119；M1 新增 task regressions 均通过。
  - `zr_vm_task_runtime_test` 在三工具链仍固定为 54 项中的 4 项 legacy
    `TaskRunner` failures，已在 acceptance 中逐项记录，归属 M6 migration，
    不计作 M1 通过证据。

## 产出位置

- 实施计划：`docs/plans/syntax/12-async-task-job-scheduler/m1-explicit-task-syntax-effect-implementation-plan.md`
- 验收记录：`tests/acceptance/2026-07-25-syntax-12-m1-explicit-task-syntax-effect.md`
- module 文档：`docs/parser-and-semantics/async-task-syntax-and-effect.md`
