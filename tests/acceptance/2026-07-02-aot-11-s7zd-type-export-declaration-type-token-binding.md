# AOT 11-S7ZD / 12-S7 Type Export Declaration Type-Token Binding

Timestamp: 2026-07-02 02:22:47 +08:00

Status: completed support sub-slice for current-module writer-input export token binding.

Completed items:
- `SZrAotManifestExportDeclaration` now exposes `hasTypeTokenBinding` and `typeToken`.
- The CLI AOT export bridge binds `type` export declarations to matching current-function `TYPE_DEF` metadata token records.
- Generated AOT C emits `manifest.export[i].typeToken = 0x...` diagnostics next to the existing export declaration markers.
- Source contracts cover the public writer fields and generated-C marker text.

RED/GREEN:
- RED: the new writer-options type export binding test failed to compile because the writer export declaration struct had no type-token binding fields.
- GREEN: the bridge resolves `type List` to the fixture's local `TYPE_DEF` token and generated C reports `0x02000001`.

Verification:
- WSL GCC: `zr_vm_cli_aot_writer_options_test` 18/0, `zr_vm_aot_c_source_contracts_test` 24/0, `zr_vm_aot_c_code_stripping_test` 10/0, focused CTest 2/2.
- WSL clang: same direct counts, focused CTest 2/2.
- Windows MSVC Debug: same direct counts, focused CTest 2/2.

Remaining:
- Persistent export manifest/table writer.
- Compacted-token file publication.
- Cross-module provider loading/version binding.
- Complete zrp metadata sweep/pruning and full trim analyzer.
