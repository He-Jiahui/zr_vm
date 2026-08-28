---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_definition_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
tests:
  - tests/language_server/test_lsp_reaching_definition_navigation.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_interface.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 7.5: Local Definition Exactness

## Scope

This slice removes request-URI, LSP-symbol-location, and token/name range
fallbacks from source-local definition projection. It uses parser relations
within one analyzer snapshot only. Cross-project imported definitions and
references remain separate open identity boundaries. Syntax05-owned paths are
untouched.

## TDD And Root Cause

On fixed HEAD `2a23616c2de002d810559284c233c28d693ec9ee`, source contracts
failed on three legacy source paths: no analyzer source binder, request URI
fallback, and symbol-location fallback. The focused runtime kept its two prior
reaching-definition cases green and failed only the new source-less snapshot
case.

Deleting the fallbacks exposed three full-interface regressions: closed-generic
type, extern function, and web-URI local definition. Each query already carried
a valid canonical SymbolId, but `DefinitionsOf(position)` had no position fact.
The missing support was not a reason to restore spelling reconstruction:
parser `DeclarationOf(SymbolId)` supplied the exact declaration fact.

## Implementation

The definition projector binds copied fact ranges only through
`SemanticAnalyzer_BindQuerySource`. It uses `DefinitionsOf` for reaching writes
and multiple CFG definitions. If there is no position fact, it uses
`DeclarationOf` only for the already-resolved valid SymbolId in the same
semantic context. Missing fact and snapshot sources fail closed.

The local-symbol branch now returns that projector directly. The large
semantic-query module no longer contains its enum-member source scan,
identifier-boundary reconstruction, offset-to-position helper, or LSP symbol
location fallback. This removes 134 lines from that module.

## Verification

GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 each return real exit zero for
source contracts `57/57`, reaching definition `3/3`, and semantic-query parity
`4/4`. The full interface runner returns exit one on every toolchain with the
same `109 Pass / 4` pre-existing markers as Task 7.4; marker delta is zero and
the runner is not counted as GREEN. Workspace, WSL, and MSVC bytes match
`4/4`; `git diff --check` passes.

## 状态与产出记录

- 完成时间：2026-08-28 20:23 +08:00。
- 状态：Task 7.5 local definition exactness 子里程碑已完成；Plan 03 Task 7
  继续。
- 完成项目：DefinitionsOf/DeclarationOf canonical projection、analyzer-only
  source binding、missing-source fail-closed、enum/name/symbol-location fallback
  deletion、三工具链 `57/3/4` focused 门禁、interface marker delta 0、三处
  `4/4` byte audit。
- 未完成项目：cross-project imported target identity、binary/native external
  navigation、rename、implementation/remaining navigation consumers，以及
  Syntax05 当前占用的 property/symbol consumer paths。
