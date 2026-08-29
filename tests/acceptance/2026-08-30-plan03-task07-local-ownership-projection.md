---
plan: docs/plans/lsp/optimize/03-canonical-semantic-query.md
implementation:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.c
tests:
  - tests/language_server/test_lsp_local_semantic_query.c
doc_type: acceptance-record
---

# Plan 03 Task 7.36 Acceptance: Local Ownership Projection

## Acceptance contract

An ownership query at a syntax prefix may use only the exact expression fact node or a canonical
ownership range structurally contained by that expression fact. The consumer does not identify a
fact by source text, identifier name, type text, or diagnostic message.

## Evidence

- GCC, Clang, and MSVC local semantic query builds returned real exit `0`.
- The ownership violation case passed on all three toolchains.
- The local semantic query process retained the same two producer failures on all three toolchains:
  missing short-circuit reachability fact and member-write reference kind/range mismatch.
- No new failure marker or fallback was added; the existing query cache and unrelated local query
  cases remained passing.

## 状态与产出记录

- 完成时间：2026-08-30 07:08 +08:00。
- 状态：focused local ownership projection GREEN；global Plan 03 remains in progress。
- 完成项目：canonical node/range ownership projection and cross-toolchain focused verification。
- 未完成项目：parser reachability/reference producer fixes, remaining Task 7 consumers, and the
  final all-green Plan 03 gate。
