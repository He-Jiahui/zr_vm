# LSP L6 Warm Request Latency Budget Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-08 17:40 +08:00 | 已完成 | warm hover p95 <= 50ms，completion/signatureHelp p95 <= 100ms，且每类输出p50/p95/p99。 |

## Acceptance Scope

- 在已打开且已经完成结果正确性断言的document v1上，对hover、completion和signatureHelp各连续采样20次。
- hover使用native descriptor callable；completion和signatureHelp使用closed generic source事实，避免把空或fallback response作为性能样本。
- 每个样本覆盖stdio request/response往返；CI只对p95执行计划阈值门禁，同时打印p50/p95/p99。

## Evidence

- 10个独立MSVC stdio进程均通过。最大观察p95：hover 20.07ms，completion 15.16ms，signatureHelp 4.17ms。
- 精确MSVC CTest smoke 1/1、真实exit 0。

## Decision

- 接受warm request latency budget leaf。
- diagnostics、100-file workspace、峰值内存、cache/LRU和跨平台可比性能仍不通过本记录验收。
