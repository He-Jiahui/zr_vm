# LSP L8 Canonical Declaration Type Display

## Scope

Verify that explicit declaration types in hover and completion signatures are
formatted only from resolved canonical type facts. An unresolved source type
must fail closed instead of being reconstructed from AST or inferred-type text.

## Baseline

The previous analyzer recursively rendered declaration AST nodes, including
ownership wrappers and generic arguments. A later LSP documentation adapter
also emitted the callable symbol's inferred type independently. Together these
paths presented an unresolved `MissingType` as exact semantic information.

## Test Inventory

- `tests/language_server/test_semantic_analyzer.c` analyzes a function whose
  parameter and return annotations are unresolved, then checks completion and
  hover at its call site.
- `tests/language_server/stdio_smoke.js` opens the same fixture at version 1 and
  validates `textDocument/completion` and `textDocument/hover` before
  `didClose`.
- Existing suites retain positive coverage for resolved generics, ownership
  qualifiers, and exact inferred local expression types.

## Tooling Evidence

| Toolchain | Semantic analyzer | Interface | Stdio JSON-RPC | Peak working set |
|---|---:|---:|---:|---:|
| GCC | real exit 0 | real exit 0 | real exit 0 | 30.41 MiB |
| Clang | real exit 0 | real exit 0 | real exit 0 | 29.41 MiB |
| MSVC | real exit 0 | real exit 0 | real exit 0 | 36.50 MiB |

| Toolchain | Hover p50/p95/p99 ms | Completion p50/p95/p99 ms | Signature p50/p95/p99 ms | Diagnostics p50/p95/p99 ms | 100-file p50/p95/p99 ms |
|---|---|---|---|---|---|
| GCC | 1.76/4.41/5.21 | 1.05/1.52/2.40 | 0.78/2.24/2.54 | 0.71/3.40/6.66 | 3.18/12.16/796.06 |
| Clang | 1.52/2.19/2.47 | 1.09/1.66/2.41 | 0.53/0.71/0.88 | 0.52/1.33/2.03 | 3.09/6.03/74.59 |
| MSVC | 3.63/6.30/36.32 | 5.72/9.99/21.55 | 1.80/3.06/37.81 | 3.00/17.09/22.25 | 52.90/120.71/538.58 |

## Results

The local AST declaration renderer and ownership-name mapping are removed.
Explicit declaration display requires a resolved type reference and canonical
formatter. Unavailable identity consistently produces
`cannot infer exact type`; callable documentation no longer adds a second
inferred-type string.

## Acceptance Decision

Accepted on 2026-08-10 11:50 +08:00 as an independent L8 source declaration
display contract. This does not accept L8 as a whole or substitute the focused
source fixture for its remaining provider, project, golden, and protocol gates.
