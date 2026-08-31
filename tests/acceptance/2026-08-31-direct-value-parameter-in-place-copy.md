---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
tests:
  - tests/core/test_frame_slot_layout_lookup.c
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
  - tests/parser/test_compiler_w2_performance_quickening.c
plan_sources:
  - docs/plans/benchmark/optimize/02-interpreter-hot-path.md
  - docs/plans/benchmark/optimize/05-execution-roadmap.md
doc_type: testing-guide
---

# Direct VALUE Parameter In-Place Copy Acceptance

## Scope

This record accepts a strict-summary specialization of
`ZrCore_Function_CopyValueFrameParameters`. It removes repeated place and
mirror-copy work from the common VM call window where the argument source is
already the callee dense parameter mirror. It does not change frame-sourced
copies, checked layouts, inline parameters, or the remaining Task 4 gates.

## Runtime Contract

- The specialization is entered only when the immutable direct VALUE-parameter
  summary passes its count, scan-length, and last-parameter checks.
- Before copying anything, it proves that the complete `frameByteSize` span
  fits between the current stack base and tail. Finalization already proves
  each direct parameter byte offset is inside that frame span.
- Direct parameter byte addresses are derived from the current callee frame
  base. No raw pointer survives a stack relocation.
- If the source is the destination's dense mirror, copying it back to itself is
  redundant. The source remains unchanged and only the byte-backed mirror is
  copied. A separate argument window still copies into both mirrors.
- `ZrCore_Value_Copy` remains the overwrite boundary. It normalizes stale
  no-owner control pointers and releases any releasable destination owner
  before the copy. The specialization does not use raw assignment.
- Unfinalized, mutated, mixed, inline, alias, malformed, and other checked
  layouts retain the original full scan, place construction, ownership reset,
  overlap check, and mirror-copy path. `CopyValueFrameParametersFromFrame` is
  unchanged.

## Test-First Evidence

Two tests were added before the implementation. The first MSVC RED run passed
the preceding 29 cases, then failed both new cases:

- `Expected FALSE Was TRUE` when individual direct parameter slots fit but the
  complete declared frame extended beyond the stack tail.
- `Expected 1 Was 2` because a source already equal to the dense destination
  still produced two profiled value copies.

The final focused binary passes `31/31`. The same-source test also seeds stale
`ownershipControl` and `ownershipWeakRef` pointers in the byte destination and
proves that the retained `ZrCore_Value_Copy` boundary normalizes both.

## Deterministic Performance

Both sides use the same GCC 11.4 Release ext4 source/build, CPU 2 affinity,
Valgrind 3.18.1 Callgrind, scale 1, and the accepted direct-drop project.
Checksums are unchanged.

| Case | Before Ir | After Ir | Change | Checksum |
|---|---:|---:|---:|---:|
| `mixed_service_loop` | 296,648,172 | 282,552,302 | -4.752% | 408940136 |
| `numeric_loops` | 111,747,140 | 111,748,486 | +0.0012% | 48943705 |
| `object_field_hot` | 110,874,914 | 110,875,036 | +0.0001% | 623146080 |

The target saves `14,095,870 Ir` and passes the `3%` deterministic gate. Both
representatives remain far inside the `1%` regression gate. Relative to the
original `868,860,510 Ir` mixed-service baseline, the retained result is down
`67.480%`.

The parameter-copy call edge falls from `21,688,474` to `7,372,972 Ir`
inclusive (`-66.01%`). Each of the `61,440` hot direct parameters now performs
one profiled value copy instead of resetting and copying the unchanged dense
mirror back from its byte copy. The final core shared library remains
`2,661,192` bytes, unchanged from the preceding accepted binary.

Raw artifacts are retained at:

- `/home/hejiahui/.cache/codex/callgrind.mixed-direct-drop-span-none-skip.after.out`
- `/home/hejiahui/.cache/codex/callgrind.mixed-direct-parameter-copy.final.out`
- `/home/hejiahui/.cache/codex/callgrind.numeric-direct-parameter-copy.final.out`
- `/home/hejiahui/.cache/codex/callgrind.object-direct-parameter-copy.final.out`

## Validation Matrix

| Toolchain | frame | precall | postcall | tail | VM closure | type layout | type metadata | member | shape | GC | relocation | numeric | quickening |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| GCC 11.4 Release | 31/31 | 17/17 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 | 20/20 |
| Clang 14 Debug | 31/31 | 17/17 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 | 20/20 |
| Clang 14 ASan/UBSan/LSan | 31/31 | 17/17 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 | 20/20 |
| MSVC 19.44 Debug, regenerated fresh outputs | 31/31 | 17/17 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 | 20/20 |

The sanitizer matrix uses disabled ASLR, leak detection, and halt-on-error for
ASan and UBSan. All selected binaries exit zero with no sanitizer report. The
MSVC temporary build lost its root generated files during an interrupted
parallel regeneration; it was reconfigured with the same Debug/shared/test
options and rebuilt through 738 steps before this matrix.

## Decision

The strict direct VALUE-parameter in-place copy specialization is accepted for
correctness and deterministic instruction reduction. Frame-sourced arguments,
checked layout/lifecycle behavior, calibrated wall time, callee identity,
return-destination specialization, and generation/token invalidation remain
open.
