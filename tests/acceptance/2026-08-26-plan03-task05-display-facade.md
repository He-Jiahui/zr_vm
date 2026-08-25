---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_display.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-26-plan03-task05-display-facade.md
tests:
  - tests/parser/test_semantic_display.c
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_canonical_consumers.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 5.1 Canonical Display Facade

## Scope

Accept display only when every formatted entity is supplied by canonical
snapshot identity. No display path may recover a TypeId, SymbolId, accessor, or
signature by source name, AST pairing, or LSP text.

## Required Results

- Type display is exactly the existing canonical TypeId formatter output.
- Symbol display requires a registered SymbolId and either its matching
  declaration signature fact or the registered TypeId fallback.
- Property display requires a matching property record, canonical property
  TypeId, and registered accessor SymbolIds for every advertised accessor.
- Invalid or incomplete identity clears the caller buffer and returns false.

## Evidence

The dedicated MSVC static cache
`.codex/build-lsp-plan03-native-generic-msvc-r2` directly returned process exit
zero for `zr_vm_semantic_display_test` (3 Tests),
`zr_vm_semantic_query_calls_test` (3 Tests), and
`zr_vm_canonical_consumers_test` (19 Tests). The shared worktree remains dirty
from unrelated sessions, so this is neither a clean-baseline nor a
cross-toolchain acceptance claim.

## Acceptance Decision

Accepted for source snapshot display projection. Documentation facts,
argument mapping, conversion exactness, binary/native display parity, and LSP
consumer migration remain unaccepted.

## 状态与产出记录

- 完成时间：2026-08-26 06:00 +08:00。
- 状态：已完成并将随本子项精确提交。
- 完成项目：type/symbol/property canonical display、generic declaration
  signature identity projection、fail-closed behavior 与 source regressions。
- 后续项目：documentation/binary/native facts 及 LSP consumers。
