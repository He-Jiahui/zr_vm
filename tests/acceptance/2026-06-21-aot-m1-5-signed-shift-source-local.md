# AOT M1.5 07-S2 Signed Shift Source-Local Acceptance

Date: 2026-06-21 01:40:20 +08:00

## Scope

This acceptance covers a third 07-S2 sub-slice from
`docs/plans/aot/07-codegen-register-model-and-environment-isolation.md`: signed i64 shift lowering
now reuses already-written C scalar locals for proven source operands instead of always validating
and reloading both shift sources through `frame.slotBase[..].value`.

It does not accept full 07-S2. The current written-before proof remains block-local, so the shift
count `slot 1` still falls back to frame validation/reload in the focused fixture. Result frame
writes and other typed hot-path `SZrTypeValue` code also remain.

## Baseline

Before this slice, the focused typed scalar fixture lowered signed shifts only after validating and
reloading both sources:

```text
if (!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[15].value.type) ||
    !ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[1].value.type)) { ... }
zr_aot_s15 = frame.slotBase[15].value.value.nativeObject.nativeInt64;
zr_aot_s1 = frame.slotBase[1].value.value.nativeObject.nativeInt64;
zr_aot_s19 = (TZrInt64)((TZrUInt64)zr_aot_s15 << zr_aot_s1);
```

The right-shift path repeated the same pattern for `slot 16`.

## Test Inventory

- `tests/parser/test_aot_c_typed_scalar.c`
  - Rejects the old signed shift source type-check pairs over slots 15/1 and 16/1.
  - Rejects the old consecutive frame reload pairs before the signed shift count guard.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_bitwise.c`
  - Uses `backend_aot_c_scalar_locals_i64_written_before()` in signed i64 shift lowering.
  - Skips frame validation/reload for each proven-written signed source local.

## RED Evidence

The focused typed scalar generated-product test failed before the implementation:

```text
Generated C still contains forbidden token
'!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[15].value.type) ||
            !ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[1].value.type)'

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

Generated C evidence from the focused fixture now keeps the already-written left source locals and
only falls back for the shift count:

```text
if (!ZR_VALUE_IS_TYPE_SIGNED_INT(frame.slotBase[1].value.type)) {
    ZR_AOT_C_FAIL();
}
zr_aot_s1 = frame.slotBase[1].value.value.nativeObject.nativeInt64;
if (ZR_UNLIKELY(zr_aot_s1 < 0 || zr_aot_s1 >= 64)) { ... }
zr_aot_s19 = (TZrInt64)((TZrUInt64)zr_aot_s15 << zr_aot_s1);
zr_aot_s20 = zr_aot_s16 >> zr_aot_s1;
```

## Acceptance Decision

Accepted as a 07-S2 partial sub-slice only. This removes signed shift left-source frame
dependencies from the generated hot path, but full 07-S2 remains open until cross-block
source-local proofs, shift counts, primitive/result frame writes, and other typed consumers stop
requiring `SZrTypeValue` frame slots.
