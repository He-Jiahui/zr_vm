---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_type_hierarchy.c
  - zr_vm_language_server/stdio/stdio_hierarchy.c
tests:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_type_hierarchy_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-28-plan03-task07-local-type-hierarchy.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 7.7 Local Type Hierarchy

## Required Results

- Local type hierarchy originates in parser base/derived relation facts.
- Prepare and follow-up requests retain exact SymbolId, TypeId, and version.
- Display names, source inheritance headers, and document-symbol scans cannot
  select hierarchy targets.
- Missing, malformed, cancelled, or stale identity fails closed.
- The stdio boundary round-trips the canonical identity without truncation.

## TDD Evidence

The original implementation parsed inheritance names from source text and
looked up matching document symbols. The RED fixture tampered returned display
names and included unrelated same-name types; it also reused an item after a
document version update. Only stable semantic identity and parser relations can
satisfy those cases.

## Final Evidence

GCC, Clang, and MSVC pass relations 19, parity six, source contracts 59,
advanced editor features 73, and the stdio type-hierarchy smoke with real
exits. Full interface retains exactly 109 passes and the same four pre-existing
markers on all three toolchains; delta is zero and it is not GREEN. Workspace,
WSL, and MSVC bytes match `10/10`.

## Acceptance Decision

Accepted for source-local type hierarchy only. Call hierarchy and external
cross-project/binary/native hierarchy remain open.

## 状态与产出记录

- 完成时间：2026-08-28 22:15 +08:00。
- 状态：本子项已验收；Task 7 与 Plan 03 继续进行。
- 完成项目：relation-only hierarchy projection、stable protocol identity、
  stale/same-name exactness、legacy scanner deletion、三工具链 focused/stdio
  exits、interface marker audit、byte audit。
