# AOT M1.5 07-S5 Static I64 Typed Direct-Call Stack-Sync Elision

Date: 2026-06-22 06:21:30 +08:00

## Scope

This slice removes the temporary destination stack-slot sync from the existing static no-arg and one-arg i64 typed direct-call routes when scalar-local liveness proves the destination value slot is no longer needed.

Covered:

- `backend_aot_write_c_static_direct_i64_no_arg_function_call()` and `backend_aot_write_c_static_direct_i64_one_arg_function_call()` now accept `syncStackSlot`.
- Function-body lowering computes that flag with `backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(functionIr, destinationSlot, instructionIndex)`.
- Proven scalar-local-only typed direct calls still assign the destination `zr_aot_sN` directly from `zr_aot_typed_i64_fn_N(...)`.
- Proven scalar-local-only generated C no longer emits `SZrTypeValue *zr_aot_typed_destination`, `zr_aot_static_i64_*_direct_call_sync_stack_slot`, or typed-destination `ZR_VALUE_FAST_SET`.
- Unproven destinations keep the old stack-slot sync fallback for frame-backed consumers.

Out of scope:

- Multiple typed arguments.
- Non-identity or general typed returns.
- Inline struct direct call/return lowering.
- `in` / `out` writeback templates.
- Deopt/dynamic bridge boxing templates.
- Global stack-sync removal outside the current no-arg and one-arg i64 direct-thunk routes.
- Marking 07-S5, 07, or M1.5 complete.

## RED

The updated call contract first failed before implementation:

- `zr_vm_aot_c_call_contracts_test`: 4/5, missing `TZrBool syncStackSlot`.

The typed direct-call shared-library smoke was updated to reject the old sync markers in the scalar-only no-arg and one-arg examples.

## GREEN

Focused WSL GCC validation:

- `zr_vm_aot_c_call_contracts_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_shared_library_smoke_test`: 2/0.

Broader WSL GCC focused AOT group:

- `zr_vm_aot_c_call_shared_library_smoke_test`: 5/0.
- `zr_vm_aot_c_shared_library_smoke_test`: 8/0.
- `zr_vm_aot_c_value_type_shared_library_smoke_test`: 1/0.
- `zr_vm_aot_c_power_contracts_test`: 2/0.
- `zr_vm_aot_c_power_shared_library_smoke_test`: 1/0.
- `zr_vm_aot_c_source_contracts_test`: 19/0.
- `zr_vm_aot_c_generic_numeric_contracts_test`: 1/0.
- `zr_vm_aot_c_generic_numeric_shared_library_smoke_test`: 1/0.
- `zr_vm_aot_c_global_shared_library_smoke_test`: 9/0.
- `zr_vm_aot_c_logical_contracts_test`: 4/0.
- `zr_vm_aot_c_logical_shared_library_smoke_test`: 4/0.
- `zr_vm_aot_c_typed_scalar_test`: 1/0.
- `zr_vm_aot_c_return_contracts_test`: 1/0.
- `zr_vm_aot_c_frame_setup_contracts_test`: 1/0.

## Notes

This closes the explicit stack-sync removal item for the currently expressible no-arg and one-arg i64 typed direct-call footholds. It does not prove that all typed-to-typed calls are stack-free; the fallback remains for call destinations that can still be read through the frame value slot.
