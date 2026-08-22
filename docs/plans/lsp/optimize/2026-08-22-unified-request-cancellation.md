<!--
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/lsp_interface.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_hierarchy.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_diagnostics.c
  - zr_vm_language_server/stdio/stdio_rename.c
related_plans:
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
related_tests:
  - zr_vm_language_server_lsp_interface_test
  - language_server_stdio_protocol_conformance
  - language_server_stdio_smoke
  - language_server_stdio_position_encoding_smoke
  - language_server_stdio_type_hierarchy_smoke
-->

# LSP Unified Request Cancellation

## Scope

This record completes the uniform cancellation-callback sub-milestone of
Task 4 in
[`01-protocol-lifecycle-and-transport.md`](./01-protocol-lifecycle-and-transport.md).
It makes long-running LSP handlers observe the typed active-request registry
while they execute, without restoring the pre-Plan-02 global document
generation approximation for `ContentModified`.

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
| --- | --- | --- |
| 2026-08-22 21:57 +08:00 | completed | 在串行 request dispatcher 中安装并清除精确 request-id cancellation callback；workspace diagnostics、workspace symbols、references、rename 与 call/type hierarchy 的长循环统一检查该 callback，并保留现有 registry 对取消请求的 `-32800` 处理。 |

## Contract

- `SZrLspContext` owns an optional request-scoped cancellation callback. Its
  callback and user data are cleared immediately after each dispatcher call,
  including unknown-method failure paths.
- The stdio dispatcher supplies the callback from the active typed request
  registry. The reader thread continues to mark only the matching JSON-RPC id;
  it does not mutate a global input generation or introduce `-32801`.
- Workspace diagnostics, workspace symbol enumeration, reference collection,
  rename location construction and normalization, and hierarchy traversals
  stop when the callback reports cancellation. The handler then returns no
  result, and the existing request registry sends the precise cancellation
  response after dispatch.
- The LSP core remains serial. No cancellation callback is retained beyond the
  request boundary or used to make core snapshots concurrently mutable.

## Evidence

Validation used the isolated source overlay at
`/home/hejiahui/.codex-snapshots/lsp-json-rpc-envelope-792a6b0` and matching
GCC Debug build directory
`/home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc`.

```text
cmake --build /home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc \
  --target zr_vm_language_server_lsp_interface_test zr_vm_language_server_stdio --parallel 8
/home/hejiahui/.codex-builds/lsp-json-rpc-envelope-snapshot-gcc/bin/zr_vm_language_server_lsp_interface_test
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

- The GCC Debug interface and native stdio targets built with exit code `0`.
- `zr_vm_language_server_lsp_interface_test` exited `0`, including
  `LSP Request Cancellation Callback`: a cancelled workspace-symbol query
  stops before producing a result, and clearing the callback restores the
  normal query.
- Protocol conformance, standard stdio smoke, position encoding smoke, and
  type hierarchy smoke each passed `1/1` with exit code `0`.

## Remaining Work

Task 4 still owns request-context `workDoneToken` and `partialResultToken`
progress. Task 5 owns deterministic reader and runtime teardown. Plan 02
Task 3 owns the dependency fence that may introduce a precise `-32801`
`ContentModified` response.
