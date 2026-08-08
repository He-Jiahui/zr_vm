# LSP L6 Rapid Stdio Stale-Response Churn Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-08 19:29 +08:00 | 已完成 | 100轮独立URI的取消、变更与关闭交错均拒绝旧workspace diagnostic响应，并完成版本诊断和关闭清理。 |

## Evidence

- 每轮双`$/cancelRequest`都返回`-32800`，没有workspace diagnostic success。
- 每轮`didChange`和`didClose`前创建的workspace request都返回`-32801`。
- 每轮严格等待replacement version diagnostics和close后的空diagnostics，覆盖100次打开、变更和关闭生命周期。
- MSVC direct stdio smoke与CTest `language_server_stdio_smoke`均真实exit 0；CTest 1/1通过，耗时6.80秒。

## Open Scope

- 不覆盖semantic snapshot历史或释放策略。
- 不覆盖workspace cache 256MiB LRU、peak memory report或GCC/Clang可比验证。
