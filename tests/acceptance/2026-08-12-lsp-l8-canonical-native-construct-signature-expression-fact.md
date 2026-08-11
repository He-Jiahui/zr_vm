---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
  - tests/language_server/test_lsp_native_construct_receiver_fact_cases.h
  - tests/language_server/test_lsp_interface.c
plan_sources:
  - docs/plans/lsp/01-semantic-core/2026-08-12-canonical-native-construct-signature-expression-fact.md
tests:
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_native_construct_receiver_fact_cases.h
doc_type: acceptance
status: completed
completed_at: 2026-08-12 04:17 +08:00
---

# LSP L8 Native Construct Signature Expression Fact Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-12 04:17 +08:00 | 已完成 | native `STRUCT_INIT_EXPRESSION` signature help exact-fact fail-closed consumer |

The interface fixture requests signature help inside
`init math.Vector3(4.0, 5.0, 6.0)`. Its valid canonical expression fact
returns the descriptor-projected `x: float`, `y: float`, and `z: float`
parameters. The same test then changes the fact to unknown, invalidates its
TypeId, and removes the same-node fact identity; each state returns no
signature help.

The production path recognizes the canonical struct-init call context, then
requires the exact fact before constructor resolution. It never re-infers the
struct-init AST or reconstructs a constructor from target/member text. Legacy
construct expressions retain their existing compatibility path.

GCC, Clang, and MSVC each passed semantic facts 13/13, local query 32/32,
interface 107/107, project features 58/58, expression/local hover 9/9 and
12/12, and the stdio/CLI CTest pair 2/2 with real exit zero. This accepts only
the eleventh L8 consumer contract, not L8 as a whole.
