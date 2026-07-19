---
plan_id: aot-08-generics
record_id: 2026-07-19-generic-sharing-runtime-baseline
status: completed
completed_at: 2026-07-19 03:53 +08:00
source_plans:
  - docs/plans/aot/08-generic-sharing.md
  - docs/plans/aot/07-12-codegen/2026-07-19-08-s6v-10-s4z43-11-s5a.md
  - docs/plans/aot/07-12-codegen/2026-07-19-08-s6w-10-s4z44-11-s5b.md
evidence_scope: historical-baseline
---

# Generic Sharing And Runtime Instance Baseline

## 可复用结论

- 已有 reference sharing、value monomorphization、generic dictionary、METHOD/TYPE_LAYOUT/SIZEOF slot 与 typed call 基线。
- runtime 能读取本地 TypeSpec 的 primitive、direct、nested generic、array、tuple、ownership、nullable 和 union argument shape。
- 已有解释器 generic type/method context carrier、GenericParam substitution、MethodSpec context、代表性 VM execution、boxed dynamic value instance 和 provider TypeSpec identity 验证。
- public generic method definition object 能物化 MethodDef 的 GenericParam owner range，并从 metadata string pool 读取名称。
- constructed generic method request能在attached runtime中按MethodDef identity、GenericParam arity、参数顺序与递归signature shape匹配已有MethodSpec；失败不伪造或缓存metadata entity。

## 证据入口

- `tests/module/test_reflection_dynamic_generic_instance.c`
- `tests/module/test_reflection_dynamic_generic_method_context.h`
- `tests/module/test_metadata_runtime_typespec_layout.c`
- `tests/acceptance/2026-07-19-aot-08-s6v-10-s4z43-11-s5a-generic-method-definition-object.md`
- `tests/acceptance/2026-07-19-aot-08-s6w-10-s4z44-11-s5b-methodspec-request-resolution.md`

## 未完成边界

- Canonical Type graph 与新 owner/ref TypeNode identity 的闭环。
- script-level `MakeGenericMethod`、完整跨模块 method binding、closed-world full-AOT closure。
- 新 `zr.reflection` API 的公共类型层级与 trim contract。
