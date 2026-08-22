---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/include/zr_vm_language_server/lsp_interface.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/stdio/stdio_request_registry.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_transport.c
  - zr_vm_language_server/stdio/stdio_diagnostics.c
implementation_files:
  - zr_vm_language_server/stdio/stdio_request_registry.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_transport.c
plan_sources:
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
  - user: optimize LSP semantic inference according to the documented milestones
tests:
  - tests/language_server/stdio_protocol_conformance.js
  - tests/language_server/stdio_smoke.js
  - tests/language_server/stdio_type_hierarchy_smoke.js
  - tests/language_server/test_lsp_interface.c
doc_type: milestone-detail
---

# LSP Protocol Task 4 Request Progress And Cancellation

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
| --- | --- | --- |
| 2026-08-22 22:49 +08:00 | completed | 完成 typed JSON-RPC request-id registry、精确取消、`$/setTrace`、request-scoped work-done/partial-result progress；workspace symbols、references、call/type hierarchy 与 workspace diagnostics 均通过各自的 canonical result schema 输出 partial batches。 |

## Delivered Contract

- Request identities distinguish number and string values. Active duplicates are
  rejected as `-32600`; `$/cancelRequest` marks only its exact active id and an
  unknown id has no response.
- Long-running LSP queries receive a request-scoped cancellation callback. The
  serial dispatcher installs it before dispatch and clears it on every return
  path. Cancellation reports `-32800`; this task does not fabricate
  `-32801 ContentModified` from a global input generation.
- Valid string or exactly representable integer progress tokens are parsed once
  per request. Work-done progress emits framed `begin` and `end` notifications
  on stderr-clean stdout protocol transport.
- Partial results preserve their response schemas: direct arrays for workspace
  symbols, references, call hierarchy and type hierarchy; `{items:[...]}` for
  workspace diagnostics. Batches are capped by
  `ZR_LSP_PARTIAL_RESULT_BATCH_SIZE`, recheck cancellation before each send,
  and replace the final ordinary result with JSON `null` only when a partial
  token is present.
- `$/setTrace` keeps protocol trace records on stderr. Core query execution
  remains serial; this task does not introduce a mutable cross-thread semantic
  snapshot.

## Evidence

Validation used the isolated GCC Debug overlay
`/home/hejiahui/.codex-snapshots/lsp-json-rpc-envelope-792a6b0` with the
matching build directory
`/home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc`.

- `zr_vm_language_server_lsp_interface_test` rebuilt and exited `0`.
- CTest cases 18 through 24 passed `7/7`: standard stdio smoke, inline-value
  semantic smoke, diagnostic-fix smoke, position encoding smoke, type
  hierarchy smoke, protocol inventory, and protocol conformance.
- The protocol conformance driver passed `28/28`, including all five partial
  result families and invalid progress-token validation.
- The interface target emitted one pre-existing missing-initializer warning in
  `tests/language_server/test_lsp_interface.c` for external `providerPhase`;
  it is outside this Task 4 write set and did not affect the zero exit result.

## Boundary

Task 5 remains incomplete. `zr_vm_language_server_stdio.c` still documents
process-exit reclamation and `stdio_transport.c` still detaches the reader;
the next milestone must replace that workaround with stop, join, drain, and
ordered runtime teardown. Plan 02 continues to own the dependency fence that
can introduce a precise ContentModified response.
