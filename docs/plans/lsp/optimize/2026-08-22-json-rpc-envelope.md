<!--
related_code:
  - zr_vm_language_server/stdio/stdio_json_rpc.h
  - zr_vm_language_server/stdio/stdio_json_rpc.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_lsp_parse.c
related_plans:
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
related_tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_smoke
  - language_server_stdio_position_encoding_smoke
  - language_server_stdio_type_hierarchy_smoke
  - language_server_stdio_protocol_inventory
-->

# LSP JSON-RPC Envelope And Params

## Scope

This record completes Task 2 of
[`01-protocol-lifecycle-and-transport.md`](./01-protocol-lifecycle-and-transport.md).
It validates each parsed native stdio message before lifecycle or request
dispatch, centralizes error classification, and rejects lossy LSP position and
range values.

## 状态与产出记录

| Time | Status | Completed items |
| --- | --- | --- |
| 2026-08-22 20:48 +08:00 | completed | Added a single JSON-RPC 2.0 envelope parser and explicit request-dispatch status. Top-level arrays/scalars, missing or wrong `jsonrpc`, and boolean/object/array ids return `-32600`; present scalar or null `params` return `-32602`. Malformed notifications log only to stderr and never emit a response. Position and size values now reject fractional, non-finite, negative, and out-of-range values; ranges reject an end before start. Valid empty semantic results remain JSON `null` or empty arrays instead of C `NULL` error sentinels. |
| 2026-08-23 03:54 +08:00 | completed | Revalidated the shared envelope and parameter boundary through the final 29-case protocol driver on GCC, Clang ASan+UBSan, and MSVC Debug. Error envelopes, stderr-only malformed notifications, numeric bounds, and legal empty results remain unchanged. |

## Contract

- A message must be a JSON object with `jsonrpc: "2.0"` and a string method.
- A request id is a string, number, or JSON null. Boolean, object, and array
  ids produce `InvalidRequest` with response id `null`.
- `params` may be absent, an object, or an array. A present scalar or JSON
  null is `InvalidParams`; malformed notifications only write an stderr
  diagnostic and do not contaminate stdout frames.
- A valid message without an id is a JSON-RPC notification. The plan's
  “request missing id” wording cannot be applied without breaking standard
  notifications, which are defined by the absence of an id; malformed no-id
  envelopes still return `InvalidRequest` where JSON-RPC requires a response.
- `EZrLspHandlerStatus` now carries result classification across the stdio
  dispatch boundary. A valid no-result response is represented by a cJSON
  null node, while malformed input maps to `InvalidParams`.
- `parse_position`, `parse_range`, and `parse_size_value` refuse truncation,
  sign changes, non-finite values, numeric overflow, and reverse ranges.

## Evidence

Validation used the isolated source overlay at
`/home/hejiahui/.codex-snapshots/lsp-json-rpc-envelope-792a6b0` and the
matching GCC Debug build directory
`/home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc`.

```text
cmake -S /home/hejiahui/.codex-snapshots/lsp-json-rpc-envelope-792a6b0 \
  -B /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIB=ON -DBUILD_STATIC_LIB=OFF -DBUILD_WASM=OFF
cmake --build /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --target zr_vm_language_server_stdio zr_vm_cli_executable \
  zr_vm_descriptor_plugin_fixture_int --parallel 8
ctest --test-dir /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --output-on-failure -R '^language_server_stdio_smoke$'
ctest --test-dir /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --output-on-failure -R '^language_server_stdio_(position_encoding_smoke|type_hierarchy_smoke|protocol_inventory)$'
```

Results:

- GCC configuration, native stdio link, CLI, and descriptor fixture targets
  exited `0`.
- `language_server_stdio_smoke` passed `1/1` with an empty stderr stream.
- Position encoding, type hierarchy, and protocol inventory smokes passed
  `3/3`.
- The protocol conformance driver has `18` passing cases. Its process exits
  `1` only for the one deliberate remaining Task 4 RED:
  duplicate active JSON-RPC request ids still yield two success responses.
  All Task 2 envelope, parameter, position, and range cases pass.
- `node --check` passed for every changed Node.js client/driver, and
  `git diff --check` found no whitespace error in the exact write set.

## Remaining Work

Task 3 owns bounded frame parsing. Task 4 owns the request registry, duplicate
id rejection, and cancellation. This task does not suppress or whitelist the
remaining duplicate-id conformance failure.
