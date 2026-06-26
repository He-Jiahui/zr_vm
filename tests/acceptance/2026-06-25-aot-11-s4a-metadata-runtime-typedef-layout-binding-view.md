# 2026-06-25 · AOT 11-S4A metadata runtime TypeDef layout binding view

## Scope

- Implement the first 11-S4 runtime view for TypeDef-backed layout identity.
- Bind a `TYPE_DEF` token to the existing type token record, attached zrp `TYPE_DEFS` row,
  `typeLayoutId`/`cTypeId`, layout version/hash, and an already cached `SZrTypeLayout` pointer
  when the cached layout's `cTypeId` matches the zrp row.
- Keep the slice read-only: no TypeSpec/generic layout materialization, no code-registration
  layout registry, no runtime layout construction side effects, and no reflection/generic/GC
  consumer migration yet.

## RED

- Added `test_metadata_runtime_reads_typedef_layout_binding_view` to
  `tests/module/test_metadata_runtime_query.c`.
- The first build failed because `SZrMetadataRuntimeTypeDefLayoutBindingView` and
  `ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()` did not exist.

## GREEN

- Added `SZrMetadataRuntimeTypeDefLayoutBindingView` and
  `ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()`.
- The runtime now rejects null runtime/output, non-`TYPE_DEF` tokens, and missing attached zrp
  metadata.
- A valid TypeDef row binds to the type record, row `typeLayoutId`/`cTypeId`, layout version/hash,
  and the matching cached prototype layout.

## Validation

- WSL gcc and WSL clang:
  - `zr_vm_metadata_runtime_query_test` 19/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
- Windows MSVC Debug:
  - metadata runtime query 19/0
  - frame setup 1/0
  - source contracts 19/0
  - shared-library smoke 8/0 with 8 ignored Unix-only branches
  - descriptor diagnostics 2/0 with 2 ignored Unix-only branches

## Remaining

- Code-registration layout registry emission.
- TypeSpec/generic layout materialization.
- Runtime layout construction and full token/cTypeId/layout cache.
- Reflection, generic dictionary, and GC descriptor consumers forced through the same layout table.
- Metadata policy and code stripping integration.
