# AOT M1.5 07-S2 Signed Branch Source-Local Acceptance

Date: 2026-06-21 01:50:07 +08:00

## Scope

This acceptance covers a fourth 07-S2 sub-slice from
`docs/plans/aot/07-codegen-register-model-and-environment-isolation.md`: signed compare branch
lowering now emits a pure C-local branch when both signed source operands are already proven written
as i64 locals.

It does not accept full 07-S2. Constant signed branches and cross-block source-local cases can still
fall back to frame validation/reload, and result frame writes remain elsewhere in the typed scalar
generated body.

## Baseline

Before this slice, the focused typed scalar fixture already emitted the direct C predicate
`if (zr_aot_s2 <= zr_aot_s4)`, but only after constructing `SZrTypeValue` source pointers and
validating both frame-slot type tags:

```text
const SZrTypeValue *zr_aot_left = ZR_NULL;
const SZrTypeValue *zr_aot_right = ZR_NULL;
zr_aot_left = &frame.slotBase[2].value;
zr_aot_right = &frame.slotBase[4].value;
if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type) ||
    !ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_right->type)) { ... }
if (zr_aot_s2 <= zr_aot_s4) { ... }
```

## Test Inventory

- `tests/parser/test_aot_c_typed_scalar.c`
  - Rejects the old signed branch source tag check over `zr_aot_left` and `zr_aot_right`.
  - Keeps the direct C predicate assertion `if (zr_aot_s2 <= zr_aot_s4) {`.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c`
  - Emits an early pure-local signed compare branch when both operands pass
    `backend_aot_c_scalar_locals_i64_written_before()`.

## RED Evidence

The focused typed scalar generated-product test failed before the implementation:

```text
Generated C still contains forbidden token
'if (!ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type) ||
 !ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_right->type))'

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

Generated C evidence from the focused fixture now emits only the C-local branch for that case:

```text
/* zr_aot_jump_if_signed_compare */
if (zr_aot_s2 <= zr_aot_s4) {
    goto zr_aot_fn_0_ins_13;
}
```

## Acceptance Decision

Accepted as a 07-S2 partial sub-slice only. This removes one signed branch source frame dependency
from the typed hot path, but full 07-S2 remains open until constant branches, cross-block sources,
primitive/result frame writes, and other typed consumers stop requiring `SZrTypeValue` frame slots.
