# Debug Phase 5.4 Threads Acceptance

## Scope

Phase 5.4 adds the DAP-facing thread model for zr execution bodies.

Affected layers:

- `zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h`
- `zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h`
- `zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c`
- `zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol.c`
- `zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c`
- `tests/debug/test_debug_threads.c`
- `tests/CMakeLists.txt`
- debug plan and launch workflow documentation

## Baseline

The focused `debug_threads` test was added before implementation. On WSL/GCC it initially failed 1/1 because
the `initialize` response did not advertise `supportsThreads`; the protocol also had no `threads` request and
stopped events did not include a `threadId`.

## Implementation Notes

- The debug agent now has an internal thread registry that maps each registered `SZrState` to a stable `threadId`.
- The main execution body is registered as `threadId=1`, name `main`.
- `threads` returns the registered execution bodies.
- `stopped` and `continued` events include `threadId`.
- `stackTrace`, `scopes`, `variables`, and `evaluate` accept `threadId`; the agent switches snapshot context to the requested state for the duration of the request.
- Frame, scope, and result objects include `threadId`. Individual variable rows do not repeat it, to avoid inflating large object expansion responses.

Current MVP boundary: the task runtime currently reuses the main `SZrState` and does not expose a separate active-state list. This phase completes protocol enumeration and per-thread snapshot routing; cross-task synchronized stepping remains future work.

## Tooling Evidence

| Time | Toolchain | Scope | Result |
|------|-----------|-------|--------|
| 2026-06-22 02:36:30 +08:00 | WSL/GCC Debug | `debug_threads` RED | 1/1 FAIL, missing `supportsThreads` |
| 2026-06-22 02:36:30 +08:00 | WSL/GCC Debug | `debug_threads` direct/CTest | PASS |
| 2026-06-22 02:36:30 +08:00 | WSL/GCC Debug | `debug_threads|debug_truncation|debug_step_edges|debug_snapshot_contracts|debug_agent|debug_agent_protocol|debug_variable_child_shape|debug_metadata|debug_trace|debug_traceback|debug_library` | 11/11 PASS |
| 2026-06-22 02:36:30 +08:00 | WSL/Clang Debug | same focused gate | 11/11 PASS |
| 2026-06-22 02:36:30 +08:00 | Windows/MSVC Debug | `debug_threads|debug_step_edges|debug_truncation` registered CTest | 3/3 PASS |

Known notes:

- One intermediate WSL build was interrupted by unrelated AOT dependency recompilation and was not counted as product validation.
- After `ZrDebugAgent` grew thread-registry fields, stale test binaries produced a false `debug_truncation` crash; rebuilding affected test targets cleared it.
- Adding `threadId` to every variable row made large `zr` global expansions too large. The accepted contract keeps `threadId` at result/frame/scope level.

## Acceptance Decision

Accepted for Phase 5.4 only. DAP clients can enumerate the main execution body and route stack, scope, variable, and evaluate requests by thread id.

Phase 5 overall remains open. Next planned work is Phase 5.5 data breakpoint / watchpoint.
