# AOT 11-S7ZSB / 12-S7ZZS `.zrp` Manifest Export Section

Date: 2026-07-02 10:15:47 +08:00

Status: completed format-layer support slice. Full manifest export row generation, pruning publication, and full trim
analysis remain open.

## Scope

- `.zrp` metadata is now version 4 with 13 sections and a 224-byte header.
- The new tail section is `manifestExports`, storing `SZrZrpMetadataManifestExportRow` rows with kind, flags, target
  string-pool offset, type token, and member token fields.
- Core metadata header read/write/validate, section lookup, definition-table payload writing, CLI dump/diff, AOT
  metadata size/count stats, and code-stripping marker deltas all recognize the section.

## RED

- WSL GCC focused `zr_vm_zrp_metadata_format_test` failed to compile because the format API had no manifest export
  section enum, row type, or header field.
- The adjacent WSL GCC `zr_vm_aot_c_code_stripping_test` failed 10/1 after the format change because generated
  registration and metadata size stats still expected the old 12-section shape.

## GREEN

- WSL GCC direct: `zr_vm_zrp_metadata_format_test` 13/0, `zr_vm_cli_zrp_metadata_dump_test` pass,
  `zr_vm_aot_c_zrp_metadata_export_token_remap_test` 10/0, `zr_vm_aot_c_zrp_metadata_size_deltas_test` 2/0,
  `zr_vm_aot_c_code_stripping_test` 10/0.
- WSL clang direct: format 13/0, CLI dump pass, size deltas 2/0, code stripping 10/0.
- Windows MSVC Debug direct: format 13/0, CLI dump pass, size deltas 2/0, code stripping 10/0.

## Notes

This acceptance record proves the persistent `.zrp` format carrier and its focused visibility paths only. It does not
claim that generated AOT output already writes manifest export rows into `.zrp`, or that pruning fully rewrites/publishes
that section.
