---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/profile.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/profile.c
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

# Direct VALUE Frame Initialization Acceptance

## Scope

This record accepts the frame-initialization optimization selected from the
post-frame-drop `mixed_service_loop` Callgrind ranking. It reuses the immutable
direct VALUE-only frame summary and does not broaden the summary's publication
contract or accept the remaining call/return and wall-time gates.

## Runtime Contract

- `ZrCore_Function_HasDirectValueFrameSlotSummary` is shared by initialization
  and frame drop. The fast path exists only for a nonzero, in-bounds summary
  whose count still matches `frameSlotLayoutLength`.
- The summary itself remains strict: every layout must be canonical, bounded,
  direct VALUE storage with no alias, inline, constructor, receiver, or other
  derived lifecycle flag. It is compiler/loader rebuilt and never serialized.
- Direct initialization walks layouts once and computes parameter order with a
  linear counter. It preserves exactly the first `preservedArgumentCount`
  parameter layouts and initializes all remaining direct VALUE slots.
- Every initialized address is still derived from the current frame base by
  `ZrCore_Function_TryGetDirectFrameValueSlotLayout`; no raw address survives a
  stack relocation.
- A missing or invalid summary retains the original alias checks, parameter
  index lookup, direct probe, generic place construction, and inline storage
  initialization.

Append-only helper IDs expose `frame_value_initialization_direct` and
`frame_value_initialization_checked`. The final scale-1 profile records
`20,486` direct initializations and one checked initialization.

## Test-First Evidence

The focused test was written before production changes. Its RED MSVC compile
failed because the two initialization helper IDs did not exist. The final
focused binary passes `26/26`.

The new runtime case uses two canonical direct VALUE parameters and preserves
one argument. It proves that the first byte-backed value remains intact, the
second is reset to null, and both direct and checked helper counters are
observable. The case lives in a separate include so the parent test source
remains below the project's roughly 1000-line modularization threshold.

## Deterministic Performance

Both sides use GCC 11.4 Release on WSL ext4 source/build trees, CPU 2 affinity,
Valgrind 3.18.1 Callgrind, scale 1, and separate clean generated projects. The
before binary is the accepted frame-drop state; the after binary adds only this
initialization slice.

| Case | Before Ir | After Ir | Change | Checksum |
|---|---:|---:|---:|---:|
| `mixed_service_loop` | 378,637,009 | 365,295,917 | -3.52% | 408940136 |
| `numeric_loops` | 134,699,862 | 134,625,875 | -0.055% | 48943705 |
| `object_field_hot` | 126,306,123 | 126,299,833 | -0.005% | 623146080 |

The target case saves `13,341,092 Ir` and passes the plan's `3%` deterministic
slice gate. Both representatives improve and remain inside the `1%` regression
limit. Relative to the original mixed-service baseline `868,860,510 Ir`, the
retained final binary is down `57.96%`.

Function-level attribution for
`ZrCore_Function_InitializeFrameLayoutStorage` is:

| Cost | Before Ir | After Ir | Change |
|---|---:|---:|---:|
| exclusive | 21,691,743 | 8,061,782 | -62.83% |
| inclusive | 21,694,073 | 8,350,930 | -61.51% |

Raw artifacts are retained at:

- `/home/hejiahui/.cache/codex/callgrind.mixed-frame-init.before.out`
- `/home/hejiahui/.cache/codex/callgrind.mixed-frame-init.after.out`
- `/home/hejiahui/.cache/codex/callgrind.numeric-frame-init.before.out`
- `/home/hejiahui/.cache/codex/callgrind.numeric-frame-init.after.out`
- `/home/hejiahui/.cache/codex/callgrind.object-frame-init.before.out`
- `/home/hejiahui/.cache/codex/callgrind.object-frame-init.after.out`
- `/home/hejiahui/.cache/codex/mixed-frame-init.profile.json`

## Validation Matrix

| Toolchain | frame | precall | postcall | tail | VM closure | type layout | type metadata | member | shape | GC | relocation | numeric |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| GCC 11.4 Release | 26/26 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |
| Clang 14 Debug | 26/26 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |
| Clang 14 ASan/UBSan/LSan | 26/26 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |
| MSVC 19.44 Debug, fresh | 26/26 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |

The sanitizer matrix uses `setarch x86_64 -R`,
`ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=1`, and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. All selected binaries finish
without AddressSanitizer, UndefinedBehaviorSanitizer, or LeakSanitizer reports.

## Decision

Strict-summary direct frame initialization is accepted for correctness and
deterministic instruction reduction. Callee identity, return-destination
specialization, generation/token invalidation, calibrated paired wall time, and
the rest of interpreter Task 4 remain open.
