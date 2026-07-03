# AOT 12-S7ZZJ / 11-S7 Retained SIGNATURE Token RID Compaction

## Scope

- Changed emitted zrp metadata pruning so retained `SIGNATURE` token records and retained MethodSpec row tokens publish compacted local `SIGNATURE` RIDs.
- Affected layers: AOT parser/codegen zrp metadata pruning, signature metadata helpers, focused pruning tests, source-contract tests, and AOT 11/12 plan records.

## Baseline

- Before this slice, signature blob bytes were compacted, but the local `SIGNATURE` metadata token namespace stayed sparse.
- Retained FieldDef signature records, MethodSpec token records, MethodSpec rows, AssemblyRef signature records, and TypeRef signature records could keep source RIDs such as `SIGNATURE` RID9/RID11/RID22/RID23 after pruning removed earlier signature records.
- That left the pruned local metadata with compacted table counts but non-compacted signature token identities, which was inconsistent with the existing MethodDef, FieldDef, TypeDef, TypeSpec, and ModuleRef pruning model.
- Existing repository-level worktree state is dirty from ongoing AOT/LSP/build work; this acceptance record only covers the focused files for this slice.

## Test Inventory

- `tests/parser/test_aot_c_zrp_metadata_pruning.c`
  - Updated the FieldDef signature fixture to expect retained signature token record RID9 to publish as compacted `SIGNATURE` RID1.
  - Updated the MethodSpec fixture to expect both the retained MethodSpec token record and retained MethodSpec row token RID11 to publish as compacted `SIGNATURE` RID1.
- `tests/parser/test_aot_c_zrp_metadata_typedef_pruning.c`
  - Updated the retained FieldDef signature token record expectation from source `SIGNATURE` RID7 to compacted RID1.
- `tests/parser/test_aot_c_zrp_metadata_pool_pruning.c`
  - Updated retained AssemblyRef and TypeRef signature token references from source RIDs 22/23 to compacted local `SIGNATURE` RIDs 1/2.
- `tests/parser/test_aot_c_source_contracts.c`
  - Locks the retained signature token remap API, retained-token-record predicate, `ZR_METADATA_TABLE_SIGNATURE`, and compacted `ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, ...)` emission path.

## Tooling Evidence

- RED, WSL GCC:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_aot_c_zrp_metadata_pruning_test'`
  - Observed: 6 tests, 2 failures. FieldDef signature token expected `0x08000001` but old code returned `0x08000009`; MethodSpec signature token expected `0x08000001` but old code returned `0x0800000b`.
- GREEN, WSL GCC:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_zrp_metadata_pruning_test zr_vm_aot_c_zrp_metadata_typedef_pruning_test zr_vm_aot_c_zrp_metadata_pool_pruning_test -j 2 && ./build-wsl-gcc/bin/zr_vm_aot_c_zrp_metadata_pruning_test && ./build-wsl-gcc/bin/zr_vm_aot_c_zrp_metadata_typedef_pruning_test && ./build-wsl-gcc/bin/zr_vm_aot_c_zrp_metadata_pool_pruning_test'`
  - Observed: zrp metadata pruning 6/0, typedef pruning 2/0, pool pruning 6/0.
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test -j 2 && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test'`
  - Observed: source contracts 24/0.
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-gcc -R "aot_c_zrp_metadata_(pruning|typespec_pruning|export_token_remap|pool_pruning)" --output-on-failure'`
  - Observed: 4/4 focused metadata CTest passed.
- GREEN, WSL Clang:
  - Same focused target build/direct sequence under `build-wsl-clang`.
  - Observed: zrp metadata pruning 6/0, typedef pruning 2/0, pool pruning 6/0, source contracts 24/0.
  - Focused metadata CTest selection passed 4/4.
- GREEN, Windows MSVC Debug:
  - `cmake --build build-msvc --config Debug --target zr_vm_aot_c_zrp_metadata_pruning_test zr_vm_aot_c_zrp_metadata_typedef_pruning_test zr_vm_aot_c_zrp_metadata_pool_pruning_test zr_vm_aot_c_source_contracts_test -- /m:2`
  - Direct executable runs passed: zrp metadata pruning 6/0, typedef pruning 2/0, pool pruning 6/0, source contracts 24/0.

## Results

- `backend_aot_c_zrp_metadata_signature.{h,c}` now exposes retained signature token remap helpers that compact local `SIGNATURE` tokens by retained signature-token-record order after the existing member, TypeDef, TypeSpec, and ModuleRef pruning filters.
- Token-record copy now rewrites retained signature tokens in `token`, `relatedToken`, `ownerToken`, `targetMetadataToken`, and `targetSignatureToken`.
- MethodSpec copy now rewrites the MethodSpec row `token` through the same retained signature token remap before copying the row and recomputing its instantiation hash.
- Signature blob offset compaction and hash recomputation behavior remains unchanged; this slice closes the token-identity side of the same retained signature metadata surface.

## Acceptance Decision

- Accepted for the focused 12-S7ZZJ / 11-S7 support slice.
- Remaining open: cross-module provider binding, real export manifest/table rewrite/publication, broader metadata sweep/pruning, annotation policy, full trim analyzer, and runtime ABI drift/deopt closure.
