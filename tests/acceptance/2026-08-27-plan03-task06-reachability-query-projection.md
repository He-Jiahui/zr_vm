---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_reachability.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_union_patterns.c
tests:
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_reachability_semantic_query.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_reachability_diagnostic_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.13 Reachability Query Projection

## Required Results

- Reachability producers append structured facts without directly adding LSP
  diagnostics.
- The parser query materializer supplies one canonical `unreachable_code`
  warning with descriptor `3001` and explicit no-fix disposition.
- Constant branches, false loops, short-circuit operands, post-exit statements,
  and exhaustive union defaults preserve cause and exact range.
- Legacy analyzer-owned reachability codes are absent.
- Focused fact/query/LSP and stdio regressions pass under GCC, Clang, and MSVC.

## Evidence

The RED source contract found all five forbidden direct diagnostic strings.
The constant-branch, short-circuit, and exhaustive-union tests also rejected
the mixed canonical/legacy result. The GREEN implementation removed only the
diagnostic side of each producer and retained the corresponding fact append.

The dedicated stdio fixture uses `true || false` and requires exactly one
`unreachable_code` warning on the `false` operand at zero-based line 1 columns
26..31. It also requires `unsafe_edit`, the registered help URI, and absence of
`short_circuit_unreachable`.

GCC 11.4, Clang 14, and MSVC 19.44 each produced real exit zero for query
diagnostics `8/8`, semantic facts `14/14`, LSP semantic query diagnostics,
semantic analyzer, local reachability query, source contracts, union-pattern
diagnostics, and the dedicated stdio reachability smoke.

The full stdio suite subsequently reached an unrelated native receiver hover
failure in the frozen L8 boundary. It is recorded as residual baseline evidence
and is not represented as a pass here.

## Acceptance Decision

Accepted for reachability diagnostic query projection. The semantic facts are
still produced during LSP analysis in this intermediate phase; moving all such
analysis into parser/compiler belongs to the later consumer migration task.
Plan 03 Task 6 remains in progress.

## 状态与产出记录

- 完成时间：2026-08-27 00:10 +08:00。
- 状态：本子项已完成并通过 GCC/Clang/MSVC focused 与独立 stdio 验收；
  Task 6 继续进行。
- 完成项目：reachability fact-only producers、canonical query diagnostic、
  精确 short-circuit range、legacy code 清理、source contract 和三工具链
  transport 证据。
