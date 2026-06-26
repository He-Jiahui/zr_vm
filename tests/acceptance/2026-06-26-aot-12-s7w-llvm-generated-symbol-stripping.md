# 2026-06-26 AOT 12-S7W LLVM generated-symbol stripping parity

## Scope

- Extended writer-level `stripGeneratedSymbols` to the LLVM backend.
- Kept default LLVM private function symbols readable as `@zr_aot_fn_<flatIndex>`.
- In stripped mode, generated LLVM private function definitions, thunk-table entries, entry thunk calls, and static direct-call references now use stable IDs such as `@zr_fn_g0`.
- Preserved the public ABI export `@ZrVm_GetAotCompiledModule`.

## RED

- Added `zr_vm_aot_llvm_symbol_stripping_test`.
- The first focused WSL gcc run failed 2/2 because generated LLVM text had no `; symbol_stripping.generatedSymbols` marker and no stripped `@zr_fn_g0` / `@zr_fn_g1` private symbols.

## GREEN

- Added `backend_aot_llvm_format_function_symbol(...)` to centralize LLVM generated function symbol formatting.
- Threaded `stripGeneratedSymbols` through LLVM emitter, lowering context, function body writing, static direct-call lowering, thunk table writing, and entry thunk emission.
- LLVM output now emits `; symbol_stripping.generatedSymbols = 0/1` in the module header.
- The stripped fixture rejects `@zr_aot_fn_` symbol references while continuing to require the public `ZrVm_GetAotCompiledModule` export.

## Validation

- WSL gcc:
  - `zr_vm_aot_llvm_symbol_stripping_test`: 2/0.
  - `zr_vm_aot_c_code_stripping_test`: 4/0.
  - `zr_vm_aot_c_generic_call_typed_test`: 7/0.
  - CTest `aot_llvm_symbol_stripping|aot_c_code_stripping|aot_c_generic_call_typed`: 3/3.
- WSL clang:
  - `zr_vm_aot_llvm_symbol_stripping_test`: 2/0.
  - `zr_vm_aot_c_code_stripping_test`: 4/0.
  - `zr_vm_aot_c_generic_call_typed_test`: 7/0.
  - CTest `aot_llvm_symbol_stripping|aot_c_code_stripping|aot_c_generic_call_typed`: 3/3.
- Windows MSVC Debug:
  - Rebuilt `zr_vm_parser_shared` and focused parser tests.
  - `zr_vm_aot_llvm_symbol_stripping_test`: 2/0.
  - `zr_vm_aot_c_code_stripping_test`: 4/0.
  - `zr_vm_aot_c_generic_call_typed_test`: 7 tests, 0 failures, 3 ignored by existing Unix shared-library guards.
  - CTest `aot_llvm_symbol_stripping|aot_c_code_stripping|aot_c_generic_call_typed`: 3/3.

## Notes

- This slice only strips LLVM private generated function symbols and references.
- It does not rename LLVM basic-block labels.
- Public ABI/export symbols remain stable and visible.
