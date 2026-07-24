# Syntax 13 M3 Iterator Frame Runtime Record

## 状态与产出记录

- 状态：completed
- 开始时间：2026-07-25 02:22 +08:00
- 完成时间：2026-07-25 03:08 +08:00
- 当前阶段：建立同步、可直接单测的 iterator frame state machine、GC-rooted
  current value、terminal cleanup 和 typed pool reuse。
- 当前边界：不连接 M2 compiler/SemIR 到可执行字节码；不实现 async iterator、
  scheduler、artifact/AOT、debug/LSP、legacy generator migration 或 dynamic
  object-property fallback。
- 已完成项目：caller-owned frame 状态机、producer reentrancy rejection、GC root
  relocation resolution、completion/fault/early-close exactly-once cleanup、typed
  frame free-list reuse，以及两个独立 core Unity targets。
- 验收结果：独立 Debug GCC、Clang 14 和 MSVC 19.44 均以真实 exit 0 运行
  `zr_vm_iterator_runtime_test`（11 tests, 0 failures）与
  `zr_vm_iterator_gc_drop_test`（4 tests, 0 failures）。
- 产出：`iterator_runtime.h`、`iterator/frame.c`、`iterator/dispatch.c`、两个
  Unity targets、core runtime 模块文档和 acceptance 记录；未连接 compiler
  lowering、async/scheduler、artifact/AOT、debug/LSP 或 fallback 路径。
