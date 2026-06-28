# AOT 07-S2/S4 Bool Condition-Copy Branch Local

Date: 2026-06-28 11:25:49 +08:00

Status: complete for this focused slice. This does not close all of 07-S2/S4 or 07/M1.5.

## Scope

- Remove the remaining frame-backed bool condition read after a forced bool condition stack copy.
- Focused case: `yes() && no()` in the bool short-circuit logical shared-library smoke, where `dstSlot=15 srcSlot=16` is copied as a bool and the next `JUMP_IF_BOOL_FALSE` consumes slot 15.
- Align scalar-local write tracking with value-level `CopyStack` plus `Sync*Local` boundaries.

## RED

- Added generated-C assertions requiring the slot 15 branch to use `if (!zr_aot_b15) {`.
- Added a regression guard forbidding `zr_aot_condition = &frame.slotBase[15].value;`.
- WSL GCC RED failed in `test_aot_c_generated_shared_library_executes_bool_short_circuit_logical_expressions` with `Expected Non-NULL` before the scalar-local proof change.

## GREEN

- `backend_aot_c_scalar_locals_record_exec_instruction_write()` now tracks the stack-copy destination slot explicitly and validates both source and destination bounds.
- Stack-copy write tracking still prefers the current or declared source kind. If no source kind is known, it now falls back to the declared destination kind, matching the generated value-copy path that immediately syncs declared destination scalar locals from the frame slot.
- The `yes() && no()` branch target can now prove all incoming paths have written `zr_aot_b15`, so generated C emits `zr_aot_jump_if_bool_false_scalar_local` and `if (!zr_aot_b15)`.
- Source contracts lock the destination-kind fallback.

## Verification

- WSL GCC:
  - `zr_vm_aot_c_logical_shared_library_smoke_test`: 5 tests, 0 failures.
  - `zr_vm_aot_c_source_contracts_test`: 22 tests, 0 failures.
- WSL Clang:
  - `zr_vm_aot_c_logical_shared_library_smoke_test`: 5 tests, 0 failures.
  - `zr_vm_aot_c_source_contracts_test`: 22 tests, 0 failures.
- Windows MSVC Debug:
  - `zr_vm_aot_c_logical_shared_library_smoke_test`: 0 failures / 5 ignored Unix-only tests.
  - `zr_vm_aot_c_source_contracts_test`: 22 tests, 0 failures.
- Generated-C inspection confirms `ins_58` contains `if (!zr_aot_b15)` and no longer contains the slot 15 frame condition read.

## Notes

- This is a focused bool condition-copy branch slice. It does not remove every value-level stack copy, and it does not prove complete typed function-body zero-frame generation.
- Remaining 07 work includes broader generic/dynamic/string boundaries, GC roots, exports, frame cleanup, wider byte-frame narrowing, performance/SZrValue counters, and full typed function-body zero-frame proof.
