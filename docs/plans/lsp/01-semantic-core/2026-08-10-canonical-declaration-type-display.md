# Canonical Declaration Type Display

## Status

- Completed: 2026-08-10 11:50 +08:00
- Scope: LSP 08 independent local declaration display fallback-deletion contract
- Baseline: `769070a`

## Contract

Explicit declaration types in source hover and completion signatures are
displayed only from a resolved `ZR_SEMANTIC_REFERENCE_TYPE` fact and its
canonical `TypeId`. The consumer formats that identity through
`ZrParser_CanonicalType_Format`; it does not recursively print AST type names,
ownership wrappers, generic arguments, or array syntax.

When a declaration type has no resolved canonical fact, the display is
`cannot infer exact type`. Building a structural inferred type is not enough:
the declaration publisher records whether every nominal root and generic
argument is available in the compiler prototype environment. Unannotated local
expressions remain displayable when an exact, precise expression fact supplies
their canonical `TypeId` and no unresolved type reference invalidates that
identity.

Callable completion documentation no longer appends a second type string from
the symbol's local inferred-type object. The canonical signature is the only
callable type display surface.

## Evidence

- The RED semantic-analyzer fixture declared
  `fn redact(value: MissingType): MissingType` and first exposed the raw
  `MissingType` AST text in completion and hover signatures.
- The version-1 stdio fixture repeated the same request path. Before the final
  interface fix, the JSON response still exposed `Resolved Type: MissingType`
  and `Type: MissingType`, proving that both the analyzer and adapter layers
  required convergence.
- `tests/language_server/test_semantic_analyzer.c` now requires parameter and
  return displays to say `cannot infer exact type` and forbids `MissingType`.
- `tests/language_server/stdio_smoke.js` verifies the same result through
  `textDocument/completion` and `textDocument/hover`, then closes the document.
- Existing generic, ownership-qualified, local inferred-type, and exact-type
  failure tests remain green.

## Validation Matrix

GCC, Clang, and MSVC each rebuilt and directly executed the semantic analyzer,
LSP interface, and complete stdio smoke on the same final source. Every process
returned real exit zero. The MSVC rebuild also removed the intermediate C4701
warning by zero-initializing the canonical type query before conditional use.

| Toolchain | Hover p50/p95/p99 ms | Completion p50/p95/p99 ms | Signature p50/p95/p99 ms | Diagnostics p50/p95/p99 ms | 100-file diagnostics p50/p95/p99 ms | Peak |
|---|---|---|---|---|---|---:|
| GCC | 1.76/4.41/5.21 | 1.05/1.52/2.40 | 0.78/2.24/2.54 | 0.71/3.40/6.66 | 3.18/12.16/796.06 | 30.41 MiB |
| Clang | 1.52/2.19/2.47 | 1.09/1.66/2.41 | 0.53/0.71/0.88 | 0.52/1.33/2.03 | 3.09/6.03/74.59 | 29.41 MiB |
| MSVC | 3.63/6.30/36.32 | 5.72/9.99/21.55 | 1.80/3.06/37.81 | 3.00/17.09/22.25 | 52.90/120.71/538.58 | 36.50 MiB |

These metrics come from the existing broad stdio workload. All three runs are
below the 512 MiB process peak limit; this leaf does not introduce a new
performance promotion gate.

## Open Scope

This closes one source-local L8 declaration display fallback. It does not
complete L8. Binary/native/project provider parity, other local ownership/type
fallbacks, reference-fixture golden coverage, and the full project/protocol
matrix remain open.
