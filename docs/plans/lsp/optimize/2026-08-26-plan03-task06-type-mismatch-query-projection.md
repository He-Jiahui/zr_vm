---
related_code:
  - zr_vm_parser/include/zr_vm_parser/type_inference.h
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_expected_type.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_expected_type.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/type_inference.h
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_expected_type.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_expected_type.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_type_mismatch_diagnostic_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_diagnostic_fix_smoke.js
  - tests/acceptance/2026-08-26-plan03-task06-type-mismatch-query-projection.md
doc_type: milestone-record
---

# Plan 03 Task 6.11: Type Mismatch Query Projection

## Goal

Make assignment, explicit initializer, and return-value type mismatch
diagnostics originate in parser/compiler structured facts. The LSP analyzer may
locate the expected declaration range and project the query result, but must not
reconstruct diagnostic policy from type names, messages, or source text.

## Contract

- `ZrParser_AssignmentCompatibility_CheckDetailed` accepts the actual
  expression range and an optional expected-type declaration range.
- An incompatible assignment creates descriptor `2011`, code `type_mismatch`,
  canonical severity/message/cause/suggestion, expected-type related
  information, and the typed placeholder cast fix.
- The LSP assignment, explicit initializer, and return branches publish the
  current compiler diagnostic into semantic query facts, then consume the
  shared projection path.
- An assignment expression statement does not independently infer and publish
  a second broad mismatch before the dedicated assignment check.
- No LSP producer replaces, merges, or reconstructs type mismatch diagnostics
  by range, name, message, or inferred type text.

## Implementation

The parser compatibility API now has a detailed entry point that threads the
expected declaration range through the existing structured diagnostic builder.
The old API remains a compatibility wrapper with no related range, so existing
compiler callers preserve behavior.

The semantic analyzer computes declaration ranges before compatibility checks
and calls the detailed API. On failure it uses the existing compiler-diagnostic
publisher, which deep-copies the structured error into persistent semantic
facts and materializes the normal LSP diagnostic query. The former
`semantic_analyzer_type_mismatch_diagnostics.c/.h` policy module was deleted;
only the symbol-table-backed expected-range helper remains in a small module.

Support-first RED also exposed a duplicate path: expression-statement checking
inferred the whole assignment and directly consumed that compiler error before
the assignment node ran. Assignment expressions are now excluded from that
generic inference branch, leaving the dedicated canonical producer as the only
source. The LSP regression asserts exactly two mismatch diagnostics for one
return and one assignment, both with precise related ranges and typed fixes.

## Verification

The initial parser RED failed to link because the detailed compatibility API
did not exist. After the first GREEN, the LSP assignment/return fixture still
observed three diagnostics: a broad assignment range without related
information plus the two precise facts. The traversal fix reduced this to the
two canonical facts and made the strengthened cardinality assertion pass.

On one fixed source snapshot, GCC 11.4, Clang 14, and MSVC 19.44 each directly
passed:

- compiler semantic query diagnostics: `48/48`;
- LSP semantic query diagnostics, including detailed initializer,
  assignment, and return mismatches;
- LSP source contracts;
- type inference: `123/123`;
- compiler integration: `127/127`;
- stdio structured-diagnostic fix smoke with real process exit zero.

## 状态与产出记录

- 完成时间：2026-08-26 22:55 +08:00。
- 状态：已完成 type mismatch canonical query 迁移并通过三工具链与 stdio
  验收；不声明 Plan 03 Task 6 完成。
- 完成项目：详细 assignment compatibility API、expected-type related range、
  descriptor 2011/type_mismatch persistent fact、typed cast fix、assignment /
  initializer / return LSP query projection、旧 LSP mismatch producer 删除、
  宽范围重复诊断消除与 exact-cardinality 回归。
- 后续项目：继续删除剩余 semantic analyzer 重复类型/语义诊断，并完成
  compiler/LSP golden parity 与 Task 6 最终门禁。
