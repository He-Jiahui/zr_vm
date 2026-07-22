# Syntax 04 M7 concurrent major + artifact/AOT/LSP acceptance

## Status

Completed at 2026-07-23 00:27 +08:00.

## Acceptance contract

- A major cycle uses a domain-local initial snapshot, bounded concurrent mark slices, short
  remark/sweep, and optional budgeted compaction.
- Mutator writes during concurrent mark remain live through the production barrier; another
  domain continues to make progress during the target domain's remark pause.
- GC pause/work and transfer lifecycle/object/byte counters are attributable to exact domain
  identity and generation.
- Source, binary artifact, VM and AOT consume one canonical domain-transfer contract.
- New source compilation emits view/into-GC/return-to-GC opcodes and does not emit legacy
  borrow/loan/detach opcodes.
- Ownership hover and diagnostics are projections of canonical parser facts, not text/name
  reconstruction in an LSP consumer.

## Evidence

- Focused suites and the exact commands/results are summarized in
  `docs/plans/syntax/04-resource-ownership-drop-gc-bridge/m7-concurrent-major-artifact-aot-lsp.md`.
- The final frozen overlay passed 26/26 GCC processes, 26/26 Clang processes and the common
  25/25 MSVC processes with real exit code zero. Each toolchain emitted 22 Unity summaries with
  474 tests and zero failures; GCC/Clang also linked and passed the extra LSP interface process.
- ASan/UBSan passed the five M7-focused processes with 121 tests and zero sanitizer markers.
  ThreadSanitizer passed concurrent-major and transfer-race processes with 15 tests and no race
  report. Both sanitizer runs used `setarch x86_64 -R` to avoid the independently reproduced WSL
  sanitizer shadow-mapping startup instability.
- An expanded, non-gating owner-borrow sanitizer run found an out-of-scope null function cache
  issue in unchanged `execution_member_access.c`; it is not represented as passing evidence here.
- WSL and MSVC frozen snapshots matched all 61 M7 exact paths byte-for-byte before final audit;
  no LSP, external dirty Syntax plan, build, log or generated root artifact is part of this
  acceptance set.
