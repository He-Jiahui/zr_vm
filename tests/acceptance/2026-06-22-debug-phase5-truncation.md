# Debug Phase 5.3 Truncation Acceptance

## Scope

Phase 5.3 makes debug previews visibly truncation-aware and adds lazy paging for long string values.

Affected layers:

- `zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c`
- `zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c`
- `zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h`
- `tests/debug/test_debug_truncation.c`
- `tests/CMakeLists.txt`
- debug plan and module documentation

## Baseline

The focused truncation test was added before implementation. On WSL/GCC it initially failed 3/4:

- long plain text was silently clipped without an omitted-byte marker
- long paths lost the informative filename tail
- long string previews clipped without a marker

After the marker work, the paging assertion exposed a separate safe-evaluate limit: very long string literals are intentionally rejected by the debug expression subset. The paging test therefore validates the target behavior through a runtime long string value read via `evaluate("zr")`.

## Implementation Notes

- `zr_debug_copy_text` now appends `...[+N]` when a fixed preview buffer truncates text.
- Path-like text uses the same marker but preserves the tail segment.
- Long string previews keep the bounded value text and expose a `STRING_CHUNKS` variables reference when the full string reaches `ZR_DEBUG_TEXT_CAPACITY`.
- Expanding that reference through `variables(start,count)` returns 64-byte string chunks named by byte range, such as `[64..128)`.

## Tooling Evidence

| Time | Toolchain | Scope | Result |
|------|-----------|-------|--------|
| 2026-06-22 01:42:22 +08:00 | WSL/GCC Debug | `debug_truncation` direct | 5/5 PASS |
| 2026-06-22 01:42:22 +08:00 | WSL/GCC Debug | `debug_truncation|debug_step_edges|debug_snapshot_contracts|debug_agent|debug_agent_protocol|debug_variable_child_shape|debug_metadata|debug_trace|debug_traceback|debug_library` | 10/10 PASS |
| 2026-06-22 01:42:22 +08:00 | WSL/Clang Debug | same focused gate | 10/10 PASS |
| 2026-06-22 01:42:22 +08:00 | Windows/MSVC Debug | `zr_vm_debug_truncation_test.exe` | 5/5 PASS |
| 2026-06-22 01:42:22 +08:00 | Windows/MSVC Debug | `debug_step_edges|debug_truncation` registered CTest | 2/2 PASS |

Known notes:

- One unwrapped WSL CTest command treated the regex `|` as shell pipes and did not execute product tests. The wrapped rerun produced the recorded 10/10 PASS.
- Windows/MSVC build logs retained the existing `debug_child_shape.c` local shadow warning.

## Acceptance Decision

Accepted for Phase 5.3 only. Truncation is now visible, path tails are preserved, and long string values can be fetched lazily through the variables paging contract.

Phase 5 overall remains open. Next planned work is Phase 5.4 task / coroutine multi-execution-body debugging.
