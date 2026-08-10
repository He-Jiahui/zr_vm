# Canonical Property Signature Type

## Status

- Completed: 2026-08-10 16:08 +08:00
- Scope: LSP 08 independent property documentation fallback-deletion contract
- Baseline: `eccd866`

## Contract

Source property hover and completion documentation may format a property
signature only when the current analyzer can query the canonical PropertyDef
by the visible property's `SymbolId`. The queried `propertySymbolId` and
`propertyTypeId` must match the symbol snapshot, and the value type is
formatted only through `ZrParser_CanonicalType_Format`.

`ZrLanguageServer_LspPropertyContract_FormatSignature` now receives the
owning analyzer and calls `ZrParser_SemanticQuery_PropertyBySymbolId`. It no
longer formats `symbol->typeInfo` or trusts the copied property contract as an
independent display authority. Missing context, missing PropertyDef, invalid
identity, mismatched TypeId, or an unavailable canonical formatter omits the
property signature instead of reconstructing it from inferred or source text.

## Evidence

- The RED fixture retained the same property symbol, precise inferred `int`
  type, declaration fact, and copied contract, but removed only the semantic
  context's PropertyDef fact. The old formatter still emitted
  `property value: int` from `symbol->typeInfo`.
- The GREEN fixture proves both directions on one analyzer snapshot: the
  canonical PropertyDef emits `property value: int`; removing only that fact
  preserves ordinary symbol documentation but removes the property signature.
- Existing source and binary property hover, completion, definition, rename,
  semantic-token, refactor, and incremental tests remain green.
- Expression-fact hover, local semantic query, interface, project feature, and
  complete stdio workloads all execute the same production source.

## Validation Matrix

GCC, Clang, and MSVC each built and directly executed the expression-fact
hover suite, local semantic-query suite, LSP interface suite, project feature
suite, and complete stdio smoke. Every test and smoke process returned real
exit zero; the focused expression suite reported 9/9 on every toolchain.

| Toolchain | Hover p50/p95/p99 ms | Completion p50/p95/p99 ms | Signature p50/p95/p99 ms | Diagnostics p50/p95/p99 ms | 100-file diagnostics p50/p95/p99 ms | Peak |
|---|---|---|---|---|---|---:|
| GCC | 1.75/3.61/9.01 | 1.15/5.42/8.15 | 0.65/8.89/10.12 | 0.62/6.79/10.69 | 3.54/7.39/213.73 | 30.36 MiB |
| Clang | 2.67/5.19/6.51 | 1.13/2.02/3.36 | 0.57/1.28/3.87 | 0.50/0.91/10.17 | 3.73/11.78/233.19 | 29.22 MiB |
| MSVC | 3.86/16.97/19.14 | 5.03/8.54/27.31 | 2.00/3.22/19.23 | 2.78/8.53/16.73 | 33.20/52.29/227.69 | 36.20 MiB |

All three stdio runs remain below the 512 MiB process peak limit. This leaf
does not add a separate performance promotion gate.

## Open Scope

This closes one source property signature fallback. It does not complete L8.
Receiver type resolution, prototype/member completion fallbacks, broader
binary/native unavailable parity, reference-fixture golden coverage, and the
full project/protocol matrix remain open.
