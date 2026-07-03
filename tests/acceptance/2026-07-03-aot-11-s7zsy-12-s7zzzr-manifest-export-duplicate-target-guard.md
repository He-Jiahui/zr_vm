# AOT 11-S7ZSY / 12-S7ZZZR Manifest Export Duplicate Target Guard

## Scope
- Tightens generated manifest export table construction for compacted AOT metadata.
- Affected layers: AOT C manifest export table builder, export-token remap tests, runtime manifest export regression coverage.

## Baseline
- Before this slice, `backend_aot_c_zrp_manifest_export_table_build()` accepted multiple declarations with the same `kind + target`.
- The runtime manifest export view already rejected duplicate matching entries as ambiguous, so generated metadata could publish a table that only failed when queried later.
- Broader 07~12 work remains open: complete metadata sweep/pruning, full trim analyzer, annotation/promotion policy, and ABI drift/deopt closure.

## Test Inventory
- `tests/parser/test_aot_c_zrp_metadata_export_token_remap.c`
  - Existing member-token remap and manifest export table kind/token guard coverage.
  - New negative fixture with two bound `METHOD` declarations targeting `"Factory.make"`.
- `tests/module/test_metadata_runtime_manifest_exports.c`
  - Existing runtime view coverage continues to reject ambiguous manifest export targets.
- Related regression suites:
  - zrp metadata pruning
  - AOT C source contracts

## Tooling Evidence
- WSL GCC RED:
  - `wsl.exe bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_zrp_metadata_export_token_remap_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_zrp_metadata_export_token_remap_test"`
  - Failed: `test_aot_c_zrp_metadata_manifest_export_table_rejects_duplicate_kind_target: FAIL: Expected FALSE Was TRUE`.
- WSL GCC GREEN and regression:
  - `wsl.exe bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_zrp_metadata_export_token_remap_test zr_vm_metadata_runtime_manifest_exports_test zr_vm_aot_c_zrp_metadata_pruning_test zr_vm_aot_c_source_contracts_test -j2 && ctest --test-dir build-wsl-gcc -R '^(aot_c_zrp_metadata_export_token_remap|metadata_runtime_manifest_exports|aot_c_zrp_metadata_pruning)$' --output-on-failure && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test"`
- WSL clang GREEN and regression:
  - Same target/test set under `build-wsl-clang`.
- Windows MSVC Debug GREEN and regression:
  - Visual Studio environment import, same four targets under `build-msvc`, matching CTest regex, then `zr_vm_aot_c_source_contracts_test.exe`.

## Results
- WSL GCC: focused CTest 3/3 passed; source contracts 24/0 passed.
- WSL clang: focused CTest 3/3 passed; source contracts 24/0 passed.
- Windows MSVC Debug: focused CTest 3/3 passed; source contracts 24/0 passed.
- Fix made: generated manifest export table construction now fails closed on duplicate `kind + target` entries and leaves no published table state.

## Acceptance Decision
- Accepted for 11-S7ZSY / 12-S7ZZZR.
- The generated manifest export table now rejects ambiguous duplicate export keys before compacted metadata/codeRegistration publication.
- Remaining risks are outside this slice: complete row-level metadata sweep/pruning, full trim analyzer, annotation/promotion policy, provider binding edges, and broader ABI drift/deopt closure.
