# AOT 07-S2/S4 signed-const scalar reset frame elision

Time: 2026-06-28 06:07:24 +08:00

Status: sub-slice complete; 07-S2/S4 partially complete; 07/M1.5 remains in progress.

## Scope

- Productize the signed `int` right-constant scalar arithmetic path as a frame-free generated-C smoke.
- Keep the existing scalar SemIR lowering markers (`zr_aot_scalar_exec_i64_binary`) and close the residual frame caused by reset liveness.
- Record that broader 07 frame-free completion remains open.

## RED

- Added `test_aot_c_generated_shared_library_elides_frame_for_signed_const_scalar_pipeline`.
- The first expectation for a legacy signed-const marker did not match current generated C, which already lowered arithmetic through `zr_aot_scalar_exec_i64_binary`.
- After aligning the test to the current lowering marker, the old implementation still failed because generated C contained:
  - `ZrAotGeneratedFrame frame = {0};`
  - `/* zr_aot_generated_frame_setup */`
  - `ZrLibrary_AotRuntime_ResetStackNull2(state, &frame, 2, 3)`
  - `ZrLibrary_AotRuntime_ResetStackNull(state, &frame, 4)`
  - `ZrLibrary_AotRuntime_ResetStackNull(state, &frame, 5)`

## Implementation

- Updated `backend_aot_c_scalar_locals_instruction_reads_slot_as_any_local()` to dispatch only through the scalar reader family that matches the opcode.
- Updated generic operand/source mention helpers so signed local consumers use `backend_aot_c_scalar_locals_signed_consumer_reads_slot()` instead of treating signed-const constant-pool operands as generic slots.
- Added source-contract checks covering the signed-const read-slot path and the family-specific reader dispatch.
- Added a Unix shared-library smoke that compiles and executes:

```zr
var seed: int = 10;
var plus: int = seed + 5;
var minus: int = plus - 3;
var scaled: int = minus * 2;
var ratio: int = scaled / 4;
var remainder: int = ratio % 5;
return remainder + 41;
```

## GREEN

- WSL gcc:
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1 tests, 0 failures.
  - `zr_vm_aot_c_shared_library_smoke_test`: 9 tests, 0 failures.
- WSL clang:
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1 tests, 0 failures.
  - `zr_vm_aot_c_shared_library_smoke_test`: 9 tests, 0 failures.
- Windows MSVC Debug:
  - `zr_vm_aot_c_frame_setup_contracts_test.exe`: 1 tests, 0 failures.
  - `zr_vm_aot_c_shared_library_smoke_test.exe`: 9 tests, 0 failures, 9 ignored Unix-only.
- `ctest -R "aot_c_(frame_setup|shared_library)"` found no registered tests in the three build trees; the direct binaries above are the authoritative focused validation.
- `git diff --check` passed for touched files, with only LF/CRLF line-ending notices.

## Generated C Evidence

`build-wsl-gcc/tests_generated/aot_c_shared_library/signed_const_scalar_frame_elision_project/bin/aot_c/src/main.c` contains:

- `zr_aot_scalar_exec_i64_binary`
- `zr_aot_direct_return_i64_local`
- `zr_aot_reset_stack_null_scalar_local_skip slot=1`
- `zr_aot_reset_stack_null2_scalar_local_skip slots=2,3`
- `zr_aot_reset_stack_null_scalar_local_skip slot=4`
- `zr_aot_reset_stack_null_scalar_local_skip slot=5`

The same generated file omits `ZrAotGeneratedFrame frame`, `/* zr_aot_generated_frame_setup */`, `ZrCore_Stack_GetValue(`, and `ZR_VALUE_FAST_SET(`.

## Remaining Work

- This slice does not claim full 07-S2 or 07-S4 completion.
- Unsigned/f64 const/reset, load-const/load-stack const, dynamic/generic/string boundaries, GC roots/exports/frame cleanup, wider byte-frame narrowing, and performance counters remain later 07 work.
