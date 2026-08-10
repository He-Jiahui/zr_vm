# LSP L8 Canonical Receiver Member Type

## Scope

Verify that a project-source receiver member formats its declaration type only
from an exact resolved declaration SymbolId and canonical TypeId. A symbol
table's inferred type object must not independently authorize visible member
type text.

## Baseline

`receiver_project_set_type_text_from_symbol` previously called
`ZrParser_TypeNameString_Get` on `declarationSymbol->typeInfo`. The member could
therefore keep displaying `int` after the analyzer's declaration fact became
unresolved.

## Test Inventory

- `test_lsp_canonical_receiver_member_type_cases.h` resolves `meter.value` and
  first requires the canonical field type text `int`.
- The same analyzer then marks every declaration fact for the exact field
  SymbolId unresolved while preserving declaration navigation and symbol
  state. A second project-member resolution must keep the declaration but omit
  `resolvedTypeText`.
- Parser semantic facts freeze shared end/start boundary selection so the
  exact member segment wins at its start; expression hover, local query,
  interface, project features, and complete stdio smoke retain positive
  integration coverage.

## Tooling Evidence

| Toolchain | Semantic facts | LSP focused binaries | Stdio JSON-RPC | Peak working set |
|---|---:|---:|---:|---:|
| GCC | 13/13, exit 0 | exit 0 | exit 0 | 30.70 MiB |
| Clang | 13/13, exit 0 | exit 0 | exit 0 | 29.95 MiB |
| MSVC | 13/13, exit 0 | exit 0 | exit 0 | 37.17 MiB |

## Results

The receiver project resolver passes the owning analyzer to the shared
canonical symbol display helper. The helper queries the declaration by exact
SymbolId, verifies TypeId agreement, and formats only that canonical TypeId.
Missing or inconsistent identity fails closed; there is no
`symbol->typeInfo`, member-name, or source-text fallback in this path.

## Acceptance Decision

Accepted on 2026-08-10 18:24 +08:00 as an independent L8 project receiver
member type contract. This does not accept L8 as a whole or replace the
remaining provider, receiver, golden, and full protocol gates.
