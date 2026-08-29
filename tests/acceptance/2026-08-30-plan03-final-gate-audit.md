---
related_code:
  - tests/language_server/stdio_smoke.js
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-30-plan03-final-gate-audit.md
doc_type: acceptance-record
---

# Plan 03 Final Gate Acceptance

## Acceptance Boundary

本验收记录只接受有真实进程退出码和 failure marker 的证据。单个测试框架返回 exit 0
但含 `Fail -` 的结果仍视为失败；缺少 parser/metadata fact 时，LSP 必须保持 fail closed。

## Result

- `semantic_query_parity`: GCC/Clang/MSVC `14/14`, exit 0。
- `source_contracts`: GCC/Clang/MSVC exit 0；`inlay_semantic_facts`: 三套 `13/13`。
- `lsp_interface`: GCC/Clang/MSVC exit 1，三套均 `111 Pass / 2 Fail`。
- `lsp_project_features`: 三套 process exit 0，但均 `51 Pass / 9 Fail`，不计通过。
- GCC `semantic_analyzer`: exit 1，`68 Pass / 2 Fail`；Clang/MSVC 当前快照没有该目标。
- `stdio_smoke.js`: GCC/Clang/MSVC 均 exit 1，统一停止于缺少
  `short_circuit_unreachable` warning 的 generic fixture。
- GCC/Clang/MSVC CLI `--version`: exit 0。

## 状态与产出记录

- 完成时间：2026-08-30 05:56 +08:00。
- 状态：未通过，Plan 03 Task 7/Task 8 保持进行中。
- 完成项目：执行最终只读门禁并记录真实 exit、Pass/Fail 计数、stdio/CLI smoke 结果，
  确认 diagnostics projector 的 canonical ownership 未新增问题。
- 未完成项目：Syntax05 producer release、跨项目 canonical identity、reference-call
  diagnostic fact、generic/ownership analyzer facts、semantic-token consumer migration。
