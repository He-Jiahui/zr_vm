---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-26-plan03-task04-overload-candidates.md
tests:
  - tests/parser/test_semantic_query_calls.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 4.2 Resolved Overload Candidate Membership

## Scope

Accept only overload declaration membership attached to an already-resolved
CallAt target. No candidate may be discovered by source name, signature text,
or a second overload resolver.

## Required Results

- A resolved call projects the registered function members of its target's
  stable overload-set identity.
- Exactly the resolved CallAt SymbolId is selected.
- Candidate callable TypeId is the declaration contract; CallAt continues to
  own any selected closed generic callable TypeId.
- An unresolved call returns no candidate result and cannot select a same-name
  declaration.

## Evidence

The dedicated MSVC static cache
`.codex/build-lsp-plan03-native-generic-msvc-r2` directly returned process exit
zero for `zr_vm_semantic_query_calls_test`; Unity reported `3 Tests 0 Failures
0 Ignored`. The shared worktree is dirty from unrelated sessions, so this is
not a clean-baseline or three-toolchain acceptance claim.

## Acceptance Decision

Accepted for source resolved overload declaration membership only. Candidate
scoring, parameter mapping, conversions, receiver/binary/native facts, and LSP
overload presentation remain unaccepted.

## 状态与产出记录

- 完成时间：2026-08-26 05:42 +08:00。
- 状态：已完成并将随本子项精确提交。
- 完成项目：stable overload membership、唯一 selected identity、declaration与
  closed-call TypeId分离、source overload regression。
- 后续项目：mapping/score/conversion/receiver/external callable 与 LSP消费。
