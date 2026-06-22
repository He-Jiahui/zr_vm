# AOT M1.5 07-S2 U64 Shift Result Local-Only

Date: 2026-06-21 04:48:06 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice removes frame destination/result materialization from the focused unsigned shift results
when every reachable later consumer can use the maintained u64 scalar local.

For the focused typed scalar source:

- `var unsignedShifted: uint = unsignedSum << right;`
- `var unsignedShiftedBack: uint = unsignedShifted >> right;`

the generated C now keeps only the signed shift-count range guard and the local assignment:

- `zr_aot_u13 = zr_aot_u8 << zr_aot_s1;`
- `zr_aot_u14 = zr_aot_u10 >> zr_aot_s1;`

for the two u64 shift result blocks. Those blocks no longer declare
`SZrTypeValue *zr_aot_destination`, no longer create `zr_aot_u_result`, and no longer write
`frame.slotBase[13].value` or `frame.slotBase[14].value` for the shift results.

The shift emitter uses the same conservative u64 result-skip proof introduced for the u64 binary
and bitwise result slices. The proof scans the current block suffix and reachable successor blocks
while the old u64 value is live, allows only supported local consumers, rejects unknown
frame-dependent reads, and stops tracking a path once the slot is overwritten. Shift-count range
validation stays inside the local-only path.

## RED/GREEN

- RED: `zr_vm_aot_c_typed_scalar_test` failed when the generated product still contained the
  `dstSlot=13` u64 shift `SZrTypeValue *zr_aot_destination` block.
- GREEN: `zr_vm_aot_c_typed_scalar_test` passed after u64 shift result local-only emission reused
  the reachable-consumer proof and preserved the range guard.
- GREEN: `zr_vm_aot_c_source_contracts_test` passed.
- GREEN: registered CTest filter `aot_c_typed_scalar` passed.
- CHECK: `git diff --check` exited 0 and reported only existing LF/CRLF conversion warnings.

## Remaining

This is not full 07-S2 completion. More scalar result materialization, broader primitive constant
frame writes, direct return/result frame fallbacks, generic float copy/type checks, prologue/frame
setup, reset-stack-null frame writes, and boundary local restoration remain open.
