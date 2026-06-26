# AOT 11-S3E metadata runtime field record cache

## Scope

- Plan slice: `docs/plans/aot/11-metadata.md` 11-S3 sub-slice.
- Goal: add field-token record lazy resolution to `SZrMetadataRuntime`.
- Implemented:
  - `SZrMetadataRuntime` now stores a single field-record cache.
  - `ZrCore_MetadataRuntime_ResolveFieldRecord()` resolves local field records
    from the attached metadata function's `MEMBER_DEF` token records.
  - Imported field records are resolved from the attached metadata function's
    module `MEMBER_REF` token records.
  - The field-record cache is independent from the method-record cache.
  - Null runtime, zero token, and non-member tokens are rejected.

## RED

- `zr_vm_metadata_runtime_query_test` failed after adding
  `test_metadata_runtime_resolves_field_records_with_independent_cache`.
- The test linked against `ZrCore_MetadataRuntime_ResolveFieldRecord()`, which
  did not exist.

## GREEN

- Added field cache members to `SZrMetadataRuntime`.
- Added `ZrCore_MetadataRuntime_ResolveFieldRecord()` to `metadata_runtime.h`
  and `metadata_runtime.c`.
- The focused cache proof resolves a local field record, performs a method
  lookup to overwrite the method cache, mutates the backing field token, and
  confirms the second field lookup still returns the cached field pointer.

## Validation

- WSL gcc:
  - `zr_vm_metadata_runtime_query_test` 9/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
- WSL clang:
  - `zr_vm_metadata_runtime_query_test` 9/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
  - Existing generated-C warning remains in the generic conversion fixture:
    `!zr_aot_b2 != 0u`.
- Windows MSVC Debug:
  - `zr_vm_metadata_runtime_query_test` 9/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0, 8 ignored Unix-only branches
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0, 2 ignored Unix-only branches

## Notes

- This slice closes only field token-record lazy lookup plus a single-entry
  cache hit.
- Current field definitions/references share `MEMBER_DEF` / `MEMBER_REF` token
  tables with methods, so this slice intentionally does not distinguish method
  and field semantics.
- It does not implement `FIELD_SIG` blob parsing, data metadata mmap lookup,
  runtime field entity materialization, token-layout mapping, generic
  dictionary lookup, or metadata policy.
