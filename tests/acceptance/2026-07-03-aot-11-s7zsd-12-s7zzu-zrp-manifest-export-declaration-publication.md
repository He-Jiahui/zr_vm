# AOT 11-S7ZSD / 12-S7ZZU `.zrp` Manifest Export Declaration Row Publication

Date: 2026-07-03 00:43:21 +08:00

Status: completed bound method/field declaration row publication support slice. Type-export persistent row publication,
unbound declaration persistence, full metadata sweep/pruning, and full trim analysis remain open.

## Scope

- Writer-level manifest export declarations that are bound to current-module method/field `MEMBER_DEF` tokens now append
  persistent `.zrp` `manifestExports` rows during embedded metadata preparation.
- Existing persistent `manifestExports` rows are preserved and still pass through the pruning/rewrite path.
- New declaration-derived rows reuse or append target strings in the `.zrp` string pool and remap source member tokens
  through the retained member-token sidecar before writing compacted `MEMBER_DEF` tokens.
- Affected layers: AOT C embedded `.zrp` metadata preparation, manifest export section rebuild/publication, and parser
  focused metadata pruning tests.

## Baseline

- 11-S7ZSC / 12-S7ZZT had already rewritten existing persistent manifest export rows after pruning, but writer-side
  declarations were not published into the file-level `.zrp` section.
- RED after adding the declaration publication test: Windows MSVC Debug focused
  `zr_vm_aot_c_zrp_metadata_pruning_test.exe` failed with `Expected 767 Was 708`, showing the prepared blob still lacked
  appended declaration rows and appended target strings.
- Known repository baseline: WSL clang still reports the pre-existing `type_inference.c` discarded-const warning in this
  focused area; it is unrelated to this slice.

## Test Inventory

- `tests/parser/test_aot_c_zrp_metadata_pruning.c`
  - Added `test_aot_c_zrp_metadata_pruning_publishes_manifest_export_declarations_as_rows`.
  - Preserves the two existing persistent manifest export rows from the fixture.
  - Appends method declaration target `api.kept` and field declaration target `api.field`.
  - Verifies source `MEMBER_DEF(2)` and `MEMBER_DEF(3)` are published as compacted `MEMBER_DEF(1)` and
    `MEMBER_DEF(2)`.
  - Verifies the string pool grows by the two new NUL-terminated target strings and the manifest export section grows
    from two rows to four rows.
- Adjacent focused regressions:
  - `zr_vm_aot_c_zrp_metadata_export_token_remap_test` keeps generated-C table member-token remap and kind/token guards
    stable.
  - `zr_vm_aot_c_code_stripping_test` keeps generated metadata size/stat markers stable after the prepared blob changes.

## Tooling Evidence

- WSL GCC:
  - `wsl.exe -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_zrp_metadata_pruning_test zr_vm_aot_c_zrp_metadata_export_token_remap_test zr_vm_aot_c_code_stripping_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_zrp_metadata_pruning_test && ./build-wsl-gcc/bin/zr_vm_aot_c_zrp_metadata_export_token_remap_test && ./build-wsl-gcc/bin/zr_vm_aot_c_code_stripping_test"`
- WSL clang:
  - `wsl.exe -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_c_zrp_metadata_pruning_test zr_vm_aot_c_zrp_metadata_export_token_remap_test zr_vm_aot_c_code_stripping_test -j2 && ./build-wsl-clang/bin/zr_vm_aot_c_zrp_metadata_pruning_test && ./build-wsl-clang/bin/zr_vm_aot_c_zrp_metadata_export_token_remap_test && ./build-wsl-clang/bin/zr_vm_aot_c_code_stripping_test"`
- Windows MSVC Debug:
  - `cmake --build build-msvc --config Debug --target zr_vm_aot_c_zrp_metadata_pruning_test zr_vm_aot_c_zrp_metadata_export_token_remap_test zr_vm_aot_c_code_stripping_test --parallel 2`
  - `.\build-msvc\bin\Debug\zr_vm_aot_c_zrp_metadata_pruning_test.exe`
  - `.\build-msvc\bin\Debug\zr_vm_aot_c_zrp_metadata_export_token_remap_test.exe`
  - `.\build-msvc\bin\Debug\zr_vm_aot_c_code_stripping_test.exe`

## Results

- RED: new pruning test failed before implementation with `Expected 767 Was 708`.
- GREEN WSL GCC: pruning 12/0, export-token remap 10/0, code stripping 10/0.
- GREEN WSL clang: pruning 12/0, export-token remap 10/0, code stripping 10/0.
- GREEN Windows MSVC Debug: pruning 12/0, export-token remap 10/0, code stripping 10/0.
- Fix made in response: `backend_aot_c_zrp_publish_manifest_export_declarations()` now appends publishable bound
  method/field declarations after pruning/prepare, recalculates section layout, replaces the owned metadata blob, and
  validates the final definition tables.

## Acceptance Decision

Accepted for the 11-S7ZSD / 12-S7ZZU support slice: bound method/field manifest export declarations are now published
as persistent `.zrp` rows with compacted member tokens and target string-pool updates. This does not accept type export
row publication, unbound declaration persistence, complete metadata sweep/pruning, full trim analysis, or the full
07~12 goal.
