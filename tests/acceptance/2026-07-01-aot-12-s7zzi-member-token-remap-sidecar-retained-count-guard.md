# AOT 12-S7ZZI / 11-S7 Member Token Remap Retained Count Guard

## Scope

- Changed generated AOT C zrp metadata member-token remap construction and direct export-token remapping.
- Affected layers: AOT parser/codegen support helpers, generated metadata remap ABI publication guardrails, focused parser tests, source-contract tests, and AOT 11/12 plan records.

## Baseline

- Before this slice, `backend_aot_c_zrp_member_token_remap_build()` trusted the caller-provided `retainedMethodDefCount`.
- Before the follow-up direct-helper check, `backend_aot_c_zrp_remap_export_member_token()` trusted the same caller-provided count and could remap a FieldDef export token into the same gap.
- A stale count larger than the actual retained MethodDef rows could make FieldDef target tokens start after a nonexistent compacted MethodDef RID, publishing or returning compacted target RID gaps that runtime token-shape and duplicate validation cannot detect.
- Existing repository-level worktree state is dirty from ongoing AOT/LSP/build work; this acceptance record only covers the focused files for this slice.

## Test Inventory

- `tests/parser/test_aot_c_zrp_metadata_export_token_remap.c`
  - Added `test_aot_c_zrp_metadata_export_token_remap_rejects_retained_method_count_drift`.
  - The direct helper fixture keeps only source MethodDef RID2 reachable, adds one FieldDef, then calls export-token remap with `actualRetainedMethodDefCount + 1`.
  - Expected behavior: direct remap fails and leaves the original FieldDef export token unchanged.
  - Added `test_aot_c_zrp_metadata_export_token_sidecar_rejects_retained_method_count_drift`.
  - The sidecar fixture uses the same retained-count drift shape for ABI publication.
  - Expected behavior: build fails and leaves `memberTokenRemapEntries`, `ownedMemberTokenRemapEntries`, and `memberTokenRemapCount` empty.
- `tests/parser/test_aot_c_source_contracts.c`
  - Locks the direct remap consistency helper and the sidecar consistency guard through `actualRetainedMethodDefCount == retainedMethodDefCount` and `actualRetainedMethodDefCount != retainedMethodDefCount` source needles.

## Tooling Evidence

- RED, WSL GCC:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_zrp_metadata_export_token_remap_test >/tmp/zr_s7zzi_red_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_zrp_metadata_export_token_remap_test'`
  - Observed: 7 tests, 1 failure. New test failed with `Expected FALSE Was TRUE`.
- RED follow-up, WSL GCC:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_zrp_metadata_export_token_remap_test >/tmp/zr_s7zzj_red_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_zrp_metadata_export_token_remap_test'`
  - Observed: 8 tests, 1 failure. Direct export-token remap count-drift test failed with `Expected FALSE Was TRUE`.
- GREEN, WSL GCC:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_zrp_metadata_export_token_remap_test zr_vm_aot_c_source_contracts_test >/tmp/zr_s7zzi2_gcc_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_zrp_metadata_export_token_remap_test && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && ctest --test-dir build-wsl-gcc -R "aot_c_zrp_metadata_export_token_remap" --output-on-failure'`
  - Observed: export-token remap 8/0, source contracts 24/0, registered remap CTest 1/1.
- GREEN, WSL Clang:
  - Same target/test sequence under `build-wsl-clang`.
  - Observed: export-token remap 8/0, source contracts 24/0, registered remap CTest 1/1.
- GREEN, Windows MSVC Debug:
  - Imported Visual Studio environment, `VSCMD_VER=17.14.34`.
  - Built `zr_vm_aot_c_zrp_metadata_export_token_remap_test` and `zr_vm_aot_c_source_contracts_test` under `build-msvc --config Debug`.
  - Observed: export-token remap 8/0, source contracts 24/0, registered remap CTest 1/1.

## Results

- Production code now recomputes the actual retained MethodDef count from the supplied MethodDef rows and function table before direct export-token remapping or sidecar entry allocation/publication.
- If the recomputed count differs from the caller-provided count, direct remap returns false without rewriting the token, and sidecar build returns false while leaving remap publication empty.
- The null row-pointer checks now use `methodCount`/`fieldCount` before any row iteration, so count drift cannot mask missing input rows.

## Acceptance Decision

- Accepted for the focused 12-S7ZZI / 11-S7 support slice.
- Remaining open: cross-module provider binding, real export manifest/table rewrite/publication, complete metadata sweep/pruning, annotation policy, full trim analyzer, and broader runtime ABI drift/deopt closure.
