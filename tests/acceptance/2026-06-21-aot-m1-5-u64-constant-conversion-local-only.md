# AOT M1.5 07-S2 U64 Constant Conversion Local-Only

Date: 2026-06-21 04:10:01 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice removes frame-backed scaffolding from the focused `uint` initialization chain where the
frontend currently emits signed integer constants followed by `TO_UINT`.

For the focused typed scalar source:

- `var unsignedLeft: uint = 9;`
- `var unsignedRight: uint = 4;`
- `var unsignedSum: uint = unsignedLeft + unsignedRight;`

the generated C now keeps the hot path in scalar locals:

- `zr_aot_u6 = (TZrUInt64)zr_aot_s6;`
- `zr_aot_u7 = (TZrUInt64)4;`
- `zr_aot_u8 = zr_aot_u6 + zr_aot_u7;`

`backend_aot_c_scalar_locals` records signed immediate writes as u64 writes when the destination has
u64 scalar-local coverage, so later `TO_UINT` and u64 binary consumers can prove the local was
already written. The u64 binary emitter now skips source frame type-checks and frame reloads when
both u64 sources are proven written.

## RED/GREEN

- RED: `zr_vm_aot_c_typed_scalar_test` first failed on the missing
  `zr_aot_u6 = (TZrUInt64)zr_aot_s6;` direct conversion.
- RED: after adding the first conversion fast path, the same test failed on the remaining slot 7
  `TO_UINT` source frame check.
- RED: after slot 7 conversion became local-only, the same test exposed the remaining u64 binary
  source frame type-check pair for slots 6 and 7.
- GREEN: `zr_vm_aot_c_typed_scalar_test` passed after signed-immediate u64 local write proof and
  u64 binary written-source checks were added.
- GREEN: `zr_vm_aot_c_source_contracts_test` passed.
- GREEN: registered CTest filter `aot_c_typed_scalar` passed.
- CHECK: `git diff --check` exited 0 and reported only existing LF/CRLF conversion warnings.

## Remaining

This is not full 07-S2 completion. Scalar result materialization, broader primitive constant frame
writes, direct return/result frame fallbacks, generic float copy/type checks, prologue/frame setup,
reset-stack-null frame writes, and boundary local restoration remain open.
