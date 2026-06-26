# 2026-06-26 AOT 12-S7X release symbol stripping CLI policy

## Scope

- Connected project AOT mode policy to generated-symbol stripping.
- `aotMode: "full-aot"` now sets both `requireFullAot` and `stripGeneratedSymbols` on `SZrAotWriterOptions`.
- Default or hybrid project mode clears both options, so hybrid output keeps readable generated symbols.
- CLI `--emit-aot-c` full-AOT project output now contains `/* symbol_stripping.generatedSymbols = 1 */`.

## RED

- Extended `zr_vm_cli_project_incremental_test` so full-AOT project writer options must set `stripGeneratedSymbols`.
- Extended the hybrid default test so hybrid project options must clear `stripGeneratedSymbols`.
- Extended the full-AOT `--emit-aot-c` fixture so generated C must contain the symbol-stripping marker.
- The first focused WSL gcc run failed 3/11 on those new expectations.

## GREEN

- `ZrCli_Compiler_ApplyProjectAotWriterOptions()` now maps `aotMode: "full-aot"` to `stripGeneratedSymbols = ZR_TRUE`.
- The same helper maps default/hybrid mode to `stripGeneratedSymbols = ZR_FALSE`, preserving readable hybrid diagnostics.
- No new module boundary was added because this is a one-line policy extension inside the existing project AOT option helper.

## Validation

- WSL gcc:
  - `zr_vm_cli_project_incremental_test`: 11/0.
  - `zr_vm_aot_c_generic_call_typed_test`: 7/0.
  - `zr_vm_aot_llvm_symbol_stripping_test`: 2/0.
  - CTest `cli_project_incremental|aot_c_generic_call_typed|aot_llvm_symbol_stripping`: 3/3.
- WSL clang:
  - `zr_vm_cli_project_incremental_test`: 11/0.
  - `zr_vm_aot_c_generic_call_typed_test`: 7/0.
  - `zr_vm_aot_llvm_symbol_stripping_test`: 2/0.
  - CTest `cli_project_incremental|aot_c_generic_call_typed|aot_llvm_symbol_stripping`: 3/3.
- Windows MSVC Debug:
  - Rebuilt `zr_vm_parser_shared` and focused CLI/parser tests.
  - `zr_vm_cli_project_incremental_test`: 11/0.
  - `zr_vm_aot_c_generic_call_typed_test`: 7 tests, 0 failures, 3 ignored by existing Unix shared-library guards.
  - `zr_vm_aot_llvm_symbol_stripping_test`: 2/0.
  - CTest `cli_project_incremental|aot_c_generic_call_typed|aot_llvm_symbol_stripping`: 3/3.

## Notes

- This slice defines the current CLI-visible release policy as the existing full-AOT project mode.
- It does not add a separate `release` manifest field.
- It does not implement the remaining trim analyzer, attribute/annotation suppression, metadata sweep/pruning, or default-min metadata policy work.
