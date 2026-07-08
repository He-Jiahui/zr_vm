# AOT 07-S3/S4 Generic Equality Zero-Frame Guardrail

Timestamp: 2026-07-06 01:13:37 +08:00

## Scope

- Added a generated-C zero-frame guardrail for pure primitive generic equality fixtures in
  `tests/parser/test_aot_c_generic_bool_equality_local_smoke.c`.
- Covered bool, i64, u64/f64, and mixed primitive equality local-branch fixtures.
- Deliberately left call-result equality and direct-call arithmetic sync bodies out of this S3/S4 guardrail;
  those still involve boundary/value-copy work planned under later 07 slices.

## Assertions

- `.registerFrameBytes = 0u`
- `value SemIR lowering frameByteSize=0`
- No `/* zr_aot_generated_frame_setup */`
- No `ZrAotGeneratedFrame frame = {0};`
- No `frame.slotBase`
- No `ZrCore_Function_CheckStackAndGc(`
- No `ZrCore_Value_ResetAsNull(&zr_aot_slot_base`

## Validation

- WSL GCC:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_bool_equality_local_smoke_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_bool_equality_local_smoke_test`
  - Result: 5 tests, 0 failures, 0 ignored.
- WSL Clang:
  `cmake --build build-wsl-clang --target zr_vm_aot_c_generic_bool_equality_local_smoke_test -j 1 && ./build-wsl-clang/bin/zr_vm_aot_c_generic_bool_equality_local_smoke_test`
  - Result: 5 tests, 0 failures, 0 ignored.
- MSVC Debug:
  `cmake --build E:\Git\zr_vm\build-msvc-aot-stack-copy --config Debug --target zr_vm_aot_c_generic_bool_equality_local_smoke_test --parallel 1 && E:\Git\zr_vm\build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_bool_equality_local_smoke_test.exe`
  - Result: 5 tests, 0 failures, 5 expected Unix-only ignores.
