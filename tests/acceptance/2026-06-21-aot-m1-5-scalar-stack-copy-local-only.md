# AOT M1.5 07-S2 Scalar Stack-Copy Local-Only

Date: 2026-06-21 03:46:02 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice extends stack-copy local-only lowering from `f64` to bool, signed i64, and unsigned u64
scalar locals. When both stack-copy operands have scalar locals, the typed hot path now emits direct
C-local assignments instead of frame-backed `SZrTypeValue` pointer setup, source tag checks,
destination ownership release, and destination frame payload writes.

The focused typed scalar product now includes these local-only shapes:

- `zr_aot_b5 = (TZrBool)(zr_aot_b7 != 0u);`
- `zr_aot_u9 = zr_aot_u12;`
- `zr_aot_u21 = zr_aot_u33;`
- `zr_aot_s13 = zr_aot_s16;`
- `zr_aot_s18 = zr_aot_s19;`
- `zr_aot_f40 = zr_aot_f19;`
- `zr_aot_f22 = zr_aot_f40;`

## RED/GREEN

- RED: after rebuilding the focused test, `zr_vm_aot_c_typed_scalar_test` failed on the expected
  missing `zr_aot_b5 = (TZrBool)(zr_aot_b7 != 0u);` direct bool stack-copy assignment.
- GREEN: `zr_vm_aot_c_typed_scalar_test` passed after bool/i64/u64 stack-copy gained local-only
  fast paths.
- GREEN: `zr_vm_aot_c_source_contracts_test` passed.
- GREEN: registered CTest filter `aot_c_typed_scalar` passed.

## Remaining

This is not full 07-S2 completion. Scalar result materialization, primitive constant frame writes,
direct return/result frame fallbacks, generic float copy/type checks, prologue/frame setup, and
boundary local restoration remain open.
