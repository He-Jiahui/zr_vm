# Debug Phase 5.2 Step Edge Acceptance

## Scope

Phase 5.2 hardens DAP step behavior for four edge cases: tail-call `stepOver`, native-call `stepIn`, exception-unwind `stepOut`, and recursive same-line `stepOver`.

Affected layers:

- `zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c`
- `zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h`
- `tests/debug/test_debug_step_edges.c`
- `tests/CMakeLists.txt`
- debug plan and module documentation

## Baseline

The new focused test was added before implementation. On WSL/GCC it initially failed 3/4:

- tail-call `stepOver` stopped inside `leaf`, the tail-called callee
- exception-unwind `stepOut` reached a parent-frame visible location, but the first assertion was too narrow and demanded catch-body-only lines
- recursive same-line `stepOver` was preempted by the original line breakpoint in the recursive child call

The native-call `stepIn` case already behaved like step-over because native frames do not expose script line safepoints.

## Implementation Notes

- The step controller records the call frame active when the user starts stepping.
- `stepOver` now skips deeper child frames and same-depth different logical frames, which covers tail-call frame reuse.
- While `stepOver` is crossing a child/tail logical frame, breakpoints in that crossed region are deferred so a recursive child on the same source line does not stop before the step completes.
- `stepOut` does not stop in the middle of exception unwind before the next visible parent-frame stop point.

## Tooling Evidence

| Time | Toolchain | Scope | Result |
|------|-----------|-------|--------|
| 2026-06-22 00:50:51 +08:00 | WSL/GCC Debug | `debug_step_edges` direct | 4/4 PASS |
| 2026-06-22 00:50:51 +08:00 | WSL/GCC Debug | `debug_step_edges|debug_snapshot_contracts|debug_agent|debug_agent_protocol|debug_variable_child_shape|debug_metadata|debug_trace|debug_traceback|debug_library` | 9/9 PASS |
| 2026-06-22 00:50:51 +08:00 | WSL/Clang Debug | same focused gate | 9/9 PASS |
| 2026-06-22 00:50:51 +08:00 | Windows/MSVC Debug | `zr_vm_debug_step_edges_test.exe` | 4/4 PASS |

Known warnings:

- WSL and Windows builds retained unrelated project/AOT/library/debug warnings already present in nearby validation.
- One WSL/Clang build+CTest combined command timed out while CTest was still running; a CTest-only rerun completed and passed 9/9.

## Acceptance Decision

Accepted for Phase 5.2 only. Step edge semantics are covered and the focused DAP/debug regression set remains green on WSL/GCC and WSL/Clang, with Windows/MSVC step-edge smoke passing.

Phase 5 overall remains open. Next planned work is Phase 5.3 truncation handling.
