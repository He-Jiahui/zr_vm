# Debug Phase 5.1 DAP Snapshot Acceptance

## Scope

Phase 5.1 replaces duplicated DAP snapshot traversal with the Phase 2 core debug introspection APIs. The covered behavior is stackTrace frame enumeration, scopes/variables local and argument reads, closure variable preview, and evaluate-time local lookup in `zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c`.

Affected layers:

- `zr_vm_lib_debug`
- `tests/debug`
- `tests/CMakeLists.txt`
- `docs/plans/debug`

## Baseline

The RED contract test was added before implementation and failed 2/2 because `debug_snapshot.c` did not include `zr_vm_core/debug.h` and did not use an `SZrDebugActivation`-based local snapshot path. The repository worktree was already dirty with unrelated AOT, parser/LSP, runtime, and documentation changes; this acceptance record only covers the debug Phase 5.1 surface.

## Test Inventory

- `tests/debug/test_debug_snapshot_contracts.c`
  - verifies stack snapshot uses `ZrCore_Debug_GetStack` and `ZrCore_Debug_GetInfo`
  - verifies variables snapshot uses `ZrCore_Debug_GetLocal` and `ZrCore_Debug_GetUpvalue`
  - guards against reintroducing raw `callInfoList`, raw frame slot, or closure value pointer traversal in the main DAP snapshot paths
- `tests/debug/test_debug_agent.c`
- `tests/debug/test_debug_agent_protocol.c`
- `tests/debug/test_debug_variable_child_shape.c`
- `tests/debug/test_debug_metadata.c`
- `tests/debug/test_debug_trace.c`
- `tests/debug/test_debug_traceback.c`
- `tests/library/test_debug_library.c`
- Windows/MSVC CLI smoke with `tests/fixtures/projects/hello_world/hello_world.zrp`

## Tooling Evidence

| Time | Toolchain | Scope | Result |
|------|-----------|-------|--------|
| 2026-06-22 00:11:45 +08:00 | WSL/GCC Debug | `debug_snapshot_contracts|debug_agent|debug_agent_protocol|debug_variable_child_shape|debug_metadata|debug_trace|debug_traceback|debug_library` | 8/8 PASS |
| 2026-06-22 00:11:45 +08:00 | WSL/Clang Debug | same focused gate | 8/8 PASS |
| 2026-06-22 00:11:45 +08:00 | Windows/MSVC Debug | CLI `hello_world.zrp` smoke | build passed; output `hello world` |

Known warnings:

- WSL/Clang retained unrelated const qualifier warnings in `zr_vm_library/src/zr_vm_library/project/project.c`.
- Windows/MSVC retained existing third-party/library/CLI warnings. No warning blocked the Phase 5.1 smoke.

## Acceptance Decision

Accepted for Phase 5.1 only. DAP stack/local/upvalue snapshot paths now reuse the core debug APIs and have focused source-contract coverage to prevent regression. Phase 5 overall remains open: step edge semantics, truncation, task/thread enumeration, data breakpoints, and optional remote-auth work are not yet accepted.

## Follow-up

- Start Phase 5.2 with `tests/debug/test_debug_step_edges.c`.
- Keep DAP-specific inline union preview/materialization in the snapshot layer unless a later refactor introduces a shared value-shape API.
- If Phase 5.5 needs upvalue identity tracking, split closure snapshot value preview from closure identity/watchpoint helpers instead of overloading one path.
