# Canonical Member Token Fail-Closed

## Status

- Completed: 2026-08-10 08:34 +08:00
- Scope: LSP 08 independent semantic-token fallback-deletion contract
- Baseline: `e9512c8`

## Contract

Semantic tokens classify a source member only when
`ZrLanguageServer_LspSemanticQuery_ResolveAtPosition` returns a structured
symbol or provider result. The existing import-chain metadata resolver remains
an allowed source of structured module/member identity.

The consumer no longer infers `namespace`, `method`, or `property` from a dot,
a following dot, or a following parenthesis. If both query paths are
unavailable, no semantic token is emitted for that member span. This is a
fail-closed presentation result, not an unknown-as-method fallback.

The fixture proves both boundaries in one document: `target.resolved()` is a
`method` because it has a source semantic-query result, while
`target.unresolved()` has no namespace, method, or property token. The same
contract is visible through `textDocument/semanticTokens/full` after a
version-1 `didOpen` request.

## Evidence

- The RED interface test first showed that `target.unresolved()` was emitted
  as `method` solely because the old scanner examined punctuation.
- `tests/language_server/test_lsp_interface.c` now asserts query-backed
  `resolved` method classification and fail-closed unresolved-member output.
- `tests/language_server/stdio_smoke.js` repeats the same positive and
  negative assertions over JSON-RPC semantic tokens, with version-1 document
  provenance and normal document close handling.
- GCC, Clang, and MSVC interface test executables and stdio smoke processes
  all completed with real exit zero. The final stdio peak working sets were
  30.05 MiB (GCC), 29.26 MiB (Clang), and 38.00 MiB (MSVC), below the 512 MiB
  limit.

## Validation Matrix

The protocol fixture uses a current document snapshot at version 1 and the
existing semantic-query schema. It sends `textDocument/didOpen`, then
`textDocument/semanticTokens/full`, and closes the same document. This slice
covers source member facts and existing structured import metadata; it does
not claim binary, native, or project provider parity.

| Toolchain | Hover p50/p95/p99 ms | Completion p50/p95/p99 ms | Signature p50/p95/p99 ms | Diagnostics p50/p95/p99 ms | 100-file diagnostics p50/p95/p99 ms | Peak |
|---|---|---|---|---|---|---:|
| GCC | 5.02/22.25/22.55 | 1.12/4.92/15.52 | 0.58/0.71/0.79 | 0.63/1.88/8.75 | 3.59/12.32/682.48 | 30.05 MiB |
| Clang | 1.62/1.85/2.71 | 1.03/1.25/1.35 | 0.55/0.75/0.76 | 0.51/1.34/2.22 | 3.42/4.29/69.08 | 29.26 MiB |
| MSVC | 3.39/5.20/25.90 | 4.54/34.83/93.30 | 2.01/3.60/3.89 | 2.57/4.33/18.09 | 27.21/136.93/348.66 | 38.00 MiB |

These are the existing broad smoke workload measurements, reported here as
required evidence rather than a new performance promotion gate.

## Open Scope

This closes one L8 local member-token fallback. It does not complete L8:
remaining local type/ownership display fallbacks, source/binary/native/project
provider coverage, reference-fixture golden coverage, and the full
project/protocol matrix remain open.
