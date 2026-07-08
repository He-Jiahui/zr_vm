# AOT 07-S6 object-slot truthiness raw-object local

Timestamp: 2026-07-06 05:15:29 +08:00

## Scope

This slice narrows generated C for immediate object truthiness after `TO_OBJECT` in the 07-S6 raw object reference direction. It covers:

- `TO_OBJECT -> LOGICAL_NOT`
- `TO_OBJECT -> JUMP_IF`

It does not claim persistent `SZrRawObject*` register declarations, generated `LOCAL_ADDRESS` root-map emission, or complete 07-S6 acceptance.

## RED

The two focused smoke suites were strengthened to require:

- `SZrRawObject *zr_aot_object_slot_object = ZR_NULL;`
- `zr_aot_object_slot_object = zr_aot_object_slot_value->value.object;`
- `TZrBool zr_aot_object_slot_truthy = (TZrBool)(zr_aot_object_slot_object != ZR_NULL);`
- absence of the old `zr_aot_object_slot_truthy = ZR_TRUE;`

Initial WSL GCC results:

- `zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test`: 8 tests / 1 failure
- `zr_vm_aot_c_generic_jump_if_bool_local_smoke_test`: 9 tests / 1 failure

## GREEN

`backend_aot_c_write_object_slot_truthiness()` now keeps the existing `SZrTypeValue` boundary slot validation for the current `TO_OBJECT` frame-slot handoff, then extracts a raw object pointer and computes truthiness from pointer non-nullness.

The generated C still rejects unsupported non-null/non-object value kinds, and both logical-not and jump-if local paths continue to bypass `GenericPrimitiveLogicalNot` / `GenericPrimitiveIsTruthy` runtime truthiness helpers.

## Verification

- WSL GCC:
  - `zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test`: 8/0
  - `zr_vm_aot_c_generic_jump_if_bool_local_smoke_test`: 9/0
- WSL Clang:
  - `zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test`: 8/0
  - `zr_vm_aot_c_generic_jump_if_bool_local_smoke_test`: 9/0
- Windows MSVC Debug:
  - both targets build
  - logical-not smoke: 8 expected Unix-only ignores / 0 failures
  - jump-if smoke: 9 expected Unix-only ignores / 0 failures

## Open Items

- Persistent `SZrRawObject*` reference register declarations.
- Generated `LOCAL_ADDRESS` root-map emission for local-address roots.
- Broader GC pressure/root-correctness stress.
- Exports/frame cleanup, in/out writeback, performance counters, and complete 07-S6/07~12 acceptance.
