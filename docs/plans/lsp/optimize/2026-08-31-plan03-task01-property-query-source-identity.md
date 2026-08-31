---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_property.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_property_code_actions.c
tests:
  - tests/parser/test_property_consumer_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
module_docs:
  - docs/parser-and-semantics/semantic-query-api-foundation.md
  - docs/parser-and-semantics/lsp-semantic-resolution-and-native-imports.md
doc_type: milestone-record
---

# Plan 03 Task 1.5: Property Query Source Identity

## Scope

This submilestone makes `PropertyAt` require exact optional source identity
when matching reference and property declaration ranges. It also preserves the
current document URI in the LSP property code-action query position.

The change does not alter property producers, imported metadata, accessor
identity, `PropertyBySymbolId`, or code-action text. No property name or hidden
accessor fallback is introduced.

## TDD And Root Cause

The parser RED first queries a source property usage with its snapshot source,
then repeats the same offset with the request source cleared. The second query
must fail closed and clear its output. The existing eleven-test suite has one
failure: `Expected FALSE Was TRUE`.

The local property comparator treated either null source as a wildcard. After
the query was corrected, the interface marker
`LSP Property Refactor Uses Canonical Query` exposed one consumer that built a
sourceless range even though it already held the current document URI.

## Implementation

`semantic_property_same_source` now accepts pointer identity, including two
null sources, or string-value equality when both sources are non-null. The
property code-action consumer passes its current URI to
`ZrParser_FileRange_Create`. All query ordering, SymbolId joins, and refactor
guards remain unchanged.

## Verification

The fixed isolated source is HEAD `481e09a` plus the three exact code/test
paths from this record. WSL GCC 11.4 and Clang 14.0.0 directly passed:

- property consumer contracts: `11/11`, run GCC then Clang serially;
- semantic facts/query/query contract/canonical consumers: `17/30/6/21`;
- LSP semantic-query parity/source contracts: `15/70`.

The first parallel property-contract run was discarded because both processes
write the same temporary `.zro` names; each lost a different file. The serial
runs returned real process exit zero. The final GCC and Clang LSP interface
runs restored the property refactor case to PASS and retained exactly the same
eight pre-existing producer markers. MSVC, the complete 16-target matrix, and
the three stdio smoke suites were not run for this narrow query correction.

## 状态与产出记录

- 完成时间：2026-08-31 10:45 +08:00。
- 状态：Task 1.5 property query source identity 子里程碑已完成；Plan 03
  整体计划继续进行。
- 完成项目：sourced/sourceless `PropertyAt` RED、exact optional source
  identity、property code-action URI投影、两工具链`11/17/30/6/21/15/70`
  真实退出门禁、property refactor PASS、interface fixed8 delta 0。
- 后续项目：继续完成Task 3/4的external origin与receiver TypeId producer缺口；
  Syntax05正在修改的native/import property producer保持串行隔离，MSVC、完整矩阵和
  stdio由后续阶段统一验收。
