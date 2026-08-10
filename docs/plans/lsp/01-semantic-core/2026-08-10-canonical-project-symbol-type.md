# Canonical Project Symbol Type

## Status

- Completed: 2026-08-10 22:19 +08:00
- Scope: LSP 08 independent project-source imported-symbol type contract
- Baseline: `3333d4a`

## Contract

Project-source imported member hover and completion consume the exact
declaration `SymbolId` and matching canonical `TypeId`. The display text is
authorized only by `ZrLanguageServer_Lsp_FormatSymbolCanonicalDeclarationType`,
which queries the current declaration fact and formats it with
`ZrParser_CanonicalType_Format`.

`lsp_metadata_provider` no longer formats `symbol->typeInfo` for source-module
member details. Project-source imported member hover also bypasses the generic
symbol hover builder, because that builder could independently render the
symbol table inferred type after the provider had already produced an
unavailable canonical result. The structured resolved metadata member is now
the display authority for both hover and completion.

Missing analyzer state, unresolved declaration facts, invalid IDs, or a TypeId
mismatch produce `cannot infer exact type`. Declaration navigation remains
available when its own identity is valid. No member-name, declaration-AST, or
inferred-type fallback is added.

## Evidence

- The RED fixture imports project source module `values` and resolves public
  `seed: int`. It then invalidates only the exact declaration fact selected by
  the resolved imported-member query, while preserving the legacy
  `symbol->typeInfo` value.
- The GREEN fixture requires the metadata-provider hover and project completion
  to show `cannot infer exact type`, never `int`. Its positive half still
  requires public hover and completion to display canonical `int`.
- The source-refresh fixture now expects canonical normalization of source
  `float` to `double`, rather than preserving an inferred-type spelling.

## Validation Matrix

Every listed test process returned real exit zero and its Unity or PASS marker
reported zero failures. The complete stdio/CLI workload ran against the same
isolated snapshot.

| Toolchain | Semantic facts | Expression/local/interface/project | Hover p50/p95/p99 ms | Completion p50/p95/p99 ms | Signature p50/p95/p99 ms | Diagnostics p50/p95/p99 ms | 100-file p50/p95/p99 ms | Peak |
|---|---:|---:|---|---|---|---|---|---:|
| GCC | 13/13 | 9/9, 32/32, 103/103, 58/58 | 1.69/2.18/2.23 | 1.29/2.41/2.47 | 0.63/0.85/0.88 | 0.52/0.75/0.96 | 3.40/4.30/140.13 | 32.78 MiB |
| Clang | 13/13 | 9/9, 32/32, 103/103, 58/58 | 1.86/6.17/6.80 | 1.19/2.40/2.72 | 0.60/3.65/4.47 | 0.58/3.90/4.51 | 3.93/24.32/311.31 | 31.46 MiB |
| MSVC | 13/13 | 9/9, 32/32, 103/103, 58/58 | 3.73/4.82/16.28 | 4.22/10.43/16.35 | 1.65/2.94/33.62 | 2.51/3.82/4.13 | 31.93/59.67/318.26 | 38.88 MiB |

All three stdio runs are below the 512 MiB process peak limit.

## Open Scope

This closes one L8 project-source imported-symbol consumer. It does not
complete L8: remaining local fallback deletion, binary/native provider parity,
reference-fixture golden coverage, and the full project/protocol matrix remain
open.
