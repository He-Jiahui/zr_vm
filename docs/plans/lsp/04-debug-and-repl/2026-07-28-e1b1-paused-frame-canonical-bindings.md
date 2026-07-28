---
plan_id: lsp-04-debug-and-repl
record_id: 2026-07-28-e1b1-paused-frame-canonical-bindings
status: completed
completed_at: 2026-07-28 16:54 +08:00
source_plan: docs/plans/lsp/04-debug-and-repl.md
related_code:
  - zr_vm_core/include/zr_vm_core/call_info.h
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/state.c
tests:
  - tests/debug/test_debug_introspection.c
  - tests/debug/test_debug_metadata.c
---

# LSP 04 E1b1 Paused Frame Canonical Bindings

##状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
|---|---|---|---|
| 2026-07-28 16:54 +08:00 | 已完成 | 每个VM和stack-local native调用帧分配非零generation；暂停帧按exact activation、PC区间和typed-local canonical row枚举活动binding；PC变化、retire和tail-frame reuse后的旧context严格失效。 | GCC、Clang、MSVC均为`zr_vm_debug_introspection_test` 2 Tests/0 Failures、`zr_vm_debug_metadata_test` 5 Tests/0 Failures，真实exit 0。 |

## 已完成计划项

- E1b1 frame generation：状态维护单调非零generation，普通VM、stack-local native和tail-frame reuse均重新分配。
- E1b1 paused-frame context：查询仅接受当前VM activation，捕获exact function metadata、instruction offset和generation。
- E1b1 visible bindings：binding由debug local PC live interval与exact stack-slot typed-local row相交得到，输出SymbolId、TypeId、PlaceId、declaration range和scope depth。
- E1b1 stale rejection：查询先确认CallInfo仍在state列表中，再比较generation、function metadata和PC；retired或reused frame返回`STALE_FRAME`。
- E1b1 unavailable boundary：缺失typed-local row或任一canonical identity为零时返回`METADATA_UNAVAILABLE`，不按name、slot、AST、display type或text恢复。

## Canonical Contract

`ZrCore_Debug_GetEvaluationContext` produces a `SZrDebugEvaluationContext` for
the exact paused activation. `ZrCore_Debug_EvaluationContext_GetBinding` is the
only local-binding projection used by this slice. It copies identities from the
artifact-backed `SZrFunctionTypedLocalBinding`; liveness comes only from the
debug local PC interval. The test fixture may use names to identify its expected
row, but the production query never matches names.

`hasGenericContext` and `hasGenericMethodContext` are availability flags only.
They do not expose a generic type/value substitution. The context also does not
yet carry a canonical receiver binding.

## Deferred Plan Items

- E1b2 receiver canonical carrier and structured generic type/value snapshots.
- E2 formal parser/binder/Place query reuse for debug expressions.
- E3 effect policy, E4 result transport, and E5 REPL generation policy.
- Removal or migration of the independent debug expression parser.

## Related Documentation

- [Debug canonical local bindings](../../../core-runtime/debug-canonical-local-bindings.md)
