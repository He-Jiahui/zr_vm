---
related_code:
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

# Direct Frame Drop Span And No-Owner Skip Acceptance

## Scope

This record accepts two related direct VALUE-only frame-drop changes: one
complete-frame stack-span preflight per drop, and an early continue when both
the byte-backed and dense mirrors have `NONE` ownership. It does not change the
checked mixed/inline-layout path or accept the remaining interpreter Task 4 and
wall-time gates.

## Runtime Contract

- `directValueFrameSlotCountPlusOne` is published only when every frame layout
  is the canonical entry for its stack slot, every layout is a bounded direct
  VALUE, and `DIRECT_VALUE` is its only derived flag.
- The direct drop path first proves that the entire immutable `frameByteSize`
  span fits between the current stack base and tail. A failed preflight returns
  false before any value is dropped.
- Because finalization already proves every direct layout's byte span is inside
  `frameByteSize`, the loop derives byte-backed addresses directly from the
  current frame base without repeating the same stack-range arithmetic per
  slot.
- When both byte-backed and dense mirrors have `NONE` ownership, neither can
  release an owner, so the loop skips the owner helper and overlap probe. Any
  unique, shared, loaned, weak, or borrowed value follows the previous release
  and overlap logic unchanged.
- Frames without the strict summary retain registry lookup, failure-atomic
  inline-layout preflight, inline drop, checked VALUE place construction, and
  the original ownership behavior.

## Test-First Evidence

The complete-span test was added before the implementation. The first MSVC
run passed the preceding 28 tests and failed the new case with
`Expected FALSE Was TRUE`: both individual direct slots fit, but the declared
four-slot frame extended past the current stack tail. After the once-per-frame
preflight was added, the focused binary passed `29/29`.

The no-owner follow-up is guarded by an exact pair of `ownershipKind == NONE`
checks. Existing type-layout `40/40`, GC `67/67`, post-call, tail reuse, closure,
and sanitizer matrices exercise the owner-bearing fallback paths.

## Deterministic Performance

All pairs use the same GCC 11.4 Release ext4 source/build, CPU 2 affinity,
Valgrind 3.18.1 Callgrind, scale 1, and the accepted generated-slot-summary
projects. Checksums are unchanged.

| Stage | Mixed Ir | Change from prior stage | Decision |
|---|---:|---:|---|
| generated-slot summary | 314,490,481 | baseline | retained |
| complete-frame span preflight | 306,794,357 | -2.447% | below the 3% slice gate alone |
| plus no-owner pair skip | 296,648,172 | -3.307% | combined slice accepted |

The final representative pairs are:

| Case | Before Ir | After Ir | Change | Checksum |
|---|---:|---:|---:|---:|
| `mixed_service_loop` | 314,490,481 | 296,648,172 | -5.673% | 408940136 |
| `numeric_loops` | 111,742,361 | 111,747,140 | +0.0043% | 48943705 |
| `object_field_hot` | 110,868,508 | 110,874,914 | +0.0058% | 623146080 |

The target saves `17,842,309 Ir` and passes the `3%` deterministic gate. Both
representatives remain well inside the `1%` regression gate. Relative to the
original `868,860,510 Ir` mixed-service baseline, the retained result is down
`65.858%`.

`function_drop_inline_frame_values` exclusive cost falls from `19,658,273` to
`9,530,544 Ir` (`-51.52%`). The owner helper falls from `7,435,923` to `1,353
Ir`, confirming that the exact no-owner guard removes the intended redundant
work without deleting the owner-bearing path.

Raw artifacts are retained at:

- `/home/hejiahui/.cache/codex/callgrind.mixed-generated-slot-summary.after.out`
- `/home/hejiahui/.cache/codex/callgrind.mixed-direct-drop-span.after.out`
- `/home/hejiahui/.cache/codex/callgrind.mixed-direct-drop-span-none-skip.after.out`
- `/home/hejiahui/.cache/codex/callgrind.numeric-direct-drop-span-none-skip.after.out`
- `/home/hejiahui/.cache/codex/callgrind.object-direct-drop-span-none-skip.after.out`

## Validation Matrix

| Toolchain | frame | precall | postcall | tail | VM closure | type layout | type metadata | member | shape | GC | relocation | numeric | quickening |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| GCC 11.4 Release | 29/29 | 17/17 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 | 20/20 |
| Clang 14 Debug | 29/29 | 17/17 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 | 20/20 |
| Clang 14 ASan/UBSan/LSan | 29/29 | 17/17 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 | 20/20 |
| MSVC 19.44 Debug, fresh tree | 29/29 | 17/17 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 | 20/20 |

The sanitizer matrix uses disabled ASLR, leak detection, and halt-on-error for
ASan and UBSan. All selected binaries exit zero with no sanitizer report.

## Decision

The combined direct-frame drop slice is accepted for correctness and
deterministic instruction reduction. Its strict summary and full-span guard are
required invariants; checked lifecycle layouts and every non-`NONE` ownership
kind remain fallback coverage. Calibrated paired wall time and the rest of
interpreter Task 4 remain open.
