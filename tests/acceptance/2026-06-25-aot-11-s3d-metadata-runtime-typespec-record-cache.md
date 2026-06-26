# AOT 11-S3D metadata runtime TypeSpec record cache

## Scope

- Plan slice: `docs/plans/aot/11-metadata.md` 11-S3 sub-slice.
- Goal: include local `TYPE_SPEC` token records in
  `ZrCore_MetadataRuntime_ResolveTypeRecord()` lazy resolution.
- Implemented:
  - `TYPE_SPEC` is accepted as a type-record token.
  - Local TypeSpec records are resolved from the attached metadata function's
    `metadataTokenRecords`.
  - Repeated lookup of the same TypeSpec token returns the cached type-record
    pointer through the existing type-record cache.

## RED

- `zr_vm_metadata_runtime_query_test` failed after adding
  `test_metadata_runtime_resolves_type_spec_records_as_type_records`.
- The new test received null because `ResolveTypeRecord()` still rejected
  `TYPE_SPEC` tokens and only routed `TYPE_DEF` / `TYPE_REF` records.

## GREEN

- `metadata_runtime_is_type_record_token()` now includes `TYPE_SPEC`.
- `ZrCore_MetadataRuntime_ResolveTypeRecord()` resolves local TypeSpec records
  from the attached metadata function before caching the positive hit.
- The cache proof resolves a local TypeSpec record, mutates the backing token,
  and confirms a second lookup still returns the cached pointer.

## Validation

- WSL gcc:
  - `zr_vm_metadata_runtime_query_test` 8/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
- WSL clang:
  - `zr_vm_metadata_runtime_query_test` 8/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
  - Existing generated-C warning remains in the generic conversion fixture:
    `!zr_aot_b2 != 0u`.
- Windows MSVC Debug:
  - `zr_vm_metadata_runtime_query_test` 8/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0, 8 ignored Unix-only branches
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0, 2 ignored Unix-only branches

## Notes

- This slice closes only TypeSpec token-record lazy lookup plus a single-entry
  cache hit.
- It does not implement signature blob semantic parsing, generic instantiation
  binding, runtime type/layout entity materialization, field resolution, data
  metadata mmap lookup, token-layout mapping, generic dictionary lookup, or
  metadata policy.
