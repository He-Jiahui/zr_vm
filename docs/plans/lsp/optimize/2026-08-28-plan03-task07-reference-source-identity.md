---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c
tests:
  - tests/language_server/test_reference_tracker.c
  - tests/language_server/test_lsp_reaching_definition_navigation.c
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 7.1: Reference Source Identity

## Scope

Task 7 requires definition, reference, highlight, and rename consumers to use
stable semantic identities and explicitly calls out the legacy reference
tracker's empty-source equality defect. The interface and symbols paths needed
for the broader migration remain owned by Syntax05, so this slice closes the
independent source-identity boundary first.

## TDD And Root Cause

On fixed HEAD `139edc796747bce112a6f76b19f55b60914af7af`, the
existing three tracker tests passed and the new source-identity case failed
only because a source-less query matched a source-bound reference.

`source_uri_equals` returned true whenever either URI was null. Its preceding
pointer comparison also treated two null pointers as equal. Range containment
therefore allowed unrelated documents with identical coordinates to share a
reference match.

## Implementation

The tracker now rejects the comparison if either source URI is absent. Only
after both identities are available may pointer equality or exact URI text
equality succeed. No symbol-name, path normalization, or coordinate fallback
is added.

The test covers source-bound/reference-less and reference-less/source-bound
comparisons, two missing sources, and equal URI text stored in distinct string
objects. The first three fail closed and the last remains a valid match.

## Verification

GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 each return real exit zero for:

- reference tracker, four scenarios;
- reaching-definition navigation `2/2`;
- source/binary/native semantic query parity `3/3`;
- LSP source contracts, 54 pass markers.

Workspace, WSL, and MSVC SHA-256 match for both code/test overlay paths
(`2/2`).

## 状态与产出记录

- 完成时间：2026-08-28 18:35 +08:00。
- 状态：Task 7.1 子里程碑已完成；Plan 03 Task 7 继续进行。
- 完成项目：missing-source fail-closed RED/GREEN、distinct-string URI text
  equality、三工具链 tracker/reaching/parity/source-contract 门禁、三处 `2/2`
  byte audit。
- 后续项目：definition/references/highlights/rename 继续迁移到 SymbolId 与
  relation index；当前 Syntax05 exact-own 的 interface/symbol paths 释放前不
  进入。Task 6 的 interface const LSP consumer loop 仍待删除。
