# AOT 12-S7ZH zrp String-Pool Compaction

## Scope
- Added emitted zrp metadata string-pool compaction after MethodDef metadata pruning.
- Affected layers: AOT C zrp metadata pruning orchestration, shared section helper split, new string-pool remap helper module, focused parser tests, source-contract tests, Windows direct-test build wiring, AOT plan/status docs.

## Baseline
- Before this slice, emitted zrp pruning could compact MethodDef-related definition tables and the signature blob pool, but copied the original string pool unchanged.
- RED evidence: WSL gcc `zr_vm_aot_c_zrp_metadata_pool_pruning_test` failed 1/1 once the fixture required MethodDef pruning to remove unreferenced string-pool bytes and remap retained row string offsets.

## Test Inventory
- Direct unit/subsystem: `tests/parser/test_aot_c_zrp_metadata_pool_pruning.c`
- Existing direct pruning regression: `tests/parser/test_aot_c_zrp_metadata_pruning.c`
- Integration size/delta contract: `tests/parser/test_aot_c_code_stripping.c`
- Source contracts: `tests/parser/test_aot_c_source_contracts.c`
- Build wiring: `tests/CMakeLists.txt` for Windows shared-DLL direct pruning tests
- Regression smokes: code stripping, frame setup, typed scalar, shared-library smoke

## Tooling Evidence
- WSL gcc:
  - pool pruning 1/0, direct zrp pruning 5/0, code stripping 5/0, source contracts 21/0, frame setup 1/0, typed scalar 1/0, shared-library smoke 8/0
  - CTest `aot_c_(code_stripping|zrp_metadata_pruning|zrp_metadata_pool_pruning|typed_scalar)$` 4/4
- WSL clang:
  - same direct set and CTest 4/4
  - Existing generated generic-conversion `-Wlogical-not-parentheses` warning remains.
- Windows MSVC Debug:
  - configured and built with Visual Studio environment
  - pool pruning 1/0, direct zrp pruning 5/0, code stripping 5/0, source contracts 21/0, frame setup 1/0
  - typed scalar 0 failures/1 ignored; shared-library smoke 0 failures/8 ignored
  - CTest focused set 4/4

## Results
- Added `backend_aot_c_zrp_metadata_string_pool.{h,c}` for NUL-terminated string slice discovery, offset dedupe, compacted string-pool copy, and retained row string-offset remap.
- Added `backend_aot_c_zrp_metadata_sections.{h,c}` so shared section lookup/layout/copy helpers stay out of the pruning orchestration file.
- Retained TypeDef, retained MethodDef, FieldDef, retained GenericParam, and ModuleRef rows now remap their string offsets to the compacted string pool.
- The code-stripping MethodDef pruning integration fixture now expects after-trim string-pool bytes to shrink when the retained rows all reference the same empty string offset.

## Acceptance Decision
- Accepted for retained-row string-pool compaction/remap in the current emitted zrp metadata pruning path.
- Remaining risks: constant-pool sweep, complete trim analyzer, cross-module/export token handling, annotation-driven metadata promotion/suppression, and dump/diff tooling remain open.
