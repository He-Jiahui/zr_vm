---
plan_id: lsp-04-debug-and-repl
record_id: 2026-07-28-e2b0-canonical-binding-injection
status: completed
completed_at: 2026-07-28 22:56 +08:00
source_plan: docs/plans/lsp/04-debug-and-repl.md
related_code:
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/src/zr_vm_parser/type_system.c
plan_sources:
  - docs/plans/lsp/04-debug-and-repl.md
  - user: 2026-07-28 optimize semantic inference and record each completed LSP milestone
tests:
  - tests/parser/test_expression_fragment_parser.c
  - docs/parser-and-semantics/canonical-binding-injection.md
doc_type: milestone-record
---

# LSP 04 E2b0 Canonical Binding Injection

##状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
|---|---|---|---|
| 2026-07-28 22:56 +08:00 | 已完成 | E2b 的 support-first 子里程碑：正式 type environment 可保留外部已验证 binding 的 `SymbolId`、`TypeId` 与 declaration range；identifier inference 直接将它们投影到 canonical reference fact，不生成临时替代 identity。 | GCC、Clang、MSVC 均独立构建并实际运行 `zr_vm_expression_fragment_parser_test`：`4 Tests 0 Failures 0 Ignored`，真实 exit 0。 |

## 已完成计划项

- 发布 `ZrParser_TypeEnvironment_RegisterCanonicalVariable`。它要求非 invalid
  `SymbolId`/`TypeId` 和非空 declaration range，并只在临时 environment 保存
  supplied identity。
- 注册或覆盖 binding 时，环境复制 inferred type，但不调用 semantic symbol/type
  registration 或 rebind API。因此 formal parser 的 ordinary inference 能消费
  canonical identity，而不会把 paused-frame binding 改写为临时 local identity。
- 新的 Unity case 以 formal fragment `paused` 调用该 API，断言
  `ZR_SEMANTIC_REFERENCE_READ` 的 `symbolId=7001`、`typeId=7002` 和
  declaration offsets `400..406` 与外部值完全一致。

## 明确边界

- 这是 E2b 的 support slice，不代表 debug evaluate/watch/conditional breakpoint
  或 REPL binder 已完成。后续 binder 必须从 E1 的 generation-validated paused
  context 取得 binding，再使用本 API；stale、trimmed 或 unavailable binding 必须
  fail closed。
- `PlaceId` 继续由 Debug evaluation context 作为独立 carrier 保存；semantic
  reference fact 没有 Place 字段，禁止按 binding name、AST 或文本推断 place。
- 本项不修改 `type_inference_import_metadata.c`、property binder、LSP Task4
  production/test/docs 或任何 Syntax05 property import fallback。

## Related Documentation

- [Canonical external binding injection](../../../parser-and-semantics/canonical-binding-injection.md)
- [Formal expression fragment parser](../../../parser-and-semantics/expression-fragment-parser.md)
