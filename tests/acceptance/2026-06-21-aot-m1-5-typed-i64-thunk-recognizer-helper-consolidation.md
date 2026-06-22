# AOT M1.5 07-S5 Typed I64 Thunk Recognizer Helper Consolidation

Date: 2026-06-22 09:21:19 +08:00

## Scope

This support slice keeps the current typed i64 direct-call behavior but consolidates duplicated recognizer guards in `backend_aot_c_typed_i64_thunks.c`.

Covered:

- `backend_aot_c_try_get_i64_arg0_unary_return()` now owns the common one-argument i64 unary-return contract.
- The negate and bitwise-not recognizers reuse that helper for `NEG_SIGNED` and `BITWISE_NOT`.
- `backend_aot_c_try_get_i64_arg0_arg1_binary_return()` now owns the common two-argument simple binary-return contract.
- The subtract and bitwise-and/or/xor recognizers reuse that helper for `SUB_SIGNED`, `SUB_SIGNED_PLAIN_DEST`, `BITWISE_AND`, `BITWISE_OR`, and `BITWISE_XOR`.
- `ADD` and `MUL` keep dedicated recognizers because they support additional load-stack instruction variants.
- The typed thunk module dropped from 762 physical / 682 non-empty lines to 670 physical / 600 non-empty lines.

Out of scope:

- Adding new typed thunk behavior shapes.
- General multi-argument typed ABI.
- Inline struct direct call/return lowering.
- `in` / `out` writeback templates.
- Deopt/dynamic bridge boxing templates.
- Marking 07-S5, 07, or M1.5 complete.

## RED

The updated call contract failed before implementation:

- `zr_vm_aot_c_call_contracts_test`: missing `backend_aot_c_try_get_i64_arg0_unary_return(`.

## GREEN

Focused WSL GCC validation:

- `zr_vm_aot_c_call_contracts_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test`: 3/0.

Broader WSL GCC focused AOT group:

- `zr_vm_aot_c_typed_direct_call_shared_library_smoke_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test`: 3/0.
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

This is a support refactor for the 07-S5 typed direct-call route. It does not add behavior coverage or close 07-S5, but it reduces recognizer duplication before further direct-return shapes are added.
