---
plan_id: aot-10-reflection
record_id: 2026-07-19-token-and-generic-reflection-baseline
status: completed
completed_at: 2026-07-19 03:53 +08:00
source_plans:
  - docs/plans/aot/10-reflection.md
  - docs/plans/aot/07-12-codegen/2026-07-19-08-s6v-10-s4z43-11-s5a.md
  - docs/plans/aot/07-12-codegen/2026-07-19-08-s6w-10-s4z44-11-s5b.md
evidence_scope: historical-baseline
---

# Token And Generic Reflection Baseline

## 可复用结论

- public token resolver 能携带 TypeDef/TypeRef/TypeSpec/FieldDef/MethodDef/MethodSpec 的部分 metadata/runtime binding。
- AOT 已有代表性 invoker buckets、arity/type guards、return-slot canonicalization 与 primitive argument/result marshaling。
- FieldInfo baseline 已覆盖 token/layout/offset、代表性 primitive和 nested inline path 读写。
- generic type/method definition and instance objects 已具备部分 GC-managed carrier 与 fail-closed validation。
- `ZrCore_Reflection_ResolveConstructedGenericMethod`已有本地MethodDef到既有MethodSpec的精确、fail-closed request resolution baseline。

## 证据入口

- `tests/module/test_reflection_token_resolve.c`
- `tests/module/test_reflection_dynamic_generic_instance.c`
- `tests/module/test_metadata_runtime_query.c`
- `tests/acceptance/2026-07-19-aot-08-s6v-10-s4z43-11-s5a-generic-method-definition-object.md`
- `tests/acceptance/2026-07-19-aot-08-s6w-10-s4z44-11-s5b-methodspec-request-resolution.md`

## 未完成边界

目标 `zr.reflection` 的 Type/TypeOf hierarchy、`typeof`/`typeid`、member query surface、`createInstance(...args)`、完整 Invoke marshaling、跨模块 identity 与 trimming diagnostics 仍按 syntax 08 实施。
