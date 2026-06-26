# AOT 11-S4D Metadata Runtime Public Type-Layout Resolver

## Scope

- Slice: 11-S4D token/cTypeId/layout bridge.
- Goal: expose a reusable runtime API for resolving `typeLayoutId` through the code-registration type-layout registry.
- Non-goals: TypeSpec/generic layout materialization, ownership offset emission, runtime layout construction, reflection/generic/GC consumer migration, and complete token/cTypeId/layout caching.

## Baseline

- 11-S4B emitted `typeLayouts/typeLayoutCount` in generated-C code registration.
- 11-S4C made TypeDef layout binding read that registry directly.
- Before this slice, no public metadata runtime API exposed the lookup, and `SZrMetadataRuntime` did not mirror `typeLayoutCount`.

## RED

- Added `tests/module/test_metadata_runtime_type_layout.c` and CMake target `zr_vm_metadata_runtime_type_layout_test`.
- First WSL GCC build failed as expected because:
  - `SZrMetadataRuntime` had no `typeLayoutCount` member.
  - `ZrCore_MetadataRuntime_ResolveTypeLayout()` was undeclared.
- The initial build directory also required CMake regeneration before the new target existed.

## GREEN

- Added `SZrMetadataRuntime.typeLayoutCount` and copied `codeRegistration->typeLayoutCount` during `ZrCore_Module_AttachMetadataRuntime()`.
- Added `ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, typeLayoutId)`.
- The resolver rejects null runtime, `ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE`, missing registry, out-of-range id, sparse null entries, and `layout->cTypeId` mismatches.
- `ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()` now uses the public resolver.
- New focused test proves the resolver reads the code-registration registry and does not fall back to `metadataFunction->prototypeFrameTypeLayouts`.

## Validation

- WSL GCC:
  - `zr_vm_metadata_runtime_type_layout_test` 3/0
  - `zr_vm_metadata_runtime_query_test` 20/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_value_type_shared_library_smoke_test` 2/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
- WSL Clang: same pass counts as WSL GCC; existing generated-C logical-not parentheses warning remains non-fatal.
- Windows MSVC Debug:
  - type-layout runtime 3/0
  - metadata runtime query 20/0
  - frame setup 1/0
  - source contracts 19/0
  - shared-library smoke 8/0 with 8 Unix-only ignored
  - value-type shared-library smoke 2/0 with 1 Unix-only ignored
  - descriptor diagnostics 2/0 with 2 Unix-only ignored

## Acceptance Decision

Accepted as an incremental 11-S4 slice. The runtime now has one public source for registry-backed type-layout lookup, but the broader 11-S4 table is still incomplete until TypeSpec/generic layout materialization, ownership offsets, runtime construction, and reflection/generic/GC consumers converge on it.
