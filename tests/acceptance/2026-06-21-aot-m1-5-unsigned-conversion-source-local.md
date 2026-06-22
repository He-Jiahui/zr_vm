# AOT M1.5 07-S2 Unsigned Conversion Source-Local

Date: 2026-06-21 03:00:10 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice applies the public `backend_aot_c_scalar_locals_u64_written_before()` query to unsigned-source
numeric conversion lowering in `backend_aot_c_scalar_conversion.c`.

For focused `TO_FLOAT_UNSIGNED` and `TO_INT_UNSIGNED` paths, generated C now skips source
`frame.slotBase[8]` assignment, source unsigned-type checks, and reloads into `zr_aot_u8` when the `u64`
local is proven written. The generated product keeps the direct conversion expressions from
`zr_aot_u8`, including the unsigned-to-signed wrap calculation.

## RED/GREEN

- RED: `zr_vm_aot_c_typed_scalar_test` failed on the old `frame.slotBase[8]` unsigned conversion source
  assignment/type-check pair.
- GREEN: `zr_vm_aot_c_typed_scalar_test` passed after unsigned-source conversions reused the proven
  `zr_aot_u8` local.
- GREEN: `zr_vm_aot_c_source_contracts_test` passed.
- GREEN: registered CTest filter `aot_c_typed_scalar` passed.

## Remaining

This is not full 07-S2 completion. Float-source conversions, unsigned shift, float binary sources,
destination frame writes, return/result materialization, and boundary local restoration still remain.
