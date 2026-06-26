# AOT 11-S3H metadata runtime signature header view

## Scope

- Plan slice: `docs/plans/aot/11-metadata.md` 11-S3 sub-slice.
- Goal: parse top-level method and field signature headers from validated
  runtime signature blob views.
- Implemented:
  - `SZrMetadataRuntimeSignatureView` stores the top-level root node, the
    original blob view, method calling convention, flags, generic parameter
    count, parameter count, return type blob offset, parameter list blob offset,
    and field type blob offset.
  - `ZrCore_MetadataRuntime_ReadSignatureView()` reads method and field
    signature headers on top of `ZrCore_MetadataRuntime_GetSignatureBlob()`.
  - Nested type nodes are skipped only to locate later top-level header fields.
  - Null runtime, null output view, and unattached runtime queries are rejected.

## RED

- `zr_vm_metadata_runtime_query_test` failed after adding
  `test_metadata_runtime_reads_method_and_field_signature_views`.
- The test referenced `SZrMetadataRuntimeSignatureView` and
  `ZrCore_MetadataRuntime_ReadSignatureView()`, neither of which existed.

## GREEN

- Added the signature view carrier to `metadata_runtime.h`.
- Added `ZrCore_MetadataRuntime_ReadSignatureView()` plus local signature
  header read/skip helpers in `metadata_runtime.c`.
- The focused proof builds a method signature with one primitive parameter and
  a field signature with one primitive field type, then verifies root kind,
  method counts, flags, and blob-relative offsets.

## Validation

- WSL gcc:
  - `zr_vm_metadata_runtime_query_test` 12/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
- WSL clang:
  - `zr_vm_metadata_runtime_query_test` 12/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
  - Existing generated-C warning remains in the generic conversion fixture:
    `!zr_aot_b2 != 0u`.
- Windows MSVC Debug:
  - `zr_vm_metadata_runtime_query_test` 12/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0, 8 ignored Unix-only branches
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0, 2 ignored Unix-only branches

## Notes

- This slice closes only method/field signature top-level header parsing.
- It does not implement nested signature AST construction, `FIELD_SIG` type
  entity parsing, TypeSpec or generic binding, runtime method/field/type entity
  materialization, token-layout mapping, generic dictionary lookup, or metadata
  policy.
