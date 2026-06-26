# 2026-06-24 AOT 11-S1F ZRP Pool Slice View

## Scope

11-S1F adds a bounded byte-pool slice API for the zrp data-metadata pools:

- String pool slices.
- Signature blob pool slices.
- Constant pool slices.
- Zero-length slices at the pool end.
- Rejection for non-pool section kinds and out-of-range slices.

This does not generate pool contents or parse string/signature/constant semantics. It only provides the safe read
primitive that later exporters and runtime resolvers can use.

## Implementation

- `zr_vm_core/include/zr_vm_core/zrp_metadata.h`
  adds `SZrZrpMetadataPoolSliceView` and `ZrCore_ZrpMetadata_GetPoolSlice()`.
- `zr_vm_core/src/zr_vm_core/zrp_metadata.c`
  resolves slices through the existing validated section-view path and constrains accepted section kinds to the three
  byte pools.
- `tests/module/test_zrp_metadata_format.c`
  verifies string/signature/constant pool payload resolution, pool-end empty slices, non-pool rejection, and bounds
  rejection.

## RED / GREEN

RED:

- `zr_vm_zrp_metadata_format_test` failed to compile after the test required `SZrZrpMetadataPoolSliceView` and
  `ZrCore_ZrpMetadata_GetPoolSlice()`.

GREEN:

- String, signature blob, and constant pool slices resolve to the expected payload bytes.
- A zero-length slice at the end of the string pool is accepted.
- A TypeDef section kind is rejected because it is not a byte pool.
- A slice extending past the string pool boundary is rejected.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'cmake --build build-wsl-gcc --target zr_vm_zrp_metadata_format_test -j2 && ./build-wsl-gcc/bin/zr_vm_zrp_metadata_format_test'`
  - 7 tests, 0 failures

## Acceptance Decision

Accepted as 11-S1F only. The byte-pool access primitive is now bounds-checked, but pool materialization and higher-level
decoding remain future 11 work.
