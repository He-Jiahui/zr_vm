# AOT M1.5 07-S2 F64 Stack-Copy Local-Only

Date: 2026-06-21 03:40:27 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice removes frame-backed stack-copy scaffolding for focused `f64` copies when both source
and destination slots have scalar locals. `backend_aot_write_c_scalar_stack_copy_f64()` now emits
direct C-local assignment for those local-only copies:

- `zr_aot_f40 = zr_aot_f19;`
- `zr_aot_f22 = zr_aot_f40;`

The generated C no longer builds `SZrTypeValue` source/destination pointers, checks source float
tags, releases the destination slot, or writes `ZR_VALUE_TYPE_DOUBLE` payload fields for those
copies.

## RED/GREEN

- RED: after rebuilding the focused test, `zr_vm_aot_c_typed_scalar_test` failed on the expected
  missing `zr_aot_f40 = zr_aot_f19;` direct assignment.
- GREEN: `zr_vm_aot_c_typed_scalar_test` passed after `f64` stack-copy gained the local-only fast
  path.
- GREEN: `zr_vm_aot_c_source_contracts_test` passed.
- GREEN: registered CTest filter `aot_c_typed_scalar` passed.

## Remaining

This is not full 07-S2 completion. Non-f64 stack-copy frame writes, scalar result materialization,
primitive constant frame writes, return/result frame fallbacks, and boundary local restoration remain
open.
