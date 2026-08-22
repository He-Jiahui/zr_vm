<!--
related_code:
  - tests/language_server/stdio_protocol_client.js
  - tests/language_server/stdio_protocol_conformance.js
  - tests/language_server/stdio_smoke.js
  - tests/CMakeLists.txt
related_plans:
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
related_tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_smoke
-->

# LSP Protocol Conformance RED Driver

## Scope

This record completes Task 3 of
[`00-baseline-and-contract.md`](./00-baseline-and-contract.md): a reusable
Node.js stdio client, a CTest-registered JSON-RPC/LSP negative conformance
driver, and extraction of the frame/request/timeout/stderr mechanics from the
existing smoke test. It does not change stdio server production code.

## 状态与产出记录

| Time | Status | Completed items |
| --- | --- | --- |
| 2026-08-22 20:08 +08:00 | completed (RED contract driver) | Added `StdioProtocolClient` with frame encoding/decoding, explicit request ids, auto ids, response and notification waiters, timeouts, exit handling, and stderr capture. Replaced the duplicate `stdio_smoke.js` client implementation with a compatibility adapter. Added 16 LSP 3.17 protocol conformance cases and CTest registration. Captured eight implemented cases and eight server gaps with exact JSON-RPC envelopes and error-code expectations. |

## Contract Coverage

The driver verifies the complete success or error envelope, including
`jsonrpc`, exact `id`, absence of `result` on errors, and the exact JSON-RPC
error code. The frozen 3.17 capability snapshot has 33 top-level capability
keys.

The isolated current behavior has eight passing contract cases:

- 3.17 capability matrix.
- `exit` before `shutdown` exits with code `1`.
- Unknown request method returns `-32601`.
- Unknown notification has no response.
- Malformed notification params have no response.
- Malformed JSON payload returns `-32700` with `id: null`.
- Cancelling an unknown id has no response.
- A `16777217` byte `Content-Length` terminates the server with a non-zero
  exit code.

The following eight RED cases are intentional, reproducible implementation
gaps owned by
[`01-protocol-lifecycle-and-transport.md`](./01-protocol-lifecycle-and-transport.md):

| Case | Expected | Actual | Owner |
| --- | --- | --- | --- |
| Request before `initialize` | `-32002` | Returned a hover result | Task 1 lifecycle |
| Repeated `initialize` | `-32600` | Returned a second initialize result | Task 1 lifecycle |
| Request after `shutdown` | `-32600` | Returned an empty workspace-symbol result | Task 1 lifecycle |
| Missing `jsonrpc` | `-32600` | Accepted initialize | Task 2 envelope |
| `jsonrpc: "1.0"` | `-32600` | Accepted initialize | Task 2 envelope |
| Boolean request id | `id: null`, `-32600` | Accepted a boolean id | Task 2 envelope |
| Scalar initialize params | `-32602` | Accepted initialize | Task 2 params |
| Duplicate active request id | One `-32600` response | Returned two successes | Task 4 request registry |

The conformance driver remains red until those server-side tasks are complete;
this completed milestone establishes the executable boundary and does not
accept the server behavior.

## Evidence

The test source was overlaid on a fresh WSL-local `f35b9cc` source snapshot
with the already-validated Task 2 registry source overlays. The build directory
was `/home/hejiahui/.codex-builds/lsp-capability-registry-gcc`.

```text
cmake -S /home/hejiahui/.codex-snapshots/lsp-capability-registry-f35b9cc \
  -B /home/hejiahui/.codex-builds/lsp-capability-registry-gcc \
  -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIB=ON -DBUILD_STATIC_LIB=OFF
cmake --build /home/hejiahui/.codex-builds/lsp-capability-registry-gcc \
  --target zr_vm_cli_executable zr_vm_descriptor_plugin_fixture_int \
  zr_vm_language_server_stdio --parallel 8
ctest --test-dir /home/hejiahui/.codex-builds/lsp-capability-registry-gcc \
  --output-on-failure -R '^language_server_stdio_protocol_conformance$'
ctest --test-dir /home/hejiahui/.codex-builds/lsp-capability-registry-gcc \
  --output-on-failure -R '^language_server_stdio_smoke$'
```

Observed results:

- `language_server_stdio_protocol_conformance`: exit `1`, `8` passes and `8`
  named expected RED failures listed above.
- `language_server_stdio_smoke`: exit `0`, `1/1` passed after its client was
  migrated to the shared implementation.
- `node --check` passed for the shared client, conformance driver, and smoke
  adapter.
- `git diff --check` produced no whitespace error.

## Follow-up

Task 1 of
[`01-protocol-lifecycle-and-transport.md`](./01-protocol-lifecycle-and-transport.md)
owns the lifecycle failures. Task 2 owns JSON-RPC envelope and parameter
validation. Task 4 owns the duplicate request id registry. Task 3 should add
the remaining malformed-header and truncation frame cases to this driver when
the frame reader abstraction exists.
