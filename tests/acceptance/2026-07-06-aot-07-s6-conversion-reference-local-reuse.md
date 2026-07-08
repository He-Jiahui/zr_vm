# AOT 07-S6 conversion reference-local reuse

Timestamp: 2026-07-06 05:54:47 +08:00

## Scope

This slice adds generated `SZrRawObject *zr_aot_oN` reference locals for `TO_STRING` and `TO_OBJECT` destinations, then reuses those locals in the immediately following string/object truthiness fast paths:

- `TO_STRING -> LOGICAL_NOT`
- `TO_STRING -> JUMP_IF`
- `TO_OBJECT -> LOGICAL_NOT`
- `TO_OBJECT -> JUMP_IF`

It does not claim root-mapped persistent reference registers, generated `LOCAL_ADDRESS` root-map emission, or complete 07-S6 acceptance.

## RED

The focused smoke suites were strengthened to require:

- `SZrRawObject *zr_aot_o2 = ZR_NULL;`
- `zr_aot_o2 = zr_aot_to_string_value->value.object;`
- `zr_aot_o2 = zr_aot_to_object_value->value.object;`
- `ZR_CAST_STRING(state, zr_aot_o2)`
- object truthiness from `zr_aot_o2 != ZR_NULL`
- absence of the old truthiness-time `zr_aot_string_slot_value` / `zr_aot_object_slot_value` frame-slot rereads

Initial WSL GCC results:

- `zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test`: 8 tests / 2 failures
- `zr_vm_aot_c_generic_jump_if_bool_local_smoke_test`: 9 tests / 2 failures

## GREEN

`backend_aot_c_reference_locals.c` now scans `TO_STRING` / `TO_OBJECT` destinations and emits one `SZrRawObject *zr_aot_oN = ZR_NULL;` declaration per destination slot before block labels.

`backend_aot_write_c_direct_to_string()` and `backend_aot_write_c_direct_to_object()` still route conversion semantics through the runtime boundary helpers, then validate the resulting destination slot and populate `zr_aot_oN`. The immediate string/object truthiness helpers now consume `zr_aot_oN` directly instead of rereading `frame.slotBase + N` at truthiness time.

## Verification

- WSL GCC:
  - `zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test`: 8/0
  - `zr_vm_aot_c_generic_jump_if_bool_local_smoke_test`: 9/0
  - `zr_vm_aot_c_logical_contracts_test`: 4/0
  - `zr_vm_aot_c_source_contracts_test`: 24/0
- WSL Clang:
  - `zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test`: 8/0
  - `zr_vm_aot_c_generic_jump_if_bool_local_smoke_test`: 9/0
- Windows MSVC Debug:
  - both focused smoke targets build
  - logical-not smoke: 8 expected Unix-only ignores / 0 failures
  - jump-if smoke: 9 expected Unix-only ignores / 0 failures

## Open Items

- Root-mapped persistent `SZrRawObject*` reference registers.
- Generated `LOCAL_ADDRESS` root-map emission for reference locals.
- Broader GC pressure/root-correctness stress.
- Exports/frame cleanup, in/out writeback, performance counters, and complete 07-S6/07~12 acceptance.

## Large-File Note

This slice adds a small focused reference-local module instead of extending `backend_aot_c_scalar_locals.c`. The edits in `backend_aot_c_lowering_values.c` and `backend_aot_c_lowering_generic_logical.c` are localized to existing `TO_STRING`/`TO_OBJECT` and immediate truthiness templates; broader extraction remains appropriate before adding another responsibility to either large file.
