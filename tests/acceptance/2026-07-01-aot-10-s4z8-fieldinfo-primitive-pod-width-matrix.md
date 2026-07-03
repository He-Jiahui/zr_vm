# AOT 10-S4Z8 / 11-S4AT / 12-S5 Support - FieldInfo Primitive POD Width Matrix

## Scope

- Slice: `10-S4Z8` / `11-S4AT` / `12-S5 support`
- Date: 2026-07-01 09:35:02 +08:00
- Goal: prove the `FieldInfo` token value boundary covers the remaining primitive POD storage widths after the representative bool/uint32/double matrix.

## Baseline

- `10-S4Z6` added the generic raw primitive read/write path in `zr_vm_core/src/zr_vm_core/reflection_field_value.c`.
- `10-S4Z7` proved representative primitive families: bool, uint32, and double.
- This slice adds full storage-width coverage for the primitive scalar widths that were still missing. The matrix passed without production changes.

## Test Coverage

- `tests/module/test_reflection_token_resolve.c`
  - Added signed raw primitive width helpers for int8, int16, and int64.
  - Added unsigned raw primitive width helpers for uint8, uint16, and uint64.
  - Added float32 raw primitive coverage through the public read/write value boundary.
  - Added `test_reflection_reads_and_writes_field_info_primitive_pod_width_matrix`.
  - Covered raw inline fields:
    - `INT8`: read `-12`, write `42`.
    - `INT16`: read `-1234`, write `2345`.
    - `INT64`: read `-1234567890123`, write `987654321012`.
    - `UINT8`: read `0xAB`, write `0x7F`.
    - `UINT16`: read `0xABCD`, write `0x1357`.
    - `UINT64`: read `0xFEDCBA9876543210`, write `0x1122334455667788`.
    - `FLOAT32`: read `1.25` as `DOUBLE`, write `-3.5` through `ZrCore_Value_InitAsFloat`.

## Validation

- Windows MSVC Debug focused build/run:
  - `zr_vm_reflection_token_resolve_test`: 16/0
- WSL GCC:
  - `zr_vm_reflection_token_resolve_test`: 16/0
  - `zr_vm_metadata_runtime_query_test`: 24/0
  - `zr_vm_metadata_runtime_typespec_layout_test`: 17/0
- WSL Clang:
  - `zr_vm_reflection_token_resolve_test`: 16/0
  - `zr_vm_metadata_runtime_query_test`: 24/0
  - `zr_vm_metadata_runtime_typespec_layout_test`: 17/0
- Windows MSVC Debug:
  - `zr_vm_reflection_token_resolve_test`: 16/0
  - `zr_vm_metadata_runtime_query_test`: 24/0
  - `zr_vm_metadata_runtime_typespec_layout_test`: 17/0
- Focused CTest:
  - WSL GCC: `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 3/3
  - WSL Clang: `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 3/3
  - Windows MSVC Debug: `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 3/3

## Acceptance

- The same public APIs, `ZrCore_Reflection_ReadFieldInfoTokenValue()` and `ZrCore_Reflection_WriteFieldInfoTokenValue()`, now have regression coverage for the supported primitive POD storage widths used by the raw field value path.
- The slice does not add or change metadata ABI, zrp rows, code-registration fields, code-stripping roots, or pruning/remap behavior.
- Remaining work: numeric overflow/range semantics, nested POD/struct field marshaling, object-level `FieldInfo.GetValue/SetValue`, cross-module provider binding, dataflow analysis, DESCRIPTION promotion, and complete metadata sweep.
