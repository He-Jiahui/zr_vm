---
related_code:
  - tests/parser/test_ssa_conditional_cleanup.c
  - tests/parser/ssa_oracle_resume_fault_allocator.c
  - tests/cmake/ssa-cleanup-tests.cmake
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
implementation_files:
  - tests/parser/test_ssa_conditional_cleanup.c
plan_sources:
  - "user: 2026-10-16 根据现役 BUG 注释修复并验证"
tests:
  - tests/parser/test_ssa_conditional_cleanup.c
  - tests/cmake/ssa-cleanup-tests.cmake
doc_type: testing-guide
---

# Conditional cleanup 的 OOM 夹具前置状态

`diamond(false)` 完成 cleanup lowering 后带有 guarded-DROP 对应的状态映射。OOM 用例把指令还原为普通 DROP 时，旧映射也必须失效。旧用例保留了旧映射，Elaborate 的 STRUCTURE|EFFECT 预校验可以在注入 owner-analysis 分配失败前拒绝输入，因此出现“失败但未触发故障”的断言。真实 RED 为 24 项中的 23 项通过，失败发生在 injected 断言。

修复先释放旧映射的内部池，再释放外壳并置空，遵循 FreeFunction 的所有权规则；显式验证原始 IR，然后才保存事务性快照并开启 ordinal 注入。原有 OOM diagnostic、零 outstanding allocation、原 function/指令不变以及最终 guarded DROP 的断言全部保留。故障 wrapper 只覆盖它实际拦截的 owner-state 分配，不代表所有 clone/map 分配失败。

Windows 入口把断言输出指向 stderr、关闭系统故障对话框，并关闭 stdout 缓冲，确保失败由自动测试收集。`boolean` 测试 helper 改名为 `oracle_boolean`，避免 Windows SDK 的同名类型冲突。Core 链接依赖已在内存模块提交中补齐，夹具调用真实异常入口。

首次定向复测通过，但同轮 exception.c 发生更新，因此该次不是最终固定输入验收。最终 `integrated-support-batch-green` 构建及 CTest 全部自然退出 0，相关输入哈希未变化；53 项 CTest 包含完整 conditional cleanup 用例，pool 14 项及 CLI smoke 同轮通过。独立复核接受两级释放和保留的事务性断言。当前证据为 Windows clang-cl，不授予 WSL 或全仓其他测试通过信用。

原始失败与最终证据位于 `E:/cargo-targets/zr_vm/reports/review-bugfix-20261016/` 的 `integrated-support-oom-string-red-r2-*`、`integrated-ssa-fixture-green-*` 与 `integrated-support-batch-green-*` 日志及收据。
