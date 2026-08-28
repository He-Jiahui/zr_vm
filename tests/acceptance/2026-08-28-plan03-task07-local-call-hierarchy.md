---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_call_hierarchy.c
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_type_hierarchy_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-28-plan03-task07-local-call-hierarchy.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 7.8 Local Call Hierarchy

## Required Results

- Source-local call hierarchy originates in parser call-edge facts.
- Prepare and follow-up requests retain exact SymbolId, callable TypeId, and
  version.
- Display names, source lines, and document-symbol scans cannot select callers
  or targets.
- Multiple calls to one target form one item with all exact call-site ranges.
- Missing, unresolved, malformed, cancelled, or stale identity fails closed.

## TDD Evidence

The original implementation searched each source line for a callable name and
selected callers from LSP document symbols. The RED fixture mutates display
names, includes unrelated declarations, calls one target twice, and submits a
stale item after an update. Parser coverage also registers duplicate SymbolIds
and TypeIds for one exact AST declaration. Only canonical call edges and stable
snapshot identity satisfy all cases.

## Final Evidence

GCC, Clang, and MSVC pass parser call queries 11, parity seven, source contracts
60, advanced editor features 73, and the combined type/call hierarchy stdio
smoke with real exits. Full interface retains exactly 109 passes and the same
four pre-existing markers on all three toolchains; delta is zero, inlay hints
pass, and the runner is not GREEN. Workspace, WSL, and MSVC code/test bytes
match `10/10`.

## Acceptance Decision

Accepted for source-local free-function call hierarchy only. Method/lambda and
external cross-project/binary/native call hierarchy remain open.

## 状态与产出记录

- 完成时间：2026-08-28 23:21 +08:00。
- 状态：本子项已验收；Task 7 与 Plan 03 继续进行。
- 完成项目：query-only call hierarchy projection、stable protocol identity、
  duplicate-record canonicalization、multi-range grouping、stale/name exactness、
  legacy scanner deletion、三工具链 focused/stdio exits、interface marker audit、
  byte audit。
