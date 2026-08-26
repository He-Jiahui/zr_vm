---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_unresolved_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c
tests:
  - tests/parser/test_semantic_query.c
  - tests/parser/test_semantic_query_diagnostics.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_unresolved_reference_diagnostic_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-27-plan03-task06-unresolved-reference-query-projection.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.17 Unresolved-Reference Query Projection

## Required Results

- Parser query materialization owns unresolved-reference diagnostic policy and
  registered descriptors `2015` and `2016`.
- Generic references and member references preserve canonical fact ranges,
  complete diagnostic fields, explicit no-fix disposition, and no fixes.
- Resolved same-token facts and legal external/native callable payloads do not
  produce false unresolved diagnostics.
- LSP projects parser query facts and does not reconstruct diagnostics from
  names, AST shapes, symbol tables, source text, or private type checks.
- Focused parser/query/LSP targets and the dedicated stdio smoke pass under GCC,
  Clang, and MSVC from one byte-exact code/test overlay.

## Evidence

The parser RED demonstrated zero diagnostics for two canonical unresolved
reference facts. Analyzer RED then demonstrated that a missing source member
had a canonical unresolved fact but no query-owned LSP diagnostic. Subsequent
regressions froze same-range resolved call shadowing and legal external
callable facts whose canonical type/signature payload makes them available even
though `isResolved` remains false.

The acceptance snapshot was fixed at HEAD `b57f0c3` plus 10 code/test paths.
GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 with
`VSCMD_VER=17.14.38` returned real exit zero for the same ten targets. Unity
totals include compiler diagnostics `53/53`, query disposition `11/11`,
semantic facts `14/14`, semantic query `30/30`, type inference `123/123`, and
compiler integration `127/127`; all four LSP focused suites also passed.

The dedicated stdio fixture freezes one descriptor-2016
`member_not_found` diagnostic at zero-based line 4 characters 17..29. It also
requires canonical message/cause/suggestion, registered help URI,
`requires_user_decision`, and an empty fixes payload. The separate
`cannot_infer_exact_type` diagnostic may remain only on the receiver range at
line 4 characters 11..16. All three toolchain servers passed this contract.

Plain identifier source publication is not accepted by this slice because its
producer paths remain under Syntax L8 ownership. Parser query coverage proves
generic named unresolved facts, while source-to-stdio acceptance proves member
binding. The complete stdio suite was not rerun.

## Acceptance Decision

Accepted for unresolved-reference query projection and member source
publication. Plan 03 Task 6 remains in progress, and plain identifier source
publication remains an explicit follow-up boundary.

## 状态与产出记录

- 完成时间：2026-08-27 06:34 +08:00。
- 状态：本子项已完成并通过 GCC/Clang/MSVC focused、compiler integration
  与独立 stdio 验收；Task 6 继续进行。
- 完成项目：registered unresolved/member descriptors、canonical fact
  materialization、exact token range、resolved shadowing、external callable
  false-positive gate、no-fix disposition、LSP source contract 与三工具链
  stdio transport 证据。
