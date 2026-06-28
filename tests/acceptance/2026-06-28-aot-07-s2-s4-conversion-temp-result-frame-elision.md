# AOT 07-S2/S4 Conversion Temporary Result Frame Elision

Date: 2026-06-28 09:53:59 +08:00

Status: complete for this focused slice. This does not close all of 07-S2/S4 or 07/M1.5.

## Scope

- Remove value-slot materialization for scalar conversion temporary results when the temp is consumed by scalar stack-copy and then overwritten before any later read.
- Focused case: bool-to-u64 and bool-to-f64 conversion temps in the generic primitive conversion smoke.

## RED

- Extended the generic primitive conversion shared-library smoke to also reject:
  - `frame.slotBase[6].value`
  - `frame.slotBase[7].value`
- RED showed `TO_UINT`/`TO_FLOAT` conversion temps were already reading bool scalar locals but still writing their temporary destination value slots.

## GREEN

- `backend_aot_c_scalar_locals_result_scan_live_value_block()` now treats an old result value as dead when the same slot is later overwritten by any scalar kind and the old value has not been read.
- The implementation reuses `backend_aot_c_scalar_locals_instruction_overwrites_slot_as_any_scalar()` and adds the required static forward declaration.
- Generated C for the focused chain now emits only:
  - `zr_aot_u6 = zr_aot_b0 ? (TZrUInt64)1u : (TZrUInt64)0u;`
  - `zr_aot_u4 = zr_aot_u6;`
  - `zr_aot_f7 = zr_aot_b0 ? (TZrFloat64)1.0 : (TZrFloat64)0.0;`
  - `zr_aot_f5 = zr_aot_f7;`
- No `frame.slotBase[6].value` or `frame.slotBase[7].value` remains in that generated function.

## Verification

- WSL GCC:
  - `zr_vm_aot_c_shared_library_smoke_test`: 13 tests, 0 failures.
  - `zr_vm_aot_c_source_contracts_test`: 21 tests, 0 failures.
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1 test, 0 failures.
- WSL Clang:
  - `zr_vm_aot_c_shared_library_smoke_test`: 13 tests, 0 failures.
  - `zr_vm_aot_c_source_contracts_test`: 21 tests, 0 failures.
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1 test, 0 failures.
- Windows MSVC Debug:
  - shared-library smoke target builds; runtime test reports 0 failures / 13 ignored Unix-only tests.
  - `zr_vm_aot_c_source_contracts_test`: 21 tests, 0 failures.
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1 test, 0 failures.

## Remaining Work

- Real variable destination slots are still materialized where runtime boundaries need them.
- Generic runtime boundaries, dynamic/string cases, GC roots, exports, frame cleanup, wider byte-frame narrowing, performance counters, and full typed function-body zero-frame goals remain later 07 work.
