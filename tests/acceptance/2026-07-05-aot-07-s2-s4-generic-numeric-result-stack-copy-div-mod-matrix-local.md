# AOT 07-S2/S4 Generic Numeric Result Stack-Copy DIV/MOD Matrix Local

## Scope

- Plan slice: 07-S2/S4 value-copy migration for proven scalar locals.
- Affected layers: AOT C codegen test harness and shared-library smoke coverage.
- Focused shapes:
  - `GET_CONSTANT float -> GET_CONSTANT float -> ADD -> SET_STACK -> GET_CONSTANT float -> MOD -> RETURN`.
  - `GET_CONSTANT float -> GET_CONSTANT float -> ADD -> SET_STACK -> GET_CONSTANT float -> MOD(right copied result) -> RETURN`.
  - `GET_CONSTANT int -> GET_CONSTANT int -> ADD -> SET_STACK -> GET_CONSTANT int -> DIV -> RETURN`.
  - `GET_CONSTANT int -> GET_CONSTANT int -> ADD -> SET_STACK -> GET_CONSTANT int -> DIV(right copied result) -> RETURN`.
- Goal: complete the straight-line f64/i64 copied-result guarded `DIV`/`MOD` matrix so both left and right divisor/modulus positions stay on scalar locals without value-slot materialization or runtime fallback.

## Baseline

- Previous result-copy guard coverage had already covered f64 `DIV` and i64 `MOD` for left copied-result shapes, then f64 `DIV` and i64 `MOD` for right copied-result shapes.
- The remaining symmetric holes were f64 `MOD` left/right and i64 `DIV` left/right.
- Existing repository baseline remains unchanged: these shared-library smokes are focused Unix validation paths and are expected ignores on MSVC.

## Test Inventory

- Added `test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_mod_float_local`.
- Added `test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_right_mod_float_local`.
- Added `test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_div_signed_int_local`.
- Added `test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_right_div_signed_int_local`.
- Added shared helper coverage for guarded f64 and guarded signed i64 result-copy shapes.
- Boundary/failure-path coverage: each `DIV`/`MOD` case asserts the generated zero guard on the actual divisor/modulus local.
- Negative coverage: each case asserts the generated C avoids targeted `CopyStack`, `GenericNumericDiv`/`GenericNumericMod`, scalar sync boundaries, generic numeric binary boundaries, and copied-slot value materialization.

## Tooling Evidence

- WSL GCC command:
  `wsl.exe -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test"`.
- WSL Clang command:
  `wsl.exe -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && ./build-wsl-clang/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test"`.
- MSVC Debug command:
  `cmd.exe /c "call ""E:\Visual Studio\Common7\Tools\VsDevCmd.bat"" -arch=x64 && E:\Git\zr_vm\build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_numeric_shared_library_smoke_test.exe"`.
- The focused shared-library smoke was used because this slice validates generated C shape, generated shared-library compilation, and runtime return behavior for AOT output.

## Results

- WSL GCC focused generic numeric shared-library smoke: `50 Tests 0 Failures 0 Ignored`.
- WSL Clang focused generic numeric shared-library smoke: `50 Tests 0 Failures 0 Ignored`.
- MSVC Debug initially failed to compile the test file after the shared assertion helpers exposed `EZrInstructionCode` on the Windows path.
- The MSVC portability issue was fixed in test code by keeping the new assertion helpers under `#if defined(ZR_PLATFORM_UNIX)` and adding expected Windows ignore branches to the four new tests.
- MSVC Debug rerun built successfully and reported expected Unix-only ignores with 0 failures: `50 Tests 0 Failures 50 Ignored`.
- No production-code change was required for this matrix coverage slice.

## Acceptance Decision

- Accepted for the covered 07-S2/S4 straight-line f64/i64 copied-result guarded `DIV`/`MOD` matrix.
- The generated C proves the copied result remains a scalar local through zero-checked downstream arithmetic:
  - f64 left `MOD`: `if (zr_aot_f3 == (TZrFloat64)0.0)` and `zr_aot_f5 = fmod(zr_aot_f4, zr_aot_f3);`.
  - f64 right `MOD`: `if (zr_aot_f4 == (TZrFloat64)0.0)` and `zr_aot_f5 = fmod(zr_aot_f3, zr_aot_f4);`.
  - i64 left `DIV`: `if (zr_aot_s3 == (TZrInt64)0)` and `zr_aot_s5 = zr_aot_s4 / zr_aot_s3;`.
  - i64 right `DIV`: `if (zr_aot_s4 == (TZrInt64)0)` and `zr_aot_s5 = zr_aot_s3 / zr_aot_s4;`.
- Remaining 07 work: dynamic/unproven operands, broader value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and complete zero-frame typed bodies.
