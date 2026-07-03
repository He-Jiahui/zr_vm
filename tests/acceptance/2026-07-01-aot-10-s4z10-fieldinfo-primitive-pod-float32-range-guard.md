# AOT 10-S4Z10 / 11-S4AV / 12-S5 Support - FieldInfo Primitive POD Float32 Range Guard

## Scope

- Slice: `10-S4Z10` / `11-S4AV` / `12-S5 support`
- Date: 2026-07-01 09:58:56 +08:00
- Goal: reject out-of-range float32 writes to primitive POD raw inline fields and preserve the original field bytes on failure.

## RED

- Added `test_reflection_rejects_out_of_range_field_info_primitive_pod_float32_writes`.
- Windows MSVC Debug focused run built and executed 18 tests, then failed the new test on the first `FLT_MAX * 2.0` write:
  - `Expected FALSE Was TRUE`
- The failure proved the previous raw primitive store path cast out-of-range double sources to float32 before copying bytes into inline storage.

## Implementation

- `zr_vm_core/src/zr_vm_core/reflection_field_value.c`
  - Added a float32 range check using `FLT_MAX`.
  - Rejected double source values greater than `FLT_MAX` or less than `-FLT_MAX` before the float32 cast and `memcpy`.
  - Kept this slice limited to float32 storage range guarding; NaN handling and precision narrowing policy remain later work.

## Test Coverage

- `tests/module/test_reflection_token_resolve.c`
  - Added a helper that attaches a same-runtime `FIELD_SIG(PRIMITIVE(FLOAT))` raw primitive FieldDef.
  - Attempted rejected writes for positive and negative double values outside the float32 finite range.
  - Asserted the raw float32 storage still decodes to the original `1.25f` after each failed write.

## Validation

- Windows MSVC Debug focused RED:
  - `zr_vm_reflection_token_resolve_test`: failed 1/18 after adding the test before production changes.
- Windows MSVC Debug focused GREEN:
  - `zr_vm_reflection_token_resolve_test`: 18/0
- WSL GCC:
  - `zr_vm_reflection_token_resolve_test`: 18/0
  - `zr_vm_metadata_runtime_query_test`: 24/0
  - `zr_vm_metadata_runtime_typespec_layout_test`: 17/0
- WSL Clang:
  - `zr_vm_reflection_token_resolve_test`: 18/0
  - `zr_vm_metadata_runtime_query_test`: 24/0
  - `zr_vm_metadata_runtime_typespec_layout_test`: 17/0
- Windows MSVC Debug:
  - `zr_vm_reflection_token_resolve_test`: 18/0
  - `zr_vm_metadata_runtime_query_test`: 24/0
  - `zr_vm_metadata_runtime_typespec_layout_test`: 17/0
- Focused CTest:
  - WSL GCC: `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 3/3
  - WSL Clang: `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 3/3
  - Windows MSVC Debug: `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 3/3

## Acceptance

- Out-of-range float32 writes fail before mutating raw inline storage.
- The slice does not add or change metadata ABI, zrp rows, code-registration fields, code-stripping roots, or pruning/remap behavior.
- Remaining work: float32 NaN/precision semantics, nested POD/struct field marshaling, object-level `FieldInfo.GetValue/SetValue`, cross-module provider binding, dataflow analysis, DESCRIPTION promotion, and complete metadata sweep.
