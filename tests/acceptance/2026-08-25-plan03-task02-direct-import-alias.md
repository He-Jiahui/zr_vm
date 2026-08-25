---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - tests/parser/test_semantic_query_symbols.c
related_docs:
  - docs/plans/lsp/optimize/2026-08-25-plan03-task02-direct-import-alias.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 2.2k Direct Import Alias Visibility Facts

## Scope

Validate that direct source module bindings are published as canonical,
opt-in import/alias candidates. The test must prove visibility uses the original
declaration identity and cannot succeed by matching `math` or `zr.math` text.

## Test Inventory

`test_visible_symbols_projects_direct_import_alias` registers the real
`zr.math` native module, compiles:

```zr
var math = import("zr.math");
fn probe(): int { return 0; }
```

It verifies the compiler produced a variable declaration symbol, that its
visible-symbol fact is marked import and alias, that default options omit
`math`, and that `includeImports` returns exactly that same `SymbolId`.

## Tooling Evidence

| Environment | Targets | Result |
| --- | --- | --- |
| MSVC static | symbols, query, contract, compiler diagnostics, compiler integration | 16/16, 29/29, 3/3, 46/46, 127/127; zero failures; compiler integration exit 0 |
| GCC/Clang | executable targets | not rerun for this narrow producer child; no pass claimed |

## Acceptance Decision

Accepted for the MSVC direct-import source producer submilestone. The feature
does not add an LSP fallback and does not claim destructured, type-value,
binary, or native import parity.

## 状态与产出记录

- 完成时间：2026-08-25 16:39:39 +08:00
- 状态：MSVC acceptance passed；GCC/Clang executable acceptance 未在本次重跑，
  不做通过声明。
- 完成项目：direct import/alias canonical fact、default filtering、opt-in
  `includeImports` identity projection、focused regression evidence。
- 后续项目：destructured import、type-value alias、binary/native producer parity
  与 LSP consumer migration。
