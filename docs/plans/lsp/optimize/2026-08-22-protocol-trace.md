<!--
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio.c
related_plans:
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
related_tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_protocol_inventory
  - language_server_stdio_smoke
-->

# LSP Stdio Protocol Trace

## Scope

This record completes the `$/setTrace` sub-milestone of Task 4 in
[`01-protocol-lifecycle-and-transport.md`](./01-protocol-lifecycle-and-transport.md).
It adds native stdio protocol tracing without allowing diagnostics or trace
records to enter the framed stdout response channel.

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
| --- | --- | --- |
| 2026-08-22 21:30 +08:00 | completed | 完成 `$/setTrace` 模式切换与 stderr-only 协议跟踪，`messages` 只输出 request/response 元数据，`verbose` 额外输出 notification，`off` 停止后续跟踪。 |

## Contract

- `$/setTrace` accepts only an object `params` value with `value` equal to
  `off`, `messages`, or `verbose`; invalid notifications do not alter the
  current trace mode and never produce a response.
- `messages` emits inbound request and outbound response metadata. `verbose`
  includes notifications. The trace record format is deliberately limited to
  direction, envelope kind, and method.
- Trace state is owned and read by the serial main-thread dispatcher. The
  reader thread remains limited to framing, envelope admission, cancellation,
  and request-id reservation.
- All protocol stdout remains `Content-Length` framed JSON-RPC. Trace records
  use `stderr` and cannot become client messages.

## Evidence

Validation used the isolated source overlay at
`/home/hejiahui/.codex-snapshots/lsp-json-rpc-envelope-792a6b0` and matching
GCC Debug build directory
`/home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc`.

```text
cmake --build /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --target zr_vm_language_server_stdio --parallel 8
ctest --test-dir /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --output-on-failure -R language_server_stdio_protocol_conformance
ctest --test-dir /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --output-on-failure -R language_server_stdio_protocol_inventory
ctest --test-dir /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --output-on-failure -R language_server_stdio_smoke
ctest --test-dir /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --output-on-failure -R language_server_stdio_position_encoding_smoke
ctest --test-dir /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --output-on-failure -R language_server_stdio_type_hierarchy_smoke
```

Results:

- GCC native stdio build exited `0`.
- The protocol conformance driver passed `22/22`, including the stderr-only
  `$/setTrace` transition and the post-`off` silence check.
- Protocol inventory and the native stdio, position encoding, and type
  hierarchy smoke tests each passed `1/1`.
- `node --check` passed for the changed protocol conformance driver.

## Remaining Work

Task 4 still owns request contexts, work-done/partial-result progress, and
uniform cancellation callbacks across all long-running handlers. Task 5 owns
deterministic thread and runtime teardown. Plan 02 Task 3 owns the dependency
fence that may reintroduce a precise `-32801` response.
