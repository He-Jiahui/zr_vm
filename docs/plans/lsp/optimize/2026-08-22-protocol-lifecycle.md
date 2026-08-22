<!--
related_code:
  - zr_vm_language_server/stdio/stdio_lifecycle.h
  - zr_vm_language_server/stdio/stdio_lifecycle.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
  - zr_vm_language_server/CMakeLists.txt
related_plans:
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
related_tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_smoke
-->

# LSP Stdio Lifecycle State Machine

## Scope

This record completes Task 1 of
[`01-protocol-lifecycle-and-transport.md`](./01-protocol-lifecycle-and-transport.md).
It replaces the implicit `shutdownRequested` boolean with a dedicated lifecycle
state object and routes stdio requests and notifications through that object.

## 状态与产出记录

| Time | Status | Completed items |
| --- | --- | --- |
| 2026-08-22 20:31 +08:00 | completed | Added the `NEW -> INITIALIZING -> RUNNING -> SHUTDOWN -> EXITED` lifecycle state machine. Requests before initialize return `-32002`; a second initialize and requests after shutdown return `-32600`. `initialized` only promotes `INITIALIZING` to `RUNNING`, while legal requests remain available after the initialize response. `exit` returns `1` before shutdown and `0` after shutdown. Removed the standalone `shutdownRequested` state. |
| 2026-08-23 03:54 +08:00 | completed | Revalidated the lifecycle contract after deterministic reader teardown. The protocol driver now explicitly proves that a pre-initialize `didOpen` is ignored, non-`exit` notifications after shutdown remain silent, and the valid `initialized` transition remains observable before normal workspace requests. |

## Contract

- `initialize` can transition only from `NEW`; all later initialize requests
  are `InvalidRequest`.
- General requests are accepted in `INITIALIZING` and `RUNNING`, allowing a
  client to issue a legal request after the initialize response but before the
  `initialized` notification.
- `initialized` records the notification and transitions only
  `INITIALIZING -> RUNNING`.
- `shutdown` transitions a request-capable state to `SHUTDOWN`. General
  requests after that point receive `-32600`; all non-`exit` notifications are
  ignored.
- `exit` is the only notification accepted after shutdown. It returns process
  exit code `0` after shutdown and `1` otherwise.

## Evidence

Validation used a fresh source archive at
`66a97e8ac3e433901f3b1adfcfbd8a937e16e0e2` with only this task's six code
paths overlaid. The archive omits Git submodule contents, so the fixed Unity,
xxHash, utf8proc, cJSON, and tinydir inputs were linked read-only from the
workspace. `BUILD_WASM=OFF`, so the unrelated dirty WASM-only CMake hunk did
not contribute to the native target.

```text
cmake -S /home/hejiahui/.codex-snapshots/lsp-protocol-lifecycle-66a97e8 \
  -B /home/hejiahui/.codex-builds/lsp-protocol-lifecycle-snapshot-gcc \
  -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIB=ON -DBUILD_STATIC_LIB=OFF
cmake --build /home/hejiahui/.codex-builds/lsp-protocol-lifecycle-snapshot-gcc \
  --target zr_vm_language_server_stdio zr_vm_cli_executable \
  zr_vm_descriptor_plugin_fixture_int --parallel 8
ctest --test-dir /home/hejiahui/.codex-builds/lsp-protocol-lifecycle-snapshot-gcc \
  --output-on-failure -R '^language_server_stdio_protocol_conformance$'
ctest --test-dir /home/hejiahui/.codex-builds/lsp-protocol-lifecycle-snapshot-gcc \
  --output-on-failure -R '^language_server_stdio_smoke$'
```

Results:

- The stdio executable and supporting CLI/fixture targets built with GCC and
  linked `stdio_lifecycle.c`.
- The protocol driver has `11` passing cases. Its lifecycle cases all pass:
  request before initialize, repeated initialize, exit before shutdown, and
  request after shutdown.
- The CTest process for the conformance driver exits `1` only because the five
  remaining expected RED cases are intentionally still owned by Task 2
  (missing/wrong `jsonrpc`, boolean id, scalar params) and Task 4 (duplicate
  active request id).
- `language_server_stdio_smoke` exits `0`, `1/1` passed.
- `git diff --check` reported no whitespace error in the lifecycle paths.

## Remaining Work

Task 2 must validate the JSON-RPC envelope and parameters before lifecycle
dispatch. Task 3 owns bounded frame parsing. Task 4 owns request-id registry
and cancellation. None of those remaining RED cases is suppressed by this
state machine.
