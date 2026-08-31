---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
tests:
  - tests/parser/test_semantic_query_contract.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
module_docs:
  - docs/parser-and-semantics/semantic-query-api-foundation.md
doc_type: milestone-record
---

# Plan 03 Task 1.2: Semantic Fact Source Identity

## Scope

This submilestone makes lower semantic-fact position lookup and canonical type
queries obey the exact optional source identity already required by the public
semantic-query contract. Two absent sources match, two present sources match by
pointer or string value, and one absent source never matches a present source.

The correction covers `FactsAt` and its expression, reference, numeric,
reachability, logical, and ownership inputs, as well as the request and
node-scope checks used by `CanonicalTypeAt`. It does not add an LSP fallback,
reconstruct identity from source text, or change protocol projection.

## TDD And Root Cause

The RED adds two contract cases at offsets already occupied by a sourced exact
expression fact. One queries `FactsAt` with a sourceless position; the other
queries `CanonicalTypeAt` with a sourced position inside a sourceless node
scope. The original four tests pass and both new cases fail: `6 Tests / 2
Failures`.

`semantic_facts_same_source` and `canonical_query_same_source` still treated a
null operand as a wildcard. Task 6.39 had already fixed the public query-scope
comparator, but these lower helpers could still admit a fact before or beside
that boundary.

## Implementation

Both helpers now accept a null source only when the opposite source is also
null. Existing pointer equality and non-null string equality remain unchanged.
The canonical exact-optional helper delegates to that single rule, so position
and node-scope containment cannot drift apart again.

Four reaching-definition tests had manually cleared the valid source returned
by their snapshot position converter. Expanded regression exposed those stale
fixtures after the production contract was corrected. They now retain the
canonical source identity and continue to test reaching-definition behavior;
no wildcard behavior was restored. An explicit native-string cast also removes
the pre-existing const warning from the rebuilt test translation unit.

## Verification

The fixed isolated source is HEAD `702ecf3` plus the four exact code/test paths
from this record. WSL GCC 11.4 and Clang 14.0.0 directly passed the same
executables with real process exit zero:

- semantic facts/query/query contract/calls: `15/30/6/30`;
- parser/compiler semantic-query diagnostics: `12/64`;
- reference facts/property contracts/canonical consumers: `6/11/21`;
- LSP semantic-query parity/source contracts: `15/70`;
- type inference: `124/124`.

The GCC and Clang LSP interface executables each returned the expected non-zero
status with exactly the same eight pre-existing producer markers; the failure
delta is zero. MSVC, the complete 16-target matrix, and the three stdio smoke
suites were not run for this narrow parser query correction.

## 状态与产出记录

- 完成时间：2026-08-31 09:33 +08:00。
- 状态：Task 1.2 semantic fact source identity 子里程碑已完成；Plan 03
  整体计划继续进行。
- 完成项目：底层fact lookup one-sided source RED、canonical position/node-scope
  exact optional identity、reaching-definition snapshot夹具修正、模块契约更新、
  GCC/Clang `15/30/6/30/12/64/6/11/21/15/70/124` 真实退出门禁、interface
  fixed8 delta 0。
- 后续项目：继续完成receiver/member与binary/native producer、Task 7 consumer
  迁移和Task 8总门禁；MSVC、完整矩阵和stdio由后续阶段统一验收。
