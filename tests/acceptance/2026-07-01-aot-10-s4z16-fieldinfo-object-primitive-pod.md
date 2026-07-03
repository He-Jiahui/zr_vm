# AOT 10-S4Z16 / 11-S4BB / 12-S5 FieldInfo Object Primitive POD

Timestamp: 2026-07-01 11:15:23 +08:00

Status: completed for this sub-slice.

Scope:
- The object-level FieldInfo value adapters are now covered against primitive POD raw inline fields, not only `VALUE_SLOT` storage.
- The fixture builds a public FieldInfo object for an int32 raw primitive field, reads through `ZrCore_Reflection_ReadFieldInfoObjectValue()`, rejects a type-mismatched write through `ZrCore_Reflection_WriteFieldInfoObjectValue()`, verifies the raw bytes stay unchanged, then writes and reads back a new int32 value.
- This is a coverage slice. It does not add new public API, nested field marshaling, metadata ABI changes, or new code-stripping root rules.

RED/GREEN:
- Coverage GREEN: the new object-level primitive POD fixture passed immediately because the S4Z14/S4Z15 object adapters already delegate to the token-driven primitive POD value path.
- Windows MSVC Debug focused run passed `zr_vm_reflection_token_resolve_test` 23/0.

Verification:
- WSL GCC direct: `zr_vm_reflection_token_resolve_test` 23/0, `zr_vm_metadata_runtime_query_test` 24/0, `zr_vm_metadata_runtime_typespec_layout_test` 17/0.
- WSL Clang direct: `zr_vm_reflection_token_resolve_test` 23/0, `zr_vm_metadata_runtime_query_test` 24/0, `zr_vm_metadata_runtime_typespec_layout_test` 17/0.
- Windows MSVC Debug direct: `zr_vm_reflection_token_resolve_test` 23/0, `zr_vm_metadata_runtime_query_test` 24/0, `zr_vm_metadata_runtime_typespec_layout_test` 17/0.
- Focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout`: WSL GCC 3/3, WSL Clang 3/3, Windows MSVC Debug 3/3.

Open:
- Full managed `FieldInfo.SetValue` method surface.
- Nested POD/struct field marshaling.
- Cross-module provider loading/version compatibility.
- `@dynamically_accessed` dataflow, DESCRIPTION promotion, complete trim analyzer, and full metadata sweep/pruning.
