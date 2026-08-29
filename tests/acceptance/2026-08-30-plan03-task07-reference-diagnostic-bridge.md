---
plan: docs/plans/lsp/optimize/03-canonical-semantic-query.md
implementation:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/language_server/test_lsp_reference_callable_consumer_cases.h
  - tests/language_server/test_lsp_interface.c
doc_type: acceptance-record
---

# Plan 03 Task 7.35 Acceptance: Reference Diagnostic Bridge

## Acceptance contract

The reference-call diagnostic is projected from the parser/compiler persistent semantic fact.
The LSP analyzer does not derive a second diagnostic from message text, callable name, member name,
or AST pairing. A failed exact inference suppresses only the generic duplicate when the structured
diagnostic was actually published.

## Evidence

- GCC fixed snapshot: interface build real exit `0`; reference-call case `Pass`; interface retains
  the pre-existing class-member fixture failure.
- Clang fixed snapshot: same result as GCC; no compiler-specific marker delta.
- MSVC fixed snapshot (`VSCMD_VER=17.14.36`): interface build real exit `0`; reference-call case
  `Pass`; same pre-existing class-member failure.
- All three 16-target builds completed with real exit `0`. Remaining matrix failures are unchanged
  producer/fixture failures in canonical graph, closed-generic/borrow-return analyzer, local query/
  hover, project guard, interface class-member, and feature-matrix coverage.
- All three stdio smoke processes returned real exit `1` at
  `tests/language_server/stdio_smoke.js:2003` because `short_circuit_unreachable` was not published.
  All three CLI `--version` checks returned real exit `0`.

## 状态与产出记录

- 完成时间：2026-08-30 06:56 +08:00。
- 状态：GREEN only for the focused reference diagnostic bridge; global Plan 03 remains in progress.
- 完成项目：structured diagnostic publication/consumption, duplicate inference-error suppression,
  cross-toolchain interface verification, 16-target rebuild/replay, and three stdio/CLI smoke
  replays with truthful failure recording.
- 未完成项目：Syntax05 producer/metadata release, `short_circuit_unreachable` producer, existing
  canonical graph/analyzer/local/project/interface feature markers, semantic-token migration, and
  the final all-green Plan 03 gate.
