# LSP L8 Canonical Property Signature Type

## Scope

Verify that source property hover and completion documentation derive the
visible property signature from an exact canonical PropertyDef and TypeId.
Neither a precise inferred type nor a copied symbol contract may independently
authorize visible property type text.

## Baseline

`ZrLanguageServer_LspPropertyContract_FormatSignature` previously formatted
`symbol->typeInfo` and paired it with `symbol->propertyContract`. A property
could therefore keep displaying `property value: int` after the analyzer's
canonical PropertyDef fact became unavailable.

## Test Inventory

- `tests/language_server/test_lsp_property_contract_cases.h` first requires a
  source property to expose its canonical `property value: int` signature.
- The same analyzer then removes only its PropertyDef fact while retaining the
  symbol, inferred type, declaration fact, and copied contract. Ordinary
  symbol documentation remains available, but the property signature must be
  absent.
- Existing source/binary property interface cases, expression-fact hover,
  local semantic query, project features, and complete stdio smoke retain
  positive consumer coverage.

## Tooling Evidence

| Toolchain | Expression facts | Local query/interface/project | Stdio JSON-RPC | Peak working set |
|---|---:|---:|---:|---:|
| GCC | 9/9, exit 0 | exit 0 | exit 0 | 30.36 MiB |
| Clang | 9/9, exit 0 | exit 0 | exit 0 | 29.22 MiB |
| MSVC | 9/9, exit 0 | exit 0 | exit 0 | 36.20 MiB |

## Results

The property signature formatter receives the owning analyzer, queries
`PropertyBySymbolId`, verifies SymbolId and TypeId agreement, and formats the
canonical property value TypeId. Missing or inconsistent identity fails
closed; there is no inferred-type, copied-contract, accessor-name, or source
text fallback in this display path.

## Acceptance Decision

Accepted on 2026-08-10 16:08 +08:00 as an independent L8 source property
documentation contract. This does not accept L8 as a whole or replace the
remaining provider, receiver, golden, and full protocol gates.
