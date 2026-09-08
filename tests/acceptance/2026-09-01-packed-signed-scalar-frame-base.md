# Packed Signed Scalar Frame-Base Acceptance

## Scope

This slice is the first address-side step of interpreter Task 2. It does not
introduce an unboxed scalar payload, change `SZrTypeValue`, or alter the public
stack ABI. Instead, a finalized strict packed direct VALUE frame derives its
fixed-stride byte-mirror base once per dispatch frame. Quickened signed integer
arithmetic, comparison, conversion, fused load, and branch handlers address
their operands and complete-value destinations from that cached base.

The cache is available only while the current call-info function base matches
the frame identity used to resolve `currentFunction`. A call switch or stack
relocation clears it until the outer dispatch loop resolves the new frame.
Missing strict summary, an out-of-range slot, or a cleared cache uses the
existing direct/checked frame getter. Unsigned, floating-point, string, object,
ownership, and inline-layout handlers are unchanged.

## Test-First Evidence

1. The fixed-stride regression initially failed to link because
   `execution_frame_value_slot_dispatch_try_packed_direct_inline` did not
   exist. It now proves slot 1 resolves to exactly
   `(stackSize + 1) * sizeof(SZrTypeValueOnStack)` from the current frame base,
   rejects slot 2 at the two-slot boundary, and returns null after the derived
   strict summary is cleared.
2. The first signed-handler candidate rechecked the strict summary on every
   access. It passed the focused tests but regressed `numeric_loops` from
   `111,746,343` to `119,654,940 Ir` (`+7.076%`) and was rejected.
3. The retained implementation computes the packed base once per resolved
   frame, records the same direct helper count at each logical access, and
   fails closed on call-info/frame identity changes. The frame suite passes
   `37/37`, including stack relocation `20/20` in the wider matrix.

## Deterministic Performance Gate

All rows use the same GCC 11.4 Release binary family, CPU 2, scale-1 input, and
unchanged workload checksum.

| Workload | Before Ir | After Ir | Delta | Checksum |
| --- | ---: | ---: | ---: | ---: |
| `numeric_loops` | 111,746,343 | 94,984,122 | -15.000241% | 48943705 |
| `mixed_service_loop` | 236,125,782 | 237,158,413 | +0.437322% | 408940136 |
| `object_field_hot` | 110,875,378 | 103,881,355 | -6.308004% | 623146080 |

The numeric target clears the independent `3%` gate. Both representative
workloads remain inside the `1%` regression limit, and object improves. A
second pre-guard numeric sample recorded `94,879,398 Ir`, consistent with the
retained direction. Relative to the original mixed-service baseline, the
current result is `868,860,510 -> 237,158,413 Ir`, a `72.704662%` cumulative
reduction.

The final helper profile preserves `frame_value_slot_direct/checked` at
`2,502,333 / 1` for `numeric_loops`. The GCC core shared library changes from
`2,665,288` to `2,698,056` bytes (`+1.229436%`), an accepted cost for the
measured numeric reduction. Full typed scalar lanes, materialization, and the
Task 2 `30%` end-state gate remain open.

Raw profiles:

- `/home/hejiahui/.cache/codex/callgrind.numeric-packed-signed-slot-final.candidate.out`
- `/home/hejiahui/.cache/codex/callgrind.mixed-packed-signed-slot-final.candidate.out`
- `/home/hejiahui/.cache/codex/callgrind.object-packed-signed-slot-final.candidate.out`
- `/home/hejiahui/.cache/codex/profile.numeric-packed-signed-slot-final.json`

## Regression Matrix

Current GCC Release, Clang 14 Debug, Clang 14
ASan/UBSan/LeakSanitizer, and MSVC 19.44 Debug binaries all pass:

- frame `37/37`
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

The sanitizer matrix used `setarch x86_64 -R`, leak detection, and
halt-on-error for ASan and UBSan. All logs were scanned explicitly and contain
no AddressSanitizer, LeakSanitizer, or undefined-behavior report.
