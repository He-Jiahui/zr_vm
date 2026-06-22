# AOT M1.5 07-S2 Cross-Block Signed Source-Local Proof

Date: 2026-06-21 02:10:32 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice extends the typed scalar generated-C contract for 07-S2. Signed i64 consumers that query
`backend_aot_c_scalar_locals_i64_written_before()` no longer rely only on same-basic-block writes. The
query now performs a conservative must-write dataflow pass over ExecIR basic-block successors and only
trusts a C local at a block entry when every reachable predecessor has definitely written the same scalar
kind.

The focused generated product now rejects the old cross-block signed binary source type-check/reload pair
for `frame.slotBase[2]` and `frame.slotBase[3]`, and the constant signed branch no longer builds a
`SZrTypeValue` left operand when `zr_aot_s2` is proven live as an i64 local.

## RED/GREEN

- RED: `zr_vm_aot_c_typed_scalar_test` failed on the forbidden cross-block
  `frame.slotBase[2/3].value.type` signed source check.
- RED: after the cross-block proof landed, the test exposed the remaining constant-branch fallback and
  failed on `const SZrTypeValue *zr_aot_left = ZR_NULL;` paired with the `42` literal.
- GREEN: `zr_vm_aot_c_typed_scalar_test` passed after the dataflow proof and constant signed branch
  local/literal lowering.
- GREEN: `zr_vm_aot_c_source_contracts_test` passed.
- GREEN: registered CTest filter `aot_c_typed_scalar` passed.

## Remaining

This is not full 07-S2 completion. Typed function bodies still contain destination frame writes and
`SZrTypeValue` result materialization for many scalar operations. Unsigned, float, conversion, return, and
broader boundary paths still need the same register-only convergence before 07-S2 can be marked complete.
