---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
implementation_files:
  - tests/parser/test_semantic_query_relations.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_relations.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 3.9 Canonical Relation Module Identities

## Scope

The relation query projects exact source and target module identities from the
canonical TypeIds already carried by each relation fact. The fixture covers two
qualified nominal endpoints, an unqualified source endpoint, and a generic
target whose module identity must come from its canonical nominal definition.

## Baseline And Diagnosis

The initial MSVC RED failed to compile only because
`SZrParserSemanticRelationQuery` did not expose `sourceModuleIdentity` and
`targetModuleIdentity`. Relation facts already carried canonical endpoint
TypeIds, so no producer, AST, name, URI, or display-text lookup was required.
The GREEN implementation resolves nominal nodes directly and follows only the
definition edge of canonical generic instances with a bounded traversal.

## Test Inventory

- Qualified source and target nominal TypeIds project exact module identities.
- A generic-instance target projects its nominal definition module identity.
- An unqualified source remains unavailable instead of using a URI or name.
- Existing hierarchy, override, constructor, import, and sorted relation query
  cases remain green.

## Tooling Evidence

MSVC used the isolated static cache after importing the Visual Studio developer
environment and invoked each executable directly. WSL GCC used
`.codex/build-lsp-plan03-override-gcc-wsl`; WSL Clang 14 used
`.codex/build-lsp-plan03-scope-clang14-wsl` with `/usr/bin/clang`. All accepted
results include the direct test process exit code.

## Results

- MSVC direct: relation 15/15, semantic query 30/30, semantic query calls
  10/10, and canonical consumers 19/19; every process exited 0.
- WSL GCC direct: relation 15/15; process exited 0.
- WSL Clang 14 direct: relation 15/15; process exited 0.
- The separately probed MSVC canonical type graph remains at its pre-existing
  tuple syntax baseline of 19 tests with 1 failure. It is not counted as a
  passing result and is outside this relation-query change.

## Acceptance Decision

Accepted for the narrow relation endpoint projection. Module identity remains
unavailable whenever canonical type evidence cannot establish it. LSP hierarchy
snapshot data, binary/native producers, and cross-snapshot copying remain
separate follow-up work.

## 状态与产出记录

- 完成时间：2026-08-26 10:49 +08:00。
- 状态：已完成。
- 完成项目：canonical relation endpoint module identity public projection、
  generic-definition traversal、unqualified/fail-closed boundary 与跨工具链定向
  验收。
- 验证：MSVC 15/15、30/30、10/10、19/19 均 exit 0；WSL GCC relation
  15/15、WSL Clang 14 relation 15/15，均 exit 0。
