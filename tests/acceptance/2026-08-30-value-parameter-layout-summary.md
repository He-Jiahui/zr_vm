---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/profile.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/profile.c
  - zr_vm_core/src/zr_vm_core/execution/execution_member_access.c
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

# VALUE Parameter Layout Summary Acceptance

## Scope

This record accepts the immutable VALUE-parameter layout summary and direct
parameter-place slice selected from the `mixed_service_loop` profile. It does
not accept the complete interpreter Task 4 call/return work or its wall-time
gates. Callee identity, return-destination specialization, generation/token
invalidation, and typed scalar argument/return copies remain open.

## Runtime Contract

- `ZrCore_Function_FinalizeDirectFrameValueSlots` clears the summary before
  validation and publishes it only for canonical layouts whose parameters are
  all validated direct VALUE slots.
- `directValueParameterCountPlusOne` reserves zero for checked fallback;
  `directValueParameterScanLength` ends at the final parameter layout so
  trailing locals are not visited.
- The fields are appended derived state. They are initialized and tombstoned
  with `SZrFunction`, immutable after publication, and not serialized.
- The writer masks `DIRECT_VALUE`; the loader rejects that untrusted input bit,
  validates and copies layouts, then rebuilds both the bit and summary.
- The copy helpers validate summary bounds and the final direct parameter at
  use time. Stale, hand-built, sparse, reordered, alias, inline, or malformed
  metadata uses the original full scan and checked place path.
- Zero-argument copies return without a layout visit. Non-empty direct copies
  preserve ownership release and keep byte-backed and dense VALUE mirrors in
  sync. Frame-source copies independently require a direct source place.

Append-only helper IDs expose `frame_value_parameter_copy_direct`,
`frame_value_parameter_copy_checked`, `frame_value_parameter_copy_empty`, and
`frame_value_parameter_layout_visit`.

## Test-First Evidence

The focused test evolved through these RED signals:

- the new helper name was initially unknown;
- direct destination and frame-source counters each reported `Expected 1 Was 0`;
- the empty path reported `Expected 1 Was 0`;
- the three-layout prefix test reported `Expected 1 Was 3`.

The final focused binary passes `21/21`. It includes summary value assertions,
stale-summary clearing, direct-bit invalidation fallback, exact one-layout
visitation, zero arguments, direct/checked destinations and sources, loader
reconstruction, and rejection of an input-derived direct flag.

## Deterministic Performance

Both sides use GCC 11.4 Release on WSL, ext4 source/build trees, CPU 2 affinity,
Valgrind 3.18.1 Callgrind, scale 1, and the same generated benchmark project.
The exact before source removes only this parameter slice; both sides include
the same member-cache null guard.

| Case | Before Ir | After Ir | Change | Checksum |
|---|---:|---:|---:|---:|
| `mixed_service_loop` | 409,431,558 | 396,430,578 | -3.18% | 408940136 |
| `numeric_loops` | 134,711,413 | 134,694,635 | -0.012% | 48943705 |
| `object_field_hot` | 126,333,965 | 126,340,855 | +0.005% | 623146080 |

The target case passes the plan's `3%` deterministic slice gate. Both unrelated
representatives remain far inside the `1%` regression limit. Relative to the
original mixed-service baseline `868,860,510 Ir`, the current binary is down
`54.37%`.

Function-level attribution for
`ZrCore_Function_CopyValueFrameParameters` is:

| Cost | Before Ir | After Ir | Change |
|---|---:|---:|---:|
| exclusive | 20,470,486 | 12,636,314 | -38.27% |
| inclusive | 34,929,366 | 21,688,474 | -37.91% |

The final helper profile records direct `61,449`, layout visit `61,449`, empty
`2`, and checked `0`. Each copied parameter therefore visits exactly one layout
in this workload.

Raw paired artifacts are retained at:

- `/home/hejiahui/.cache/codex/callgrind.mixed-param-summary-final.before.out`
- `/home/hejiahui/.cache/codex/callgrind.mixed-param-summary-final.after.out`
- `/home/hejiahui/.cache/codex/mixed-param-summary-final.profile.json`

## Validation Matrix

| Toolchain | frame | precall | postcall | tail | VM closure | type layout | type metadata | member | shape | GC | relocation | numeric |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| GCC 11.4 Release | 21/21 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |
| Clang 14 Debug | 21/21 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |
| Clang 14 ASan/UBSan/LSan | 21/21 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |
| MSVC 19.44 Debug, fresh | 21/21 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |

The sanitizer run uses `setarch x86_64 -R` because Clang 14 ASan otherwise
intermittently faults during WSL runtime initialization before
`AddressSanitizer Init done`. `detect_leaks=1`, UBSan, and halt-on-error remain
enabled. The first stable run found a real null-member-address UB in the
single-slot member cache when `cachedReceiverObject` was absent. A null guard
was added at the lowest-level exact-receiver helper; GCC, Clang Debug, ASan/UBSan
and MSVC then pass member `102/102`, followed by the complete clean matrix above.

## Decision

The VALUE-parameter direct-place and immutable prefix-summary slice is accepted
for correctness and deterministic instruction reduction. The calibrated paired
wall-time requirement and the rest of interpreter Task 4 remain open.
