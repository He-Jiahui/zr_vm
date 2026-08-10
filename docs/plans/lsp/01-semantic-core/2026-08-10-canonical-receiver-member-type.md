# Canonical Receiver Member Type

## Status

- Completed: 2026-08-10 18:24 +08:00
- Scope: LSP 08 independent project receiver member type fallback-deletion contract
- Baseline: `b452bb5`

## Contract

Project-source receiver member resolution may expose `resolvedTypeText` from a
declaration symbol only when the current analyzer can query that symbol's exact
resolved declaration fact. The declaration `SymbolId` and `TypeId` must match
the symbol snapshot, and display text is produced only by
`ZrParser_CanonicalType_Format`.

`receiver_project_set_type_text_from_symbol` now receives the owning analyzer
and delegates to `ZrLanguageServer_Lsp_FormatSymbolCanonicalDeclarationType`.
It no longer formats `declarationSymbol->typeInfo`. Missing analyzer context,
unresolved declaration identity, invalid IDs, mismatched TypeId, or an
unavailable canonical formatter leaves `resolvedTypeText` absent while
preserving exact declaration navigation.

## Evidence

- The RED fixture resolved `meter.value` to the exact field declaration and
  initially obtained canonical `int` text. It then removed only resolved
  declaration facts for that field SymbolId. The old consumer still emitted
  `int` from `symbol->typeInfo`.
- The GREEN fixture preserves the same declaration URI/range and SymbolId but
  requires `resolvedTypeText == NULL` after canonical identity becomes
  unavailable. No member-name, AST-type, or inferred-type fallback is added.
- Final validation uses the isolated `b452bb5 + LSP overlay` snapshot. The
  parser shared-boundary support regression discovered on `0d1f8f0` is closed
  by the exact two-path `b452bb5` support commit before LSP acceptance.

## Validation Matrix

GCC, Clang, and MSVC each built and directly executed parser semantic facts,
expression-fact hover, local semantic query, LSP interface, and project
feature tests, followed by the complete stdio/CLI workload. Every process
returned real exit zero; semantic facts reported 13/13 on every toolchain.

| Toolchain | Hover p50/p95/p99 ms | Completion p50/p95/p99 ms | Signature p50/p95/p99 ms | Diagnostics p50/p95/p99 ms | 100-file diagnostics p50/p95/p99 ms | Peak |
|---|---|---|---|---|---|---:|
| GCC | 1.42/1.82/2.20 | 1.30/1.95/3.39 | 0.65/0.82/0.88 | 0.54/0.75/1.58 | 2.87/3.16/71.11 | 30.70 MiB |
| Clang | 1.52/1.84/1.87 | 1.06/1.22/1.29 | 0.55/0.66/0.84 | 0.49/0.60/0.69 | 3.01/4.03/77.14 | 29.95 MiB |
| MSVC | 3.76/6.16/13.56 | 4.81/7.56/10.28 | 1.79/3.02/3.02 | 2.64/4.12/17.10 | 37.12/145.40/305.41 | 37.17 MiB |

All three stdio runs remain below the 512 MiB process peak limit. This leaf
does not add a separate performance promotion gate.

## Open Scope

This closes one project receiver declaration-symbol type fallback. It does not
complete L8. Other receiver/prototype completion paths, binary/native
unavailable parity, reference-fixture golden coverage, remaining local
fallback deletion, and the full project/protocol matrix remain open.
