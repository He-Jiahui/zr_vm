# LSP L8 Canonical Project Symbol Type

## Scope

Verify that project-source imported member hover and completion consume only
the exact declaration `SymbolId` and canonical `TypeId` of the imported public
symbol.

## Test Inventory

- `test_lsp_project_canonical_symbol_type_cases.h` creates `main` importing
  `values`, with public `seed: int`.
- The positive path requires completion and public hover to display canonical
  `int`.
- The negative path resolves the exact imported declaration, invalidates only
  its declaration fact while preserving `symbol->typeInfo`, and requires the
  metadata-provider hover plus completion to display `cannot infer exact type`.
- Source-module refresh keeps its canonical spelling assertion: source `float`
  is displayed as `double`.

## Tooling Evidence

| Toolchain | Semantic facts | Interface | Project features | Stdio JSON-RPC / CLI | Peak working set |
|---|---:|---:|---:|---:|---:|
| GCC | 13/13 | 103/103 | 58/58 | exit 0 | 32.78 MiB |
| Clang | 13/13 | 103/103 | 58/58 | exit 0 | 31.46 MiB |
| MSVC | 13/13 | 103/103 | 58/58 | exit 0 | 38.88 MiB |

Expression hover (9/9) and local semantic query (32/32) also passed on every
toolchain. All direct processes returned exit zero and all failure markers were
zero.

## Acceptance Decision

Accepted on 2026-08-10 22:19 +08:00 as an independent L8 project-source
imported-symbol type contract. L8 remains in progress pending its remaining
provider and protocol work.
