# AOT 11-S7ZSE / 12-S7ZZV `.zrp` Manifest Export Type Declaration Row Publication

Date: 2026-07-03 01:33:09 +08:00

Status: completed bound type declaration row publication support slice. Unbound declaration persistence, full metadata
sweep/pruning, full trim analysis, and the full 07~12 goal remain open.

## Scope

- Writer-level manifest export declarations that are bound to current-module `TYPE_DEF` tokens now append persistent
  `.zrp` `manifestExports` rows during embedded metadata preparation.
- Type declarations are remapped from source `TYPE_DEF` tokens to compacted `TYPE_DEF` tokens after TypeDef pruning.
- Existing persistent `manifestExports` rows and method/field declaration publication continue to use the same
  prepare/prune publication path.
- Affected layers: AOT C embedded `.zrp` metadata pruning, TypeDef token remap sidecar state, manifest export section
  publication, generated-C manifest export table building, and parser focused metadata pruning tests.

## Baseline

- 11-S7ZSD / 12-S7ZZU had already published bound method/field declarations as persistent rows, but skipped bound type
  declarations because there was no post-prune source `TYPE_DEF` to compacted `TYPE_DEF` remap sidecar.
- RED after adding the type declaration publication test: WSL GCC focused `zr_vm_aot_c_zrp_metadata_pruning_test`
  failed at the new assertion with `Expected 559 Was 526`, showing the prepared blob still lacked the appended type
  declaration row and `api.LiveType` target string.

## Test Inventory

- `tests/parser/test_aot_c_zrp_metadata_pruning.c`
  - Added `build_type_def_manifest_export_declaration_fixture`.
  - Added `test_aot_c_zrp_metadata_pruning_publishes_type_manifest_export_declarations_as_rows`.
  - Fixture prunes source `TYPE_DEF(1)` as an orphan and keeps source `TYPE_DEF(2)` as compacted `TYPE_DEF(1)`.
  - Test publishes `api.LiveType` as a type manifest export declaration and verifies the persistent row carries
    `ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_TYPE`, `HAS_TYPE_TOKEN`, compacted `TYPE_DEF(1)`, and no member token.
- Adjacent focused regressions:
  - `zr_vm_aot_c_zrp_metadata_export_token_remap_test` keeps generated-C manifest export table remapping stable for
    compacted member/type token publication.
  - `zr_vm_aot_c_zrp_metadata_typespec_pruning_test` and `zr_vm_aot_c_zrp_metadata_pool_pruning_test` keep neighboring
    metadata pruning support stable.
  - `zr_vm_aot_c_code_stripping_test` and `zr_vm_aot_c_guardrail_contracts_test` keep generated metadata statistics and
    guardrail contracts stable.

## Tooling Evidence

- WSL GCC:
  - `cmake --build build-wsl-gcc --target zr_vm_aot_c_zrp_metadata_pruning_test zr_vm_aot_c_zrp_metadata_export_token_remap_test zr_vm_aot_c_zrp_metadata_typespec_pruning_test zr_vm_aot_c_zrp_metadata_pool_pruning_test zr_vm_aot_c_zrp_metadata_size_deltas_test zr_vm_aot_c_code_stripping_test zr_vm_aot_c_guardrail_contracts_test -j2`
  - `ctest --test-dir /mnt/e/Git/zr_vm/build-wsl-gcc -R 'aot_c_zrp_metadata|aot_c_code_stripping|aot_c_guardrail_contracts' --output-on-failure`
- WSL clang:
  - `cmake --build build-wsl-clang --target zr_vm_aot_c_zrp_metadata_pruning_test zr_vm_aot_c_zrp_metadata_export_token_remap_test zr_vm_aot_c_zrp_metadata_typespec_pruning_test zr_vm_aot_c_zrp_metadata_pool_pruning_test zr_vm_aot_c_zrp_metadata_size_deltas_test zr_vm_aot_c_code_stripping_test zr_vm_aot_c_guardrail_contracts_test -j2`
  - `ctest --test-dir /mnt/e/Git/zr_vm/build-wsl-clang -R 'aot_c_zrp_metadata|aot_c_code_stripping|aot_c_guardrail_contracts' --output-on-failure`
- Windows MSVC Debug:
  - `cmake --build build-msvc --config Debug --target zr_vm_aot_c_zrp_metadata_pruning_test zr_vm_aot_c_zrp_metadata_export_token_remap_test zr_vm_aot_c_zrp_metadata_typespec_pruning_test zr_vm_aot_c_zrp_metadata_pool_pruning_test zr_vm_aot_c_zrp_metadata_size_deltas_test zr_vm_aot_c_code_stripping_test zr_vm_aot_c_guardrail_contracts_test --parallel 2`
  - `ctest --test-dir E:\Git\zr_vm\build-msvc -R 'aot_c_zrp_metadata|aot_c_code_stripping|aot_c_guardrail_contracts' --output-on-failure -C Debug`

## Results

- RED: new pruning test failed before implementation with `Expected 559 Was 526`.
- GREEN WSL GCC: focused CTest group passed 7/7.
- GREEN WSL clang: focused CTest group passed 7/7.
- GREEN Windows MSVC Debug: focused CTest group passed 7/7.
- Fix made in response: TypeDef pruning now builds a source-to-compacted TypeDef token sidecar, and manifest export
  declaration publication remaps bound type declarations through that sidecar before appending persistent `.zrp`
  `TYPE` rows.

## Acceptance Decision

Accepted for the 11-S7ZSE / 12-S7ZZV support slice: bound type manifest export declarations are now published as
persistent `.zrp` rows with compacted `TYPE_DEF` tokens and target string-pool updates. This does not accept unbound
declaration persistence, complete metadata sweep/pruning, full trim analysis, or the full 07~12 goal.
