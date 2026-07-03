# 11-S7ZA / 12-S7 Export Declaration Writer Options

## Scope

- Bridge parsed `.zrp` `exports` declarations from `SZrLibrary_Project` into `SZrAotWriterOptions`.
- Publish a generated-C diagnostic surface for manifest export declarations: count, kind, and target.
- Keep the bridge in CLI-owned scratch storage so writer options never own project strings directly.

Out of scope: export target token binding, persistent export manifest/table writing, compacted-token file publication,
cross-module provider loading/version binding, full trim analyzer integration, and ABI drift/deopt closure.

## RED

- `tests/cli/test_cli_aot_writer_options.c` added
  `test_cli_aot_writer_options_bridges_manifest_export_declarations`.
- The WSL GCC focused build failed because `SZrAotWriterOptions` had no
  `manifestExportDeclarations` / `manifestExportDeclarationCount`, `SZrCliAotPreserveRoots` had no export-declaration
  scratch storage, and `ZR_AOT_MANIFEST_EXPORT_DECLARATION_*` did not exist.

## Implementation

- `zr_vm_parser/include/zr_vm_parser/writer.h` now exposes `SZrAotManifestExportDeclaration`,
  `EZrAotManifestExportDeclarationKind`, and writer-option fields for manifest export declarations.
- `zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot_exports.{h,c}` maps project export declarations to writer-level
  declarations, borrows validated project-owned target strings, and rejects null/empty targets or unknown kinds.
- `ZrCli_Compiler_ApplyProjectAotPreserveRules()` now resets and injects export declarations together with existing
  manifest root options.
- `backend_aot_c_emitter.c` emits `manifest.exports` and per-entry `manifest.export[i]` comments.
- `tests/CMakeLists.txt` links the new CLI helper into tests that compile `compiler_aot.c` directly.

## Validation

- WSL GCC builds: `zr_vm_cli_aot_writer_options_test`, `zr_vm_aot_c_source_contracts_test`,
  `zr_vm_aot_c_code_stripping_test`.
- WSL clang builds: same three focused targets.
- Windows MSVC Debug builds: same three focused targets.
- WSL GCC direct: `zr_vm_cli_aot_writer_options_test` 15/0, `zr_vm_aot_c_source_contracts_test` 24/0,
  `zr_vm_aot_c_code_stripping_test` 10/0.
- WSL clang direct: same 15/0, 24/0, and 10/0.
- Windows MSVC Debug direct: same 15/0, 24/0, and 10/0.
- Focused CTest on WSL GCC, WSL clang, and Windows MSVC Debug passed 2/2 for registered
  `cli_aot_writer_options|aot_c_code_stripping`; `aot_c_source_contracts` is direct-run only in these build trees.

## Status

Complete as a support slice. Remaining 11-S7/12-S7 work is persistent export manifest/table writer, export target token
binding, compacted-token file publication, cross-module provider loading/version binding, full metadata sweep/pruning,
full trim analyzer integration, and ABI drift/deopt coverage.
