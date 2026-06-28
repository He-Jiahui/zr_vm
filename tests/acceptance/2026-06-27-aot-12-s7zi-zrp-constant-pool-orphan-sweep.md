# AOT 12-S7ZI zrp Constant-Pool Orphan Sweep

## Scope
- Added emitted zrp metadata constant-pool orphan sweep after MethodDef metadata pruning.
- Affected layers: AOT C zrp metadata pruning header layout, direct pool-pruning tests, generated-C code-stripping size/delta tests, AOT plan/status docs, and module documentation.

## Baseline
- Before this slice, emitted zrp pruning compacted MethodDef-related tables, signature blob pool slices, and string-pool slices, but copied the original constant pool unchanged.
- Current 11-S1 zrp metadata rows expose no constant-pool offset/length fields, so no retained row can prove a live constant-pool slice.
- RED evidence: WSL gcc `zr_vm_aot_c_zrp_metadata_pool_pruning_test` failed 1/2 once the fixture required MethodDef pruning to drop a 5-byte orphan constant pool (`Expected 488 Was 493`).

## Test Inventory
- Direct unit/subsystem: `tests/parser/test_aot_c_zrp_metadata_pool_pruning.c`
- Existing direct pruning regression: `tests/parser/test_aot_c_zrp_metadata_pruning.c`
- Integration size/delta contract: `tests/parser/test_aot_c_code_stripping.c`
- Regression smokes: source contracts, frame setup, typed scalar, shared-library smoke

## Tooling Evidence
- WSL gcc:
  - pool pruning 2/0, direct zrp pruning 5/0, code stripping 5/0, source contracts 21/0, frame setup 1/0, typed scalar 1/0, shared-library smoke 8/0
  - CTest `aot_c_(code_stripping|zrp_metadata_pruning|zrp_metadata_pool_pruning|typed_scalar)$` 4/4
- WSL clang:
  - same direct set and CTest 4/4
  - Existing generated generic-conversion `-Wlogical-not-parentheses` warning remains.
- Windows MSVC Debug:
  - pool pruning 2/0, direct zrp pruning 5/0, code stripping 5/0, source contracts 21/0, frame setup 1/0
  - typed scalar 0 failures/1 ignored; shared-library smoke 0 failures/8 ignored
  - CTest focused set 4/4

## Results
- `backend_aot_c_zrp_metadata_prune.c` now sizes the pruned constantPool section from a retained byte count.
- The current MethodDef pruning path passes retained constant-pool bytes as 0, so pruned metadata emits constantPool byteLength/count/elementSize as 0.
- `tests/parser/test_aot_c_zrp_metadata_pool_pruning.c` now locks the direct orphan constant-pool sweep behavior.
- `tests/parser/test_aot_c_code_stripping.c` now expects after-trim constantPool bytes to fall from 5 to 0 and includes those bytes in pool/metadata removed deltas.

## Acceptance Decision
- Accepted for the current no-reference constant-pool model.
- Remaining risks: if future zrp rows add constant literal/default-value offsets, this orphan-only sweep must be replaced or extended with retained constant slice remap/compaction.
- Still open for 12-S7/11-S7: complete trim analyzer, annotation promotion/suppression, cross-module/export token handling, and dump/diff tooling.
