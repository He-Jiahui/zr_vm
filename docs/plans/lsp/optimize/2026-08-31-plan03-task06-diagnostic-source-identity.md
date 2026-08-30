---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
tests:
  - tests/language_server/test_lsp_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_semantic_query_diagnostic_replacement_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 6.37: Diagnostic Source Identity

## Scope

Task 6.36 made a parser-owned diagnostic authoritative over an analyzer-owned
duplicate with the same range and stable code. Its duplicate predicate still
treated a missing source on either side as equal. Equal offsets and code could
therefore replace a diagnostic from a source-scoped parser snapshot with an
unscoped or stale analyzer item.

This submilestone makes source identity part of the duplicate key. It changes
only duplicate replacement; diagnostic materialization, ordering, and protocol
projection remain unchanged.

## TDD And Root Cause

The first characterization used two different non-null source strings and was
already green because the existing string equality check distinguished them.
The corrected RED gives the canonical fact a source URI and the stale analyzer
item no source while retaining the same offsets and `numeric_overflow` code.
All previous 17 cases passed; only the new source-identity case failed because
the stale item was replaced.

`query_diagnostic_same_source` now accepts pointer identity first, rejects an
asymmetric null source, and compares string content only when both sources are
non-null. Two explicitly source-less diagnostics remain comparable. This is a
fail-closed identity check and does not inspect message text, descriptor text,
AST nodes, or tokens.

The GREEN case requires two diagnostics in stable order: the original unscoped
item remains at its slot and the canonical source-scoped projection is
appended with its exact source and message.

## Verification

The three code/test paths were byte-identical between the workspace and the
independent WSL source snapshot. GCC 11.4 and Clang 14.0.0 both returned real
exit zero for:

- LSP semantic-query diagnostics: `18/18`;
- parser semantic-query diagnostics: `11/11`;
- compiler semantic-query diagnostics: `64/64`;
- semantic-query parity and LSP source-contract targets.

The LSP interface parent returned exit one on each toolchain with exactly the
same eight pre-existing producer markers, delta zero. MSVC, the complete
16-target matrix, and the three stdio/CLI smoke suites were not run.

## 状态与产出记录

- 完成时间：2026-08-31 06:55 +08:00。
- 状态：Task 6.37 diagnostic source identity 子里程碑已完成；Plan 03
  Task 6 继续进行。
- 完成项目：source-aware duplicate key、asymmetric-null fail closed、
  cross-source preservation RED/GREEN、GCC/Clang `18/11/64` focused gate、
  parity/source-contract真实退出、interface fixed-marker delta 0、三路径byte audit。
- 后续项目：继续迁移剩余analyzer-owned semantic diagnostic producers；
  Syntax05持有的property/import/symbol paths释放前不跨边界编辑。
