# AOT 07-S2/S4 Generic LOGICAL_NOT Numeric Source Local Branch

Timestamp: 2026-06-29 10:25:39 +08:00

## Scope

- Milestone: M1.5 / 07-S2/S4 partial.
- Goal: lower generic `LOGICAL_NOT` for proven i64/u64/f64 scalar-local sources directly into bool destination locals when the destination is immediately consumed by `JUMP_IF_BOOL_FALSE`.
- Non-goals: call-result truthiness execution proof, string/object/dynamic truthiness, value-copy migration, GC roots/exports/frame cleanup, full zero-frame acceptance, and performance gates.

## RED

- Added `tests/parser/test_aot_c_generic_logical_not_numeric_local_smoke.c` and CMake target `zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test`.
- Initial WSL GCC run built the new target but failed with `Expected Non-NULL` because generated C lacked `zr_aot_generic_logical_not_i64_scalar_local` and still emitted `GenericPrimitiveLogicalNot` for numeric sources.

## Implementation

- `backend_aot_c_write_generic_logical_not_scalar_local()` now supports bool, i64, u64, and f64 source locals.
- Numeric paths emit zero predicates into bool locals:
  - `zr_aot_bD = (TZrBool)(zr_aot_sS == (TZrInt64)0);`
  - `zr_aot_bD = (TZrBool)(zr_aot_uS == (TZrUInt64)0u);`
  - `zr_aot_bD = (TZrBool)(zr_aot_fS == (TZrFloat64)0.0);`
- Scalar-local declaration and bool-value write tracking accept only primitive bool/i64/u64/f64 sources for this generic `LOGICAL_NOT` local path.
- Logical source contracts lock the new markers and written-before gates.

## Validation

- WSL GCC: logical contracts 4/0, new generic LOGICAL_NOT numeric local smoke 1/0, logical shared-library smoke 6/0, generic JUMP_IF smoke 3/0, generic bool equality smoke 1/0, control contracts 2/0, frame setup contracts 1/0.
- WSL Clang: same target set passed with the same counts.
- Windows MSVC Debug: logical/control/frame contracts passed 4/0, 2/0, 1/0; Unix-only new smoke reported 0 failures / 1 ignored, logical shared-library smoke 0 failures / 6 ignored, generic JUMP_IF smoke 0 failures / 3 ignored, generic bool equality smoke 0 failures / 1 ignored.
- Diff whitespace check passed for touched tracked files with only LF/CRLF conversion warnings.

## Acceptance

Accepted as a focused 07-S2/S4 support slice. This does not complete 07-S2/S4 or M1.5.
