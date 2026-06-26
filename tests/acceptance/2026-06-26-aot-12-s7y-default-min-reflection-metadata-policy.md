# 2026-06-26 AOT 12-S7Y default-min reflection metadata policy

## Scope

- Connected generated MethodInfo reflection metadata level to the current code-stripping policy.
- Default and non-stripped AOT C output keeps `ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING`.
- Opt-in `enableCodeStripping` output now emits `ZR_AOT_REFLECTION_METADATA_NONE`.
- Generated C now reports the chosen policy as `/* metadata_policy.reflectionLevel = ... */`.

## RED

- Extended `zr_vm_aot_c_code_stripping_test` so the ordinary stripped fixture must contain `metadata_policy.reflectionLevel = 0`.
- Required stripped MethodInfo initializers to use `ZR_AOT_REFLECTION_METADATA_NONE`.
- Required stripped output to omit `ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING`.
- The first focused WSL gcc run failed 1/4 on the new expectations.

## GREEN

- Added `backend_aot_option_reflection_metadata_level(...)` as the shared writer-policy helper.
- Threaded the selected level through AOT C emission and method metadata emission.
- Method metadata byte sampling uses the same level, so emitted byte totals still match the generated MethodInfo block.

## Validation

- WSL gcc:
  - `zr_vm_aot_c_code_stripping_test`: 4/0.
  - `zr_vm_aot_c_source_contracts_test`: 21/0.
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1/0.
  - `zr_vm_aot_c_typed_scalar_test`: 1/0.
  - `zr_vm_aot_c_shared_library_smoke_test`: 8/0.
  - Focused CTest `aot_c_code_stripping|aot_c_source_contracts|aot_c_frame_setup_contracts|aot_c_typed_scalar|aot_c_shared_library_smoke`: 2/2 matched registered tests.
- WSL clang:
  - Same direct test set: 4/0, 21/0, 1/0, 1/0, 8/0.
  - Focused CTest: 2/2 matched registered tests.
- Windows MSVC Debug:
  - `zr_vm_aot_c_code_stripping_test`: 4/0.
  - `zr_vm_aot_c_source_contracts_test`: 21/0.
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1/0.
  - `zr_vm_aot_c_typed_scalar_test`: 1 test, 0 failures, 1 ignored by existing Unix shared-library guard.
  - `zr_vm_aot_c_shared_library_smoke_test`: 8 tests, 0 failures, 8 ignored by existing Unix shared-library guards.
  - Focused CTest: 2/2 matched registered tests.

## Notes

- This slice implements generated MethodInfo default-min policy only.
- It does not rewrite or prune embedded zrp metadata pools.
- It does not implement annotation-driven `DESCRIPTION` promotion or trim analyzer warning suppression.
