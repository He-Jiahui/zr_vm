---
related_code:
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame_copy_fast.h
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

# Direct VALUE Copy-Probe Skip Acceptance

## Scope

This record accepts the direct-to-direct stack-copy follow-up selected from the
post-getter-inline `mixed_service_loop` ranking. It removes speculative inline
copy work for a pair that cannot be inline, without changing the ordinary value
copy or any alias, inline payload, union, constructor-carrier, or ownership path.

## Runtime Contract

- `execution_inline_frame_try_copy_stack_slot` retains its null-function and
  return-slot guards before consulting the new predicate.
- When both slot layouts carry the canonical `DIRECT_VALUE` proof,
  `execution_frame_value_slot_copy_requires_inline_probe` returns false. The
  caller then performs the existing ordinary `SZrTypeValue` copy.
- If either layout lacks that proof, the original layout lookup, generic getter,
  inline layout compatibility, lifecycle, union, and carrier logic runs intact.
- No frame address is cached, and no serialized format or public ABI changes.
- The predicate is isolated in `execution_inline_frame_copy_fast.h`. This keeps
  an inline-frame-only change from invalidating the large dispatch translation
  unit that includes `execution_frame_value_slot_fast.h`.

## Test-First Evidence

The first RED exposed that including the entire inline-frame internal boundary
pulled in a private GC header unavailable to the focused test. The second RED
attempted to test the hidden implementation function and failed to link on
Windows. The contract was narrowed to a pure layout predicate; its behavior RED
then failed because the predicate did not exist. The final test builds two
canonical direct VALUE layouts, requires the probe to be skipped, clears one
direct proof, and requires the complete probe. The focused binary passes
`28/28`.

## Deterministic Performance

Both sides use GCC 11.4 Release on WSL ext4 source/build trees, CPU 2 affinity,
Valgrind 3.18.1 Callgrind, scale 1, and separately generated projects. The before
binary is the accepted ordinary-inline dispatch getter state. All checksums are
unchanged.

| Case | Before Ir | After Ir | Change | Checksum |
|---|---:|---:|---:|---:|
| `mixed_service_loop` | 349,179,948 | 325,175,994 | -6.87% | 408940136 |
| `numeric_loops` | 111,726,116 | 111,767,607 | +0.037% | 48943705 |
| `object_field_hot` | 110,883,674 | 110,881,829 | -0.002% | 623146080 |

The target saves `24,003,954 Ir` and passes the plan's `3%` deterministic gate.
Both representatives stay within the `1%` regression gate. Relative to the
original mixed-service baseline `868,860,510 Ir`, the current binary saves
`543,684,516 Ir` (`-62.57%`).

The target attribution changes as follows:

- `execution_inline_frame_try_copy_stack_slot` inclusive:
  `35,097,636 -> 11,083,464 Ir` (`-68.42%`); exclusive:
  `11,699,212 -> 11,083,464 Ir` (`-5.26%`).
- Generic frame getter exclusive: `26,244,430 -> 8,695,612 Ir`.
- `ZrCore_Function_FindFrameSlotLayout`: `12,260,588 -> 7,642,478 Ir`.
- Profile direct/checked getter counts:
  `1,890,775 / 30,725 -> 1,582,901 / 30,725`, exactly `307,874` fewer
  speculative direct getter calls.

The final shared library remains `2,657,032` bytes and the dispatch object
remains `808,736` bytes, unchanged from the getter-inline binary. Separating the
predicate into its own header leaves the dispatch object SHA-256 unchanged and
lets the final GCC incremental target rebuild compile only
`execution_inline_frame.c` (`253.70s`). Clang Debug took `47.64s`, Clang
sanitizer Debug `149.18s`, and MSVC Debug `32.49s` for the corresponding final
incremental builds.

Raw artifacts are retained at:

- `/home/hejiahui/.cache/codex/callgrind.mixed-frame-getter-auto-inline.out`
- `/home/hejiahui/.cache/codex/callgrind.mixed-copy-probe.final.out`
- `/home/hejiahui/.cache/codex/callgrind.numeric-frame-getter-inline.after.out`
- `/home/hejiahui/.cache/codex/callgrind.numeric-copy-probe.final.out`
- `/home/hejiahui/.cache/codex/callgrind.object-frame-getter-inline.after.out`
- `/home/hejiahui/.cache/codex/callgrind.object-copy-probe.final.out`
- `/home/hejiahui/.cache/codex/mixed-copy-probe-final.profile.json`

## Validation Matrix

| Toolchain | frame | precall | postcall | tail | VM closure | type layout | type metadata | member | shape | GC | relocation | numeric |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| GCC 11.4 Release | 28/28 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |
| Clang 14 Debug | 28/28 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |
| Clang 14 ASan/UBSan/LSan | 28/28 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |
| MSVC 19.44 Debug, fresh | 28/28 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |

The sanitizer matrix runs with `setarch x86_64 -R`, leak detection, and
halt-on-error for ASan and UBSan. Every selected binary exits zero with no
sanitizer report. MSVC was rechecked directly after the final incremental build;
all twelve binaries exit zero.

## Decision

The direct VALUE-to-VALUE speculative copy-probe skip is accepted for
correctness and deterministic instruction reduction. The calibrated wall-time,
callee/return specialization, typed scalar lanes, and remaining interpreter
roadmap gates stay open.
