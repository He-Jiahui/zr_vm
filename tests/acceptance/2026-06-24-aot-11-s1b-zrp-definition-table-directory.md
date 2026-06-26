# 2026-06-24 AOT 11-S1B ZRP Definition Table Directory

## Scope

11-S1B expands the zrp data-metadata header from a generic carrier into the first fixed table directory for data
metadata:

- Header version 2 with a fixed 208-byte little-endian directory.
- Twelve ordered sections: token records; TypeDef, MethodDef, FieldDef, GenericParam, GenericParamConstraint,
  TypeSpec, MethodSpec, ModuleRef; string pool, signature blob pool, and constant pool.
- Compact row carrier structs for the definition tables.
- Section validation that rejects counted definition tables whose `elementSize` or `byteLength` does not match the
  expected row width.

This is still only a format ABI slice. It does not export real compiler rows, populate string/signature/constant pool
contents, implement zrp manifest file IO, or add metadata dump/diff tooling.

## Implementation

- `zr_vm_core/include/zr_vm_core/zrp_metadata.h`
  bumps the zrp metadata version, section count, and header size, adds the definition-table section kinds, extends
  `SZrZrpMetadataHeader`, and defines the row structs for TypeDef/MethodDef/FieldDef/generic/spec/module-ref tables.
- `zr_vm_core/src/zr_vm_core/zrp_metadata.c`
  serializes/deserializes all 12 section directory entries and validates each counted section against its expected
  element width.
- `tests/module/test_zrp_metadata_format.c`
  verifies the 12-section round-trip and malformed definition-table rejection.

## RED / GREEN

RED:

- `zr_vm_zrp_metadata_format_test` failed to compile after the test required new section kinds, header fields, and row
  types.

GREEN:

- Header init reports version 2, a 208-byte header, and 12 sections.
- Header write/read round-trips all definition-table and pool directory entries.
- Validation rejects a TypeDef section with the wrong row width and a MethodDef section whose byte length does not
  match `count * elementSize`.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'cmake --build build-wsl-gcc --target zr_vm_zrp_metadata_format_test -j2 && ./build-wsl-gcc/bin/zr_vm_zrp_metadata_format_test'`
  - 3 tests, 0 failures

## Acceptance Decision

Accepted as 11-S1B only. The zrp data-metadata directory now has explicit definition-table slots and row widths, but
full 11-S1 remains open until compiler output materializes those rows and pools into a zrp data metadata payload.
