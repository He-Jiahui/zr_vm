# Debug Phase 5.5 Data Breakpoint Acceptance

## Scope

Phase 5.5 adds DAP-facing software data breakpoints / watchpoints for local variables and upvalues.

Affected layers:

- `zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h`
- `zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h`
- `zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c`
- `zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol.c`
- `tests/debug/test_debug_data_breakpoint.c`
- `tests/CMakeLists.txt`
- debug plan and launch workflow documentation

## Baseline

The focused `debug_data_breakpoint` test was added before implementation. On WSL/GCC it initially failed because
the `initialize` response did not advertise `supportsDataBreakpoints`; the protocol also had no
`dataBreakpointInfo` / `setDataBreakpoints` handling and stopped events had no data-breakpoint fields.

## Implementation Notes

- `initialize` now advertises `supportsDataBreakpoints`.
- Scope objects include the DAP alias `variablesReference`, in addition to the existing internal scope id.
- `dataBreakpointInfo` returns stable data ids for local variables and upvalues.
- `setDataBreakpoints` replaces the current software watch list.
- The agent checks watchpoints only when the list is non-empty, on the LINE/COUNT hook path.
- A watchpoint compares the current slot value with the previous snapshot; a changed value triggers `reason=dataBreakpoint`.
- Stopped events for data breakpoints include `dataId`, `description`, and `threadId`.
- On trigger, the watchpoint snapshots the new value to avoid immediate repeated stops for the same change.

Current boundary: object field watchpoints are intentionally unsupported and return non-persistent data-breakpoint info. This phase documents the cost model as linear in the number of active watchpoints.

## Tooling Evidence

| Time | Toolchain | Scope | Result |
|------|-----------|-------|--------|
| 2026-06-22 03:32:22 +08:00 | WSL/GCC Debug | `debug_data_breakpoint` RED | FAIL, missing `supportsDataBreakpoints` |
| 2026-06-22 03:32:22 +08:00 | WSL/GCC Debug | `debug_data_breakpoint` direct/CTest | PASS |
| 2026-06-22 03:32:22 +08:00 | WSL/GCC Debug | `debug_data_breakpoint|debug_threads|debug_truncation|debug_step_edges|debug_snapshot_contracts|debug_agent|debug_agent_protocol|debug_variable_child_shape|debug_metadata|debug_trace|debug_traceback|debug_library` | 12/12 PASS |
| 2026-06-22 03:32:22 +08:00 | WSL/Clang Debug | same focused gate | 12/12 PASS |
| 2026-06-22 03:32:22 +08:00 | Windows/MSVC Debug | same focused gate | 12/12 PASS |

Known notes:

- One intermediate WSL/GCC focused gate failed in `debug_truncation` after `ZrDebugAgent` grew data-breakpoint fields; rebuilding the stale test target cleared the false stack-smashing failure.
- The WSL/Clang explicit rebuild exceeded the first wait window, then completed successfully when continued with a longer timeout.
- Windows/MSVC build logs retain the existing `debug_child_shape.c` local-shadow warning.

## Acceptance Decision

Accepted for Phase 5.5. DAP clients can set software data breakpoints on local variables and upvalues and receive a stopped event when the watched value changes.

Phase 5.6 remote authentication is optional in the plan and was recorded as not enabled because this run did not open non-loopback debugging. Phase 5 is therefore complete; the next required debug-plan work is Phase 6 profiling and tooling.
