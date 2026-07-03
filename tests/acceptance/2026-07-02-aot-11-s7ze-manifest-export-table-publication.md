# AOT 11-S7ZE / 12-S7 Manifest Export Table Publication

Timestamp: 2026-07-02 03:24:57 +08:00

Status: completed support sub-slice for generated-C manifest export table publication.

Completed items:
- `ZR_VM_AOT_ABI_VERSION` now covers `SZrAotManifestExportEntry` and `manifestExports/manifestExportCount` on both `ZrAotCompiledModule` and `SZrAotCodeRegistration`.
- Generated AOT C emits `zr_aot_manifest_exports[]` and wires it into the module descriptor and code registration.
- Manifest export table entries preserve type bindings, remap retained method/field member tokens through the compacted member-token sidecar, and reject invalid export kind/target/token shapes.
- Runtime descriptor validation rejects descriptor/codeRegistration manifest export table mismatches and invalid entry flags or token shapes.

RED/GREEN:
- RED: the focused remap test expected a persistent manifest export table entry for a retained source `MEMBER_DEF` token remapped from RID 7 to compacted RID 2, but no table existed.
- GREEN: the builder publishes type/method/field export entries, method/field entries carry compacted `MEMBER_DEF` tokens, generated C exposes `manifest.exportTableEntries`, and runtime validation checks the new ABI table.

Verification:
- WSL GCC: built `zr_vm_aot_c_zrp_metadata_export_token_remap_test`, `zr_vm_cli_aot_writer_options_test`, and `zr_vm_aot_c_source_contracts_test`; direct remap 9/0, writer options 18/0, source contracts 24/0; focused CTest `aot_c_zrp_metadata_export_token_remap` and `cli_aot_writer_options` 2/2.
- WSL clang: same targets built; focused CTest 2/2 and source contracts 24/0.
- Windows MSVC Debug: same targets built; focused CTest 2/2 and source contracts 24/0.

Remaining:
- Cross-module provider loading/version binding.
- Standalone provider manifest consumption beyond the generated-C descriptor table.
- Complete zrp metadata sweep/pruning, full trim analyzer, and broader ABI drift/deopt closure.
