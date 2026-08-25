---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - tests/parser/test_semantic_query_symbols.c
related_docs:
  - docs/plans/lsp/optimize/2026-08-25-plan03-task02-destructured-import-type-value-alias.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 2.2l Destructured Import and Type-Value Alias Facts

## Scope

Validate that source destructuring and type-value alias candidates have exact
compiler-owned declaration identity. The test must fail if a candidate is found
by imported member name, module path, or alias text rather than the resolved
declaration fact.

## Test Inventory

`test_visible_symbols_projects_destructured_import_and_type_value_aliases`
registers the real `zr.math` module, compiles:

```zr
var {Vec3: Vector3} = import("zr.math");
var MatrixType = int[][];
fn probe(): int { return 0; }
```

It verifies that `Vec3` has a variable SymbolId and TypeId associated with the
binding token, not `Vector3`; that `SymbolAt` resolves each declaration to the
same identity as its visible-symbol fact; and that the facts classify
destructured import as import/alias and type-value binding as alias only.
Default options omit both aliases, while `includeImports` returns each exact
SymbolId once.

## Tooling Evidence

| Environment | Targets | Result |
| --- | --- | --- |
| MSVC static | symbols, query, contract, compiler diagnostics, compiler integration | 17/17, 29/29, 3/3, 46/46, 127/127; zero failures; real process exit 0 |
| Windows GCC 4.8.3 | symbols build | blocked before parser compilation by existing `_Thread_local` incompatibility in shared core; no test result |
| Clang | executable targets | no local executable and WSL distribution unavailable; no test result |

## Acceptance Decision

Accepted for the MSVC source-producer submilestone. This slice adds no LSP
fallback and does not claim binary/native alias parity or Task 2 completion.

## 状态与产出记录

- 完成时间：2026-08-25 17:39:05 +08:00
- 状态：MSVC acceptance passed；GCC/Clang executable acceptance 未完成，
  不做通过声明。
- 完成项目：exact destructured binding identity、type-value alias fact、
  SymbolAt/VisibleSymbols parity、import/alias opt-in filtering、focused
  regression evidence。
- 后续项目：binary/native producer parity、alias relation facts 与 LSP
  consumer migration。
