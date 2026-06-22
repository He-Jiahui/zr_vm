# AOT M1.5 07-S2 Signed Compare Source-Local

Date: 2026-06-21 02:23:15 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice applies the source-local rule to signed i64 compare lowering in
`backend_aot_c_scalar_semir.c`. When the destination has a bool C local and the signed operands have
proven i64 C locals, generated C now skips source frame type-checks and frame reloads for those operands.
Unproven operands still use the existing frame fallback.

The focused typed scalar generated product rejects the old `frame.slotBase[2/4].value.type` source
checks and the consecutive reloads before `zr_aot_b27 = (TZrBool)(zr_aot_s2 > zr_aot_s4);`.

## RED/GREEN

- RED: `zr_vm_aot_c_typed_scalar_test` failed on the old signed compare source type-check pair for
  `frame.slotBase[2]` and `frame.slotBase[4]`.
- GREEN: `zr_vm_aot_c_typed_scalar_test` passed after signed i64 compare used per-source
  `backend_aot_c_scalar_locals_i64_written_before()` checks.
- GREEN: `zr_vm_aot_c_source_contracts_test` passed.
- GREEN: registered CTest filter `aot_c_typed_scalar` passed.

## Remaining

This is not full 07-S2 completion. The compare still writes the bool result through the current
`SZrTypeValue` destination path, and many unsigned, float, conversion, return, and result materialization
paths remain frame-backed.
