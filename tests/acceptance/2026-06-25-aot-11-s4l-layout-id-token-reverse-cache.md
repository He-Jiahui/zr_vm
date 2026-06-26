# AOT 11-S4L Layout Id Token Reverse Cache

## Scope

- Slice: 11-S4L, registry-backed `typeLayoutId` to TypeDef/TypeSpec metadata token reverse lookup.
- Public API: `ZrCore_MetadataRuntime_ResolveTypeLayoutToken(runtime, typeLayoutId)`.
- Boundary: this is a latest-hit reverse resolver/cache path only. It does not complete a persistent cTypeId-to-token index, multi-entry token/cTypeId/layout cache, generic layout synthesis, ownership offset emission, runtime layout construction, or public generic reflection entities.

## Baseline

- 11-S4K provided TypeDef/TypeSpec token to registry-backed layout lookup and cached the latest successful token/layout hit.
- The reverse direction was still missing: callers with a proven registry layout id could not ask the metadata runtime for the corresponding TypeDef or TypeSpec token.
- Reverse lookup must keep the same no-prototype-fallback rule as the forward lookup.

## RED

- Added focused coverage in `tests/module/test_metadata_runtime_typespec_layout.c` for:
  - TypeDef `typeLayoutId` to token lookup.
  - TypeSpec `typeLayoutId` to token lookup.
  - Latest-hit cache behavior after the registry entry is cleared.
  - Null runtime, `ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE`, and stale prototype-cache-only rejection.
- RED command:
  - `wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_typespec_layout_test -j2"`
- Expected RED result:
  - Build failed on missing `ZrCore_MetadataRuntime_ResolveTypeLayoutToken` declaration/definition, including implicit declaration and undefined reference diagnostics.

## GREEN

- Added `ZrCore_MetadataRuntime_ResolveTypeLayoutToken()` to `metadata_runtime.h`.
- Implemented reverse lookup in `metadata_runtime_layout_binding.c`.
- Behavior:
  - Returns 0 for null runtime and `ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE`.
  - Reuses the latest cached `typeToken/typeLayoutId/typeLayout` hit when it matches the requested layout id.
  - Scans zrp `TYPE_DEFS` first, then zrp `TYPE_SPECS`.
  - Revalidates candidates through the existing TypeDef/TypeSpec binding views.
  - Requires registry-backed non-null `SZrTypeLayout` data.
  - Does not read `prototypeFrameTypeLayouts` as a fallback.
  - Stores the successful reverse lookup into the latest-hit cache.

## Validation

- WSL GCC:
  - Focused `zr_vm_metadata_runtime_typespec_layout_test` passed 8/0.
  - CTest filter `metadata_runtime_typespec_layout|metadata_runtime_query|metadata_runtime_type_layout|zrp_metadata_format` passed 4/4.
- WSL Clang:
  - Same CTest filter passed 4/4.
- Windows MSVC Debug:
  - `zr_vm_metadata_runtime_typespec_layout_test` passed 8/0.
  - `zr_vm_zrp_metadata_format_test` passed 11/0.
  - `zr_vm_metadata_runtime_query_test` passed 22/0.
  - `zr_vm_metadata_runtime_type_layout_test` passed 10/0.

## Acceptance Decision

Accepted for 11-S4L. The slice adds the minimal `typeLayoutId` to TypeDef/TypeSpec token reverse resolver and latest-hit cache behavior while preserving the registry-only no-prototype-fallback rule. Full 11-S4 remains open for complete cache topology, persistent reverse indexing, generic layout materialization, ownership offsets, and runtime layout construction.
