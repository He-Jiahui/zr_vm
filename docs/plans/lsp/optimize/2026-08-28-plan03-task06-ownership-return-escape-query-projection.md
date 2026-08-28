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
  - tests/acceptance/2026-08-28-plan03-task06-ownership-return-escape-query-projection.md
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 6.32: Ownership Return Escape Query Projection

## Scope

This submilestone migrates owner-backed reference-return escape diagnostics
from the LSP semantic analyzer into the parser/compiler reference-escape pass.
It covers writable `Unique` returns, readonly `Shared` returns, legal caller
reference passthrough, exact related ranges, ownership facts, query
materialization, and LSP projection. Other ownership rules remain separate
support-first slices.

## TDD And Root Cause

On fixed HEAD `29a182e4447123ae92a497a17e8c74d740796b1c`, the new
parser runner was RED with three tests and two failures: writable and readonly
owner returns had no descriptor-backed compiler diagnostic. The legal caller
reference case already passed the lower-layer escape lattice.

The LSP analyzer had a second implementation that matched return AST shape,
rebuilt owner qualifiers and line-wide ranges, selected diagnostic builders,
and appended ownership facts locally. A tightened LSP runner on the fixed
parent passed 20 of 25 cases: both migrated return diagnostics were missing,
and the local matcher incorrectly rejected legal caller-reference passthrough.
Two direct-Weak receiver guard failures were also present and are unrelated to
this slice.

## Implementation

`ZrParser_Compiler_ValidateReferenceEscapes` exposes the existing compiler
pass without exposing its internal context. The return analysis uses only the
binding's declared ownership qualifier, return reference access, and canonical
provenance escape bound. It publishes descriptor 4003 `loan_escape` or 4002
`borrow_escape`, exact source and lifetime-end related ranges, explicit
user-decision no-fix disposition, and a matching ownership violation fact.

The semantic analyzer calls that pass for both module and function analysis
roots, consumes its structured compiler error, and later projects the same
persistent query fact. Existing code-and-range deduplication guarantees a
single diagnostic; focused tests assert that uniqueness. The analyzer-local
matcher, builder dispatcher, line-range reconstruction, fact publisher, and
ownership diagnostic module are deleted. No name, message, text, or AST-pair
fallback replaces them.

## Verification

The fixed overlay has eight modified/added files plus two deleted production
files. Workspace-to-MSVC SHA-256 matched `8/8`, and both deleted files were
absent. GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 each passed the same
five direct-exit runners:

- ownership return producer: 3/3;
- reference escape and suspension: 13/13;
- semantic query fix disposition: 11/11;
- compiler semantic query diagnostics: 64/64;
- LSP source contracts: 54 pass markers.

The tightened LSP ownership runner improved from parent 20/25 to overlay
23/25 on GCC. All three toolchains pass the migrated loan, borrow, legal
passthrough, and LSP related-information cases. Each retains only the same two
direct-Weak receiver failures already present in the fixed parent. Those
failures are explicitly not counted as GREEN and are not repaired through an
LSP compatibility path.

## 状态与产出记录

- 完成时间：2026-08-28 16:58 +08:00。
- 状态：本子里程碑已完成；Plan 03 Task 6 继续进行。
- 完成项目：parser/compiler canonical return-escape producer、public scoped
  validation entry、descriptor 4002/4003、精确 primary/related ranges、ownership
  fact、query materialization、LSP 单条投影、local ownership producer 删除、
  source contract、GCC parent `20/25` 到 overlay `23/25` 差分、三工具链
  `3/13/11/64/54` 门禁与 MSVC `8/8` byte audit。
- 后续项目：继续逐条迁移剩余 analyzer-owned semantic producers；direct-Weak
  receiver guard 必须在 canonical receiver fact/guard producer 层解决，不得在
  LSP 按成员名、类型文本或 diagnostic message 兼容。
