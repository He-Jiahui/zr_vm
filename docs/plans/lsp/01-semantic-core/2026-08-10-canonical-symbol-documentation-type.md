# Canonical Symbol Documentation Type

## Status

- Completed: 2026-08-10 15:23 +08:00
- Scope: LSP 08 independent source symbol documentation fallback-deletion contract
- Baseline: `2b6e367`

## Contract

Source symbol hover and completion documentation may display a type only when
the current analyzer exposes an exact resolved declaration for the symbol's
`SymbolId`. `ZrLanguageServer_Lsp_FormatSymbolCanonicalDeclarationType`
requires that declaration's canonical `TypeId` to match the symbol snapshot
and formats it through `ZrParser_CanonicalType_Format`.

`ZrLanguageServer_Lsp_BuildSymbolMarkdownDocumentation` no longer calls
`ZrParser_TypeNameString_Get` on `symbol->typeInfo`. Every interface,
semantic-query, completion, and metadata-provider caller passes the analyzer
whose semantic context owns the declaration identity. Missing analyzer,
unresolved declaration, invalid identity, or mismatched `TypeId` omits the
type section instead of reconstructing inferred text.

The canonical formatter is implemented in the cohesive private
`lsp_canonical_symbol_display` module. Inlay hints reuse the same identity
helper, so both display surfaces share one SymbolId/TypeId agreement rule.

## Evidence

- The RED focused fixture first retained a precise inferred `int` symbol,
  invalidated only its resolved declaration fact, and observed completion
  documentation still exposing `Type: int` through `symbol->typeInfo`.
- The final fixture proves both directions on the same source snapshot:
  resolved identity emits canonical `Type: int`; invalidating only that fact
  preserves the completion item but removes the type section.
- The existing inlay fail-closed fixture remains green through the extracted
  shared helper.
- Interface, local semantic-query, and project metadata regressions prove all
  updated builder callers remain valid. The complete version-1 stdio workload
  preserves positive hover/completion behavior.
- The new source is discovered by the existing module source glob; the dirty
  language-server CMake file was not modified by this milestone.

## Validation Matrix

GCC, Clang, and MSVC each built and directly executed the focused expression
hover/documentation suite, inlay semantic-fact suite, LSP interface suite,
local semantic-query suite, project feature suite, and complete stdio smoke on
the same production source. Every process returned real exit zero. The
focused suite reported 9/9 and the inlay suite reported 10/10 on every
toolchain.

| Toolchain | Hover p50/p95/p99 ms | Completion p50/p95/p99 ms | Signature p50/p95/p99 ms | Diagnostics p50/p95/p99 ms | 100-file diagnostics p50/p95/p99 ms | Peak |
|---|---|---|---|---|---|---:|
| GCC | 1.10/1.83/2.13 | 0.77/0.86/0.94 | 0.48/0.60/0.60 | 0.50/0.70/0.76 | 3.30/5.50/75.42 | 30.23 MiB |
| Clang | 2.00/3.44/4.95 | 1.25/1.74/1.78 | 0.70/1.07/3.86 | 0.67/3.06/7.32 | 4.14/6.24/112.39 | 30.54 MiB |
| MSVC | 1.94/2.53/2.70 | 2.51/3.06/3.11 | 0.99/1.31/1.43 | 1.43/1.94/1.94 | 14.78/22.39/119.17 | 36.39 MiB |

These metrics come from the existing broad stdio workload. All three runs are
below the 512 MiB process peak limit; this leaf does not add a performance
promotion gate.

## Open Scope

This closes one source-local L8 symbol documentation fallback. It does not
complete L8. Other receiver/prototype/completion inference fallbacks in the
language-server implementation, explicit binary/native unavailable parity,
reference-fixture golden coverage, and the full project/protocol matrix remain
open.
