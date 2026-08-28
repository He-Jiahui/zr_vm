---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_call_hierarchy.c
tests:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_semantic_call_hierarchy_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_type_hierarchy_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 7.9: Method And Lambda Call Hierarchy

## Scope

This slice extends the source-local canonical call-hierarchy consumer from free
functions to resolved receiver methods and variable-bound lambdas. It does not
enter Syntax05-owned property, rename, interface, metadata-provider, or semantic
token paths. Cross-project, binary, and native external call hierarchy remains
open.

## TDD And Root Cause

The receiver-method fixture was already GREEN through exact call edges: two
unrelated classes with the same method spelling retained distinct SymbolIds,
and two calls to the selected method grouped into one item. No production
change was needed for methods.

The lambda RED was narrower. Parser facts already published a stable lambda
SymbolId, callable TypeId, declaration fact, lexical caller scope, and exact
call edge. The LSP call-hierarchy consumer nevertheless discarded the edge
because anonymous functions are not LSP symbol-table entries. Consequently a
callee called only inside a lambda reported no incoming caller.

## Implementation

Hierarchy item construction now treats the parser semantic function record as
the identity source. A matching LSP function/method symbol may supply display
kind and selection range, but is not required for a lambda. The lambda path is
enabled only when the semantic record has a valid SymbolId and TypeId, its AST
identity is exactly `ZR_AST_LAMBDA_EXPRESSION`, and `DeclarationOf` returns the
same resolved identity and exact range.

Returned lambda items carry only copied SymbolId, TypeId, URI, ranges, and
document version. Follow-up incoming/outgoing requests reacquire the current
analyzer and require version, semantic identity, declaration fact, URI, and
both protocol ranges to match. A mutated name is ignored; a mutated range,
missing fact, non-lambda record, or stale version fails closed. No symbol-name,
callable-variable-name, source-text, or AST-pair scan was added.

## Verification

GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 / VS 17.14.38 each return real
exit zero for parser call queries `11/11`, semantic-query parity `9/9`, source
contracts `60/60`, advanced editor features `73/73`, and the combined
type/call-hierarchy stdio smoke. The C and stdio fixtures verify same-name
method separation, lambda incoming identity, returned-item outgoing
re-resolution, display-name mutation, exact call-site ranges, and range
tampering fail-closed behavior.

The full interface runner returns exit one on every toolchain with the same
`109 Pass / 4` pre-existing markers as Task 7.8; marker names and count are
unchanged, so delta is zero and the runner is not counted as GREEN. Workspace,
WSL, and MSVC code/test bytes match `5/5` with ten comparisons and zero
mismatches. `git diff --check` passes.

## 状态与产出记录

- 完成时间：2026-08-28 23:58 +08:00。
- 状态：Task 7.9 method/lambda call hierarchy 子里程碑已完成；Plan 03
  Task 7 继续。
- 完成项目：same-name receiver method identity、lambda caller item projection、
  parser SymbolId/TypeId/declaration-fact identity、returned lambda item
  re-resolution、name/range/stale fail-closed、三工具链 focused/stdio 门禁、
  interface marker delta 0、三处 `5/5` byte audit。
- 未完成项目：rename、cross-project/binary/native external call/type hierarchy
  and implementations，以及其余 consumer migrations。
