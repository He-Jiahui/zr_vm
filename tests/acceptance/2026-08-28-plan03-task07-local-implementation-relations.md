---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_relations.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_implementation_query.c
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-28-plan03-task07-local-implementation-relations.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 7.6 Local Implementation Relations

## Required Results

- Source type/interface/member implementation edges originate in parser facts.
- Member matching uses canonical signature and receiver-effect contracts.
- The LSP consumer queries `ImplementationsOf` by one valid SymbolId.
- Missing or ambiguous identity/source fails closed.
- Same-name unrelated members, source text, and the reference tracker cannot
  produce implementation targets.

## TDD Evidence

The original implementation request returned the queried interface itself.
The RED fixture paired the real implementation with an unrelated same-name
method. Parser relations were initially empty because compiler prototype
contracts were not published in the analyzer lifecycle. GDB then exposed a
prototype-array relocation during publication; fixed-count shallow snapshots
removed that transient-address dependency.

## Final Evidence

GCC, Clang, and MSVC pass relations 19, parity five, source contracts 58,
reaching definition three, and tracker five with real exits. Full interface
retains exactly the same four pre-existing markers and 109 passes on every
toolchain; delta is zero and the runner is not GREEN. Workspace, WSL, and MSVC
bytes match `11/11`.

## Acceptance Decision

Accepted for source-local implementation navigation only. Cross-project,
binary/native external implementations, rename, call/type hierarchy, and the
remaining consumer migrations stay open.

## 状态与产出记录

- 完成时间：2026-08-28 21:28 +08:00。
- 状态：本子项已验收；Task 7 与 Plan 03 继续进行。
- 完成项目：canonical compiler relation publication、exact member identity、
  `ImplementationsOf` projection、snapshot source ownership、same-name
  exactness、三工具链 focused exits、interface marker audit、byte audit。
