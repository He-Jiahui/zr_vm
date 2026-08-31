---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/profile.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/profile.c
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
plan_sources:
  - docs/plans/benchmark/optimize/02-interpreter-hot-path.md
  - docs/plans/benchmark/optimize/05-execution-roadmap.md
doc_type: testing-guide
---

# Direct VALUE Frame Drop Summary Acceptance

## Scope

This record accepts the immutable direct VALUE-only frame-drop summary selected
from the final `mixed_service_loop` Callgrind ranking. It does not relax inline
struct, alias, constructor-unwind, ownership, GC, or stack-boundary behavior and
does not accept the remaining interpreter Task 4 or wall-time gates.

## Runtime Contract

- `ZrCore_Function_FinalizeDirectFrameValueSlots` clears
  `directValueFrameSlotCountPlusOne` before validation and publishes it only
  when every frame layout is the canonical entry for its stack slot, every slot
  is a bounded direct VALUE layout, and `DIRECT_VALUE` is its only derived flag.
- Zero is the checked fallback. Unfinalized, hand-built, mixed inline/VALUE,
  alias, reordered, sparse, malformed, or extra-flag layouts retain the original
  registry lookup, inline-layout preflight, inline drop, and checked VALUE-place
  path.
- The summary is append-only runtime-derived state. It is initialized and
  tombstoned with `SZrFunction`, immutable after publication, never serialized,
  and rebuilt by compiler/loader finalization after validated layout loading.
- The direct drop loop still calls the bounded direct-place helper for every
  slot. It releases ownership independently in byte-backed and dense VALUE
  mirrors and preserves the existing overlap guard.
- The checked path still resolves every inline layout before performing any
  inline or VALUE drop. A resolver failure therefore retains the existing
  failure-atomic preflight behavior.
- Strict direct-only publication excludes constructor-bitmap and inline receiver
  flags, so the same direct loop is valid during ordinary exit and unwind. Any
  frame carrying those lifecycle flags remains on the checked path.

Append-only helper IDs expose `frame_value_drop_direct` and
`frame_value_drop_checked`. The final scale-1 profile records `20,485` direct
drops and one checked drop.

## Test-First Evidence

The focused test was written before production changes. Its RED compile failed
because `directValueFrameSlotCountPlusOne`,
`ZR_PROFILE_HELPER_FRAME_VALUE_DROP_DIRECT`, and
`ZR_PROFILE_HELPER_FRAME_VALUE_DROP_CHECKED` did not exist.

The final focused binary passes `24/24`. The new coverage proves append-only
helper names, stale-summary clearing, direct-only publication, mixed-inline
rejection, loader reconstruction, and direct/checked runtime counters. Existing
inline-layout tests additionally preserve resolver validation, partial
constructor unwind, alias single-drop, nested registry drops, ownership release,
and post-call/tail cleanup behavior.

## Deterministic Performance

Both sides use GCC 11.4 Release on WSL ext4 source/build trees, CPU 2 affinity,
Valgrind 3.18.1 Callgrind, scale 1, and separate clean generated projects. The
before binary is the accepted VALUE-parameter-summary state; the after binary
adds only this frame-drop slice.

| Case | Before Ir | After Ir | Change | Checksum |
|---|---:|---:|---:|---:|
| `mixed_service_loop` | 396,142,221 | 378,649,763 | -4.42% | 408940136 |
| `numeric_loops` | 134,678,155 | 134,649,802 | -0.021% | 48943705 |
| `object_field_hot` | 126,311,780 | 126,310,664 | -0.0009% | 623146080 |

The target case saves `17,492,458 Ir` and passes the plan's `3%` deterministic
slice gate. Both representatives improve slightly and remain inside the `1%`
regression limit. Relative to the original mixed-service baseline
`868,860,510 Ir`, the retained final binary is down `56.42%`.

Function-level attribution for `function_drop_inline_frame_values` is:

| Cost | Before Ir | After Ir | Change |
|---|---:|---:|---:|
| exclusive | 37,282,153 | 19,658,273 | -47.27% |
| inclusive | 49,861,149 | 32,114,373 | -35.59% |

Raw artifacts are retained at:

- `/home/hejiahui/.cache/codex/callgrind.mixed-frame-drop.before.out`
- `/home/hejiahui/.cache/codex/callgrind.mixed-frame-drop.after.out`
- `/home/hejiahui/.cache/codex/callgrind.numeric-frame-drop.before.out`
- `/home/hejiahui/.cache/codex/callgrind.numeric-frame-drop.after.out`
- `/home/hejiahui/.cache/codex/callgrind.object-frame-drop.before.out`
- `/home/hejiahui/.cache/codex/callgrind.object-frame-drop.after.out`
- `/home/hejiahui/.cache/codex/mixed-frame-drop.profile.json`

## Validation Matrix

| Toolchain | frame | precall | postcall | tail | VM closure | type layout | type metadata | member | shape | GC | relocation | numeric |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| GCC 11.4 Release | 24/24 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |
| Clang 14 Debug | 24/24 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |
| Clang 14 ASan/UBSan/LSan | 24/24 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |
| MSVC 19.44 Debug, fresh | 24/24 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |

The sanitizer matrix uses `setarch x86_64 -R`,
`ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=1`, and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. All selected binaries finish
without AddressSanitizer, UndefinedBehaviorSanitizer, or LeakSanitizer reports.

## Decision

The direct VALUE-only frame-drop summary is accepted for correctness and
deterministic instruction reduction. Mixed/inline lifecycle paths, calibrated
paired wall time, and the rest of interpreter Task 4 remain open.
