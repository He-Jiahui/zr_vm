# 2026-06-28 AOT 07-S2/S4 Bool Condition Copy Local-Only

## Scope

This slice covers the 07/M1.5 register-model cleanup for a bool stack-copy destination that is consumed immediately by a bool short-circuit branch.

It does not claim all 07-S2/S4 value-level stack-copy migration or full typed function-body zero-frame proof.

## Baseline

RED was added to `tests/parser/test_aot_c_logical_shared_library_smoke.c` for the bool short-circuit logical fixture.

The old generated C already branched with `if (!zr_aot_b15)`, but the immediately preceding `dstSlot=15 srcSlot=16` stack copy still materialized `frame.slotBase[15].value` before assigning the bool local.

The focused RED failure was:

```text
test_aot_c_generated_shared_library_executes_bool_short_circuit_logical_expressions:FAIL: Expected Non-NULL
```

## Changes

- `backend_aot_c_function_body.c` no longer treats a stack-copy destination followed by `JUMP_IF_BOOL_FALSE` as a forced value-slot write.
- Stack-copy value-slot forcing remains for next-call arguments and later bool value operands that still need the frame-backed value boundary.
- `tests/parser/test_aot_c_logical_shared_library_smoke.c` now requires the local-only `zr_aot_b15 = (TZrBool)(zr_aot_b16 != 0u);` assignment and rejects the slot 15 frame destination write.
- `tests/parser/test_aot_c_source_contracts.c` now rejects reintroducing the next-bool-condition value-slot forcing helper.

## Generated C Evidence

The WSL GCC generated logical fixture now contains:

```c
/* zr_aot_scalar_stack_copy_bool dstSlot=15 srcSlot=16 */
zr_aot_b15 = (TZrBool)(zr_aot_b16 != 0u);
/* zr_aot_jump_if_bool_false_scalar_local */
if (!zr_aot_b15) {
```

The copy path no longer contains `zr_aot_destination = &frame.slotBase[15].value;`.

## Validation

- WSL GCC build: `zr_vm_aot_c_logical_shared_library_smoke_test` and `zr_vm_aot_c_source_contracts_test`.
- WSL GCC run: logical smoke 5 tests / 0 failures; source contracts 22 tests / 0 failures.
- WSL Clang build: same two targets.
- WSL Clang run: logical smoke 5 tests / 0 failures; source contracts 22 tests / 0 failures.
- Windows MSVC Debug build: same two targets.
- Windows MSVC Debug run: logical smoke 0 failures / 5 ignored because the runtime shared-library path is Unix-only; source contracts 22 tests / 0 failures.

## Acceptance Decision

Accepted for the focused 07-S2/S4 bool condition-copy local-only slice. Remaining 07 work includes broader value-level stack-copy migration, dynamic/generic/string boundaries, GC roots/exports/frame cleanup, wider byte-frame narrowing, performance counters, and full typed function-body zero-frame proof.
