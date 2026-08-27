---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_top_level_duplicate.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_semantic_query_diagnostics.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_duplicate_type_diagnostic_smoke.js
  - tests/acceptance/2026-08-27-plan03-task06-duplicate-type-query-projection.md
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 6.21: Duplicate Type Query Projection

## Scope

This submilestone moves duplicate type-definition policy from the language
server into parser/compiler structured diagnostics. It covers source class
declarations end to end and preserves the existing fail-closed range boundary
for declaration shapes without a dedicated AST name range.

The fixed overlay contains thirteen modified or added code/test paths and two
deleted LSP producer paths. Module, milestone, and acceptance documentation add
three paths to the final exact commit.

## TDD And Root Cause

The parser RED compiled two `class Pair` declarations but exposed only a generic
compiler failure; no `duplicate_type` structured query fact existed. The LSP
test remained green because `semantic_analyzer_duplicate_diagnostics.c` owned a
parallel rule, selected a previous symbol by name, and built descriptor `2010`
inside the analyzer.

The first parser-backed LSP run also exposed a bootstrap-placeholder ordering
problem: the first matching semantic symbol had no source AST and produced a
zero range for related information. The parser registration bridge now prefers
an exact same-name source type symbol with declaration AST identity. It does
not ask the LSP symbol table for `selectionRange` and does not reconstruct a
declaration from text.

## Implementation

`compiler_diagnostics.c` now owns the one duplicate-type descriptor builder.
It publishes stable code `duplicate_type`, descriptor `2010`, error severity,
the exact current class-name range, one exact previous-declaration relation
when available, cause and suggestion, no machine fixes, and explicit
`REQUIRES_USER_DECISION` disposition.

Normal compiler entry points use the cohesive
`compiler_top_level_duplicate.c` helper after canonical prototypes exist.
Language-server symbol collection delegates type-environment registration to
`ZrParser_Compiler_RegisterTypeBinding`, consumes the compiler error, and later
projects the persistent semantic-query diagnostic. The former LSP duplicate
diagnostic source and header are deleted.

The source-contract test rejects a local diagnostic builder, the literal
`duplicate_type`, or restoration of the deleted producer in analyzer code. The
same-snapshot golden parity fixture compares every compiler-query field with
the projected analyzer diagnostic. A dedicated stdio smoke freezes protocol
serialization without expanding the shared all-feature smoke.

## Verification

The final source baseline is HEAD
`96f6b731c0572c1c91a5defaea8c5876ce0afbb7` plus thirteen byte-identical
modified/added code-test paths and two deleted legacy producer paths. SHA-256
auditing matched all thirteen files across the workspace, GCC snapshot, Clang
snapshot, and MSVC snapshot; both legacy files were absent in every snapshot.

GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0
(`VSCMD_VER=17.14.38`) each directly passed the same eleven checks with real
process exits:

- compiler semantic-query diagnostics: `57/57`;
- semantic-query diagnostic disposition: `11/11`;
- semantic facts: `15/15`;
- semantic query: `30/30`;
- type inference: `123/123`;
- compiler integration: `127/127`;
- LSP semantic-query diagnostics: 15 pass markers;
- semantic analyzer: 55 pass markers, including duplicate golden parity;
- LSP source contracts: 47 pass markers;
- union-pattern diagnostics: 12 pass markers;
- dedicated duplicate-type stdio smoke: real exit zero.

All non-integration logs contained zero `Fail -` markers. Compiler integration
retained the same nine embedded negative-fixture markers on all three
toolchains while its outer Unity result remained `127 Tests 0 Failures`.

## 状态与产出记录

- 完成时间：2026-08-27 12:40 +08:00。
- 状态：已完成 duplicate type 的 parser/compiler structured diagnostic、
  semantic query 与 LSP/stdIO 单一投影，并通过 GCC/Clang/MSVC 同基线验收；
  Plan 03 Task 6 继续进行。
- 完成项目：descriptor `2010` 单一生产、精确主 range、first-declaration
  related range、explicit no-fix、旧 LSP producer 删除、compiler/LSP 全字段
  golden parity、source contract、独立 stdio smoke、三工具链 byte-exact
  11 项证据。
- 后续项目：继续迁移仍由 analyzer 重复实现的语义诊断；任何缺失 parser
  producer 必须 support-first 修复，不得在 LSP 按名称、AST 或消息补偿。
