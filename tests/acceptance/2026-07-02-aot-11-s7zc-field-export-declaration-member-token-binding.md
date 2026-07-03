# 11-S7ZC / 12-S7 Field Export Declaration Member-Token Binding

## Scope

- Bind current-module field `exports` declarations to existing typed exported variable `MEMBER_DEF` metadata tokens.
- Reuse the writer-visible `SZrAotManifestExportDeclaration.hasMemberTokenBinding/memberToken` carrier.
- Surface the bound field token in generated-C manifest diagnostics.

Out of scope: type export target binding, persistent export manifest/table writing, compacted-token file publication,
cross-module provider loading/version binding, full metadata sweep/pruning, full trim analyzer integration, and ABI
drift/deopt closure.

## RED

- `tests/cli/test_cli_aot_writer_options.c` added
  `test_cli_aot_writer_options_binds_field_export_declaration_to_member_token`.
- WSL GCC direct `zr_vm_cli_aot_writer_options_test` failed with `Expected TRUE Was FALSE` because the export binding
  helper only accepted method declarations backed by typed exported function symbols.

## Implementation

- `compiler_aot_exports.c` now maps writer declaration kind to the matching typed export symbol kind:
  method declarations bind function symbols, and field declarations bind variable symbols.
- The existing token guard remains unchanged: only nonzero `MEMBER_DEF` tokens are bound.
- The generated-C manifest marker remains the same `manifest.export[i].memberToken = 0x...` diagnostic used by method
  export declarations.
- `tests/cli/test_cli_aot_writer_options.c` now covers field declaration binding and verifies the generated-C token
  marker for `Widget.value`.

## Validation

- WSL GCC direct: `zr_vm_cli_aot_writer_options_test` 17/0, `zr_vm_aot_c_source_contracts_test` 24/0,
  `zr_vm_aot_c_code_stripping_test` 10/0.
- WSL clang direct: same 17/0, 24/0, and 10/0.
- Windows MSVC Debug direct: same 17/0, 24/0, and 10/0.
- Focused CTest on WSL GCC, WSL clang, and Windows MSVC Debug passed 2/2 for registered
  `cli_aot_writer_options|aot_c_code_stripping`.

## Status

Complete as a support slice. Remaining 11-S7/12-S7 work is type export target binding, persistent export manifest/table
writer, compacted-token file publication, cross-module provider loading/version binding, full metadata sweep/pruning,
full trim analyzer integration, and ABI drift/deopt coverage.
