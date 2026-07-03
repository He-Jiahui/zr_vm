# AOT 11-S7ZSH / 12-S7ZZY CLI .zrp Sidecar Path Bridge

Date: 2026-07-03 03:45:20 +08:00

Status: complete for this sub-slice. The broader AOT 07-12 goal remains active.

## Scope

- Added project helpers to derive the compacted `.zrp` metadata sidecar path from the generated AOT C path.
- Wired CLI AOT generation to set `SZrAotWriterOptions.compactedZrpMetadataOutputPath` only for publishable embedded
  `.zrp` metadata blobs.
- Required both `.zrp` header parsing and definition-table validation before publishing a sidecar, so ordinary `.zro`
  blobs and malformed metadata blobs do not create sidecars.
- Removed derived `.zrp` sidecars when optional AOT C output is disabled or a removed module's AOT C artifact is cleaned.

## RED

- After adding `zr_vm_cli_aot_compacted_metadata_sidecar_test`, WSL GCC failed to link because
  `ZrCli_Project_ResolveAotCompactedMetadataPathFromAotCPath` did not exist.
- After the first implementation, the invalid definition-table fixture failed with `Expected FALSE Was TRUE`, showing
  that a header-only metadata check could publish an invalid sidecar.

## GREEN

- WSL GCC focused CTest:
  `cli_aot_compacted_metadata_sidecar|cli_aot_writer_options|aot_c_zrp_metadata_publication|aot_c_zrp_metadata_pruning|zrp_metadata_format`
  passed 5/5.
- WSL clang focused CTest: same set passed 5/5.
- Windows MSVC Debug focused CTest: same set passed 5/5.
- `git diff --check` passed for the touched code and documentation files.

## Adjacent Follow-Up

- During this slice, adjacent `cli_project_incremental` exposed an older broad full-AOT dynamic-bridge anti-needle. The
  run did not create a normal project `.zrp` sidecar, so it was not a sidecar path issue.
- The follow-up `2026-07-03-aot-11-s6h-12-s8e-cli-full-aot-metadata-drift-assertion.md` narrowed that stale assertion:
  full-AOT still forbids missing generic-instance deopt, while 11-S6H metadata-drift fallback remains valid.
