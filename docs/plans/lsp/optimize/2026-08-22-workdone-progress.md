<!--
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
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

# LSP Request Work-Done Progress

## Scope

This record completes the first request-progress sub-milestone of Task 4 in
[`01-protocol-lifecycle-and-transport.md`](./01-protocol-lifecycle-and-transport.md).
It establishes the request-scoped token lifecycle and the unified work-done
notification sink. Partial-result batching is intentionally separate because
it changes each handler's result contract.

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
| --- | --- | --- |
| 2026-08-22 22:10 +08:00 | completed | 解析并校验 long-running request 的 `workDoneToken` 与 `partialResultToken`，为已支持的 long query 通过统一 framed sink 发出 `$/progress` begin/end；保留正常 JSON-RPC 响应并拒绝非法 progress token。 |

## Contract

- The request context borrows JSON token nodes only for the current envelope
  and clears them on every dispatcher return path. It never stores them in a
  global queue or past message destruction.
- A progress token is an LSP string or an exactly representable integer. A
  supplied object, boolean, fraction, or out-of-range numeric token produces
  `-32602` before the handler runs.
- The work-done sink is active for workspace symbol, references, workspace
  diagnostics, rename, and call/type hierarchy traversal requests. It emits a
  framed `$/progress` `begin` followed by `end`, then leaves the ordinary
  request result unchanged.
- `partialResultToken` is parsed and retained in the same context but emits no
  result data in this sub-milestone. The next Task 4 sub-milestone will add
  per-method partial batches and checks before each batch.
- The request dispatcher remains serial and continues to use the exact
  request-id cancellation registry. This change does not introduce a global
  generation fence or `-32801`.

## Evidence

Validation used the isolated source overlay at
`/home/hejiahui/.codex-snapshots/lsp-json-rpc-envelope-792a6b0` and matching
GCC Debug build directory
`/home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc`.

```text
cmake --build /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --target zr_vm_language_server_stdio zr_vm_language_server_lsp_interface_test --parallel 8
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

- GCC Debug stdio and interface targets built with exit code `0`.
- The direct protocol driver passed `23/23`, including string and numeric
  work-done tokens, an accepted partial-result token, and rejection of an
  invalid partial-result token without a progress notification.
- The direct LSP interface test exited `0`; protocol conformance, stdio smoke,
  position encoding smoke, and type hierarchy smoke each passed `1/1`.
- `node --check` passed for the modified protocol driver.

## Remaining Work

Task 4 still needs method-specific `partialResultToken` result batches, with
the same cancellation and future dependency-fence check before every batch.
Task 5 owns deterministic reader and runtime teardown. Plan 02 Task 3 owns
the dependency fence that may introduce a precise `-32801` response.
