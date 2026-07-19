# AOT 07-S7 value construction counter and scalar performance guardrail

Timestamp: 2026-07-18 14:46:58 +08:00

Plan slice: M1.5 / 07-S7, plan section 9.5 and the typed-scalar subset of section 9.6

Status: section 9.5 construction-count instrumentation is complete for the covered typed scalar benchmark, and the
typed-scalar performance sub-gate is complete. Typed loops, the half-degraded AOT comparison, and full typed benchmark
coverage remain open, so 07-S7 is partially complete.

Follow-up: the typed-loop and current general-AOT comparison completed at 2026-07-18 15:42:44 +08:00. See
`2026-07-18-aot-07-s7-typed-loop-performance-stage-acceptance.md`; this file remains the historical scalar sub-slice
record.

## Plan Mapping

- Section 9.1 and 9.2 remain enforced by `test_aot_c_guardrail_contracts.c`: typed generated bodies reject interpreter
  environment symbols and unclassified `SZrTypeValue` writes.
- Section 9.5 gains a runtime `value_construct` helper counter and a generated-C direct typed-thunk zero-count gate.
- Section 9.6 gains a focused scalar hard gate: AOT must be at least 1x interpreter speed, while the 3x target is
  reported separately. The plan's typed-loop and current half-degraded AOT comparisons are not covered here.

## Completed Scope

- `ZR_PROFILE_HELPER_VALUE_CONSTRUCT` is appended as helper id 8. Existing ids 0 through 7 remain unchanged and are
  locked by a core contract test.
- Public `ZrCore_Value_InitAsRawObject`, `InitAsUInt`, `InitAsInt`, `InitAsBool`, `InitAsFloat`,
  `InitAsNativePointer`, `ZrCore_Value_ResetAsNull`, and `ZR_VALUE_FAST_SET` record value construction when helper
  profiling is enabled.
- `test_value_construction_profile.c` covers the stable helper name, append-only numeric ids, eight public
  materialization paths, the existing reset counter, and disabled-recording behavior.
- `test_aot_c_value_construction_guardrail.c` generates full AOT C for `add(left:int, right:int):int`, compiles it as a
  shared library, directly calls the exported typed thunk with 19 and 23, and requires result 42 with construct/copy/reset
  counts all zero. A public `ZrCore_Value_InitAsInt` call is the positive control.
- The same Unix test compares best positive nanoseconds per call across three samples after warmup: 2,000,000 direct
  AOT calls per sample versus 4,096 semantically equivalent interpreter calls per sample.

## TDD Evidence

- RED: the new core profile test was added before production support and failed to compile under MSVC only because
  `ZR_PROFILE_HELPER_VALUE_CONSTRUCT` was undeclared.
- GREEN: the minimal helper-name and public-materialization instrumentation made the original three counter tests pass.
- The generated-C fixture first exposed two test-harness expectation errors: the generator emitted an explicit
  `(TZrInt64)` return cast, and the public export macro is `ZR_VM_AOT_EXPORT`. Correcting those expectations did not
  require a production generator change.
- Regression diagnosis: initially inserting the new helper in the middle of the enum shifted existing numeric ids.
  Incrementally mixed MSVC objects then reported `stack_get=2` and failed the stack relocation helper bound. Moving the
  new helper to the end restored the old ABI, and the new 0..8 numeric contract prevents recurrence.

## Verification Matrix

- WSL GCC: value construction profile 3/0, existing AOT guardrail 6/0, generated-C value/performance guardrail 1/0,
  stack relocation 20/0.
- WSL Clang: value construction profile 3/0, existing AOT guardrail 6/0, generated-C value/performance guardrail 1/0,
  stack relocation 20/0.
- Clean Windows MSVC Debug build at `%TEMP%\zr_vm-aot-07-s7-msvc-clean`: value construction profile 4/0, existing
  AOT guardrail 6/0, stack relocation 20/0; the Unix-only generated-C guard is expected ignored with zero failures.
- The fourth core test is the final test-only append-id assertion added after the full GCC/Clang runs. Its exact 0..8
  compile-time contract was separately accepted by both WSL GCC and WSL Clang with `-fsyntax-only`.

## Scalar Performance Evidence

- GCC: AOT 3.836 ns/call, interpreter 11428.354 ns/call, speedup 2979.05x; hard redline and 3x target both pass.
- Clang: AOT 4.354 ns/call, interpreter 16996.413 ns/call, speedup 3903.29x; hard redline and 3x target both pass.
- This is a direct-call scalar throughput guard, not an end-to-end process benchmark. It deliberately excludes code
  generation, shared-library loading, and one-time initialization from the hot-call timing window.

## Decision And Remaining Work

- Section 9.5 now has a usable runtime construction counter and a zero-count typed scalar regression gate.
- The scalar subset of section 9.6 has a build-breaking 1x floor and reports the 3x target.
- Current typed thunk generation supports only finite constant and one-to-three-argument scalar arithmetic/bitwise
  shapes. Loops still use the general generated path with VM environment state. A C wrapper loop would not prove typed
  loop lowering and is therefore not accepted as a substitute.
- 07-S7 remains open until representative typed-loop timing, comparison with the current half-degraded AOT path, and
  broader typed benchmark construction counts are implemented and GREEN.
