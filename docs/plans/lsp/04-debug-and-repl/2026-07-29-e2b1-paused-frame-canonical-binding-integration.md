---
plan_id: lsp-04-debug-and-repl
record_id: 2026-07-29-e2b1-paused-frame-canonical-binding-integration
status: completed
completed_at: 2026-07-29 00:13 +08:00
source_plan: docs/plans/lsp/04-debug-and-repl.md
related_code:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_semantic_bindings.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_semantic_facts.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h
implementation_files:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_semantic_bindings.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_semantic_facts.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h
plan_sources:
  - docs/plans/lsp/04-debug-and-repl.md
  - docs/plans/lsp/semantic-inference/status-and-output.md
tests:
  - tests/debug/test_debug_expression_diagnostics.c
  - docs/core-runtime/debug-canonical-local-bindings.md
  - docs/parser-and-semantics/canonical-binding-injection.md
doc_type: milestone-record
---

# LSP 04 E2b1 Paused-Frame Canonical Binding Integration

##状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
|---|---|---|---|
| 2026-07-29 00:13 +08:00 | 已完成 | E2b1 paused-frame canonical binding integration：Debug 语义事实通过正式单表达式 parser 消费 generation-validated readonly frame binding，并保留编译期 `SymbolId`、`TypeId` 和 declaration range，不生成临时替代 identity。 | GCC、Clang、MSVC 均构建并实际运行 `zr_vm_debug_expression_diagnostics_test`：`34 Tests 0 Failures 0 Ignored`，真实 exit 0；新用例精确比较 `paused` binding 与 inferred reference fact 的 identity/range。 |

## 已完成计划项

- `zr_debug_semantic_register_frame_variables` obtains the read-only evaluation
  context, enumerates only its active bindings, and fails closed when context,
  metadata, canonical identity, or the exact typed-local row is unavailable.
- Each verified binding is registered with
  `ZrParser_TypeEnvironment_RegisterCanonicalVariable`; no name, AST, text, or
  raw-slot fallback may synthesize a semantic identity.
- Debug semantic facts now use `ZrParser_ParseExpressionWithState` instead of
  constructing a synthetic expression statement.
- The internal semantic binder is exported only through `debug_internal.h` so
  the shared-library regression target can inspect the production inference
  path. It remains absent from the public `zr_vm_lib_debug/debug.h` API.

## 明确边界

- This completes the exact paused-frame binding path, not all of E2b. The
  separate `PlaceId` carrier and shared debug evaluate/watch/conditional
  breakpoint/REPL consumer query remain follow-up work.
- Stale, trimmed, missing, or mismatched frame metadata fails closed; the
  implementation does not fall back to a temporary semantic identity.

## Related Documentation

- [Debug canonical local bindings](../../../core-runtime/debug-canonical-local-bindings.md)
- [Canonical external binding injection](../../../parser-and-semantics/canonical-binding-injection.md)
