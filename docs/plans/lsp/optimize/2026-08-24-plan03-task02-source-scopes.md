# Plan 03 Task 2.2b: Source Lexical Scope Facts

## Goal

Publish compiler-owned lexical scope facts for the source subset already backed
by resolved declaration reference facts. `VisibleSymbols` must consume those
facts directly and must not rebuild name lookup from LSP state, token text, or
an AST traversal.

## Implementation

- Added the private `semantic_scope_facts` builder and invoke it after source
  semantic facts and query diagnostics have been published in `compile_script`.
- Publish a module root plus function, block, and loop scope parentage. The source
  subset publishes hoisted functions plus exact resolved function parameter and
  local declaration candidates.
- Use AST nodes only to establish source scope boundaries. Each candidate comes
  from the exact declaration reference fact for the declaration node and keeps
  that fact's `SymbolId`, declaration/definition ranges, and display contract.
  Missing or unresolved declaration facts are omitted rather than guessed.
- The focused source test compiles nested `value` declarations and verifies the
  inner return site has one visible `value` and the enclosing `seed` parameter.
- A second focused source fixture verifies a `for` initializer does not remain
  visible at the declaration following the loop.

## Deliberate Boundary

This child milestone does not publish generic/type scopes, imports/aliases,
receiver members, access-restricted source members, `.zro`, or native descriptor
scope facts. It also does not migrate an LSP completion consumer. Those remain
Task 2 work because their identities must be produced by canonical parser or
artifact facts before `VisibleSymbols` can consume them.

## Verification

- RED: the new compiled-source lexical scope test failed before the producer
  existed because `VisibleSymbols` had no containing scope fact.
- GCC static: `zr_vm_semantic_query_symbols_test` 6/6,
  `zr_vm_semantic_query_test` 29/29,
  `zr_vm_semantic_query_contract_test` 3/3, and
  `zr_vm_compiler_semantic_query_diagnostics_test` 46/46, all with process
  exit 0.
- MSVC static: the same four targets passed 6/6, 29/29, 3/3, and 46/46 with
  process exit 0.
- Clang 14 WSL compiled the final scope builder and focused test source
  successfully. Its static test link remains blocked by pre-existing C11
  inline ABI undefined references beginning at `ZrCore_Memory_RawFree`; no
  Clang test execution is claimed for this child milestone.

## 状态与产出记录

- 完成时间：2026-08-24 10:32:04 +08:00
- 状态：GCC 与 MSVC 子里程碑完成；Clang static 执行被已记录的链接门禁阻断，
  不宣称三工具链完成。
- 完成项目：compiler-published source scope facts、fail-closed source
  declaration projection、focused RED/GREEN coverage、module documentation
  与本 Plan 03 Task 2 记录。
- 后续项目：canonical generic/type/import/alias/receiver facts、binary/native
  producer parity、LSP consumer migration 与 Task 2 最终验收。
