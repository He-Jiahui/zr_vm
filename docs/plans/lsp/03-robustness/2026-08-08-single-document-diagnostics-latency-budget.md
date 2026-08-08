---
related_code:
  - tests/language_server/stdio_smoke.js
implementation_files:
  - tests/language_server/stdio_smoke.js
tests:
  - tests/language_server/stdio_smoke.js
  - tests/acceptance/2026-08-08-lsp-l6-single-document-diagnostics-latency-budget.md
doc_type: milestone-detail
plan_id: lsp-03-robustness
record_id: 2026-08-08-single-document-diagnostics-latency-budget
status: completed
completed_at: 2026-08-08 17:47 +08:00
---

# Single Document Diagnostics Latency Budget

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-08 17:47 +08:00 | 已完成 | versioned single-document diagnostics 的20样本p50/p95/p99测量与250ms p95门禁。 |

## 已实现契约

- 独立且语义有效的document先以v1打开，再连续20次发送等长trivia edit v2..v21；每次只在收到同一URI和同一version的空publishDiagnostics后计时。
- 样本覆盖didChange到publishDiagnostics的stdio协议往返，不以request latency替代诊断时间。p95超过250ms时smoke失败并打印p50/p95/p99。
- 此leaf不改变semantic query/schema或LSP推断路径，只把已有versioned diagnostics契约变为可重复性能门禁。

## 验证

- 10个独立MSVC stdio进程均真实exit 0；最大的diagnostics p50/p95/p99为3.02/10.23/29.56ms。
- 精确MSVC CTest language_server_stdio_smoke为1/1、真实exit 0。

## 未完成边界

- 100-file reference workspace、峰值服务端内存、cache/LRU、跨平台可比性能和完整snapshot stress仍未完成。
