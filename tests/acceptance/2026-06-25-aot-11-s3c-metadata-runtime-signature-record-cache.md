# AOT 11-S3C metadata runtime signature record cache

## Scope

- Plan slice: `docs/plans/aot/11-metadata.md` 11-S3 sub-slice.
- Goal: add entity token to `SIGNATURE` record lazy resolution to
  `SZrMetadataRuntime`.
- Implemented:
  - `SZrMetadataRuntime` now stores a single signature-record cache.
  - `ZrCore_MetadataRuntime_ResolveSignatureRecord()` resolves a local
    signature record from the attached metadata function's token records.
  - If no local signature record exists, the resolver checks the attached
    metadata function's module metadata ref table.
  - Null runtime, zero token, and direct `SIGNATURE` token inputs are rejected.
  - Repeated lookup of the same entity token returns the cached signature record
    pointer.

## RED

- `zr_vm_metadata_runtime_query_test` failed to link after adding
  `test_metadata_runtime_resolves_signature_records_lazily`, because
  `ZrCore_MetadataRuntime_ResolveSignatureRecord()` did not exist.

## GREEN

- Added the resolver declaration to `metadata_runtime.h`.
- Added `signatureRecordCacheEntityToken` and `signatureRecordCache` to the
  runtime carrier.
- Reused the existing function/module signature-record query helpers.
- The focused cache proof resolves a local signature record, mutates the backing
  token, and confirms a second lookup still returns the cached pointer.

## Validation

- WSL gcc:
  - `zr_vm_metadata_runtime_query_test` 7/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
- WSL clang:
  - `zr_vm_metadata_runtime_query_test` 7/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
  - Existing generated-C warning remains in the generic conversion fixture:
    `!zr_aot_b2 != 0u`.
- Windows MSVC Debug:
  - `zr_vm_metadata_runtime_query_test` 7/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0, 8 ignored Unix-only branches
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0, 2 ignored Unix-only branches

## Notes

- This slice closes only entity token to `SIGNATURE` record lazy lookup plus a
  single-entry cache.
- It does not implement field resolution, TypeSpec handling, data metadata mmap
  lookup, signature blob semantic parsing, runtime entity materialization,
  token-layout mapping, generic dictionary lookup, or metadata policy.
