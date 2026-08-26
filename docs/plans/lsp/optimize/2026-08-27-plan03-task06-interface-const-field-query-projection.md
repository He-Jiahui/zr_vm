---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/interface_contract.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c
  - zr_vm_parser/src/zr_vm_parser/semantic/interface_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_messages.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/interface_contract.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c
  - zr_vm_parser/src/zr_vm_parser/semantic/interface_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_messages.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_semantic_query.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_interface_const_field_diagnostic_smoke.js
  - tests/acceptance/2026-08-27-plan03-task06-interface-const-field-query-projection.md
doc_type: milestone-record
---

# Plan 03 Task 6.16: Interface Const-Field Query Projection

## Goal

Move interface const-field implementation validation and diagnostic creation
into one parser-owned semantic contract so compiler and LSP consumers share the
same violation identity, exact ranges, message, related information, and no-fix
disposition.

## Contract

- `ZrParser_InterfaceContract_ConstFieldViolationAt` enumerates missing and
  non-const implementations in stable inherited-interface/member order.
- Results carry the violation kind, borrowed field identity, exact primary
  range, and exact required interface field declaration range.
- `ZrParser_InterfaceContract_BuildConstFieldDiagnostic` is the only formatter
  for descriptor `2014` / `const_interface_mismatch`.
- The compiler consumes violation zero as a structured compilation failure.
  LSP enumerates every violation and only appends parser-built diagnostic facts.
- LSP must not scan inheritance AST, use symbol-table or member-name matching,
  retain the old missing-field TODO, or construct the diagnostic directly.
- Both failure kinds require user intent and therefore publish no machine fix.

## Implementation

The prior compiler validation loop became a small consumer of a public ordinal
query. The query walks canonical type prototypes and their member facts; it
returns a missing-field violation when no implementation member exists and a
drop-const violation when the canonical member is mutable. The shared builder
publishes one descriptor/message family and a related link to the interface
requirement.

The LSP analyzer's inheritance AST traversal, symbol-table type lookup,
member-name pairing, direct diagnostic construction, and unimplemented missing
field branch were removed. Its replacement only enumerates parser violations,
builds the canonical structured diagnostic, and appends a deep-copied semantic
fact.

Protocol RED exposed that interface field AST nodes did not retain their name
range and therefore produced a related range extending into later declarations.
`SZrInterfaceFieldDeclaration.nameLocation` now captures the existing parsed
identifier token range, allowing the query to project exact identity without
text-width inference.

## Verification

TDD RED independently established the missing compiler structured diagnostic,
missing descriptor `2014`, LSP's missing-field omission, analyzer-owned policy,
and imprecise related range. The shared query/builder, LSP projector, and exact
AST name range closed those gaps.

On fixed HEAD `d12911e` plus a byte-exact 13-path code/test overlay, GCC 11.4,
Clang 14, and MSVC 19.44 (`VSCMD_VER=17.14.38`) each directly passed the same ten
targets:

- compiler semantic-query diagnostics: `53/53`;
- semantic-query diagnostic disposition: `8/8`;
- semantic facts: `14/14`;
- semantic query: `30/30`;
- type inference: `123/123`;
- LSP semantic-query diagnostics;
- semantic analyzer regressions;
- LSP source contracts;
- union-pattern diagnostics;
- compiler integration: `127/127`.

Each toolchain also passed the dedicated stdio smoke. It requires exactly two
descriptor-2014 diagnostics, canonical full messages, non-const field and
missing-class primary ranges, exact interface-field related ranges, registered
help URI, `requires_user_decision`, and no fixes. The complete stdio suite was
not rerun for this submilestone.

## 状态与产出记录

- 完成时间：2026-08-27 04:41 +08:00。
- 状态：已完成 interface const-field parser-owned query projection，并通过
  GCC/Clang/MSVC focused、compiler integration 与独立 stdio 验收；不声明
  完整 stdio 基线或 Plan 03 Task 6 完成。
- 完成项目：稳定 ordinal violation 枚举、missing/drop-const 两类结构化
  结果、接口字段 exact name range、descriptor 2014、canonical
  primary/related ranges、requires_user_decision no-fix、compiler/LSP parity、
  LSP duplicate policy 与 missing-field TODO 删除、source contract 和三工具链
  transport 回归。
- 后续项目：继续迁移 field/call compatibility、unresolved reference 等
  analyzer-owned semantic diagnostics。
