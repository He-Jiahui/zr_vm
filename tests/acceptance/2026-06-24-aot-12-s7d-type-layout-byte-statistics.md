# 2026-06-24 AOT 12-S7D Type Layout Byte Statistics

## Scope

12-S7D adds generated-C byte attribution for value-type layout carriers:

- Each emitted `ZrLayout_<typeLayoutId>` block reports `aot_size.typeLayoutBytes[typeLayoutId]`.
- The span includes layout typedef/static assertions.
- If a generated GC descriptor is colocated with that layout, the descriptor block is included in the same statistic.
- POD layouts without descriptors still report their layout block byte span.

This does not implement metadata table/pool byte attribution, trim warnings, pre/post trim layout comparisons, or release
symbol stripping.

## Implementation

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.c`
  records the output position before and after each generated type-layout block and appends the size marker.
- `tests/parser/test_aot_c_value_type_shared_library_smoke.c`
  verifies both reference-field and POD generated C include the marker while preserving existing GC descriptor behavior.

## RED / GREEN

RED:

- The value-type shared-library smoke failed after the test required `aot_size.typeLayoutBytes[` in generated ref/POD
  layout C.

GREEN:

- Reference-field generated C includes the new type-layout byte marker and still emits a GC descriptor.
- POD generated C includes the new type-layout byte marker and still omits GC descriptor blocks.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_value_type_shared_library_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_value_type_shared_library_smoke_test'`
  - 2 tests, 0 failures

## Acceptance Decision

Accepted as 12-S7D only. Type-layout/generated-descriptor byte attribution is now present, but broader 12-S7 trim
warnings, metadata byte statistics, and symbol stripping remain open.
