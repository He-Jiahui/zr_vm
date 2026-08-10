# Canonical Inlay Declaration Type

## Status

- Completed: 2026-08-10 14:00 +08:00
- Scope: LSP 08 independent source inlay type fallback-deletion contract
- Baseline: `1958469`

## Contract

Source inlay type hints require an exact resolved declaration identity. The
consumer queries the declaration by `SymbolId` through
`ZrParser_SemanticQuery_DeclarationOf`, verifies that its canonical `TypeId`
matches the symbol snapshot, and formats only that identity through
`ZrParser_CanonicalType_Format`.

The symbol's inferred type object remains only a precision and availability
gate. It is not a display source. If the declaration fact is missing,
unresolved, belongs to another symbol, or carries a different `TypeId`, the
consumer emits no type hint. It does not use
`ZrParser_TypeNameString_Get`, declaration AST text, or a callable/member name
fallback.

The exact identity query is necessary because declarations with overlapping
source ranges can represent different semantic symbols. Position-only lookup
selected a callable declaration when the requested hint belonged to its
inferred return symbol; `DeclarationOf(SymbolId)` preserves the intended
identity without range or name heuristics.

## Evidence

- `tests/parser/test_semantic_query.c` creates two resolved declarations at the
  same range and proves that `DeclarationOf` selects the requested `SymbolId`.
  Marking that exact declaration unresolved makes the query unavailable.
- `tests/language_server/test_lsp_inlay_semantic_facts.c` invalidates only the
  declaration fact for an otherwise precise inferred local and requires the
  inlay consumer to emit no `: int` fallback.
- Existing interface and version-1 stdio fixtures retain positive inferred
  local and callable return hints, including closed `Box<int>` and
  `Matrix<int, 4>` displays.
- The final source contains no `TypeNameString_Get` call in
  `lsp_inlay_hints.c`.

## Validation Matrix

GCC, Clang, and MSVC each built and directly executed the parser semantic-query
suite, focused inlay semantic-fact suite, LSP interface suite, and complete
stdio smoke on the same final source. Every process returned real exit zero.
The parser suite reported 29/29 and the focused suite reported 10/10 on every
toolchain.

| Toolchain | Hover p50/p95/p99 ms | Completion p50/p95/p99 ms | Signature p50/p95/p99 ms | Diagnostics p50/p95/p99 ms | 100-file diagnostics p50/p95/p99 ms | Peak |
|---|---|---|---|---|---|---:|
| GCC | 1.52/1.71/1.78 | 0.96/1.18/1.42 | 0.46/0.54/0.55 | 0.44/0.52/0.58 | 2.69/3.18/58.61 | 30.30 MiB |
| Clang | 2.85/9.52/9.91 | 1.51/3.70/6.25 | 0.70/1.05/5.21 | 0.80/5.73/11.20 | 3.88/38.95/514.26 | 30.09 MiB |
| MSVC | 3.55/4.54/4.75 | 4.27/5.98/6.71 | 1.54/2.02/2.49 | 2.71/4.19/4.49 | 27.82/47.83/230.60 | 36.43 MiB |

These metrics come from the existing broad stdio workload. All three runs are
below the 512 MiB process peak limit; this leaf does not add a performance
promotion gate.

## Open Scope

This closes one source-local L8 inlay display fallback. It does not complete
L8. Binary/native/project provider parity, other local type and ownership
fallbacks, reference-fixture golden coverage, and the full project/protocol
matrix remain open.
