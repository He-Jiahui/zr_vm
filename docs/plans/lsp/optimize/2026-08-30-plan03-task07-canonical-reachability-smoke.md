---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - tests/language_server/stdio_reachability_diagnostic_smoke.js
  - tests/language_server/stdio_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/stdio_smoke.js
doc_type: milestone-record
---

# Plan 03 Task 7.39: Canonical Reachability Smoke Contract

## 目标

使通用 stdio smoke 消费 parser 已发布的 canonical `unreachable_code` diagnostic，
并与专用 reachability smoke、local semantic query 的 warning code 和 message 保持一致。
LSP 只验证 query fact 的协议投影，不创建 short-circuit 专用事实。

## 执行

- 将 generic fixture 的断言从已废弃的 `short_circuit_unreachable` 改为
  `unreachable_code` 与 canonical `Unreachable code` message。
- 保留 warning severity 断言；未增加 marker 白名单，也未按源码、名称、类型文本或消息
  重建 semantic fact。
- parser producer 与 `stdio_reachability_diagnostic_smoke.js` 的既有合同均已核对，确认
  专用 smoke 已明确拒绝 `short_circuit_unreachable`。

## 验证

- `node --check tests/language_server/stdio_smoke.js`：通过。
- `git diff --check`：通过。
- parser producer、专用 smoke 与通用 smoke 的 source-level contract：已对齐。
- 运行时 stdio smoke：本次未计为 GREEN。现有独立 GCC binary 启动前缺少
  `libzr_vm_lib_math.so`，且该 build 的 `lib` 目录没有该库，因此无法取得有效的 fixture
  生成与 smoke 退出码证据。修复或重建带完整 native library 的三工具链快照后，必须重跑
  三套 stdio/CLI smoke 以及 Plan 03 的 16-target matrix。

## 状态与产出记录

- 完成时间：2026-08-30 11:35 +08:00。
- 状态：canonical reachability smoke 合同修正完成；运行时回放待补，Plan 03 Task 7/Task 8
  与整体 GREEN 仍未完成。
- 完成项目：generic stdio smoke 的 canonical diagnostic 断言、producer/专用 smoke
  一致性审计、Node syntax 与 diff 检查。
- 未完成项目：完整三工具链 16-target matrix、三套 stdio/CLI smoke 的新基线回放，以及
  既有 parser/metadata producer failures。
