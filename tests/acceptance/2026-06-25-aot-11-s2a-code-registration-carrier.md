# AOT 11-S2A code registration carrier

## Scope

- Plan slice: `docs/plans/aot/11-metadata.md` 11-S2 sub-slice.
- Goal: expose a generated-C code registration carrier that can later be consumed by token/runtime metadata resolution.
- Implemented:
  - `ZR_VM_AOT_ABI_VERSION` advanced to `8u`.
  - `SZrAotCodeRegistration` added to the public AOT ABI.
  - `ZrAotCompiledModule.codeRegistration` added.
  - Generated C emits `zr_aot_reflection_invokers[]`.
  - Generated C emits `zr_aot_code_registration` pointing at function, method, invoker, and GC descriptor tables.
  - Runtime descriptor validation rejects missing or mismatched code registration before dereferencing it.

## RED

- `zr_vm_aot_c_shared_library_smoke_test` failed to compile after adding
  `module->codeRegistration` assertions because `ZrAotCompiledModule` had no
  `codeRegistration` field.
- Frame/source contracts failed on missing ABI v8, code registration structure,
  invoker table, and generated-C registration text.
- Descriptor diagnostics then exposed the runtime gap: an ABI v8 shared library
  with `codeRegistration = ZR_NULL` aborted before a diagnostic could be reported.

## GREEN

- Public ABI now contains the code registration carrier.
- Generated C binds `zr_aot_code_registration` to the same arrays exposed by the
  legacy module descriptor fields.
- Shared-library smoke verifies descriptor pointer equality for function and
  method tables, a single invoker bucket, and GC descriptor count parity.
- Descriptor diagnostics now reject `codeRegistration = ZR_NULL`, mismatched
  table pointers/counts, and missing invoker tables with AOT descriptor
  validation errors.

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

- This slice closes the generated-C carrier and descriptor-safety validation.
- Full 11-S2 remains open: module-load registration into a metadata runtime,
  token to function/layout resolution, and runtime caches are not complete.
