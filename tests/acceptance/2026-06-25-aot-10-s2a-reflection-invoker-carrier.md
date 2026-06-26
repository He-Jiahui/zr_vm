# AOT 10-S2A reflection invoker carrier

## Scope

- Plan slice: `docs/plans/aot/10-reflection.md` 10-S2 sub-slice.
- Goal: add the first AOT reflection invoker carrier for the current generated-C target shape.
- Implemented:
  - `ZR_VM_AOT_ABI_VERSION` advanced to `7u`.
  - `FZrAotReflectionInvoker` added to the public AOT ABI.
  - `SZrAotMethodInfo.invoker` added.
  - Generated C emits shared `zr_aot_invoker_entry_thunk`.
  - Generated MethodInfo records bind `.invoker = zr_aot_invoker_entry_thunk`.

## RED

- `zr_vm_aot_c_shared_library_smoke_test` failed to compile after adding the
  `module->methodInfos[0]->invoker` assertion because `SZrAotMethodInfo` had no
  `invoker` field.
- `zr_vm_aot_c_frame_setup_contracts_test` failed on missing invoker ABI/source
  contract text.

## GREEN

- `zr_vm_common/include/zr_vm_common/zr_aot_abi.h` now defines the invoker type
  and MethodInfo field.
- `backend_aot_c_method_metadata.c` emits a shared entry-thunk invoker and binds
  each generated MethodInfo to it.
- `backend_aot_c_emitter.c` emits invokers before MethodInfo declarations.

## Validation

- WSL gcc:
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 1/0
  - `zr_vm_aot_c_return_contracts_test` 1/0
- WSL clang:
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 1/0
  - `zr_vm_aot_c_return_contracts_test` 1/0
- Windows MSVC Debug:
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0, 8 ignored Unix-only branches
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 1/0, 1 ignored Unix-only branch
  - `zr_vm_aot_c_return_contracts_test` 1/0

## Notes

- This is an ABI carrier and generated-C registration point for the current
  `FZrAotEntryThunk(SZrState *)` target shape.
- Full 10-S2 remains open: reflection `Method.Invoke` argument unpacking,
  return packing, token/registry consumption, and future typed-target signature
  buckets are not complete in this slice.
