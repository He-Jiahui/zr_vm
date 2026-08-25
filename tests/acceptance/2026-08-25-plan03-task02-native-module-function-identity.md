---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - tests/parser/test_semantic_query_symbols.c
related_docs:
  - docs/plans/lsp/optimize/2026-08-25-plan03-task02-native-module-function-identity.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 2.3a Native Module Function Identity

## Scope

Validate that a resolved non-generic native module function call is projected
by parser facts with one snapshot-scoped identity. The test must fail when the
call has no resolved fact, has an invalid `SymbolId` or `TypeId`, or fabricates
a source declaration range from the call position.

## Test Inventory

`test_symbol_at_projects_native_module_function_identity` registers the real
`zr.math` module and compiles:

```zr
var math = import("zr.math");
fn probe(): float { return math.abs(-3.0); }
```

It verifies the `CALL` fact at `abs`, its resolved `SymbolId` and closed
callable `TypeId`, and the matching `SymbolAt` identity. Both declaration
ranges must be zero because a descriptor function has no source AST
declaration. The test uses only parser facts and query APIs.

## Tooling Evidence

| Environment | Targets | Result |
| --- | --- | --- |
| MSVC static | symbols, query, contract, compiler diagnostics, compiler integration | 18/18, 29/29, 3/3, 46/46, 127/127; zero failures; real process exit 0 |
| Windows GCC 4.8.3 | symbols build | blocked before parser compilation by existing `_Thread_local` incompatibility in shared core; no test result |
| Clang | executable targets | no local executable and WSL distribution unavailable; no test result |

## Acceptance Decision

Accepted for the MSVC non-generic native module-function producer submilestone.
This slice adds no LSP fallback, does not assign generic native declarations
from one closed call, and does not claim binary metadata parity or Task 2
completion.

## 状态与产出记录

- 完成时间：2026-08-25 18:31:18 +08:00
- 状态：MSVC acceptance passed；GCC/Clang executable acceptance 未完成，
  不做通过声明。
- 完成项目：native call fact identity、zero declaration-range projection、
  SymbolAt parity、generic fail-closed boundary、focused regression evidence。
- 后续项目：native generic declaration identity、binary producer parity、
  relation/external-origin facts 与 LSP consumer migration。
