# AOT 10-S4Z9 / 11-S4AU / 12-S5 Support - FieldInfo Primitive POD Integer Range Guard

## Scope

- Slice: `10-S4Z9` / `11-S4AU` / `12-S5 support`
- Date: 2026-07-01 09:49:10 +08:00
- Goal: reject out-of-range integer writes to primitive POD raw inline fields and preserve the original field bytes on failure.

## RED

- Added `test_reflection_rejects_out_of_range_field_info_primitive_pod_integer_writes`.
- Windows MSVC Debug focused run built and executed 17 tests, then failed the new test at the first int8 overflow case:
  - `Expected FALSE Was TRUE`
- The failure proved the previous raw primitive store path truncated out-of-range integer values before copying bytes into inline storage.

## Implementation

- `zr_vm_core/src/zr_vm_core/reflection_field_value.c`
  - Added signed min/max lookup by primitive storage width.
  - Added unsigned max lookup by primitive storage width.
  - Rejected unsigned-to-signed writes when the source value exceeds the signed target max.
  - Rejected signed-to-unsigned writes when the source value is negative.
  - Rejected signed or unsigned writes that exceed the target storage width before any `memcpy`.

## Test Coverage

- `tests/module/test_reflection_token_resolve.c`
  - Added helpers that attach a same-runtime raw primitive FieldDef, attempt a rejected write, then assert the raw bytes still decode to the original value.
  - Covered:
    - `INT8`: reject `128`, `-129`, and unsigned `128`.
    - `UINT8`: reject signed `-1` and unsigned `256`.
    - `INT64`: reject unsigned `INT64_MAX + 1`.

## Validation

- Windows MSVC Debug focused RED:
  - `zr_vm_reflection_token_resolve_test`: 16/1 after adding the test before production changes.
- Windows MSVC Debug focused GREEN:
  - `zr_vm_reflection_token_resolve_test`: 17/0
- WSL GCC:
  - `zr_vm_reflection_token_resolve_test`: 17/0
  - `zr_vm_metadata_runtime_query_test`: 24/0
  - `zr_vm_metadata_runtime_typespec_layout_test`: 17/0
- WSL Clang:
  - `zr_vm_reflection_token_resolve_test`: 17/0
  - `zr_vm_metadata_runtime_query_test`: 24/0
  - `zr_vm_metadata_runtime_typespec_layout_test`: 17/0
- Windows MSVC Debug:
  - `zr_vm_reflection_token_resolve_test`: 17/0
  - `zr_vm_metadata_runtime_query_test`: 24/0
  - `zr_vm_metadata_runtime_typespec_layout_test`: 17/0
- Focused CTest:
  - WSL GCC: `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 3/3
  - WSL Clang: `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 3/3
  - Windows MSVC Debug: `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 3/3

## Acceptance

- Out-of-range integer writes fail before mutating raw inline storage.
- The slice does not add or change metadata ABI, zrp rows, code-registration fields, code-stripping roots, or pruning/remap behavior.
- Remaining work: float32 narrowing/finite semantics, nested POD/struct field marshaling, object-level `FieldInfo.GetValue/SetValue`, cross-module provider binding, dataflow analysis, DESCRIPTION promotion, and complete metadata sweep.
