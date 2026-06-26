# 2026-06-24 AOT 11-S1A ZRP Metadata Header

## Scope

11-S1A covered the first stable data-metadata container surface for zrp metadata:

- A fixed-size zrp metadata header with magic, version, flags, section count, and table/pool directories.
- Four initial sections: token records, string pool, signature blob pool, and constant pool.
- Little-endian in-memory read/write helpers that support mmap-style validation of a read-only buffer.

This did not close full 11-S1. The current header was later expanded by 11-S1B/11-S1C to version 2, 208 bytes, 12
sections, definition-table rows, and mmap section views. This acceptance record remains the historical 11-S1A baseline.

## Implementation

- `zr_vm_core/include/zr_vm_core/zrp_metadata.h`
  defined the first `SZrZrpMetadataHeader`, `SZrZrpMetadataSection`, magic/version constants, fixed header size,
  section kinds, and public init/read/write/validate APIs.
- `zr_vm_core/src/zr_vm_core/zrp_metadata.c`
  implements little-endian serialization and validation for header and section directory entries.
- `tests/module/test_zrp_metadata_format.c`
  validates header round-trip and rejects malformed mmap views.
- `tests/CMakeLists.txt`
  registers `zr_vm_zrp_metadata_format_test` and the `zrp_metadata_format` CTest entry.

## RED / GREEN

RED:

- After CMake regeneration, the new format test failed to compile because `zr_vm_core/zrp_metadata.h` did not exist.

GREEN:

- Header init produced the expected magic, version, fixed 80-byte header size, and section count.
- Header write/read round-trips token table and pool directory entries.
- Validation rejects bad magic, future version, wrong header size, section data before the header, and counted sections
  without an element size.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm cmake --build build-wsl-gcc --target zr_vm_zrp_metadata_format_test -j2`
  - target built successfully
- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_zrp_metadata_format_test`
  - 2 tests, 0 failures
- `wsl.exe --cd /mnt/e/Git/zr_vm cmake --build build-wsl-gcc --target zr_vm_metadata_token_model_test zr_vm_metadata_runtime_query_test -j2`
  - targets built successfully
- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_metadata_runtime_query_test`
  - 3 tests, 0 failures
- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_metadata_token_model_test`
  - 21 tests, 0 failures
- `wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'ctest --test-dir build-wsl-gcc -R "(zrp_metadata_format|metadata_runtime_query|metadata_token_model)" --output-on-failure'`
  - 3 tests, 0 failures

## Acceptance Decision

Accepted as 11-S1A only. The zrp metadata header and section directory could round-trip and validate as a stable
carrier at this point. Later 11-S1 slices supersede the current header shape; full zrp metadata table/pool export
remains open.
