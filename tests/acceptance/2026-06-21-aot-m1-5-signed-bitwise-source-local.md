# AOT M1.5 07-S2 Signed Bitwise Source-Local Acceptance

Date: 2026-06-21 01:31:55 +08:00

## Scope

This acceptance covers a second 07-S2 sub-slice from
`docs/plans/aot/07-codegen-register-model-and-environment-isolation.md`: signed i64 bitwise
lowering now reuses C scalar locals for source operands that have already been written in the
current proven block, instead of always validating and reloading those sources through
`frame.slotBase[..].value`.

It does not accept full 07-S2. The current written-before proof remains block-local, so cross-block
values such as the original `left` constant can still fall back to frame validation/reload until the
register model gains dominance-aware source proofs. Typed generated function bodies also still
contain result frame writes and other `SZrTypeValue` hot-path code.

## Baseline

Before this slice, the focused typed scalar fixture lowered the signed bitwise expression
`zr_aot_s16 = zr_aot_s12 & zr_aot_s0` only after validating both source frame slots and reloading
both source values:

```text
if (!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[12].value.type) ||
    !ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[0].value.type)) { ... }
zr_aot_s12 = frame.slotBase[12].value.value.nativeObject.nativeInt64;
zr_aot_s0 = frame.slotBase[0].value.value.nativeObject.nativeInt64;
zr_aot_s16 = zr_aot_s12 & zr_aot_s0;
```

## Test Inventory

- `tests/parser/test_aot_c_typed_scalar.c`
  - Rejects the old signed bitwise source type-check pair over slots 12 and 0.
  - Rejects the old consecutive frame reload pair before `zr_aot_s16 = zr_aot_s12 & zr_aot_s0`.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_bitwise.c`
  - Uses `backend_aot_c_scalar_locals_i64_written_before()` for signed bitwise source operands.
  - Skips frame validation/reload for each i64 source already proven written as a C local.

## RED Evidence

The focused typed scalar generated-product test failed before the implementation:

```text
Generated C still contains forbidden token
'!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[12].value.type) ||
            !ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[0].value.type)'

zr_vm_aot_c_typed_scalar_test
1 test, 1 failure
```

## GREEN Evidence

After the implementation:

```text
zr_vm_aot_c_typed_scalar_test
1 test, 0 failures

zr_vm_aot_c_source_contracts_test
19 tests, 0 failures

ctest -R 'aot_c_typed_scalar'
1/1 tests passed
```

Generated C evidence from the focused fixture now keeps the already-written `zr_aot_s12` local and
only falls back for `slot 0`, which is not yet proven across the current block boundary:

```text
zr_aot_destination = &frame.slotBase[16].value;
if (!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[0].value.type)) {
    ZR_AOT_C_FAIL();
}
zr_aot_s0 = frame.slotBase[0].value.value.nativeObject.nativeInt64;
zr_aot_s16 = zr_aot_s12 & zr_aot_s0;
```

## Acceptance Decision

Accepted as a 07-S2 partial sub-slice only. This removes one more typed scalar source frame
dependency from the generated hot path, but full 07-S2 remains open until primitive constants,
scalar results, bitwise/shift/branch/conversion consumers, and cross-block source proofs stop
requiring `SZrTypeValue` frame slots in typed generated function bodies.
