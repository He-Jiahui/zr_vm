# AOT 11-S3G metadata runtime signature blob view

## Scope

- Plan slice: `docs/plans/aot/11-metadata.md` 11-S3 sub-slice.
- Goal: connect entity-token signature records to validated zrp signature blob
  pool slices.
- Implemented:
  - `ZrCore_MetadataRuntime_GetSignatureBlob()` resolves the entity token's
    paired `SIGNATURE` record through the existing runtime signature resolver.
  - The runtime requires attached zrp metadata before reading the signature blob
    pool.
  - The returned `SZrZrpMetadataPoolSliceView` points at the bounded pool slice
    described by the signature record's offset and length.
  - The slice is validated through `ZrCore_ZrpMetadata_ValidateSignatureBlob()`.
  - Failed queries clear the output slice.

## RED

- `zr_vm_metadata_runtime_query_test` failed after adding
  `test_metadata_runtime_gets_validated_signature_blob_view`.
- The test linked against `ZrCore_MetadataRuntime_GetSignatureBlob()`, which did
  not exist yet.

## GREEN

- Added the public declaration to `metadata_runtime.h`.
- Added `ZrCore_MetadataRuntime_GetSignatureBlob()` to `metadata_runtime.c`.
- The focused proof creates a valid method signature blob in the zrp signature
  blob pool, resolves it through `TEST_MEMBER_DEF_TOKEN`, verifies the returned
  payload pointer/length, then mutates the signature record length to prove a
  truncated blob is rejected and clears the output view.

## Validation

- WSL gcc:
  - `zr_vm_metadata_runtime_query_test` 11/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
- WSL clang:
  - `zr_vm_metadata_runtime_query_test` 11/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
  - Existing generated-C warning remains in the generic conversion fixture:
    `!zr_aot_b2 != 0u`.
- Windows MSVC Debug:
  - `zr_vm_metadata_runtime_query_test` 11/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0, 8 ignored Unix-only branches
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0, 2 ignored Unix-only branches

## Notes

- This slice closes only entity token to validated signature blob pool slice
  lookup.
- It does not implement signature AST construction, method/field/type signature
  semantic parsing, `FIELD_SIG` discrimination, TypeSpec or generic binding,
  runtime entity materialization, token-layout mapping, generic dictionary
  lookup, or metadata policy.
