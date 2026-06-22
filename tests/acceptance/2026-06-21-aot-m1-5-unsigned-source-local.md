# AOT M1.5 07-S2 Unsigned Source-Local

Date: 2026-06-21 02:51:35 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice exposes `backend_aot_c_scalar_locals_u64_written_before()` and applies the same source-local
proof to focused unsigned scalar consumers. Unsigned compare lowering now skips source frame type-checks
and frame reloads when both operands are proven written `u64` C locals. The focused `zr_aot_b14` and
`zr_aot_b23` compare paths now branch from `zr_aot_u8` / `zr_aot_u7` and `zr_aot_u21` / `zr_aot_u7`
without first refreshing those locals from `frame.slotBase`.

The same proof also covers the unsigned bitwise source producers needed by the compare slice. `u64`
bitwise binary and bit-not lowering skip proven source frame checks/reloads, keeping `zr_aot_u12` and
`zr_aot_u33` as local-state producers for later consumers.

## RED/GREEN

- RED: `zr_vm_aot_c_typed_scalar_test` failed on the old `frame.slotBase[21/7]` unsigned compare source
  type-check pair.
- RED: after compare started using `u64_written_before`, the test exposed the remaining unsigned
  bitwise source frame fallback for `frame.slotBase[8/7]`.
- GREEN: `zr_vm_aot_c_typed_scalar_test` passed after unsigned compare, bitwise, and bit-not consumers
  used proven `u64` locals.
- GREEN: `zr_vm_aot_c_source_contracts_test` passed.
- GREEN: registered CTest filter `aot_c_typed_scalar` passed.

## Remaining

This is not full 07-S2 completion. Unsigned shift, unsigned/float conversion sources, float binary
sources, destination frame writes, return/result materialization, and broader boundary paths still need
register-only convergence.
