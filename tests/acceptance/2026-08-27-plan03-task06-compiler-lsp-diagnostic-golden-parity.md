---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_diagnostic_projection.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
tests:
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_diagnostic_golden_parity_cases.h
  - tests/language_server/stdio_assignment_ownership_diagnostic_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-27-plan03-task06-compiler-lsp-diagnostic-golden-parity.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.20 Compiler/LSP Diagnostic Golden Parity

## Required Results

- Query compiler diagnostics and projected LSP diagnostics from the same
  semantic snapshot.
- Require exact parity for code, severity, primary range, text fields, related
  information, typed fixes, descriptor id, and no-fix reason.
- Derive `codeDescription.href` only from the registered compiler descriptor.
- Require exactly one projected row for each of the three compiler ownership
  mismatch rows.
- Keep production behavior unchanged when the existing projector already
  satisfies the contract.

## Evidence

The source fixture independently exercises explicit initializer, ordinary
assignment, and return ownership mismatch. The first GCC run passed the new
comparison, proving Task 6.19 already preserved the required fields. The test
was retained as the missing plan-level golden contract; no production fallback
or message rewrite was added.

The final baseline is HEAD
`a902ee9c8416df386c738b4cba16e1dd2ba767de` plus two byte-exact test paths.
GCC, Clang, and MSVC each passed the same eleven direct checks with
`RUN=11 / EXIT_FAILURES=0`. Shared Unity totals were compiler diagnostics
`56/56`, query disposition `11/11`, semantic facts `15/15`, semantic query
`30/30`, type inference `123/123`, and compiler integration `127/127`.

The LSP suites reported 15 semantic-query diagnostic passes, 54 analyzer
passes, 46 source-contract passes, and 12 union-pattern passes. The dedicated
stdio fixture also exited zero and retained its exact three descriptor-2008
diagnostics. All final logs were marker-audited: only the same nine embedded
compiler-integration negative fixtures appeared on each toolchain, with no
outer Unity failure and no unexpected `Fail -` marker.

SHA-256 parity matched the workspace, GCC snapshot, Clang snapshot, and MSVC
snapshot for both changed test paths.

## Acceptance Decision

Accepted as the Plan 03 Task 6 golden-parity contract for assignment,
initializer, and return compatibility. Task 6 remains in progress because
receiver method-call compatibility still requires the frozen Syntax L8
producer rather than an LSP-side reconstruction.

## 状态与产出记录

- 完成时间：2026-08-27 11:05 +08:00。
- 状态：本子项已完成并通过 GCC/Clang/MSVC 同基线 11 项验收；Task 6
  继续进行。
- 完成项目：compiler-query/LSP 全字段一致性、三诊断精确计数、registry
  help URI、无 duplicate semantic policy、模块化 case header、byte-exact
  hash 与真实退出证据。
