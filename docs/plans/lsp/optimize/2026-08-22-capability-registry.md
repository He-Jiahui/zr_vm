# LSP Capability Registry Record

## Scope

This record covers Task 2 of
[`00-baseline-and-contract.md`](./00-baseline-and-contract.md). It introduces
runtime-neutral capability metadata and validation only. It does not change the
native initialize response, request dispatch, or WASM worker behavior.

## 状态与产出记录

| Time | Status | Completed items |
| --- | --- | --- |
| 2026-08-22 19:45 +08:00 | completed | Added a 33-entry LSP capability registry. Each descriptor records its capability key, protocol method, client capability path, core entry point, native adapter, WASM export contract, protocol test id, runtime mask, minimum protocol version, and resolve policy. Added structural and publishability validators; identity-only resolve contracts and unmarked 3.18 capabilities are rejected. Added an isolated CTest target that proves the registry rejects incomplete runtime metadata, identity resolve, unmarked 3.18 declarations, and missing test ids. |

## Build Integration

- `zr_declare_module` recursively collects `src/**/*.c`, so the new
  `protocol/lsp_capability_registry.c` is compiled into the LSP library without
  changing `zr_vm_language_server/CMakeLists.txt`.
- That CMake file has concurrent WASM dependency changes and was intentionally
  left unmodified and unstaged. The new test target and CTest registration are
  isolated in `tests/CMakeLists.txt`.

## Evidence

- RED: direct GCC compilation of the test failed at the expected missing
  `zr_vm_language_server/lsp_capability_registry.h` include.
- GREEN: direct GCC `-Wall -Wextra -Werror` compilation and execution passed.
- Portability: direct Clang 14 `-Wall -Wextra -Werror` compilation and execution
  passed. The current PowerShell shell has no loaded MSVC environment, so no
  MSVC result is claimed.
- Integration: a fresh WSL-local `f35b9cc` snapshot plus the four Task 2
  overlays configured successfully; the module target linked the new protocol
  source and `language_server_lsp_capability_registry` passed `1/1`.

## Open Work

- Task 3 must add the JSON-RPC lifecycle and negative conformance driver.
- Task 4 owns withdrawing or materially implementing the descriptors now
  labelled `ZR_LSP_CAPABILITY_RESOLVE_IDENTITY`.
- Task 5 must consume this registry from native initialize and the WASM worker,
  then prove the declared runtime masks and export contracts against real code.
