---
plan_id: lsp-04-debug-and-repl
milestone: E4c
status: completed
completed_at: 2026-08-05 13:15 +08:00
baseline_revision: 591b97a
source_plans:
  - docs/plans/lsp/04-debug-and-repl.md
implementation:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c
tests:
  - tests/debug/test_debug_agent_protocol.c
  - tests/acceptance/2026-08-05-lsp-04-e4c-children-handles.md
record_type: milestone-acceptance
---

# E4c Generation-Checked Children Handles

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-05 13:15 +08:00 | 已完成 | E4 result transport children handle 的跨 resume 失效、单调分配、编号耗尽 fail-closed 与高值 scope fallback 封闭。 |

## 完成项目

1. `ZrDebug_AgentStart` 在 agent 生命周期开始时初始化 children-handle allocator。暂停状态清理只释放当前 snapshot，不再重置该 allocator；后续 stop 不能重新发放已清除的数值。
2. allocator wrap 到保留 handle 区间以下时停止发布 expandable reference。没有回绕重用、generation 猜测或替代 identity。
3. `ZrDebug_ReadVariables` 将 `ZR_DEBUG_VARIABLE_HANDLE_BASE` 以上的值保留给当前 stop 已注册 handles。找不到时直接失败，不能回退为 `frameId * 10 + scopeKind` 并意外读取新暂停态。
4. 新 DAP/TCP 黑盒回归在两个 paused states 中分别 evaluate 数组，断言 handles 不同，并确认旧 handle 的 `variables` 请求返回稳定 `-32002` failure。

## 验收证据

- GCC 11.4、Clang 14、MSVC 19.44 均构建并直接执行 `zr_vm_debug_agent_protocol_test`，每套 8/8、0 failures、真实 exit 0。
- RED 先在 GCC 复现：旧实现第一和第二个 `variablesReference` 均为 `1000`，新断言报 `Expected 1000 to be not equal to 1000`，共 8 tests / 1 failure。
- 详细命令、运行时长和边界清单见 [E4c acceptance](../../../../tests/acceptance/2026-08-05-lsp-04-e4c-children-handles.md)。

## 边界

本记录只关闭 LSP 04 E4 的 children-handle generation audit。它不改变 formal expression 的 canonical TypeId、evaluate structured failure schema 或 E5 REPL environment generation；这些 consumer 继续只使用已发布的 canonical contracts。
