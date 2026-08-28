---
related_code:
  - zr_vm_parser/include/zr_vm_parser/interface_contract.h
  - zr_vm_parser/src/zr_vm_parser/semantic/interface_contract.c
tests:
  - tests/parser/test_compiler_interface_const_query_producer.c
  - tests/parser/test_semantic_query_diagnostics.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 6.35: Interface Const Fact Producer Support

## Scope

The remaining interface const-field LSP producer enumerates parser contract
violations, builds descriptor `2014`, and appends facts inside the symbols
analyzer. That LSP path is held by the parallel Syntax05 property task. This
slice establishes the complete parser publisher without editing or staging the
reserved consumer. Consumer deletion remains a separate follow-up.

## TDD And Root Cause

On fixed HEAD `9a5c77e31aebc6e9afbb3ac687d17a08319fafc4`, the new
parser target linked RED only for the absent
`ZrParser_InterfaceContract_PublishConstFieldDiagnostics` API. An initial test
that compiled an invalid class produced no query rows because compilation
stops before retaining that class prototype. The corrected lower-layer test
therefore supplies the exact canonical prototype facts consumed by the public
contract: one mutable implementation field and one missing field satisfy two
const interface requirements incorrectly.

## Implementation

The publisher repeatedly calls
`ZrParser_InterfaceContract_ConstFieldViolationAt`, builds each diagnostic with
the existing shared builder, and appends a deep-copied persistent fact to the
semantic context. It validates compiler state and semantic context, releases
temporary diagnostic storage on every path, and never changes `hasError`,
`hasStructuredError`, or `errorMessage`.

No LSP source is changed. In particular, the reserved symbols analyzer still
owns its temporary loop until Syntax05 releases the path. This support API does
not add a name, source-text, or AST-pair fallback.

## Verification

GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 each return real exit zero for:

- interface const producer `1/1`, covering drop-const and missing-field facts;
- diagnostic disposition `11/11`;
- compiler semantic query diagnostics `64/64`.

The test also preserves a pre-existing unstructured compiler error byte-for-
byte while materializing both descriptor `2014` rows, each with one related
range and `REQUIRES_USER_DECISION`. Workspace, WSL, and MSVC SHA-256 match for
all four code/test overlay paths (`4/4`).

## 状态与产出记录

- 完成时间：2026-08-28 18:25 +08:00。
- 状态：parser producer support 子里程碑已完成；LSP consumer 迁移待外部
  exact ownership 释放，Plan 03 Task 6 继续进行。
- 完成项目：public persistent-fact publisher、drop-const + missing-field
  双 violation、既有 compiler error 状态保持、descriptor 2014 query
  materialization、三工具链 `1/11/64` focused 门禁、三处 `4/4` byte audit。
- 后续项目：释放 `semantic_analyzer_symbols.c` 后删除其中的 violation
  enumerator/builder/append loop，只调用本 API，并补 LSP/stdio parity；禁止
  member-name 或 AST pairing fallback。
