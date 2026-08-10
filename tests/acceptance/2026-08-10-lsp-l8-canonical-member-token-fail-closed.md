# LSP L8 Canonical Member Token Fail-Closed

## Scope

Verify that semantic token member kinds come only from the current semantic
query or structured import metadata. Punctuation alone must not classify an
unresolved member as a namespace, method, or property.

## Baseline

The previous scanner classified a dotted identifier as `method` when a later
parenthesis existed, `property` when it did not, and `namespace` when another
dot followed. This fabricated LSP semantics when query resolution was
unavailable.

## Test Inventory

- `tests/language_server/test_lsp_interface.c` uses one source fixture with
  `target.resolved()` and `target.unresolved()`. The former must be `method`
  through the semantic query; the latter must have no member token.
- `tests/language_server/stdio_smoke.js` opens the same version-1 fixture and
  validates the `textDocument/semanticTokens/full` JSON-RPC result before
  closing the document.

## Tooling Evidence

| Toolchain | Interface | Stdio JSON-RPC | Peak working set |
|---|---:|---:|---:|
| GCC | real exit 0 | real exit 0 | 30.05 MiB |
| Clang | real exit 0 | real exit 0 | 29.26 MiB |
| MSVC | real exit 0 | real exit 0 | 38.00 MiB |

The version-1 source fixture exercises `didOpen`,
`textDocument/semanticTokens/full`, and `didClose` under the unchanged
semantic-query schema. It covers source identity and existing structured
import metadata only; binary, native, and project parity remain outside this
leaf.

| Toolchain | Hover p50/p95/p99 ms | Completion p50/p95/p99 ms | Signature p50/p95/p99 ms | Diagnostics p50/p95/p99 ms | 100-file p50/p95/p99 ms |
|---|---|---|---|---|---|
| GCC | 5.02/22.25/22.55 | 1.12/4.92/15.52 | 0.58/0.71/0.79 | 0.63/1.88/8.75 | 3.59/12.32/682.48 |
| Clang | 1.62/1.85/2.71 | 1.03/1.25/1.35 | 0.55/0.75/0.76 | 0.51/1.34/2.22 | 3.42/4.29/69.08 |
| MSVC | 3.39/5.20/25.90 | 4.54/34.83/93.30 | 2.01/3.60/3.89 | 2.57/4.33/18.09 | 27.21/136.93/348.66 |

All three runs remained below the existing 512 MiB peak-memory limit.

## Results

`semantic_token_guess_member_type` and both of its call sites are removed.
The scanner emits a member token only for an exact semantic-query or
structured metadata result; unavailable identity remains unclassified.

## Acceptance Decision

Accepted on 2026-08-10 08:34 +08:00 as an independent L8 fallback-deletion
contract. This does not accept L8 as a whole or substitute the focused tests
for its remaining provider/project and full protocol-matrix gates.
