---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_diagnostic_projection.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
tests:
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_diagnostic_golden_parity_cases.h
  - tests/language_server/stdio_assignment_ownership_diagnostic_smoke.js
  - tests/acceptance/2026-08-27-plan03-task06-compiler-lsp-diagnostic-golden-parity.md
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-27-plan03-task06-assignment-compatibility-query-projection.md
doc_type: milestone-record
---

# Plan 03 Task 6.20: Compiler/LSP Diagnostic Golden Parity

## Scope

This submilestone closes the explicit Plan 03 Task 6 golden-parity test gap for
the assignment-compatibility producer completed in Task 6.19. One source
snapshot contains three independent ownership failures: an explicit typed
initializer, an ordinary assignment, and a return statement.

The test reads compiler diagnostics through
`ZrParser_SemanticQuery_Diagnostics` and matches each result to the diagnostic
already projected by the semantic analyzer. It does not hard-code a second LSP
message policy or compare two LSP-owned objects.

The fixed overlay contains two test paths. The large semantic-analyzer runner
only includes and invokes the case; all comparator and fixture logic lives in a
cohesive case header, so this acceptance contract does not add another
responsibility to the existing large runner.

## Contract

For every descriptor-2008 `ownership_mismatch`, the projected diagnostic must
equal its compiler query source for:

- stable code and severity;
- complete primary range, including source identity and offset/line/column;
- message, cause, and suggestion;
- related-information count, order, ranges, and messages;
- typed-fix count, order, title, edit range/text, and applicability;
- descriptor id and explicit no-fix reason.

The LSP-only `codeDescription.href` must equal the help URI registered for the
compiler descriptor. The fixture requires exactly three compiler query rows and
three projected LSP rows, preventing a duplicate analyzer-owned diagnostic from
silently satisfying the comparison.

## Discovery

The first GCC execution was already green. Task 6.19 had implemented the
projection correctly, but the plan's required same-snapshot, all-field golden
comparison did not exist. No production change was justified, and none was
made. This slice therefore records a characterization and acceptance contract,
not a new semantic rule or an LSP fallback.

Source receiver method-call compatibility remains outside this slice. Its RED
is in the Syntax L8-owned `type_inference_native.c` producer and must not be
compensated in LSP code.

## Verification

The final baseline is HEAD
`a902ee9c8416df386c738b4cba16e1dd2ba767de` plus the two test paths. GCC 11.4,
Clang 14.0.0, and MSVC 19.44.35228.0 (`VSCMD_VER=17.14.38`) each directly
passed the same eleven checks with real process exits:

- compiler semantic-query diagnostics: `56/56`;
- semantic-query diagnostic disposition: `11/11`;
- semantic facts: `15/15`;
- semantic query: `30/30`;
- type inference: `123/123`;
- compiler integration: `127/127`;
- LSP semantic-query diagnostics: 15 pass markers;
- semantic analyzer: 54 pass markers, including golden parity;
- LSP source contracts: 46 pass markers;
- union-pattern diagnostics: 12 pass markers;
- dedicated assignment-ownership stdio smoke: real exit zero.

Each toolchain reported `RUN=11` and `EXIT_FAILURES=0`. Compiler integration
retained the same nine embedded negative-fixture `Fail -` lines on all three
toolchains while the outer Unity result was `127 Tests 0 Failures`; no other
final log contained a `Fail -` marker.

SHA-256 auditing matched both test paths across the shared workspace and all
three snapshots. The runner hash was
`c13f29f3ca99410ce3e8575d7dae4e4b14989ae760cf0c574786f6187c6d3bf2`;
the case-header hash was
`7cf3ae3193e0bd895ac8aabe3d60c439888efb3eab25e0bcf3844924de77530f`.

## 状态与产出记录

- 完成时间：2026-08-27 11:05 +08:00。
- 状态：已完成 compiler semantic query 与 LSP diagnostic 的同 snapshot
  全字段 golden parity 验收；Plan 03 Task 6 继续进行。
- 完成项目：initializer/assignment/return 三路 descriptor-2008 精确计数、
  stable code/severity/range/message/cause/suggestion/related/fix/descriptor/
  no-fix 全字段一致性、registry help URI 投影、case-header 模块化、
  GCC/Clang/MSVC byte-exact 11 项证据。
- 后续项目：等待 Syntax L8 释放 receiver method-call producer 后迁移其
  compatibility diagnostic，并继续删除剩余 analyzer-owned semantic checks。
