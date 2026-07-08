# AOT 07-S6 string-slot truthiness raw-object local

Timestamp: 2026-07-06 05:24:51 +08:00

## Scope

This slice narrows generated C for immediate string truthiness after `TO_STRING` in the 07-S6 raw object reference direction. It covers:

- `TO_STRING -> LOGICAL_NOT`
- `TO_STRING -> JUMP_IF`

It does not claim persistent `SZrRawObject*` register declarations, generated `LOCAL_ADDRESS` root-map emission, or complete 07-S6 acceptance.

## RED

The two focused smoke suites were strengthened to require:

- `SZrRawObject *zr_aot_string_slot_object = zr_aot_string_slot_value->value.object;`
- `const SZrString *zr_aot_string_slot_string = ZR_CAST_STRING(state, zr_aot_string_slot_object);`
- absence of direct `ZR_CAST_STRING(state, zr_aot_string_slot_value->value.object)`

Initial WSL GCC result:

- `zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test`: 8 tests / 1 failure

## GREEN

`backend_aot_c_write_string_slot_truthiness()` now keeps the existing `SZrTypeValue` boundary slot validation, including the string tag and non-null object payload checks, then extracts the raw object and casts that local before calling `ZrCore_String_GetByteLength()`.

Both logical-not and jump-if local paths continue to bypass `GenericPrimitiveLogicalNot` / `GenericPrimitiveIsTruthy` runtime truthiness helpers.

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

- Persistent `SZrRawObject*` reference locals.
- Generated `LOCAL_ADDRESS` root-map emission for local-address roots.
- Broader GC pressure/root-correctness stress.
- Exports/frame cleanup, in/out writeback, performance counters, and complete 07-S6/07~12 acceptance.
