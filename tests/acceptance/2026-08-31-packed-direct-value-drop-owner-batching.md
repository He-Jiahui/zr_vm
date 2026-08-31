# Packed Direct VALUE Drop Owner Batching Acceptance

## Scope

This slice removes generic stack-read profiling from strict packed direct VALUE
frame teardown and batches the common no-owner test four slots at a time.

The optimization is limited by `directValueFrameSlotCountPlusOne`, whose packed
contract proves that byte mirrors and dense slots occupy disjoint fixed-stride
regions. The direct loop therefore reads dense mirrors with
`ZrCore_Stack_GetValueNoProfile`, combines the ownership kinds of four byte and
four dense values, and skips the group when all eight are `NONE`. If any value
may own storage, every pair in that group still executes the original
releasable-ownership checks and releases byte and dense mirrors independently.
The remainder loop applies the same pair logic one slot at a time.

Frames without the strict packed summary retain the complete registry,
inline-layout, alias, unwind-bitmap, checked place, overlap, and profiled stack
access paths.

## Test-First Evidence

1. The new assertion in
   `test_frame_value_drop_profile_counts_direct_and_checked_paths` initially
   failed after the preceding cases with `Expected 0 Was 1`, proving that one
   strict direct slot still emitted a generic `stack_get_value` helper event.
2. Switching only the direct dense access to the no-profile getter made the
   focused test pass, but produced `255,021,394 -> 248,600,396 Ir`
   (`-2.517827%`). This candidate was not independently accepted because it
   missed the `3%` target gate.
3. `test_packed_direct_value_frame_drop_keeps_batched_owner_release` covers a
   five-slot packed frame with borrowed owners in different byte/dense lanes.
   Both values are released even when owner-free slots share their four-slot
   batch. The final focused frame target passes `36/36`.

## Deterministic Performance Gate

All rows use the same GCC 11.4 Release binary family, CPU 2, scale-1 input, and
unchanged workload checksum.

| Workload | Before Ir | After Ir | Delta | Checksum |
| --- | ---: | ---: | ---: | ---: |
| `mixed_service_loop` | 255,021,394 | 245,339,382 | -3.796549% | 408940136 |
| `numeric_loops` | 111,727,700 | 111,750,412 | +0.020328% | 48943705 |
| `object_field_hot` | 110,877,254 | 110,860,460 | -0.015146% | 623146080 |

The target passes the independent `3%` gate and both representatives remain
inside the `1%` regression limit. Relative to the original mixed-service
baseline, the retained result is `868,860,510 -> 245,339,382 Ir`, a
`71.763087%` cumulative reduction.

`function_drop_inline_frame_values` falls from `8,578,193` to `3,324,901 Ir`
exclusive (`-61.240%`). `__tls_get_addr` falls from `5,985,228` to
`2,175,828 Ir` (`-63.647%`). The final `libzr_vm_core.so` remains
`2,661,192` bytes, unchanged from the preceding accepted binary. The GCC
Release incremental CLI plus focused-test build took `74.34s` with `50,420
KiB` peak RSS.

Raw profiles:

- `/home/hejiahui/.cache/codex/callgrind.mixed-direct-drop-no-profile-get.candidate.out`
- `/home/hejiahui/.cache/codex/callgrind.mixed-direct-drop-no-profile-batch4.candidate.out`
- `/home/hejiahui/.cache/codex/callgrind.numeric-direct-drop-no-profile-batch4-scale1.after.out`
- `/home/hejiahui/.cache/codex/callgrind.object-direct-drop-no-profile-batch4-scale1.after.out`

## Regression Matrix

Fresh GCC 11.4 Release, Clang 14 Debug, Clang 14
ASan/UBSan/LeakSanitizer, and MSVC 19.44 Debug binaries all pass:

- frame `36/36`
- precall `17/17`
- postcall `3/3`
- tail reuse `4/4`
- VM closure `6/6`
- type layout `40/40`
- type metadata `9/9`
- member access `102/102`
- object shape `3/3`
- GC `67/67`
- stack relocation `20/20`
- numeric fast paths `11/11`
- quickening `20/20`

The sanitizer run used `setarch x86_64 -R` with leak detection and
halt-on-error enabled and produced no ASan, UBSan, or LeakSanitizer report.
