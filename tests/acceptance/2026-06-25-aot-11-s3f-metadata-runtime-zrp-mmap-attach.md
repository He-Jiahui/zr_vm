# AOT 11-S3F metadata runtime zrp mmap attach

## Scope

- Plan slice: `docs/plans/aot/11-metadata.md` 11-S3 sub-slice.
- Goal: attach validated zrp data metadata to `SZrMetadataRuntime` and expose
  raw mmap section views.
- Implemented:
  - `SZrMetadataRuntime` now stores `hasZrpMetadata`, the zrp metadata buffer,
    buffer length, and a validated header copy.
  - `ZrCore_MetadataRuntime_AttachZrpMetadata()` validates the header and
    definition-table directory before attaching the buffer.
  - `ZrCore_MetadataRuntime_GetZrpSectionView()` returns read-only section views
    from the attached runtime header.
  - Null runtime, null buffer, short header, invalid header, and unattached
    runtime queries are rejected.

## RED

- `zr_vm_metadata_runtime_query_test` failed after adding
  `test_metadata_runtime_attaches_zrp_metadata_view`.
- The test referenced runtime zrp metadata fields and attach/view APIs that did
  not exist yet.
- The first implementation also exposed a missing `zr_vm_core/memory.h` include
  for `ZrCore_Memory_RawSet()`.

## GREEN

- Added zrp metadata attachment fields to `SZrMetadataRuntime`.
- Added `ZrCore_MetadataRuntime_AttachZrpMetadata()` and
  `ZrCore_MetadataRuntime_GetZrpSectionView()`.
- Included the core memory header in `metadata_runtime.c` for the existing
  zero-fill helper style.
- The focused proof creates an in-memory zrp header with one TypeDef row,
  attaches it to a runtime, and verifies the returned typeDefs section view
  points at the expected mmap payload.

## Validation

- WSL gcc:
  - `zr_vm_metadata_runtime_query_test` 10/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
- WSL clang:
  - `zr_vm_metadata_runtime_query_test` 10/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
  - Existing generated-C warning remains in the generic conversion fixture:
    `!zr_aot_b2 != 0u`.
- Windows MSVC Debug:
  - `zr_vm_metadata_runtime_query_test` 10/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0, 8 ignored Unix-only branches
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0, 2 ignored Unix-only branches

## Notes

- This slice closes only raw data metadata mmap attachment plus section-view
  querying.
- It does not implement definition-row semantic parsing, string/signature/
  constant pool interpretation, signature blob semantic parsing, token-to-
  runtime entity materialization, token-layout mapping, generic dictionary
  lookup, or metadata policy.
