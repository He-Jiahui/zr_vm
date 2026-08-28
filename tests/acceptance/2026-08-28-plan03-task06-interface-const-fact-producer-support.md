---
related_code:
  - zr_vm_parser/include/zr_vm_parser/interface_contract.h
  - zr_vm_parser/src/zr_vm_parser/semantic/interface_contract.c
tests:
  - tests/parser/test_compiler_interface_const_query_producer.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-28-plan03-task06-interface-const-fact-producer-support.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.35 Interface Const Fact Producer Support

## Required Results

- Publish every canonical interface const-field violation from parser-owned
  prototype facts.
- Preserve descriptor `2014`, exact primary/related ranges, and explicit
  user-decision disposition.
- Leave the compiler's current error state unchanged.
- Do not enter the externally owned LSP symbols producer or add fallback
  semantics.

## TDD Evidence

The target first linked RED for the missing publisher. A compiler-failure
fixture then exposed that invalid class compilation does not retain a complete
prototype; the test was narrowed to the public contract's real lower-layer
input. Its canonical interface/class prototype pair yields one mutable-field
violation and one missing-field violation from a single publication call.

## Final Evidence

GCC, Clang, and MSVC each pass producer `1/1`, disposition `11/11`, and compiler
query diagnostics `64/64` with real process exits. Both produced facts retain
descriptor `2014`, one related information row, no typed fix, and
`REQUIRES_USER_DECISION`; the pre-existing compiler error pointer and flags are
unchanged. Workspace, WSL, and MSVC bytes match for all four code/test paths.

## Acceptance Decision

Accepted as parser producer support only. The LSP symbols consumer is not yet
migrated and Plan 03 Task 6 remains active.

## 状态与产出记录

- 完成时间：2026-08-28 18:25 +08:00。
- 状态：本 support 子项已验收；LSP consumer migration 未完成。
- 完成项目：valid link RED、canonical prototype input correction、双 violation
  persistent facts、compiler state preservation、三工具链 `1/11/64`、三处
  `4/4` byte audit。
