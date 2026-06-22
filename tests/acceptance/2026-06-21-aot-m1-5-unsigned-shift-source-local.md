# AOT M1.5 07-S2 Unsigned Shift Source-Local

Date: 2026-06-21 03:05:55 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice applies source-local written-before proofs to unsigned shift lowering in
`backend_aot_c_scalar_bitwise.c`. `u64` shift now checks the unsigned left operand with
`backend_aot_c_scalar_locals_u64_written_before()` and the signed shift count with
`backend_aot_c_scalar_locals_i64_written_before()`.

When both operands are proven written, generated C keeps the range guard over the existing `zr_aot_sN`
shift count but skips source frame type-checks and frame reloads. The focused generated C now emits
`zr_aot_u13 = zr_aot_u8 << zr_aot_s1;` and `zr_aot_u14 = zr_aot_u10 >> zr_aot_s1;` from locals.

## RED/GREEN

- RED: `zr_vm_aot_c_typed_scalar_test` failed on the old unsigned shift
  `frame.slotBase[8]` / `frame.slotBase[1]` source type-check pair.
- GREEN: `zr_vm_aot_c_typed_scalar_test` passed after unsigned shift reused proven `u64` and `i64`
  locals without source frame reloads.
- GREEN: `zr_vm_aot_c_source_contracts_test` passed.
- GREEN: registered CTest filter `aot_c_typed_scalar` passed.

## Remaining

This is not full 07-S2 completion. Float source paths, destination frame writes, return/result
materialization, and broader boundary local restoration remain open.
