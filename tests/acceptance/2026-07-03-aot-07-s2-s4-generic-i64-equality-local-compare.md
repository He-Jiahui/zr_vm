# AOT 07-S2/S4 Generic i64 Equality Local Compare

## Scope

- Slice: AOT 07-S2/S4, plain generic equality/inequality where both operands are proven i64 scalar locals.
- Goal: emit direct bool-local C comparisons for that proven i64-local subset, while preserving runtime generic
  equality helpers for mixed or unproven operand cases.
- Non-goals: mixed primitive equality direct lowering, dynamic/string/object truthiness, value-copy migration,
  GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and complete zero-frame typed bodies.

## Baseline

- RED added `generic_i64_equality_local_project` to
  `tests/parser/test_aot_c_generic_bool_equality_local_smoke.c`.
- The fixture executes `42 == 42`, branches on the bool result, then executes `42 != 7` and returns 17 on success.
- Generated-C assertions required:
  - `zr_aot_generic_i64_compare_scalar_local`
  - `zr_aot_b2 = (TZrBool)((zr_aot_s0 == zr_aot_s1) != 0u);`
  - `zr_aot_b3 = (TZrBool)((zr_aot_s0 != zr_aot_s4) != 0u);`
  - `if (!zr_aot_b2) {` and `if (!zr_aot_b3) {`
- Generated-C assertions forbade:
  - `ZrLibrary_AotRuntime_GenericPrimitiveLogicalEqual(state, &frame, 2, 0, 1)`
  - `ZrLibrary_AotRuntime_GenericPrimitiveLogicalNotEqual(state, &frame, 3, 0, 4)`
  - `ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, 2`
  - `ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, 3`
  - `frame.slotBase[0].value`, `frame.slotBase[1].value`, and `frame.slotBase[4].value`
- WSL GCC focused run failed with `Expected Non-NULL`, proving the old generator still missed the direct i64-local
  compare marker.

## Implementation

- `backend_aot_c_lowering_generic_logical.c` adds a generic i64 scalar-local compare fast path for
  `LOGICAL_EQUAL` and `LOGICAL_NOT_EQUAL`.
- The fast path is gated by:
  - bool destination local can skip the value slot
  - left and right operands have i64 scalar locals
  - left and right operands are written before the current equality instruction
- The emitted expressions are:
  - `zr_aot_bD = (TZrBool)((zr_aot_sL == zr_aot_sR) != 0u);`
  - `zr_aot_bD = (TZrBool)((zr_aot_sL != zr_aot_sR) != 0u);`
- `backend_aot_c_scalar_locals.c` now treats the same plain generic equality/inequality shape as:
  - an i64 local consumer
  - a slot mention for both operands
  - a bool-result local write
- Dynamic or mixed primitive operands continue to use `GenericPrimitiveLogicalEqual/NotEqual`.

## Tooling Evidence

- RED:
  - `wsl.exe bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_bool_equality_local_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_bool_equality_local_smoke_test"`
  - Result: `2 Tests 1 Failures 0 Ignored`, failing the new i64 equality test with `Expected Non-NULL`.
- Focused GREEN:
  - Same WSL GCC focused command passed `2 Tests 0 Failures 0 Ignored`.
- Generated C confirmation:
  - `zr_aot_generic_i64_compare_scalar_local` is present.
  - `zr_aot_b2 = (TZrBool)((zr_aot_s0 == zr_aot_s1) != 0u);` is present.
  - `zr_aot_b3 = (TZrBool)((zr_aot_s0 != zr_aot_s4) != 0u);` is present.
  - Targeted runtime equality helper calls, bool sync calls, and operand `frame.slotBase[0/1/4].value` reads are absent.

## Regression Matrix

- WSL GCC passed:
  - generic bool equality local smoke 2/0
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
  - generic bool equality local smoke 2/0
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

Accepted for this slice. Proven plain generic i64 equality/inequality can now stay entirely in scalar locals and lower
to direct C comparisons. This is a partial 07-S2/S4 improvement only; broader 07~12 work remains active.
