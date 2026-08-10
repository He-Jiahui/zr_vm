# LSP L8 Canonical Symbol Documentation Type

## Scope

Verify that source symbol hover and completion documentation display types
only from exact resolved declaration identity and canonical TypeId formatting.
An inferred type object must not independently authorize visible type text.

## Baseline

`ZrLanguageServer_Lsp_BuildSymbolMarkdownDocumentation` previously formatted
`symbol->typeInfo` directly. A symbol could therefore keep precise inferred
state while its declaration fact was unavailable and still expose `Type: int`
as though the semantic query snapshot supported that conclusion.

## Test Inventory

- `tests/language_server/test_lsp_expression_fact_hover.c` requests completion
  inside a valid function and first requires canonical `Type: int` while the
  declaration fact is resolved.
- The same analyzer then marks only that declaration fact unresolved, repeats
  completion, and requires the item to remain while its type section is absent.
- Existing inlay, interface, local semantic-query, project feature, and stdio
  suites retain positive canonical display and provider regression coverage.

## Tooling Evidence

| Toolchain | Focused documentation | Inlay facts | Interface/local query/project | Stdio JSON-RPC | Peak working set |
|---|---:|---:|---:|---:|---:|
| GCC | 9/9, exit 0 | 10/10, exit 0 | exit 0 | exit 0 | 30.23 MiB |
| Clang | 9/9, exit 0 | 10/10, exit 0 | exit 0 | exit 0 | 30.54 MiB |
| MSVC | 9/9, exit 0 | 10/10, exit 0 | exit 0 | exit 0 | 36.39 MiB |

| Toolchain | Hover p50/p95/p99 ms | Completion p50/p95/p99 ms | Signature p50/p95/p99 ms | Diagnostics p50/p95/p99 ms | 100-file p50/p95/p99 ms |
|---|---|---|---|---|---|
| GCC | 1.10/1.83/2.13 | 0.77/0.86/0.94 | 0.48/0.60/0.60 | 0.50/0.70/0.76 | 3.30/5.50/75.42 |
| Clang | 2.00/3.44/4.95 | 1.25/1.74/1.78 | 0.70/1.07/3.86 | 0.67/3.06/7.32 | 4.14/6.24/112.39 |
| MSVC | 1.94/2.53/2.70 | 2.51/3.06/3.11 | 0.99/1.31/1.43 | 1.43/1.94/1.94 | 14.78/22.39/119.17 |

## Results

Symbol documentation receives its owning analyzer, joins the symbol to an
exact declaration by SymbolId, verifies TypeId agreement, and uses the
canonical formatter. Missing or inconsistent identity omits the type section;
there is no inferred-type, declaration-text, or member-name display fallback
in this path.

## Acceptance Decision

Accepted on 2026-08-10 15:23 +08:00 as an independent L8 source symbol
documentation contract. This does not accept L8 as a whole or substitute the
focused source fixture for remaining provider, golden, and full protocol
gates.
