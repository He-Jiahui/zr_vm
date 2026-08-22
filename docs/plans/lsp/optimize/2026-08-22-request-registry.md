<!--
related_code:
  - zr_vm_language_server/stdio/stdio_request_registry.h
  - zr_vm_language_server/stdio/stdio_request_registry.c
  - zr_vm_language_server/stdio/stdio_transport.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_diagnostics.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio.c
related_plans:
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
  - docs/plans/lsp/optimize/02-snapshots-workspaces-and-diagnostics.md
related_tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_smoke
  - language_server_stdio_position_encoding_smoke
  - language_server_stdio_type_hierarchy_smoke
  - language_server_stdio_protocol_inventory
-->

# LSP Typed Request Registry

## Scope

This record completes the request-registry sub-milestone of Task 4 in
[`01-protocol-lifecycle-and-transport.md`](./01-protocol-lifecycle-and-transport.md).
It makes reader-accepted JSON-RPC request identities explicit, preserves
precise cancellation across the reader/main-thread boundary, and removes the
global document-generation approximation for `ContentModified`.

## 状态与产出记录

| Time | Status | Completed items |
| --- | --- | --- |
| 2026-08-22 21:18 +08:00 | completed | Added a thread-safe request registry keyed by JSON-RPC id kind and exact value. Reader-side reserve rejects a duplicate queued or active id with `-32600` without dispatching it, while numeric `1` and string `"1"` remain distinct. `$/cancelRequest` marks only the matching registered request and unknown ids remain silent no-ops. Removed all stdio global `inputGeneration` checks and the resulting speculative `-32801` responses; workspace diagnostics now retain only exact cancellation until the plan 02 dependency fence is available. |

## Contract

- Registry entries distinguish JSON null, number, and string identities. A
  matching request retains its entry from reader-side reserve through main
  dispatch completion, so it cannot reuse a cancellation node.
- A duplicate entry is carried to the main thread as a classified inbound
  state. The main thread emits `InvalidRequest` and does not invoke the
  handler or complete the original entry.
- `$/cancelRequest` is consumed by the reader only for a valid notification
  and marks the exact structured identity. It never writes a response.
- Before plan 02 supplies immutable snapshots and a dependency fingerprint,
  document changes do not produce `ContentModified`. Requests either complete
  from the serial service state or report an exact cancellation.

## Evidence

Validation used the isolated source overlay at
`/home/hejiahui/.codex-snapshots/lsp-json-rpc-envelope-792a6b0` and the
matching GCC Debug build directory
`/home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc`.

```text
cmake --build /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --target zr_vm_language_server_stdio --parallel 8
node tests/language_server/stdio_protocol_conformance.js \
  /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc/bin/zr_vm_language_server_stdio
ctest --test-dir /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --output-on-failure -R language_server_stdio_smoke
ctest --test-dir /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --output-on-failure -R language_server_stdio_position_encoding_smoke
ctest --test-dir /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --output-on-failure -R language_server_stdio_type_hierarchy_smoke
ctest --test-dir /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --output-on-failure -R language_server_stdio_protocol_inventory
```

Results:

- GCC native stdio build exited `0`.
- The protocol conformance driver passed `21/21`, including duplicate queued
  ids, distinct typed ids, unknown cancellation, envelope validation, and
  bounded frame failures.
- The native stdio smoke, position encoding smoke, type hierarchy smoke, and
  protocol inventory smoke each passed `1/1`.
- `node --check` passed for the changed conformance and smoke drivers.

## Remaining Work

Task 4 still owns request contexts, work-done/partial-result progress, uniform
cancellation callbacks across all long-running handlers, and `$/setTrace`.
Task 5 owns deterministic thread and runtime teardown. Plan 02 Task 3 owns
the dependency fence that may reintroduce a precise `-32801` response.
