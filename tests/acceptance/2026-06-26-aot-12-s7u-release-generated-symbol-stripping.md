# 2026-06-26 AOT 12-S7U release generated symbol stripping option

## Scope

- Added `SZrAotWriterOptions.stripGeneratedSymbols` as an explicit writer-level opt-in.
- AOT C output now reports `symbol_stripping.generatedSymbols`.
- When enabled, generic monomorphization and generic sharing private helper symbols use stable generated IDs instead of type-derived names.
- Shared generic slot `debugName` strings are stripped to stable `generic#<id>` values.
- Default output remains unchanged and still emits readable generic helper/debug names for diagnostics.

## RED

- `zr_vm_aot_c_generic_call_typed_test` failed to compile after adding the RED fixture because `SZrAotWriterOptions` had no `stripGeneratedSymbols` member.
- `test_aot_c_source_contracts.c` was extended to require the public option and emitter plumbing before production support existed.

## GREEN

- Added `backend_aot_option_strip_generated_symbols()`.
- Threaded `stripGeneratedSymbols` through `backend_aot_c_emitter.c` into `backend_aot_c_generic_monomorphization` and `backend_aot_c_generic_sharing`.
- Updated source contracts, frame setup contracts, and generic generated-C assertions.

## Validation

- WSL gcc:
  - `zr_vm_aot_c_code_stripping_test`: 4/0.
  - CTest `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format`: 3/3.
  - `zr_vm_aot_c_source_contracts_test`: 20/0.
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1/0.
- WSL clang:
  - Initial fast-target run exposed a stale `test_aot_c_code_stripping.c.o` built against the old writer-options layout; a clean rebuild fixed the artifact mismatch.
  - `zr_vm_aot_c_code_stripping_test`: 4/0.
  - CTest `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format`: 3/3.
  - `zr_vm_aot_c_source_contracts_test`: 20/0.
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1/0.
- Windows MSVC Debug:
  - Rebuilt `zr_vm_parser_shared` and focused parser tests.
  - `zr_vm_aot_c_code_stripping_test`: 4/0.
  - CTest `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format`: 3/3.
  - `zr_vm_aot_c_source_contracts_test`: 20/0.
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1/0.

## Notes

- This completes the writer-level generated-symbol stripping option for current AOT C generic private helpers.
- Broader release-mode policy integration, CLI/project automatic option wiring, full trim analyzer, attribute/annotation suppression, and metadata sweep/pruning remain open.
