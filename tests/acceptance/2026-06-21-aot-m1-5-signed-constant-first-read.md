# AOT M1.5 07-S2 Signed Constant First-Read Acceptance

Date: 2026-06-21 00:38:52 +08:00

## Scope

This acceptance covers a 07-S2 sub-slice from
`docs/plans/aot/07-codegen-register-model-and-environment-isolation.md`: signed primitive constants
with i64 scalar-local coverage now publish a C-local assignment block before the existing frame
write path, and the first i64 binary source read uses already-written C locals instead of validating
those operands through frame-slot type tags.

It does not accept full 07-S2. Typed generated function bodies still contain `SZrTypeValue` frame
writes and later source frame-slot checks that must be removed in following slices.

## Baseline

Before this slice, the focused typed scalar fixture generated `left=21` and `right=2` through
`zr_aot_value_exec_primitive_constant`, wrote each constant into `frame.slotBase[..].value`, then
mirrored the value into `zr_aot_s0` / `zr_aot_s1`. The first multiply then validated
`frame.slotBase[0].value.type` and `frame.slotBase[1].value.type` before using the scalar locals.

## Test Inventory

- `tests/parser/test_aot_c_typed_scalar.c`
  - Requires `zr_aot_scalar_constant_i64_local` blocks for the first two signed constants.
  - Rejects the first multiply's frame-slot source type check over slots 0 and 1.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c`
  - Emits the signed constant scalar-local block before the legacy frame write fallback.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_binary.c`
  - Skips i64 binary source type checks when both source operands are proven written C locals.

## RED Evidence

The focused typed scalar test failed before the implementation:

```text
zr_vm_aot_c_typed_scalar_test
1 test, 1 failure
Expected: /* zr_aot_scalar_constant_i64_local */\n        zr_aot_s0 = (TZrInt64)21;
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

Generated C evidence from the focused fixture now contains:

```text
/* zr_aot_scalar_constant_i64_local */
zr_aot_s0 = (TZrInt64)21;
/* zr_aot_scalar_constant_i64_local */
zr_aot_s1 = (TZrInt64)2;
zr_aot_s2 = zr_aot_s0 * zr_aot_s1;
```

## Acceptance Decision

Accepted as a 07-S2 partial sub-slice only. This improves the first signed constant read and first
i64 binary source path, but full 07-S2 remains open until primitive constants, scalar results, and
all typed scalar consumers stop writing and validating through `SZrTypeValue` frame slots in the
typed hot path.
