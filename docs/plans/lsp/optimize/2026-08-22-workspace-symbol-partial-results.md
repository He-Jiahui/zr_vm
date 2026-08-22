<!--
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/stdio/stdio_requests.c
related_plans:
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
related_tests:
  - tests/language_server/stdio_protocol_conformance.js
  - zr_vm_language_server_lsp_interface_test
  - language_server_stdio_protocol_conformance
  - language_server_stdio_smoke
  - language_server_stdio_position_encoding_smoke
  - language_server_stdio_type_hierarchy_smoke
-->

# LSP Workspace Symbol Partial Results

## Scope

This record completes the first method-specific partial-result sub-milestone
of Task 4 in
[`01-protocol-lifecycle-and-transport.md`](./01-protocol-lifecycle-and-transport.md).
It publishes `workspace/symbol` result arrays through the request-scoped
`partialResultToken` rather than duplicating the same data in the final
JSON-RPC response.

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
| --- | --- | --- |
| 2026-08-22 22:20 +08:00 | completed | 完成 `workspace/symbol` 的 bounded `partialResultToken` batches：每批最多 64 项、每批前检查精确 cancellation，并在 batches 后返回 JSON-RPC `null`，避免结果双发。 |

## Contract

- Only `workspace/symbol` enters this array-only sink. Other long-running
  methods retain their direct result schema until their own partial-result
  contracts are implemented.
- A supplied `partialResultToken` causes the workspace symbol result array to
  be deep-copied into ordered `$/progress` notification values. Every batch
  preserves the exact string or integer token from the request.
- The batch limit is `ZR_LSP_PARTIAL_RESULT_BATCH_SIZE` (`64`). Cancellation is
  checked before each batch through the active request callback. A cancelled
  request follows the existing exact `-32800` registry response path.
- When all batches are sent, the owned result array is replaced with JSON
  `null`; a missing partial token leaves the existing response unchanged.
- This sub-milestone does not create a global content-generation fence or a
  `-32801` response. That remains Plan 02 work.

## Evidence

Validation used the isolated source overlay at
`/home/hejiahui/.codex-snapshots/lsp-json-rpc-envelope-792a6b0` and matching
GCC Debug build directory
`/home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc`.

```text
cmake --build /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --target zr_vm_language_server_stdio --parallel 8
node /home/hejiahui/.codex-snapshots/lsp-json-rpc-envelope-792a6b0/tests/language_server/stdio_protocol_conformance.js \
  /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc/bin/zr_vm_language_server_stdio
node --check tests/language_server/stdio_protocol_conformance.js
ctest --test-dir /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --output-on-failure -R '^language_server_stdio_protocol_conformance$'
ctest --test-dir /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --output-on-failure -R '^language_server_stdio_smoke$'
ctest --test-dir /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --output-on-failure -R '^language_server_stdio_position_encoding_smoke$'
ctest --test-dir /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --output-on-failure -R '^language_server_stdio_type_hierarchy_smoke$'
```

Results:

- The GCC Debug stdio target built with exit code `0`; the direct interface
  test also exited `0`.
- The protocol driver passed `24/24`. Its partial-result case opens 65 source
  symbols, receives progress batches of exactly 64 and 1 entries under numeric
  token `17`, and receives a final JSON-RPC `null` result.
- Protocol conformance, stdio smoke, position encoding smoke, and type
  hierarchy smoke each passed `1/1`. `node --check` also passed.

## Remaining Work

Task 4 still needs partial-result adapters for references, hierarchy, and the
diagnostic-specific report schema, each with the same cancellation boundary.
Task 5 owns deterministic reader and runtime teardown. Plan 02 Task 3 owns
the dependency fence that may introduce a precise `-32801` response.
