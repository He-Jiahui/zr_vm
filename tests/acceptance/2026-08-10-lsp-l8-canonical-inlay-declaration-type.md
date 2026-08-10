# LSP L8 Canonical Inlay Declaration Type

## Scope

Verify that source inlay type hints are formatted only from an exact resolved
declaration `SymbolId` and canonical `TypeId`. Missing declaration identity
must suppress the hint instead of exposing inferred-type or AST text.

## Baseline

The previous inlay consumer called `ZrParser_TypeNameString_Get` on the local
symbol's inferred type object. That object could remain precise after the
canonical declaration fact became unavailable, allowing the LSP to present a
type unsupported by the semantic query snapshot.

Position-only declaration lookup is also insufficient: overlapping callable
and inferred return declarations can carry different `SymbolId` and `TypeId`
values at the same source range.

## Test Inventory

- `tests/parser/test_semantic_query.c` freezes exact declaration lookup by
  `SymbolId`, including same-range collisions and unresolved fail-closed
  behavior.
- `tests/language_server/test_lsp_inlay_semantic_facts.c` invalidates only the
  target declaration fact and forbids the previous `: int` fallback.
- Existing LSP interface and stdio fixtures preserve positive canonical hints
  for inferred locals, inferred callable returns, and closed generic types.

## Tooling Evidence

| Toolchain | Parser query | Focused inlay facts | Interface | Stdio JSON-RPC | Peak working set |
|---|---:|---:|---:|---:|---:|
| GCC | 29/29, exit 0 | 10/10, exit 0 | exit 0 | exit 0 | 30.30 MiB |
| Clang | 29/29, exit 0 | 10/10, exit 0 | exit 0 | exit 0 | 30.09 MiB |
| MSVC | 29/29, exit 0 | 10/10, exit 0 | exit 0 | exit 0 | 36.43 MiB |

| Toolchain | Hover p50/p95/p99 ms | Completion p50/p95/p99 ms | Signature p50/p95/p99 ms | Diagnostics p50/p95/p99 ms | 100-file p50/p95/p99 ms |
|---|---|---|---|---|---|
| GCC | 1.52/1.71/1.78 | 0.96/1.18/1.42 | 0.46/0.54/0.55 | 0.44/0.52/0.58 | 2.69/3.18/58.61 |
| Clang | 2.85/9.52/9.91 | 1.51/3.70/6.25 | 0.70/1.05/5.21 | 0.80/5.73/11.20 | 3.88/38.95/514.26 |
| MSVC | 3.55/4.54/4.75 | 4.27/5.98/6.71 | 1.54/2.02/2.49 | 2.71/4.19/4.49 | 27.82/47.83/230.60 |

## Results

`ZrParser_SemanticQuery_DeclarationOf` exposes exact resolved declaration
identity. The inlay consumer verifies SymbolId and TypeId agreement and uses
the canonical formatter only. Unavailable or mismatched identity emits no
hint; no member-name, declaration-text, or inferred-type display fallback
remains in this path.

## Acceptance Decision

Accepted on 2026-08-10 14:00 +08:00 as an independent L8 source inlay type
contract. This does not accept L8 as a whole or substitute this source fixture
for the remaining provider, project, golden, and full protocol gates.
