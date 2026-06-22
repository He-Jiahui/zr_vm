# AOT M1.5 07-S2 U64 Bit-Not Result Local-Only

Date: 2026-06-21 04:55:24 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice removes frame destination/result materialization from the focused unsigned bit-not result
when every reachable later consumer can use the maintained u64 scalar local.

For the focused typed scalar source:

- `var unsignedInverted: uint = ~unsignedRight;`
- later local consumers of `unsignedInverted`, including stack-copy and compare paths

the generated C now emits only:

- `zr_aot_u33 = ~zr_aot_u7;`

for the `unsignedInverted` bit-not block. It no longer declares
`SZrTypeValue *zr_aot_destination`, no longer creates `zr_aot_u_result`, and no longer writes
`frame.slotBase[33].value` for that result block.

The bit-not emitter uses the same conservative u64 result-skip proof introduced for the u64
binary, bitwise, and shift result slices. The proof scans the current block suffix and reachable
successor blocks while the old u64 value is live, allows only supported local consumers, rejects
unknown frame-dependent reads, and stops tracking a path once the slot is overwritten.

## RED/GREEN

- RED: `zr_vm_aot_c_typed_scalar_test` failed when the generated product still contained the
  `dstSlot=33` u64 bit-not `SZrTypeValue *zr_aot_destination` block.
- GREEN: `zr_vm_aot_c_typed_scalar_test` passed after u64 bit-not result local-only emission reused
  the reachable-consumer proof.
- GREEN: `zr_vm_aot_c_source_contracts_test` passed.
- GREEN: registered CTest filter `aot_c_typed_scalar` passed.
- CHECK: `git diff --check` exited 0 and reported only existing LF/CRLF conversion warnings.

## Remaining

This is not full 07-S2 completion. More scalar result materialization, broader primitive constant
frame writes, direct return/result frame fallbacks, generic float copy/type checks, prologue/frame
setup, reset-stack-null frame writes, and boundary local restoration remain open.
