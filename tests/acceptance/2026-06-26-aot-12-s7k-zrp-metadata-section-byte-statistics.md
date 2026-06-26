# AOT 12-S7K zrp Metadata Section Byte Statistics

## Scope

- Slice: 12-S7K, under 12-S7 trim warnings and size statistics.
- Goal: generated AOT C reports zrp data-metadata bytes by total, table group, pool group, and individual section when the embedded module blob is a valid zrp metadata buffer.
- Non-goals: byte-level trim before/after metadata diff, default-min metadata policy, zrp dump/diff tooling, and release symbol stripping.

## RED

- Command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_code_stripping_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_code_stripping_test"`
- Result before implementation: `zr_vm_aot_c_code_stripping_test` ran 4 tests and failed 1 test in `test_aot_c_reports_zrp_metadata_section_table_pool_byte_stats` with `Expected Non-NULL`.
- Generated evidence before implementation: `build-wsl-gcc/tests_generated/aot_c_code_stripping/generated/zrp_metadata_size.c` only reported `aot_size.embeddedModuleBytes = 374` and did not report zrp section/table/pool markers.

## GREEN

- Production: `backend_aot_c_emitter.c` now reads `SZrAotWriterOptions.embeddedModuleBlob` with `ZrCore_ZrpMetadata_ReadHeader()` and emits stable AOT size markers.
- Test: `test_aot_c_code_stripping.c` builds a valid synthetic zrp metadata buffer and asserts total, grouped, and section-level byte markers in generated C.
- Generated evidence after implementation:
  - `aot_size.zrpMetadataBytes = 374`
  - `aot_size.zrpMetadataTokenRecordBytes = 96`
  - `aot_size.zrpMetadataDefinitionTableBytes = 52`
  - `aot_size.zrpMetadataPoolBytes = 18`
  - section markers cover `tokenRecords`, `typeDefs`, `methodDefs`, `fieldDefs`, `genericParams`, `genericParamConstraints`, `typeSpecs`, `methodSpecs`, `moduleRefs`, `stringPool`, `signatureBlobPool`, and `constantPool`.
- Invalid, missing, or non-zrp embedded blobs still emit zero zrp metadata markers so existing `.zro` embedded-module paths keep a stable reporting shape.

## Validation

- WSL gcc:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_code_stripping_test zr_vm_aot_c_generic_call_typed_test zr_vm_aot_c_source_contracts_test zr_vm_zrp_metadata_format_test -j2`
  plus CTest filter `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format`: 3/3 passed.
  Direct `zr_vm_aot_c_source_contracts_test`: 19/0 passed.
- WSL clang:
  same target set and CTest filter: 3/3 passed.
  Direct `zr_vm_aot_c_source_contracts_test`: 19/0 passed.
- Windows MSVC Debug:
  same target set and CTest filter: 3/3 passed.
  Direct `zr_vm_aot_c_source_contracts_test.exe`: 19/0 passed.

## Remaining

- 12-S7 remains open for full trim analyzer behavior, full source spans, warning suppression, byte-level before/after type/layout diff, and release symbol stripping.
- 11 metadata remains open for default-min metadata policy, real compiler zrp table/pool export policy, cross-module metadata policy, and dump/diff tooling.
