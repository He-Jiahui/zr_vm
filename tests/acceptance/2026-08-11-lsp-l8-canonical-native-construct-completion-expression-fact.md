---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_native_construct_receiver_fact_cases.h
plan_sources:
  - docs/plans/lsp/01-semantic-core/2026-08-11-canonical-native-construct-completion-expression-fact.md
tests:
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_native_construct_receiver_fact_cases.h
doc_type: acceptance
status: completed
completed_at: 2026-08-11 19:57 +08:00
---

# LSP L8 Native Construct Completion Expression Fact Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-11 19:57 +08:00 | 已完成 | incomplete native construct receiver completion exact-fact fail-closed consumer |

The interface fixture establishes a valid native construct member snapshot, then
issues an incomplete version with the cursor after the member dot. With the
receiver expression fact intact, completion returns `x`, `y`, and `z`. After
the same fact is marked unknown, completion succeeds with no items.

The dispatcher performs this gate before semantic-query import resolution and
before the last-good scoped analyzer. Therefore no branch can re-infer the
receiver from the fallback AST, member spelling, or text. GCC, Clang, and MSVC
each passed semantic facts 13/13, local query 32/32, interface 106/106,
project 58/58, expression/local hover, and the stdio/CLI CTest pair 2/2 with
real exit zero.

This accepts only the native construct completion consumer. It does not accept
the full L8 convergence milestone or the unrelated expanded stdio collection.
