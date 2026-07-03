# AOT 11-S7ZSI / 12-S7ZZZ CLI .zrp Sidecar Stale Cleanup

Date: 2026-07-03 04:18:41 +08:00

Status: complete for this sub-slice. The broader AOT 07-12 goal remains active.

## Scope

- Affected layers: CLI AOT project artifact orchestration, compacted `.zrp` metadata sidecar publication, and focused CLI tests.
- When the current embedded blob is not publishable `.zrp` metadata, the CLI now removes any stale derived sidecar next
  to the generated AOT C file.
- If that stale sidecar cannot be removed, the CLI AOT write path now fails closed and does not write a fresh `.c`
  artifact beside stale metadata.
- This builds on 11-S7ZSH / 12-S7ZZY path derivation and 11-S7ZSG / 12-S7ZZX writer-level sidecar publication.

## Baseline

- The previous sidecar path bridge avoided publishing invalid metadata, but it did not remove an old sidecar already at
  the derived path.
- That meant a later AOT C regeneration from invalid or non-metadata input could leave stale compacted metadata beside a
  fresh `.c` artifact.
- Cleanup failure was also ignored, so a blocked stale sidecar could coexist with a new generated `.c` artifact.

## Test Inventory

- `tests/cli/test_cli_aot_compacted_metadata_sidecar.c`
  - derives compacted metadata sidecar path from generated AOT C path
  - publishes a sidecar for a valid `.zrp` metadata input blob
  - does not publish a sidecar for an invalid definition-table blob
  - removes a pre-existing stale sidecar when the current blob is not publishable metadata
  - fails closed when a stale sidecar path cannot be removed
- Adjacent focused regression set:
  - `cli_aot_writer_options`
  - `aot_c_zrp_metadata_publication`
  - `aot_c_zrp_metadata_pruning`
  - `zrp_metadata_format`

## Tooling Evidence

- WSL GCC:
  `cmake --build build-wsl-gcc --target zr_vm_cli_aot_compacted_metadata_sidecar_test -j 8`
- WSL GCC:
  `ctest --test-dir build-wsl-gcc -R '^cli_aot_compacted_metadata_sidecar$' --output-on-failure`
- WSL GCC:
  `ctest --test-dir build-wsl-gcc -R '^(cli_aot_compacted_metadata_sidecar|cli_aot_writer_options|aot_c_zrp_metadata_publication|aot_c_zrp_metadata_pruning|zrp_metadata_format)$' --output-on-failure`
- WSL clang:
  `cmake --build build-wsl-clang --target zr_vm_cli_aot_compacted_metadata_sidecar_test -j 8`
- WSL clang:
  `ctest --test-dir build-wsl-clang -R '^(cli_aot_compacted_metadata_sidecar|cli_aot_writer_options|aot_c_zrp_metadata_publication|aot_c_zrp_metadata_pruning|zrp_metadata_format)$' --output-on-failure`
- Windows MSVC Debug, Visual Studio environment `VSCMD_VER=17.14.34`:
  `cmake --build build-msvc --config Debug --target zr_vm_cli_aot_compacted_metadata_sidecar_test --parallel 8`
- Windows MSVC Debug:
  `ctest --test-dir build-msvc -C Debug -R '^(cli_aot_compacted_metadata_sidecar|cli_aot_writer_options|aot_c_zrp_metadata_publication|aot_c_zrp_metadata_pruning|zrp_metadata_format)$' --output-on-failure`

## Results

- RED: the new stale invalid-metadata fixture failed on WSL GCC with `Expected FALSE Was TRUE`, proving the stale sidecar
  remained after AOT C generation.
- RED: the blocked-stale-sidecar fixture then failed on WSL GCC with `Expected FALSE Was TRUE`, proving cleanup failure
  was ignored.
- Fix: `ZrCli_Compiler_WriteAotCFileForModule()` now derives the sidecar path before metadata publishability gating and
  removes that path when the current embedded blob is not valid publishable `.zrp` metadata. Missing files are accepted;
  other removal failures block AOT C generation and remove any same-run generated C artifact.
- GREEN: WSL GCC, WSL clang, and Windows MSVC Debug focused CTest set passed 5/5.

## Acceptance Decision

- Accepted for this sub-slice: stale derived `.zrp` sidecars are removed when current CLI AOT input is not publishable
  compacted metadata, and stale-sidecar cleanup failure fails closed.
- Remaining work: full metadata sweep/pruning, full trim analyzer integration, attribute/annotation promotion policy,
  and broader ABI drift/deopt coverage remain open.
- Modularization note: `compiler_aot.c` is already above the large-file threshold. This change only extends the existing
  AOT C artifact orchestration branch; further sidecar policy should be extracted into a focused helper or module.
