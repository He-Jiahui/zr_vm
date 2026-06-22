# AOT M1.5 07-S1 Environment Isolation Acceptance

Date: 2026-06-21 00:02:31 +08:00

## Scope

This acceptance covers only `docs/plans/aot/07-codegen-register-model-and-environment-isolation.md`
slice 07-S1: remove `zr_aot_begin_instruction` emission from typed generated C function bodies.
It does not accept the full M1.5/07 milestone. The 07-S2 through 07-S7 items remain open.

## Baseline

Before this slice, `backend_aot_c_function_body.c` emitted
`backend_aot_write_c_begin_instruction(...)` for each instruction unless a narrow constant-path
skip detected a direct primitive constant. That kept typed generated C coupled to the interpreter
observation environment through `zr_aot_begin_instruction`, frame refresh, current-instruction
bookkeeping, program-counter publication, and line-hook checks.

The 07 plan requires typed C function bodies to default to zero per-instruction observation and to
keep interpreter environment publication out of the hot path.

## Test Inventory

- `tests/parser/test_aot_c_source_contracts.c`
  - Added a forbidden function-body source needle for `backend_aot_write_c_begin_instruction(`.
- `tests/parser/test_aot_c_typed_scalar.c`
  - Added generated-product coverage requiring focused typed scalar C output to omit
    `zr_aot_begin_instruction`.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`
  - Removed the per-instruction begin-instruction emission and its skip bookkeeping.

## RED Evidence

Manual WSL GCC source-contract run failed before the implementation:

```text
zr_vm_aot_c_source_contracts_test
19 tests, 1 failure
test_aot_c_source_emits_value_frame_cleanup_exit:
unexpected backend_aot_write_c_begin_instruction(
```

## GREEN Evidence

After removing the function-body call:

```text
zr_vm_aot_c_source_contracts_test
19 tests, 0 failures
```

Focused isolated WSL GCC/Ninja validation:

```text
zr_vm_aot_c_typed_scalar_test
1 test, 0 failures

zr_vm_aot_c_source_contracts_test
19 tests, 0 failures
```

CTest filtered validation:

```text
ctest -R 'aot_c_(source_contracts|typed_scalar)'
1/1 tests passed
```

Only `aot_c_typed_scalar` is registered as a CTest entry in this build; the source-contract binary
was run directly from the same isolated build directory.

## Acceptance Decision

Accepted for 07-S1 only. Typed generated C products in the focused scalar path no longer contain
`zr_aot_begin_instruction`, and the function-body source contract now rejects future attempts to
restore per-instruction begin-instruction emission.

Remaining M1.5 work starts at 07-S2: convert `CONST` and scalar writes to single-register writes,
remove typed hot-path `SZrTypeValue` frame writes, and then continue through MethodInfo, byte-frame
narrowing, boundary marshaling, GC roots, and CI/performance gates.
