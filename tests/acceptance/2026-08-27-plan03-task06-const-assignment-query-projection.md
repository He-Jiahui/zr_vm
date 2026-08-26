---
related_code:
  - zr_vm_parser/include/zr_vm_parser/const_assignment.h
  - zr_vm_parser/src/zr_vm_parser/semantic/const_assignment.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_const_assignment.c
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_semantic_query.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_const_assignment_diagnostic_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-27-plan03-task06-const-assignment-query-projection.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.14 Const Assignment Query Projection

## Required Results

- Parser-owned evaluation classifies const locals, parameters, instance fields,
  and static fields from canonical declaration identity.
- Constructor writes to the current instance field are allowed; immutable writes
  elsewhere produce one structured descriptor `2012` diagnostic.
- Compiler and LSP diagnostics share code, severity, primary range, related
  declaration range, help URI, and no-fix disposition.
- LSP consumes `SymbolAt` and stable `SymbolId` without name lookup or direct
  diagnostic construction.
- Focused parser/query/LSP tests and dedicated stdio transport pass under GCC,
  Clang, and MSVC.

## Evidence

The initial REDs showed no compiler structured fact and no LSP field diagnostic.
After traversal and parser-query projection were added, expanded local,
parameter, instance-field, and static-field coverage retained one static-field
failure. Adding the exact current-type static declaration rule to parser context
evaluation made the same expanded tests GREEN without an LSP fallback.

On fixed HEAD `682f9c0` plus the 13-path code/test overlay, GCC 11.4, Clang 14,
and MSVC 19.44 each returned real exit zero for the same ten focused targets.
Their Unity totals include compiler diagnostics `51/51`, query disposition
`8/8`, semantic facts `14/14`, semantic query `30/30`, type inference `123/123`,
and compiler integration `127/127`; the four LSP targets also passed.

The dedicated stdio fixture requires exactly one method-body diagnostic at
zero-based line 6 columns 8..25, related immutable field declaration at line 1
columns 14..19, descriptor `2012`, code `const_assignment`, registered help URI,
`requires_user_decision`, and no fixes. All three toolchain servers passed it.

The full GCC stdio suite stopped at an unrelated legacy generic-fixture
reachability-code expectation. It is recorded as residual baseline evidence and
is not represented as a pass here.

## Acceptance Decision

Accepted for const-assignment query projection. Parser and LSP still have other
semantic diagnostic families to migrate, so Plan 03 Task 6 remains in progress.

## 状态与产出记录

- 完成时间：2026-08-27 02:33 +08:00。
- 状态：本子项已完成并通过 GCC/Clang/MSVC focused 与独立 stdio 验收；
  Task 6 继续进行。
- 完成项目：canonical const legality、constructor exception、structured
  descriptor 2012、related range、no-fix disposition、compiler/LSP parity、
  no-name-fallback source contract 与三工具链 transport 证据。
