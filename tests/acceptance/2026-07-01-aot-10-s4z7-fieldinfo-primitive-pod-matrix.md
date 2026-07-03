# AOT 10-S4Z7 / 11-S4AS / 12-S5 Support - FieldInfo Primitive POD Matrix

## Scope

- Slice: `10-S4Z7` / `11-S4AS` / `12-S5 support`
- Date: 2026-07-01 05:06:21 +08:00
- Goal: prove the `FieldInfo` token value boundary covers representative primitive POD raw inline fields beyond the initial int32 path.

## Baseline

- `10-S4Z6` moved `FieldInfo` token value read/write into `zr_vm_core/src/zr_vm_core/reflection_field_value.c`.
- That implementation already maps `FIELD_SIG(PRIMITIVE(...))` to raw primitive byte sizes and load/store families.
- This slice intentionally adds focused matrix coverage first. The new matrix passed immediately, so no production code change was needed.

## Test Coverage

- `tests/module/test_reflection_token_resolve.c`
  - Added a shared raw primitive FieldDef fixture helper.
  - Added `test_reflection_reads_and_writes_field_info_primitive_pod_matrix`.
  - Covered representative raw inline fields:
    - `BOOL`: read true, reject int write, write false.
    - `UINT32`: read `0xFEDC1234` as `UINT64`, reject bool write, write `0xAABBCCDD`.
    - `DOUBLE`: read `6.25` as `DOUBLE`, reject bool write, write `-12.5`.

## Validation

- Windows MSVC Debug focused build/run:
  - `zr_vm_reflection_token_resolve_test`: 15/0
- WSL GCC:
  - `zr_vm_reflection_token_resolve_test`: 15/0
  - `zr_vm_metadata_runtime_query_test`: 24/0
  - `zr_vm_metadata_runtime_typespec_layout_test`: 17/0
- WSL Clang:
  - `zr_vm_reflection_token_resolve_test`: 15/0
  - `zr_vm_metadata_runtime_query_test`: 24/0
  - `zr_vm_metadata_runtime_typespec_layout_test`: 17/0
- Windows MSVC Debug:
  - `zr_vm_reflection_token_resolve_test`: 15/0
  - `zr_vm_metadata_runtime_query_test`: 24/0
  - `zr_vm_metadata_runtime_typespec_layout_test`: 17/0
- Focused CTest:
  - WSL GCC: `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 3/3
  - WSL Clang: `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 3/3
  - Windows MSVC Debug: `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 3/3

## Acceptance

- The same public APIs, `ZrCore_Reflection_ReadFieldInfoTokenValue()` and `ZrCore_Reflection_WriteFieldInfoTokenValue()`, now have focused regression coverage for signed, unsigned, bool, and floating primitive raw field families.
- The slice does not add or change metadata ABI, zrp rows, code-registration fields, code-stripping roots, or pruning/remap behavior.
- Remaining work: full primitive width/overflow matrix, nested POD/struct field marshaling, object-level `FieldInfo.GetValue/SetValue`, cross-module provider binding, dataflow analysis, DESCRIPTION promotion, and complete metadata sweep.
