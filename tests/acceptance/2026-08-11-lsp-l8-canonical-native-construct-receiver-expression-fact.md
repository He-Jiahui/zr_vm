---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_canonical_symbol_display.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
  - tests/language_server/test_lsp_native_construct_receiver_fact_cases.h
plan_sources:
  - docs/plans/lsp/01-semantic-core/2026-08-11-canonical-native-construct-receiver-expression-fact.md
tests:
  - tests/language_server/test_lsp_interface.c
doc_type: acceptance
status: completed
completed_at: 2026-08-11 15:09 +08:00
---

# LSP L8 Native Construct Receiver Expression Fact Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-11 15:09 +08:00 | 已完成 | native construct receiver exact expression fact fail-closed consumer |

The native descriptor resolver receives its receiver type only from the exact
semantic expression fact attached to the receiver-prefix AST node. Missing,
unknown, and invalid facts produce no native member target. The LSP does not
re-infer the construct expression or reconstruct the receiver from member text.

The interface fixture validates the positive `init math.Vector3(...).y` hover,
the separate exactness and TypeId negative gates, and a construct-derived
member-chain missing-fact boundary. GCC, Clang, and MSVC each passed semantic
facts 13/13, local hover 12/12, local query 32/32, interface 105/105, project
58/58, and stdio/CLI 2/2 with real exit zero.

The expanded 33-test CTest collection is not acceptance evidence: its
`language_server_stdio_inline_value_semantic_smoke` failure reproduces against
the prior validated binary and remains outside this L8 consumer contract.
