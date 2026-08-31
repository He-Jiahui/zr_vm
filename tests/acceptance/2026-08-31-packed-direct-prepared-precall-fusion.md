# Packed Direct Prepared Precall Fusion Acceptance

## Scope

This slice specializes prepared VM calls only when all of the following are
already true: debug hooks are inactive, the argument count exactly matches the
parameter count, the callee has the strict packed direct VALUE frame summary,
the byte frame is larger than the logical stack, the current call window is
exact, the existing stack allocation covers the complete frame, the entry-clear
range fits that allocation, and a reusable next call-info already exists.

The specialized path initializes the reusable call-info, publishes the callee
metadata, clears logical entry locals, and clears every storage slot beyond the
logical stack. That padding clear also initializes the complete packed byte
mirror region, so a second layout-initialization walk is unnecessary. Exact
arguments are then copied directly from the dense parameter prefix to the
fixed-stride byte mirrors. `ZrCore_Value_Copy` remains the ownership boundary,
and the existing helper profile counts are preserved.

Any failed guard returns to the original exact-args probe and generic precall.
Stack growth, first-time call-info allocation, debug hooks, non-packed frames,
inline/alias layouts, non-exact arguments, and malformed storage bounds are
therefore unchanged.

## Test-First Evidence

1. The new complete-frame regression initially failed to link because
   `function_try_pre_call_prepared_resolved_vm_packed_direct_exact_args` did not
   exist. The first implementation also demonstrated that the steady-state
   specialization correctly declines when no reusable call-info is available.
2. The fixture then seeded the reusable call-info required by the contract and
   verified the function top, metadata publication, dense local reset, byte
   parameter copy, and byte local reset.
3. The final regression calls the public
   `ZrCore_Function_PreCallPreparedResolvedVmFunction` entry, verifies
   `toBeClosedValueOffset` cleanup in all reset lanes, and passes `18/18`.

## Deterministic Performance Gate

All rows use the same GCC 11.4 Release binary family, CPU 2, scale-1 input, and
unchanged workload checksum.

| Workload | Before Ir | After Ir | Delta | Checksum |
| --- | ---: | ---: | ---: | ---: |
| `mixed_service_loop` | 245,339,382 | 236,125,782 | -3.755451% | 408940136 |
| `numeric_loops` | 111,750,412 | 111,746,343 | -0.003641% | 48943705 |
| `object_field_hot` | 110,860,460 | 110,875,378 | +0.013457% | 623146080 |

The target passes the independent `3%` gate and both representatives remain
inside the `1%` regression limit. Relative to the original mixed-service
baseline, the retained result is `868,860,510 -> 236,125,782 Ir`, a
`72.823511%` cumulative reduction.

The first specialization retained generic parameter copy and layout
initialization and reached `241,384,455 Ir` (`-1.611%`). Reusing the padding
clear for byte-layout initialization reached `238,937,915 Ir` (`-2.609%`). Both
intermediate candidates remained below the independent gate. Exact packed
parameter copy reached `236,521,831 Ir`; making the implementation private and
testing the public integration produced the retained `236,125,782 Ir` result.

The final `libzr_vm_core.so` is `2,665,288` bytes, 4,096 bytes larger than the
preceding accepted binary.

Raw profiles:

- `/home/hejiahui/.cache/codex/callgrind.mixed-direct-drop-no-profile-batch4.candidate.out`
- `/home/hejiahui/.cache/codex/callgrind.mixed-packed-direct-precall.candidate.out`
- `/home/hejiahui/.cache/codex/callgrind.mixed-packed-direct-precall-fused.candidate.out`
- `/home/hejiahui/.cache/codex/callgrind.mixed-packed-direct-precall-fused-copy.candidate.out`
- `/home/hejiahui/.cache/codex/callgrind.mixed-packed-direct-precall-fused-copy-static.candidate.out`
- `/home/hejiahui/.cache/codex/callgrind.numeric-packed-direct-precall-final.candidate.out`
- `/home/hejiahui/.cache/codex/callgrind.object-packed-direct-precall-final.candidate.out`

## Regression Matrix

Current GCC 11.4 Release, Clang 14 Debug, Clang 14
ASan/UBSan/LeakSanitizer, and MSVC 19.44 Debug binaries all pass:

- frame `36/36`
- precall `18/18`
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
