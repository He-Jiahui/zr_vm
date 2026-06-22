# AOT M1.5 07-S2 Signed Conversion Source-Local

Date: 2026-06-21 02:35:27 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice applies the source-local rule to signed-source numeric conversion lowering in
`backend_aot_c_scalar_conversion.c`. `backend_aot_try_write_c_scalar_conversion()` now receives the
current exec instruction index so typed conversion emitters can query
`backend_aot_c_scalar_locals_i64_written_before()`.

For `TO_FLOAT_SIGNED` and `TO_UINT_SIGNED`, when the signed source slot has a proven written i64 C
local, generated C skips the source `frame.slotBase[N]` assignment, source signed-type check, and reload
back into `zr_aot_sN`. The focused typed scalar product keeps the direct casts
`zr_aot_f31 = (TZrFloat64)zr_aot_s2;` and `zr_aot_u31 = (TZrUInt64)zr_aot_s2;`.

## RED/GREEN

- RED: `zr_vm_aot_c_typed_scalar_test` failed on the old `frame.slotBase[2]` signed conversion source
  assignment/type-check pair.
- GREEN: `zr_vm_aot_c_typed_scalar_test` passed after signed-source conversions reused the proven
  `zr_aot_s2` local.
- GREEN: `zr_vm_aot_c_source_contracts_test` passed.
- GREEN: registered CTest filter `aot_c_typed_scalar` passed.

## Remaining

This is not full 07-S2 completion. Float-source and unsigned-source conversions still use the current
frame fallback until their written-before queries are exposed, and conversion destinations still
materialize through `SZrTypeValue` frame writes.
