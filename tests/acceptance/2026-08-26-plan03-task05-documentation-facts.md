---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_display.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
tests:
  - tests/parser/test_semantic_display.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 5.4 Documentation Facts

## Required Results

- Documentation is keyed by exact `SymbolId`, not by display name or source
  spelling.
- Publication snapshots the text into the semantic context and rejects a
  conflicting duplicate identity.
- A same-name symbol without its own fact returns no documentation.
- Invalid identities and reset contexts fail closed.
- The returned documentation view is borrowed and snapshot-scoped.

## Evidence

`zr_vm_semantic_display_test` directly reports 4 Tests, 0 Failures, 0 Ignored
with process exit zero on fresh MSVC, WSL GCC 11.4, and WSL Clang 14 builds.
The fresh MSVC cache additionally runs semantic query 30/0, symbols 19/0,
calls 10/0, contract 4/0, canonical consumers 19/0, and compiler diagnostics
46/0, all with direct process exit zero.

## Acceptance Decision

Accepted for the exact SymbolId documentation fact and snapshot-lifetime
contract. Source, binary, and native metadata producer parity and LSP consumer
migration remain outside this acceptance record.

## 状态与产出记录

- 完成时间：2026-08-26 15:46 +08:00。
- 状态：已完成并通过 GCC/Clang/MSVC 验收。
- 完成项目：exact-ID documentation publication/query and lifecycle tests。
