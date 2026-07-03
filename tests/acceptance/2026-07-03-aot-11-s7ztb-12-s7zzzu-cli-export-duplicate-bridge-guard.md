# AOT 11-S7ZTB / 12-S7ZZZU CLI Export Duplicate Bridge Guard

- Date: 2026-07-03 23:36:13 +08:00
- Status: completed for this focused slice; the broader AOT 07~12 goal remains in progress.
- Plans: `docs/plans/aot/11-metadata.md`, `docs/plans/aot/12-code-stripping.md`, `docs/plans/aot/index.md`

## Scope

This slice closes the CLI writer bridge duplicate export declaration gap. The project parser rejects duplicate
`exports` entries loaded from JSON, the schema rejects exact duplicate objects, and the generated manifest export table
rejects duplicate published rows. This change covers the remaining in-memory bridge path by rejecting duplicate
`kind + target` declarations before they become AOT writer manifest export options.

## Completed Items

- Added `test_cli_aot_writer_options_rejects_duplicate_manifest_export_declarations`.
- Added a duplicate declaration scan in `compiler_aot_exports.c` while copying project export declarations into
  `SZrAotWriterOptions`.
- On duplicate rejection, the bridge now frees CLI-owned export declaration scratch storage and leaves
  `manifestExportDeclarations` / `manifestExportDeclarationCount` empty.
- Updated AOT plan status records and module/parser documentation to reference this focused slice.

## RED / GREEN

- RED: WSL GCC focused run failed with
  `test_cli_aot_writer_options_rejects_duplicate_manifest_export_declarations:FAIL: Expected FALSE Was TRUE`.
- GREEN: WSL GCC focused run passed `zr_vm_cli_aot_writer_options_test` with 19 tests, 0 failures.

## Verification

- WSL GCC:
  - `zr_vm_cli_aot_writer_options_test`: 19 tests, 0 failures.
  - `zr_vm_project_manifest_normalization_test`: 29 tests, 0 failures.
  - CTest `aot_c_zrp_metadata_export_token_remap|metadata_runtime_manifest_exports`: 2/2 passed.
  - `zr_vm_aot_c_source_contracts_test`: 24 tests, 0 failures.
- WSL Clang:
  - `zr_vm_cli_aot_writer_options_test`: 19 tests, 0 failures.
  - `zr_vm_project_manifest_normalization_test`: 29 tests, 0 failures.
  - CTest `aot_c_zrp_metadata_export_token_remap|metadata_runtime_manifest_exports`: 2/2 passed.
  - `zr_vm_aot_c_source_contracts_test`: 24 tests, 0 failures.
- Windows MSVC Debug:
  - `zr_vm_cli_aot_writer_options_test`: 19 tests, 0 failures.
  - `zr_vm_project_manifest_normalization_test`: 29 tests, 0 failures.
  - CTest `aot_c_zrp_metadata_export_token_remap|metadata_runtime_manifest_exports`: 2/2 passed.
  - `zr_vm_aot_c_source_contracts_test`: 24 tests, 0 failures.
- Hygiene:
  - `git diff --check` on the touched code files exited 0 with only the existing LF/CRLF warning.
  - Tail-whitespace scan on the touched code files returned no matches.

## Remaining Work

This does not complete the full 11-S7, 12-S7, or AOT 07~12 plan. Complete metadata sweep/pruning, full trim analyzer,
annotation/promotion policy, provider binding remaining edges, and broader ABI drift/deopt closure remain open.
