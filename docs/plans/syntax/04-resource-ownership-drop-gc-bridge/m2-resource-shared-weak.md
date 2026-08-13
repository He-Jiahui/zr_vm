# 04-M2 Shared/Weak 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md`
的 `M2 Shared/Weak`。

## 状态与产出记录

- 完成时间：2026-07-21 19:49 +08:00
- 状态：已完成（M2 晋级门）
- 已完成项目：
  - 已建立 process-local non-atomic Shared/Weak stable control schema，包含 object、strong、weak、
    isolation domain、alive/drop-in-progress 与 GC-ignore ownership；移除 linked weak-slot lifetime。
  - 已实现 share/clone/drop/degrade/wake、implicit weak、last-strong-before-Drop 失效、many-Weak
    存续和 final Weak control free；drop body 内 wake 必须失败。
  - 已让 source resource Shared/Weak 覆盖正常退出、显式 drop、throw unwind、value parameter、
    nested owner fields，并同步 frame-layout physical slot 与 dense cleanup mirror。
  - 已把 ownership builtins 标为 no-throw operation，同时递归保留 receiver/argument throw profile。
  - 已发布 structured `resource_shared_strong_cycle` warning fact，覆盖 self/reciprocal Shared edge，
    Weak back-edge negative 不报。
  - 已明确当前 `wake(weak)` 使用 nullable Shared ABI niche；built-in `Option<Shared<T>>` carrier
    与 VM/AOT construction contract 未完成，不作为本里程碑晋级声明。

## 最终验证结果

- GCC、Clang、MSVC 在各自独立构建目录完成同一 14-target 矩阵；每套 14/14 进程真实
  `exit 0`，Unity 合计 523/523、0 failure。
- 每套矩阵包含 resource Shared/Weak 10/10、resource Unique 13/13、parser 75/75、type
  inference 119/119、compiler integration 127/127、semantic-query diagnostics 33/33、
  dataflow 9/9、closure 1/1、exceptions 8/8、AOT ownership/scope 各 1/1、GC 66/66、
  native fast path 59/59，以及 resource-focused ExecBC/AOT 1/1。
- ExecBC/AOT 全量目标仍会触发与本里程碑无关的既有 generic arithmetic/bitwise AOT 基线；
  该运行明确作废，未被计入 M2 证据。GCC/Clang 的 resource-focused 目标已独占串行重跑，
  MSVC 同一 focused 目标也以真实进程退出码验收。
- `git diff --check` 通过；Shared/Weak 新模块在 GCC、Clang、MSVC 均完成 clean build。

## 当前实现边界

- M2 不包含 M3 owner borrow/receiver compile-time facts。
- M2 不包含 M4 `GcDomain`、`Gc<T>`/`GcBox<T>` bridge 或 no-hidden-ignore-registry gate。
- M2 不包含 `AtomicShared<T>`；普通 Shared control 始终使用 non-atomic counter 并拒绝不同
  isolation domain 的 copy/wake。
