# AOT 07-S2/S4 Generic Call-Result Equality + U64 Reset Thunk

## Scope

- Slice: AOT 07-S2/S4, no-argument typed u64/f64 call results consumed by generic equality/inequality.
- Goal: keep provable u64/f64 call results in scalar locals through generic equality, and make reset-tail u64
  constant-return helpers eligible for direct typed no-arg thunks.
- Non-goals: mixed primitive equality direct lowering, dynamic/string/object truthiness, value-copy migration,
  GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and complete zero-frame typed bodies.

## Baseline

- RED added `generic_call_result_equality_local_project` to
  `tests/parser/test_aot_c_generic_bool_equality_local_smoke.c`.
- The fixture compiles helpers returning:
  - `uint`: `unsignedFortyTwo()`, `unsignedAlsoFortyTwo()`, `unsignedSeven()`
  - `float`: `floatTwoPointFive()`, `floatAlsoTwoPointFive()`, `floatThreePointFive()`
- The test rewrites four typed unsigned/float equality instructions to generic
  `LOGICAL_EQUAL` / `LOGICAL_NOT_EQUAL`, executes the generated shared library, and returns 17 on success.
- Generated-C assertions require:
  - u64/f64 no-arg static direct-call markers
  - `static TZrUInt64 zr_aot_typed_u64_fn_3(void);`
  - `return (TZrUInt64)7;`
  - `zr_aot_generic_u64_compare_scalar_local`
  - `zr_aot_generic_f64_compare_scalar_local`
- Generated-C assertions forbid targeted fallback markers:
  - `direct_static_function_call_sync_bool`
  - `generic_logical_sync_bool`
  - `GenericPrimitiveLogicalEqual` / `GenericPrimitiveLogicalNotEqual`
  - `SZrTypeValue *zr_aot_typed_destination`
  - `ZR_VALUE_FAST_SET(zr_aot_typed_destination,`

## Implementation

- `backend_aot_c_typed_u64_thunks.c` now treats `RESET_STACK_NULL` after `GET_CONSTANT` as a no-op for u64
  constant-return thunk recognition when the reset slot is not the constant return slot.
- The accepted u64 no-arg shape now matches the f64 reset-tail handling:
  - `GET_CONSTANT`
  - `RESET_STACK_NULL` for a non-return slot
  - `TO_UINT`
  - `FUNCTION_RETURN`
- This allows `unsignedSeven()` to emit a direct typed thunk returning `(TZrUInt64)7`.
- Existing generic equality scalar-local lowering then handles the u64/f64 call-result operands through the same
  proven local-compare path used by local constants.

## Tooling Evidence

- RED:
  - WSL GCC focused generic equality smoke failed after adding the call-result equality fixture because the generated C
    lacked the reset-tail u64 typed thunk and fell back to direct static call sync.
- Focused GREEN:
  - `wsl -e bash -lc 'cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_bool_equality_local_smoke_test'`
  - Result: `4 Tests 0 Failures 0 Ignored`.
- Generated C confirmation:
  - Required u64 thunk and u64/f64 compare markers are present.
  - Direct static call sync, generic logical sync, generic equality helpers, and typed-destination materialization
    markers are absent in the focused generated project.

## Regression Matrix

- WSL GCC passed:
  - generic bool equality local smoke 4/0
  - logical shared-library smoke 6/0
  - typed direct-call u64 25/0
  - call shared-library smoke 5/0
- WSL clang passed the same matrix:
  - generic bool equality local smoke 4/0
  - logical shared-library smoke 6/0
  - typed direct-call u64 25/0
  - call shared-library smoke 5/0
- Windows MSVC Debug:
  - Built `zr_vm_aot_c_generic_bool_equality_local_smoke_test`.
  - Ran the test binary: `4 Tests 0 Failures 4 Ignored`.
- Patch checks:
  - `git diff --check -- tests/parser/test_aot_c_generic_bool_equality_local_smoke.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_thunks.c`
  - Result: no whitespace errors; only LF/CRLF normalization warnings.

## Acceptance Decision

Accepted for this slice. Proven no-argument typed u64/f64 call results can now flow through generic equality as
scalar-local direct comparisons, and reset-tail u64 constant-return helpers are eligible for typed no-arg thunks. This
is a partial 07-S2/S4 improvement only; broader 07~12 work remains active.
