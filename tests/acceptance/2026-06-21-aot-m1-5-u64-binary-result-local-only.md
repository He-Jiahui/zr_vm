# AOT M1.5 07-S2 U64 Binary Result Local-Only

Date: 2026-06-21 04:33:00 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice removes frame destination/result materialization from the focused `uint` binary result
when every reachable later consumer can use the maintained u64 scalar local.

For the focused typed scalar source:

- `var unsignedSum: uint = unsignedLeft + unsignedRight;`
- later local consumers of `unsignedSum`, including bitwise, shift, compare, and numeric conversion

the generated C now emits only:

- `zr_aot_u8 = zr_aot_u6 + zr_aot_u7;`

for the `unsignedSum` binary block. It no longer declares `SZrTypeValue *zr_aot_destination`, no
longer creates `zr_aot_u_result`, and no longer writes `frame.slotBase[8].value` for that result
block. Divide/modulo guards remain in the local-only path for the corresponding u64 opcodes.

`backend_aot_c_scalar_locals` now exposes a conservative u64 result-skip proof. It scans the
current block suffix and reachable successor blocks while the old u64 value is live, allows only
supported local consumers, rejects unknown frame-dependent reads, and stops tracking a path once the
slot is overwritten.

## RED/GREEN

- RED: `zr_vm_aot_c_typed_scalar_test` failed when the generated product still contained the
  `dstSlot=8` u64 binary `SZrTypeValue *zr_aot_destination` block.
- GREEN: `zr_vm_aot_c_typed_scalar_test` passed after u64 binary result local-only emission and the
  reachable-consumer proof landed.
- GREEN: `zr_vm_aot_c_source_contracts_test` passed.
- GREEN: registered CTest filter `aot_c_typed_scalar` passed.
- CHECK: `git diff --check` exited 0 and reported only existing LF/CRLF conversion warnings.

## Remaining

This is not full 07-S2 completion. More scalar result materialization, broader primitive constant
frame writes, direct return/result frame fallbacks, generic float copy/type checks, prologue/frame
setup, reset-stack-null frame writes, and boundary local restoration remain open.
