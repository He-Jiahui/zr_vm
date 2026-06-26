# 2026-06-24 AOT 12-S7G Method Metadata Byte Statistics

## Scope

12-S7G publishes byte attribution for generated AOT C method metadata descriptors:

- Each emitted method metadata block gets `aot_size.methodMetadataBytes[flatIndex]`.
- The method metadata region gets `aot_size.methodMetadataBytesTotal`.
- Each block currently covers the generated `zr_aot_signature_<flatIndex>` descriptor and the matching
  `zr_aot_method_info_<flatIndex>` descriptor.

This does not break down zrp internal metadata sections, definition tables, or pools, and it is not a pre/post trim
metadata comparison.

## Implementation

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.{h,c}`
  owns generated method metadata emission, measures the `backend_aot_write_c_signature()` + `SZrAotMethodInfo`
  output span per retained function, and sums it.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
  now keeps method metadata emission as orchestration only; after the split it is 691 non-empty lines, while the method
  metadata module is 326 non-empty lines.
- `tests/parser/test_aot_c_generic_call_typed.c`
  verifies generated C contains per-method and total method metadata byte markers.

## RED / GREEN

RED:

- `zr_vm_aot_c_generic_call_typed_test` failed when the generated-C generic call typed fixture required
  `aot_size.methodMetadataBytes[0]` and `aot_size.methodMetadataBytesTotal`.

GREEN:

- Generated C includes per-method and total method metadata byte markers.
- The source, binary, and full-AOT generic call typed paths still execute through the focused smoke suite.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_call_typed_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_call_typed_test'`
  - 6 tests, 0 failures

## Acceptance Decision

Accepted as 12-S7G only. Generated signature/method-info metadata bytes are now visible in AOT C size attribution, but
trim warnings, zrp section/table/pool byte statistics, metadata trim before/after comparisons, and release symbol
stripping remain open.
