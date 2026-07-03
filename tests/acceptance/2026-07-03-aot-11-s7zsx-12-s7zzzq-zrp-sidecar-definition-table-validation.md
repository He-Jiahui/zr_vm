# AOT 11-S7ZSX / 12-S7ZZZQ ZRP Sidecar Definition-Table Validation

## Scope
- Tightens writer-level compacted `.zrp` metadata sidecar publication.
- Affected layers: AOT C writer, `.zrp` metadata publication helper, parser tests, CLI sidecar regression coverage.

## Baseline
- Before this slice, `backend_aot_c_publish_compacted_zrp_metadata()` accepted any blob with a readable `.zrp` header.
- A direct writer call with `compactedZrpMetadataOutputPath` could leave or publish a sidecar whose definition tables failed `ZrCore_ZrpMetadata_ValidateDefinitionTables()`.
- Broader 07~12 work remains open: complete metadata sweep/pruning, full trim analyzer, annotation/promotion policy, and ABI drift/deopt closure.

## Test Inventory
- `tests/parser/test_aot_c_zrp_metadata_publication.c`
  - Existing positive sidecar publication for compacted MethodDef metadata.
  - New negative direct-writer fixture with readable header, invalid MethodDef definition tables, and a stale sidecar at the requested output path.
- `tests/cli/test_cli_aot_compacted_metadata_sidecar.c`
  - CLI/project path derivation and invalid/stale sidecar behavior remains covered.
- Related regression suites:
  - zrp metadata pruning
  - MethodSpec pruning
  - string/pool pruning
  - AOT C source contracts

## Tooling Evidence
- WSL GCC RED:
  - `wsl.exe bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_zrp_metadata_publication_test -j2 && ctest --test-dir build-wsl-gcc -R '^aot_c_zrp_metadata_publication$' --output-on-failure"`
  - Failed: `test_aot_c_writer_rejects_invalid_compacted_zrp_metadata_sidecar: FAIL: Expected FALSE Was TRUE`.
- WSL GCC GREEN and regression:
  - `wsl.exe bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_zrp_metadata_publication_test zr_vm_cli_aot_compacted_metadata_sidecar_test zr_vm_aot_c_zrp_metadata_pruning_test zr_vm_aot_c_zrp_metadata_methodspec_pruning_test zr_vm_aot_c_zrp_metadata_pool_pruning_test zr_vm_aot_c_source_contracts_test -j2 && ctest --test-dir build-wsl-gcc -R '^(aot_c_zrp_metadata_publication|cli_aot_compacted_metadata_sidecar|aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_methodspec_pruning|aot_c_zrp_metadata_pool_pruning)$' --output-on-failure && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test"`
- WSL clang GREEN and regression:
  - Same target/test set under `build-wsl-clang`.
- Windows MSVC Debug GREEN and regression:
  - Visual Studio environment import, same six targets under `build-msvc`, matching CTest regex, then `zr_vm_aot_c_source_contracts_test.exe`.

## Results
- WSL GCC: focused CTest 5/5 passed; source contracts 24/0 passed.
- WSL clang: focused CTest 5/5 passed; source contracts 24/0 passed.
- Windows MSVC Debug: focused CTest 5/5 passed; source contracts 24/0 passed.
- Fix made: publication helper now validates definition tables before writing and removes stale sidecar output on validation failure.

## Acceptance Decision
- Accepted for 11-S7ZSX / 12-S7ZZZQ.
- The direct writer no longer publishes or leaves stale compacted `.zrp` sidecars when the final blob has invalid definition tables.
- Remaining risks are outside this slice: complete row-level metadata sweep/pruning, full trim analyzer, annotation/promotion policy, provider binding edges, and broader ABI drift/deopt closure.
