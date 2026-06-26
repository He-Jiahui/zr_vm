# AOT 09-S5A · Public Boxing/Unboxing Bridge Boundary

## Scope

- Close the boxing/unboxing half of 09-S5 for generated AOT C.
- Keep `TO_OBJECT` and `TO_STRUCT` as explicit typed↔dynamic runtime boundaries.
- Do not change value-type layout, boxing object payload semantics, allocation policy, or FFI pinning.

## RED

- `tests/parser/test_aot_c_global_contracts.c` was extended to require:
  - `zr_vm_core/include/zr_vm_core/bridge.h`
  - `zr_vm_core/src/zr_vm_core/bridge.c`
  - `ZrCore_Bridge_BoxTyped(...)`
  - `ZrCore_Bridge_UnboxTyped(...)`
  - AOT runtime `TO_OBJECT` / `TO_STRUCT` helpers calling the bridge API instead of directly calling `ZrCore_Execution_ToObject` / `ZrCore_Execution_ToStruct`.
- The first RED failed because the bridge header/source did not exist and the runtime source still used the execution conversion entry points directly.

## GREEN

- Added the public bridge API:
  - `ZrCore_Bridge_BoxTyped(state, callInfo, value, typeInfo, outValue)`
  - `ZrCore_Bridge_UnboxTyped(state, callInfo, value, typeInfo, outValue)`
- The bridge implementation delegates to the existing execution conversion functions so current layout and error behavior stay unchanged.
- Updated `zr_vm_library/src/zr_vm_library/aot_runtime.c` and the mirrored `zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c` to call the bridge API for `TO_OBJECT` and `TO_STRUCT`.
- Guardrail coverage classifies the bridge functions as allowed explicit runtime boundaries.

## Validation

- WSL gcc direct:
  - `zr_vm_aot_c_global_contracts_test`: 8/0
  - `zr_vm_aot_c_global_shared_library_smoke_test`: 10/0
  - `zr_vm_aot_c_guardrail_contracts_test`: 6/0
  - `zr_vm_aot_c_value_type_shared_library_smoke_test`: 2/0
  - `zr_vm_aot_c_constant_contracts_test`: 5/0
  - `zr_vm_aot_gc_root_frame_test`: 4/0
- WSL clang direct:
  - `zr_vm_aot_c_global_contracts_test`: 8/0
  - `zr_vm_aot_c_global_shared_library_smoke_test`: 10/0
  - `zr_vm_aot_c_guardrail_contracts_test`: 6/0
  - `zr_vm_aot_c_value_type_shared_library_smoke_test`: 2/0
  - `zr_vm_aot_c_constant_contracts_test`: 5/0
  - `zr_vm_aot_gc_root_frame_test`: 4/0
- Windows MSVC Debug direct:
  - `zr_vm_aot_c_global_contracts_test`: 8/0
  - `zr_vm_aot_c_global_shared_library_smoke_test`: 10/0, 10 ignored Unix shared-library branches
  - `zr_vm_aot_c_guardrail_contracts_test`: 6/0
  - `zr_vm_aot_c_value_type_shared_library_smoke_test`: 2/0, 1 ignored Unix execution branch
  - `zr_vm_aot_c_constant_contracts_test`: 5/0
  - `zr_vm_aot_gc_root_frame_test`: 4/0

## Remaining Work

- 09-S5 FFI pin/unpin is still open.
- This slice does not implement new boxing allocation optimizations or change generated value-type storage.
