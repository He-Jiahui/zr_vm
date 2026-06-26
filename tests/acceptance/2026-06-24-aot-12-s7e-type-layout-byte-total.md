# 2026-06-24 AOT 12-S7E Type Layout Byte Total

## Scope

12-S7E aggregates the generated type-layout byte spans introduced by 12-S7D:

- Each `ZrLayout_<typeLayoutId>` block still emits `aot_size.typeLayoutBytes[typeLayoutId]`.
- The type-layout declaration region now also emits `aot_size.typeLayoutBytesTotal`.
- The total covers emitted generated type-layout blocks and colocated generated GC descriptor blocks.

This does not include zrp data-metadata tables or pools, and it is not a pre/post trim size comparison.

## Implementation

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.c`
  returns each emitted layout block span from `backend_aot_c_type_layout_emit_one()` and sums those values in
  `backend_aot_write_c_type_layout_declarations()`.
- `tests/parser/test_aot_c_value_type_shared_library_smoke.c`
  verifies both reference-field and POD generated C include the total marker.

## RED / GREEN

RED:

- The value-type shared-library smoke failed when it required `aot_size.typeLayoutBytesTotal = ` in generated ref/POD
  layout C.

GREEN:

- Reference-field generated C includes per-layout and total type-layout byte markers.
- POD generated C includes per-layout and total type-layout byte markers while still omitting GC descriptor blocks.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_value_type_shared_library_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_value_type_shared_library_smoke_test'`
  - 2 tests, 0 failures

## Acceptance Decision

Accepted as 12-S7E only. Generated type-layout byte totals are now visible, but trim warnings, metadata byte statistics,
release symbol stripping, and before/after type-layout comparisons remain open.
