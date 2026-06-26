# AOT 11-S3I metadata runtime signature type-node view

## Scope

- Plan slice: `docs/plans/aot/11-metadata.md` 11-S3 sub-slice.
- Goal: expose a readonly, blob-offset-based view for nested signature type
  nodes inside validated runtime signature blob slices.
- Implemented:
  - `SZrMetadataRuntimeSignatureTypeNodeView` stores node kind, original blob
    offset, next blob offset, raw payload values, base type offset, child
    count, and child-list offset.
  - `ZrCore_MetadataRuntime_ReadSignatureTypeNode()` reads a type node from a
    `SZrZrpMetadataPoolSliceView` at a caller-provided blob offset.
  - The reader covers primitive nodes, `TYPE_REF` / `TYPE_DEF` nodes,
    `GENERIC_INST` base and argument ranges, and the existing structural node
    shapes already understood by the signature skip helper.
  - Null blob, null output, null blob data, and out-of-range offsets are
    rejected with a cleared output view.

## RED

- `zr_vm_metadata_runtime_query_test` failed after adding
  `test_metadata_runtime_reads_signature_type_node_views`.
- The test referenced `SZrMetadataRuntimeSignatureTypeNodeView` and
  `ZrCore_MetadataRuntime_ReadSignatureTypeNode()`, neither of which existed.

## GREEN

- Added the signature type-node view carrier to `metadata_runtime.h`.
- Added `ZrCore_MetadataRuntime_ReadSignatureTypeNode()` in
  `metadata_runtime.c`, reusing the existing signature blob read/skip helpers.
- The focused proof validates:
  - method return primitive type node at the offset exposed by
    `ZrCore_MetadataRuntime_ReadSignatureView()`;
  - method parameter primitive type node after the parameter flag byte;
  - TypeSpec `GENERIC_INST` root node exposing base type offset, child count,
    child-list offset, and next offset;
  - nested base `TYPE_REF` and primitive generic argument node reads.

## Validation

- WSL gcc:
  - `zr_vm_metadata_runtime_query_test` 13/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
- WSL clang:
  - `zr_vm_metadata_runtime_query_test` 13/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
  - Existing generated-C warning remains in the generic conversion fixture:
    `!zr_aot_b2 != 0u`.
- Windows MSVC Debug:
  - `zr_vm_metadata_runtime_query_test` 13/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0, 8 ignored Unix-only branches
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0, 2 ignored Unix-only branches

## Notes

- This slice closes only readonly signature type-node structure views.
- It does not implement TypeSpec or generic semantic binding, `FIELD_SIG` type
  entity parsing, runtime method/field/type entity materialization,
  token-layout mapping, generic dictionary lookup, reflection entity
  construction, or metadata policy.
