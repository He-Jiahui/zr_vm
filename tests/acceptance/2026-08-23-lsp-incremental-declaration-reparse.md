# LSP Incremental Declaration Reparse Acceptance

## Scope

- Plan 02 Task 5 declaration-scoped incremental parsing and conservative
  fallback.
- Parser support for exact token-boundary seeking and a one-statement top-level
  parse entry point.
- Project semantic-cache invalidation and canonical public-contract dependency
  classification after declaration replacement.

## Contract

- Full parsing remains the default and explicit fallback mode.
- Token-equivalent reuse remains gated by equal source length, token values, and
  token coordinates.
- Declaration reparse accepts one exact top-level statement only when the
  replacement preserves declaration kind and full source range. It retains the
  script root and untouched sibling node identities.
- A declaration replacement clears the owning semantic cache before analysis.
  Importers are preserved only after the canonical parser public-contract hash
  matches the prior record.

## Test Inventory

- `test_incremental_parser.c`: declaration replacement retains the root and
  untouched sibling; a length-changing edit explicitly falls back to full
  parsing.
- `test_lsp_incremental_equivalence.c`: incremental and clean contexts produce
  equal AST shape, diagnostics, document symbols, semantic tokens, and
  TypeId/SymbolId relationships.
- `test_lsp_project_features.c`: an explicitly typed exported-function body
  change preserves importer analysis; an inferred body or public signature
  change reanalyzes it.
- `test_lsp_semantic_snapshot.c`: dependency fences remain valid across the
  new invalidation path.

## Tooling Evidence

GCC Debug shared build directory: `.codex/build-lsp-snapshot-gcc`.

The final direct run rebuilt all four targets and required both a zero process
exit and no `Fail -` or `FAIL:` marker in each captured result. All conditions
were met on 2026-08-23.

## Acceptance Decision

Accepted for Plan 02 Task 5 on 2026-08-23 13:27 +08:00. This is a focused GCC
acceptance only; Task 7 owns the final GCC/Clang/MSVC matrix.

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
| --- | --- | --- | --- |
| 2026-08-23 13:27 +08:00 | 已完成 | 声明级增量重解析、保守回退、项目 cache invalidation 和 public-contract dependency gate。 | GCC direct suite：4 targets exit 0，差分与 snapshot tests 均为 `0 failure(s)`。 |
