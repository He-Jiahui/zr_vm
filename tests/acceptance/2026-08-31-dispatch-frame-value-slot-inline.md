---
related_code:
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_frame_value_slot_fast.h
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c
  - zr_vm_core/src/zr_vm_core/execution/execution_internal.h
tests:
  - tests/core/test_frame_slot_layout_lookup.c
  - tests/core/frame_slot_layout_initialization_tests.inc
  - tests/core/test_precall_frame_slot_reset.c
  - tests/core/test_postcall_fast_paths.c
  - tests/core/test_tail_reuse_callinfo_reset.c
  - tests/core/test_vm_closure_precall.c
  - tests/core/test_type_layout_inline_copy.c
  - tests/core/test_type_layout_metadata_contracts.c
  - tests/core/test_execution_member_access_fast_paths.c
  - tests/core/test_object_shape_transition_cache.c
  - tests/gc/gc_tests.c
  - tests/core/test_execution_add_stack_relocation.c
  - tests/core/test_execution_numeric_fast_paths.c
plan_sources:
  - docs/plans/benchmark/optimize/02-interpreter-hot-path.md
  - docs/plans/benchmark/optimize/05-execution-roadmap.md
doc_type: testing-guide
---

# Dispatch Frame VALUE-Slot Inline Acceptance

## Scope

This record accepts the dispatch-only inlining slice selected from the
post-initialization `mixed_service_loop` ranking. It removes the dominant
out-of-line direct getter boundary without changing the public frame getter,
layout trust contract, or the still-open calibrated wall-time gates.

## Runtime Contract

- `FRAME_VALUE_SLOT` calls a translation-unit-local wrapper with the current
  function, current frame base, and loop-cached profile runtime.
- `execution_frame_value_slot_dispatch_try_direct_inline` returns a value only
  for an in-range canonical indexed layout carrying the locally rebuilt
  `DIRECT_VALUE` proof. It records the existing direct helper count.
- A guard miss returns null and calls `execution_inline_frame_get_value_slot`.
  The generic getter retains checked helper accounting, layout lookup, place
  construction, bounds validation, dense fallback, and inline-layout behavior.
- The direct result is always derived from the current frame base. No pointer is
  retained across debug hooks, stack growth, call boundaries, or relocation.
- The outer wrapper uses ordinary `inline`. This lets GCC retain one outlined
  helper for low-frequency call sites instead of forcing the probe into every
  dispatch handler.

The final scale-1 profile records `1,890,775` direct and `30,725` checked frame
VALUE-slot accesses. The retained outlined wrapper executes only `20,510`
times and accounts for `430,710 Ir`; the former
`execution_inline_frame_get_value_slot_dispatch_fast` hotspot is absent.

## Test-First Evidence

The focused probe contract was added before production code. Its first MSVC RED
failed because `execution_frame_value_slot_fast.h` did not exist. An initial
header that exposed the generic getter then failed at link time in the focused
test, which forced the final narrower boundary: the header exports only the
direct probe, while `execution_dispatch.c` owns the generic fallback wrapper.

The final test proves that a canonical direct slot returns the current
frame-relative address and increments only the direct counter. An out-of-range
slot returns the null fallback signal without fabricating a checked hit. The
focused binary passes `27/27`.

## Deterministic Performance

Both sides use GCC 11.4 Release on WSL ext4 source/build trees, CPU 2 affinity,
Valgrind 3.18.1 Callgrind, scale 1, and separate clean generated projects. The
before binary is the accepted strict-summary frame-initialization state. All
checksums are unchanged.

| Case | Before Ir | After Ir | Change | Checksum |
|---|---:|---:|---:|---:|
| `mixed_service_loop` | 365,326,562 | 349,179,948 | -4.42% | 408940136 |
| `numeric_loops` | 134,635,195 | 111,726,116 | -17.02% | 48943705 |
| `object_field_hot` | 126,304,766 | 110,883,674 | -12.21% | 623146080 |

The target saves `16,146,614 Ir` and passes the plan's `3%` deterministic gate.
Both representatives improve substantially. Relative to the original
`mixed_service_loop` baseline `868,860,510 Ir`, the current result is down
`59.81%`.

The force-inline candidate reached `348,704,899 Ir`, only `0.14%` below the
retained ordinary-inline result. Ordinary inline reduces `libzr_vm_core.so`
from `2,681,536` to `2,657,032` bytes and the dispatch object from `837,320` to
`808,736` bytes, so it is retained. Compared with the before binary, the final
shared library still grows from `2,566,920` to `2,657,032` bytes (`+3.51%`) and
the dispatch object grows from `723,336` to `808,736` bytes (`+11.81%`).

The measured GCC Release incremental target build took `449.59s` with
`1,280,792 KiB` peak RSS. Clang Debug took `291.42s`; a fresh MSVC Debug target
build took `419.38s`. These build and code-size costs are accepted explicitly
for the cross-workload instruction reduction, and remain a reason not to force
the wrapper into every handler.

Raw artifacts are retained at:

- `/home/hejiahui/.cache/codex/callgrind.mixed-frame-getter-inline.before.out`
- `/home/hejiahui/.cache/codex/callgrind.mixed-frame-getter-auto-inline.out`
- `/home/hejiahui/.cache/codex/callgrind.numeric-frame-getter-inline.before.out`
- `/home/hejiahui/.cache/codex/callgrind.numeric-frame-getter-inline.after.out`
- `/home/hejiahui/.cache/codex/callgrind.object-frame-getter-inline.before.out`
- `/home/hejiahui/.cache/codex/callgrind.object-frame-getter-inline.after.out`
- `/home/hejiahui/.cache/codex/mixed-frame-getter-auto-inline.profile.json`

## Validation Matrix

| Toolchain | frame | precall | postcall | tail | VM closure | type layout | type metadata | member | shape | GC | relocation | numeric |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| GCC 11.4 Release | 27/27 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |
| Clang 14 Debug | 27/27 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |
| Clang 14 ASan/UBSan/LSan | 27/27 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |
| MSVC 19.44 Debug, fresh | 27/27 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |

The sanitizer matrix uses `setarch x86_64 -R`, leak detection, and
halt-on-error for ASan and UBSan. Every selected binary exits zero with no
AddressSanitizer, UndefinedBehaviorSanitizer, or LeakSanitizer report.

## Decision

Ordinary-inline dispatch direct probing is accepted for correctness and
deterministic instruction reduction. Forced inlining is rejected because its
`0.14%` extra target gain does not justify the additional binary growth.
Calibrated wall time, callee/return specialization, typed scalar lanes, and the
remaining interpreter roadmap gates stay open.
