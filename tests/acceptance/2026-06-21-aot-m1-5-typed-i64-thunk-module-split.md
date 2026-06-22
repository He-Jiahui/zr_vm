# AOT M1.5 07-S5 Typed I64 Thunk Module Split

Date: 2026-06-22 07:54:45 +08:00

## Scope

This support slice moves the typed i64 thunk recognizers and writers out of the main AOT C emitter so later 07-S5 typed direct-call work can continue without growing `backend_aot_c_emitter.c`.

Covered:

- `backend_aot_c_typed_i64_thunks.{h,c}` now owns the no-arg, one-arg, and two-arg i64 typed thunk recognizers.
- The new module emits typed i64 thunk forward declarations and definitions.
- `backend_aot_c_emitter.c` includes the new header and delegates typed i64 thunk emission.
- `zr_vm_aot_c_call_contracts_test` now checks the new module for the existing source-contract needles.
- Existing constant, identity, add-constant, multiply-constant, add, subtract, and multiply typed thunk routes are preserved.
- `backend_aot_c_emitter.c` is 421 lines after the split; the new typed thunk module is 419 lines.
- The arithmetic typed direct-call smoke remains focused at 371 lines and the older typed direct-call smoke was not grown.

Out of scope:

- Adding new typed thunk expression coverage.
- General expression returns.
- General multi-argument typed ABI.
- Inline struct direct call/return lowering.
- `in` / `out` writeback templates.
- Deopt/dynamic bridge boxing templates.
- Marking 07-S5, 07, or M1.5 complete.

## RED

This was a support refactor after existing behavior was green. The relevant failure guard is structural: without moving the source contract to `backend_aot_c_typed_i64_thunks.c`, future recognizer work could regress outside the module that now owns typed i64 thunk emission.

The first focused WSL validation attempt timed out during unrelated semantic build work before the same focused targets were rerun successfully.

## GREEN

Focused WSL GCC validation:

- `zr_vm_aot_c_call_contracts_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test`: 2/0.

Broader WSL GCC focused AOT group:

- `zr_vm_aot_c_typed_direct_call_shared_library_smoke_test`: 5/0.
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

This slice preserves behavior and improves module boundaries. It is intentionally recorded as a support slice, not as completion of the typed return ABI plan.
