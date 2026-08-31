# Dispatch Direct VALUE Copy-Probe Bypass Acceptance

## Scope

This slice removes the remaining call into
`execution_inline_frame_try_copy_stack_slot` from normal and fast
`GET_STACK` / `SET_STACK` dispatch when the current function carries the strict
direct VALUE-only frame summary. The existing inline-copy helper remains the
fallback for mixed, inline, union, constructor-carrier, unfinalized, malformed,
and out-of-range cases.

The dispatch wrapper first requires the immutable
`directValueFrameSlotCountPlusOne` proof and bounds both slots by
`frameSlotLayoutLength`. Without that proof it applies the existing per-slot
direct predicate. Only a pair that may still require inline handling calls the
unchanged probe helper, which repeats its own fail-closed predicate before
performing any inline/object/union work.

`ZR_PROFILE_HELPER_FRAME_VALUE_COPY_PROBE` is appended to the helper profile
enum so an actual fallback entry remains observable. The production wrapper
receives the existing probe as a compile-time-known callback; the focused unit
test supplies a stub and verifies whether the helper boundary was crossed.

## Test-First Evidence

1. The initial pass-through wrapper failed
   `test_dispatch_direct_value_copy_bypasses_inline_probe_helper` with
   `Expected 0 Was 1` after the preceding cases passed.
2. A per-slot call-site predicate made the focused MSVC binary pass `33/33`,
   but its exact mixed-service result was only
   `282,552,302 -> 274,182,784 Ir` (`-2.961%`). This intermediate was rejected
   because it did not meet the independent `3%` target gate.
3. The strict-summary case then failed with `Expected 0 Was 1` after its
   per-slot flags were deliberately cleared while the published summary stayed
   valid. The final wrapper uses the strict summary first and falls back when
   that summary is cleared. The focused binary passes `33/33`.

## Deterministic Performance Gate

All rows use the same GCC 11.4 Release binary family, CPU 2, scale-1 input, and
unchanged workload checksum.

| Workload | Before Ir | After Ir | Delta | Checksum |
| --- | ---: | ---: | ---: | ---: |
| `mixed_service_loop` | 282,552,302 | 273,765,184 | -3.109908% | 408940136 |
| `numeric_loops` | 111,748,486 | 111,741,735 | -0.006041% | 48943705 |
| `object_field_hot` | 110,875,036 | 110,865,674 | -0.008444% | 623146080 |

The target case passes the independent `3%` gate and both representatives stay
inside the `1%` regression limit. Relative to the original mixed-service
baseline, the retained result is `868,860,510 -> 273,765,184 Ir`, a
`68.491469%` cumulative reduction. The final mixed Callgrind artifact contains
no occurrence of `execution_inline_frame_try_copy_stack_slot`, confirming that
the helper was not entered by this workload.

The final `libzr_vm_core.so` is `2,657,096` bytes, `4,096` bytes smaller than
the preceding accepted binary.

Raw profiles:

- `/home/hejiahui/.cache/codex/callgrind.mixed-dispatch-copy-probe-summary.after.out`
- `/home/hejiahui/.cache/codex/callgrind.numeric-dispatch-copy-probe-summary.after.out`
- `/home/hejiahui/.cache/codex/callgrind.object-dispatch-copy-probe-summary.after.out`

## Regression Matrix

Freshly rebuilt GCC 11.4 Release, Clang 14 Debug, Clang 14
ASan/UBSan/LeakSanitizer, and MSVC 19.44 Debug binaries all pass:

- frame `33/33`
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
