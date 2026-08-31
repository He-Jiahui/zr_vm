# Packed Direct VALUE Frame Summary Acceptance

## Scope

This slice strengthens `directValueFrameSlotCountPlusOne` from a direct-only
frame proof into a packed direct VALUE-frame proof. Publication now additionally
requires:

- one canonical layout for every logical stack slot;
- byte mirrors packed after the dense frame at `SZrTypeValueOnStack` stride;
- an exact `sizeof(SZrTypeValue)` payload for every mirror; and
- parameter layouts forming the exact `parameterCount` prefix.

Canonical direct slots in non-packed frames retain their per-slot
`DIRECT_VALUE` flags, but the strict frame summary stays zero and all existing
checked/per-layout paths remain active.

The packed proof lets precall reject inline-parameter work without scanning
layouts, and lets parameter copy, frame initialization, and frame drop derive
the byte-mirror base once and walk by a fixed stride. The three postcall inline
copy probes also return immediately for a strict direct-only callee; the caller
inline object-destination fallback remains unchanged.

The prepared-precall fast guard was deliberately not loosened. GDB on the
target fallback showed `3` arguments/parameters, `stackSize=25`,
`frameByteSize=3200`, `frameSlotLayoutLength=25`, and
`directValueFrameSlotCountPlusOne=26`. The byte mirror extends beyond the
logical stack, and GC-safe entry clearing covers the complete logical frame, so
the existing `frameStorageSlotCount == stackSize` and exact-clear requirements
remain valid fail-closed guards.

## Test-First Evidence

1. `test_strict_direct_value_summary_skips_precall_inline_parameter_scan`
   initially failed after the preceding `33/33` frame cases with
   `Expected FALSE Was TRUE`. The summary early return made the focused target
   pass `34/34`.
2. That change alone produced `273,765,184 -> 271,970,350 Ir`
   (`-0.655611%`) and was retained only as a combination candidate because it
   did not pass the independent `3%` target gate.
3. `test_direct_value_frame_summary_rejects_gapped_mirror` then failed after
   the preceding `34/34` cases with `Expected FALSE Was TRUE`. Strengthening
   summary publication made the focused target pass `35/35` and preserved
   direct per-slot flags for checked fallback.
4. The packed scan/copy/init/drop combination before postcall guards produced
   `265,991,878 Ir` (`-2.839406%` from the accepted baseline), still below the
   gate. Adding the return-side negative guards produced the accepted result.
5. The existing single-result postcall test now uses a manually strict
   summarized callee and still verifies movement to an explicit destination.

## Deterministic Performance Gate

All rows use the same GCC 11.4 Release binary family, CPU 2, scale-1 input, and
unchanged workload checksum.

| Workload | Before Ir | After Ir | Delta | Checksum |
| --- | ---: | ---: | ---: | ---: |
| `mixed_service_loop` | 273,765,184 | 255,021,394 | -6.846667% | 408940136 |
| `numeric_loops` | 111,741,735 | 111,727,700 | -0.012560% | 48943705 |
| `object_field_hot` | 110,865,674 | 110,877,254 | +0.010445% | 623146080 |

The target passes the independent `3%` gate and both representatives remain
inside the `1%` regression limit. Relative to the original mixed-service
baseline, the retained result is `868,860,510 -> 255,021,394 Ir`, a
`70.648753%` cumulative reduction.

The final exclusive ranking records:

- `ZrCore_Execute`: `93,274,192 Ir`
- `ZrCore_Function_PreCallPreparedResolvedVmFunction`: `8,693,791 Ir`
- `function_drop_inline_frame_values`: `8,578,193 Ir`
- `ZrCore_Function_CopyValueFrameParameters`: `5,324,987 Ir`
- `ZrCore_Function_InitializeFrameLayoutStorage`: `3,863,256 Ir`
- `ZrCore_Function_FindFrameSlotLayout`: `2,767,670 Ir`

The final `libzr_vm_core.so` is `2,661,192` bytes, `4,096` bytes larger than
the preceding accepted binary. The `function_precall_internal.h` edit also
invalidated the large dispatch translation unit; that build cost is retained
as a constraint for the next call-boundary slice.

Raw profiles:

- `/home/hejiahui/.cache/codex/callgrind.mixed-precall-summary-probe.after.out`
- `/home/hejiahui/.cache/codex/callgrind.mixed-packed-direct-frame.after.out`
- `/home/hejiahui/.cache/codex/callgrind.mixed-packed-direct-frame-return-guards.after.out`
- `/home/hejiahui/.cache/codex/callgrind.numeric-packed-direct-frame-return-guards-scale1.after.out`
- `/home/hejiahui/.cache/codex/callgrind.object-packed-direct-frame-return-guards-scale1.after.out`

## Regression Matrix

Fresh GCC 11.4 Release, Clang 14 Debug, Clang 14
ASan/UBSan/LeakSanitizer, and MSVC 19.44 Debug binaries all pass:

- frame `35/35`
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
