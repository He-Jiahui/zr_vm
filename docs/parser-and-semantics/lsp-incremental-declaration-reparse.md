---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/incremental_parser.h
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/src/zr_vm_language_server/incremental/incremental_declaration_index.c
  - zr_vm_language_server/src/zr_vm_language_server/incremental/incremental_syntax_reparse.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_parser/src/zr_vm_parser/parser.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/src/zr_vm_language_server/incremental/incremental_declaration_index.c
  - zr_vm_language_server/src/zr_vm_language_server/incremental/incremental_syntax_reparse.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_parser/src/zr_vm_parser/parser.c
plan_sources:
  - docs/plans/lsp/optimize/02-snapshots-workspaces-and-diagnostics.md
tests:
  - tests/language_server/test_incremental_parser.c
  - tests/language_server/test_lsp_incremental_equivalence.c
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_semantic_snapshot.c
doc_type: module-guide
---

# LSP Incremental Declaration Reparse

## Scope

The language server has three explicit parse modes. `full_reparse` is the
conservative default. `token_equivalent` retains the existing fast path only
when the edit has equal length and identical token values and coordinates.
`declaration_reparse` replaces one proven-safe top-level declaration while
retaining the script root and every untouched sibling node.

## Safe Reparse Boundary

The declaration index accepts one nonempty old change range only when exactly
one top-level statement contains it. The reparse path additionally requires
unchanged source length, an identical changed-range start, equal replacement
length, and unchanged CR/LF layout inside that range. It seeks the exact lexer
token boundary in the new source and parses one top-level statement.

The candidate is committed only when its AST kind and full source range match
the previous declaration. The root and untouched sibling identities remain
unchanged; the old selected declaration is released only after the replacement
has passed every guard. The current implementation deliberately falls back to
`full_reparse` for block-local, length-changing, boundary-ambiguous, malformed,
or otherwise unproven edits. That fallback is part of the contract, not a
silent optimization failure.

## Semantic Invalidation

A declaration replacement changes AST-node identity even when a structural AST
hash remains equal. Project reanalysis therefore clears the owning semantic
cache when `lastParseMode` is `declaration_reparse`. It then rebuilds only the
changed document's analysis. The project layer compares the previous and new
canonical parser public-contract hash: an equal contract preserves reverse
dependencies, while a changed or unavailable contract invalidates importers.
No member-name, source-text, or LSP-local contract reconstruction is used.

## Differential Gate

`test_lsp_incremental_equivalence.c` applies an equal-length declaration edit
to one LSP context and parses the resulting text from scratch in another. It
compares AST shape, literal values, source ranges, diagnostics, document
symbols, semantic tokens, and TypeId/SymbolId relationships. The project
refresh regression additionally proves that a public function body edit keeps
the importer analysis when the canonical public contract is unchanged.

The acceptance differential additionally performs 10,000 deterministic random
equal-length replacements over ASCII, three-byte UTF-8 CJK, and four-byte
astral-plane code points. Every iteration round-trips UTF-16 positions, checks
the incremental snapshot against a clean full parse, and compares final LSP
JSON. The test reports `clock()` tick p50/p95/p99 plus the explicit full-reparse
fallback ratio; those telemetry values are toolchain-local evidence, not a
cross-platform wall-clock threshold.

## Status

This module documents Plan 02 Task 5 only. Block-level reparse, diagnostic
aggregation, and the cross-toolchain acceptance matrix remain separate work.

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
| --- | --- | --- | --- |
| 2026-08-23 13:27 +08:00 | 已完成 | 发布 declaration-scoped incremental reparse、显式 full fallback 和 public-contract dependency gate。 | GCC focused direct tests exit 0：incremental parser、incremental equivalence、semantic snapshot、project features。 |
