---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_unresolved_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_unresolved_diagnostics.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_messages.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_unresolved_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_unresolved_diagnostics.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_messages.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query.c
  - tests/parser/test_semantic_query_diagnostics.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_unresolved_reference_diagnostic_smoke.js
  - tests/acceptance/2026-08-27-plan03-task06-unresolved-reference-query-projection.md
doc_type: milestone-record
---

# Plan 03 Task 6.17: Unresolved-Reference Query Projection

## Goal

Materialize unresolved-reference diagnostics from canonical parser reference
facts so LSP consumers project registered diagnostics without member-name,
symbol-table, AST-shape, message, or source-text reconstruction.

## Contract

- A named unresolved read, write, call, or type fact with no canonical target
  payload produces descriptor `2015` / `unresolved_reference`.
- A named unresolved member read or write with no canonical target payload
  produces descriptor `2016` / `member_not_found`.
- The reference fact range is the exact diagnostic range. Both families use
  error severity, canonical message/cause/suggestion, and
  `requires_user_decision` with no machine fix.
- A resolved fact with the same source, range, and name suppresses a
  provisional unresolved fact, including member-access-to-call upgrades.
- External/native callable facts with a canonical symbol, type, signature, or
  contract role are not binding failures even when `isResolved` is false.
- LSP only consumes `ZrParser_SemanticQuery_Diagnostics` and must not own either
  diagnostic code or reconstruct unresolved-reference policy.

## Implementation

The parser query materialization loop now delegates unresolved reference facts
to a cohesive module. That module recognizes only reportable reference kinds,
requires a name, rejects facts with canonical target payload, applies resolved
fact shadowing, builds the registered diagnostic, and appends it to the query
diagnostic array. The generic LSP projector already deep-copies query results,
so no LSP production branch was added.

Descriptor IDs `2015` and `2016` are registered in the parser diagnostic
registry and message table. Parser tests freeze generic unresolved and missing
member facts, exact ranges, no-fix disposition, provisional fact shadowing, and
the external callable exclusion. LSP source coverage freezes a missing member
from source through canonical fact, analyzer query projection, and stdio JSON.

This slice does not add the plain identifier source-fact publisher because its
type-inference fact paths are owned by the active Syntax L8 milestone. The
query supports any named canonical unresolved fact; source end-to-end evidence
in this submilestone covers member binding only.

## Verification

TDD RED first showed that canonical unresolved facts produced no query
diagnostics. The first GREEN exposed provisional member facts shadowed later by
resolved calls, and the final audit exposed legal external callable facts whose
`isResolved` flag is intentionally false. Exact range/name shadowing and the
canonical target-payload gate close those two false-positive classes.

On fixed HEAD `b57f0c3` plus a byte-exact 10-path code/test overlay, GCC 11.4,
Clang 14.0.0, and MSVC 19.44.35228.0 (`VSCMD_VER=17.14.38`) each directly passed
the same ten targets:

- compiler semantic-query diagnostics: `53/53`;
- semantic-query diagnostic disposition: `11/11`;
- semantic facts: `14/14`;
- semantic query: `30/30`;
- type inference: `123/123`;
- LSP semantic-query diagnostics;
- semantic analyzer regressions;
- LSP source contracts;
- union-pattern diagnostics;
- compiler integration: `127/127`.

Each toolchain also passed the dedicated stdio smoke with a real exit code of
zero. It requires exactly one `member_not_found` result on the missing member
token, descriptor `2016`, canonical full text, registered help URI,
`requires_user_decision`, and no fixes. A distinct receiver type-inference
diagnostic is allowed only on its own non-overlapping range. The complete stdio
suite was not rerun for this submilestone.

## 状态与产出记录

- 完成时间：2026-08-27 06:34 +08:00。
- 状态：已完成 unresolved-reference parser query projection，并通过
  GCC/Clang/MSVC focused、compiler integration 与独立 stdio 验收；Plan 03
  Task 6 继续进行。
- 完成项目：descriptor 2015/2016、canonical fact 分类与 exact range、
  resolved fact shadowing、external callable payload 排除、统一 no-fix、LSP
  generic query projection、source contract、member source-to-stdio 三工具链证据。
- 后续项目：在 Syntax L8 释放 producer 路径后补 plain identifier source
  publication，并继续迁移剩余 analyzer-owned semantic diagnostics。
