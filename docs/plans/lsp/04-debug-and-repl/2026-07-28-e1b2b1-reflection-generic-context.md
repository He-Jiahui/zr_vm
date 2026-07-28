---
plan_id: lsp-04-debug-and-repl
record_id: 2026-07-28-e1b2b1-reflection-generic-context
status: completed
completed_at: 2026-07-28 20:35 +08:00
source_plan: docs/plans/lsp/04-debug-and-repl.md
related_code:
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/debug_evaluation_context.c
  - zr_vm_core/src/zr_vm_core/metadata_runtime.c
  - zr_vm_core/src/zr_vm_core/reflection.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/debug_evaluation_context.c
  - zr_vm_core/src/zr_vm_core/debug_evaluation_context_internal.h
plan_sources:
  - docs/plans/lsp/04-debug-and-repl.md
  - user: 2026-07-28 optimize semantic inference and record each completed LSP milestone
tests:
  - tests/module/test_reflection_dynamic_generic_instance_interpreter.h
  - tests/module/test_reflection_dynamic_generic_method_context.h
  - tests/debug/test_debug_introspection.c
  - docs/core-runtime/debug-canonical-local-bindings.md
doc_type: milestone-record
---

# LSP 04 E1b2b1 Reflection Generic Context

##状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
|---|---|---|---|
| 2026-07-28 20:35 +08:00 | 已完成 | E1b2b1 为暂停 VM 帧发布反射元数据泛型实参查询：TYPE/METHOD context、owner token、parameter index 均通过公开 reflection resolver 严格绑定；wrong owner、opposite context 与retired frame明确不可用。 | GCC、Clang、fresh MSVC shared 均为 `zr_vm_reflection_dynamic_generic_instance_test` 35 Tests/0 Failures、`zr_vm_debug_introspection_test` 2 Tests/0 Failures，真实 exit 0。 |

## 已完成计划项

- `ZrCore_Debug_EvaluationContext_GetGenericArgument` 先复验 captured activation、frame generation 与 instruction offset，再查询当前 call-info；frame reuse、return 和 PC 漂移均 fail closed。
- `EZrDebugGenericContextKind` 区分 TYPE 与 METHOD。TYPE 只调用 `ZrCore_Reflection_ResolveInterpreterGenericCallInfoParameterTypeObject`，METHOD 只调用 `ZrCore_Reflection_ResolveInterpreterGenericMethodCallInfoParameterTypeObject`。
- API 输入固定为 structured context kind、owner metadata token 与 parameter index。错误 owner、缺失 generic context、缺失 metadata argument 或反射 resolver 无结果均返回 `METADATA_UNAVAILABLE`。
- 输出 `SZrDebugGenericArgument.typeObject` 是 borrowed reflection type object，只能在同一 paused context 仍有效时消费；没有复制、缓存或跨帧复用合同。
- 公共 `debug.c` 中的 paused-frame validation 提取到 `debug_evaluation_context.c` 内聚模块，binding、receiver 与generic query统一使用同一 activation/generation/PC 真实性检查。
- 测试在真实 type-generic 与 method-generic trace hook 中将 debug 输出与既有公开 reflection resolver 的对象身份比较；同时覆盖 opposite context、wrong owner 及 return 后 stale context。

## 明确边界

- 本项只发布 reflection metadata 能解析的 generic type-object 实参。source generic `TypeId`、const-generic literal/parameter substitution 当前没有稳定的 call-frame 或 artifact fact；不能为完成 E1 伪造 runtime carrier。E4 仅在 canonical facts 已发布 concrete TypeId 时传输它。
- 不读取 reflection 私有布局，不从 generic name、display string、AST、hidden accessor 或文本重建参数。
- 本项没有修改 parser import metadata 或 LSP property consumer；Syntax05 Task4 对 `type_inference_import_metadata.c` 的 compiled property prototype 修复保持独立 ownership。

## Deferred Plan Items

- E2 formal parser/binder/Place query reuse，及 E3-E5 effect policy、result transport 和 REPL generation。

## Related Documentation

- [Debug canonical local bindings](../../../core-runtime/debug-canonical-local-bindings.md)
