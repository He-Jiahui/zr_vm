# Plan 03 Task 7.30 Canonical Implementation Identity Acceptance

## 验收基线

- 基线 HEAD：`8bb46f7ebb3f0bee23d084ac229616c041375418`。
- code/test overlay:
  `zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_implementation_query.c`、
  `tests/language_server/test_lsp_semantic_query_parity.c`。
- RED: canonical interface relation existed, but detaching the analyzer symbol table made
  `GetImplementation` return no location; parity was 13 Pass/1 Fail, real exit 1.
- GREEN: the detached query consumes copied SymbolId and returns one exact implementation location;
  parity is 14/14.

## 验收结果

- GCC 11.4, Clang 14, and MSVC 19.44 semantic-query parity all pass 14/14 with real exit 0.
- All three LSP source-contract executables print `PASSED` and exit 0.
- Full interface remains 111 Pass/2 fixed markers and project remains 51 Pass/9 fixed markers on all
  three toolchains; marker names and counts are unchanged from Task 7.29.
- The implementation projector does not synthesize relation targets from names or declaration text.
  Producer mismatches remain visible as an explicit support-first boundary.
- `git diff --check` passes; GCC/Clang use fixed source snapshots and MSVC uses an isolated snapshot/build.

## 状态与产出记录

- 完成时间：2026-08-30 04:54 +08:00。
- 状态：通过本子里程碑验收。
- 完成项目：detached analyzer symbol-table RED/GREEN、canonical implementation relation projection、
  three-toolchain focused verification, and fixed-marker delta audit.
