---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.c
tests:
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_interface.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 7.4: Project Reference Fallback

## Scope

This slice migrates the source-symbol fallback in project find-references from
`SZrReferenceTracker` to the same parser relation projector used by source-local
references. Imported-member aggregation across project records remains an open
name-keyed boundary. Syntax05-owned interface, property, symbol, metadata, and
token paths are untouched.

## TDD And Root Cause

On fixed HEAD `0e01d533d3dae9eca5a1b54572e283a43035ea83`, the new source
contract failed twice: project navigation did not call the shared SymbolId fact
projector and still called `ReferenceTracker_FindReferences`.

The first GREEN implementation exposed a control-flow distinction that the old
tracker helper hid. A valid symbol can have zero references in its defining
file while imported references exist elsewhere. Treating zero local appends as
query failure would skip project aggregation. The shared operation therefore
reports query success independently through its return value and local append
state through `outAppended`.

## Implementation

`ZrLanguageServer_LspSemanticReferenceQuery_AppendReferencesForSymbol` accepts
an analyzer snapshot and one exact SymbolId-bearing symbol. It queries parser
`DeclarationOf/ReferencesOf`, projects exact fact locations, preserves source
binding, deduplication, and cancellation, and returns false for invalid identity
or semantic context. Its existing query-object wrapper retains the prior
"at least one local result" behavior.

Project navigation now calls that shared operation and no longer reads the
reference tracker. A successful zero-local-result query continues to the
existing imported-reference stage. This slice does not claim that stage as
canonical: it still aggregates by module/member spelling and remains open.

## Verification

GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 each return real exit zero for
source contracts `56/56`, semantic-query parity `4/4`, and the project runner.
The project runner retains `54 Pass / 6` pre-existing markers on every
toolchain. GCC fixed-parent and overlay runs have marker delta zero.

The full interface runner returns exit one on every toolchain with `109 Pass /
4` identical pre-existing markers; it is not counted as GREEN. Workspace, WSL,
and MSVC snapshots match `4/4`; `git diff --check` passes.

## 状态与产出记录

- 完成时间：2026-08-28 19:59 +08:00。
- 状态：Task 7.4 project reference fallback 子里程碑已完成；Plan 03 Task 7
  继续。
- 完成项目：project source-symbol fallback relation projection、tracker call
  removal、zero-local-result control flow、三工具链 `56/4` focused 门禁、
  project/interface marker audit、GCC parent/overlay delta 0、三处 `4/4`
  byte audit。
- 未完成项目：cross-project imported-member stable identity aggregation、
  binary/native external references、rename 与其余 navigation consumers，
  以及 Syntax05 当前占用的 property/symbol consumer paths。
