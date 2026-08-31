---
related_code:
  - zr_vm_core/include/zr_vm_core/call_info.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/function_precall_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame_copy_fast.h
implementation_files:
  - zr_vm_core/include/zr_vm_core/call_info.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/function_precall_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame_copy_fast.h
plan_sources:
  - docs/plans/benchmark/optimize/02-interpreter-hot-path.md
  - docs/plans/benchmark/optimize/05-execution-roadmap.md
tests:
  - tests/core/test_frame_slot_layout_lookup.c
  - tests/core/test_precall_frame_slot_reset.c
  - tests/core/test_postcall_fast_paths.c
  - tests/core/test_tail_reuse_callinfo_reset.c
  - tests/core/test_type_layout_inline_copy.c
  - tests/core/test_execution_member_access_fast_paths.c
  - tests/core/test_object_shape_transition_cache.c
  - tests/gc/gc_tests.c
  - tests/core/test_execution_add_stack_relocation.c
  - tests/core/test_execution_numeric_fast_paths.c
doc_type: testing-guide
---

# Mixed-Service Frame Fast Paths Acceptance

## Scope

This acceptance record covers the frame and call-boundary follow-up selected by
the `mixed_service_loop` profile. It does not accept the complete interpreter
Task 4 call optimization or the benchmark wall-time gate.

## Implementation Contract

- Canonical finalized VALUE slots use a bounded direct branch in the public
  frame-place helper, frame initialization, overlap checks, reverse pointer
  mapping, and inline-frame drop.
- Direct addresses are always derived from the current frame base. Stack
  relocation does not leave a cached raw address behind.
- Alias, indirect/borrowed alias, inline struct, sparse, reordered, malformed,
  misaligned, or out-of-bounds layouts retain the checked place and full reverse
  scan paths.
- VM precall records the allocation-time frame storage count in the three bytes
  of legacy ABI padding after `hasReturnDestination`. The 24-bit `count + 1`
  encoding leaves zero and over-capacity frames as a legacy/native fallback that
  resolves function metadata and scans generated temporaries as before.
- The allocation boundary is call-local and immutable. No mutable derived cache
  is stored on shared `SZrFunction` metadata.
- Tail-call reuse writes the new callee's storage count after resizing the reused
  frame, so a differently sized prior callee cannot leave a stale boundary.

The focused precall test proves the last point by changing the function's
instruction length after the call has been allocated and requiring
`ZrCore_Function_GetCallInfoFrameStorageTop` to return the original boundary.

## Deterministic Performance Evidence

All profiles use the same ext4 GCC 11.4 Release build, CPU 2 affinity,
scale-1 `mixed_service_loop` input, and checksum `408940136`.

| Stage | Total Ir | Stage change | Cumulative change |
|---|---:|---:|---:|
| baseline | 868,860,510 | - | - |
| direct public frame place | 818,869,898 | -5.75% | -5.75% |
| direct initialization/intersection paths | 663,252,028 | -19.00% | -23.67% |
| per-call frame storage count | 492,531,900 | -25.74% | -43.31% |
| direct reverse pointer mapping | 426,876,372 | -13.33% | -50.87% |
| direct inline-frame drop place | 409,692,473 | -4.03% | -52.85% |
| direct VALUE-parameter place and immutable layout summary | 396,430,578 | -3.18% exact paired slice | -54.37% |
| direct VALUE-only frame-drop summary | 378,649,763 | -4.42% exact paired slice | -56.42% |
| strict-summary direct frame initialization | 365,295,917 | -3.52% exact paired slice | -57.96% |
| dispatch direct VALUE getter inline, final current binary | 349,179,948 | -4.42% exact paired slice | -59.81% |
| direct VALUE-to-VALUE inline-copy probe skip, final current binary | 325,175,994 | -6.87% exact paired slice | -62.57% |

The original frame-consumer slice saves `459,168,037 Ir`. Public
`ZrCore_Function_MakeFrameSlotPlace` calls fall from `450,682` to `112,686`.
Its raw profile is archived at
`build/benchmark-artifacts/tests_generated/performance_profile_callgrind_mixed_service_loop_frame_fast_paths/callgrind.out`.
The later parameter-summary slice uses an exact baseline that includes the same
member-cache null guard on both sides: `409,431,558 -> 396,430,578 Ir`
(`-3.18%`). Its raw paired outputs are
`/home/hejiahui/.cache/codex/callgrind.mixed-param-summary-final.before.out`
and `/home/hejiahui/.cache/codex/callgrind.mixed-param-summary-final.after.out`.
The later direct VALUE-only frame-drop summary uses a separate exact pair:
`396,142,221 -> 378,649,763 Ir` (`-4.42%`). Its raw outputs are
`/home/hejiahui/.cache/codex/callgrind.mixed-frame-drop.before.out` and
`/home/hejiahui/.cache/codex/callgrind.mixed-frame-drop.after.out`.
The initialization follow-up uses `378,637,009 -> 365,295,917 Ir` (`-3.52%`)
from `/home/hejiahui/.cache/codex/callgrind.mixed-frame-init.before.out` and
`/home/hejiahui/.cache/codex/callgrind.mixed-frame-init.after.out`.
The dispatch getter follow-up uses a fresh exact pair,
`365,326,562 -> 349,179,948 Ir` (`-4.42%`), from
`/home/hejiahui/.cache/codex/callgrind.mixed-frame-getter-inline.before.out`
and `/home/hejiahui/.cache/codex/callgrind.mixed-frame-getter-auto-inline.out`.
The direct-to-direct copy-probe follow-up uses
`349,179,948 -> 325,175,994 Ir` (`-6.87%`) from
`/home/hejiahui/.cache/codex/callgrind.mixed-frame-getter-auto-inline.out` and
`/home/hejiahui/.cache/codex/callgrind.mixed-copy-probe.final.out`.
The current binary saves `543,684,516 Ir` relative to the original baseline.
The checksum is unchanged, so the accepted claim is the deterministic
instruction-count reduction only. No calibrated, eligible paired wall-time
series exists for this final state; `mixed_service_loop +10%` remains open.

## Validation Matrix

The current final source passes the following focused binaries:

| Toolchain | frame | precall | postcall | tail reuse | VM closure | type layout | type metadata | member | shape | GC | relocation | numeric |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| GCC 11.4 Release | 28/28 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |
| Clang 14 Debug | 28/28 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |
| Clang 14 ASan/UBSan/LeakSanitizer Debug | 28/28 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |
| MSVC 19.44 Debug, fresh build | 28/28 | 16/16 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 |

The sanitizer matrix runs with fixed process ASLR (`setarch x86_64 -R`) because
this WSL/Clang 14 runtime otherwise intermittently faults before
`AddressSanitizer Init done`. Leak detection and halt-on-error remain enabled.
It exposed and fixed a null `cachedReceiverObject` member-address conversion in
the single-slot member fast path before the final clean run above.

An intermediate tail-appended MSVC layout exposed a stale-object CRT failure.
The final cache instead occupies existing padding, and a focused test asserts
that its full byte range ends before the next legacy field and that
`sizeof(SZrCallInfo)` equals the aligned pre-cache tail. A completely new CMake
directory rebuilt every core object and passed the full matrix above; the stale
directory is not used as acceptance evidence.

## Decision

The direct-frame and per-call storage-boundary slice is accepted for correctness
and deterministic instruction-count reduction. The complete call/return Task 4,
typed scalar work, and the mixed-service wall-time gate remain open.
