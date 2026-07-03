# 11-S7ZB / 12-S7 Export Declaration Member-Token Binding

## Scope

- Bind current-module method `exports` declarations to existing typed exported `MEMBER_DEF` metadata tokens.
- Publish the binding through `SZrAotManifestExportDeclaration.hasMemberTokenBinding/memberToken`.
- Surface the binding in generated-C manifest diagnostics.

Out of scope: type/field export target binding, persistent export manifest/table writing, compacted-token file
publication, cross-module provider loading/version binding, full metadata sweep/pruning, full trim analyzer integration,
and ABI drift/deopt closure.

## RED

- `tests/cli/test_cli_aot_writer_options.c` added
  `test_cli_aot_writer_options_binds_method_export_declaration_to_member_token`.
- The WSL GCC focused build failed because `SZrAotManifestExportDeclaration` had no
  `hasMemberTokenBinding` / `memberToken` fields.

## Implementation

- `zr_vm_parser/include/zr_vm_parser/writer.h` now exposes writer-visible member-token binding fields on manifest export
  declarations.
- `zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot_exports.c` scans the current function's typed exported symbols for
  matching method targets and binds only nonzero `MEMBER_DEF` tokens.
- `ZrCli_Compiler_ApplyProjectAotExportDeclarations()` now receives the current function context from
  `ZrCli_Compiler_ApplyProjectAotPreserveRules()`.
- `backend_aot_c_emitter.c` emits `manifest.export[i].memberToken = 0x...` when the binding is present.
- `tests/parser/test_aot_c_source_contracts.c` protects the public writer fields and generated-C marker.

## Validation

- WSL GCC direct: `zr_vm_cli_aot_writer_options_test` 16/0, `zr_vm_aot_c_source_contracts_test` 24/0,
  `zr_vm_aot_c_code_stripping_test` 10/0.
- WSL clang direct: same 16/0, 24/0, and 10/0.
- Windows MSVC Debug direct: same 16/0, 24/0, and 10/0.
- Focused CTest on WSL GCC, WSL clang, and Windows MSVC Debug passed 2/2 for registered
  `cli_aot_writer_options|aot_c_code_stripping`.

## Status

Complete as a support slice. Remaining 11-S7/12-S7 work is persistent export manifest/table writer, type/field export
token binding, compacted-token file publication, cross-module provider loading/version binding, full metadata
sweep/pruning, full trim analyzer integration, and ABI drift/deopt coverage.
