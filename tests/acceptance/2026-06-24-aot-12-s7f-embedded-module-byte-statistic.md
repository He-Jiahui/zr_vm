# 2026-06-24 AOT 12-S7F Embedded Module Byte Statistic

## Scope

12-S7F publishes the byte size of the module blob embedded in generated AOT C:

- Binary AOT outputs now include `aot_size.embeddedModuleBytes = <bytes>`.
- The value is sourced from `SZrAotWriterOptions.embeddedModuleBlobLength`.
- The marker sits beside the generated module descriptor comments so size reporting can account for embedded `.zro/.zrp`
  carrier bytes.

This does not break down internal zrp sections, definition tables, or byte pools, and it is not a pre/post trim
metadata comparison.

## Implementation

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
  emits `aot_size.embeddedModuleBytes` in the generated C header.
- `tests/parser/test_aot_c_generic_call_typed.c`
  verifies a binary-input generic-call AOT C output reports the embedded blob length.

## RED / GREEN

RED:

- `zr_vm_aot_c_generic_call_typed_test` failed once the binary-input generic call typed smoke required
  `aot_size.embeddedModuleBytes`.

GREEN:

- The generated C includes the expected embedded module byte marker.
- The binary-input shared-generic AOT C smoke still compiles and executes through AOT C, returning `42`.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_call_typed_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_call_typed_test'`
  - 6 tests, 0 failures

## Acceptance Decision

Accepted as 12-S7F only. Embedded module blob bytes are now visible in generated C size attribution, but trim warnings,
zrp section/table/pool byte statistics, metadata trim before/after comparisons, and release symbol stripping remain open.
