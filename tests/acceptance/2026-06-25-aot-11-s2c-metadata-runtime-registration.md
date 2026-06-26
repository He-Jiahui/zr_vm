# AOT 11-S2C metadata runtime registration carrier

## Scope

- Plan slice: `docs/plans/aot/11-metadata.md` 11-S2 sub-slice.
- Goal: register the generated AOT code registration into a runtime metadata
  carrier when an AOT module is loaded.
- Implemented:
  - Added minimal `SZrMetadataRuntime`.
  - `SZrObjectModule` stores the metadata runtime carrier.
  - Added module attach/query APIs for the metadata runtime.
  - AOT module load attaches module, metadata function, code registration, and
    table counts through `ZrCore_Module_AttachMetadataRuntime()`.
  - GC mark/rewrite visits the metadata runtime's `metadataFunction`.
  - Source contracts cover the loader attach call and failure diagnostic.

## RED

- `zr_vm_metadata_runtime_query_test` failed to compile because
  `zr_vm_core/metadata_runtime.h` and module attach/query APIs did not exist.
- After the core carrier existed, `zr_vm_aot_c_frame_setup_contracts_test`
  failed because the AOT loader did not attach
  `record.module`, `record.moduleFunction`, and `record.codeRegistration` to a
  metadata runtime.

## GREEN

- `SZrMetadataRuntime` carries the owning module, metadata function, code
  registration pointer, and function/method/invoker/GC descriptor counts.
- `ZrCore_Module_AttachMetadataRuntime()` initializes the carrier from the
  generated `SZrAotCodeRegistration`.
- `ZrCore_Module_GetMetadataRuntime()` exposes the attached carrier.
- AOT module load fails with a clear diagnostic if metadata runtime attachment
  fails.
- GC module mark/rewrite keeps the metadata function reference live and updated.

## Validation

- WSL gcc:
  - `zr_vm_metadata_runtime_query_test` 4/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
- WSL clang:
  - `zr_vm_metadata_runtime_query_test` 4/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
- Windows MSVC Debug:
  - `zr_vm_metadata_runtime_query_test` 4/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0, 8 ignored Unix-only branches
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0, 2 ignored Unix-only branches

## Notes

- This slice closes only the module-load metadata runtime registration carrier.
- It does not implement data metadata mmap attach, token to function/layout lazy
  resolution, runtime caches, token-layout three-way mapping, default-minimal
  metadata policy, or dump/diff tooling.
