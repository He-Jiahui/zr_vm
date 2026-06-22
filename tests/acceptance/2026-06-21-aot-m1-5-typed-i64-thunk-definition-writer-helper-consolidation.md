# AOT M1.5 07-S5 Typed I64 Thunk Definition Writer Helper Consolidation

Date: 2026-06-22 10:11:42 +08:00

## Scope

This support slice keeps the current typed i64 direct-call behavior but consolidates repeated thunk definition writer templates in `backend_aot_c_typed_i64_thunks.c`.

Covered:

- `backend_aot_c_write_i64_no_arg_thunk_definition()` now owns the no-argument i64 thunk definition template.
- `backend_aot_c_write_i64_one_arg_thunk_definition()` now owns the one-argument i64 thunk definition template.
- `backend_aot_c_write_i64_two_arg_thunk_definition()` now owns the two-argument i64 thunk definition template.
- Existing direct-return shapes are preserved: no-arg constant, one-arg identity, unary negate, unary bitwise not, one-arg constant add/sub/mul/bitwise and/or/xor, and two-arg add/sub/mul/bitwise and/or/xor.
- The call source contract locks the helper names so future changes keep the shared writer surface explicit.
- The typed thunk module is 793 physical / 707 non-empty lines after the refactor.

Out of scope:

- Adding new typed thunk behavior shapes.
- General multi-argument typed ABI.
- Bool, u64, or f64 typed thunk families.
- Inline struct direct call/return lowering.
- `in` / `out` writeback templates.
- Deopt/dynamic bridge boxing templates.
- Marking 07-S5, 07, or M1.5 complete.

## RED

The updated call contract failed before implementation:

- `zr_vm_aot_c_call_contracts_test`: missing `backend_aot_c_write_i64_no_arg_thunk_definition(`.

## GREEN

Focused WSL GCC validation:

- `zr_vm_aot_c_call_contracts_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_shared_library_smoke_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test`: 6/0.

Broader WSL GCC focused AOT group:

- `zr_vm_aot_c_call_contracts_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_shared_library_smoke_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test`: 6/0.
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

This is a support refactor for the 07-S5 typed direct-call route. It does not add behavior coverage or close 07-S5, but it reduces writer duplication before wider typed return ABI work.
