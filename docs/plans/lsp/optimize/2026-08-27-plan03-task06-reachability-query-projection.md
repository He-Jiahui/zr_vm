---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_reachability.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_union_patterns.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_reachability.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_union_patterns.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_reachability_semantic_query.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_reachability_diagnostic_smoke.js
  - tests/acceptance/2026-08-27-plan03-task06-reachability-query-projection.md
doc_type: milestone-record
---

# Plan 03 Task 6.13: Reachability Query Projection

## Goal

Make reachability diagnostics originate only from structured
`SZrSemanticReachabilityFact` values and the parser semantic-query diagnostic
materializer. LSP analysis may append exact reachability facts during this
intermediate migration, but it must not also publish a second diagnostic code,
message, or severity.

## Contract

- Constant branches, constant-false loop bodies, deterministic short-circuit
  operands, statements after control-flow exits, and exhaustive union defaults
  append reachability facts only.
- `ZrParser_SemanticQuery_MaterializeDiagnostics` is the sole diagnostic policy
  owner. It emits descriptor `3001`, code `unreachable_code`, warning severity,
  cause-specific explanation/suggestion, and `unsafe_edit` no-fix reason.
- The primary range remains the unreachable source construct. A short-circuit
  fact uses the right operand range rather than the whole logical expression.
- Legacy analyzer-owned codes `unreachable_branch`,
  `unreachable_loop_body`, `short_circuit_unreachable`, and
  `unreachable_union_switch_default` are not published.
- LSP only converts the materialized query diagnostic to protocol data.

## Implementation

The reachability, typecheck, and union-pattern analyzers retain their existing
fact creation and cause identity but no longer call
`SemanticAnalyzer_AddDiagnostic` for reachability. This removes four legacy
codes plus the direct `unreachable_code` block warning. The existing parser
query materializer now supplies one consistent diagnostic for every fact.

The short-circuit fact previously carried the whole logical-expression range
while the direct warning used the skipped right operand. With the direct path
removed, fact publication now records `right->location`, preserving the precise
user-facing range without reconstructing it in the protocol layer.

Source-contract coverage requires all three fact producers to remain and
rejects the legacy diagnostic strings. Analyzer and local-query tests require
canonical `unreachable_code` and explicitly reject old codes.

## Verification

The test-only RED produced five source-contract failures for the five direct
diagnostic strings. The semantic-analyzer constant-branch/short-circuit tests
and exhaustive-union reachability test also exited nonzero while legacy codes
were present. After the production change, those focused tests passed.

On one fixed seven-path source overlay, GCC 11.4, Clang 14, and MSVC 19.44 each
directly passed:

- semantic query diagnostic disposition: `8/8`;
- semantic facts: `14/14`;
- LSP semantic query diagnostics;
- semantic analyzer regressions;
- local reachability semantic query regressions;
- LSP source contracts;
- union-pattern diagnostics;
- dedicated stdio reachability diagnostic smoke with exact code, range,
  severity, no-fix reason, help URI, and diagnostic cardinality.

The repository-wide `stdio_smoke.js` passed the new reachability assertion but
later stopped at the known native receiver hover assertion in the separately
frozen L8 path. That run is not counted as a full stdio pass for this record.

## 状态与产出记录

- 完成时间：2026-08-27 00:10 +08:00。
- 状态：已完成 reachability diagnostic query-only 投影并通过三工具链 focused
  与独立 stdio 验收；不声明完整 stdio 基线或 Plan 03 Task 6 完成。
- 完成项目：5 条 direct reachability diagnostic producer 删除、统一
  descriptor 3001/unreachable_code、cause-specific query materialization、
  short-circuit 右操作数精确 range、unsafe_edit no-fix、legacy code 排除、
  source contract 与独立 stdio transport 回归。
- 后续项目：继续迁移 const assignment、variance、interface contract 等剩余
  analyzer-owned semantic diagnostics；L8 ownership 释放后复验完整 stdio 并
  补齐 receiver method mismatch producer。
