---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/lsp_semantic_snapshot.h
  - zr_vm_language_server/src/zr_vm_language_server/snapshot/lsp_semantic_snapshot.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_semantic_tokens.c
  - zr_vm_language_server/stdio/stdio_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_workspace_edit_snapshot.c
implementation_files:
  - zr_vm_language_server/include/zr_vm_language_server/lsp_semantic_snapshot.h
  - zr_vm_language_server/src/zr_vm_language_server/snapshot/lsp_semantic_snapshot.c
  - zr_vm_language_server/stdio/stdio_requests.c
plan_sources:
  - docs/plans/lsp/optimize/02-snapshots-workspaces-and-diagnostics.md
tests:
  - tests/language_server/test_lsp_semantic_snapshot.c
  - tests/language_server/stdio_smoke.js
  - tests/language_server/stdio_inline_value_semantic_smoke.js
  - tests/language_server/stdio_diagnostic_fix_smoke.js
  - tests/language_server/stdio_workspace_folders_smoke.js
doc_type: milestone-record
---

# Plan 02 Task 3: Semantic Snapshot Dependency Fence

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
| --- | --- | --- | --- |
| 2026-08-23 11:18 +08:00 | 已完成 | 发布不可变语义 snapshot identity、实际读取依赖 fence、请求发布前验证，以及 semantic token、诊断和 workspace edit 的共享结果身份。 | GCC Debug shared: snapshot 28/28、完整 LSP interface suite、完整 stdio smoke、inline-value、diagnostic-fix 和 workspace-folders smoke 均真实 exit 0。 |

## Delivered Contract

- `SZrLspSemanticSnapshotIdentity` 固定 document、project、provider、semantic
  generation 与 dependency fingerprint。Acquire 通过内容块引用固定 URI 和文本，
  同时捕获当前 AST、last-good 标志、analyzer、project/provider view；公共 API
  不暴露可变 `SZrFileVersion *`。
- snapshot 在 acquisition 时记录项目 import 依赖，并在 active request scope 中由
  跨文档 analyzer 查找追加实际读取的 URI。未读取文档变化保持有效；源文档、已读取
  依赖、provider 或 project view 变化均会使 Validate 失败。
- 所有带 `textDocument.uri` 的 stdio request 在 dispatch 前获取 snapshot，并在
  写出响应前验证。已取消请求先返回 `-32800`，未取消但 fence 失效的请求返回
  `-32801 Content modified`，不发布部分结果。
- semantic token `resultId`、diagnostic `resultId` 和 workspace-edit action snapshot
  统一使用 `zr-snapshot:<dependencyFingerprint>:<payloadLength>`。workspace edit
  在动作带有 identity 时按同一 snapshot 验证；旧的独立 token/diagnostic hash 已移除。
- delta smoke 的 stale resultId 改为合法但不存在的 `zr-snapshot` identity，继续验证
  身份不匹配时的全量替换；不再保留旧 `zr-semantic` 格式兼容路径。
- Workspace symbol 枚举仅保留仍由 project index 覆盖或明确打开的 overlay。一个共享
  source path 在第一个 project 删除后仍可查询，最终引用 project 删除后不再泄漏旧 analyzer
  的 symbols。

## Validation

```text
.codex/build-lsp-snapshot-gcc/bin/zr_vm_language_server_lsp_semantic_snapshot_test
.codex/build-lsp-snapshot-gcc/bin/zr_vm_language_server_lsp_interface_test
node tests/language_server/stdio_smoke.js .codex/build-lsp-snapshot-gcc/bin/zr_vm_language_server_stdio
node tests/language_server/stdio_inline_value_semantic_smoke.js .codex/build-lsp-snapshot-gcc/bin/zr_vm_language_server_stdio
node tests/language_server/stdio_diagnostic_fix_smoke.js .codex/build-lsp-snapshot-gcc/bin/zr_vm_language_server_stdio
node tests/language_server/stdio_workspace_folders_smoke.js .codex/build-lsp-snapshot-gcc/bin/zr_vm_language_server_stdio
```

- Snapshot unit test: 28/28 pass, including unrelated update, tracked dependency
  invalidation, provider invalidation, owner invalidation, active scope, and shared
  result identity.
- Full LSP interface suite passed with exit 0.
- Full stdio smoke passed with latency and memory budgets reported; the three focused
  stdio smoke commands also exited 0.
- `language_server_stdio_protocol_inventory` CTest passed. The wider GCC/Clang/MSVC
  URI, differential-edit, multi-root diagnostic, and performance gates remain owned
  by Plan 02 Task 7 and are not represented as completed here.

## Scope Boundary

This record completes only Plan 02 Task 3. Task 4 document synchronization,
Task 5 incremental parsing, Task 6 diagnostic aggregation, and Task 7 acceptance
remain separate milestones.
