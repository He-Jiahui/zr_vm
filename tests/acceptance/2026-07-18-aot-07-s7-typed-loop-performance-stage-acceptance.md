# AOT 07-S7 typed-loop performance stage acceptance

Timestamp: 2026-07-18 15:42:44 +08:00

Last verified: 2026-07-18 16:08:24 +08:00

Plan slice: M1.5 / 07-S7, sections 9.1, 9.2, 9.5, and 9.6

Status: 07-S7 is accepted for the currently supported typed scalar shapes and the canonical one-argument `i64`
counting-sum loop. Stages 08 through 12 continue.

## Completed Scope

- `backend_aot_c_typed_i64_loop_thunks.{h,c}` recognizes one exact ten-instruction shape: `i64` argument and return,
  zero-initialized index and accumulator, signed `index < limit`, accumulator plus index, index plus one, back edge, and
  accumulator return.
- The emitted thunk has one `TZrInt64` argument, two scalar C locals, a C `while`, and a direct scalar return. It has no
  `SZrState`, generated frame, `SZrTypeValue`, runtime helper, or safepoint dependency.
- The shape is integrated through the existing one-argument i64 thunk capability check, so forward declarations, typed
  direct calls, and reflection invoker cases agree on availability.
- The source entry `return sum_to(4096)` now emits `zr_aot_static_i64_one_arg_direct_call_full_aot` and
  `zr_aot_s2 = zr_aot_typed_i64_fn_1(zr_aot_s3);`. Constant call arguments, one-argument typed return kinds, and static
  tail-call result slots participate in scalar-local analysis; the entry no longer calls the environment-AOT shim
  through `ZrLibrary_AotRuntime_CallStaticDirect`.
- Non-matching loops remain on the general generated path; this slice does not claim general typed CFG lowering.
- The runtime guard calls the generated typed loop with limit 4096, requires result 8386560, and requires runtime
  construct/copy/reset helper counts all to remain zero.

## Baselines And Gates

- Interpreter baseline executes the same source-level `sum_to(4096)` entry.
- The current general environment-AOT baseline is a separately generated top-level loop with the same arithmetic. Its
  generated function still accepts `SZrState`, performs a GC safepoint on every back edge, and returns through
  `ZrLibrary_AotRuntime_ReturnI64`; it contains no typed thunk. This is a stronger current baseline than the historical
  frame-heavy half-degraded AOT described in the plan.
- Hard gates: typed loop speedup must be at least 1x interpreter, the 3x interpreter target is reported, and typed loop
  speedup must be at least 1.1x current general environment AOT.
- The general AOT positive control must perform at least one value construction, while the typed loop must perform zero.

## TDD Evidence

- RED: the generated-C contract was added before production support. A clean MSVC build produced two tests with one
  failure and one expected Unix ignore because `zr_aot_typed_i64_fn_1(TZrInt64)` was absent.
- The actual generated general function was inspected before implementation. Its child loop contained exactly ten
  instructions and established the slot, constant, compare, jump, add, copy, increment, back-edge, and return contract.
- GREEN: the dedicated strict recognizer and canonical emitter made the MSVC structure contract pass without broadening
  any existing scalar shape recognizer.
- Follow-up RED: source inspection showed that the generated entry still used
  `ZrLibrary_AotRuntime_CallStaticDirect(..., zr_aot_fn_1)` even though the thunk and reflection invoker existed. The
  exact source-entry direct-call assertions failed on clean MSVC; recognizing only the constant argument local remained
  RED because the `KNOWN_VM_TAIL_CALL` result was not classified as a scalar call result.
- Follow-up GREEN: scalar-local analysis now recognizes statically typed function/known-VM tail-call results,
  one-argument typed thunk return kinds, and call-argument constant consumers. The exact source-entry typed call passes
  on MSVC, GCC, and Clang.
- An initial baseline assertion required frame setup, but the current pure-scalar general generator had already elided
  it. The assertion was corrected to the observed `state` signature, back-edge safepoint, and ReturnI64 boundary; no
  production behavior changed for that correction.

## Verification Matrix

- WSL GCC: value construction profile 4/0, existing section 9.1/9.2 guardrail 6/0, typed-call contracts 4/0, generated
  scalar/loop dynamic guardrail 3/0, stack relocation 20/0.
- WSL Clang: value construction profile 4/0, existing section 9.1/9.2 guardrail 6/0, typed-call contracts 4/0,
  generated scalar/loop dynamic guardrail 3/0, stack relocation 20/0.
- Clean Windows MSVC Debug: value construction profile 4/0, existing guardrail 6/0, generated guardrail 3 tests with
  zero failures and two expected Unix-only ignores, typed-call contracts 4/0, stack relocation 20/0.

## Performance Evidence

- GCC scalar: AOT 8.844 ns/call, interpreter 28039.571 ns/call, speedup 3170.54x.
- GCC loop: typed 6189.799 ns/call, interpreter 10854020.562 ns/call, current general AOT 275531.578 ns/call;
  speedups are 1753.53x and 44.51x.
- Clang scalar: AOT 3.334 ns/call, interpreter 16358.443 ns/call, speedup 4905.84x.
- Clang loop: typed 4.251 ns/call, interpreter 6836853.312 ns/call, current general AOT 133322.656 ns/call;
  speedups are 1608292.95x and 31362.66x. Clang optimized the canonical arithmetic-sum loop to a near-closed form.
- Timings use the best positive value from three post-warmup samples. Code generation, shared-library compilation, and
  loading are excluded from the hot-call timing window.

## Decision

- Sections 9.1 and 9.2 remain build-breaking through the existing generated-body guardrail.
- The canonical source entry now proves the section 07-S5 typed-to-typed boundary directly; reflection-invoker
  availability alone is not accepted as direct-call evidence.
- Section 9.5 is GREEN for the available typed scalar and canonical typed-loop benchmark set.
- Section 9.6 is GREEN for representative typed scalar and loop paths against both interpreter and the current general
  environment-AOT path.
- More complex loops, other numeric types, and multi-block typed thunk generation remain valuable coverage expansion,
  but are not represented as completed general CFG lowering.
