# AOT 12-S7ZZL / 11-S7 GenericParamConstraint TypeSpec Remap

## Scope

- Changed emitted zrp metadata pruning so retained `GenericParamConstraint.constraintTypeToken` values follow retained `TYPE_SPEC` RID compaction.
- Affected layers: AOT parser/codegen zrp metadata pruning orchestration, focused pruning tests, source-contract tests, and AOT 11/12 plan records.

## Baseline

- Before this slice, `backend_aot_c_zrp_copy_generic_param_constraints()` remapped direct `TYPE_DEF` constraint tokens but skipped `TYPE_SPEC` constraint tokens.
- If TypeSpec RID1 was pruned and TypeSpec RID2 was retained, a retained generic-parameter constraint that referenced source `TYPE_SPEC` RID2 still published RID2 after the TypeSpec table compacted to RID1.
- Existing repository-level worktree state is dirty from ongoing AOT/LSP/build work; this acceptance record only covers the focused files for this slice.

## Test Inventory

- `tests/parser/test_aot_c_zrp_metadata_pruning.c`
  - Added a fixture with a retained generic method parameter constraint whose `constraintTypeToken` points at source `TYPE_SPEC` RID2 while source RID1 is pruned.
  - Requires the pruned metadata to publish the constraint token as compacted `TYPE_SPEC` RID1.
- `tests/parser/test_aot_c_source_contracts.c`
  - Locks the `backend_aot_c_zrp_remap_type_spec_token(&row.constraintTypeToken, ...)` call inside the GenericParamConstraint copy path.

## Tooling Evidence

- RED, WSL GCC:
  - `wsl bash -lc "cmake --build build-wsl-gcc --target zr_vm_aot_c_zrp_metadata_pruning_test >/tmp/s7zzl_red_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_zrp_metadata_pruning_test"`
  - Observed failure: 8 tests, 1 failure. The new constraint fixture expected compacted `TYPE_SPEC` RID1 (`117440513`) but old code left source RID2 (`117440514`).
- GREEN, WSL GCC:
  - Direct focused runs passed: zrp metadata pruning 8/0, TypeSpec pruning 2/0, pool pruning 6/0, source contracts 24/0.
  - Focused metadata CTest selection `aot_c_zrp_metadata_(pruning|typespec_pruning|export_token_remap|pool_pruning)` passed 4/4.
- GREEN, WSL Clang:
  - Direct focused runs passed: zrp metadata pruning 8/0, TypeSpec pruning 2/0, pool pruning 6/0, source contracts 24/0.
  - Focused metadata CTest selection passed 4/4.
- GREEN, Windows MSVC Debug:
  - Built focused targets with `cmake --build build-msvc --config Debug`.
  - Direct executable runs passed: zrp metadata pruning 8/0, TypeSpec pruning 2/0, pool pruning 6/0, source contracts 24/0.
  - Focused metadata CTest selection passed 4/4.
- Hygiene:
  - `git diff --check` over the focused code files reported no whitespace errors; Git printed only CRLF normalization warnings.

## Results

- `backend_aot_c_zrp_metadata_prune.c` now threads TypeSpec rows/count into the GenericParamConstraint copy path.
- Retained GenericParamConstraint rows first keep their existing GenericParam owner/range remap and direct TypeDef remap, then remap `TYPE_SPEC` constraint tokens through the existing retained TypeSpec compaction helper.
- Retained constraints no longer publish source TypeSpec RID holes after TypeSpec row pruning.

## Acceptance Decision

- Accepted for the focused 12-S7ZZL / 11-S7 support slice.
- Remaining open: cross-module provider binding, real export manifest/table rewrite/publication, broader metadata sweep/pruning, annotation policy, full trim analyzer, and runtime ABI drift/deopt closure.
