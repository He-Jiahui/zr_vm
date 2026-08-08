---
related_code:
  - tests/language_server/stdio_smoke.js
implementation_files:
  - tests/language_server/stdio_smoke.js
plan_sources:
  - user: 2026-08-08 improve semantic inference and execute docs/plans/lsp stages
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/language_server/stdio_smoke.js
  - tests/acceptance/2026-08-08-lsp-l6-warm-request-latency-budget.md
doc_type: milestone-detail
plan_id: lsp-03-robustness
record_id: 2026-08-08-warm-request-latency-budget
status: completed
completed_at: 2026-08-08 17:40 +08:00
evidence_scope: stdio-warm-hover-completion-signature-latency
---

# Warm Request Latency Budget

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-08 17:40 +08:00 | 已完成 | stdio warm hover/completion/signatureHelp 的20样本p50/p95/p99测量、计划p95门禁与10次独立重复证据。 |

## 已实现契约

- tests/language_server/stdio_smoke.js 在已通过语义正确性断言后的warm server上，分别连续发送20个 textDocument/hover、textDocument/completion 和 textDocument/signatureHelp 请求。
- 样本使用 process.hrtime.bigint() 从写入request到对应response完成的端到端时间；p50、p95和p99由排序样本计算。启动、打开文档、初始parse和冷cache不计入该leaf。
- hover使用native descriptor callable，completion使用closed generic Derived<Item, 4>，signatureHelp使用closed generic receiver call。这些请求继续只消费既有canonical query/property/callable contracts，本leaf不增加LSP本地类型或文本推断。
- CI gate强制warm hover p95不超过50ms，completion/signatureHelp p95各不超过100ms，超过时输出三分位值并失败。

## TDD 与验证

- MSVC stdio E2E：10个独立Node server进程均真实exit 0。每个进程测量60个warm request。10轮中观察到的最大值为：hover p50/p95/p99 = 4.04/20.07/20.53ms，completion = 4.81/15.16/23.81ms，signatureHelp = 1.98/4.17/13.91ms。
- ctest --test-dir .codex/build-e5-closure-msvc --output-on-failure -R ^language_server_stdio_smoke$ 为1/1、真实exit 0。该smoke同时保留LSP初始化、document v1、canonical request结果、cancellation和content-modified协议断言。
- query/schema generation没有在本leaf改变；请求均针对已打开的version 1 document。该记录验证LSP 03预算门禁的一部分，不把单机Debug MSVC数据泛化为跨平台性能结论。

## 未完成边界

- 单文件diagnostics p95 <= 250ms、100-file reference workspace单文件增量p95 <= 500ms、p99趋势、峰值服务端内存、256MiB cache/LRU、Linux/Clang/MSVC可比性能环境和完整snapshot stress仍未完成。
- 本leaf不改变完整L6状态；它只提供可重复的canonical request warm-latency baseline。
