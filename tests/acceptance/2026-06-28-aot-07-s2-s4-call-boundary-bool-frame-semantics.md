# AOT 07-S2/S4 Call-Boundary Bool Frame Semantics

Date: 2026-06-28 10:55:19 +08:00

Status: complete for this focused slice. This does not close all of 07-S2/S4 or 07/M1.5.

## Scope

- Preserve required value-slot materialization for bool constants used as runtime stack-call arguments.
- Prefer a bool scalar source when a typed bool direct-call result is immediately forced back into a value slot for a following bool condition.
- Focused cases:
  - `choose(true)` in the generic primitive equality boundary helper.
  - `yes() && no()` in the bool short-circuit logical shared-library smoke.

## RED

- The existing logical shared-library smoke exposed two failures:
  - generic primitive equality returned the wrong value because a bool argument constant was elided from the call argument frame slot.
  - bool short-circuit execution failed because a forced condition stack-copy wrote an i64 value slot from stale scalar kind state.
- Added generated-C assertions requiring:
  - bool argument constants at call boundaries to still write `ZR_VALUE_TYPE_BOOL`.
  - the direct bool call result copy `dstSlot=15 srcSlot=16` to lower as `zr_aot_scalar_stack_copy_bool`.
  - no `zr_aot_scalar_stack_copy_i64 dstSlot=15 srcSlot=16` regression on that edge.

## GREEN

- `backend_aot_c_scalar_locals_instruction_reads_call_argument_slot()` now treats `functionSlot + 1 .. functionSlot + argumentCount` as value-frame reads for stack-call opcodes.
- Scalar-local write tracking now records the exact current kind with `backend_aot_c_scalar_locals_set_slot()`; reset instructions clear slots, call results use SemIR destination kind when available, and stack-copy destinations inherit the source kind.
- `backend_aot_c_scalar_stack_copy_try_prefer_available_source_type()` accepts a bool scalar source for forced value-slot writes when the source is the previous call result. This lets the generated C write a bool frame slot for the next condition instead of falling back to stale i64 state.
- Source contracts now lock the previous-call-result bool preference path.

## Verification

- WSL GCC:
  - `zr_vm_aot_c_logical_shared_library_smoke_test`: 5 tests, 0 failures.
  - `zr_vm_aot_c_shared_library_smoke_test`: 13 tests, 0 failures.
  - `zr_vm_aot_c_source_contracts_test`: 21 tests, 0 failures.
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1 test, 0 failures.
- WSL Clang:
  - `zr_vm_aot_c_logical_shared_library_smoke_test`: 5 tests, 0 failures.
  - `zr_vm_aot_c_shared_library_smoke_test`: 13 tests, 0 failures.
  - `zr_vm_aot_c_source_contracts_test`: 21 tests, 0 failures.
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1 test, 0 failures.
- Windows MSVC Debug:
  - `zr_vm_aot_c_logical_shared_library_smoke_test`: 0 failures / 5 ignored Unix-only tests.
  - `zr_vm_aot_c_shared_library_smoke_test`: 0 failures / 13 ignored Unix-only tests.
  - `zr_vm_aot_c_source_contracts_test`: 21 tests, 0 failures.
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1 test, 0 failures.

## Notes

- WSL Clang still reports the pre-existing generated string-equality warning around `!zr_aot_equal != 0u`; this slice did not change that path.
- Remaining 07 work includes broader generic/dynamic/string boundaries, GC roots, exports, frame cleanup, wider byte-frame narrowing, performance/SZrValue counters, and full typed function-body zero-frame proof.
