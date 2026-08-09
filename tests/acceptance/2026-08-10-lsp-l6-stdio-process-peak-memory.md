# LSP L6 Stdio Process Peak Memory Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-10 00:57 +08:00 | 已完成 | 验证 stdio language-server child 的 OS peak working-set high-water、512MiB default budget 和 100 次 lifecycle churn 的精确 snapshot outcome。 |

## Evidence

- Clang 以 `ZR_LSP_STDIO_PEAK_MEMORY_LIMIT_BYTES=1` 运行完整 smoke，真实
  exit 1，报告 31,723,520 bytes，并由 peak-budget assertion 拒绝。
- 默认 512MiB budget 的完整 smoke 真实 exit 0：GCC 33,062,912 bytes，
  Clang 30,969,856 bytes，MSVC 38,301,696 bytes。
- Linux 读取 `VmHWM`，Windows 读取 `PeakWorkingSet64`；测量发生在
  `shutdown` 后、`exit` 前，输出含 bytes 与 MiB。
- rapid churn 对 cancellation/change/close 只接受 lifecycle error 或
  exact request-version workspace report，仍严格等待 replacement diagnostics
  与 close empty diagnostics。

## Open Scope

- 此证据不替代完整 L6 stdio/CLI matrix。
- process high-water 与 context exact cache-storage accounting 是不同度量。
