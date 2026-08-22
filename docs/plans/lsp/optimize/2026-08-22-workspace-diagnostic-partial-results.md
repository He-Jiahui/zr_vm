---
related_code:
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_diagnostics.c
implementation_files:
  - zr_vm_language_server/stdio/stdio_requests.c
plan_sources:
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
  - user: optimize LSP semantic inference according to the documented milestones
tests:
  - tests/language_server/stdio_protocol_conformance.js
doc_type: milestone-detail
---

# LSP Workspace Diagnostic Partial Results

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
| --- | --- | --- |
| 2026-08-22 22:44 +08:00 | completed | `workspace/diagnostic` 接入 report-shaped partial sink；每个精确 token 的 `$/progress.value` 保持 `{ "items": [...] }`，最终 JSON-RPC response 为 `null`。 |

## Contract

- Workspace diagnostics retain the LSP `WorkspaceDiagnosticReportPartialResult`
  envelope. They never use the direct-array schema used by symbols, references,
  and hierarchy traversals.
- The shared bounded batch sender preserves `items`, observes the active request
  cancellation callback before every batch, and only consumes the ordinary
  response when a valid `partialResultToken` is present.
- Requests without a partial token retain the original workspace diagnostic
  report object. ContentModified remains a dependency-fence concern owned by
  plan 02.

## Evidence

The isolated GCC Debug stdio target rebuilt with exit code `0`.
`stdio_protocol_conformance.js` passed `28/28`. The regression fixture opens a
document, requests `workspace/diagnostic` with a token, receives the matching
report URI under `$/progress.value.items`, and receives a final JSON `null`
result.

## Remaining Work

Task 4 partial-result handling now covers workspace symbols, references, call
hierarchy, type hierarchy, and workspace diagnostics. The next Task 4 action is
the consolidated focused regression and record audit before deterministic
teardown begins.
