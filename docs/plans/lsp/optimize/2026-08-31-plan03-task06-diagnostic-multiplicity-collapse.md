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

# Plan 03 Task 6.38: Diagnostic Multiplicity Collapse

## Scope

Task 6.36 replaced one stale analyzer diagnostic with its canonical query row,
and Task 6.37 made source identity part of that match. If a legacy analyzer had
already produced two rows with the same source/range/code, only the first was
replaced and the second stale object still reached protocol output.

This submilestone enforces one result per canonical diagnostic identity without
coalescing diagnostics whose source, range, or stable code differs.

## TDD And Implementation

The RED fixture publishes one complete `numeric_overflow` fact and preloads two
analyzer diagnostics at the same source/range/code with different stale
severities and messages. All prior 18 cases passed; only the new case failed
because the array retained two rows after projection.

After materializing the complete canonical object, the bridge replaces the
earliest matching slot. It then scans from the end of the array toward that
slot, frees each later matching object, compacts pointer storage with
`memmove`, and decrements the length. Reverse removal keeps the retained slot
stable and preserves the order of unrelated diagnostics. If canonical
materialization fails, no stale row is removed and no partial projection is
published.

The GREEN fixture requires one row, the original first-slot order, canonical
error severity/message, and a pointer distinct from both stale objects.

## Verification

Workspace and independent WSL SHA-256 values matched for all three code/test
paths. GCC 11.4 and Clang 14.0.0 both returned real exit zero for:

- LSP semantic-query diagnostics: `19/19`;
- parser semantic-query diagnostics: `11/11`;
- compiler semantic-query diagnostics: `64/64`;
- semantic-query parity and LSP source-contract targets.

The LSP interface parent returned exit one on both toolchains with exactly the
same eight pre-existing producer markers, delta zero. MSVC, the complete
16-target matrix, and the three stdio/CLI smoke suites were not run.

## 状态与产出记录

- 完成时间：2026-08-31 07:07 +08:00。
- 状态：Task 6.38 diagnostic multiplicity collapse 子里程碑已完成；
  Plan 03 Task 6 继续进行。
- 完成项目：first-slot canonical replacement、reverse duplicate removal、
  stale object释放、stable unrelated ordering、双stale RED/GREEN、GCC/Clang
  `19/11/64`门禁、parity/source-contract真实退出、interface marker delta 0、
  三路径byte audit。
- 后续项目：继续删除剩余analyzer-owned semantic diagnostic producers；
  Syntax05持有的property/import/symbol paths释放前不跨边界编辑。
