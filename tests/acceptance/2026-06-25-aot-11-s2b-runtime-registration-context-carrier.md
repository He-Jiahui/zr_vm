# AOT 11-S2B runtime code registration context carrier

## Scope

- Plan slice: `docs/plans/aot/11-metadata.md` 11-S2 sub-slice.
- Goal: make the runtime consume the generated `SZrAotCodeRegistration`
  carrier through loaded-module records, generated module context, and generated
  frames.
- Implemented:
  - Loaded AOT module records cache `descriptor->codeRegistration`.
  - `ZrAotGeneratedModuleContext` carries `codeRegistration`.
  - `ZrAotGeneratedFrame` carries `codeRegistration`.
  - Runtime MethodInfo lookup reads `record->codeRegistration->methodInfos`.
  - Runtime function thunk lookup reads `record->codeRegistration->functionPointers`.
  - Callable constant materialization, native direct calls, meta calls, and
    static direct calls now use the code registration carrier.
  - Generated C frame setup assigns
    `frame.codeRegistration = zr_aot_context.codeRegistration`.

## RED

- `zr_vm_aot_c_frame_setup_contracts_test` failed because the runtime header
  did not expose a `SZrAotCodeRegistration` pointer on generated context/frame
  carriers and generated frame setup did not copy it into `frame`.
- The same contract also required runtime source consumption through
  `record->codeRegistration->methodInfos`, `record->codeRegistration->functionPointers`,
  and `context->codeRegistration = record->codeRegistration`.
- A follow-up RED showed the loaded-record binding was still represented as a
  stack-value assignment instead of a record-pointer binding.

## GREEN

- Runtime loaded records bind the descriptor registration through a small record
  binding function.
- Generated module context and generated frame publish the registration pointer.
- Compatibility `functionThunks` fields are derived from
  `codeRegistration->functionPointers`.
- Runtime direct-call, static-call, meta-call, and callable-constant paths now
  consume the registration table from the record.

## Validation

- WSL gcc:
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
- WSL clang:
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
- Windows MSVC Debug:
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0, 8 ignored Unix-only branches
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0, 2 ignored Unix-only branches

## Notes

- This slice closes only the runtime record/context/frame consumption carrier
  for generated code registration.
- Full 11-S2 remains open: module-load registration into `SZrMetadataRuntime`,
  token to function/layout resolution, and runtime caches are not complete.
