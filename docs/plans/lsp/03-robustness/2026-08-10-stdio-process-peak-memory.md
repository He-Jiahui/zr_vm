---
plan_id: lsp-03-robustness
record_id: 2026-08-10-stdio-process-peak-memory
status: completed
completed_at: 2026-08-10 00:57 +08:00
related_code:
  - tests/language_server/stdio_smoke.js
tests:
  - tests/language_server/stdio_smoke.js
  - tests/acceptance/2026-08-10-lsp-l6-stdio-process-peak-memory.md
doc_type: milestone-detail
---

# Stdio Process Peak Memory

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-10 00:57 +08:00 | 已完成 | stdio smoke 在完整协议负载完成后、进程退出前读取服务端 OS process high-water，并以默认 512MiB 预算失败门禁；rapid churn 同时只接受 lifecycle error 或精确线性化快照。 |

## 已实现契约

- `ZR_LSP_STDIO_PEAK_MEMORY_LIMIT_BYTES` 缺省为 `536870912`。显式值必须是
  正的 safe integer；越界使 smoke 失败，不能静默跳过预算。
- Linux 使用服务端 PID 的 `/proc/<pid>/status` `VmHWM`；Windows 使用同一
  PID 的 `PeakWorkingSet64`。不支持的 host platform fail closed。
- 采集发生在 `shutdown` 响应之后、`exit` 通知之前，因此覆盖启动到完整
  stdio workload 的 child-process lifetime。输出同时包含 bytes 与 MiB。
- 100 次 open/cancel/change/close churn 不再依赖 reader/main-thread 的偶然
  调度。若 reader 先观察到 lifecycle notification，请求必须得到
  `RequestCancelled` 或 `ContentModified`；若请求先线性化，成功 report 必须
  包含该请求精确 document version。任何混合或较新 snapshot 都失败。

## 验证

- RED：Clang 完整 stdio workload 使用 1-byte budget 真实 exit 1，报告
  `VmHWM=31,723,520` bytes 并命中峰值预算断言；warm、diagnostics 和
  100-file latency assertions 在该门禁前已通过。
- GREEN：GCC 真实 exit 0，`VmHWM=33,062,912` bytes（31.53MiB）。
- GREEN：Clang 真实 exit 0，`VmHWM=30,969,856` bytes（29.54MiB）。
- GREEN：MSVC 真实 exit 0，`PeakWorkingSet64=38,301,696` bytes（36.53MiB）。
- 三套 GREEN 均运行完整 `stdio_smoke.js`，并继续满足 hover/completion/
  signature、single-document diagnostics 与 100-file incremental latency
  budgets。

## 未完成边界

- 此 leaf 只约束 native stdio language-server child process，不估算完整
  workspace、extension host 或 allocator ownership graph。
- `SZrAnalysisCache` 的 256MiB LRU 是独立 exact-storage contract，不能
  伪称为 process RSS。
- L6 的完整最终 stdio/CLI matrix 继续单独验收。
