# AOT 11-S4K Type Token Layout Cache

## Scope

- Slice: 11-S4K, `TYPE_DEF` / `TYPE_SPEC` token to registry-backed layout resolver.
- Public API: `ZrCore_MetadataRuntime_ResolveTypeTokenLayout(runtime, typeToken, outTypeLayoutId)`.
- Boundary: this is a latest-hit token-to-layout resolver/cache only. It does not complete the full multi-entry token/cTypeId/layout cache, reverse cTypeId-to-token table, generic layout synthesis, ownership offset emission, runtime layout construction, or public generic reflection entities.

## Baseline

- 11-S4A..11-S4J already provide TypeDef and TypeSpec row binding views backed by the code-registration layout registry.
- There was no public token-level resolver that accepts a TypeDef/TypeSpec token and caches the resulting `typeLayoutId` plus `SZrTypeLayout*`.
- TypeDef/TypeSpec lookup must keep rejecting stale `prototypeFrameTypeLayouts` when the code-registration registry layout is missing.

## RED

- Added focused coverage in `tests/module/test_metadata_runtime_typespec_layout.c` for:
  - TypeDef token to layout lookup and latest-hit cache behavior.
  - TypeSpec token to layout lookup and latest-hit cache behavior.
  - Non-type token and null runtime rejection.
  - Missing registry layout with stale prototype cache rejection.
- RED command:
  - `wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_typespec_layout_test -j2"`
- Expected RED result:
  - Build failed on missing `ZrCore_MetadataRuntime_ResolveTypeTokenLayout` declaration/definition, including implicit declaration and undefined reference diagnostics.

## GREEN

- Added latest-hit cache fields to `SZrMetadataRuntime`.
- Implemented `ZrCore_MetadataRuntime_ResolveTypeTokenLayout()` in `metadata_runtime_layout_binding.c`.
- Behavior:
  - Resets `outTypeLayoutId` to `ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE` before failure returns.
  - Accepts only `TYPE_DEF` and `TYPE_SPEC` tokens.
  - Resolves TypeDef through `ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()`.
  - Resolves TypeSpec through `ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView()`.
  - Requires registry-backed non-null layout data.
  - Caches the latest successful `typeToken/typeLayoutId/typeLayout` triple.
  - Does not read `prototypeFrameTypeLayouts` as a fallback.

## Validation

- WSL GCC:
  - `zr_vm_metadata_runtime_typespec_layout_test` passed 5/0.
  - CTest filter `metadata_runtime_typespec_layout|metadata_runtime_query|metadata_runtime_type_layout|zrp_metadata_format` passed 4/4 after rebuilding related targets.
  - The first CTest attempt failed because `metadata_runtime_query` was stale after the runtime header layout changed; rebuilding the related targets cleared it.
- WSL Clang:
  - Same CTest filter passed 4/4.
- Windows MSVC Debug:
  - `zr_vm_metadata_runtime_typespec_layout_test` passed 5/0.
  - `zr_vm_zrp_metadata_format_test` passed 11/0.
  - `zr_vm_metadata_runtime_query_test` passed 22/0.
  - `zr_vm_metadata_runtime_type_layout_test` passed 10/0.

## Acceptance Decision

Accepted for 11-S4K. The slice adds the public TypeDef/TypeSpec token layout resolver and latest-hit cache while preserving the registry-only no-prototype-fallback rule. Full 11-S4 remains open for complete cache topology, reverse mapping, generic layout materialization, ownership offsets, and runtime layout construction.
