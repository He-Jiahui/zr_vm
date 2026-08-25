---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-26-plan03-task04-call-metadata.md
tests:
  - tests/parser/test_semantic_query_calls.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 4.3 Canonical Call Metadata Projection

## Scope

Accept only value fields that are copied directly from the selected canonical
call expression fact. This slice does not infer metadata absent from that fact.

## Required Results

- `CallAt` returns the full call-site range and exact call-target token range.
- `CallAt` returns the fact's argument count, named-argument state, and
  member-call state.
- The query must not read an AST, inspect a spelling, invoke overload
  resolution, or manufacture receiver/mapping/conversion data.
- A failed query clears its output structure, including all copied values.

## Evidence

The dedicated MSVC static cache
`.codex/build-lsp-plan03-native-generic-msvc-r2` directly ran
`zr_vm_semantic_query_calls_test`. The RED executable returned process exit one
with the expected zero `callSiteRange.source`; after the minimal projection,
the direct GREEN executable returned process exit zero and Unity reported
`3 Tests 0 Failures 0 Ignored`. The shared worktree remains dirty from other
sessions, so this is not a clean-baseline or three-toolchain claim.

## Acceptance Decision

Accepted for source call metadata already present in canonical expression facts
only. Mapping, scores, conversions, receiver TypeIds, native/binary metadata,
and LSP presentation remain unaccepted.

## 状态与产出记录

- 完成时间：2026-08-26 06:11 +08:00。
- 状态：已完成并将随本子项精确提交。
- 完成项目：canonical call/target ranges、argument count、named/member flags，
  以及输出清零和 no-recompute acceptance boundary。
- 后续项目：mapping、score、conversion、receiver、external callable 和 LSP消费。
