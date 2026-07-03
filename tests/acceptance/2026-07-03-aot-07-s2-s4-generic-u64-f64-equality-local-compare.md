# AOT 07-S2/S4 Generic u64/f64 Equality Local Compare

## Scope

- Slice: AOT 07-S2/S4, plain generic equality/inequality where both operands are proven u64 or f64 scalar locals.
- Goal: emit direct bool-local C comparisons for the proven u64/f64-local subsets, while preserving runtime generic
  equality helpers for mixed or unproven operand cases.
- Non-goals: mixed primitive equality direct lowering, dynamic/string/object truthiness, value-copy migration,
  GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and complete zero-frame typed bodies.

## Baseline

- RED added `generic_u64_f64_equality_local_project` to
  `tests/parser/test_aot_c_generic_bool_equality_local_smoke.c`.
- The fixture executes `u64 42 == 42`, `u64 42 != 7`, `f64 2.5 == 2.5`, and `f64 2.5 != 3.5`, branching after each
  bool result and returning 17 on success.
- Generated-C assertions required:
  - `zr_aot_scalar_constant_u64_local`
  - `zr_aot_generic_u64_compare_scalar_local`
  - `zr_aot_generic_f64_compare_scalar_local`
  - `zr_aot_b2 = (TZrBool)((zr_aot_u0 == zr_aot_u1) != 0u);`
  - `zr_aot_b3 = (TZrBool)((zr_aot_u0 != zr_aot_u4) != 0u);`
  - `zr_aot_b9 = (TZrBool)((zr_aot_f6 == zr_aot_f7) != 0u);`
  - `zr_aot_b10 = (TZrBool)((zr_aot_f6 != zr_aot_f8) != 0u);`
- Generated-C assertions forbade:
  - targeted `GenericPrimitiveLogicalEqual/NotEqual` helper calls for the u64/f64 compare slots
  - targeted `SyncBoolLocal` calls for slots 2, 3, 9, and 10
  - `frame.slotBase[0/1/4/6/7/8].value`
- WSL GCC focused RED first failed with `Expected Non-NULL`, proving the old generator missed the direct u64/f64
  compare markers. After adding compare lowering, the test failed with `Expected NULL` on `frame.slotBase[0].value`,
  proving unsigned constants still fell through to frame materialization.

## Implementation

- `backend_aot_c_lowering_generic_logical.c` generalized the i64 fast path into a primitive scalar-local compare path
  for i64, u64, and f64 operands.
- The fast path is gated by:
  - bool destination local can skip the value slot
  - left and right operands have the same primitive scalar local kind
  - left and right operands are written before the current equality instruction
- The emitted u64/f64 expressions are:
  - `zr_aot_bD = (TZrBool)((zr_aot_uL == zr_aot_uR) != 0u);`
  - `zr_aot_bD = (TZrBool)((zr_aot_uL != zr_aot_uR) != 0u);`
  - `zr_aot_bD = (TZrBool)((zr_aot_fL == zr_aot_fR) != 0u);`
  - `zr_aot_bD = (TZrBool)((zr_aot_fL != zr_aot_fR) != 0u);`
- `backend_aot_c_scalar_locals.c` now treats matching plain generic equality/inequality as u64/f64 local consumers,
  operand mentions, and bool-result local writes.
- `backend_aot_c_lowering_values.c` now writes unsigned constants through `zr_aot_scalar_constant_u64_local` before
  the frame fallback and returns early when the scalar-local proof shows the value slot can be skipped.
- Dynamic or mixed primitive operands continue to use `GenericPrimitiveLogicalEqual/NotEqual`.

## Tooling Evidence

- RED:
  - `wsl.exe bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_bool_equality_local_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_bool_equality_local_smoke_test"`
  - Result: the new u64/f64 equality test failed first with `Expected Non-NULL`, then exposed unsigned constant frame
    materialization with `Expected NULL`.
- Focused GREEN:
  - Same WSL GCC focused command passed `3 Tests 0 Failures 0 Ignored`.
- Generated C confirmation:
  - `zr_aot_scalar_constant_u64_local` is present for slots 0, 1, and 4.
  - `zr_aot_generic_u64_compare_scalar_local` and `zr_aot_generic_f64_compare_scalar_local` are present.
  - The four exact bool-local compare assignments are present.
  - Targeted runtime equality helper calls, bool sync calls, and operand `frame.slotBase[0/1/4/6/7/8].value` reads are
    absent.

## Regression Matrix

- WSL GCC passed:
  - generic bool equality local smoke 3/0
  - logical shared-library smoke 6/0
  - shared-library smoke 13/0
  - generic LOGICAL_NOT numeric local smoke 1/0
  - generic JUMP_IF bool/numeric local smoke 3/0
  - logical contracts 4/0
  - source contracts 24/0
  - guardrail contracts 6/0
  - typed direct-call bool 28/0
  - typed direct-call u64 25/0
  - typed direct-call f64 19/0
- WSL clang passed the same matrix:
  - generic bool equality local smoke 3/0
  - logical shared-library smoke 6/0
  - shared-library smoke 13/0
  - generic LOGICAL_NOT numeric local smoke 1/0
  - generic JUMP_IF bool/numeric local smoke 3/0
  - logical contracts 4/0
  - source contracts 24/0
  - guardrail contracts 6/0
  - typed direct-call bool 28/0
  - typed direct-call u64 25/0
  - typed direct-call f64 19/0
- Windows MSVC Debug:
  - Built the same target group successfully.
  - Logical contracts passed 4/0.
  - Source contracts passed 24/0.
  - Guardrail contracts passed 6/0.
  - Unix-only shared-library/direct-call smoke cases reported ignored with 0 failures.

## Acceptance Decision

Accepted for this slice. Proven plain generic u64/f64 equality/inequality can now stay entirely in scalar locals and
lower to direct C comparisons. This is a partial 07-S2/S4 improvement only; broader 07~12 work remains active.
