# AOT 11-S3A metadata runtime method record cache

## Scope

- Plan slice: `docs/plans/aot/11-metadata.md` 11-S3 sub-slice.
- Goal: add the first token lazy-resolution API to `SZrMetadataRuntime` without
  implementing type/field/entity materialization yet.
- Implemented:
  - `SZrMetadataRuntime` now stores a single method-record cache.
  - `ZrCore_MetadataRuntime_ResolveMethodRecord()` resolves local `MEMBER_DEF`
    records from the attached metadata function's token records.
  - Imported `MEMBER_REF` records resolve from the attached metadata function's
    module metadata ref table.
  - Null runtime, zero token, and non-method token inputs are rejected.
  - Repeated lookup of the same token returns the cached record pointer.

## RED

- `zr_vm_metadata_runtime_query_test` failed to link after adding
  `test_metadata_runtime_resolves_method_records_lazily`, because
  `ZrCore_MetadataRuntime_ResolveMethodRecord()` did not exist.

## GREEN

- Added the resolver declaration to `metadata_runtime.h`.
- Added `metadata_runtime.c` and reused the existing function metadata token
  query helpers.
- Added `methodRecordCacheToken` and `methodRecordCache` to the runtime carrier.
- The focused cache proof resolves a local method record, mutates the backing
  token, and confirms a second lookup still returns the cached pointer.

## Validation

- WSL gcc:
  - `zr_vm_metadata_runtime_query_test` 5/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
- WSL clang:
  - `zr_vm_metadata_runtime_query_test` 5/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
  - Existing generated-C warning remains in the generic conversion fixture:
    `!zr_aot_b2 != 0u`.
- Windows MSVC Debug:
  - `zr_vm_metadata_runtime_query_test` 5/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0, 8 ignored Unix-only branches
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0, 2 ignored Unix-only branches

## Notes

- This slice closes only method token record lazy lookup plus a single-entry
  cache.
- It does not implement `ResolveType`, `ResolveField`, data metadata mmap
  lookup, signature semantic resolution, runtime entity materialization,
  token-layout mapping, generic dictionary lookup, or metadata policy.
