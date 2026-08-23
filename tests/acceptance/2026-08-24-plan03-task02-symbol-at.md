# Plan 03 Task 2.1 SymbolAt Acceptance

## Scope

- Implement the Plan 03 Task 2 `SymbolAt` sub-milestone.
- Expose only a resolved canonical reference fact through the parser public
  query API.
- Keep unresolved references and unavailable owner identity fail-closed.

## Contract

- `ZrParser_SemanticQuery_SymbolAt` reads the already materialized
  `ZrParser_SemanticQuery_FactsAt` reference selection.
- A successful result copies `symbolId`, `typeId`, role, declaration range,
  and definition range from the canonical fact or its canonical definition
  lookup.
- Display and signature strings are borrowed fact fields. No LSP or caller
  reconstructs them from source tokens.
- The current fact model does not publish an owner identity; successful
  results therefore keep `ownerSymbolId` invalid rather than inventing one.
- Missing or unresolved references clear the output and return `ZR_FALSE`.

## Test Inventory

- `tests/parser/test_semantic_query_symbols.c`
  - resolved identity, range, role, and borrowed display/signature projection;
  - unresolved-reference output clearing and fail-closed behavior.
- `tests/parser/test_semantic_query.c`
  - existing fact selection, definition, reference, scope, diagnostics, and
    public-contract regression coverage.
- `tests/parser/test_semantic_query_contract.c`
  - read-only/fail-closed query lifecycle contract coverage.

## Toolchain Evidence

All commands used the actual CMake target or test executable in independent
toolchain caches and returned process exit code zero.

| Toolchain | Target/Test | Result |
| --- | --- | --- |
| GCC 4.8.3 | `zr_vm_semantic_query_symbols_test` | 2 tests, 0 failures |
| GCC 4.8.3 | query and query-contract regressions | 29 tests + 3 tests, 0 failures |
| Clang 19.1.5 | `zr_vm_semantic_query_symbols_test` | 2 tests, 0 failures |
| Clang 19.1.5 | query and query-contract regressions | 29 tests + 3 tests, 0 failures |
| MSVC 19.44.35228 | `zr_vm_semantic_query_symbols_test` | 2 tests, 0 failures |
| MSVC 19.44.35228 | query and query-contract regressions | 29 tests + 3 tests, 0 failures |

## Boundary

Plan 03 Task 2 remains open. `VisibleSymbols` requires canonical lexical
scope, shadowing, owner/access/static-context, import/alias, generic, and
declaration-order facts. This sub-milestone deliberately does not derive
those semantics from LSP symbol names or token scans.

## Acceptance Decision

Accepted for Plan 03 Task 2.1 after the three-toolchain query matrix. The
completion record in `docs/plans/lsp/optimize/` records the final timestamp
and remaining Task 2 gate.
