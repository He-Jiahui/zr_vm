---
related_code:
  - tests/language_server/stdio_smoke.js
implementation_files:
  - tests/language_server/stdio_smoke.js
tests:
  - tests/language_server/stdio_smoke.js
  - tests/acceptance/2026-08-08-lsp-l6-rapid-stdio-stale-response-churn.md
doc_type: milestone-detail
plan_id: lsp-03-robustness
record_id: 2026-08-08-rapid-stdio-stale-response-churn
status: completed
completed_at: 2026-08-08 19:29 +08:00
---

# Rapid Stdio Stale-Response Churn

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-08 19:29 +08:00 | 已完成 | 独立文档URI完成100轮取消、变更与关闭交错，每一轮均拒绝旧workspace diagnostic响应。 |

## 已实现契约

- 每轮以严格递增的document version打开独立URI，并等待该URI/version的`publishDiagnostics`，使每次请求都有可验证的输入快照。
- 两个乱序`workspace/diagnostic`请求在`$/cancelRequest`后都只返回`RequestCancelled` (`-32800`)；测试不接受成功结果。
- `didChange`和`didClose`均紧随一个workspace请求。各自的旧请求只能返回`ContentModified` (`-32801`)；变更后必须发布精确replacement version，关闭后必须发布空diagnostics。
- 100轮顺序化等待所有响应和通知，避免测试客户端的notification backlog掩盖服务端跨generation泄漏。

## 验证

- GREEN：MSVC stdio executable直接运行`tests/language_server/stdio_smoke.js`真实exit 0；warm、diagnostics和100-file现有门禁均继续输出通过数据。
- GREEN：`ctest --test-dir .codex/build-e5-closure-msvc -R '^language_server_stdio_smoke$' --output-on-failure`真实exit 0，1/1通过，耗时6.80秒。

## 未完成边界

- 此leaf不保留、淘汰或计量semantic snapshot。
- workspace semantic cache的256MiB LRU、peak memory report和GCC/Clang可比验证仍未完成。
