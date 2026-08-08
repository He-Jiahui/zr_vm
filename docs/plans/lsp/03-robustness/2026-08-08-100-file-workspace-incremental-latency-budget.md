---
related_code:
  - tests/language_server/stdio_smoke.js
implementation_files:
  - tests/language_server/stdio_smoke.js
tests:
  - tests/language_server/stdio_smoke.js
  - tests/acceptance/2026-08-08-lsp-l6-100-file-workspace-incremental-latency-budget.md
doc_type: milestone-detail
plan_id: lsp-03-robustness
record_id: 2026-08-08-100-file-workspace-incremental-latency-budget
status: completed
completed_at: 2026-08-08 17:56 +08:00
---

# 100 File Workspace Incremental Latency Budget

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-08 17:56 +08:00 | 已完成 | 100个source项目中单文件versioned trivia edit到同URI/version publishDiagnostics的20样本p50/p95/p99测量与500ms p95门禁。 |

## 已实现契约

- smoke在临时.zrp项目中创建100个source：target模块显式导入99个helper模块，故目标语义绑定依赖完整项目图。
- workspace/didChangeWatchedFiles先索引项目，workspace/symbol必须返回target的workspace_latency_target，随后才开始计时。
- target以v1打开，连续20次发送v2..v21等长trivia edit；每次仅在收到相同URI和version的空publishDiagnostics后记录样本。
- p95超过500ms时stdio smoke失败并打印p50/p95/p99；该检查不以request latency或目录文件计数代替project解析、依赖绑定和diagnostics发布。

## 验证

- 10个独立MSVC stdio进程均真实exit 0；最大的100-file增量diagnostics p50/p95/p99为55.63/141.18/349.43ms。
- 精确MSVC CTest language_server_stdio_smoke为1/1、真实exit 0。

## 未完成边界

- 此验收不覆盖峰值服务端内存、current加两个historical snapshot上限、256MiB workspace cache/LRU、GCC/Clang可比性能或完整rapid edit/cancel/close snapshot stress。
