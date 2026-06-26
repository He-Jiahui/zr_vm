# 2026-06-24 AOT 11-S1C ZRP Section View API

## Scope

11-S1C adds the first read-only payload access API for the zrp data-metadata section directory:

- Resolve a section kind to the validated directory entry.
- Return the payload pointer, byte length, row count, and element size for mmap-style buffers.
- Treat empty sections as valid empty views.
- Reject truncated buffers and unknown section kinds without leaving stale output pointers.

This does not implement high-level token/type/method/field lookup. That remains part of 11-S3/11-S4.

## Implementation

- `zr_vm_core/include/zr_vm_core/zrp_metadata.h`
  adds `SZrZrpMetadataSectionView` and `ZrCore_ZrpMetadata_GetSectionView()`.
- `zr_vm_core/src/zr_vm_core/zrp_metadata.c`
  maps `EZrZrpMetadataSectionKind` values to header sections, revalidates the header before exposing a payload pointer,
  and clears the output view on failure.
- `tests/module/test_zrp_metadata_format.c`
  verifies TypeDef and string-pool payload resolution, empty constant-pool views, truncated-buffer rejection, and invalid
  section-kind rejection.

## RED / GREEN

RED:

- `zr_vm_zrp_metadata_format_test` failed to compile after the test required `SZrZrpMetadataSectionView` and
  `ZrCore_ZrpMetadata_GetSectionView()`.

GREEN:

- TypeDef section views point at the expected mmap payload and report the section metadata.
- String-pool views resolve byte-pool payloads.
- Empty constant-pool sections return a valid empty view.
- Truncated buffers and unknown section kinds are rejected and do not expose stale pointers.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'cmake --build build-wsl-gcc --target zr_vm_zrp_metadata_format_test -j2 && ./build-wsl-gcc/bin/zr_vm_zrp_metadata_format_test'`
  - 4 tests, 0 failures

## Acceptance Decision

Accepted as 11-S1C only. Section payload access is now available for mmap-style zrp data metadata, but full runtime
metadata resolution and compiler-produced table contents remain open.
