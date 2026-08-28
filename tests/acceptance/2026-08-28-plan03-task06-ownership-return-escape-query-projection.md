---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape_statements.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_analysis.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/parser/test_compiler_return_ownership_diagnostics.c
  - tests/language_server/test_ownership_diagnostics.c
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-28-plan03-task06-ownership-return-escape-query-projection.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.32 Ownership Return Escape Query Projection

## Required Results

- Make parser/compiler reference provenance the only owner return-escape rule.
- Publish stable descriptor identity, exact primary and related ranges,
  ownership fact, and explicit no-fix disposition.
- Preserve legal caller-reference passthrough.
- Project exactly one LSP diagnostic from the persistent query fact.
- Delete the analyzer-local matcher, fact producer, and helper module.
- Reject source-level reintroduction of local ownership return matching.

## TDD Evidence

The fixed-parent parser RED was 1/3: descriptor-backed loan and borrow escapes
were absent. With the tightened LSP test held constant, the parent ownership
runner was 20/25. Its three migration failures were missing loan projection,
missing borrow projection, and a false positive on legal caller passthrough.
Two separate direct-Weak receiver failures were also present.

After the parser producer and LSP consumer migration, the parser runner is
3/3 and the LSP ownership runner is 23/25. Loan and borrow diagnostics each
have one exact projection, two exact related ranges, and the expected ownership
fact; legal caller passthrough has neither code. The unchanged two Weak failures
remain classified as parent RED.

## Final Evidence

GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 each report real exit zero for
the same five focused runners: 3 ownership-return, 13 reference-escape, 11
query-disposition, 64 compiler-diagnostic tests, and 54 LSP source-contract
markers. All three independently reproduce only the two parent Weak failures
in the 25-case ownership runner. The MSVC snapshot matches the eight present
overlay files by SHA-256 and omits both deleted local producer files.

## Acceptance Decision

Accepted as the canonical ownership return-escape diagnostic slice of Plan 03
Task 6. Full ownership-runner GREEN and full Plan 03 completion are not claimed.

## 状态与产出记录

- 完成时间：2026-08-28 16:58 +08:00。
- 状态：本子项已验收；Plan 03 Task 6 继续进行。
- 完成项目：parser RED `1/3`、parent LSP `20/25`、overlay parser `3/3`、
  overlay LSP `23/25`、三工具链 `3/13/11/64/54` 真实退出、精确诊断唯一性、
  两条 local producer 文件删除、MSVC `8/8` byte audit 与 deleted `0`。
