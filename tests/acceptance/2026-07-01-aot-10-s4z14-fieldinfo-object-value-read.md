# AOT 10-S4Z14 / 11-S4AZ / 12-S5 FieldInfo Object Value Read

Timestamp: 2026-07-01 10:52:48 +08:00

Status: completed for this sub-slice.

Scope:
- `ZrCore_Reflection_ReadFieldInfoObjectValue()` now reads a minimum public FieldInfo object as the carrier for FieldDef value access.
- The adapter extracts `metadataRuntime` and `metadataToken` from the FieldInfo object, validates their shape, and delegates to the existing token-driven inline value reader.
- This slice is read-only and does not implement object-level `SetValue`, nested field marshaling, metadata ABI changes, or new code-stripping root rules.

RED:
- Added `test_reflection_reads_field_info_object_value_from_inline_storage()`.
- Windows MSVC Debug build failed with an unresolved external for `ZrCore_Reflection_ReadFieldInfoObjectValue`, proving the object-level read API did not exist.

GREEN:
- Added the public object-level read API in `reflection.h`.
- Implemented the adapter in `reflection_field_value.c`, including native-call pinning while reading the FieldInfo object fields.
- Windows MSVC Debug focused run passed `zr_vm_reflection_token_resolve_test` 21/0.

Verification:
- WSL GCC direct: `zr_vm_reflection_token_resolve_test` 21/0, `zr_vm_metadata_runtime_query_test` 24/0, `zr_vm_metadata_runtime_typespec_layout_test` 17/0.
- WSL Clang direct: `zr_vm_reflection_token_resolve_test` 21/0, `zr_vm_metadata_runtime_query_test` 24/0, `zr_vm_metadata_runtime_typespec_layout_test` 17/0.
- Windows MSVC Debug direct: `zr_vm_reflection_token_resolve_test` 21/0, `zr_vm_metadata_runtime_query_test` 24/0, `zr_vm_metadata_runtime_typespec_layout_test` 17/0.
- Focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout`: WSL GCC 3/3, WSL Clang 3/3, Windows MSVC Debug 3/3.

Open:
- Object-level `FieldInfo.SetValue` and full FieldInfo method surface.
- Nested POD/struct field marshaling.
- Cross-module provider loading/version compatibility.
- `@dynamically_accessed` dataflow, DESCRIPTION promotion, complete trim analyzer, and full metadata sweep/pruning.
