# LSP L6 Single Document Diagnostics Latency Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-08 17:47 +08:00 | 已完成 | 20个versioned trivia edit 的didChange到publishDiagnostics p95 <= 250ms。 |

## Evidence

- 10个独立MSVC stdio进程的最差p95为10.23ms，p99为29.56ms。
- CTest stdio smoke 1/1、真实exit 0。

## Open Scope

- 此验收不覆盖100-file增量、峰值内存、LRU或跨平台性能。
