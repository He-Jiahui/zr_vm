# AOT 12-S7ZG zrp MethodSpec Signature-Pool Rewrite

## Scope
- Added emitted zrp metadata signature blob pool compaction and MethodSpec signature rewrite for retained metadata after MethodDef pruning.
- Affected layers: AOT C zrp metadata pruning orchestration, new signature remap helper module, focused parser tests, source-contract tests, Windows direct-test build wiring, AOT plan/status docs.

## Baseline
- Before this slice, retained MethodSpec rows could have their `methodToken` remapped, but the signature blob pool stayed byte-for-byte unchanged.
- RED evidence: WSL gcc `zr_vm_aot_c_zrp_metadata_pruning_test` failed 1/5 once the MethodSpec fixture required signature pool compaction from 30 to 15 bytes, MethodSpec signature payload token RID 2->1, and hash recomputation.

## Test Inventory
- Direct unit/subsystem: `tests/parser/test_aot_c_zrp_metadata_pruning.c`
- Integration size/delta contract: `tests/parser/test_aot_c_code_stripping.c`
- Source contracts: `tests/parser/test_aot_c_source_contracts.c`
- Build wiring: `tests/CMakeLists.txt` for Windows shared-DLL direct pruning test private module sources
- Regression smokes: code stripping, frame setup, typed scalar, shared-library smoke

## Tooling Evidence
- WSL gcc:
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
- Added `backend_aot_c_zrp_metadata_signature.{h,c}` for retained signature blob slice collection, exact slice dedupe, compacted signature pool copy, signature blob offset remap, MethodSpec signature payload rewrite, and stable hash recomputation.
- Retained MethodSpec signatures with `GENERIC_INST(MEMBER_REF methodToken, args...)` now rewrite the method-token payload through the compacted `MEMBER_DEF` token map before hashes are recalculated.
- Token records, TypeDefs, MethodDefs, FieldDefs, GenericParamConstraints, TypeSpecs, and MethodSpecs now receive remapped signature blob offsets when their referenced slices survive compaction.
- The code-stripping MethodDef pruning integration fixture now expects unreferenced signature blob pool bytes to be removed from after-trim zrp metadata size and pool delta markers.

## Acceptance Decision
- Accepted for retained signature blob pool compaction/rewrite and MethodSpec signature method-token rewrite.
- Remaining risks: complete trim analyzer, non-signature pool sweep/compaction, cross-module/export token handling, annotation-driven metadata promotion/suppression, and dump/diff tooling remain open.
