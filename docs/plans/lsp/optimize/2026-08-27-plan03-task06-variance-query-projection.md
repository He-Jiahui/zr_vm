---
related_code:
  - zr_vm_parser/include/zr_vm_parser/variance.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_generic_semantics.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_messages.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_variance.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/variance.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_generic_semantics.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_messages.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_variance.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_semantic_query.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_variance_diagnostic_smoke.js
  - tests/acceptance/2026-08-27-plan03-task06-variance-query-projection.md
doc_type: milestone-record
---

# Plan 03 Task 6.15: Variance Query Projection

## Goal

Move interface variance validation and diagnostic construction into one
parser-owned semantic contract so compiler and LSP consumers publish identical
diagnostic identities, ranges, related information, and no-fix disposition.

## Contract

- `ZrParser_Variance_InterfaceViolationAt` enumerates violations in stable AST
  order and returns exact use/declaration ranges plus structured context.
- The query covers interface fields, method/meta parameters and returns,
  read/write properties, and nested generic variance composition.
- Result AST/string identities are borrowed from the compiler snapshot.
- `ZrParser_Variance_BuildDiagnostic` owns descriptor `2013`, code
  `invalid_variance`, message, cause, suggestion, related declaration range, and
  `requires_user_decision` no-fix disposition.
- The compiler consumes the first violation as a compilation failure. LSP
  enumerates all violations and only appends parser-built semantic facts.
- LSP must not use symbol-table lookup, member/type name inference, duplicated
  recursive variance checking, or direct diagnostic construction.

## Implementation

The existing compiler variance traversal was converted into an ordinal query
over a small public result structure. The traversal still uses canonical
interface generic declarations and registered prototype variance for nested
generic arguments; no textual type fallback was introduced. Compiler failure
now builds and publishes the same structured diagnostic later consumed by
semantic-query clients.

The prior 377-line LSP variance implementation was replaced by a focused
projector. It loops over parser violations, calls the shared builder, and
appends deep-copied diagnostic facts. The diagnostic registry and message table
publish stable descriptor `2013` and one canonical message family.

Parser tests freeze the compiler/query payload and registry identity. Analyzer
tests cover all existing variance contexts and require descriptor, related
range, and no-fix parity. A source-contract test forbids the removed LSP policy,
and a dedicated stdio fixture validates exact protocol transport.

## Verification

TDD RED established four independent gaps: compiler emitted only a plain error,
descriptor `2013` was absent, LSP payload lacked canonical metadata, and the LSP
source still owned recursive variance policy. After the shared query/builder and
projector replacement, all four gates became GREEN.

On fixed HEAD `67f45cc` plus the byte-exact 10-path code/test overlay, GCC 11.4,
Clang 14, and MSVC 19.44 (`VSCMD_VER=17.14.38`) each directly passed the same ten
targets:

- compiler semantic-query diagnostics: `52/52`;
- semantic-query diagnostic disposition: `8/8`;
- semantic facts: `14/14`;
- semantic query: `30/30`;
- type inference: `123/123`;
- LSP semantic-query diagnostics;
- semantic analyzer regressions;
- LSP source contracts;
- union-pattern diagnostics;
- compiler integration: `127/127`.

Each toolchain also passed the dedicated variance stdio smoke with exact code,
descriptor, severity, primary range, related declaration range, full message,
help URI, no-fix reason, and one-diagnostic cardinality. The full stdio suite was
not rerun in this submilestone; the previously recorded unrelated legacy
reachability expectation remains outside this acceptance claim.

## 状态与产出记录

- 完成时间：2026-08-27 03:09 +08:00。
- 状态：已完成 variance parser-owned query projection，并通过
  GCC/Clang/MSVC focused、compiler integration 与独立 stdio 验收；不声明
  完整 stdio 基线或 Plan 03 Task 6 完成。
- 完成项目：稳定 violation 枚举、field/parameter/return/property/nested
  variance context、descriptor 2013、canonical primary/related ranges、
  requires_user_decision no-fix、compiler/LSP parity、LSP duplicate policy
  删除、source contract 与三工具链 transport 回归。
- 后续项目：继续迁移 interface contract、field/call compatibility、
  unresolved reference 等 analyzer-owned semantic diagnostics。
