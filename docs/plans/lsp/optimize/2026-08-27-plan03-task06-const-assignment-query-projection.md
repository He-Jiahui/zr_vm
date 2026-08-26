---
related_code:
  - zr_vm_parser/include/zr_vm_parser/const_assignment.h
  - zr_vm_parser/src/zr_vm_parser/semantic/const_assignment.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_messages.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_const_assignment.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/const_assignment.h
  - zr_vm_parser/src/zr_vm_parser/semantic/const_assignment.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_messages.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_const_assignment.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_semantic_query.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_const_assignment_diagnostic_smoke.js
  - tests/acceptance/2026-08-27-plan03-task06-const-assignment-query-projection.md
doc_type: milestone-record
---

# Plan 03 Task 6.14: Const Assignment Query Projection

## Goal

Move const-assignment legality and structured diagnostic construction into a
parser-owned query contract so compiler and LSP consumers publish the same
diagnostic identity, ranges, disposition, and constructor exception without
reconstructing policy from names or source text.

## Contract

- The parser classifies local, parameter, instance-field, and static-field
  assignment targets from resolved declaration identity.
- An instance `const` field is writable only through the current instance in
  the declaring type's constructor. Other const targets remain immutable.
- If a member reference fact is unavailable, parser context evaluation accepts
  only a unique explicit `this.field` or `CurrentType.staticField` declaration
  from the current canonical prototype and otherwise fails closed.
- Descriptor `2012`, code `const_assignment`, error severity, immutable
  declaration related range, help URI, and `requires_user_decision` no-fix
  disposition are produced by `ZrParser_ConstAssignment_BuildDiagnostic`.
- LSP resolves assignment targets with `SymbolAt` and stable `SymbolId`; it does
  not call symbol-table name lookup or assemble a parallel diagnostic.

## Implementation

The new `const_assignment.h` and `semantic/const_assignment.c` API owns target
classification, constructor context, and structured diagnostic construction.
Compiler assignment lowering delegates its prior local, parameter, and field
checks to this API, preserving compilation failure while publishing the same
persistent semantic diagnostic fact consumed by query clients.

The LSP analyzer now has a focused const-assignment projector. Typecheck remains
an orchestrator: it traverses class and struct method bodies with the active
owner/function context, asks `SymbolAt` for exact target identity, delegates the
decision to the parser API, and appends the returned structured fact. The old
direct analyzer diagnostic branch was removed.

The diagnostic registry adds stable descriptor `2012` and the message table
provides one canonical title/message pair. Parser and LSP tests cover const
locals, parameters, instance fields, static fields, constructor allowance,
related declaration ranges, and no-fix disposition. Source-contract coverage
forbids name lookup and direct diagnostic assembly.

## Verification

TDD first exposed the missing compiler structured fact and missing LSP field
diagnostic. Expanded analyzer coverage then produced one final RED for a static
const field because only current-instance context projection existed. The
parser context resolver was extended with the symmetric, canonical current-type
static-field rule, after which the expanded target matrix passed.

On fixed HEAD `682f9c0` plus one 13-path code/test overlay, GCC 11.4, Clang 14,
and MSVC 19.44 each directly passed the same ten targets:

- compiler semantic-query diagnostics: `51/51`;
- semantic-query diagnostic disposition: `8/8`;
- semantic facts: `14/14`;
- semantic query: `30/30`;
- type inference: `123/123`;
- LSP semantic-query diagnostics;
- semantic analyzer regressions;
- LSP source contracts;
- union-pattern diagnostics;
- compiler integration: `127/127`.

Each toolchain also passed the dedicated const-assignment stdio smoke with exact
descriptor, code, full message, primary range, related declaration range,
no-fix reason, help URI, and diagnostic cardinality. The repository-wide GCC
`stdio_smoke.js` stopped earlier at its legacy generic-fixture expectation for
`short_circuit_unreachable` while the current canonical producer reports
`unreachable_code`; that unrelated baseline is not counted as a full stdio pass.

## 状态与产出记录

- 完成时间：2026-08-27 02:33 +08:00。
- 状态：已完成 const-assignment parser-owned query projection，并通过
  GCC/Clang/MSVC focused 与独立 stdio 验收；不声明完整 stdio 基线或
  Plan 03 Task 6 完成。
- 完成项目：local/parameter/instance/static target classification、constructor
  exception、descriptor 2012、canonical related range、requires_user_decision
  no-fix、compiler/LSP parity、stable SymbolId consumer、name lookup 排除、
  source contract 与三工具链 transport 回归。
- 后续项目：继续迁移 variance、interface contract、unresolved reference 等
  analyzer-owned semantic diagnostics，并在独立基线任务中更新完整 stdio 的
  legacy reachability expectation。
