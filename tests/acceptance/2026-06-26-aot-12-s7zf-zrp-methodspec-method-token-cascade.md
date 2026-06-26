# AOT 12-S7ZF zrp MethodSpec Method-Token Cascade

## Scope
- Added emitted zrp metadata pruning support for MethodSpec rows when MethodDefs are trimmed by code stripping.
- Affected layers: AOT C metadata pruning/remap helpers, focused parser tests, source-contract tests, AOT plan/status docs.

## Baseline
- Before this slice, MethodSpec-present zrp metadata blobs were conservatively left unpruned.
- RED evidence: WSL gcc `zr_vm_aot_c_zrp_metadata_pruning_test` failed 1/5 because the MethodSpec fixture expected an owned pruned blob, but `ownedBlob` was null.

## Test Inventory
- Direct unit/subsystem: `tests/parser/test_aot_c_zrp_metadata_pruning.c`
- Source contracts: `tests/parser/test_aot_c_source_contracts.c`
- Regression smokes: code stripping, frame setup, typed scalar, shared-library smoke
- Boundary cases: MethodSpec pointing to retained MethodDef is remapped; MethodSpec pointing to removed MethodDef is dropped; signature blob pool is preserved byte-for-byte for this slice.

## Tooling Evidence
- WSL gcc:
  - `cmake --build build-wsl-gcc --target ...`
  - direct zrp pruning 5/0, code stripping 5/0, source contracts 21/0, frame setup 1/0, typed scalar 1/0, shared-library smoke 8/0
  - CTest `aot_c_(code_stripping|zrp_metadata_pruning|typed_scalar)$` 3/3
- WSL clang:
  - same direct set and CTest 3/3
  - Existing generated generic-conversion `-Wlogical-not-parentheses` warning remains.
- Windows MSVC Debug:
  - configured and built with Visual Studio environment
  - direct zrp pruning 5/0, code stripping 5/0, source contracts 21/0, frame setup 1/0
  - typed scalar 0 failures/1 ignored; shared-library smoke 0 failures/8 ignored
  - CTest focused set 3/3

## Results
- Implemented `backend_aot_c_zrp_remap_method_spec_row()` and retained MethodSpec counting in the remap module.
- The pruning module now computes retained MethodSpec count, emits compacted MethodSpec section layout, copies retained MethodSpec rows, drops rows tied to removed MethodDefs, and leaves signature blob pool offsets/hashes valid by preserving the pool.

## Acceptance Decision
- Accepted for MethodSpec method-token cascade.
- Remaining risks: MethodSpec signature-pool compaction/rewrite, broader pool compaction, annotation-driven metadata promotion/suppression, export token handling, and dump/diff tooling remain open.
