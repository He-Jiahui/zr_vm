# AOT 11-S7ZSZ / 12-S7ZZZS Project Export Duplicate Target Guard

## Scope
- Tightens `.zrp` project manifest export declaration parsing.
- Affected layers: library project manifest parser, manifest normalization tests, downstream manifest export generator/runtime regression coverage.

## Baseline
- Before this slice, `library_project_parse_export_declarations()` accepted multiple project `exports` entries with the same `kind + target`.
- The generated manifest export table and runtime view now reject duplicate export keys, so the project parser should fail earlier and keep ambiguous declarations out of writer options.
- Broader 07~12 work remains open: complete metadata sweep/pruning, full trim analyzer, annotation/promotion policy, and ABI drift/deopt closure.

## Test Inventory
- `tests/library/test_project_manifest_normalization.c`
  - Existing export declaration parse, invalid kind, and invalid target coverage.
  - New negative fixture with duplicate `method` declarations targeting `"Widget.run"`.
- Related regression suites:
  - generated manifest export table duplicate guard
  - metadata runtime manifest export view
  - AOT C source contracts

## Tooling Evidence
- WSL GCC RED:
  - `wsl.exe bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_project_manifest_normalization_test -j2 && ./build-wsl-gcc/bin/zr_vm_project_manifest_normalization_test"`
  - Failed: `test_project_manifest_normalization_rejects_duplicate_export_target: FAIL: Expected NULL`.
- WSL GCC GREEN:
  - Same focused project manifest command passed 29/0.
- WSL GCC downstream regression:
  - `wsl.exe bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_project_manifest_normalization_test zr_vm_aot_c_zrp_metadata_export_token_remap_test zr_vm_metadata_runtime_manifest_exports_test zr_vm_aot_c_source_contracts_test -j2 && ctest --test-dir build-wsl-gcc -R '^(project_manifest_normalization|aot_c_zrp_metadata_export_token_remap|metadata_runtime_manifest_exports)$' --output-on-failure && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test"`
  - The CTest regex ran downstream manifest export suites 2/2; the project manifest target is covered by the focused direct run.
- WSL clang GREEN and regression:
  - Same targets under `build-wsl-clang`, direct project manifest run, downstream manifest export CTest 2/2, then source contracts.
- Windows MSVC Debug GREEN and regression:
  - Visual Studio environment import, same targets under `build-msvc`, direct project manifest run, downstream manifest export CTest 2/2, then `zr_vm_aot_c_source_contracts_test.exe`.

## Results
- WSL GCC: focused project manifest 29/0 passed; downstream CTest 2/2 passed; source contracts 24/0 passed.
- WSL clang: project manifest 29/0 passed; downstream CTest 2/2 passed; source contracts 24/0 passed.
- Windows MSVC Debug: project manifest 29/0 passed; downstream CTest 2/2 passed; source contracts 24/0 passed.
- Fix made: project `exports` parsing now fails closed on duplicate `kind + target` entries before writer option injection.

## Acceptance Decision
- Accepted for 11-S7ZSZ / 12-S7ZZZS.
- Ambiguous duplicate project manifest export keys are rejected during project load.
- Remaining risks are outside this slice: complete row-level metadata sweep/pruning, full trim analyzer, annotation/promotion policy, provider binding edges, and broader ABI drift/deopt closure.
