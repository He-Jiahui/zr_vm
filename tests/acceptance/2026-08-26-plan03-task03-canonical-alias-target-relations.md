---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
implementation_files:
  - tests/parser/test_semantic_query_relations.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_relations.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 3.10 Canonical Alias Target Relations

## Scope

The source compiler publishes alias-target relations from existing semantic
scope facts. A type-value alias receives one canonical target edge. Imported
aliases retain both that type-identity edge and their separate external-origin
edge.

## Baseline And Diagnosis

The initial MSVC RED ran 16 relation tests with one failure: the new
type-value-alias query returned no relation. Source scope facts already marked
the declaration as an alias and registered its SymbolId, canonical TypeId, and
declaration range. The missing layer was therefore a relation producer, not an
LSP name lookup or AST reconstruction path.

## Test Inventory

- A compiled type-value alias publishes exactly one `ALIAS_TARGET` edge.
- The edge carries the exact alias SymbolId, canonical source/target TypeId,
  and source declaration range while leaving target SymbolId/range unavailable.
- Re-running the producer does not duplicate the edge.
- Destructured and direct imports expose a sorted alias-target edge plus an
  independent external-origin edge with the exact import URI.
- Existing hierarchy, override, constructor, definition, property, and sorted
  relation query cases remain green.

## Tooling Evidence

MSVC used the isolated static cache after importing the Visual Studio developer
environment and invoked every executable directly. WSL GCC used
`.codex/build-lsp-plan03-override-gcc-wsl`; WSL Clang 14 used
`.codex/build-lsp-plan03-scope-clang14-wsl` with `/usr/bin/clang`. Accepted
results include both the Unity summary and direct process exit code.

## Results

- MSVC direct: relation 16/16, semantic query 30/30, symbols 19/19, canonical
  consumers 19/19, compiler semantic query diagnostics 46/46, and compiler
  integration 127/127; every process exited 0.
- WSL GCC direct: relation 16/16; process exited 0.
- WSL Clang 14 direct: relation 16/16; process exited 0.

## Acceptance Decision

Accepted for source alias-target publication. The producer fails closed when
the alias symbol, canonical TypeId, or source range is unavailable. Exact target
declaration identity, alias-chain provenance, binary/native producers, project
generation, and LSP consumption remain separate follow-up work.

## 状态与产出记录

- 完成时间：2026-08-26 11:34 +08:00。
- 状态：已完成。
- 完成项目：canonical alias-target producer、type-value/import alias relation
  coverage、idempotence、fail-closed target boundary 与三工具链定向验收。
- 验证：MSVC relation 16/16 与 expanded 30/30、19/19、19/19、46/46、
  127/127 均 exit 0；WSL GCC relation 16/16、WSL Clang 14 relation 16/16，
  均 exit 0。
