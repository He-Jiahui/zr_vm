# AOT M1.5 07-S2 F64 Source-Local

Date: 2026-06-21 03:25:22 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice exposes `backend_aot_c_scalar_locals_f64_written_before()` and applies the same
must-write proof to focused float sources. `backend_aot_write_c_scalar_f64_binary()` now skips
source frame type-checks and frame reloads when both `f64` operands are proven written before the
current instruction.

The focused float-source numeric conversions also reuse the live `zr_aot_fN` local. `TO_INT_FLOAT`
and `TO_UINT_FLOAT` no longer reload `frame.slotBase[19]` into `zr_aot_f19` before emitting
`zr_aot_s31 = (TZrInt64)zr_aot_f19;` or `zr_aot_u31 = (TZrUInt64)zr_aot_f19;`.

## RED/GREEN

- RED: `zr_vm_aot_c_typed_scalar_test` failed on the old float binary
  `frame.slotBase[19]` / `frame.slotBase[20]` source type-check pair.
- GREEN: `zr_vm_aot_c_typed_scalar_test` passed after float binary and float-source conversions
  reused proven `f64` locals without source frame reloads.
- GREEN: `zr_vm_aot_c_source_contracts_test` passed.
- GREEN: registered CTest filter `aot_c_typed_scalar` passed.

## Remaining

This is not full 07-S2 completion. Destination frame writes, return/result materialization,
generic float copy/type checks, and broader boundary local restoration remain open.
